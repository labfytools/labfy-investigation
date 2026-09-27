import { edgePath, parallelEdgeOffsets } from "./graph-state.js";

const SVG_NAMESPACE = "http://www.w3.org/2000/svg";

// CONTRACT: object_kind vient du modèle C. Les anciens snapshots J2/J3 qui
// n'en possèdent pas conservent une adaptation explicite par type historique.
export function presentationKind(node) {
  if (
    ["evidence", "extraction", "observation", "entity"].includes(
      node.object_kind,
    )
  )
    return node.object_kind;
  if (["EVIDENCE", "SOURCE"].includes(node.type)) return "evidence";
  if (node.type === "EXTRACTION") return "extraction";
  if (node.type === "OBSERVATION") return "observation";
  return "entity";
}

function svgElement(name, attributes = {}) {
  const element = document.createElementNS(SVG_NAMESPACE, name);
  for (const [key, value] of Object.entries(attributes)) {
    element.setAttribute(key, value);
  }
  return element;
}

export class GraphRenderer {
  constructor({ svg, viewport, edgesLayer, nodesLayer, pinned, callbacks }) {
    this.svg = svg;
    this.viewport = viewport;
    this.edgesLayer = edgesLayer;
    this.nodesLayer = nodesLayer;
    this.pinned = pinned;
    this.callbacks = callbacks;
    this.graph = { nodes: [], edges: [] };
    this.edgeElements = new Map();
    this.edgeOffsets = new Map();
    this.nodeElements = new Map();
    this.nodeMap = new Map();
    this.scale = 1;
  }

  position(node) {
    return this.pinned.get(node.id) ?? { x: node.x, y: node.y };
  }

  render(graph, { selectedNodeId, scale, offset, focusNodeId = null }) {
    this.graph = graph;
    this.nodeMap = new Map(graph.nodes.map((node) => [node.id, node]));
    this.scale = scale;
    this.edgeOffsets = parallelEdgeOffsets(graph.edges);
    this.edgeElements.clear();
    this.nodeElements.clear();
    this.edgesLayer.replaceChildren();
    this.nodesLayer.replaceChildren();

    const neighbors = new Set([selectedNodeId]);
    for (const edge of graph.edges) {
      if (edge.source === selectedNodeId) neighbors.add(edge.target);
      if (edge.target === selectedNodeId) neighbors.add(edge.source);
      this.renderEdge(edge, scale, neighbors);
    }
    for (const node of graph.nodes)
      this.renderNode(node, node.id === selectedNodeId, scale, neighbors);
    this.updateViewport(scale, offset);

    if (focusNodeId) {
      this.nodeElements.get(focusNodeId)?.focus({ preventScroll: true });
    }
  }

  renderEdge(edge, scale, neighbors) {
    const group = svgElement("g", {
      class: `edge-group ${edge.kind}`,
      tabindex: "0",
      "data-edge-id": edge.id,
      role: "button",
      "aria-label": `Relation ${edge.kind}, ${edge.id}, de ${edge.source} vers ${edge.target}`,
    });
    const pathId = `edge-path-${edge.id}`;
    const path = svgElement("path", {
      id: pathId,
      class: `edge ${edge.kind}`,
      "data-edge-id": edge.id,
      "marker-end": edge.directed ? "url(#arrowhead)" : "",
    });
    const label = svgElement("text", {
      class: `edge-label${
        scale >= 0.85 ||
        neighbors.has(edge.source) ||
        neighbors.has(edge.target)
          ? ""
          : " label-hidden"
      }`,
      "text-anchor": "middle",
    });
    label.textContent = `${edge.kind} · ${edge.semantic ?? edge.id}`;
    group.append(path, label);
    group.addEventListener("click", (event) => {
      event.stopPropagation();
      this.callbacks.onEdgeSelect(edge.id);
    });
    group.addEventListener("keydown", (event) => {
      if (event.key === "Enter") this.callbacks.onEdgeSelect(edge.id);
    });
    this.edgesLayer.append(group);
    this.edgeElements.set(edge.id, { path, label });
    this.updateEdge(edge);
  }

