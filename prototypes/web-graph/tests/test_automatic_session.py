"""Régressions de session sans code visible du lanceur Web-only."""

import json
import sys
import tempfile
import threading
import unittest
from http.client import HTTPConnection
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from workspace_server import Handler, WorkspaceServer


class AutomaticSessionTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="labfy-auto-SPECIMEN-")
        root = Path(self.temporary.name)
        self.server = WorkspaceServer(("127.0.0.1", 0), Handler,
            workspace=root, bridge=root / "missing-bridge", bootstrap="unused",
            automatic_session=True)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.authority = f"127.0.0.1:{self.server.server_port}"

    def tearDown(self):
        self.server.shutdown(); self.server.server_close(); self.thread.join()
        self.temporary.cleanup()

    def request(self, method, path, headers=None, body=None):
        connection = HTTPConnection("127.0.0.1", self.server.server_port, timeout=3)
        connection.request(method, path, body=body, headers=headers or {"Host": self.authority})
        response = connection.getresponse(); value = response.read()
        answer = response.status, dict(response.getheaders()), value
        connection.close(); return answer

    def test_root_establishes_http_only_session_without_code_or_url_token(self):
        status, headers, _ = self.request("GET", "/")
        self.assertEqual(status, 303)
        self.assertEqual(headers["Location"], "/")
        self.assertIn("HttpOnly", headers["Set-Cookie"])
        self.assertIn("SameSite=Strict", headers["Set-Cookie"])
        self.assertNotIn("unused", headers["Set-Cookie"])
        cookie = headers["Set-Cookie"].split(";", 1)[0]
        status, _, body = self.request("GET", "/api/v1/session", {"Host": self.authority, "Cookie": cookie})
        self.assertEqual(status, 200)
        self.assertIn("csrf", json.loads(body))

    def test_third_party_origin_cannot_mutate_auto_session(self):
        _, headers, _ = self.request("GET", "/")
        cookie = headers["Set-Cookie"].split(";", 1)[0]
        status, _, _ = self.request("POST", "/api/v1/queue/pause", {
            "Host": self.authority, "Cookie": cookie, "Origin": "https://evil.test",
            "Content-Type": "application/json", "Content-Length": "2"}, "{}")
        self.assertEqual(status, 403)


if __name__ == "__main__":
    unittest.main()
