#!/usr/bin/env python3
"""Real Qwen developer smoke in a disposable clone and detached dev worktree."""

from __future__ import annotations

import argparse
import json
import os
import shutil
import socket
import subprocess
import tempfile
import time
import uuid
from urllib.request import urlopen
from pathlib import Path

from code_change import CodeChangeService
from tool_provisioning import PodmanToolboxBuilder, ToolProvisioningService
from workspace_server import Handler, WorkspaceServer


ROOT = Path(__file__).resolve().parents[2]


def new_id():
    return str(uuid.uuid4())


def workspace_fixture(root):
    workspace = root / "workspace-SPECIMEN"
    workspace.mkdir()
    bridge = ROOT / "tools/local-jobs"
    for command in ("init-specimen", "export"):
        subprocess.run([str(bridge), command, "--workspace", str(workspace)],
                       cwd=ROOT, check=True, capture_output=True, timeout=30)
    graph_path = workspace / "core-snapshot.json"
    graph = json.loads(graph_path.read_text(encoding="utf-8"))
    identity = new_id()
    node = {"id": "entity:" + identity, "object_id": identity,
            "object_kind": "username", "label": "SPECIMEN_user",
            "type": "username", "group": "entity", "state": "proposed", "raw": {}}
    graph["nodes"].append(node)
    investigation_id = json.loads((workspace / ".labfy/runtime/specimen.json").read_text(
        encoding="utf-8"))["investigation_id"]
    graph["capabilities"].append({
        "node_id": node["id"],
        "object_ref": {"investigation_id": investigation_id,
                       "object_kind": "username", "object_id": identity},
        "capability_id": "focus-neighborhood", "available": True,
        "reason": "Navigation locale SPECIMEN",
    })
    graph_path.write_text(json.dumps(graph, ensure_ascii=False), encoding="utf-8")
    return workspace, bridge, node


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--endpoint", required=True)
    parser.add_argument("--model", required=True)
    parser.add_argument("--trace-model", action="store_true")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="labfy-SPECIMEN-real-developer-") as name:
        root = Path(name)
        clone = root / "repo-SPECIMEN"
        subprocess.run(["git", "clone", "--local", "--no-hardlinks", "--quiet",
                        str(ROOT), str(clone)], check=True, timeout=60)
        if subprocess.run(["git", "rev-parse", "HEAD"], cwd=clone,
                          capture_output=True, text=True, check=True).stdout.strip() != \
                "f72a192d4dd3f25f6fe1757d52b117abbfe0994d":
            raise RuntimeError("baseline du clone SPECIMEN inattendue")
        changes = CodeChangeService(clone, root / "xdg-state" / "labfy-investigation")
        workspace, bridge, node = workspace_fixture(root)
        image_prefix = "localhost/labfy-investigation-toolbox-" + uuid.uuid4().hex[:12]
        tooling = ToolProvisioningService(root / "xdg-data" / "labfy-investigation" / "toolbox",
            builder=PodmanToolboxBuilder(image_prefix=image_prefix))
        server = WorkspaceServer(("127.0.0.1", 0), Handler, workspace=workspace,
            bridge=bridge, agent_mode="local-model", agent_endpoint=args.endpoint,
            agent_model=args.model, agent_timeout=90, tool_provisioning=tooling,
            code_changes=changes)
        image_ref = None
        try:
            if server.agent_runtime is None:
                raise RuntimeError(server.agent_runtime_reason or "Qwen indisponible")
            if args.trace_model:
                original = server.agent_runtime._model_client.complete
                def traced(messages):
                    raw = original(messages)
                    print("MODEL_RAW=" + raw[:1600], flush=True)
                    return raw
                server.agent_runtime._model_client.complete = traced
            workspace_id = server.agent_workspace_id()
            mission = server.start_agent_mission(workspace_id, {
                "goal": "Préparer une amélioration UI SPECIMEN du pseudo",
                "scoped_refs": [{"object_id": node["id"]}],
                "pivots": [{"object_id": node["id"]}],
                "allowed_risk_classes": ["LOCAL_READ_ONLY"],
                "network_profile": "OFFLINE", "max_contacts": 0,
                "max_duration_seconds": 900, "max_tool_calls": 16,
            }, new_id(), True)
            request = tooling.propose("jq", workspace_id=workspace_id,
                mission_id=mission["mission"]["mission_id"], turn_id=new_id(),
                idempotency_key=new_id())
            tooling.approve_provision(request["request_id"], decision_id=new_id(),
                                      actor="human", human_confirmed=True)
            request = tooling.build(request["request_id"])
            image_ref = tooling.store.read_generation(request["generation_id"])["image_ref"]
            scope = server.agent_scope()
            objective = (
                "Dans SPECIMEN, le manifest ne peut pas changer le libellé de "
                "focus-neighborhood intégré. Appelle MAINTENANT le tool "
                "code.change.propose avec exactement arguments "
                "{\"example_id\":\"specimen.username.ui.v1\",\"request_id\":\""
                + request["request_id"] + "\"}. Attends C1 et C2 humains."
            )
            turn = server.agent_runtime.start_turn(scope, {
                "objective": objective, "idempotency_key": new_id()})
            print("turn_id=" + turn["turn_id"], flush=True)
            first = server.agent_runtime.wait_for_state(scope, turn["turn_id"],
                {"CODE_CHANGE_APPROVAL_REQUIRED", "COMPLETED", "FAILED",
                 "MODEL_PROTOCOL_ERROR", "BUDGET_EXHAUSTED"}, timeout=240)
            print("gate_c1_state=" + first["state"], "diagnostic=" +
                  str(first["diagnostic"]), flush=True)
            if first["state"] != "CODE_CHANGE_APPROVAL_REQUIRED":
                for event in server.agent_gateway.events(scope, 0)["events"]:
                    if event["kind"] == "agent.tool.completed" and \
                            event["payload"].get("state") == "FAILED":
                        result = server.agent_gateway.result(scope,
                            event["payload"]["result_id"])
                        print("GATEWAY_FAILURE=" + str(result.get("diagnostic")), flush=True)
                raise RuntimeError("proposition code Qwen absente")
            change_id = first["pending_call"]["result"]["output"]["change_id"]
            print("change_id=" + change_id, flush=True)
            changes.approve_prepare(change_id, actor="human", decision_id=new_id(),
                                    idempotency_key=new_id())
            worktree = changes.worktree_root / change_id
            # WHY: the detached checkout contains only tracked sources. The
            # disposable clone supplies installed Node dependencies; the
            # fixed WEB_FIXTURE_BUILD recipe prepares its C fixture binaries.
            for target in (clone, worktree):
                shutil.copytree(ROOT / "prototypes/web-graph/node_modules",
                                target / "prototypes/web-graph/node_modules",
                                dirs_exist_ok=True)
            agent = server._ensure_developer_agent()
            original_dev_complete = agent.model_client.complete
            def traced_dev_complete(messages):
                raw = original_dev_complete(messages)
                print("DEV_MODEL_RAW=" + raw[:2000], flush=True)
                return raw
            agent.model_client.complete = traced_dev_complete
            dev_tools = []
            original_execute = agent._execute
            def traced_execute(identifier, tool_id, arguments):
                dev_tools.append(tool_id)
                print("dev_tool=" + tool_id, flush=True)
                return original_execute(identifier, tool_id, arguments)
            agent._execute = traced_execute
            result = agent.continue_development(change_id)
            print("developer_state=" + result["state"], "model_calls=" +
                  str(result.get("model_calls")), flush=True)
            if result["state"] != "PAUSED_C2" or not {"dev.repo.read", "dev.repo.edit",
                    "dev.test.run", "dev.diff.get", "dev.change.ready"} <= set(dev_tools):
                raise RuntimeError("édition/test Qwen incomplet")
            review = changes.get(change_id)
            if not review["patch_digest"] or not review["tests"] or \
                    any(not item["passed"] for item in review["tests"]):
                raise RuntimeError("diff et tests C2 incomplets")
            with socket.socket() as probe:
                probe.bind(("127.0.0.1", 0))
                port = probe.getsockname()[1]
            runtime_dir = root / "xdg-runtime"
            runtime_dir.mkdir(mode=0o700)
            environment = {**os.environ, "XDG_RUNTIME_DIR": str(runtime_dir),
                           "XDG_STATE_HOME": str(root / "preview-state"),
                           "XDG_DATA_HOME": str(root / "preview-data"),
                           "XDG_CACHE_HOME": str(root / "preview-cache")}
            preview_process = subprocess.Popen([
                "python3", str(worktree / "prototypes/web-graph/web_app.py"), "serve",
                "--workspace", str(workspace), "--port", str(port),
                "--bridge", str(worktree / "tools/local-jobs"),
                "--automatic-session",
            ], cwd=worktree, env=environment, stdout=subprocess.DEVNULL,
                stderr=subprocess.PIPE)
            try:
                url = f"http://127.0.0.1:{port}"
                deadline = time.monotonic() + 20
                while True:
                    try:
                        with urlopen(url + "/healthz", timeout=1) as response:
                            if response.status == 200:
                                break
                    except OSError:
                        if preview_process.poll() is not None or time.monotonic() > deadline:
                            raise RuntimeError("preview SPECIMEN indisponible")
                        time.sleep(0.1)
                browser = subprocess.run([
                    "node", str(ROOT / "prototypes/web-graph/manual_self_integration_preview.mjs"),
                    url,
                ], cwd=ROOT / "prototypes/web-graph", check=False,
                    capture_output=True, text=True, timeout=120)
                print(browser.stdout[-1000:], flush=True)
                if browser.returncode:
                    raise RuntimeError("Firefox preview échoué: " + browser.stderr[-500:])
                changes.set_preview_metadata(change_id,
                    {"url": url, "notes": "Firefox SPECIMEN: libellé username vérifié"},
                    idempotency_key=new_id())
            finally:
                preview_process.terminate()
                try:
                    preview_process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    preview_process.kill()
                    preview_process.wait(timeout=5)
            difference = changes.get_diff(change_id)
            if "Explorer ce pseudo" not in difference["patch"]:
                raise RuntimeError("diff preview attendu absent")
            server.resume_agent_runtime(turn["turn_id"])
            waiting_c2 = server.agent_runtime.status(scope, turn["turn_id"])
            if waiting_c2["state"] != "CODE_CHANGE_APPLY_REQUIRED":
                raise RuntimeError("pause C2 absente")
            try:
                applied = changes.approve_apply(change_id, actor="human",
                    decision_id=new_id(), idempotency_key=new_id())
            except Exception:
                failed = changes.get(change_id).get("post_apply_tests", [])
                for item in failed:
                    print("post_apply_recipe=" + item["recipe_id"] +
                          " passed=" + str(item["passed"]), flush=True)
                    if not item["passed"]:
                        print("post_apply_failure_output=" + item["output"][-6000:], flush=True)
                raise
            if applied["state"] != "APPLIED_LOCAL":
                raise RuntimeError("application contrôlée absente")
            resumed = server.resume_agent_runtime(turn["turn_id"])
            print("post_c2_runtime_state=" + resumed["state"], flush=True)
            terminal = server.agent_runtime.wait_for_state(scope, turn["turn_id"],
                server.agent_runtime.TERMINAL_STATES,
                timeout=240)
            print("terminal_diagnostic=" + str(terminal["diagnostic"]), flush=True)
            if terminal["state"] != "COMPLETED" or not terminal["final"]:
                raise RuntimeError("final Qwen absent après C2")
            print("applied_state=" + applied["state"], "post_apply_tests=" +
                  str(len(applied["post_apply_tests"])), "terminal=" + terminal["state"],
                  flush=True)
            print("REAL_QWEN_SELF_INTEGRATION_SMOKE=PASS", flush=True)
        finally:
            server.server_close()
            if image_ref:
                subprocess.run(["podman", "image", "rm", image_ref],
                               check=False, capture_output=True, timeout=60)


if __name__ == "__main__":
    main()
