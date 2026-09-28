import json
import subprocess
import sys
import tempfile
import threading
import time
import unittest
import uuid
from http.client import HTTPConnection
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from workspace_server import Handler, WorkspaceServer


class AgentMissionHttpTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix="labfy-mission-http-SPECIMEN-")
        cls.workspace = Path(cls.temporary.name)
        cls.repository = Path(__file__).resolve().parents[3]
        cls.bridge = cls.repository / "tools/local-jobs"
        for arguments in (("init-j7-specimen",), ("export",)):
            subprocess.run(
                [str(cls.bridge), *arguments, "--workspace", str(cls.workspace)],
                cwd=cls.repository, check=True, capture_output=True, timeout=30,
            )
        cls.server = WorkspaceServer(
            ("127.0.0.1", 0), Handler, workspace=cls.workspace, bridge=cls.bridge,
        )
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()
        status, headers, _ = cls.request("GET", "/")
        if status != 303:
            raise RuntimeError("Session automatique indisponible")
        cls.cookie = headers["Set-Cookie"].split(";", 1)[0]
        status, _, body = cls.request("GET", "/api/v1/session", cookie=cls.cookie)
        session = json.loads(body)
        cls.csrf = session["csrf"]
        cls.workspace_id = session["investigation_id"]
        graph = json.loads((cls.workspace / "core-snapshot.json").read_text())
        cls.object_ids = [node["id"] for node in graph["nodes"]]

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()
        cls.thread.join()
        cls.temporary.cleanup()

    @classmethod
    def request(cls, method, path, value=None, *, cookie=None, origin=None, csrf=None):
        connection = HTTPConnection("127.0.0.1", cls.server.server_port, timeout=10)
        headers = {"Host": cls.server.authority}
        if cookie:
            headers["Cookie"] = cookie
        if origin:
            headers["Origin"] = origin
        if csrf:
            headers["X-Labfy-CSRF"] = csrf
        body = None
        if value is not None:
            headers["Content-Type"] = "application/json"
            body = json.dumps(value)
        connection.request(method, path, body=body, headers=headers)
        response = connection.getresponse()
        result = response.status, dict(response.getheaders()), response.read()
        connection.close()
        return result

    def mutate(self, path, value):
        time.sleep(0.04)
        return self.request(
            "POST", path, value, cookie=self.cookie,
            origin=self.server.origin, csrf=self.csrf,
        )

    def specification(self, **changes):
        value = {
            "goal": "Examiner le SPECIMEN sélectionné",
            "scoped_refs": [{"object_id": self.object_ids[0]}],
            "pivots": [{"object_id": self.object_ids[1]}],
            "allowed_risk_classes": ["LOCAL_READ_ONLY"],
            "network_profile": "OFFLINE",
            "max_contacts": 0,
            "max_duration_seconds": 300,
            "max_tool_calls": 8,
        }
        value.update(changes)
        return value

    def start(self, specification=None, **changes):
        value = {
            "workspace_id": self.workspace_id,
            "specification": specification or self.specification(),
            "human_confirmed": True,
            "idempotency_key": str(uuid.uuid4()),
        }
        value.update(changes)
        return self.mutate("/api/v1/agent-mission/start", value)

    def test_routes_require_authentication_origin_and_csrf(self):
        self.assertEqual(self.request("GET", "/api/v1/agent-mission/current")[0], 401)
        request = {
            "workspace_id": self.workspace_id,
            "specification": self.specification(),
            "human_confirmed": True,
            "idempotency_key": str(uuid.uuid4()),
        }
        self.assertEqual(self.request(
            "POST", "/api/v1/agent-mission/start", request,
            cookie=self.cookie, origin="http://evil.test", csrf=self.csrf,
        )[0], 403)

    def test_start_replay_current_rescope_and_cancel(self):
        key = str(uuid.uuid4())
        status, _, body = self.start(idempotency_key=key)
        self.assertEqual(status, 201, body)
        started = json.loads(body)
        self.assertEqual(started["state"], "ACTIVE")
        self.assertFalse(started["replayed"])
        self.assertEqual(started["mission"]["network_profile"], "OFFLINE")
        replay_status, _, replay_body = self.start(idempotency_key=key)
        self.assertEqual(replay_status, 200, replay_body)
        self.assertTrue(json.loads(replay_body)["replayed"])
        mission_id = started["mission"]["mission_id"]
        rescope = {
            "workspace_id": self.workspace_id,
            "mission_id": mission_id,
            "scoped_refs": [{"object_id": self.object_ids[1]}],
            "pivots": [{"object_id": self.object_ids[0]}],
            "human_confirmed": True,
            "idempotency_key": str(uuid.uuid4()),
        }
        self.assertEqual(self.mutate("/api/v1/agent-mission/rescope", rescope)[0], 200)
        cancel = {
            "workspace_id": self.workspace_id,
            "mission_id": mission_id,
            "human_confirmed": True,
            "idempotency_key": str(uuid.uuid4()),
        }
        status, _, body = self.mutate("/api/v1/agent-mission/cancel", cancel)
        self.assertEqual(status, 200, body)
        self.assertEqual(json.loads(body)["state"], "CANCELLED")

    def test_backend_rejects_foreign_scope_active_risk_and_offline_network(self):
        foreign = self.start(workspace_id="workspace-SPECIMEN-foreign")
        self.assertEqual(foreign[0], 403, foreign[2])
        active = self.start(self.specification(
            allowed_risk_classes=["PUBLIC_ACTIVE"],
        ))
        self.assertEqual(active[0], 409, active[2])
        offline_network = self.start(self.specification(
            allowed_risk_classes=["PASSIVE_PUBLIC"],
        ))
        self.assertEqual(offline_network[0], 409, offline_network[2])
        unknown = self.start(self.specification(
            pivots=[{"object_id": "evidence:00000000-0000-4000-8000-000000000000"}],
        ))
        self.assertEqual(unknown[0], 409, unknown[2])


if __name__ == "__main__":
    unittest.main()
