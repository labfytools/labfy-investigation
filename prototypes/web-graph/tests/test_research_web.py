import json
import os
import sqlite3
import subprocess
import tempfile
import threading
import time
import unittest
from http.client import HTTPConnection
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from workspace_server import Handler, WorkspaceServer


class Provider(BaseHTTPRequestHandler):
    requests = []
    bodies = {}
    protocol_version = "HTTP/1.1"

    def do_GET(self):
        type(self).requests.append((self.path, self.headers.get("X-Labfy-Action")))
        body = type(self).bodies.get(self.path)
        if body is None:
            body = json.dumps({"contract": "labfy.fixture.provider.v1",
                               "path": self.path}).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *_args):
        pass


class ResearchWebTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.repository = Path(__file__).resolve().parents[3]
        cls.bridge = cls.repository / "tools/local-jobs"
        cls.temporary = tempfile.TemporaryDirectory(prefix="labfy-research-web-")
        cls.provider = ThreadingHTTPServer(("127.0.0.1", 0), Provider)
        cls.provider_thread = threading.Thread(
            target=cls.provider.serve_forever, daemon=True)
        cls.provider_thread.start()
        cls.instances = []

    @classmethod
    def tearDownClass(cls):
        for server, thread in cls.instances:
            server.shutdown(); server.server_close(); thread.join()
        cls.provider.shutdown(); cls.provider.server_close()
        cls.provider_thread.join(); cls.temporary.cleanup()

    @classmethod
    def start_workspace(cls, name):
        workspace = Path(cls.temporary.name) / name
        subprocess.run([str(cls.bridge), "init-j7-specimen", "--workspace",
                        str(workspace)], cwd=cls.repository, check=True,
                       capture_output=True, timeout=30)
        subprocess.run([str(cls.bridge), "export", "--workspace", str(workspace)],
                       cwd=cls.repository, check=True, timeout=15)
        server = WorkspaceServer(("127.0.0.1", 0), Handler,
            workspace=workspace, bridge=cls.bridge,
            research_fixture_authority=f"127.0.0.1:{cls.provider.server_port}")
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start(); cls.instances.append((server, thread))
        return workspace, server

    @staticmethod
    def request(server, method, path, value=None, *, cookie=None, csrf=None,
                origin=None, host=None):
        connection = HTTPConnection("127.0.0.1", server.server_port, timeout=10)
        headers = {"Host": host or server.authority}
        if cookie: headers["Cookie"] = cookie
        if csrf: headers["X-Labfy-CSRF"] = csrf
        if origin: headers["Origin"] = origin
        body = None
        if value is not None:
            headers["Content-Type"] = "application/json"
            body = json.dumps(value)
        connection.request(method, path, body=body, headers=headers)
        response = connection.getresponse(); data = response.read()
        result = response.status, dict(response.getheaders()), data
        connection.close(); return result

    def open_automatic_session(self, server):
        status, headers, _ = self.request(server, "GET", "/")
        self.assertEqual(status, 303)
        cookie = headers["Set-Cookie"].split(";", 1)[0]
        status, _, body = self.request(server, "GET", "/api/v1/session",
                                       cookie=cookie)
        self.assertEqual(status, 200)
        return cookie, json.loads(body)["csrf"]

    def mutate(self, server, cookie, csrf, path, value):
        time.sleep(0.04)
        return self.request(server, "POST", path, value, cookie=cookie,
                            csrf=csrf, origin=server.origin)

    def test_two_waves_refusal_never_contacts_and_workspace_b_isolated(self):
        Provider.requests.clear()
        workspace_a, server_a = self.start_workspace("A")
        cookie_a, csrf_a = self.open_automatic_session(server_a)
        graph = json.loads((workspace_a / "core-snapshot.json").read_text())
        seed = graph["nodes"][0]["id"]
        prepared = {"selection_ids": [seed],
            "question": "Que confirme cette source SPECIMEN ?",
            "exclusions": [],
            "idempotency_key": "95000000-0000-4000-8000-000000000001"}
        status, _, body = self.mutate(server_a, cookie_a, csrf_a,
                                      "/api/v1/research/prepare", prepared)
        self.assertEqual(status, 200, body); snapshot = json.loads(body)
        first, second, refused = snapshot["actions"]

        def campaign(action, suffix):
            decisions = [{"action_id": item["action_id"],
                          "decision": ("REFUSE" if item == refused else
                                       "AUTHORIZE" if item == first or item == action
                                       else "DEFER")}
                         for item in snapshot["actions"]]
            status, _, grant_body = self.mutate(server_a, cookie_a, csrf_a,
                "/api/v1/research/grants", {"plan_id": snapshot["plan_id"],
                "input_revision": snapshot["input_revision"],
                "selected_action_ids": [action["action_id"]], "exclusions": [],
                "decisions": decisions,
                "idempotency_key": f"95000000-0000-4000-8000-0000000000{suffix}"})
            self.assertEqual(status, 200, grant_body); grant = json.loads(grant_body)
            grant_id = grant["grants"][0]["grant_id"]
            status, _, result_body = self.mutate(server_a, cookie_a, csrf_a,
                "/api/v1/research/campaigns", {"grant_id": grant_id,
                "input_revision": snapshot["input_revision"],
                "action_ids": [action["action_id"]],
                "idempotency_key": f"96000000-0000-4000-8000-0000000000{suffix}"})
            self.assertEqual(status, 200, result_body)
            return json.loads(result_body)

        premature = [{"action_id": item["action_id"],
                      "decision": "AUTHORIZE" if item == second else "DEFER"}
                     for item in snapshot["actions"]]
        status, _, body = self.mutate(server_a, cookie_a, csrf_a,
            "/api/v1/research/grants", {"plan_id": snapshot["plan_id"],
            "input_revision": snapshot["input_revision"],
            "selected_action_ids": [second["action_id"]], "exclusions": [],
            "decisions": premature,
            "idempotency_key": "95000000-0000-4000-8000-000000000009"})
        self.assertEqual(status, 409, body)

        previous_proxy = os.environ.get("http_proxy")
        os.environ["http_proxy"] = "http://127.0.0.1:1"
        try:
            after_first = campaign(first, "11")
        finally:
            if previous_proxy is None: os.environ.pop("http_proxy", None)
            else: os.environ["http_proxy"] = previous_proxy
        self.assertEqual([item[0] for item in Provider.requests],
                         ["/specimen/wave-1"])
        second_view = next(item for item in after_first["actions"]
                           if item["action_id"] == second["action_id"])
        self.assertFalse(second_view["contacted"])
        self.assertEqual(next(item for item in after_first["actions"]
            if item["action_id"] == first["action_id"])["result_status"], "NEW")

        retry_refused = [{"action_id": refused["action_id"],
                          "decision": "AUTHORIZE"}]
        status, _, body = self.mutate(server_a, cookie_a, csrf_a,
            "/api/v1/research/grants", {"plan_id": snapshot["plan_id"],
            "input_revision": snapshot["input_revision"],
            "selected_action_ids": [refused["action_id"]], "exclusions": [],
            "decisions": retry_refused,
            "idempotency_key": "95000000-0000-4000-8000-000000000019"})
        self.assertEqual(status, 409, body)

        after_second = campaign(second, "12")
        self.assertEqual([item[0] for item in Provider.requests],
                         ["/specimen/wave-1", "/specimen/wave-2"])
        self.assertEqual(next(item for item in after_second["actions"]
            if item["action_id"] == second["action_id"])["result_status"],
            "CONTRADICTION")
        self.assertFalse(next(item for item in after_second["actions"]
            if item["action_id"] == refused["action_id"])["contacted"])
        with sqlite3.connect(workspace_a / ".labfy/runtime/jobs.sqlite") as connection:
            self.assertEqual(connection.execute(
                "SELECT count(*) FROM research_results").fetchone()[0], 2)
            self.assertEqual(connection.execute(
                "SELECT count(*) FROM research_receipts").fetchone()[0], 2)
        self.assertEqual(len(list((workspace_a / ".labfy/research").glob("*.txt"))), 2)

        _, server_b = self.start_workspace("B")
        cookie_b, _ = self.open_automatic_session(server_b)
        status, _, body = self.request(server_b, "GET", "/api/v1/research",
                                       cookie=cookie_b)
        self.assertEqual(status, 200); self.assertEqual(json.loads(body)["state"], "EMPTY")
        self.assertNotEqual(server_a.context()["investigation_id"],
                            server_b.context()["investigation_id"])
        self.assertNotIn("/specimen/refused", [item[0] for item in Provider.requests])

    def test_result_failure_compensates_final_artifact(self):
        Provider.requests.clear()
        workspace, server = self.start_workspace("artifact-failure")
        cookie, csrf = self.open_automatic_session(server)
        graph = json.loads((workspace / "core-snapshot.json").read_text())
        prepared = {"selection_ids": [graph["nodes"][0]["id"]],
            "question": "Faute SQLite SPECIMEN ?", "exclusions": [],
            "idempotency_key": "98000000-0000-4000-8000-000000000001"}
        status, _, body = self.mutate(server, cookie, csrf,
            "/api/v1/research/prepare", prepared)
        self.assertEqual(status, 200, body); snapshot = json.loads(body)
        first = snapshot["actions"][0]
        decisions = [{"action_id": item["action_id"],
                      "decision": "AUTHORIZE" if item == first else "DEFER"}
                     for item in snapshot["actions"]]
        status, _, body = self.mutate(server, cookie, csrf,
            "/api/v1/research/grants", {"plan_id": snapshot["plan_id"],
            "input_revision": snapshot["input_revision"],
            "selected_action_ids": [first["action_id"]], "decisions": decisions,
            "exclusions": [],
            "idempotency_key": "98000000-0000-4000-8000-000000000002"})
        self.assertEqual(status, 200, body); grant = json.loads(body)
        database = workspace / ".labfy/runtime/jobs.sqlite"
        with sqlite3.connect(database) as connection:
            connection.execute("CREATE TRIGGER specimen_fail_result BEFORE INSERT ON research_results BEGIN SELECT RAISE(FAIL, 'SPECIMEN'); END")
        status, _, body = self.mutate(server, cookie, csrf,
            "/api/v1/research/campaigns", {"grant_id": grant["grants"][0]["grant_id"],
            "input_revision": snapshot["input_revision"],
            "action_ids": [first["action_id"]],
            "idempotency_key": "98000000-0000-4000-8000-000000000003"})
        self.assertEqual(status, 409, body)
        artifact_dir = workspace / ".labfy/research"
        self.assertEqual(list(artifact_dir.glob("*.txt")), [])
        with sqlite3.connect(database) as connection:
            self.assertEqual(connection.execute(
                "SELECT count(*) FROM research_results").fetchone()[0], 0)
            self.assertEqual(connection.execute(
                "SELECT count(*) FROM research_receipts").fetchone()[0], 0)

    def test_response_over_action_limit_publishes_nothing(self):
        Provider.requests.clear()
        Provider.bodies["/specimen/wave-1"] = b"x" * 4097
        try:
            workspace, server = self.start_workspace("response-limit")
            cookie, csrf = self.open_automatic_session(server)
            graph = json.loads((workspace / "core-snapshot.json").read_text())
            prepared = {"selection_ids": [graph["nodes"][0]["id"]],
                "question": "Borne corps SPECIMEN ?", "exclusions": [],
                "idempotency_key": "98100000-0000-4000-8000-000000000001"}
            status, _, body = self.mutate(server, cookie, csrf,
                "/api/v1/research/prepare", prepared)
            self.assertEqual(status, 200, body); snapshot = json.loads(body)
            first = snapshot["actions"][0]
            decisions = [{"action_id": item["action_id"],
                          "decision": "AUTHORIZE" if item == first else "DEFER"}
                         for item in snapshot["actions"]]
            status, _, body = self.mutate(server, cookie, csrf,
                "/api/v1/research/grants", {"plan_id": snapshot["plan_id"],
                "input_revision": snapshot["input_revision"],
                "selected_action_ids": [first["action_id"]],
                "decisions": decisions, "exclusions": [],
                "idempotency_key": "98100000-0000-4000-8000-000000000002"})
            self.assertEqual(status, 200, body); grant = json.loads(body)
            status, _, body = self.mutate(server, cookie, csrf,
                "/api/v1/research/campaigns", {
                "grant_id": grant["grants"][0]["grant_id"],
                "input_revision": snapshot["input_revision"],
                "action_ids": [first["action_id"]],
                "idempotency_key": "98100000-0000-4000-8000-000000000003"})
            self.assertEqual(status, 409, body)
            self.assertEqual(list((workspace / ".labfy/research").glob("*.txt")), [])
            with sqlite3.connect(workspace / ".labfy/runtime/jobs.sqlite") as connection:
                self.assertEqual(connection.execute(
                    "SELECT count(*) FROM research_results").fetchone()[0], 0)
                self.assertEqual(connection.execute(
                    "SELECT count(*) FROM research_receipts").fetchone()[0], 0)
        finally:
            Provider.bodies.pop("/specimen/wave-1", None)

    def test_crash_after_rename_reconciles_without_recontact(self):
        Provider.requests.clear()
        workspace, server = self.start_workspace("crash-reconcile")
        cookie, csrf = self.open_automatic_session(server)
        graph = json.loads((workspace / "core-snapshot.json").read_text())
        prepared = {"selection_ids": [graph["nodes"][0]["id"]],
            "question": "Reprise publication SPECIMEN ?", "exclusions": [],
            "idempotency_key": "98200000-0000-4000-8000-000000000001"}
        status, _, body = self.mutate(server, cookie, csrf,
            "/api/v1/research/prepare", prepared)
        self.assertEqual(status, 200, body); snapshot = json.loads(body)
        first = snapshot["actions"][0]
        decisions = [{"action_id": item["action_id"],
                      "decision": "AUTHORIZE" if item == first else "DEFER"}
                     for item in snapshot["actions"]]
        status, _, body = self.mutate(server, cookie, csrf,
            "/api/v1/research/grants", {"plan_id": snapshot["plan_id"],
            "input_revision": snapshot["input_revision"],
            "selected_action_ids": [first["action_id"]],
            "decisions": decisions, "exclusions": [],
            "idempotency_key": "98200000-0000-4000-8000-000000000002"})
        self.assertEqual(status, 200, body); grant = json.loads(body)
        campaign = {"grant_id": grant["grants"][0]["grant_id"],
            "input_revision": snapshot["input_revision"],
            "action_ids": [first["action_id"]],
            "idempotency_key": "98200000-0000-4000-8000-000000000003"}
        os.environ["LABFY_TEST_RESEARCH_CRASH_AFTER_RENAME"] = "1"
        try:
            status, _, _ = self.mutate(server, cookie, csrf,
                "/api/v1/research/campaigns", campaign)
        finally:
            os.environ.pop("LABFY_TEST_RESEARCH_CRASH_AFTER_RENAME", None)
        self.assertEqual(status, 409)
        self.assertEqual(len(Provider.requests), 1)
        artifact_dir = workspace / ".labfy/research"
        self.assertEqual(len(list(artifact_dir.glob("*.txt"))), 1)
        self.assertEqual(len(list(artifact_dir.glob(".publishing-*"))), 1)
        status, _, body = self.mutate(server, cookie, csrf,
            "/api/v1/research/campaigns", campaign)
        self.assertEqual(status, 200, body)
        self.assertEqual(len(Provider.requests), 1)
        self.assertEqual(list(artifact_dir.glob(".publishing-*")), [])
        with sqlite3.connect(workspace / ".labfy/runtime/jobs.sqlite") as connection:
            self.assertEqual(connection.execute(
                "SELECT count(*) FROM research_results").fetchone()[0], 1)
            self.assertEqual(connection.execute(
                "SELECT count(*) FROM research_receipts").fetchone()[0], 1)

    def test_research_routes_keep_host_origin_csrf_and_shape_guards(self):
        _, server = self.start_workspace("guards")
        cookie, csrf = self.open_automatic_session(server)
        self.assertEqual(self.request(server, "GET", "/api/v1/research",
                                     cookie=cookie, host="evil.test")[0], 403)
        value = {"selection_ids": ["x"], "question": "SPECIMEN",
                 "exclusions": [],
                 "idempotency_key": "97000000-0000-4000-8000-000000000001"}
        self.assertEqual(self.request(server, "POST", "/api/v1/research/prepare",
            value, cookie=cookie, csrf=csrf, origin="http://evil.test")[0], 403)
        malformed = dict(value, endpoint="http://127.0.0.1/")
        self.assertEqual(self.mutate(server, cookie, csrf,
            "/api/v1/research/prepare", malformed)[0], 400)


if __name__ == "__main__":
    unittest.main()
