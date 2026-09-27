import {
  applyGraphEvent,
  capabilitiesForNode,
  provenanceOrigins,
  visibleGraph,
} from "./graph-state.js";
import { GraphRenderer, presentationKind } from "./graph-renderer.js";

const byId = (id) => document.getElementById(id);
const svg = byId("graph");

function prepareSnapshot(value) {
  if (value?.contract === "labfy.web_graph.error.v1") {
    return {
      contract: "labfy.web_graph.snapshot.v2",
      origin: "core",
      revision: 1,
      nodes: [],
      edges: [],
      capability_catalog: [],
      capabilities: [],
      _loadError: value.message,
    };
  }
  if (value?.contract === "labfy.web_graph.snapshot.v1") return value;
  if (
    !["labfy.web_graph.snapshot.v2", "labfy.web_graph.snapshot.v3"].includes(
      value?.contract,
    ) ||
    value.origin !== "core"
  ) {
    throw new Error("contrat snapshot inattendu");
  }
  const lanes = { evidence: 0, extraction: 1, observation: 2, entity: 3 };
  const ordered = [...value.nodes].sort((left, right) => {
    const lane = lanes[presentationKind(left)] - lanes[presentationKind(right)];
    return lane || left.id.localeCompare(right.id);
  });
  const laneCounts = new Map();
  const positions = new Map();
  for (const node of ordered) {
    const lane = lanes[presentationKind(node)];
    const index = laneCounts.get(lane) ?? 0;
    laneCounts.set(lane, index + 1);
    positions.set(node.id, {
      x: (lane - 1.5) * 360 + Math.floor(index / 12) * 90,
      y: (index % 12) * 92,
    });
  }
  for (const [lane, count] of laneCounts)
    for (const node of ordered.filter(
      (item) => lanes[presentationKind(item)] === lane,
    ))
      positions.get(node.id).y -= ((Math.min(count, 12) - 1) * 92) / 2;
  // CONTRACT: les étages preuve → extraction → observation → entité sont une
  // disposition déterministe bornée. Ils ne créent ni ne retirent une arête.
  return {
    ...value,
    nodes: value.nodes.map((node) => ({ ...node, ...positions.get(node.id) })),
  };
}

function snapshotFingerprint(value) {
  return JSON.stringify({
    contract: value.contract,
    investigation: value.investigation,
    investigation_id: value.investigation_id,
    revision: value.revision,
    nodes: value.nodes,
    edges: value.edges,
    capability_catalog: value.capability_catalog,
    capabilities: value.capabilities,
  });
}

function fitGraphScale() {
  const horizontalExtent = Math.max(
    700,
    ...snapshot.nodes.map((node) => Math.abs(node.x) * 2 + 160),
  );
  const verticalExtent = Math.max(
    500,
    ...snapshot.nodes.map((node) => Math.abs(node.y) * 2 + 120),
  );
  return Math.min(
    1,
    Math.max(
      0.25,
      Math.min(
        (svg.clientWidth - 100) / horizontalExtent,
        (svg.clientHeight - 70) / verticalExtent,
      ),
    ),
  );
}

let snapshot = prepareSnapshot({
  contract: "labfy.web_graph.snapshot.v3",
  origin: "core",
  revision: 0,
  nodes: [],
  edges: [],
  capability_catalog: [],
  capabilities: [],
});
let coreMode = snapshot.origin === "core";
let projection = { nodes: [], edges: [], hidden: 0 };
let selectedNodeId = null;
let focusNodeId = null;
let navigationHistory = [];
let collapsedGroups = new Set();
let filters = {};
let scale = 1;
let offset = { x: 0, y: 0 };
let keyboardIndex = -1;
let eventSource = null;
let reconnectCount = 0;
let operationalMode = false;
let csrfToken = null;
let workspaceReady = false;
let workspaceMode = "specimen";
const preparedUploads = new Map();
let jobsRefreshing = false;
let graphRefreshing = false;
let correlationsRefreshing = false;
let correlationRevision = null;
let plannerRevision = null;
let plannerValue = null;
let specializedView = "graph";
const pinnedPositions = new Map();
const reportSelection = new Set();
let reportPreview = null;
let acceptedSnapshotFingerprint = snapshotFingerprint(snapshot);
let previewSequence = 0;
let reportDraftKey = "labfy-report-draft:unbound";
let evidenceOpenSequence = 0;
let openedEvidenceId = null;

function saveReportDraft() {
  if (!operationalMode) return;
  const draft = {
    title: byId("report-name").value,
    comment: byId("report-comment").value,
    sections: [...document.querySelectorAll('[name="report-section"]')]
      .filter((input) => input.checked)
      .map((input) => input.value),
    selection: [...reportSelection],
  };
  sessionStorage.setItem(reportDraftKey, JSON.stringify(draft));
}

function restoreReportDraft() {
  try {
    const draft = JSON.parse(sessionStorage.getItem(reportDraftKey) ?? "null");
    if (!draft) return;
    byId("report-name").value = draft.title ?? "Rapport SPECIMEN";
    byId("report-comment").value = draft.comment ?? "";
    for (const input of document.querySelectorAll('[name="report-section"]'))
      input.checked = draft.sections?.includes(input.value) ?? input.checked;
    for (const id of draft.selection ?? [])
      if (snapshot.nodes.some((node) => node.id === id))
        reportSelection.add(id);
  } catch (_) {
    sessionStorage.removeItem(reportDraftKey);
  }
}

function invalidateReportPreview(
  message = "Aperçu à recalculer après modification.",
) {
  previewSequence += 1;
  reportPreview = null;
  byId("report-generate").disabled = true;
  if (byId("report-preview-content").textContent)
    byId("report-preview-content").textContent = message;
}

async function postCommand(path, value = {}) {
  const response = await fetch(path, {
    method: "POST",
    headers: { "Content-Type": "application/json", "X-Labfy-CSRF": csrfToken },
    body: JSON.stringify(value),
  });
  const result = await response.json();
  if (!response.ok) throw new Error(result.message ?? "Commande refusée");
  return result;
}

async function evidenceJson(path) {
  const response = await fetch(path, { cache: "no-store" });
  const value = await response.json();
  if (!response.ok) throw new Error(value.message ?? "Preuve indisponible");
  return value;
}

function reviewEnvelope(observation, reason) {
  return {
    operation_id: crypto.randomUUID(),
    expected_revision: String(observation.revision),
    author: "opérateur local",
    reason,
  };
}

async function mutateObservation(evidenceId, observation, operation, payload) {
  const result = await postCommand(
    `/api/v1/evidence/${evidenceId}/observations/${observation.id}/${operation}`,
    payload,
  );
  await Promise.all([
    refreshOperationalGraph(),
    refreshCorrelations(),
    refreshPlanner(),
  ]);
  if (openedEvidenceId === evidenceId) await loadEvidenceObservations(evidenceId);
  return result;
}

