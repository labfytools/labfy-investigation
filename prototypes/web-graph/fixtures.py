"""Fixtures SPECIMEN déterministes, sans base d'enquête ni accès réseau."""

from __future__ import annotations

import math
import random
from copy import deepcopy

KINDS = (
    "PERSON", "USERNAME", "EMAIL", "DOMAIN", "IP", "EVIDENCE",
    "SOURCE", "OBSERVATION", "HYPOTHESIS", "TRANSACTION",
)

CAPABILITY_CATALOG = [
    {"id": "focus-neighborhood", "intent": "Explorer le voisinage", "network_contact": "NONE", "experimental": True},
    {"id": "show-provenance", "intent": "Remonter aux sources", "network_contact": "NONE", "experimental": True},
    {"id": "rdap", "intent": "Interroger RDAP", "network_contact": "THIRD_PARTY", "experimental": True},
]


def _node(identifier, kind, label, index, total, *, state="observed", group=None, raw=None):
    angle = (index / max(total, 1)) * math.tau
    radius = 180 + (index % 7) * 24
    return {
        "id": identifier,
        "object_kind": kind.lower(),
        "object_id": f"SPECIMEN-{identifier}",
        "type": kind,
        "label": label,
        "state": state,
        "x": round(math.cos(angle) * radius, 3),
        "y": round(math.sin(angle) * radius, 3),
        "group": group,
        "raw": raw,
    }


def _capability(node, capability_id, available, reason):
    # CONTRACT: l'applicabilité vient du serveur et porte l'identité métier ;
    # l'interface n'infère aucune action depuis le seul type du nœud.
    return {
        "node_id": node["id"],
        "object_ref": {"object_kind": node["object_kind"], "object_id": node["object_id"]},
        "capability_id": capability_id,
        "available": available,
        "reason": reason,
    }


def _demo_capabilities(nodes):
    by_id = {node["id"]: node for node in nodes}
    applications = [
        _capability(node, "focus-neighborhood", True, "Navigation locale sur la fixture")
        for node in nodes
    ]
    for node_id in ("artifact", "transform", "observation"):
        applications.append(
            _capability(by_id[node_id], "show-provenance", True, "Origine synthétique disponible")
        )
    applications.append(
        _capability(
            by_id["domain"],
            "rdap",
            False,
            "Simulation — aucun outil ni contact réseau exécuté",
        )
    )
    return applications


def demo_snapshot():
    specs = [
        ("person-a", "PERSON", "Camille Exemple", "identity", None),
        ("person-b", "PERSON", "Morgan Exemple", "identity", None),
        ("username", "USERNAME", "atlas_specimen", "identity", None),
        ("email", "EMAIL", "atlas@demo.test", "identity", None),
        ("domain", "DOMAIN", "demo.test", "infrastructure", None),
        ("ip", "IP", "192.0.2.42", "infrastructure", None),
        ("source", "SOURCE", "Source SPECIMEN A", "evidence", "Source locale synthétique A"),
        ("source-b", "SOURCE", "Source SPECIMEN B", "evidence", "Source locale synthétique B"),
        ("artifact", "EVIDENCE", "Artefact SPECIMEN", "evidence", "Brut synthétique : atlas@demo.test"),
        ("transform", "OBSERVATION", "Extraction déterministe", "evidence", "Transformation v1"),
        ("observation", "OBSERVATION", "Pseudo observé", "evidence", "atlas_specimen"),
        ("hypothesis-a", "HYPOTHESIS", "Identité candidate", "analysis", None),
        ("hypothesis-b", "HYPOTHESIS", "Hypothèse contradictoire", "analysis", None),
        ("transaction", "TRANSACTION", "42,00 EUR — fictif", "finance", "Montant fictif non vérifié"),
    ]
    nodes = [
        _node(
            identifier,
            kind,
            label,
            index,
            len(specs),
            state="under_review" if kind == "HYPOTHESIS" else "observed",
            group=group,
            raw=raw,
        )
        for index, (identifier, kind, label, group, raw) in enumerate(specs)
    ]
    links = [
        ("e1", "person-a", "username", "business"),
        ("e2", "person-b", "username", "support"),
        ("e3", "email", "domain", "business"),
        ("e4", "domain", "ip", "business"),
        ("e5", "source", "artifact", "provenance"),
        ("e5b", "source-b", "artifact", "provenance"),
        ("e6", "artifact", "transform", "provenance"),
        ("e7", "transform", "observation", "provenance"),
        ("e8", "observation", "hypothesis-a", "support"),
        ("e9", "observation", "hypothesis-b", "contradiction"),
        ("e10", "person-a", "transaction", "business"),
        ("e11", "person-a", "username", "support"),
    ]
    edges = [
        {
            "id": identifier,
            "source": source,
            "target": target,
            "kind": kind,
            "directed": True,
            "review_state": "unreviewed",
            "time_precision": None,
        }
        for identifier, source, target, kind in links
    ]
    return {
        "contract": "labfy.web_graph.snapshot.v1",
        "investigation": {"id": "SPECIMEN-WEB-GRAPH", "label": "Enquête synthétique J2", "synthetic": True},
        "revision": 1,
        "nodes": nodes,
        "edges": edges,
        "capability_catalog": deepcopy(CAPABILITY_CATALOG),
        "capabilities": _demo_capabilities(nodes),
    }


