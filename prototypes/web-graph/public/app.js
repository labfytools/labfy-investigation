import {
  applyGraphEvent,
  capabilitiesForNode,
  provenanceOrigins,
  visibleGraph,
} from "./graph-state.js";
import { GraphRenderer, presentationKind } from "./graph-renderer.js";

const byId = (id) => document.getElementById(id);
const svg = byId("graph");

function newUuid() {
  // CONTRACT: le poste local peut être servi en HTTP sous un nom dédié. Firefox
  // y expose getRandomValues, mais réserve randomUUID aux contextes sécurisés.
  const bytes = new Uint8Array(16);
  crypto.getRandomValues(bytes);
  bytes[6] = (bytes[6] & 0x0f) | 0x40;
  bytes[8] = (bytes[8] & 0x3f) | 0x80;
  const hex = Array.from(bytes, (value) => value.toString(16).padStart(2, "0")).join("");
  return `${hex.slice(0, 8)}-${hex.slice(8, 12)}-${hex.slice(12, 16)}-${hex.slice(16, 20)}-${hex.slice(20)}`;
}

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
let graph_scale_initialized = false;
let offset = { x: 0, y: 0 };
let keyboardIndex = -1;
let eventSource = null;
let reconnectCount = 0;
let operationalMode = false;
let csrfToken = null;
let workspaceReady = false;
let libraryMode = false;
const preparedUploads = new Map();
let jobsRefreshing = false;
let graphRefreshing = false;
let correlationsRefreshing = false;
let correlationRevision = null;
let plannerRevision = null;
let plannerValue = null;
let researchValue = null;
let specializedView = "graph";
const pinnedPositions = new Map();
const reportSelection = new Set();
let reportPreview = null;
let acceptedSnapshotFingerprint = snapshotFingerprint(snapshot);
let previewSequence = 0;
let reportDraftKey = "labfy-report-draft:unbound";
let evidenceOpenSequence = 0;
let openedEvidenceId = null;
let libraryGeneration = null;
let pendingCreateIntent = null;
let workspaceContext = null;
let workspaceAbortController = null;
let workspaceReadTail = Promise.resolve();
const refreshTimers = new Set();
let sessionExpired = false;
let agentEventCursor = 0;
let agentCalling = false;
let agentEventsRefreshing = false;
let agentEventTimer = null;
let activeAgentTurnId = null;
let agentMode = "DEMO";
let agentTurnState = null;
let agentMissionState = null;

function invalidateWorkspaceContext() {
  workspaceAbortController?.abort();
  workspaceAbortController = null;
  workspaceContext = null;
  eventSource?.close();
  eventSource = null;
  for (const timer of refreshTimers) clearInterval(timer);
  refreshTimers.clear();
  if (agentEventTimer !== null) clearInterval(agentEventTimer);
  agentEventTimer = null;
  evidenceOpenSequence += 1;
  previewSequence += 1;
  openedEvidenceId = null;
  jobsRefreshing = false;
  graphRefreshing = false;
  correlationsRefreshing = false;
  researchValue = null;
  agentEventCursor = 0;
  agentEventsRefreshing = false;
  activeAgentTurnId = null;
  agentTurnState = null;
  agentMissionState = null;
  document.querySelector(".agent-conversation")?.replaceChildren();
  document.querySelector(".activity-stream")?.replaceChildren();
  byId("agent-proposal-list")?.replaceChildren();
}

function beginWorkspaceContext(workspaceId, generation) {
  invalidateWorkspaceContext();
  workspaceAbortController = new AbortController();
  workspaceContext = Object.freeze({ workspaceId, generation });
  return workspaceContext;
}

function expireAuthenticatedSession() {
  if (sessionExpired || !operationalMode) return;
  sessionExpired = true;
  // CONTRACT: le premier 401 authentifié ferme tous les producteurs de données
  // et invalide les secrets en mémoire. Aucun appel refusé n'est rejoué.
  invalidateWorkspaceContext();
  csrfToken = null;
  operationalMode = false;
  libraryMode = false;
  workspaceReady = false;
  libraryGeneration = null;
  pendingCreateIntent = null;
  preparedUploads.clear();
  for (const dialog of document.querySelectorAll("dialog[open]")) dialog.close();
  byId("node-context-menu").hidden = true;
  byId("workbench-shell").hidden = true;
  byId("library-home").hidden = false;
  byId("workspace-create").hidden = true;
  byId("library-list-section").hidden = true;
  // CONTRACT: la reconnexion automatique ne rejoue aucune mutation. La
  // navigation racine obtient un nouveau cookie HttpOnly local sans code ni URL.
  window.location.replace("/");
}

async function detectExpiredSession(response) {
  if (!operationalMode || response.status !== 401) return false;
  let value = null;
  try { value = await response.clone().json(); } catch (_) { return false; }
  if (value?.error !== "session_required") return false;
  expireAuthenticatedSession();
  return true;
}

async function workspaceFetch(path, options = {}) {
  const context = workspaceContext;
  const controller = workspaceAbortController;
  if (!context || !controller) throw new DOMException("Contexte fermé", "AbortError");
  const isRead = !options.method || options.method === "GET";
  let releaseRead = null;
  if (isRead) {
    const previousRead = workspaceReadTail;
    workspaceReadTail = new Promise((resolve) => { releaseRead = resolve; });
    await previousRead;
    // INVARIANT: les sondages périodiques n'émettent pas plusieurs requêtes
    // après l'expiration. Le premier 401 invalide le contexte ; les lectures
    // déjà mises en file s'arrêtent donc avant de toucher le réseau.
    if (context !== workspaceContext || controller !== workspaceAbortController ||
        sessionExpired)
      throw new DOMException("Contexte remplacé", "AbortError");
  }
  let response;
  try {
    response = await fetch(path, { ...options, signal: controller.signal });
    if (await detectExpiredSession(response))
      throw new DOMException("Session expirée", "AbortError");
  } finally {
    releaseRead?.();
  }
  // INVARIANT: une réponse appartient au couple espace/génération qui l'a
  // demandée ; elle ne peut jamais repeupler l'espace ouvert ensuite.
  if (context !== workspaceContext) throw new DOMException("Contexte remplacé", "AbortError");
  Object.defineProperty(response, "_labfyWorkspaceContext", { value: context });
  return response;
}

async function workspaceJson(response) {
  const value = await response.json();
  if (response._labfyWorkspaceContext !== workspaceContext)
    throw new DOMException("Contexte remplacé", "AbortError");
  return value;
}

async function responseJson(response, fallback) {
  let value;
  try {
    value = await response.json();
  } catch (_) {
    throw new Error(fallback);
  }
  if (!response.ok) throw new Error(value.message ?? fallback);
  return value;
}

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
    byId("report-name").value = draft.title ?? "Rapport d’enquête";
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
  const request = () => workspaceFetch(path, {
    method: "POST",
    headers: { "Content-Type": "application/json", "X-Labfy-CSRF": csrfToken },
    body: JSON.stringify(value),
  });
  let response = await request();
  // WHY: deux intentions humaines liées (préparer puis autoriser) peuvent se
  // succéder plus vite que le garde-fou local anti-burst. Une réponse 429 n'a
  // admis aucune mutation ; un unique backoff borné rejoue donc la même
  // intention, avec sa clé d'idempotence lorsqu'elle est requise.
  if (response.status === 429) {
    await new Promise((resolve) => setTimeout(resolve, 35));
    response = await request();
  }
  const result = await workspaceJson(response);
  if (!response.ok) throw new Error(result.message ?? "Commande refusée");
  return result;
}

function appendAgentCard(kind, message) {
  const card = document.createElement("li");
  card.className = `agent-card ${kind.toLowerCase()}`;
  const label = document.createElement("b");
  label.textContent = kind;
  const text = document.createElement("p");
  text.textContent = message;
  card.append(label, text);
  document.querySelector(".agent-conversation").append(card);
}

