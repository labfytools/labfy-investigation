"""HTTP gates for the SPECIMEN toolbox, through the real workspace handler."""

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
sys.path.insert(0, str(Path(__file__).resolve().parent))
from tool_provisioning import ToolProvisioningService
from workspace_server import Handler, WorkspaceServer
from test_tool_provisioning import FakeBuilder, FakeExecutor, FakeProvider, integration
from browser_tooling_fixture import BrowserCodeChanges, BrowserDeveloper


def new_id():
    return str(uuid.uuid4())


class WorkspaceToolingHTTPTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix="labfy-SPECIMEN-tooling-http-")
        cls.root = Path(cls.temporary.name)
        cls.workspace = cls.root / "workspace-SPECIMEN"
        cls.workspace.mkdir()
        cls.repository = Path(__file__).resolve().parents[3]
        cls.bridge = cls.repository / "tools/local-jobs"
        subprocess.run([str(cls.bridge), "init-specimen", "--workspace", str(cls.workspace)],
                       cwd=cls.repository, check=True, capture_output=True, timeout=30)
        subprocess.run([str(cls.bridge), "export", "--workspace", str(cls.workspace)],
                       cwd=cls.repository, check=True, capture_output=True, timeout=30)
        cls.service = ToolProvisioningService(
            cls.root / "toolbox", provider=FakeProvider(), builder=FakeBuilder(),
            executor=FakeExecutor(), policy_check=lambda _capability, _context: True)
        cls.changes = BrowserCodeChanges()
        cls.server = WorkspaceServer(("127.0.0.1", 0), Handler,
                                     workspace=cls.workspace, bridge=cls.bridge,
                                     tool_provisioning=cls.service,
                                     code_changes=cls.changes,
                                     developer_agent=BrowserDeveloper(cls.changes))
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()
        status, headers, _body = cls.request("GET", "/")
        if status != 303:
            raise RuntimeError("session SPECIMEN absente")
        cls.cookie = headers["Set-Cookie"].split(";", 1)[0]
        status, _headers, body = cls.request("GET", "/api/v1/session", cookie=cls.cookie)
        cls.csrf = json.loads(body)["csrf"]
        cls.workspace_id = json.loads(body)["investigation_id"]

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()
        cls.thread.join()
        cls.temporary.cleanup()

    @classmethod
    def request(cls, method, path, value=None, *, cookie=None, origin=None, csrf=None,
                host=None):
        connection = HTTPConnection("127.0.0.1", cls.server.server_port, timeout=10)
        headers = {"Host": host or cls.server.authority}
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
        result = (response.status, dict(response.getheaders()), response.read())
        connection.close()
        return result

    def decide(self, path, *, actor="human", csrf=None):
        time.sleep(0.04)
        return self.request("POST", path, {
            "decision_id": new_id(), "idempotency_key": new_id(),
            "actor": actor, "human_confirmed": True, "reason": "",
        }, cookie=self.cookie, origin=self.server.origin,
            csrf=self.csrf if csrf is None else csrf)

    def test_toolbox_routes_require_session_host_origin_and_csrf(self):
        self.assertEqual(self.request("GET", "/api/v1/tool-provisioning")[0], 401)
        self.assertEqual(self.request("GET", "/api/v1/capability-manifests",
                                      cookie=self.cookie, host="evil.test")[0], 403)
        status, _, body = self.request("GET", "/api/v1/tool-provisioning",
                                       cookie=self.cookie)
        self.assertEqual(status, 200, body)
        self.assertEqual(json.loads(body)["requests"], [])
        request = self.service.propose("jq", workspace_id=self.workspace_id,
                                       mission_id=new_id(), turn_id=new_id(),
                                       idempotency_key=new_id())
        path = f"/api/v1/tool-provisioning/{request['request_id']}/approve"
        self.assertEqual(self.request("POST", path, {}, cookie=self.cookie,
                                      origin="http://evil.test", csrf=self.csrf)[0], 403)
        self.assertEqual(self.request("POST", path, {}, cookie=self.cookie,
                                      origin=self.server.origin)[0], 403)
        self.assertEqual(self.decide(path, actor="qwen")[0], 403)
        self.assertEqual(self.decide(path)[0], 202)
        deadline = time.monotonic() + 5
        while self.service.get(request["request_id"])["state"] != "QUARANTINED":
            if time.monotonic() > deadline:
                self.fail("build SPECIMEN non terminé")
            time.sleep(0.02)
        self.service.propose_integration(request["request_id"],
                                         integration(self.service.get(request["request_id"])),
                                         idempotency_key=new_id())
        integration_path = f"/api/v1/tool-integrations/{request['request_id']}/approve-integration"
        self.assertEqual(self.decide(integration_path)[0], 202)
        status, _, body = self.request("GET", "/api/v1/capability-manifests",
                                       cookie=self.cookie)
        self.assertEqual(status, 200, body)
        self.assertEqual(len(json.loads(body)["catalog"]), 1)

    def test_code_gates_require_human_session_and_preserve_diff_text(self):
        path = "/api/v1/code-changes/browser-SPECIMEN/approve-prepare"
        self.assertEqual(self.request("POST", path, {}, cookie=self.cookie,
            origin="http://evil.test", csrf=self.csrf)[0], 403)
        self.assertEqual(self.request("POST", path, {}, cookie=self.cookie,
            origin=self.server.origin)[0], 403)
        self.assertEqual(self.decide(path, actor="qwen")[0], 403)
        self.assertEqual(self.decide(path)[0], 202)
        deadline = time.monotonic() + 5
        while self.changes.record["state"] != "WAITING_APPLY_APPROVAL":
            if time.monotonic() > deadline:
                self.fail("préparation SPECIMEN non terminée")
            time.sleep(0.02)
        status, _, body = self.request("GET",
            "/api/v1/code-changes/browser-SPECIMEN/diff", cookie=self.cookie)
        self.assertEqual(status, 200)
        self.assertIn("<img", json.loads(body)["patch"])
        self.assertEqual(self.decide(
            "/api/v1/code-changes/browser-SPECIMEN/approve-apply")[0], 202)
        deadline = time.monotonic() + 5
        while self.changes.record["state"] != "APPLIED_LOCAL":
            if time.monotonic() > deadline:
                self.fail("application SPECIMEN non terminée")
            time.sleep(0.02)

    def test_model_code_change_summary_excludes_captured_logs(self):
        record = {**self.changes.record, "base_sha": "a" * 40,
                  "patch_digest": "b" * 64,
                  "tests": [{"recipe_id": "FIREFOX_FULL", "passed": True,
                             "returncode": 0, "output": "SECRET-SPECIMEN-LOG"}],
                  "post_apply_tests": []}
        summary = self.server._code_change_model_summary(record)
        self.assertEqual(summary["tests"][0]["recipe_id"], "FIREFOX_FULL")
        self.assertNotIn("SECRET-SPECIMEN-LOG", json.dumps(summary))


if __name__ == "__main__":
    unittest.main()
