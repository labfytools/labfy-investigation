#!/usr/bin/env python3
"""Real Qwen + rootless Podman smoke on a disposable SPECIMEN workspace.

Run with ``--endpoint`` and ``--model`` for an already READY local Qwen model.
Only this script's unique toolbox image is removed; no other Podman image or
llama-server process is owned or stopped by the smoke.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import tempfile
import uuid
from pathlib import Path

from tool_provisioning import PodmanToolboxBuilder, ToolProvisioningService
from workspace_server import Handler, WorkspaceServer


ROOT = Path(__file__).resolve().parents[2]


def new_id():
    return str(uuid.uuid4())


def specimen_workspace(root):
    workspace = root / "workspace-SPECIMEN"
    workspace.mkdir()
    bridge = ROOT / "tools/local-jobs"
    for command in ("init-specimen", "export"):
        subprocess.run([str(bridge), command, "--workspace", str(workspace)],
                       cwd=ROOT, check=True, capture_output=True, timeout=30)
    graph_path = workspace / "core-snapshot.json"
    graph = json.loads(graph_path.read_text(encoding="utf-8"))
    object_id = new_id()
    node = {"id": "entity:" + object_id, "object_id": object_id,
            "object_kind": "username", "label": "SPECIMEN_user",
            "type": "username", "group": "entity", "state": "proposed", "raw": {}}
    graph["nodes"].append(node)
    graph_path.write_text(json.dumps(graph, ensure_ascii=False), encoding="utf-8")
    return workspace, bridge, node


def wait(runtime, scope, turn_id, states, seconds=240):
    record = runtime.wait_for_state(scope, turn_id, states, timeout=seconds)
    if record["state"] not in states:
        raise RuntimeError("état Agent inattendu")
    return record


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--endpoint", required=True)
    parser.add_argument("--model", required=True)
    parser.add_argument("--trace-model", action="store_true")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="labfy-SPECIMEN-real-provision-") as name:
        root = Path(name)
        workspace, bridge, node = specimen_workspace(root)
        image_prefix = "localhost/labfy-investigation-toolbox-" + uuid.uuid4().hex[:12]
        service = ToolProvisioningService(
            root / "xdg-data" / "labfy-investigation" / "toolbox",
            builder=PodmanToolboxBuilder(image_prefix=image_prefix))
        server = WorkspaceServer(("127.0.0.1", 0), Handler, workspace=workspace,
                                 bridge=bridge, agent_mode="local-model",
                                 agent_endpoint=args.endpoint, agent_model=args.model,
                                 agent_timeout=90, tool_provisioning=service)
        image_ref = None
        try:
            if server.agent_runtime is None:
                raise RuntimeError(server.agent_runtime_reason or "Qwen indisponible")
            if args.trace_model:
                original_complete = server.agent_runtime._model_client.complete
                def traced_complete(messages):
                    raw = original_complete(messages)
                    print("MODEL_RAW=" + raw[:1200], flush=True)
                    return raw
                server.agent_runtime._model_client.complete = traced_complete
            workspace_id = server.agent_workspace_id()
            server.start_agent_mission(workspace_id, {
                "goal": "Analyser le pseudo SPECIMEN_user avec jq offline",
                "scoped_refs": [{"object_id": node["id"]}],
                "pivots": [{"object_id": node["id"]}],
                "allowed_risk_classes": ["LOCAL_READ_ONLY"],
                "network_profile": "OFFLINE", "max_contacts": 0,
                "max_duration_seconds": 900, "max_tool_calls": 16,
            }, new_id(), True)
            scope = server.agent_scope()
            objective = (
                "Analyse SPECIMEN_user avec jq offline. Utilise investigation.search "
                "pour son object_id, tool.catalog, tool.provision.search puis "
                "tool.provision.propose pour jq. Après accord, lis tool.docs.read "
                "debian.jq, puis appelle tool.integration.propose avec le request_id "
                "du document et proposal={example_id:'jq.username.v1'}. "
                "Après activation, appelle capability.execute sur ce pseudo puis conclus."
            )
            turn = server.agent_runtime.start_turn(scope, {
                "objective": objective, "idempotency_key": new_id()})
            turn_id = turn["turn_id"]
            print("turn_id=" + turn_id, flush=True)
            first = wait(server.agent_runtime, scope, turn_id,
                         {"TOOL_PROVISIONING_REQUIRED", "FAILED", "COMPLETED",
                          "MODEL_PROTOCOL_ERROR", "BUDGET_EXHAUSTED"})
            if first["state"] != "TOOL_PROVISIONING_REQUIRED":
                raise RuntimeError("Gate A non atteinte: " + first["state"] +
                                   " / " + str(first["diagnostic"]))
            request_id = first["pending_call"]["result"]["output"]["request_id"]
            print("gate_a_request=" + request_id, flush=True)
            service.approve_provision(request_id, decision_id=new_id(), actor="human",
                                      human_confirmed=True)
            quarantined = service.build(request_id)
            if quarantined["state"] != "QUARANTINED":
                raise RuntimeError("quarantaine absente")
            generation = service.store.read_generation(quarantined["generation_id"])
            image_ref = generation["image_ref"]
            if not generation["health"]["healthy"]:
                raise RuntimeError("healthcheck toolbox non prêt")
            print("quarantine=" + image_ref, "health=" + str(generation["health"]),
                  flush=True)
            server.resume_agent_runtime(turn_id)
            second = wait(server.agent_runtime, scope, turn_id,
                          {"TOOL_INTEGRATION_REQUIRED", "FAILED", "COMPLETED",
                           "MODEL_PROTOCOL_ERROR", "BUDGET_EXHAUSTED"})
            if second["state"] != "TOOL_INTEGRATION_REQUIRED":
                for event in server.agent_gateway.events(scope, 0)["events"]:
                    if event["kind"] == "agent.tool.completed" and \
                            event["payload"].get("state") == "FAILED":
                        result = server.agent_gateway.result(
                            scope, event["payload"]["result_id"])
                        print("GATEWAY_FAILURE=" + str(result.get("diagnostic")), flush=True)
                raise RuntimeError("Gate B non atteinte: " + second["state"] +
                                   " / " + str(second["diagnostic"]))
            integrated = service.get(request_id)
            if integrated["state"] != "WAITING_INTEGRATION_APPROVAL":
                raise RuntimeError("manifest Qwen absent")
            print("gate_b_proposal=" + integrated["integration"]["proposal_id"], flush=True)
            service.approve_integration(request_id, decision_id=new_id(), actor="human",
                                        human_confirmed=True)
            service.activate(request_id)
            manifests = server.dynamic_capability_manifests()
            if not any(item["capability_id"] == "data.jq.username"
                       for item in manifests["applications"]):
                raise RuntimeError("menu dynamique SPECIMEN absent")
            server.resume_agent_runtime(turn_id)
            terminal = wait(server.agent_runtime, scope, turn_id,
                            {"COMPLETED", "FAILED", "MODEL_PROTOCOL_ERROR",
                             "BUDGET_EXHAUSTED"})
            events = server.agent_gateway.events(scope, 0)["events"]
            calls = [event["payload"]["tool_id"] for event in events
                     if event["kind"] == "agent.tool.completed"]
            for event in events:
                if event["kind"] == "agent.tool.completed" and \
                        event["payload"].get("state") == "FAILED":
                    result = server.agent_gateway.result(scope,
                        event["payload"]["result_id"])
                    print("GATEWAY_FAILURE=" + str(result.get("diagnostic")), flush=True)
            print("terminal=" + terminal["state"], "model_calls=" +
                  str(terminal["budgets"]["model_calls"]), "tool_calls=" +
                  str(terminal["budgets"]["tool_calls"]), "tool_ids=" +
                  ",".join(calls), "final=" + str(bool(terminal["final"])), flush=True)
            if terminal["state"] != "COMPLETED" or not terminal["final"] or \
                    "capability.execute" not in calls:
                raise RuntimeError("exécution Qwen/capability incomplète")
            print("REAL_TOOL_PROVISIONING_SMOKE=PASS", flush=True)
        finally:
            server.server_close()
            if image_ref:
                subprocess.run(["podman", "image", "rm", image_ref],
                               check=False, capture_output=True, timeout=60)


if __name__ == "__main__":
    main()
