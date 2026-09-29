#!/usr/bin/env python3
"""Disposable SPECIMEN backend for browser gate tests; no Podman/model process."""

import json
import signal
import subprocess
import sys
import tempfile
import threading
import time
import uuid
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "prototypes/web-graph"))
sys.path.insert(0, str(Path(__file__).resolve().parent))

from test_tool_provisioning import FakeBuilder, FakeExecutor, FakeProvider
from tool_integration_examples import example_for
from tool_provisioning import ToolProvisioningService
from workspace_server import Handler, WorkspaceServer


def new_id():
    return str(uuid.uuid4())


class BrowserToolbox(ToolProvisioningService):
    def build(self, request_id):
        record = super().build(request_id)
        self.propose_integration(request_id, example_for(record), idempotency_key=new_id())
        return self.get(request_id)


class BrowserCodeChanges:
    def __init__(self):
        self.record = {
            "change_id": "browser-SPECIMEN", "state": "WAITING_DEV_APPROVAL",
            "purpose": "Explorer ce pseudo SPECIMEN",
            "why_declarative_insufficient": "Le libellé de l'action intégrée exige du code.",
            "affected_areas": ["Web"], "expected_files": ["public/app.js"],
            "required_tests": ["NODE_TEST", "FIREFOX_FULL"], "risk": "LOW",
            "preview": {},
        }

    def list_changes(self):
        return [dict(self.record)]

    def get(self, _change_id):
        return dict(self.record)

    def get_diff(self, _change_id):
        return {"stat": "public/app.js | 1 +", "patch":
                "+ Explorer ce pseudo <img src=x onerror='window.__injected=true'> SPECIMEN"}

    def approve_prepare(self, _change_id, **_kwargs):
        self.record["state"] = "EDITING"
        return dict(self.record)

    def reject_prepare(self, _change_id, **_kwargs):
        self.record["state"] = "DEV_REJECTED"
        return dict(self.record)

    def approve_apply(self, _change_id, **_kwargs):
        self.record["state"] = "APPLIED_LOCAL"
        return dict(self.record)

    def reject_apply(self, _change_id, **_kwargs):
        self.record["state"] = "APPLY_REJECTED"
        return dict(self.record)


class BrowserDeveloper:
    def __init__(self, changes):
        self.changes = changes

    def continue_development(self, _change_id):
        self.changes.record["state"] = "WAITING_APPLY_APPROVAL"
        return {"state": "PAUSED_C2"}


def main():
    with tempfile.TemporaryDirectory(prefix="labfy-browser-tooling-SPECIMEN-") as name:
        root = Path(name)
        workspace = root / "workspace-SPECIMEN"
        workspace.mkdir()
        bridge = ROOT / "tools/local-jobs"
        for command in ("init-specimen", "export"):
            subprocess.run([str(bridge), command, "--workspace", str(workspace)],
                           cwd=ROOT, check=True, capture_output=True, timeout=30)
        graph_path = workspace / "core-snapshot.json"
        graph = json.loads(graph_path.read_text(encoding="utf-8"))
        object_id = new_id()
        node_id = "entity:" + object_id
        graph["nodes"].append({"id": node_id, "object_id": object_id,
                               "object_kind": "username", "label": "SPECIMEN_user",
                               "type": "username", "group": "entity", "state": "proposed",
                               "raw": {}})
        graph_path.write_text(json.dumps(graph), encoding="utf-8")
        tooling = BrowserToolbox(root / "toolbox", provider=FakeProvider(),
                                 builder=FakeBuilder(), executor=FakeExecutor())
        changes = BrowserCodeChanges()
        server = WorkspaceServer(("127.0.0.1", 0), Handler, workspace=workspace,
            bridge=bridge, tool_provisioning=tooling, code_changes=changes,
            developer_agent=BrowserDeveloper(changes))
        workspace_id = server.agent_workspace_id()
        mission = server.start_agent_mission(workspace_id, {
            "goal": "Analyser SPECIMEN", "scoped_refs": [{"object_id": node_id}],
            "pivots": [{"object_id": node_id}],
            "allowed_risk_classes": ["LOCAL_READ_ONLY"], "network_profile": "OFFLINE",
            "max_contacts": 0, "max_duration_seconds": 900, "max_tool_calls": 16,
        }, new_id(), True)
        tooling.propose("jq", workspace_id=workspace_id,
            mission_id=mission["mission"]["mission_id"], turn_id=new_id(),
            idempotency_key=new_id())
        changes.record["preview"] = {"url": f"http://127.0.0.1:{server.server_port}/"}
        print(f"http://127.0.0.1:{server.server_port}/", flush=True)
        stop = threading.Event()
        signal.signal(signal.SIGINT, lambda *_: stop.set())
        signal.signal(signal.SIGTERM, lambda *_: stop.set())
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            while not stop.wait(0.1):
                pass
        finally:
            server.shutdown()
            server.server_close()
            thread.join()


if __name__ == "__main__":
    main()