function shortAgentReason(reason) {
  const value = typeof reason === "string" ? reason.trim() : "configuration absente";
  return value.slice(0, 120) || "configuration absente";
}

function setAgentControls() {
  const running = ["QUEUED", "RUNNING", "WAITING_MODEL", "WAITING_TOOL"].includes(
    agentTurnState,
  );
  byId("agent-cancel").disabled = agentMode !== "LOCAL" || !activeAgentTurnId || !running;
  byId("agent-resume").disabled = agentMode !== "LOCAL" || !activeAgentTurnId ||
    !["PAUSED", "INTERRUPTED", "AUTHORIZATION_REQUIRED"].includes(agentTurnState);
}

function renderAgentMissionSelection() {
  const selected = snapshot.nodes.find((node) => node.id === selectedNodeId);
  byId("agent-mission-selection").textContent = selected
    ? `Sélection courante : ${selected.label ?? selected.id}`
    : "Sélection courante : aucune.";
}

function renderAgentMission(value) {
  agentMissionState = value;
  const mission = value.mission;
  const active = value.state === "ACTIVE" && mission !== null;
  byId("agent-mission-state").textContent = active
    ? "Mission active · scope et budgets contrôlés par le backend."
    : value.state === "CANCELLED" ? "Mission annulée." : "Aucune mission active.";
  byId("agent-mission-goal").textContent = mission?.goal ?? "—";
  byId("agent-mission-scope").textContent = mission
    ? `${mission.scoped_refs.length} objet(s) · ${mission.pivots.length} pivot(s)` : "—";
  byId("agent-mission-risk").textContent =
    mission?.allowed_risk_classes.join(", ") ?? "—";
  byId("agent-mission-profile").textContent = mission?.network_profile ?? "—";
  byId("agent-mission-contacts").textContent = mission
    ? `${mission.used.contacts}/${mission.limits.contacts}` : "—";
  byId("agent-mission-tools").textContent = mission
    ? `${mission.used.tool_calls}/${mission.limits.tool_calls}` : "—";
  byId("agent-mission-elapsed").textContent = `${value.elapsed_seconds ?? 0} s`;
  byId("agent-mission-start").disabled = active || !selectedNodeId;
  byId("agent-mission-cancel").disabled = !active;
  byId("agent-system-privacy").textContent = mission?.network_profile === "OFFLINE"
    ? "non requis · hors ligne" : "indisponible";
  renderAgentMissionSelection();
}

async function refreshAgentMission() {
  const response = await workspaceFetch("/api/v1/agent-mission/current", {
    cache: "no-store",
  });
  const value = await workspaceJson(response);
  if (!response.ok || value.contract !== "labfy.agent_mission_state.v1")
    throw new Error(value.message ?? "État de mission indisponible");
  renderAgentMission(value);
}

async function startAgentMission() {
  const goal = byId("agent-mission-goal-input").value.trim();
  const profile = byId("agent-mission-profile-input").value;
  if (!goal) throw new Error("Le but explicite de mission est obligatoire.");
  if (!selectedNodeId || !snapshot.nodes.some((node) => node.id === selectedNodeId))
    throw new Error("Sélectionnez d’abord un objet existant du graphe.");
  const passive = profile === "PASSIVE_PUBLIC";
  const value = await postCommand("/api/v1/agent-mission/start", {
    workspace_id: workspaceContext.workspaceId,
    specification: {
      goal,
      scoped_refs: [{ object_id: selectedNodeId }],
      pivots: [{ object_id: selectedNodeId }],
      allowed_risk_classes: [passive ? "PASSIVE_PUBLIC" : "LOCAL_READ_ONLY"],
      network_profile: profile,
      max_contacts: passive ? 5 : 0,
      max_duration_seconds: 900,
      max_tool_calls: 16,
    },
    human_confirmed: true,
    idempotency_key: newUuid(),
  });
  renderAgentMission(value);
  appendAgentActivity({
    timestamp: new Date().toISOString(),
    kind: "agent.mission.started",
    payload: { message: `Mission démarrée : ${goal}` },
  }, "AGENT");
}

async function cancelAgentMission() {
  const missionId = agentMissionState?.mission?.mission_id;
  if (!missionId) return;
  const value = await postCommand("/api/v1/agent-mission/cancel", {
    workspace_id: workspaceContext.workspaceId,
    mission_id: missionId,
    human_confirmed: true,
    idempotency_key: newUuid(),
  });
  renderAgentMission(value);
  appendAgentActivity({
    timestamp: new Date().toISOString(),
    kind: "agent.mission.cancelled",
    payload: { message: "Mission annulée explicitement." },
  }, "WARNING");
}

function proposalReferenceButton(record) {
  const button = document.createElement("button");
  button.type = "button";
  button.textContent = "Voir dans le graphe";
  const reference = record.proposal.object_refs.find(({ object_id: objectId }) =>
    snapshot.nodes.some((node) => node.id === objectId));
  button.disabled = !reference;
  button.addEventListener("click", () => {
    if (reference) selectNode(reference.object_id, { preserveDomFocus: true });
  });
  return button;
}

async function decideAgentProposal(record, action) {
  const outcome = await postCommand(
    `/api/v1/agent-proposals/${encodeURIComponent(record.proposal_id)}/${action}`,
    {
      workspace_id: workspaceContext.workspaceId,
      decision_id: newUuid(),
      reason: "Décision explicite depuis l’interface locale",
      decided_by: "opérateur local",
      decided_at: new Date().toISOString(),
      idempotency_key: newUuid(),
    },
  );
  await refreshAgentProposals();
  const label = outcome.decision?.decision === "APPROVED" ? "approuvée" : "refusée";
  appendAgentActivity({
    timestamp: new Date().toISOString(),
    kind: `agent.proposal.${label}`,
    payload: { message: `Proposition ${label} · aucun grant créé.` },
  }, "POLICY");
}

function renderAgentProposals(records) {
  const list = byId("agent-proposal-list");
  list.replaceChildren();
  for (const record of records) {
    const item = document.createElement("li");
    item.className = "agent-proposal-card";
    const title = document.createElement("h4");
    title.textContent = record.proposal.title;
    const reason = document.createElement("p");
    reason.textContent = record.proposal.reason;
    const meta = document.createElement("p");
    meta.textContent = `${record.proposal.risk_class} · ` +
      `${record.proposal.suggested_capability}`;
    const actions = document.createElement("div");
    actions.append(proposalReferenceButton(record));
    if (record.decision === null) {
      for (const [label, action] of [
        ["Approuver (sans grant)", "approve"], ["Refuser", "reject"],
      ]) {
        const button = document.createElement("button");
        button.type = "button";
        button.textContent = label;
        button.addEventListener("click", () => void decideAgentProposal(record, action)
          .catch((error) => { byId("agent-mission-state").textContent = error.message; }));
        actions.append(button);
      }
    } else {
      const decision = document.createElement("strong");
      decision.textContent = record.decision.decision === "APPROVED"
        ? "Approuvée · aucun grant" : "Refusée";
      actions.append(decision);
    }
    item.append(title, reason, meta, actions);
    list.append(item);
  }
  if (records.length === 0) {
    const empty = document.createElement("li");
    empty.textContent = "Aucune proposition candidate.";
    list.append(empty);
  }
}

async function refreshAgentProposals() {
  const response = await workspaceFetch("/api/v1/agent-proposals", {
    cache: "no-store",
  });
  const value = await workspaceJson(response);
  if (!response.ok || value.contract !== "labfy.agent_proposal_list.v1")
    throw new Error(value.message ?? "Propositions indisponibles");
  renderAgentProposals(value.proposals);
}