function observationActions(evidenceId, observation, item) {
  const controls = document.createElement("div");
  controls.className = "observation-actions";
  const reason = document.createElement("input");
  reason.placeholder = "Motif obligatoire";
  reason.maxLength = 512;
  reason.setAttribute("aria-label", `Motif pour ${observation.value_normalized}`);
  const correction = document.createElement("input");
  correction.placeholder = "Valeur corrigée";
  correction.maxLength = 512;
  const entity = document.createElement("input");
  entity.placeholder = "UUID d’entité à rattacher";
  entity.maxLength = 36;
  const add = (label, operation, build) => {
    const button = document.createElement("button");
    button.textContent = label;
    button.type = "button";
    button.addEventListener("click", async () => {
      if (!reason.value.trim()) {
        byId("evidence-status").textContent = "Un motif est obligatoire.";
        return;
      }
      button.disabled = true;
      try {
        await mutateObservation(evidenceId, observation, operation,
          build(reviewEnvelope(observation, reason.value.trim())));
        byId("evidence-status").textContent = `${label} : mutation journalisée.`;
      } catch (error) {
        byId("evidence-status").textContent = error.message;
        button.disabled = false;
      }
    });
    controls.append(button);
  };
  for (const [label, state] of [["Conserver proposée", "proposed"],
    ["Confirmer", "confirmed"], ["Rejeter", "rejected"],
    ["Contradictoire", "conflicted"]])
    add(label, "review", (base) => ({ ...base, action: "decide",
      verification_status: state, corrected_value: "" }));
  add("Corriger", "review", (base) => ({ ...base, action: "correct",
    verification_status: "", corrected_value: correction.value.trim() }));
  add("Créer un indicateur", "promotion", (base) => ({ ...base,
    action: "create", entity_id: "" }));
  add("Rattacher l’indicateur", "promotion", (base) => ({ ...base,
    action: "attach", entity_id: entity.value.trim() }));
  if (observation.entity_id)
    add("Retirer l’indicateur", "withdraw", (base) => base);
  controls.prepend(reason, correction, entity); item.append(controls);
}

async function loadEvidenceObservations(evidenceId) {
  const value = await evidenceJson(`/api/v1/evidence/${evidenceId}/observations`);
  if (openedEvidenceId !== evidenceId) return;
  const list = byId("observation-list"); list.replaceChildren();
  for (const observation of value.observations) {
    const item = document.createElement("li");
    const summary = document.createElement("p");
    summary.textContent = `${observation.entity_type} · ${observation.value_normalized}`+
      ` · ${observation.verification_status} · révision ${observation.revision}`;
    item.append(summary); observationActions(evidenceId, observation, item);
    list.append(item);
  }
  if (!value.observations.length) list.textContent = "Aucune observation persistée.";
}

async function openEvidence(evidenceId) {
  const sequence = ++evidenceOpenSequence;
  openedEvidenceId = evidenceId;
  byId("evidence-dialog").showModal();
  byId("evidence-status").textContent = "Chargement de la preuve…";
  byId("evidence-image").hidden = true; byId("evidence-text").hidden = true;
  byId("observation-list").replaceChildren();
  try {
    const [preview] = await Promise.all([
      evidenceJson(`/api/v1/evidence/${evidenceId}/preview`),
      loadEvidenceObservations(evidenceId),
    ]);
    // INVARIANT: une réponse lente d’une ancienne sélection ne remplace jamais
    // l’aperçu demandé plus récemment.
    if (sequence !== evidenceOpenSequence || openedEvidenceId !== evidenceId) return;
    byId("evidence-title").textContent = `Ouvrir la preuve — ${preview.display_name}`;
    if (preview.image_png_base64) {
      byId("evidence-image").src = `data:image/png;base64,${preview.image_png_base64}`;
      byId("evidence-image").hidden = false;
    } else if (preview.text !== null) {
      byId("evidence-text").textContent = preview.text;
      byId("evidence-text").hidden = false;
    }
    byId("evidence-status").textContent =
      `Intégrité vérifiée · cache ${preview.cache_id.slice(0, 12)} · aperçu ${preview.renderer_version}`;
  } catch (error) {
    if (sequence === evidenceOpenSequence)
      byId("evidence-status").textContent = error.message;
  }
}

async function refreshGraphAfterImport() {
  const response = await fetch("/api/v1/snapshot", { cache: "no-store" });
  const value = await response.json();
  if (!response.ok) throw new Error(value.message ?? "Projection indisponible");
  snapshot = prepareSnapshot(value); coreMode = true; render();
  await refreshPlanner();
}

async function receiveFile(file, selectionId) {
  const list = byId("import-list");
  const item = document.createElement("li");
  item.textContent = `${file.name} — réception…`; list.append(item);
  try {
    const intention = await postCommand("/api/v1/uploads", {
      name: file.name, size: String(file.size),
      declared_type: file.type || "application/octet-stream",
      selection_id: selectionId,
    });
    const response = await fetch(`/api/v1/uploads/${intention.upload_id}`, {
      method: "PUT", headers: {"Content-Type":"application/octet-stream",
        "X-Labfy-CSRF":csrfToken}, body:file,
    });
    const prepared = await response.json();
    if (!response.ok) throw new Error(prepared.message ?? "Réception refusée");
    preparedUploads.set(prepared.upload_id, prepared);
    item.textContent = `${prepared.name} — ${prepared.expected_size} octets — ${prepared.recognized_type} — préparé`;
    const remove=document.createElement("button");remove.textContent="Retirer";
    remove.addEventListener("click",async()=>{await postCommand(`/api/v1/uploads/${prepared.upload_id}/cancel`);preparedUploads.delete(prepared.upload_id);item.remove();byId("import-confirm").disabled=!preparedUploads.size;});
    item.append(" ",remove); byId("import-confirm").disabled=false;
  } catch (error) { item.textContent=`${file.name} — refusé : ${error.message}`; }
}

function configureImport() {
  byId("open-import").addEventListener("click",()=>byId("import-dialog").showModal());
  byId("import-files").addEventListener("change",async(event)=>{
    const files=[...event.target.files];
    if(files.length>8 || files.reduce((n,f)=>n+f.size,0)>16*1024*1024){byId("import-status").textContent="Sélection limitée à 8 fichiers et 16 Mio.";return;}
    // CONTRACT: la sélection reste multiple, mais le client respecte la
    // concurrence serveur au lieu de transformer le troisième fichier en
    // erreur artificielle. La limite reste contrôlée côté serveur.
    const selectionId=crypto.randomUUID();
    for(const file of files) await receiveFile(file,selectionId);
    event.target.value="";
  });
  byId("import-confirm").addEventListener("click",async()=>{
    byId("import-confirm").disabled=true;
    for(const [id,prepared] of [...preparedUploads]){
      try{await postCommand(`/api/v1/uploads/${id}/confirm`,{
        idempotency_key:crypto.randomUUID(),source:byId("import-source").value || "Non déclarée",
        description:byId("import-description").value || "Sans commentaire"});
        preparedUploads.delete(id);byId("import-status").textContent=`${prepared.name} importé avec l’UUID ${prepared.evidence_id}.`;
      }catch(error){byId("import-status").textContent=error.message;}
    }
    await refreshGraphAfterImport();byId("import-confirm").disabled=!preparedUploads.size;
    if (!preparedUploads.size) byId("import-dialog").close();
  });
}