def scenario_events():
    # INVARIANT: les identifiants/révisions appartiennent au scénario serveur ;
    # ils ne sont jamais calculés depuis un curseur fourni par le client.
    return [
        {
            "contract": "labfy.web_graph.event.v1",
            "id": "1",
            "base_revision": 1,
            "revision": 2,
            "kind": "node_patch",
            "node_id": "person-a",
            "changes": {"label": "Camille Exemple · revue", "state": "under_review"},
            "message": "Libellé et état synthétiques actualisés",
        },
        {
            "contract": "labfy.web_graph.event.v1",
            "id": "2",
            "base_revision": 2,
            "revision": 3,
            "kind": "node_patch",
            "node_id": "observation",
            "changes": {"label": "Pseudo observé · confirmé par fixture"},
            "message": "Deuxième mise à jour synthétique appliquée",
        },
        {
            "contract": "labfy.web_graph.event.v1",
            "id": "3",
            "base_revision": 3,
            "revision": 3,
            "kind": "scenario_end",
            "node_id": None,
            "changes": {},
            "message": "Fin normale du scénario synthétique",
        },
    ]


def scenario_snapshot():
    snapshot = demo_snapshot()
    for event in scenario_events():
        if event["kind"] != "node_patch":
            continue
        node = next(node for node in snapshot["nodes"] if node["id"] == event["node_id"])
        node.update(event["changes"])
        snapshot["revision"] = event["revision"]
    return snapshot


def generated_snapshot(size, *, latest=False):
    if size == "demo":
        return scenario_snapshot() if latest else demo_snapshot()
    count = int(size)
    if count not in (100, 1000, 5000):
        raise ValueError("taille non autorisée")
    rng = random.Random(20260925 + count)
    nodes = [
        _node(
            f"node-{index}",
            KINDS[index % len(KINDS)],
            f"SPECIMEN {KINDS[index % len(KINDS)]} {index}",
            index,
            count,
            group=f"group-{index % 12}",
        )
        for index in range(count)
    ]
    edges = []
    for index in range(count * 2):
        source = index % count
        target = (source + 1 + rng.randrange(max(1, min(count - 1, 31)))) % count
        edges.append(
            {
                "id": f"edge-{index}",
                "source": f"node-{source}",
                "target": f"node-{target}",
                "kind": ("business", "support", "provenance", "contradiction")[index % 4],
                "directed": True,
                "review_state": "unreviewed",
                "time_precision": None,
            }
        )
    base = demo_snapshot()
    base["investigation"] = {
        "id": f"SPECIMEN-GENERATED-{count}",
        "label": f"Jeu synthétique {count}",
        "synthetic": True,
    }
    base["nodes"] = nodes
    base["edges"] = edges
    base["capabilities"] = [
        _capability(node, "focus-neighborhood", True, "Navigation locale sur la fixture générée")
        for node in nodes
    ]
    return base