async function loadAgentRuntime() {
  const response = await workspaceFetch("/api/v1/agent-runtime/status", {
    cache: "no-store",
  });
  if (response.status === 404) return false;
  const value = await workspaceJson(response);
  if (!response.ok) throw new Error(value.message ?? "Statut du modèle local indisponible");
  if (value.contract !== "labfy.agent_runtime.status.v1") {
    throw new Error("Contrat de statut du modèle local invalide");
  }
  if (value.mode === "DETERMINISTIC_DEMO") return false;
  agentMode = "LOCAL";
  if (value.mode === "LOCAL_MODEL" && value.available === true) {
    byId("agent-status").textContent = `Qwen prêt · ${value.model ?? "modèle configuré"}`;
    byId("agent-send").disabled = false;
    byId("agent-system-qwen").textContent = "prêt";
  } else {
    byId("agent-status").textContent =
      `Modèle local indisponible · ${shortAgentReason(value.reason)}`;
    byId("agent-send").disabled = true;
    byId("agent-system-qwen").textContent = "indisponible";
  }
  byId("agent-runtime-detail").textContent =
    "Runtime local borné · aucune autorisation implicite";
  return true;
}

async function loadAgentCatalog() {
  if (!operationalMode) return;
  try {
    if (await loadAgentRuntime()) return;
  } catch (error) {
    if (error.name === "AbortError") throw error;
    byId("agent-status").textContent =
      `Modèle local indisponible · ${shortAgentReason(error.message)}`;
    byId("agent-send").disabled = true;
    return;
  }
  agentMode = "DEMO";
  const response = await workspaceFetch("/api/v1/agent-tools/catalog", {
    cache: "no-store",
  });
  const value = await workspaceJson(response);
  if (!response.ok || value.contract !== "labfy.agent_tool_protocol.v1" ||
      value.transport !== "HTTP_POLLING" || !Array.isArray(value.tools))
    throw new Error(value.message ?? "Catalogue agent indisponible");
  const select = byId("agent-tool");
  select.replaceChildren();
  for (const tool of value.tools) {
    const option = document.createElement("option");
    option.value = tool.tool_id;
    option.textContent = tool.title;
    select.append(option);
  }
  select.disabled = value.tools.length === 0;
  byId("agent-send").disabled = value.tools.length === 0;
  byId("agent-status").textContent =
    "Agent de démonstration déterministe";
  byId("agent-runtime-detail").textContent =
    "Gateway mémoire · aucun modèle, shell ou auto-autorisation";
  byId("agent-system-qwen").textContent = "démo sans modèle";
  byId("agent-system-sandbox").textContent = value.tools.some((tool) =>
    String(tool.tool_id).startsWith("sandbox.") && tool.availability === "AVAILABLE")
    ? "disponible" : "indisponible";
}

function agentEventKind(event) {
  const aliases = {
    "agent.turn.started": "USER",
    "agent.plan.updated": "PLAN",
    "agent.authorization.required": "AUTHORIZATION_REQUIRED",
    "agent.tool.completed": "TOOL_RESULT",
    "agent.turn.completed": "RESULT",
    "agent.runtime.queued": "USER",
    "agent.runtime.running": "MODEL",
    "agent.runtime.tool_requested": "TOOL_REQUEST",
    "agent.runtime.tool_completed": "TOOL_RESULT",
    "agent.runtime.authorization_required": "AUTHORIZATION_REQUIRED",
    "agent.runtime.resumed": "AGENT",
    "agent.runtime.completed": "RESULT",
    "agent.runtime.failed": "ERROR",
    "agent.runtime.cancelled": "WARNING",
    "agent.runtime.model_unavailable": "ERROR",
    "agent.runtime.model_protocol_error": "ERROR",
    "agent.runtime.budget_exhausted": "WARNING",
  };
  if (aliases[event.kind]) return aliases[event.kind];
  return String(event.kind ?? event.type ?? "").split(".").at(-1).toUpperCase();
}

function agentEventMessage(kind, event) {
  const payload = event.payload ?? event.data ?? {};
  if (kind === "USER") return payload.objective ?? payload.message ?? "Objectif reçu";
  if (kind === "AGENT") return payload.message ?? payload.summary ?? "Réponse de l’agent";
  if (kind === "PLAN") {
    if (Array.isArray(payload.steps)) return payload.steps.join(" → ");
    return payload.summary ?? "Plan mis à jour";
  }
  if (kind === "MODEL") {
    const round = payload.round ?? "—";
    const duration = payload.duration_ms ?? "—";
    const input = payload.input_bytes ?? "—";
    const output = payload.output_bytes ?? "—";
    return `Tour ${round} · ${duration} ms · entrée ${input} octets · ` +
      `sortie ${output} octets · ${payload.status ?? event.state ?? "terminé"}`;
  }
  if (kind === "TOOL_REQUEST") {
    return `${payload.tool_id ?? "outil"} · ${payload.status ?? "demandé"}`;
  }
  if (kind === "TOOL_RESULT") {
    return `${payload.tool_id ?? "outil"} · ${payload.status ?? payload.state ?? "terminé"}`;
  }
  if (kind === "AUTHORIZATION_REQUIRED") {
    return payload.message ?? "Décision humaine requise ; l’agent ne peut pas l’accorder.";
  }
  if (kind === "RESULT") return payload.summary ?? payload.message ?? "Tour terminé";
  if (kind === "POLICY") return payload.message ?? "Décision humaine enregistrée";
  if (kind === "WARNING") return payload.message ?? "Avertissement du runtime";
  if (kind === "ERROR") return payload.message ?? "Erreur du runtime";
  return null;
}

function routeAgentObjectRefs(event) {
  const references = event.object_refs ?? event.payload?.object_refs ?? [];
  const reference = references.find(({ object_id: objectId }) =>
    snapshot.nodes.some((node) => node.id === objectId));
  if (reference) selectNode(reference.object_id);
}

function appendAgentActivity(event, kind) {
  const stream = document.querySelector(".activity-stream");
  const details = document.createElement("details");
  details.className = `activity-entry ${kind === "RESULT" ? "result" : ""}`;
  const activityTypes = {
    USER: "AGENT", AGENT: "AGENT", PLAN: "AGENT", MODEL: "MODEL",
    TOOL_REQUEST: "TOOL", TOOL_RESULT: "TOOL",
    AUTHORIZATION_REQUIRED: "POLICY", POLICY: "POLICY", RESULT: "RESULT",
    WARNING: "ERROR", ERROR: "ERROR",
  };
  details.dataset.activityType = activityTypes[kind] ?? "AGENT";
  const summary = document.createElement("summary");
  const time = document.createElement("time");
  time.textContent = String(event.timestamp ?? "").slice(11, 19) || "—";
  const label = document.createElement("b");
  label.textContent = kind;
  const description = document.createElement("span");
  const message = agentEventMessage(kind, event) ?? "Événement runtime";
  description.textContent = kind === "MODEL" || !event.kind
    ? message : `${event.kind} · ${message}`;
  summary.append(time, label, description);
  details.append(summary);
  if (kind === "MODEL") {
    stream.append(details);
    return;
  }
  const detail = document.createElement("pre");
  detail.textContent = `sequence: ${event.sequence ?? "—"}\nturn: ${event.turn_id ?? "—"}`;
  details.append(detail);
  stream.append(details);
}

function renderAgentEvent(event) {
  if (agentMode === "LOCAL" && event.turn_id !== activeAgentTurnId) return;
  const kind = agentEventKind(event);
  const supported = new Set([
    "USER", "AGENT", "PLAN", "MODEL", "TOOL_REQUEST", "TOOL_RESULT",
    "AUTHORIZATION_REQUIRED", "RESULT", "WARNING", "ERROR",
  ]);
  if (!supported.has(kind)) return;
  const message = agentEventMessage(kind, event);
  if (message !== null) appendAgentCard(kind, message);
  appendAgentActivity(event, kind);
  routeAgentObjectRefs(event);
}