async function refreshJobs() {
  if (jobsRefreshing) return;
  jobsRefreshing = true;
  try {
    const response = await fetch("/api/v1/jobs", { cache: "no-store" });
    if (!response.ok) return;
    const value = await response.json();
    if (
      value.contract !== "labfy.local_jobs.snapshot.v1" ||
      !Array.isArray(value.jobs)
    )
      return;
    const list = byId("jobs-list");
    list.replaceChildren();
    for (const job of value.jobs) {
      const item = document.createElement("li");
      item.textContent = `${job.capability_id} — ${job.state} — tentative ${job.attempt_count}`;
      item.dataset.jobId = job.job_id;
      item.dataset.jobState = job.state;
      if (
        ["QUEUED", "RETRY_WAIT", "RUNNING"].includes(job.state) &&
        operationalMode
      ) {
        const cancel = document.createElement("button");
        cancel.textContent = "Annuler";
        cancel.addEventListener(
          "click",
          () =>
            void postCommand(
              `/api/v1/jobs/${encodeURIComponent(job.job_id)}/cancel`,
            )
              .then(refreshJobs)
              .catch((error) => {
                byId("jobs-note").textContent = error.message;
              }),
        );
        item.append(" ", cancel);
      }
      if (job.state === "COMPLETED") {
        const result = document.createElement("button");
        result.textContent = "Voir le résultat";
        result.addEventListener("click", () =>
          selectNode(`evidence:${job.derivative_evidence_id}`),
        );
        item.append(" ", result);
      }
      list.append(item);
    }
    const plans = byId("plans-list");
    plans.replaceChildren();
    for (const plan of value.plans ?? []) {
      const item = document.createElement("li");
      const title = document.createElement("strong");
      title.textContent = `${plan.state} · ${plan.profile_id}`;
      const budget = document.createElement("p");
      budget.textContent =
        `Analyses réservées ${plan.reserved_analyses}/${plan.max_analyses} · ` +
        `tentatives consommées ${plan.consumed_attempts}, restantes ${plan.remaining_attempts} · ` +
        `temps actif consommé ${plan.consumed_active_ms} ms, restant ${plan.remaining_active_ms} ms · ` +
        `sources réservées ${plan.reserved_source_bytes}/${plan.max_source_bytes} octets.`;
      const outcome = document.createElement("p");
      outcome.textContent =
        `${plan.completed_jobs} terminé(s), ${plan.pending_jobs} en attente, ` +
        `${plan.failed_jobs} en échec, ${plan.cancelled_jobs} annulé(s), ` +
        `${plan.recovery_jobs} à réconcilier. Une limite atteinte ne prouve aucune absence de piste.`;
      item.append(title, budget, outcome);
      plans.append(item);
    }
  } catch (_) {
    // Le graphe reste utilisable lorsque l'export opérationnel est absent.
  } finally {
    jobsRefreshing = false;
  }
}

async function refreshOperationalGraph() {
  if (!operationalMode || graphRefreshing) return;
  graphRefreshing = true;
  try {
    const response = await fetch("/api/v1/snapshot", { cache: "no-store" });
    const value = await response.json();
    if (!response.ok) throw new Error(value.message ?? "export indisponible");
    const fingerprint = snapshotFingerprint(value);
    if (fingerprint !== acceptedSnapshotFingerprint) {
      const replacement = prepareSnapshot(value);
      const previousPositions = new Map(
        snapshot.nodes.map((node) => [node.id, { x: node.x, y: node.y }]),
      );
      replacement.nodes = replacement.nodes.map((node) => ({
        ...node,
        ...(previousPositions.get(node.id) ?? {}),
      }));
      const nodeIds = new Set(replacement.nodes.map((node) => node.id));
      if (
        replacement.edges.some(
          (edge) => !nodeIds.has(edge.source) || !nodeIds.has(edge.target),
        )
      )
        throw new Error("référence d’arête inconnue");
      snapshot = replacement;
      acceptedSnapshotFingerprint = fingerprint;
      byId("node-context-menu").hidden = true;
      syncTypeFilterOptions();
      if (selectedNodeId && !nodeIds.has(selectedNodeId)) selectedNodeId = null;
      for (const id of [...reportSelection]) {
        if (!nodeIds.has(id)) {
          reportSelection.delete(id);
          invalidateReportPreview(
            "Sélection absente après actualisation ; aperçu à recalculer.",
          );
        }
      }
      render({ focusDomNodeId: selectedNodeId });
      renderTimeline();
      renderReportSelection();
      byId("connection").textContent =
        "Poste local synchronisé · changements appliqués";
    } else {
      byId("connection").textContent =
        "Poste local synchronisé · sans changement";
    }
  } catch (error) {
    byId("connection").textContent = `Dernière vue valide · ${error.message}`;
  } finally {
    graphRefreshing = false;
  }
}

async function refreshCorrelations() {
  if (!operationalMode || correlationsRefreshing) return;
  correlationsRefreshing = true;
  try {
    const response = await fetch("/api/v1/correlations", { cache: "no-store" });
    const value = await response.json();
    if (
      !response.ok ||
      value.contract !== "labfy.local_correlation.snapshot.v1"
    )
      throw new Error(value.message ?? "index local indisponible");
    if (value.revision === correlationRevision) return;
    correlationRevision = value.revision;
    const list = byId("correlations-list");
    list.replaceChildren();
    for (const group of value.groups) {
      const item = document.createElement("li");
      const summary = document.createElement("button");
      summary.textContent = `${group.normalized} — ${group.distinct_evidence_count} preuves distinctes`;
      const details = document.createElement("div");
      details.hidden = true;
      details.className = "correlation-explanation";
      details.textContent =
        `${group.occurrence_count} occurrences · ${group.analysis_count} analyses · ` +
        `${group.distinct_content_count} contenus distincts. ${group.warning} ` +
        `Règle ${group.rule_version}.`;
      const connections = value.connections.filter(
        (connection) =>
          connection.source === group.id || connection.target === group.id,
      );
      if (connections.length > 0) {
        details.textContent +=
          ` ${connections.length} connexion(s) exploratoire(s) bornée(s) ` +
          `calculée(s) par le cœur C via une preuve commune.`;
      }
      const proofs = document.createElement("div");
      for (const memberId of group.members) {
        const observation = value.observations.find(
          (entry) => entry.id === memberId,
        );
        if (!observation) continue;
        const button = document.createElement("button");
        button.textContent = `Preuve ${observation.evidence_id.slice(0, 8)}`;
        button.addEventListener("click", () =>
          selectNode(`evidence:${observation.evidence_id}`),
        );
        proofs.append(button, " ");
      }
      summary.addEventListener("click", () => {
        details.hidden = !details.hidden;
      });
      item.append(summary, details, proofs);
      list.append(item);
    }
    byId("correlations-note").textContent =
      value.groups.length === 0
        ? "Aucun rapprochement calculé depuis les observations persistées."
        : `${value.groups.length} rapprochements calculés · ${value.complete ? "index complet" : "résultat borné"}`;
  } catch (error) {
    byId("correlations-note").textContent =
      `Index local indisponible · ${error.message}`;
  } finally {
    correlationsRefreshing = false;
  }
}