  renderNode(node, selected, scale, neighbors) {
    const position = this.position(node);
    const kind = presentationKind(node);
    const group = svgElement("g", {
      class: `node${selected ? " selected" : ""}`,
      transform: `translate(${position.x} ${position.y})`,
      tabindex: "0",
      "data-id": node.id,
      "data-type": node.type,
      "data-kind": kind,
      role: "button",
      "aria-label": `${node.type} ${node.label}`,
    });
    if (kind === "evidence")
      group.append(
        svgElement("rect", { x: -24, y: -18, width: 48, height: 36, rx: 4 }),
      );
    else if (kind === "extraction")
      group.append(svgElement("path", { d: "M0,-23 L23,0 L0,23 L-23,0 Z" }));
    else if (kind === "observation")
      group.append(
        svgElement("rect", { x: -20, y: -20, width: 40, height: 40, rx: 20 }),
      );
    else group.append(svgElement("circle", { r: 20 }));
    const showLabel = selected || neighbors.has(node.id) || scale >= 0.78;
    const text = svgElement("text", {
      y: 38,
      class: showLabel ? "node-label" : "node-label label-hidden",
    });
    text.textContent =
      node.label.length > 32 ? `${node.label.slice(0, 29)}…` : node.label;
    group.append(text);
    this.enableDrag(group, node);
    this.nodesLayer.append(group);
    this.nodeElements.set(node.id, group);
  }

  enableDrag(group, node) {
    let drag = null;
    let suppressClick = false;
    group.addEventListener("pointerdown", (event) => {
      if (event.button !== 0) return;
      event.stopPropagation();
      const position = this.position(node);
      drag = {
        pointerId: event.pointerId,
        startX: event.clientX,
        startY: event.clientY,
        nodeX: position.x,
        nodeY: position.y,
        moved: false,
      };
      group.setPointerCapture(event.pointerId);
    });
    group.addEventListener("pointermove", (event) => {
      if (!drag || event.pointerId !== drag.pointerId) return;
      const deltaX = event.clientX - drag.startX;
      const deltaY = event.clientY - drag.startY;
      if (Math.hypot(deltaX, deltaY) > 4) drag.moved = true;
      const position = {
        x: drag.nodeX + deltaX / this.scale,
        y: drag.nodeY + deltaY / this.scale,
      };
      this.pinned.set(node.id, position);
      group.setAttribute("transform", `translate(${position.x} ${position.y})`);
      this.updateConnectedEdges(node.id);
    });
    const finish = (event) => {
      if (!drag || event.pointerId !== drag.pointerId) return;
      suppressClick = drag.moved;
      if (group.hasPointerCapture(event.pointerId)) {
        group.releasePointerCapture(event.pointerId);
      }
      drag = null;
    };
    group.addEventListener("pointerup", finish);
    group.addEventListener("pointercancel", finish);
    group.addEventListener("click", (event) => {
      event.stopPropagation();
      if (suppressClick) {
        suppressClick = false;
        return;
      }
      this.callbacks.onNodeSelect(node.id);
    });
    group.addEventListener("keydown", (event) => {
      if (event.key === "Enter") this.callbacks.onNodeSelect(node.id, true);
    });
  }

  updateConnectedEdges(nodeId) {
    for (const edge of this.graph.edges) {
      if (edge.source === nodeId || edge.target === nodeId)
        this.updateEdge(edge);
    }
  }

  updateEdge(edge) {
    const source = this.nodeMap.get(edge.source);
    const target = this.nodeMap.get(edge.target);
    const elements = this.edgeElements.get(edge.id);
    if (!source || !target || !elements) return;
    const sourcePosition = this.position(source);
    const targetPosition = this.position(target);
    const offset = this.edgeOffsets.get(edge.id) ?? 0;
    const dx = targetPosition.x - sourcePosition.x;
    const dy = targetPosition.y - sourcePosition.y;
    const length = Math.hypot(dx, dy) || 1;
    // CONTRACT: le marqueur dirigé s'arrête avant la forme cible ; une flèche
    // placée au centre serait masquée par le nœud rendu au-dessus des arêtes.
    const pathSource = {
      x: sourcePosition.x + (dx / length) * 24,
      y: sourcePosition.y + (dy / length) * 24,
    };
    const pathTarget = {
      x: targetPosition.x - (dx / length) * 28,
      y: targetPosition.y - (dy / length) * 28,
    };
    elements.path.setAttribute("d", edgePath(pathSource, pathTarget, offset));
    elements.label.setAttribute(
      "x",
      (sourcePosition.x + targetPosition.x) / 2 - (dy / length) * offset,
    );
    elements.label.setAttribute(
      "y",
      (sourcePosition.y + targetPosition.y) / 2 + (dx / length) * offset - 5,
    );
  }

  updateViewport(scale, offset) {
    this.scale = scale;
    this.viewport.setAttribute(
      "transform",
      `translate(${this.svg.clientWidth / 2 + offset.x} ${
        this.svg.clientHeight / 2 + offset.y
      }) scale(${scale})`,
    );
  }
}