async function refreshAgentTurn() {
  if (agentMode !== "LOCAL" || !activeAgentTurnId) return null;
  const response = await workspaceFetch(
    `/api/v1/agent-runtime/turns/${encodeURIComponent(activeAgentTurnId)}`,
    { cache: "no-store" },
  );
  const value = await workspaceJson(response);
  if (!response.ok) throw new Error(value.message ?? "État du tour indisponible");
  agentTurnState = value.state ?? value.status ?? agentTurnState;
  setAgentControls();
  const terminal = [
    "COMPLETED", "CANCELLED", "FAILED", "MODEL_UNAVAILABLE",
    "MODEL_PROTOCOL_ERROR", "BUDGET_EXHAUSTED",
  ];
  value.terminal = terminal.includes(agentTurnState);
  return value;
}

async function refreshAgentEvents() {
  if (!operationalMode || !workspaceReady || agentEventsRefreshing) return;
  agentEventsRefreshing = true;
  try {
    const query = agentMode === "LOCAL"
      ? `/api/v1/agent-runtime/events?cursor=${agentEventCursor}`
      : `/api/v1/agent-tools/events?cursor=${agentEventCursor}`;
    if (agentMode === "LOCAL" && !activeAgentTurnId) return;
    const response = await workspaceFetch(query, { cache: "no-store" });
    const value = await workspaceJson(response);
    if (!response.ok || !Array.isArray(value.events)) return;
    agentEventCursor = value.cursor ?? agentEventCursor;
    const turn = await refreshAgentTurn();
    for (const event of value.events) {
      const payload = { ...(event.payload ?? {}) };
      const kind = agentEventKind(event);
      if (kind === "USER" && turn?.objective) payload.objective = turn.objective;
      if (kind === "RESULT" && turn?.final) payload.summary = turn.final;
      if (["WARNING", "ERROR"].includes(kind) && turn?.diagnostic) {
        payload.message = turn.diagnostic;
      }
      renderAgentEvent({ ...event, payload });
    }
    if (turn?.terminal && value.events.some((event) =>
      event.turn_id === activeAgentTurnId && [
        "agent.runtime.completed", "agent.runtime.cancelled", "agent.runtime.failed",
        "agent.runtime.model_unavailable", "agent.runtime.model_protocol_error",
        "agent.runtime.budget_exhausted",
      ].includes(event.kind)) && agentEventTimer !== null) {
      clearInterval(agentEventTimer);
      agentEventTimer = null;
    }
    if (turn?.terminal && agentMode === "LOCAL") {
      byId("agent-system-qwen").textContent =
        agentTurnState === "MODEL_UNAVAILABLE" ? "indisponible" : "prêt";
    }
    byId("activity-filter").dispatchEvent(new Event("change"));
  } finally {
    agentEventsRefreshing = false;
  }
}

function pollAgentEvents() {
  void refreshAgentEvents().catch((error) => {
    if (error.name === "AbortError") return;
    if (agentEventTimer !== null) clearInterval(agentEventTimer);
    agentEventTimer = null;
  });
}

function startAgentEventPolling() {
  if (agentEventTimer !== null) return;
  pollAgentEvents();
  agentEventTimer = setInterval(pollAgentEvents, 1200);
}

async function callAgentTool() {
  if (agentCalling) return;
  const objective = byId("agent-prompt").value.trim();
  if (!objective) throw new Error("L’objectif est obligatoire.");
  agentCalling = true;
  byId("agent-send").disabled = true;
  try {
    const path = agentMode === "LOCAL"
      ? "/api/v1/agent-runtime/turns" : "/api/v1/agent-tools/turns";
    const body = agentMode === "LOCAL"
      ? { objective, idempotency_key: newUuid() } : { objective };
    const admitted = await postCommand(path, body);
    if (agentMode === "LOCAL") byId("agent-system-qwen").textContent = "occupé";
    activeAgentTurnId = admitted.turn_id;
    agentTurnState = admitted.state ?? "QUEUED";
    agentEventCursor = 0;
    document.querySelector(".agent-conversation").replaceChildren();
    document.querySelector(".activity-stream").replaceChildren();
    setAgentControls();
    startAgentEventPolling();
    if (agentMode === "DEMO") {
      if (Array.isArray(admitted.research_plan?.actions)) {
        researchValue = admitted.research_plan;
        renderResearch();
      } else {
        await refreshResearch();
      }
      byId("research-note").textContent =
        "Plan préparé par le flux existant ; décision humaine requise.";
      await refreshOperationalGraph();
    }
    await refreshAgentEvents();
  } finally {
    agentCalling = false;
    byId("agent-send").disabled = agentMode === "DEMO" && byId("agent-tool").disabled;
  }
}

async function controlAgentTurn(action) {
  if (!activeAgentTurnId || agentMode !== "LOCAL") return;
  const path = `/api/v1/agent-runtime/turns/${encodeURIComponent(activeAgentTurnId)}/${action}`;
  const value = await postCommand(path, {});
  agentTurnState = value.state ?? agentTurnState;
  setAgentControls();
  await refreshAgentEvents();
}

async function evidenceJson(path) {
  const response = await workspaceFetch(path, { cache: "no-store" });
  const value = await workspaceJson(response);
  if (!response.ok) throw new Error(value.message ?? "Preuve indisponible");
  return value;
}

function reviewEnvelope(observation, reason) {
  return {
    operation_id: newUuid(),
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
    if (error.name !== "AbortError" && sequence === evidenceOpenSequence)
      byId("evidence-status").textContent = error.message;
  }
}