async function refreshPlanner() {
  if (!operationalMode) return;
  try {
    const response = await fetch("/api/v1/planner", { cache: "no-store" });
    const value = await response.json();
    if (!response.ok || value.contract !== "labfy.local_planner.snapshot.v1")
      throw new Error(value.message ?? "planner indisponible");
    if (value.input_revision === plannerRevision) return;
    plannerRevision = value.input_revision;
    plannerValue = value;
    const profiles = byId("planner-profile");
    profiles.replaceChildren();
    for (const profile of value.profiles)
      profiles.add(
        new Option(
          `${profile.id} · ${profile.max_analyses} analyses · ${profile.max_source_bytes} octets`,
          profile.id,
        ),
      );
    const list = byId("planner-list");
    list.replaceChildren();
    for (const recommendation of value.recommendations) {
      const item = document.createElement("li");
      const input = document.createElement("input");
      input.type = "checkbox";
      input.value = recommendation.id;
      input.disabled =
        !recommendation.available || recommendation.kind !== "ANALYSIS";
      input.addEventListener("change", () => {
        byId("planner-launch").disabled = !list.querySelector("input:checked");
      });
      const label = document.createElement("label");
      label.append(
        input,
        ` ${recommendation.kind} · ${recommendation.priority} · ${recommendation.reason}`,
      );
      if (recommendation.kind === "NAVIGATION") {
        const open = document.createElement("button");
        open.textContent = "Voir la preuve";
        open.addEventListener("click", () =>
          selectNode(`evidence:${recommendation.object_id}`),
        );
        item.append(open);
      }
      const expert = document.createElement("details");
      const summary = document.createElement("summary");
      summary.textContent = "Détails et préconditions";
      const body = document.createElement("p");
      body.textContent =
        `${recommendation.capability_id} · adapter ${recommendation.adapter_id} ` +
        `v${recommendation.adapter_version} · ${recommendation.source_size} octets · réseau NONE · ${recommendation.reason_code}`;
      expert.append(summary, body);
      item.append(label, expert);
      list.append(item);
    }
    byId("planner-note").textContent = value.recommendations.length
      ? "Gain d'information attendu : heuristique locale, jamais un score d'identité."
      : "Aucune action locale pertinente dans cette projection.";
  } catch (error) {
    byId("planner-note").textContent =
      `Planner indisponible · ${error.message}`;
  }
}

const renderer = new GraphRenderer({
  svg,
  viewport: byId("viewport"),
  edgesLayer: byId("edges"),
  nodesLayer: byId("nodes"),
  pinned: pinnedPositions,
  callbacks: {
    onNodeSelect: (nodeId, preserveDomFocus = false) =>
      selectNode(nodeId, { preserveDomFocus }),
    onEdgeSelect: showEdgeDetails,
  },
});

function presentationState() {
  return {
    selectedNodeId,
    focusNodeId,
    filters: { ...filters },
    collapsedGroups: new Set(collapsedGroups),
    scale,
    offset: { ...offset },
    specializedView,
  };
}

function restorePresentation(state) {
  selectedNodeId = state.selectedNodeId;
  focusNodeId = state.focusNodeId;
  filters = { ...state.filters };
  collapsedGroups = new Set(state.collapsedGroups);
  scale = state.scale;
  offset = { ...state.offset };
  specializedView = state.specializedView ?? "graph";
  byId("type-filter").value = filters.type ?? "";
  byId("state-filter").value = filters.state ?? "";
  updateProjectionControls();
}

function selectedNode() {
  return snapshot.nodes.find((node) => node.id === selectedNodeId) ?? null;
}

function syncTypeFilterOptions() {
  const select = byId("type-filter");
  const selected = filters.type ?? "";
  select.replaceChildren(new Option("Tous", ""));
  for (const type of [
    ...new Set(snapshot.nodes.map((node) => node.type)),
  ].sort())
    select.add(new Option(type, type));
  select.value = [...select.options].some((option) => option.value === selected)
    ? selected
    : "";
  if (select.value !== selected) filters.type = "";
}

function render({ focusDomNodeId = null } = {}) {
  const activeNodeId = document.activeElement?.dataset?.id ?? focusDomNodeId;
  projection = visibleGraph(snapshot, filters, collapsedGroups, focusNodeId);
  if (specializedView === "evidence") {
    const allowed = new Set(
      projection.nodes
        .filter((node) =>
          ["evidence", "extraction", "observation"].includes(node.object_kind),
        )
        .map((node) => node.id),
    );
    projection = {
      ...projection,
      nodes: projection.nodes.filter((node) => allowed.has(node.id)),
      edges: projection.edges.filter(
        (edge) => allowed.has(edge.source) && allowed.has(edge.target),
      ),
    };
  } else if (specializedView === "infrastructure") {
    const allowed = new Set(
      projection.nodes
        .filter((node) =>
          ["email_address", "domain_name", "ip_address"].includes(node.type),
        )
        .map((node) => node.id),
    );
    projection = {
      ...projection,
      nodes: projection.nodes.filter((node) => allowed.has(node.id)),
      edges: projection.edges.filter(
        (edge) => allowed.has(edge.source) && allowed.has(edge.target),
      ),
    };
  }
  projection.hidden = snapshot.nodes.length - projection.nodes.length;
  const visibleIds = new Set(projection.nodes.map((node) => node.id));

  if (selectedNodeId && !visibleIds.has(selectedNodeId)) selectedNodeId = null;
  if (keyboardIndex >= projection.nodes.length) {
    keyboardIndex = projection.nodes.length - 1;
  }

  byId("hidden-count").textContent = `${projection.hidden} masqué${
    projection.hidden > 1 ? "s" : ""
  }`;
  byId("graph-state").textContent =
    projection.nodes.length === 0
      ? "Aucun objet ne correspond à la projection active."
      : "";
  byId("back").disabled = navigationHistory.length === 0;
  byId("collapse").textContent = collapsedGroups.has("evidence")
    ? "Déplier Evidence"
    : "Replier Evidence";

  renderer.render(projection, {
    selectedNodeId,
    scale,
    offset,
    focusNodeId: visibleIds.has(activeNodeId) ? activeNodeId : null,
  });
  renderObjectList();
  renderSelection();
}

function renderObjectList() {
  const list = byId("object-list");
  list.replaceChildren();
  projection.nodes.forEach((node, index) => {
    const item = document.createElement("li");
    const button = document.createElement("button");
    button.textContent = `${node.type} — ${node.label}`;
    button.addEventListener("click", () => selectNode(node.id));
    button.addEventListener("focus", () => {
      keyboardIndex = index;
    });
    item.append(button);
    list.append(item);
  });
}

