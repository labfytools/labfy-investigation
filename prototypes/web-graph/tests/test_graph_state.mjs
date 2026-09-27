import test from "node:test";
import assert from "node:assert/strict";

import {
  applyGraphEvent,
  capabilitiesForNode,
  edgePath,
  neighborhood,
  parallelEdgeOffsets,
  provenanceOrigins,
  visibleGraph,
} from "../public/graph-state.js";

const nodes = [
  { id: "a", object_kind: "source", object_id: "A", type: "SOURCE", state: "observed", group: "evidence", label: "A" },
  { id: "b", object_kind: "source", object_id: "B", type: "SOURCE", state: "observed", group: "evidence", label: "B" },
  { id: "c", object_kind: "evidence", object_id: "C", type: "EVIDENCE", state: "observed", group: "evidence", label: "C", raw: null },
  { id: "d", object_kind: "person", object_id: "D", type: "PERSON", state: "under_review", group: "identity", label: "D", raw: null },
];

const snapshot = {
  contract: "labfy.web_graph.snapshot.v1",
  revision: 1,
  nodes,
  edges: [
    { id: "e1", source: "a", target: "c", kind: "provenance" },
    { id: "e2", source: "b", target: "c", kind: "provenance" },
    { id: "e3", source: "c", target: "d", kind: "support" },
  ],
  capability_catalog: [
    { id: "focus-neighborhood", intent: "Focus", network_contact: "NONE" },
    { id: "rdap", intent: "RDAP", network_contact: "THIRD_PARTY" },
  ],
  capabilities: [
    {
      node_id: "d",
      object_ref: { object_kind: "person", object_id: "D" },
      capability_id: "focus-neighborhood",
      available: true,
      reason: "fixture",
    },
    {
      node_id: "d",
      object_ref: { object_kind: "domain", object_id: "WRONG" },
      capability_id: "rdap",
      available: false,
      reason: "mauvaise référence",
    },
  ],
};

test("la projection commune combine filtres, repli et focus sans muter la source", () => {
  assert.equal(visibleGraph(snapshot).nodes.length, 4);
  assert.equal(visibleGraph(snapshot, { type: "SOURCE" }).nodes.length, 2);
  assert.equal(visibleGraph(snapshot, {}, new Set(["evidence"])).hidden, 3);
  assert.deepEqual(
    visibleGraph(snapshot, {}, new Set(), "d").nodes.map((node) => node.id).sort(),
    ["c", "d"],
  );
  assert.equal(visibleGraph(snapshot, { type: "DOMAIN" }).nodes.length, 0);
  assert.equal(snapshot.nodes.length, 4);
});

test("le voisinage conserve les identités stables", () => {
  assert.deepEqual([...neighborhood(snapshot, "c")].sort(), ["a", "b", "c", "d"]);
});

test("la provenance conserve deux origines séparées", () => {
  const result = provenanceOrigins(snapshot, "c");
  assert.deepEqual(
    result.branches.map((branch) => branch.map((node) => node.id)),
    [["c", "a"], ["c", "b"]],
  );
  assert.equal(result.cycleDetected, false);
});

test("la provenance signale une source manquante et borne un cycle", () => {
  const broken = {
    ...snapshot,
    edges: [
      { id: "missing", source: "absent", target: "c", kind: "provenance" },
      { id: "cycle-1", source: "d", target: "c", kind: "provenance" },
      { id: "cycle-2", source: "c", target: "d", kind: "provenance" },
    ],
  };
  const result = provenanceOrigins(broken, "c");
  assert.deepEqual(result.missingSourceIds, ["absent"]);
  assert.equal(result.cycleDetected, true);
  assert.ok(result.branches.length <= 20);
});

test("les capabilities exigent une référence objet cohérente", () => {
  assert.deepEqual(
    capabilitiesForNode(snapshot, "d").map((capability) => capability.id),
    ["focus-neighborhood"],
  );
  assert.deepEqual(capabilitiesForNode(snapshot, "unknown"), []);
});

test("les liens parallèles reçoivent des trajectoires déterministes distinctes", () => {
  const edges = [
    { id: "z", source: "a", target: "b" },
    { id: "a", source: "a", target: "b" },
  ];
  const offsets = parallelEdgeOffsets(edges);
  assert.equal(offsets.get("a"), -17);
  assert.equal(offsets.get("z"), 17);
  assert.notEqual(
    edgePath({ x: 0, y: 0 }, { x: 100, y: 0 }, offsets.get("a")),
    edgePath({ x: 0, y: 0 }, { x: 100, y: 0 }, offsets.get("z")),
  );
});

function graphEvent(overrides = {}) {
  return {
    contract: "labfy.web_graph.event.v1",
    id: "1",
    base_revision: 1,
    revision: 2,
    kind: "node_patch",
    node_id: "d",
    changes: { label: "D revue" },
    message: "fixture",
    ...overrides,
  };
}

test("un événement cohérent modifie réellement le snapshot", () => {
  const result = applyGraphEvent(snapshot, graphEvent());
  assert.equal(result.status, "applied");
  assert.equal(result.snapshot.revision, 2);
  assert.equal(result.snapshot.nodes.find((node) => node.id === "d").label, "D revue");
  assert.equal(snapshot.nodes.find((node) => node.id === "d").label, "D");
});

test("doublon, ancien, trou et contrat inattendu sont distingués", () => {
  assert.equal(applyGraphEvent({ ...snapshot, revision: 2 }, graphEvent()).status, "duplicate");
  assert.equal(
    applyGraphEvent(snapshot, graphEvent({ id: "2", base_revision: 2, revision: 3 })).status,
    "gap",
  );
  assert.equal(applyGraphEvent(snapshot, graphEvent({ contract: "unexpected" })).status, "invalid");
  assert.equal(
    applyGraphEvent(snapshot, graphEvent({ changes: { forbidden: true } })).status,
    "invalid",
  );
});

test("fin normale et demande de rattrapage ne mutent pas le snapshot", () => {
  assert.equal(
    applyGraphEvent(snapshot, graphEvent({ kind: "scenario_end", revision: 1, changes: {} })).status,
    "end",
  );
  assert.equal(
    applyGraphEvent(snapshot, graphEvent({ kind: "resync_required" })).status,
    "resync",
  );
});