async function refreshGraphAfterImport() {
  const response = await workspaceFetch("/api/v1/snapshot", { cache: "no-store" });
  const value = await workspaceJson(response);
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
    const response = await workspaceFetch(`/api/v1/uploads/${intention.upload_id}`, {
      method: "PUT", headers: {"Content-Type":"application/octet-stream",
        "X-Labfy-CSRF":csrfToken}, body:file,
    });
    const prepared = await workspaceJson(response);
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
    const selectionId=newUuid();
    for(const file of files) await receiveFile(file,selectionId);
    event.target.value="";
  });
  byId("import-confirm").addEventListener("click",async()=>{
    byId("import-confirm").disabled=true;
    for(const [id,prepared] of [...preparedUploads]){
      try{await postCommand(`/api/v1/uploads/${id}/confirm`,{
        idempotency_key:newUuid(),source:byId("import-source").value || "Non déclarée",
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
    const response = await workspaceFetch("/api/v1/jobs", { cache: "no-store" });
    if (!response.ok) return;
    const value = await workspaceJson(response);
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
    const response = await workspaceFetch("/api/v1/snapshot", { cache: "no-store" });
    const value = await workspaceJson(response);
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
    if (error.name !== "AbortError")
      byId("connection").textContent = `Dernière vue valide · ${error.message}`;
  } finally {
    graphRefreshing = false;
  }
}

async function refreshCorrelations() {
  if (!operationalMode || correlationsRefreshing) return;
  correlationsRefreshing = true;
  try {
    const response = await workspaceFetch("/api/v1/correlations", { cache: "no-store" });
    const value = await workspaceJson(response);
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
    if (error.name !== "AbortError")
      byId("correlations-note").textContent =
        `Index local indisponible · ${error.message}`;
  } finally {
    correlationsRefreshing = false;
  }
}

async function refreshPlanner() {
  if (!operationalMode) return;
  try {
    const response = await workspaceFetch("/api/v1/planner", { cache: "no-store" });
    const value = await workspaceJson(response);
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
    if (error.name !== "AbortError")
      byId("planner-note").textContent =
        `Planner indisponible · ${error.message}`;
  }
}

function researchDecision(actionId) {
  return document.querySelector(
    `input[name="research-${CSS.escape(actionId)}"]:checked`,
  )?.value ?? "defer";
}

function renderResearch() {
  // INVARIANT: toutes les cartes proviennent du snapshot C ; aucune action ou
  // adresse de fournisseur n'est construite ni conservée comme vérité JS.
  const panel = byId("research-plan");
  panel.replaceChildren();
  if (!researchValue || researchValue.state === "EMPTY") {
    byId("research-launch").disabled = true;
    byId("research-revoke").disabled = true;
    return;
  }
  const heading = document.createElement("h4");
  heading.textContent = `Question : ${researchValue.question}`;
  panel.append(heading);
  const waveOneDone = researchValue.actions.some(
    (action) => action.wave === 1 && action.contacted,
  );
  for (const action of researchValue.actions) {
    const card = document.createElement("article");
    card.className = "research-card";
    const title = document.createElement("h5");
    title.textContent = `Vague ${action.wave} · ${action.capability_id}`;
    const explanation = document.createElement("p");
    explanation.textContent = action.wave === 1
      ? "Gain attendu : obtenir un fait sourcé. Incertitude : le fournisseur peut ne rien connaître."
      : action.wave === 2
        ? "Nouvelle action non contactée : vérifier ou contredire le premier résultat."
        : "Action optionnelle distincte ; aucun contact sans autorisation explicite.";
    const provenance = document.createElement("p");
    provenance.textContent = `Source/provenance : sélection du graphe · sujet exact : ${action.subject}`;
    const disclosure = document.createElement("p");
    disclosure.textContent = `Fournisseur : ${action.provider_id} · contact ${action.contact} · ${action.disclosure}`;
    const budget = document.createElement("p");
    budget.textContent = `Budget maximal : ${action.max_requests} requête · ${action.max_response_bytes} octets · ${action.max_active_ms} ms.`;
    const status = document.createElement("p");
    status.className = action.contacted ? "research-contacted" : "research-uncontacted";
    status.textContent = action.contacted
      ? `Résultat : ${action.result_status || "MISSING"}`
      : "Non contactée — aucune donnée fournisseur reçue.";
    const choices = document.createElement("fieldset");
    choices.disabled = action.contacted || (action.wave === 2 && !waveOneDone);
    const legend = document.createElement("legend");
    legend.textContent = "Décision humaine";
    choices.append(legend);
    for (const [value, label] of [["authorize", "Autoriser"],
      ["defer", "Différer"], ["refuse", "Refuser"]]) {
      const choice = document.createElement("label");
      const input = document.createElement("input");
      input.type = "radio";
      input.name = `research-${action.action_id}`;
      input.value = value;
      input.checked = value === (action.decision?.toLowerCase() ?? "defer");
      input.addEventListener("change", () => {
        byId("research-launch").disabled = !researchValue.actions.some(
          (item) => !item.contacted && researchDecision(item.action_id) === "authorize",
        );
      });
      choice.append(input, ` ${label}`);
      choices.append(choice);
    }
    card.append(title, explanation, provenance, disclosure, budget, status, choices);
    panel.append(card);
  }
  byId("research-launch").disabled = true;
  byId("research-revoke").disabled = !researchValue.grants.some(
    (grant) => !grant.revoked_at,
  );
}

async function refreshResearch() {
  if (!operationalMode) return;
  const response = await workspaceFetch("/api/v1/research", { cache: "no-store" });
  const value = await workspaceJson(response);
  if (!response.ok || value.contract !== "labfy.research.snapshot.v1")
    throw new Error(value.message ?? "Recherche indisponible");
  researchValue = value;
  renderResearch();
}

async function prepareResearch() {
  const node = selectedNode();
  if (!node) throw new Error("Sélectionnez d’abord une preuve ou une entité.");
  const exclusions = byId("research-exclusions").value.split("\n")
    .map((item) => item.trim()).filter(Boolean);
  researchValue = await postCommand("/api/v1/research/prepare", {
    selection_ids: [node.id],
    question: byId("research-question").value.trim(),
    exclusions,
    idempotency_key: newUuid(),
  });
  byId("research-note").textContent =
    "Plan préparé par le cœur C. Choisissez chaque action avant le lancement final.";
  renderResearch();
}

async function launchResearch() {
  const actionIds = researchValue.actions.filter((action) =>
    !action.contacted && researchDecision(action.action_id) === "authorize")
    .map((action) => action.action_id);
  if (!actionIds.length) return;
  const exclusions = byId("research-exclusions").value.split("\n")
    .map((item) => item.trim()).filter(Boolean);
  const decisions = researchValue.actions.map((action) => ({
    action_id: action.action_id,
    decision: researchDecision(action.action_id).toUpperCase(),
  }));
  const grant = await postCommand("/api/v1/research/grants", {
    plan_id: researchValue.plan_id,
    input_revision: researchValue.input_revision,
    selected_action_ids: actionIds,
    decisions,
    exclusions,
    idempotency_key: newUuid(),
  });
  const grantId = grant.grants[0]?.grant_id;
  if (!grantId) throw new Error("Autorisation durable absente.");
  researchValue = await postCommand("/api/v1/research/campaigns", {
    grant_id: grantId,
    input_revision: grant.input_revision,
    action_ids: actionIds,
    idempotency_key: newUuid(),
  });
  // CONTRACT: seul le flux C existant persiste grant et campagne. Le turn ne
  // reprend qu'après ce succès ; l'agent ne possède aucune voie d'autorisation.
  if (activeAgentTurnId) {
    const namespace = agentMode === "LOCAL" ? "agent-runtime" : "agent-tools";
    await postCommand(
      `/api/v1/${namespace}/turns/${encodeURIComponent(activeAgentTurnId)}/resume`, {},
    );
    await refreshAgentEvents();
  }
  byId("research-note").textContent =
    "Campagne terminée ; résultats sourcés et actions non contactées affichés.";
  renderResearch();
}

async function revokeResearch() {
  const grant = [...researchValue.grants].reverse().find((item) => !item.revoked_at);
  if (!grant) return;
  researchValue = await postCommand("/api/v1/research/revoke", {
    grant_id: grant.grant_id,
  });
  byId("research-note").textContent = "Autorisation révoquée durablement.";
  renderResearch();
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
  byId("graph-zoom").textContent = `${Math.round(scale * 100)} %`;
  renderObjectList();
  renderSelection();
}

function renderObjectList() {
  const list = byId("object-list");
  // INVARIANT: un rafraîchissement de projection ne doit pas détacher un
  // bouton qui représente toujours le même objet. Les actualisations
  // opérationnelles sont périodiques ; préserver cette identité DOM évite
  // d'interrompre une activation utilisateur entre son ciblage et son clic.
  const existing = new Map(
    [...list.children].map((item) => [item.dataset.objectId, item]),
  );
  const next = [];
  projection.nodes.forEach((node, index) => {
    let item = existing.get(node.id);
    if (!item) {
      item = document.createElement("li");
      item.dataset.objectId = node.id;
      const button = document.createElement("button");
      button.addEventListener("click", () => selectNode(button.dataset.objectId));
      button.addEventListener("focus", () => {
        keyboardIndex = Number(button.dataset.keyboardIndex);
      });
      item.append(button);
    }
    const button = item.firstElementChild;
    button.textContent = `${node.type} — ${node.label}`;
    button.dataset.objectId = node.id;
    button.dataset.keyboardIndex = String(index);
    next.push(item);
  });
  list.replaceChildren(...next);
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
    byId("drawer-details-summary").textContent =
      "Sélectionnez un objet du graphe pour conserver son contexte ici.";
    byId("drawer-provenance-summary").textContent = "Aucune source sélectionnée.";
    byId("drawer-timeline-summary").textContent =
      "La chronologie pertinente apparaît avec l’objet sélectionné.";
    byId("drawer-observations-summary").textContent =
      "Aucune observation à promouvoir automatiquement.";
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
  byId("drawer-details-summary").textContent =
    `${node.label} · ${node.type} · état ${node.state}`;
  byId("drawer-provenance-summary").textContent =
    `Provenance disponible pour ${node.label}. Utilisez l’onglet Provenance de l’activité pour parcourir les origines.`;
  byId("drawer-timeline-summary").textContent =
    `La timeline filtre les événements attribués à ${node.label} sans inventer de date.`;
  byId("drawer-observations-summary").textContent =
    node.object_kind === "observation"
      ? `${node.label} est une observation : revue explicite requise.`
      : `Aucune observation n’est automatiquement déduite de ${node.label}.`;
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
    idempotency_key: newUuid(),
  });
  byId("report-status").textContent = "Génération locale en cours…";
  for (let i = 0; i < 100; i++) {
    await new Promise((resolve) => setTimeout(resolve, 100));
    const response = await workspaceFetch(`/api/v1/reports/${admission.report_id}`, {
        cache: "no-store",
      }),
      status = await workspaceJson(response);
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
    const storageKey = `labfy-intent:${workspaceContext.workspaceId}:` +
      `${workspaceContext.generation}:${node.object_id}:${capability.id}`;
    const key = sessionStorage.getItem(storageKey) ?? newUuid();
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
  renderAgentMissionSelection();
  if (agentMissionState?.state !== "ACTIVE")
    byId("agent-mission-start").disabled = false;
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
  renderAgentMissionSelection();
  byId("agent-mission-start").disabled = true;
  focusNodeId = null;
  filters = {};
  collapsedGroups.clear();
  // CONTRACT: « Réinitialiser » revient à la vue globale visible. Une échelle
  // fixe à 1 ferait sortir le snapshot cœur sous les panneaux persistants.
  scale = snapshot.nodes.length > 0 ? fitGraphScale() : 1;
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
    const response = await workspaceFetch("/api/v1/snapshot?size=demo&revision=latest");
    const replacement = await workspaceJson(response);
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
      byId("connection").textContent = "Reconnexion au flux…";
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

function configureResizableLayout() {
  // CONTRACT: les dimensions de panneau sont des préférences locales par
  // origine. Elles ne modifient ni le snapshot, ni les positions épinglées.
  const workbench = document.querySelector(".workbench");
  const dimensions = [
    { id: "agent-resizer", property: "--agent-pane", key: "agent", axis: "x",
      minimum: 208, maximum: 460, direction: 1 },
    { id: "inspector-resizer", property: "--inspector-pane", key: "inspector", axis: "x",
      minimum: 256, maximum: 520, direction: -1 },
    { id: "drawer-resizer", property: "--drawer", key: "drawer", axis: "y",
      minimum: 130, maximum: 440, direction: -1 },
  ];
  for (const dimension of dimensions) {
    const stored = Number.parseInt(localStorage.getItem(`labfy-layout:${dimension.key}`), 10);
    if (Number.isFinite(stored))
      workbench.style.setProperty(dimension.property, `${stored}px`);
    const separator = byId(dimension.id);
    const applyKeyboardDelta = (delta) => {
      const current = Number.parseInt(
        getComputedStyle(workbench).getPropertyValue(dimension.property), 10,
      );
      const next = Math.max(dimension.minimum, Math.min(dimension.maximum, current + delta));
      workbench.style.setProperty(dimension.property, `${next}px`);
      localStorage.setItem(`labfy-layout:${dimension.key}`, String(next));
      requestAnimationFrame(() => renderer.updateViewport(scale, offset));
    };
    separator.addEventListener("keydown", (event) => {
      const backward = dimension.axis === "x" ? "ArrowLeft" : "ArrowUp";
      const forward = dimension.axis === "x" ? "ArrowRight" : "ArrowDown";
      if (![backward, forward].includes(event.key)) return;
      event.preventDefault();
      applyKeyboardDelta((event.key === forward ? 16 : -16) * dimension.direction);
    });
    separator.addEventListener("pointerdown", (event) => {
      if (matchMedia("(max-width: 900px)").matches) return;
      const initial = Number.parseInt(
        getComputedStyle(workbench).getPropertyValue(dimension.property), 10,
      );
      const origin = dimension.axis === "x" ? event.clientX : event.clientY;
      separator.setPointerCapture(event.pointerId);
      const move = (moveEvent) => {
        if (moveEvent.pointerId !== event.pointerId) return;
        const coordinate = dimension.axis === "x" ? moveEvent.clientX : moveEvent.clientY;
        const next = Math.max(
          dimension.minimum,
          Math.min(dimension.maximum, initial + (coordinate - origin) * dimension.direction),
        );
        workbench.style.setProperty(dimension.property, `${next}px`);
        renderer.updateViewport(scale, offset);
      };
      const finish = (finishEvent) => {
        if (finishEvent.pointerId !== event.pointerId) return;
        if (separator.hasPointerCapture(event.pointerId))
          separator.releasePointerCapture(event.pointerId);
        const value = Number.parseInt(
          getComputedStyle(workbench).getPropertyValue(dimension.property), 10,
        );
        localStorage.setItem(`labfy-layout:${dimension.key}`, String(value));
        separator.removeEventListener("pointermove", move);
        separator.removeEventListener("pointerup", finish);
        separator.removeEventListener("pointercancel", finish);
      };
      separator.addEventListener("pointermove", move);
      separator.addEventListener("pointerup", finish);
      separator.addEventListener("pointercancel", finish);
    });
  }
}

function configureControls() {
  // CONTRACT: la disposition est une préférence locale par origine ; elle ne
  // transporte ni l'enquête, ni un secret, ni un état métier vers le navigateur.
  for (const button of document.querySelectorAll(".pane-toggle")) {
    const pane = byId(button.dataset.pane);
    const key = `labfy-pane:${button.dataset.pane}`;
    const apply = (collapsed) => {
      pane.classList.toggle("pane-collapsed", collapsed);
      button.setAttribute("aria-expanded", String(!collapsed));
      button.textContent = collapsed ? "Déployer" : "Réduire";
      requestAnimationFrame(() => renderer.updateViewport(scale, offset));
    };
    apply(localStorage.getItem(key) === "collapsed");
    button.addEventListener("click", () => {
      const collapsed = !pane.classList.contains("pane-collapsed");
      localStorage.setItem(key, collapsed ? "collapsed" : "expanded");
      apply(collapsed);
    });
  }
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
    localStorage.setItem("labfy-pane:drawer", collapsed ? "collapsed" : "expanded");
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
      byId("graph-state").textContent = "Aucun objet trouvé.";
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
  byId("graph-search-focus").addEventListener("click", () => byId("search").focus());
  byId("graph-home").addEventListener("click", resetGlobalView);
  byId("graph-reset-layout").addEventListener("click", () => byId("reset-layout").click());
  byId("graph-fullscreen").addEventListener("click", () => {
    const workbench = document.querySelector(".workbench");
    const active = workbench.classList.toggle("graph-priority");
    byId("graph-fullscreen").setAttribute("aria-pressed", String(active));
    byId("graph-fullscreen").title = active ? "Quitter le mode graphe" : "Agrandir le graphe";
    requestAnimationFrame(() => renderer.updateViewport(scale, offset));
  });
  byId("activity-filter").addEventListener("change", (event) => {
    for (const entry of document.querySelectorAll("[data-activity-type]"))
      entry.hidden = Boolean(event.target.value) &&
        entry.dataset.activityType !== event.target.value;
  });
  byId("activity-expert-toggle").addEventListener("click", (event) => {
    const active = byId("inspector").classList.toggle("expert-mode");
    event.currentTarget.setAttribute("aria-pressed", String(active));
    event.currentTarget.textContent = active ? "Mode normal" : "Mode expert";
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
  // WHY: les panneaux persistants modifient réellement la largeur SVG. Le
  // renderer doit recaler son viewport avant le prochain drag, sans muter les
  // coordonnées épinglées qui restent un état de présentation local.
  new ResizeObserver(() => requestAnimationFrame(() => {
    const panel = byId("graph-panel");
    if (coreMode && !graph_scale_initialized && snapshot.nodes.length > 0 &&
        panel.clientWidth > 100 && panel.clientHeight > 100) {
      // WHY: le snapshot peut arriver pendant que la coque tri-panneaux passe
      // de hidden à sa largeur finale. L'auto-fit unique évite que des nœuds
      // SVG débordent sous Activité, tout en préservant ensuite zoom/pins.
      scale = fitGraphScale();
      graph_scale_initialized = true;
      render();
      return;
    }
    renderer.updateViewport(scale, offset);
  })).observe(byId("graph-panel"));
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
        Math.min(3, scale * (event.deltaY < 0 ? 1.2 : 0.9)),
      );
      renderer.updateViewport(scale, offset);
      byId("graph-zoom").textContent = `${Math.round(scale * 100)} %`;
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
    if (
      event.key === "Escape" &&
      document.querySelector(".workbench").classList.contains("graph-priority")
    ) {
      byId("graph-fullscreen").click();
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

async function libraryRequest(path, options = {}) {
  const response = await fetch(path, options);
  if (await detectExpiredSession(response))
    throw new DOMException("Session expirée", "AbortError");
  return responseJson(response, "Bibliothèque locale indisponible");
}

function renderLibrary(value) {
  if (value.contract !== "labfy.web_library.v1" ||
      !Array.isArray(value.entries) || !Number.isInteger(value.generation))
    throw new Error("Contrat de bibliothèque inattendu");
  libraryGeneration = value.generation;
  const list = byId("workspace-list");
  list.replaceChildren();
  for (const entry of value.entries) {
    const item = document.createElement("li");
    item.className = "workspace-card";
    const heading = document.createElement("h3");
    heading.textContent = entry.title;
    const state = document.createElement("p");
    const active = value.active_workspace_id === entry.workspace_id;
    const anotherActive = value.active_workspace_id !== null && !active;
    state.textContent = active ? "Ouverte dans cette session" : anotherActive
      ? "Disponible après redémarrage du poste local"
      : entry.state === "READY" ? "Prête à ouvrir" : `État : ${entry.state}`;
    const open = document.createElement("button");
    open.type = "button";
    open.textContent = active ? "Revenir à l’enquête" : "Ouvrir l’enquête";
    open.disabled = entry.state !== "READY" || anotherActive;
    open.dataset.workspaceId = entry.workspace_id;
    open.addEventListener("click", () => void openWorkspace(entry.workspace_id, open));
    item.append(heading, state, open);
    list.append(item);
  }
  byId("library-empty").hidden = value.entries.length !== 0;
  byId("library-connection").textContent =
    `${value.entries.length} enquête(s) · génération ${value.generation}`;
}

async function refreshLibrary() {
  const value = await libraryRequest("/api/v1/library", { cache: "no-store" });
  renderLibrary(value);
  byId("library-load").hidden = true;
  byId("workspace-create").hidden = false;
  byId("library-list-section").hidden = false;
  return value;
}

async function showLibrary({ focus = false, load = true } = {}) {
  invalidateWorkspaceContext();
  for (const dialog of document.querySelectorAll("dialog[open]")) dialog.close();
  preparedUploads.clear();
  workspaceReady = false;
  byId("workbench-shell").hidden = true;
  byId("library-home").hidden = false;
  byId("workspace-create").hidden = !load;
  byId("library-list-section").hidden = !load;
  byId("library-load").hidden = load;
  byId("library-title").textContent = "Bibliothèque d’enquêtes";
  byId("library-connection").textContent = load
    ? "Actualisation…" : "Bibliothèque non chargée";
  if (load) {
    try {
      await refreshLibrary();
    } catch (error) {
      byId("library-connection").textContent = error.message;
    }
  }
  if (focus) byId("library-title").focus();
}

function resetWorkspacePresentation() {
  snapshot = prepareSnapshot({ contract: "labfy.web_graph.snapshot.v3", origin: "core",
    revision: 0, nodes: [], edges: [], capability_catalog: [], capabilities: [] });
  acceptedSnapshotFingerprint = snapshotFingerprint(snapshot);
  selectedNodeId = null;
  focusNodeId = null;
  navigationHistory = [];
  reportSelection.clear();
  reportPreview = null;
  correlationRevision = null;
  plannerRevision = null;
  plannerValue = null;
  preparedUploads.clear();
  pinnedPositions.clear();
  graph_scale_initialized = false;
  resetGlobalView();
}

async function activateWorkspace(opened) {
  beginWorkspaceContext(opened.workspace_id, opened.generation);
  resetWorkspacePresentation();
  workspaceReady = true;
  byId("library-home").hidden = true;
  byId("workbench-shell").hidden = false;
  byId("home-button").hidden = false;
  byId("investigation-title").textContent = opened.title;
  byId("mode-badge").textContent = "Espace local · contrôles locaux";
  byId("open-import").disabled = false;
  byId("connection").textContent = "Ouverture…";
  reportDraftKey = `labfy-report-draft:${opened.workspace_id}:${opened.generation}`;
  try {
    const response = await workspaceFetch("/api/v1/snapshot", { cache: "no-store" });
    const value = await workspaceJson(response);
    if (!response.ok) throw new Error(value.message ?? "Projection indisponible");
    snapshot = prepareSnapshot(value);
    acceptedSnapshotFingerprint = snapshotFingerprint(value);
    coreMode = snapshot.origin === "core";
    // CONTRACT: la mesure suit la publication de la coque visible ; elle ne
    // repose pas sur la largeur héritée lorsque le shell était hidden.
    await new Promise((resolve) => requestAnimationFrame(resolve));
    scale = fitGraphScale();
    graph_scale_initialized = true;
    syncTypeFilterOptions();
    restoreReportDraft();
    render();
    renderTimeline();
    renderReportSelection();
    byId("connection").textContent = `Ouverte · génération ${opened.generation}`;
    await Promise.all([refreshJobs(), refreshCorrelations(), refreshPlanner(),
      refreshResearch(), loadAgentCatalog(), refreshAgentMission(),
      refreshAgentProposals()]);
    for (const [callback, delay] of [[refreshJobs, 500],
      [refreshOperationalGraph, 700], [refreshCorrelations, 900],
      [refreshPlanner, 1100]])
      refreshTimers.add(setInterval(callback, delay));
    svg.focus();
  } catch (error) {
    if (error.name !== "AbortError") {
      byId("connection").textContent = `Ouverture incomplète · ${error.message}`;
      byId("graph-state").textContent = "La projection de cette enquête est indisponible.";
    }
  }
}

async function activateLegacyWorkspace(session) {
  const workspaceId = session?.investigation_id ?? "legacy-read-only";
  const generation = Number.isInteger(session?.generation) ? session.generation : 0;
  beginWorkspaceContext(workspaceId, generation);
  resetWorkspacePresentation();
  workspaceReady = session?.workspace_state === "READY" || session === null;
  byId("library-home").hidden = true;
  byId("workbench-shell").hidden = false;
  byId("home-button").hidden = true;
  byId("investigation-title").textContent = session?.title ?? "Enquête locale";
  byId("open-import").disabled = !operationalMode || !workspaceReady;
  reportDraftKey = `labfy-report-draft:${workspaceId}:${generation}`;
  try {
    const response = await workspaceFetch("/api/v1/snapshot", { cache: "no-store" });
    const value = await workspaceJson(response);
    snapshot = prepareSnapshot(value);
    acceptedSnapshotFingerprint = snapshotFingerprint(value);
    coreMode = snapshot.origin === "core";
    // CONTRACT: le snapshot cœur doit être ajusté après le layout Agent |
    // Graphe | Activité, sinon ses coordonnées SVG débordent sous Activité.
    await new Promise((resolve) => requestAnimationFrame(resolve));
    byId("mode-badge").textContent = operationalMode
      ? "Poste local · contrôles locaux"
      : coreMode ? "Snapshot du cœur C · lecture seule" : "Démonstration locale · lecture seule";
    byId("connection").textContent = snapshot._loadError
      ? `Erreur d'export cœur · ${snapshot._loadError}`
      : operationalMode ? "Poste local synchronisé" :
        coreMode ? "Lecture seule — snapshot cœur chargé" : `Connecté · révision ${snapshot.revision}`;
    scale = coreMode ? fitGraphScale() :
      Math.min(1, Math.max(0.6, (svg.clientWidth - 160) / 700));
    graph_scale_initialized = true;
    syncTypeFilterOptions();
    restoreReportDraft();
    render();
    if (snapshot._loadError)
      byId("graph-state").textContent = `Snapshot cœur indisponible : ${snapshot._loadError}`;
    if (!coreMode) connectEvents();
    if (operationalMode) {
      await Promise.all([refreshJobs(), refreshCorrelations(), refreshPlanner(),
        refreshResearch(), loadAgentCatalog(), refreshAgentMission(),
        refreshAgentProposals()]);
      for (const [callback, delay] of [[refreshJobs, 500],
        [refreshOperationalGraph, 700], [refreshCorrelations, 900],
        [refreshPlanner, 1100]])
        refreshTimers.add(setInterval(callback, delay));
    } else {
      byId("queue-controls").hidden = true;
    }
    renderTimeline();
    renderReportSelection();
  } catch (error) {
    if (error.name !== "AbortError") {
      byId("connection").textContent = `Chargement impossible · ${error.message}`;
      byId("graph-state").textContent = "La projection est indisponible.";
    }
  }
}

function showLegacyCreation() {
  beginWorkspaceContext("legacy-empty", 0);
  workspaceReady = false;
  byId("workbench-shell").hidden = true;
  byId("library-home").hidden = false;
  byId("library-title").textContent = "Créer l’enquête locale";
  byId("library-connection").textContent = "Aucune enquête ouverte";
  byId("library-list-section").hidden = true;
}

async function openWorkspace(workspaceId, button) {
  button.disabled = true;
  byId("library-connection").textContent = "Ouverture…";
  try {
    const opened = await libraryRequest("/api/v1/library/open", {
      method: "POST",
      headers: { "Content-Type": "application/json", "X-Labfy-CSRF": csrfToken },
      body: JSON.stringify({ workspace_id: workspaceId, expected_generation: libraryGeneration }),
    });
    if (opened.contract !== "labfy.web_library.open.v1")
      throw new Error("Contrat d’ouverture inattendu");
    libraryGeneration = opened.generation;
    await activateWorkspace(opened);
  } catch (error) {
    byId("library-connection").textContent = error.message;
    button.disabled = false;
    await refreshLibrary().catch(() => {});
  }
}

function configureApplication() {
  configureControls();
  configureResizableLayout();
  configureGraphInteractions();
  configureImport();
  const drawerCollapsed = localStorage.getItem("labfy-pane:drawer") === "collapsed";
  if (drawerCollapsed && !byId("work-panel").classList.contains("collapsed"))
    byId("work-panel-toggle").click();
  const submitAgentPrompt = () => void callAgentTool().catch((error) => {
    if (error.name !== "AbortError") {
      appendAgentCard("ERROR", error.message);
      byId("agent-status").textContent = error.message;
    }
  });
  byId("agent-send").addEventListener("click", submitAgentPrompt);
  byId("agent-resume").addEventListener("click", () =>
    void controlAgentTurn("resume").catch((error) => appendAgentCard("ERROR", error.message)));
  byId("agent-cancel").addEventListener("click", () =>
    void controlAgentTurn("cancel").catch((error) => appendAgentCard("ERROR", error.message)));
  byId("agent-mission-start").addEventListener("click", () =>
    void startAgentMission().catch((error) => {
      byId("agent-mission-state").textContent = error.message;
    }));
  byId("agent-mission-cancel").addEventListener("click", () =>
    void cancelAgentMission().catch((error) => {
      byId("agent-mission-state").textContent = error.message;
    }));
  byId("agent-proposals-refresh").addEventListener("click", () =>
    void refreshAgentProposals().catch((error) => {
      byId("agent-mission-state").textContent = error.message;
    }));
  byId("agent-prompt").addEventListener("keydown", (event) => {
    if (event.key === "Enter" && (event.ctrlKey || event.metaKey)) {
      event.preventDefault();
      submitAgentPrompt();
    }
  });
  byId("home-button").addEventListener("click", () => void showLibrary({ focus: true }));
  byId("library-load").addEventListener("click", async (event) => {
    const button = event.currentTarget;
    button.disabled = true;
    byId("library-connection").textContent = "Actualisation…";
    try {
      await refreshLibrary();
    } catch (error) {
      byId("library-connection").textContent = error.message;
    } finally {
      button.disabled = false;
    }
  });
  byId("workspace-create-form").addEventListener("submit", async (event) => {
    event.preventDefault();
    const button = event.currentTarget.querySelector("button");
    button.disabled = true;
    byId("workspace-create-status").textContent = "Création…";
    try {
      const title = byId("workspace-title").value.trim();
      if (pendingCreateIntent?.title !== title)
        pendingCreateIntent = { title, idempotencyKey: newUuid() };
      const path = libraryMode ? "/api/v1/library/workspaces" : "/api/v1/workspace";
      const body = libraryMode
        ? { title, idempotency_key: pendingCreateIntent.idempotencyKey }
        : { title };
      const created = await libraryRequest(path, {
        method: "POST",
        headers: { "Content-Type": "application/json", "X-Labfy-CSRF": csrfToken },
        body: JSON.stringify(body),
      });
      if (libraryMode && created.contract !== "labfy.web_library.workspace.v1")
        throw new Error("Contrat de création inattendu");
      byId("workspace-title").value = "";
      pendingCreateIntent = null;
      if (libraryMode) {
        byId("workspace-create-status").textContent = `« ${created.title} » créée. Ouvrez-la depuis la liste.`;
        await refreshLibrary();
        document.querySelector(`[data-workspace-id="${CSS.escape(created.workspace_id)}"]`)?.focus();
      } else {
        byId("workspace-create").hidden = true;
        await activateLegacyWorkspace({ workspace_state: "READY", title,
          investigation_id: created.investigation_id, generation: 0 });
      }
    } catch (error) {
      byId("workspace-create-status").textContent = error.message;
    } finally { button.disabled = false; }
  });
  byId("jobs-note").textContent = "Contrôle local authentifié.";
  byId("research-form").addEventListener("submit", (event) => {
    event.preventDefault();
    byId("research-note").textContent = "Préparation…";
    void prepareResearch().catch((error) => {
      if (error.name !== "AbortError")
        byId("research-note").textContent = error.message;
    });
  });
  byId("research-launch").addEventListener("click", () => {
    byId("research-note").textContent = "Admission et campagne…";
    void launchResearch().catch((error) => {
      if (error.name !== "AbortError")
        byId("research-note").textContent = error.message;
    });
  });
  byId("research-revoke").addEventListener("click", () =>
    void revokeResearch().catch((error) => {
      if (error.name !== "AbortError")
        byId("research-note").textContent = error.message;
    }),
  );
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
  byId("planner-launch").addEventListener("click", async () => {
      const ids = [
        ...byId("planner-list").querySelectorAll("input:checked"),
      ].map((input) => input.value);
      if (!ids.length || !plannerValue) return;
      const key = newUuid();
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
}

async function start() {
  try {
    const sessionResponse = await fetch("/api/v1/session", { cache: "no-store" });
    const session = sessionResponse.ok ? await sessionResponse.json() : null;
    csrfToken = session?.csrf ?? null;
    operationalMode = session !== null;
    libraryMode = session?.library_mode === true;
    configureApplication();
    // WHY: seule l'application bibliothèque ouvre toujours sur l'accueil ; les
    // serveurs historiques et de démonstration conservent leur contrat direct.
    if (libraryMode) await showLibrary({ load: session?.library_load_required !== true });
    else if (session?.workspace_state === "EMPTY") showLegacyCreation();
    else await activateLegacyWorkspace(session);
  } catch (error) {
    byId("library-home").hidden = false;
    byId("library-connection").textContent = error.message;
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
    workspaceId: workspaceContext?.workspaceId ?? null,
    workspaceGeneration: workspaceContext?.generation ?? null,
    sessionExpired,
    refreshTimerCount: refreshTimers.size,
    hasCsrf: csrfToken !== null,
  }),
  processEventData,
  selectNode,
};

void start();