function renderSelection() {
  const details = byId("details");
  const actions = byId("actions");
  details.replaceChildren();
  actions.replaceChildren();
  const node = selectedNode();
  if (!node) {
    details.textContent = "Sélectionnez un objet.";
    actions.textContent = "Aucune action sans sélection valide.";
    byId("provenance-mirror").textContent = "Sélectionnez un objet.";
    return;
  }

  const title = document.createElement("h3");
  title.textContent = node.label;
  details.append(title);
  for (const key of ["type", "state"]) {
    const paragraph = document.createElement("p");
    paragraph.textContent = `${key} : ${node[key]}`;
    details.append(paragraph);
  }
  const expert = document.createElement("details");
  const expertSummary = document.createElement("summary");
  expertSummary.textContent = "Détails techniques et valeurs persistées";
  expert.append(expertSummary);
  const identifier = document.createElement("p");
  identifier.textContent = `object_id : ${node.object_id}`;
  expert.append(identifier);
  if (node.raw) {
    const raw = document.createElement("div");
    raw.className = "raw";
    raw.textContent = node.raw;
    expert.append(raw);
  }
  if (node.details) {
    for (const [key, value] of Object.entries(node.details)) {
      if (value === null || value === "" || key === "provenance_complete")
        continue;
      const paragraph = document.createElement("p");
      paragraph.textContent = `${key} : ${value}`;
      expert.append(paragraph);
    }
    if (node.details.provenance_complete === false) {
      const warning = document.createElement("p");
      warning.className = "warning";
      warning.textContent = `Chaîne de provenance incomplète : ${
        node.details.missing_provenance_reason ?? "origine non persistée"
      }`;
      details.append(warning);
    }
  }
  details.append(expert);
  renderProvenance(details, node.id, "provenance-details");
  const provenanceMirror = byId("provenance-mirror");
  provenanceMirror.replaceChildren();
  renderProvenance(provenanceMirror, node.id, "provenance-tab-details");
  if (!provenanceMirror.hasChildNodes())
    provenanceMirror.textContent = "Aucune origine supplémentaire persistée.";
  renderActions(actions, node);
}

function renderProvenance(container, nodeId, sectionId) {
  const provenance = provenanceOrigins(snapshot, nodeId);
  const usefulBranches = provenance.branches.filter(
    (branch) => branch.length > 1,
  );
  if (usefulBranches.length === 0 && !provenance.cycleDetected) return;

  const section = document.createElement("section");
  section.id = sectionId;
  const title = document.createElement("h4");
  title.textContent = "Origines distinctes";
  section.append(title);
  for (const branch of usefulBranches) {
    const origin = branch.at(-1);
    const button = document.createElement("button");
    button.textContent = branch.map((node) => node.label).join(" ← ");
    button.dataset.originId = origin.id;
    button.addEventListener("click", () => navigateToNode(origin.id, true));
    section.append(button);
  }
  if (provenance.missingSourceIds.length > 0) {
    const warning = document.createElement("p");
    warning.textContent = `Sources absentes : ${provenance.missingSourceIds.join(", ")}`;
    section.append(warning);
  }
  if (provenance.cycleDetected || provenance.truncated) {
    const warning = document.createElement("p");
    warning.textContent = provenance.cycleDetected
      ? "Cycle de provenance détecté ; parcours arrêté."
      : "Parcours de provenance borné atteint.";
    section.append(warning);
  }
  container.append(section);
}

function navigateToNode(nodeId, preserveReturn = false) {
  if (preserveReturn) navigationHistory.push(presentationState());
  if (projection.nodes.some((node) => node.id === nodeId))
    return selectNode(nodeId);
  if (!preserveReturn) navigationHistory.push(presentationState());
  specializedView = "graph";
  filters = {};
  focusNodeId = null;
  byId("type-filter").value = "";
  byId("state-filter").value = "";
  byId("timeline-panel").hidden = true;
  updateProjectionControls();
  render();
  return selectNode(nodeId);
}

function renderActions(container, node) {
  const capabilities = capabilitiesForNode(snapshot, node.id);
  const evidenceButton = document.createElement("button");
  evidenceButton.textContent = "Voir les éléments justificatifs";
  evidenceButton.addEventListener("click", () =>
    activateInspectorPanel("provenance-panel"),
  );
  container.append(evidenceButton);
  if (node.object_kind === "evidence") {
    const open = document.createElement("button");
    open.textContent = "Ouvrir la preuve";
    open.addEventListener("click", () => openEvidence(node.object_id));
    container.append(open);
  }
  const reportButton = document.createElement("button");
  reportButton.textContent = reportSelection.has(node.id)
    ? "Retirer du rapport"
    : "Ajouter au rapport";
  reportButton.dataset.reportAction = "toggle";
  reportButton.addEventListener("click", () => {
    if (reportSelection.has(node.id)) reportSelection.delete(node.id);
    else reportSelection.add(node.id);
    invalidateReportPreview();
    saveReportDraft();
    renderReportSelection();
    renderSelection();
  });
  container.append(reportButton);
  if (capabilities.length === 0) {
    const note = document.createElement("span");
    note.className = "reason";
    note.textContent = "Aucune autre capability déclarée pour cet objet.";
    container.append(note);
  }
  for (const capability of capabilities) {
    const wrapper = document.createElement("div");
    const button = document.createElement("button");
    button.textContent = capability.intent;
    button.disabled = !capability.available;
    button.dataset.capabilityId = capability.id;
    button.addEventListener("click", () => runCapability(capability));
    const reason = document.createElement("span");
    reason.className = "reason";
    reason.textContent = `${capability.network_contact} · ${capability.reason}`;
    wrapper.append(button, reason);
    container.append(wrapper);
  }
}

function renderTimeline() {
  const body = byId("timeline-list");
  body.replaceChildren();
  const events = [];
  for (const node of snapshot.nodes) {
    for (const [field, category] of [
      ["started_at", "début de traitement"],
      ["finished_at", "fin de traitement"],
    ]) {
      const value = node.details?.[field];
      if (value) events.push({ value, category, node });
    }
  }
  events.sort(
    (a, b) =>
      a.value.localeCompare(b.value) || a.node.id.localeCompare(b.node.id),
  );
  for (const event of events) {
    const row = document.createElement("tr");
    for (const value of [event.value, event.category, event.node.label]) {
      const cell = document.createElement("td");
      cell.textContent = value;
      row.append(cell);
    }
    row.tabIndex = 0;
    row.addEventListener("click", () => selectNode(event.node.id));
    body.append(row);
  }
  if (!events.length) {
    const row = document.createElement("tr"),
      cell = document.createElement("td");
    cell.colSpan = 3;
    cell.textContent =
      "Aucune date positionnable ; date du message non disponible dans ce parcours.";
    row.append(cell);
    body.append(row);
  }
}

function renderReportSelection() {
  const list = byId("report-selection");
  list.replaceChildren();
  for (const id of [...reportSelection].sort()) {
    const node = snapshot.nodes.find((item) => item.id === id);
    if (!node) continue;
    const item = document.createElement("li"),
      button = document.createElement("button");
    button.textContent = `Retirer ${node.label}`;
    button.addEventListener("click", () => {
      reportSelection.delete(id);
      invalidateReportPreview();
      saveReportDraft();
      renderReportSelection();
    });
    item.append(button);
    list.append(item);
  }
  byId("report-count").textContent =
    `${reportSelection.size} objet(s) sélectionné(s) · fermeture de provenance calculée par C à la prévisualisation`;
  byId("report-preview").disabled =
    !operationalMode || reportSelection.size === 0;
  byId("report-generate").disabled = !reportPreview;
}

