import json
import sys
import tempfile
import threading
import time
import unittest
import uuid
from http.client import HTTPConnection
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from workspace_server import Handler, WorkspaceServer


class FakeLibrary:
    lazy = True

    def __init__(self, workspace):
        self.workspace = workspace
        self.generation = 4
        self.registered = False

    def discover_existing(self):
        return {
            "contract": "labfy.web_library.discovery.v1",
            "generation": self.generation,
            "candidates": [{
                "candidate_id": "opaque-SPECIMEN",
                "display_name": "Existing-SPECIMEN",
                "workspace_id": "81000000-0000-4000-8000-000000000001",
                "title": "Titre <script>SPECIMEN</script>",
                "state": "READY_TO_REGISTER",
                "reason": "Workspace compatible",
            }],
        }

    def register_existing(self, candidate_id, expected_generation, key, confirmed):
        if (candidate_id != "opaque-SPECIMEN" or expected_generation != self.generation
                or confirmed is not True):
            raise ValueError("Enregistrement SPECIMEN refusé")
        uuid.UUID(key)
        self.registered = True
        self.generation += 1
        return {
            "contract": "labfy.web_library.workspace.v1",
            "workspace_id": "81000000-0000-4000-8000-000000000001",
            "title": "Titre <script>SPECIMEN</script>",
            "state": "READY", "generation": self.generation, "replayed": False,
        }

    def open(self, workspace_id, expected_generation):
        if workspace_id not in {"81000000-0000-4000-8000-000000000001",
                                "82000000-0000-4000-8000-000000000002"}:
            raise ValueError("Enquête inconnue")
        if expected_generation != self.generation:
            raise ValueError("Projection de bibliothèque périmée")
        return self.workspace, {"title": "Titre SPECIMEN"}, self.generation


class ExistingWorkspaceLifecycleHttpTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(
            prefix="labfy-existing-lifecycle-SPECIMEN-")
        root = Path(self.temporary.name)
        self.inactive = root / "inactive"
        self.inactive.mkdir()
        self.existing = root / "Existing-SPECIMEN"
        (self.existing / ".labfy/runtime").mkdir(parents=True)
        (self.existing / "Enquete.sqlite").write_bytes(b"SPECIMEN")
        (self.existing / ".labfy/runtime/workspace.json").write_text(json.dumps({
            "contract": "labfy.local_workspace.v1",
            "investigation_id": "81000000-0000-4000-8000-000000000001",
            "title": "Titre SPECIMEN",
        }), encoding="utf-8")
        self.existing_b = root / "Existing-B-SPECIMEN"
        (self.existing_b / ".labfy/runtime").mkdir(parents=True)
        (self.existing_b / "Enquete.sqlite").write_bytes(b"SPECIMEN-B")
        (self.existing_b / ".labfy/runtime/workspace.json").write_text(json.dumps({
            "contract": "labfy.local_workspace.v1",
            "investigation_id": "82000000-0000-4000-8000-000000000002",
            "title": "Titre B SPECIMEN",
        }), encoding="utf-8")
        self.library = FakeLibrary(self.existing)
        self.server = WorkspaceServer(
            ("127.0.0.1", 0), Handler, workspace=self.inactive,
            bridge=root / "unused-bridge", library=self.library,
        )
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        status, headers, _ = self.request("GET", "/")
        self.assertEqual(status, 303)
        self.cookie = headers["Set-Cookie"].split(";", 1)[0]
        status, _, body = self.request("GET", "/api/v1/session", cookie=self.cookie)
        self.assertEqual(status, 200)
        self.csrf = json.loads(body)["csrf"]

    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join()
        self.temporary.cleanup()

    def request(self, method, path, value=None, *, cookie=None, origin=None, csrf=None):
        connection = HTTPConnection("127.0.0.1", self.server.server_port, timeout=10)
        headers = {"Host": self.server.authority}
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
        return self.request("POST", path, value, cookie=self.cookie,
                            origin=self.server.origin, csrf=self.csrf)

    def test_discover_and_register_routes_use_human_mutation_guards(self):
        self.assertEqual(self.request("POST", "/api/v1/library/discover-existing", {})[0],
                         403)
        status, _, body = self.mutate("/api/v1/library/discover-existing", {})
        self.assertEqual(status, 200, body)
        discovery = json.loads(body)
        self.assertEqual(discovery["candidates"][0]["candidate_id"],
                         "opaque-SPECIMEN")
        self.assertNotIn(str(Path(self.temporary.name)), body.decode())

        status, _, body = self.mutate("/api/v1/library/register-existing", {
            "candidate_id": "opaque-SPECIMEN",
            "expected_generation": discovery["generation"],
            "idempotency_key": str(uuid.uuid4()),
            "human_confirmed": True,
        })
        self.assertEqual(status, 201, body)
        self.assertTrue(self.library.registered)

    def test_open_failure_is_atomic_and_close_sets_inactive_state(self):
        with mock.patch.object(self.server, "bridge_call",
                               side_effect=ValueError("export SPECIMEN refusé")):
            with self.assertRaisesRegex(ValueError, "export SPECIMEN refusé"):
                self.server.open_library_workspace(
                    "81000000-0000-4000-8000-000000000001", 4)
        self.assertIsNone(self.server.active_workspace_id)
        self.assertEqual(self.server.workspace, self.inactive.resolve())

        self.server.workspace = self.existing.resolve()
        self.server.active_workspace_id = "81000000-0000-4000-8000-000000000001"
        self.server.library_generation = 4
        self.server.agent_missions[self.server.active_workspace_id] = {
            "state": "ACTIVE", "mission": mock.Mock(), "started_monotonic": 0,
        }
        key = str(uuid.uuid4())
        value = self.server.close_library_workspace(
            self.server.active_workspace_id, 4, key, True)
        self.assertEqual(value["state"], "CLOSED")
        self.assertIsNone(self.server.active_workspace_id)
        self.assertEqual(self.server.workspace, self.inactive.resolve())
        self.assertEqual(
            self.server.agent_missions["81000000-0000-4000-8000-000000000001"]["state"],
            "CANCELLED",
        )
        replay = self.server.close_library_workspace(
            "81000000-0000-4000-8000-000000000001", 4, key, True)
        self.assertTrue(replay["replayed"])

        self.library.workspace = self.existing_b
        with mock.patch.object(self.server, "bridge_call", return_value=""):
            opened = self.server.open_library_workspace(
                "82000000-0000-4000-8000-000000000002", 4)
        self.assertEqual(opened["workspace_id"],
                         "82000000-0000-4000-8000-000000000002")
        self.assertEqual(self.server.active_workspace_id,
                         "82000000-0000-4000-8000-000000000002")
        self.assertEqual(self.server.workspace, self.existing_b.resolve())

    def test_agent_runtime_start_is_rejected_without_active_workspace(self):
        status, _, body = self.mutate("/api/v1/agent-runtime/turns", {
            "objective": "Analyser SPECIMEN", "idempotency_key": "start-SPECIMEN",
        })
        self.assertEqual(status, 409, body)
        self.assertIn("Aucun workspace actif", body.decode())

    def test_agent_search_has_no_unrelated_first_node_fallback(self):
        self.server.workspace = self.existing.resolve()
        self.server.active_workspace_id = "81000000-0000-4000-8000-000000000001"
        graph = {"nodes": [{"id": "entity:SPECIMEN-A", "label": "Alice SPECIMEN"}]}
        with mock.patch.object(self.server, "_agent_graph", return_value=graph):
            absent = self.server._agent_search({"query": "BRAVO-SPECIMEN-ONLY"}, "k1")
            active = self.server._agent_search({"query": "Titre SPECIMEN"}, "k2")
        self.assertEqual(absent, {"matches": [], "object_refs": []})
        self.assertEqual(active["matches"], [{
            "object_kind": "investigation",
            "workspace_id": "81000000-0000-4000-8000-000000000001",
            "label": "Titre SPECIMEN",
        }])
        self.assertEqual(active["object_refs"], [])

    def test_close_route_rejects_none_and_wrong_then_replays_success(self):
        request = {"workspace_id": "81000000-0000-4000-8000-000000000001",
                   "expected_generation": 4,
                   "idempotency_key": str(uuid.uuid4()),
                   "human_confirmed": True}
        self.assertEqual(self.mutate("/api/v1/library/close", request)[0], 409)
        self.server.workspace = self.existing.resolve()
        self.server.active_workspace_id = request["workspace_id"]
        self.server.library_generation = 4
        wrong = {**request, "workspace_id": "82000000-0000-4000-8000-000000000002",
                 "idempotency_key": str(uuid.uuid4())}
        self.assertEqual(self.mutate("/api/v1/library/close", wrong)[0], 403)
        status, _, body = self.mutate("/api/v1/library/close", request)
        self.assertEqual(status, 200, body)
        self.assertFalse(json.loads(body)["replayed"])
        status, _, body = self.mutate("/api/v1/library/close", request)
        self.assertEqual(status, 200, body)
        self.assertTrue(json.loads(body)["replayed"])

    def test_open_b_rejects_all_old_a_mission_and_tooling_references(self):
        workspace_a = "81000000-0000-4000-8000-000000000001"
        workspace_b = "82000000-0000-4000-8000-000000000002"
        mission_id = str(uuid.uuid4())
        proposal_id = str(uuid.uuid4())
        provision_id = str(uuid.uuid4())
        change_id = str(uuid.uuid4())
        mission = mock.Mock()
        mission.snapshot.return_value = {"mission_id": mission_id}
        self.server.workspace = self.existing.resolve()
        self.server.active_workspace_id = workspace_a
        self.server.library_generation = 4
        self.server.agent_missions[workspace_a] = {
            "state": "ACTIVE", "mission": mission, "started_monotonic": 0,
        }
        self.server.agent_proposals[workspace_a] = {
            proposal_id: {"proposal_id": proposal_id, "mission_id": mission_id,
                          "proposal": {}, "decision": None},
        }
        provisioning = mock.Mock()
        provisioning.get.return_value = {
            "request_id": provision_id, "workspace_id": workspace_a,
            "turn_id": str(uuid.uuid4()),
        }
        self.server._tool_provisioning = provisioning
        self.server.tooling_enabled = True
        self.server.code_change_turns[change_id] = {
            "workspace_id": workspace_a, "turn_id": str(uuid.uuid4()),
        }

        self.server.close_library_workspace(
            workspace_a, 4, str(uuid.uuid4()), True)
        self.library.workspace = self.existing_b
        with mock.patch.object(self.server, "bridge_call", return_value=""):
            self.server.open_library_workspace(workspace_b, 4)

        # INVARIANT: changing the active workspace invalidates every durable
        # continuation from A before its underlying service can mutate state.
        status, _, body = self.mutate("/api/v1/agent-mission/rescope", {
            "workspace_id": workspace_a, "mission_id": mission_id,
            "scoped_refs": [], "pivots": [], "human_confirmed": True,
            "idempotency_key": str(uuid.uuid4()),
        })
        self.assertEqual(status, 403, body)
        status, _, body = self.mutate(
            f"/api/v1/agent-proposals/{proposal_id}/approve", {
                "workspace_id": workspace_a, "decision_id": str(uuid.uuid4()),
                "reason": "Décision SPECIMEN", "decided_by": "human",
                "decided_at": "2026-01-01T00:00:00Z",
                "idempotency_key": str(uuid.uuid4()),
            })
        self.assertEqual(status, 403, body)
        decision = {
            "decision_id": str(uuid.uuid4()),
            "idempotency_key": str(uuid.uuid4()),
            "actor": "human", "human_confirmed": True,
            "reason": "Décision SPECIMEN",
        }
        status, _, body = self.mutate(
            f"/api/v1/tool-provisioning/{provision_id}/approve", decision)
        self.assertEqual(status, 403, body)
        status, _, body = self.mutate(
            f"/api/v1/code-changes/{change_id}/approve-prepare",
            {**decision, "decision_id": str(uuid.uuid4()),
             "idempotency_key": str(uuid.uuid4())})
        self.assertEqual(status, 403, body)
        provisioning.approve_provision.assert_not_called()


if __name__ == "__main__":
    unittest.main()
