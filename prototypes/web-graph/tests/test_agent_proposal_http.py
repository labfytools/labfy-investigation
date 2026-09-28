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


class AgentProposalHttpTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix="labfy-proposal-http-SPECIMEN-")
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
        _, _, body = cls.request("GET", "/api/v1/session", cookie=cls.cookie)
        session = json.loads(body)
        cls.csrf = session["csrf"]
        cls.workspace_id = session["investigation_id"]
        graph = json.loads((cls.workspace / "core-snapshot.json").read_text())
        cls.object_ids = [node["id"] for node in graph["nodes"]]
        request = {
            "workspace_id": cls.workspace_id,
            "specification": {
                "goal": "Mission SPECIMEN pour propositions",
                "scoped_refs": [{"object_id": cls.object_ids[0]}],
                "pivots": [{"object_id": cls.object_ids[1]}],
                "allowed_risk_classes": ["LOCAL_READ_ONLY"],
                "network_profile": "OFFLINE", "max_contacts": 0,
                "max_duration_seconds": 300, "max_tool_calls": 8,
            },
            "human_confirmed": True,
            "idempotency_key": str(uuid.uuid4()),
        }
        status, _, body = cls.mutate_class("/api/v1/agent-mission/start", request)
        if status != 201:
            raise RuntimeError(body)
        cls.mission_id = json.loads(body)["mission"]["mission_id"]

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

    @classmethod
    def mutate_class(cls, path, value):
        time.sleep(0.04)
        return cls.request(
            "POST", path, value, cookie=cls.cookie,
            origin=cls.server.origin, csrf=cls.csrf,
        )

    def proposal(self, **changes):
        value = {
            "title": "Vérifier la piste SPECIMEN",
            "reason": '<img src=x onerror="window.__proposalInjected=true"> texte hostile',
            "object_refs": [{"object_id": self.object_ids[0]}],
            "suggested_capability": "investigation.search",
            "risk_class": "LOCAL_READ_ONLY",
            "expected_value": "Clarifier une correspondance synthétique",
        }
        value.update(changes)
        return value

    def create(self, **changes):
        request = {
            "workspace_id": self.workspace_id,
            "mission_id": self.mission_id,
            "proposal": self.proposal(),
            "idempotency_key": str(uuid.uuid4()),
        }
        request.update(changes)
        return self.mutate_class("/api/v1/agent-proposals", request)

    def test_create_list_replay_and_foreign_reference_rejection(self):
        key = str(uuid.uuid4())
        status, _, body = self.create(idempotency_key=key)
        self.assertEqual(status, 201, body)
        record = json.loads(body)
        self.assertIn("<img", record["proposal"]["reason"])
        status, _, replay_body = self.create(idempotency_key=key)
        self.assertEqual(status, 200, replay_body)
        self.assertEqual(json.loads(replay_body)["proposal_id"], record["proposal_id"])
        status, _, list_body = self.request(
            "GET", "/api/v1/agent-proposals", cookie=self.cookie,
        )
        self.assertEqual(status, 200, list_body)
        self.assertTrue(json.loads(list_body)["proposals"])
        foreign = self.create(proposal=self.proposal(
            object_refs=[{"object_id": "entity:00000000-0000-4000-8000-000000000000"}],
        ))
        self.assertEqual(foreign[0], 409, foreign[2])

    def test_approval_is_a_decision_only_and_cross_workspace_is_forbidden(self):
        status, _, body = self.create()
        self.assertEqual(status, 201, body)
        proposal_id = json.loads(body)["proposal_id"]
        decision = {
            "workspace_id": self.workspace_id,
            "decision_id": str(uuid.uuid4()),
            "reason": "Retenue pour examen humain SPECIMEN",
            "decided_by": "opérateur local",
            "decided_at": "2026-09-28T12:00:00Z",
            "idempotency_key": str(uuid.uuid4()),
        }
        status, _, body = self.mutate_class(
            f"/api/v1/agent-proposals/{proposal_id}/approve", decision,
        )
        self.assertEqual(status, 200, body)
        trace = json.loads(body)["decision"]
        self.assertEqual(trace["decision"], "APPROVED")
        self.assertFalse(trace["policy_grant_created"])
        self.assertEqual(trace["effect"], "PROPOSAL_DECISION_ONLY")
        foreign = {**decision, "workspace_id": "workspace-SPECIMEN-foreign",
                   "decision_id": str(uuid.uuid4()),
                   "idempotency_key": str(uuid.uuid4())}
        status, _, _ = self.mutate_class(
            f"/api/v1/agent-proposals/{proposal_id}/reject", foreign,
        )
        self.assertEqual(status, 403)


if __name__ == "__main__":
    unittest.main()