async function previewReport() {
  const sequence = ++previewSequence;
  const sections = [
    ...document.querySelectorAll('[name="report-section"]:checked'),
  ].map((x) => x.value);
  const intent = {
    object_ids: [...reportSelection],
    title: byId("report-name").value,
    comment: byId("report-comment").value,
    profile: "MINIMAL",
    sections,
  };
  const preview = await postCommand("/api/v1/reports/preview", intent);
  if (sequence !== previewSequence) return;
  reportPreview = preview;
  const selected = preview.objects.filter(
    (x) => x.selection_role === "selected",
  ).length;
  const dependencies = preview.objects.length - selected;
  byId("report-preview-content").textContent =
    `Coupe ${preview.revision.slice(0, 12)} · ${selected} sélectionné(s) · ${dependencies} justificatif(s) · ${preview.timeline.length} événement(s).`;
  byId("report-generate").disabled = false;
  renderReportSelection();
}

async function generateReport() {
  if (!reportPreview) return;
  const admission = await postCommand("/api/v1/reports", {
    preview_revision: reportPreview.revision,
    idempotency_key: crypto.randomUUID(),
  });
  byId("report-status").textContent = "Génération locale en cours…";
  for (let i = 0; i < 100; i++) {
    await new Promise((resolve) => setTimeout(resolve, 100));
    const response = await fetch(`/api/v1/reports/${admission.report_id}`, {
        cache: "no-store",
      }),
      status = await response.json();
    if (status.state === "READY") {
      const box = byId("report-status");
      box.className = "report-ready";
      box.replaceChildren(document.createTextNode("Dossier prêt : "));
      for (const name of [
        "report.html",
        "report.json",
        "report.pdf",
        "manifest.json",
        "NOTICE.txt",
      ]) {
        const link = document.createElement("a");
        link.href = `/api/v1/reports/${admission.report_id}/${name}`;
        link.textContent = name;
        link.style.marginRight = ".5rem";
        box.append(link);
      }
      return;
    }
    if (status.state === "FAILED" || status.state === "CORRUPTED")
      throw new Error(status.error || "Génération échouée");
  }
  throw new Error("Génération expirée");
}

async function runCapability(capability) {
  if (!selectedNodeId || !capability.available) return;
  if (capability.id === "focus-neighborhood") {
    navigationHistory.push(presentationState());
    focusNodeId = selectedNodeId;
    render({ focusDomNodeId: selectedNodeId });
  }
  if (capability.id === "show-provenance") {
    activateInspectorPanel("provenance-panel");
  }
  if (
    operationalMode &&
    [
      "labfy.capability.eml_headers.v1",
      "labfy.capability.exif_metadata.v1",
    ].includes(capability.id)
  ) {
    const node = selectedNode();
    const button = document.querySelector(
      `[data-capability-id="${capability.id}"]`,
    );
    if (button) button.disabled = true;
    const storageKey = `labfy-intent:${node.object_id}:${capability.id}`;
    const key = sessionStorage.getItem(storageKey) ?? crypto.randomUUID();
    sessionStorage.setItem(storageKey, key);
    try {
      await postCommand("/api/v1/jobs", {
        evidence_id: node.object_id,
        capability_id: capability.id,
        idempotency_key: key,
      });
      sessionStorage.removeItem(storageKey);
      byId("jobs-note").textContent = "Analyse admise par le cœur C.";
      await refreshJobs();
    } catch (error) {
      byId("jobs-note").textContent = error.message;
      if (button) button.disabled = false;
    }
  }
}

function selectNode(nodeId, { preserveDomFocus = false } = {}) {
  if (!projection.nodes.some((node) => node.id === nodeId)) {
    byId("graph-state").textContent =
      "Objet absent de la projection : sélection rejetée.";
    return false;
  }
  selectedNodeId = nodeId;
  byId("node-context-menu").hidden = true;
  keyboardIndex = projection.nodes.findIndex((node) => node.id === nodeId);
  render({ focusDomNodeId: preserveDomFocus ? nodeId : null });
  return true;
}

function showEdgeDetails(edgeId) {
  const edge = projection.edges.find((candidate) => candidate.id === edgeId);
  if (!edge) return;
  const details = byId("details");
  details.replaceChildren();
  const title = document.createElement("h3");
  title.textContent = `Relation ${edge.kind} · ${edge.id}`;
  const description = document.createElement("p");
  description.textContent = `${edge.source} → ${edge.target} · ${
    edge.directed ? "dirigée" : "non dirigée"
  } · ${edge.review_state}`;
  details.append(title, description);
}

function resetGlobalView() {
  navigationHistory = [];
  selectedNodeId = null;
  focusNodeId = null;
  filters = {};
  collapsedGroups.clear();
  scale = 1;
  offset = { x: 0, y: 0 };
  keyboardIndex = -1;
  specializedView = "graph";
  byId("timeline-panel").hidden = true;
  byId("type-filter").value = "";
  byId("state-filter").value = "";
  updateProjectionControls();
  render();
}

async function resyncSnapshot(reason) {
  byId("connection").textContent = `Rattrapage snapshot — ${reason}`;
  try {
    const response = await fetch("/api/v1/snapshot?size=demo&revision=latest");
    const replacement = await response.json();
    if (
      !response.ok ||
      replacement.contract !== "labfy.web_graph.snapshot.v1" ||
      !Number.isInteger(replacement.revision)
    ) {
      throw new Error("contrat snapshot inattendu");
    }
    snapshot = replacement;
    render();
    byId("connection").textContent = `Rattrapé · révision ${snapshot.revision}`;
  } catch (error) {
    byId("connection").textContent = `Erreur de rattrapage · ${error.message}`;
  }
}

async function processEventData(data) {
  let event;
  try {
    event = JSON.parse(data);
  } catch (_error) {
    byId("connection").textContent = "Événement invalide · JSON illisible";
    byId("graph-state").textContent = "Flux rejeté : événement JSON illisible.";
    return "invalid-json";
  }
  const result = applyGraphEvent(snapshot, event);
  if (result.status === "applied") {
    snapshot = result.snapshot;
    render();
    byId("connection").textContent =
      `Synchronisé · révision ${snapshot.revision}`;
  } else if (result.status === "duplicate") {
    byId("connection").textContent =
      `Doublon ignoré · révision ${snapshot.revision}`;
  } else if (result.status === "gap" || result.status === "resync") {
    await resyncSnapshot(
      result.status === "gap" ? "trou de révision" : "curseur inconnu",
    );
  } else if (result.status === "end") {
    eventSource?.close();
    byId("connection").textContent =
      `Scénario terminé · révision ${snapshot.revision}`;
  } else {
    byId("connection").textContent = "Événement invalide · contrat rejeté";
    byId("graph-state").textContent =
      "Flux rejeté : contrat d'événement inattendu.";
  }
  return result.status;
}

