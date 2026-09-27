const EVENT_CONTRACT = "labfy.web_graph.event.v1";
const PATCH_FIELDS = new Set(["label", "state", "raw"]);

export function neighborhood(snapshot, nodeId) {
  const ids = new Set([nodeId]);
  for (const edge of snapshot.edges) {
    if (edge.source === nodeId) ids.add(edge.target);
    if (edge.target === nodeId) ids.add(edge.source);
  }
  return ids;
}

export function visibleGraph(
  snapshot,
  filters = {},
  collapsed = new Set(),
  focusNodeId = null,
) {
  const focusIds = focusNodeId ? neighborhood(snapshot, focusNodeId) : null;
  const nodes = snapshot.nodes.filter(
    (node) =>
      (!filters.type || node.type === filters.type) &&
      (!filters.state || node.state === filters.state) &&
      !collapsed.has(node.group) &&
      (!focusIds || focusIds.has(node.id)),
  );
  const ids = new Set(nodes.map((node) => node.id));
  return {
    nodes,
    edges: snapshot.edges.filter(
      (edge) => ids.has(edge.source) && ids.has(edge.target),
    ),
    hidden: snapshot.nodes.length - nodes.length,
  };
}

export function capabilitiesForNode(snapshot, nodeId) {
  const node = snapshot.nodes.find((candidate) => candidate.id === nodeId);
  if (!node) return [];
  const catalog = new Map(
    snapshot.capability_catalog.map((capability) => [capability.id, capability]),
  );
  return snapshot.capabilities
    .filter(
      (application) =>
        application.node_id === nodeId &&
        application.object_ref.object_kind === node.object_kind &&
        application.object_ref.object_id === node.object_id,
    )
    .map((application) => ({
      ...catalog.get(application.capability_id),
      ...application,
    }))
    .filter((capability) => capability.id);
}

export function provenanceOrigins(
  snapshot,
  nodeId,
  { maxDepth = 12, maxBranches = 20 } = {},
) {
  const nodes = new Map(snapshot.nodes.map((node) => [node.id, node]));
  const incoming = new Map();
  for (const edge of snapshot.edges) {
    if (edge.kind !== "provenance" || edge.target === edge.source) continue;
    if (!incoming.has(edge.target)) incoming.set(edge.target, []);
    incoming.get(edge.target).push(edge);
  }
  for (const edges of incoming.values()) {
    edges.sort((left, right) => left.id.localeCompare(right.id));
  }

  const result = {
    branches: [],
    missingSourceIds: [],
    cycleDetected: false,
    truncated: false,
  };
  if (!nodes.has(nodeId)) return result;

  function walk(currentId, path, seen) {
    if (result.branches.length >= maxBranches || path.length >= maxDepth) {
      result.truncated = true;
      result.branches.push(path);
      return;
    }
    const predecessors = incoming.get(currentId) ?? [];
    if (predecessors.length === 0) {
      result.branches.push(path);
      return;
    }
    for (const edge of predecessors) {
      if (seen.has(edge.source)) {
        result.cycleDetected = true;
        result.branches.push(path);
        continue;
      }
      const source = nodes.get(edge.source);
      if (!source) {
        result.missingSourceIds.push(edge.source);
        result.branches.push(path);
        continue;
      }
      walk(edge.source, [...path, source], new Set([...seen, edge.source]));
    }
  }

  walk(nodeId, [nodes.get(nodeId)], new Set([nodeId]));
  result.missingSourceIds = [...new Set(result.missingSourceIds)].sort();
  return result;
}

export function parallelEdgeOffsets(edges, spacing = 34) {
  const groups = new Map();
  for (const edge of edges) {
    const key = [edge.source, edge.target].sort().join("\u0000");
    if (!groups.has(key)) groups.set(key, []);
    groups.get(key).push(edge);
  }
  const offsets = new Map();
  for (const group of groups.values()) {
    group.sort((left, right) => left.id.localeCompare(right.id));
    group.forEach((edge, index) => {
      offsets.set(edge.id, (index - (group.length - 1) / 2) * spacing);
    });
  }
  return offsets;
}

export function edgePath(source, target, offset = 0) {
  const dx = target.x - source.x;
  const dy = target.y - source.y;
  const length = Math.hypot(dx, dy) || 1;
  const middleX = (source.x + target.x) / 2 - (dy / length) * offset;
  const middleY = (source.y + target.y) / 2 + (dx / length) * offset;
  if (Math.abs(offset) < 0.001) {
    return `M ${source.x} ${source.y} L ${target.x} ${target.y}`;
  }
  return `M ${source.x} ${source.y} Q ${middleX} ${middleY} ${target.x} ${target.y}`;
}

export function applyGraphEvent(snapshot, event) {
  if (
    !event ||
    event.contract !== EVENT_CONTRACT ||
    !/^\d+$/.test(event.id ?? "") ||
    !Number.isInteger(event.revision) ||
    !Number.isInteger(event.base_revision)
  ) {
    return { status: "invalid", snapshot };
  }
  if (event.kind === "resync_required") {
    return { status: "resync", snapshot };
  }
  if (event.kind === "scenario_end") {
    const coherent =
      event.base_revision === snapshot.revision &&
      event.revision === snapshot.revision;
    return { status: coherent ? "end" : "gap", snapshot };
  }
  if (event.kind !== "node_patch") {
    return { status: "invalid", snapshot };
  }
  if (event.revision <= snapshot.revision) {
    return { status: "duplicate", snapshot };
  }
  if (
    event.base_revision !== snapshot.revision ||
    event.revision !== snapshot.revision + 1
  ) {
    return { status: "gap", snapshot };
  }
  const nodeIndex = snapshot.nodes.findIndex((node) => node.id === event.node_id);
  const changes = event.changes;
  if (
    nodeIndex < 0 ||
    !changes ||
    Object.keys(changes).length === 0 ||
    Object.keys(changes).some((field) => !PATCH_FIELDS.has(field))
  ) {
    return { status: "invalid", snapshot };
  }
  const updatedNode = { ...snapshot.nodes[nodeIndex], ...changes };
  if (
    typeof updatedNode.label !== "string" ||
    typeof updatedNode.state !== "string" ||
    !["string", "object"].includes(typeof updatedNode.raw)
  ) {
    return { status: "invalid", snapshot };
  }
  const nodes = [...snapshot.nodes];
  nodes[nodeIndex] = updatedNode;
  return {
    status: "applied",
    snapshot: { ...snapshot, revision: event.revision, nodes },
  };
}