function connectEvents() {
  eventSource = new EventSource("/api/v1/events?pace=demo");
  eventSource.addEventListener("graph-update", (event) => {
    void processEventData(event.data);
  });
  eventSource.onerror = () => {
    if (eventSource.readyState !== EventSource.CLOSED) {
      reconnectCount += 1;
      byId("connection").textContent = "Reconnexion au scénario synthétique…";
    }
  };
}

function activateInspectorPanel(panelId) {
  for (const candidate of document.querySelectorAll("[data-panel]")) {
    const active = candidate.dataset.panel === panelId;
    candidate.setAttribute("aria-selected", String(active));
    byId(candidate.dataset.panel).hidden = !active;
    if (active) candidate.focus();
  }
  byId(panelId).querySelector("button")?.focus();
}

function updateProjectionControls() {
  const activeId =
    specializedView === "evidence"
      ? "view-evidence"
      : specializedView === "infrastructure"
        ? "view-infrastructure"
        : byId("timeline-panel").hidden
          ? "reset"
          : "view-timeline";
  for (const button of document.querySelectorAll(".projection-tab")) {
    const active = button.id === activeId;
    button.classList.toggle("active", active);
    button.setAttribute("aria-pressed", String(active));
  }
}

function configureControls() {
  for (const tab of document.querySelectorAll("[data-panel]")) {
    tab.addEventListener("click", () =>
      activateInspectorPanel(tab.dataset.panel),
    );
  }
  for (const tab of document.querySelectorAll("[data-work-panel]")) {
    tab.addEventListener("click", () => {
      for (const candidate of document.querySelectorAll("[data-work-panel]")) {
        const active = candidate === tab;
        candidate.setAttribute("aria-selected", String(active));
        byId(candidate.dataset.workPanel).hidden = !active;
      }
    });
  }
  byId("work-panel-toggle").addEventListener("click", (event) => {
    const collapsed = byId("work-panel").classList.toggle("collapsed");
    document
      .querySelector(".workbench")
      .classList.toggle("drawer-collapsed", collapsed);
    event.currentTarget.setAttribute("aria-expanded", String(!collapsed));
    event.currentTarget.textContent = collapsed ? "Déployer" : "Réduire";
    requestAnimationFrame(() => renderer.updateViewport(scale, offset));
  });
  byId("view-evidence").addEventListener("click", () => {
    specializedView = "evidence";
    byId("timeline-panel").hidden = true;
    updateProjectionControls();
    render();
  });
  byId("view-infrastructure").addEventListener("click", () => {
    specializedView = "infrastructure";
    byId("timeline-panel").hidden = true;
    updateProjectionControls();
    render();
  });
  byId("view-timeline").addEventListener("click", () => {
    specializedView = "graph";
    byId("timeline-panel").hidden = false;
    updateProjectionControls();
    renderTimeline();
    render();
  });
  byId("search-button").addEventListener("click", () => {
    const query = byId("search").value.trim().toLocaleLowerCase("fr");
    if (!query) return;
    const node = snapshot.nodes.find(
      (candidate) =>
        candidate.label.toLocaleLowerCase("fr").includes(query) ||
        candidate.id.toLocaleLowerCase("fr").includes(query),
    );
    if (!node) {
      byId("graph-state").textContent = "Aucun objet synthétique trouvé.";
      return;
    }
    if (!projection.nodes.some((candidate) => candidate.id === node.id)) {
      byId("graph-state").textContent =
        "Résultat masqué par la projection active.";
      return;
    }
    const position = renderer.position(node);
    offset = { x: -position.x * scale, y: -position.y * scale };
    selectNode(node.id);
  });
  byId("search").addEventListener("keydown", (event) =>
    event.stopPropagation(),
  );

  byId("back").addEventListener("click", () => {
    const previous = navigationHistory.pop();
    if (!previous) return;
    restorePresentation(previous);
    render({ focusDomNodeId: selectedNodeId });
  });
  byId("collapse").addEventListener("click", () => {
    if (collapsedGroups.has("evidence")) collapsedGroups.delete("evidence");
    else collapsedGroups.add("evidence");
    render();
  });
  byId("reset").addEventListener("click", resetGlobalView);
  byId("reset-layout").addEventListener("click", () => {
    pinnedPositions.clear();
    snapshot = prepareSnapshot(snapshot);
    scale = fitGraphScale();
    offset = { x: 0, y: 0 };
    render({ focusDomNodeId: selectedNodeId });
  });
  byId("type-filter").addEventListener("change", (event) => {
    filters.type = event.target.value;
    render();
  });
  byId("state-filter").addEventListener("change", (event) => {
    filters.state = event.target.value;
    render();
  });
}

function openNodeContextMenu(nodeId, clientX, clientY) {
  if (!selectNode(nodeId)) return;
  const menu = byId("node-context-menu");
  menu.replaceChildren();
  const title = document.createElement("strong");
  title.textContent = selectedNode()?.label ?? "Objet";
  menu.append(title);
  renderActions(menu, selectedNode());
  menu.hidden = false;
  const panel = byId("graph-panel").getBoundingClientRect();
  const x = Math.max(
    8,
    Math.min(clientX - panel.left, panel.width - menu.offsetWidth - 8),
  );
  const y = Math.max(
    8,
    Math.min(clientY - panel.top, panel.height - menu.offsetHeight - 8),
  );
  menu.style.left = `${x}px`;
  menu.style.top = `${y}px`;
  menu.querySelector("button")?.focus();
}

function configureGraphInteractions() {
  let pan = null;
  svg.addEventListener("pointerdown", (event) => {
    if (event.target !== svg && event.target.id !== "graph-background") return;
    pan = {
      pointerId: event.pointerId,
      x: event.clientX,
      y: event.clientY,
      offset: { ...offset },
    };
    svg.setPointerCapture(event.pointerId);
  });
  svg.addEventListener("pointermove", (event) => {
    if (!pan || event.pointerId !== pan.pointerId) return;
    offset = {
      x: pan.offset.x + event.clientX - pan.x,
      y: pan.offset.y + event.clientY - pan.y,
    };
    renderer.updateViewport(scale, offset);
  });
  const finishPan = (event) => {
    if (!pan || event.pointerId !== pan.pointerId) return;
    if (svg.hasPointerCapture(event.pointerId))
      svg.releasePointerCapture(event.pointerId);
    pan = null;
  };
  svg.addEventListener("pointerup", finishPan);
  svg.addEventListener("pointercancel", finishPan);
  svg.addEventListener(
    "wheel",
    (event) => {
      event.preventDefault();
      scale = Math.max(
        0.25,
        Math.min(3, scale * (event.deltaY < 0 ? 1.1 : 0.9)),
      );
      renderer.updateViewport(scale, offset);
    },
    { passive: false },
  );
  svg.addEventListener("keydown", (event) => {
    if (event.target !== svg && !event.target.classList.contains("node"))
      return;
    if (projection.nodes.length === 0) return;
    if (["ArrowRight", "ArrowDown"].includes(event.key)) {
      keyboardIndex =
        (keyboardIndex + 1 + projection.nodes.length) % projection.nodes.length;
    } else if (["ArrowLeft", "ArrowUp"].includes(event.key)) {
      keyboardIndex =
        (keyboardIndex - 1 + projection.nodes.length) % projection.nodes.length;
    } else if (["ContextMenu", "F10"].includes(event.key)) {
      if (event.key === "F10" && !event.shiftKey) return;
      event.preventDefault();
      const node = projection.nodes[Math.max(0, keyboardIndex)];
      if (node)
        openNodeContextMenu(
          node.id,
          svg.getBoundingClientRect().left + svg.clientWidth / 2,
          svg.getBoundingClientRect().top + svg.clientHeight / 2,
        );
      return;
    } else if (event.key !== "Enter") {
      return;
    }
    event.preventDefault();
    const node = projection.nodes[Math.max(0, keyboardIndex)];
    if (node) selectNode(node.id, { preserveDomFocus: true });
  });
  svg.addEventListener("contextmenu", (event) => {
    const element = event.target.closest?.(".node");
    if (!element?.dataset.id) return;
    event.preventDefault();
    openNodeContextMenu(element.dataset.id, event.clientX, event.clientY);
  });
  document.addEventListener("keydown", (event) => {
    if (event.key === "Escape" && !byId("node-context-menu").hidden) {
      byId("node-context-menu").hidden = true;
      svg.focus();
    }
  });
  document.addEventListener("pointerdown", (event) => {
    if (
      !event.target.closest?.("#node-context-menu") &&
      !event.target.closest?.(".node")
    )
      byId("node-context-menu").hidden = true;
  });
}

async function start() {
  try {
    const initialResponse = await fetch("/api/v1/snapshot", {
      cache: "no-store",
    });
    const initialValue = await initialResponse.json();
    snapshot = prepareSnapshot(initialValue);
    acceptedSnapshotFingerprint = snapshotFingerprint(initialValue);
    coreMode = snapshot.origin === "core";
  } catch (_) {}
  scale = coreMode
    ? fitGraphScale()
    : Math.min(1, Math.max(0.6, (svg.clientWidth - 160) / 700));
  syncTypeFilterOptions();
  try {
    const sessionResponse = await fetch("/api/v1/session", {
      cache: "no-store",
    });
    if (sessionResponse.ok) {
      const session = await sessionResponse.json();
      csrfToken = session.csrf;
      operationalMode = true;
      workspaceReady = session.workspace_state === "READY";
      byId("workspace-create").hidden = workspaceReady;
      byId("open-import").disabled = !workspaceReady;
      if (session.title) byId("investigation-title").textContent=session.title;
      workspaceMode = session.mode;
      byId("mode-badge").textContent = session.mode === "specimen"
        ? "Mode SPECIMEN" : "Local expérimental";
      const investigation =
        snapshot.investigation?.id ?? snapshot.investigation_id ?? "unknown";
      reportDraftKey = `labfy-report-draft:${session.contract}:${session.origin}:${investigation}`;
    }
  } catch (_) {}
  if (coreMode) {
    byId("mode-badge").textContent = workspaceMode === "specimen"
      ? (operationalMode ? "Poste J6 · SPECIMEN · contrôles locaux" : "Snapshot du cœur C · SPECIMEN · lecture seule")
      : "Local expérimental · contrôles locaux";
    byId("connection").textContent = snapshot._loadError
      ? `Erreur d'export cœur · ${snapshot._loadError}`
      : "Lecture seule — snapshot cœur chargé";
  } else {
    byId("connection").textContent = `Connecté · révision ${snapshot.revision}`;
  }
  configureControls();
  configureGraphInteractions();
  render();
  if (snapshot._loadError) {
    byId("graph-state").textContent =
      `Snapshot cœur indisponible : ${snapshot._loadError}`;
  }
  if (!coreMode) connectEvents();
  if (operationalMode) {
    byId("workspace-create-form").addEventListener("submit",async(event)=>{
      event.preventDefault();
      try{await postCommand("/api/v1/workspace",{title:byId("workspace-title").value});
        workspaceReady=true;byId("workspace-create").hidden=true;byId("open-import").disabled=false;
        await refreshGraphAfterImport();
      }catch(error){byId("workspace-create-status").textContent=error.message;}
    });
    configureImport();
    byId("jobs-note").textContent = "Contrôle local authentifié.";
    for (const [id, command] of [
      ["queue-pause", "pause"],
      ["queue-resume", "resume"],
      ["queue-stop", "stop"],
    ]) {
      byId(id).addEventListener(
        "click",
        () =>
          void postCommand(`/api/v1/queue/${command}`)
            .then(() => refreshJobs())
            .catch((error) => {
              byId("jobs-note").textContent = error.message;
            }),
      );
    }
    await refreshJobs();
    await refreshCorrelations();
    await refreshPlanner();
    byId("planner-launch").addEventListener("click", async () => {
      const ids = [
        ...byId("planner-list").querySelectorAll("input:checked"),
      ].map((input) => input.value);
      if (!ids.length || !plannerValue) return;
      const key = crypto.randomUUID();
      try {
        await postCommand("/api/v1/plans", {
          recommendation_ids: ids,
          input_revision: plannerValue.input_revision,
          profile_id: byId("planner-profile").value,
          idempotency_key: key,
        });
        byId("planner-note").textContent =
          "Plan durable admis ; exécution locale en cours.";
        await refreshJobs();
      } catch (error) {
        byId("planner-note").textContent = error.message;
      }
    });
    byId("report-preview").addEventListener(
      "click",
      () =>
        void previewReport().catch((error) => {
          byId("report-preview-content").textContent = error.message;
        }),
    );
    byId("report-generate").addEventListener(
      "click",
      () =>
        void generateReport().catch((error) => {
          byId("report-status").textContent = error.message;
        }),
    );
    restoreReportDraft();
    for (const input of [
      byId("report-name"),
      byId("report-comment"),
      ...document.querySelectorAll('[name="report-section"]'),
    ]) {
      input.addEventListener("input", () => {
        invalidateReportPreview();
        saveReportDraft();
      });
    }
    renderTimeline();
    renderReportSelection();
    setInterval(refreshJobs, 500);
    setInterval(refreshOperationalGraph, 700);
    setInterval(refreshCorrelations, 900);
    setInterval(refreshPlanner, 1100);
  } else {
    byId("queue-controls").hidden = true;
  }
}

// Ce crochet ne transporte aucune donnée réelle ; il permet au test navigateur
// d'observer l'état de présentation et d'injecter des contrats synthétiques.
window.__LABFY_TEST__ = {
  getState: () => ({
    revision: snapshot.revision,
    selectedNodeId,
    focusNodeId,
    visibleNodeIds: projection.nodes.map((node) => node.id),
    pinned: Object.fromEntries(pinnedPositions),
    scale,
    offset: { ...offset },
    reconnectCount,
    coreMode,
    contract: snapshot.contract,
  }),
  processEventData,
  selectNode,
};

void start();
