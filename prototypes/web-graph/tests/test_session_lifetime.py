"""Session locale : connexion tardive, expiration et nouvelle authentification."""

import http.client
import json
import sys
import tempfile
import threading
import time
import unittest
from pathlib import Path
from unittest import mock

WEB = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(WEB))
import workspace_server  # noqa: E402


class SessionLifetimeTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="labfy-session-SPECIMEN-")
        self.now = 1000.0
        clock = mock.Mock(wraps=time)
        clock.monotonic.side_effect = lambda: self.now
        self.clock_patch = mock.patch.object(workspace_server, "time", clock)
        self.clock_patch.start()
        self.addCleanup(self.clock_patch.stop)
        self.addCleanup(self.temporary.cleanup)
        self.server = workspace_server.WorkspaceServer(
            ("127.0.0.1", 0), workspace_server.Handler,
            workspace=Path(self.temporary.name), bridge=WEB / "unused-SPECIMEN",
            bootstrap="code-SPECIMEN-only")
        self.server.bridge_call = mock.Mock(
            side_effect=AssertionError("L'authentification ne doit pas ouvrir d'enquête"))
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()

    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(timeout=3)
        self.assertFalse(self.thread.is_alive())
        self.server.bridge_call.assert_not_called()

    def request(self, method, path, *, cookie=None, body=None, origin=None):
        connection = http.client.HTTPConnection(
            "127.0.0.1", self.server.server_port, timeout=3)
        headers = {"Host": self.server.authority}
        if cookie:
            headers["Cookie"] = cookie
        if body is not None:
            headers.update({"Content-Type": "application/json",
                            "Origin": origin or self.server.origin})
        try:
            connection.request(method, path, headers=headers,
                               body=json.dumps(body) if body is not None else None)
            response = connection.getresponse()
            return response.status, dict(response.getheaders()), response.read()
        finally:
            connection.close()

    def login(self):
        status, headers, body = self.request("POST", "/api/v1/session",
            body={"bootstrap_code": "code-SPECIMEN-only"})
        self.assertEqual(status, 200)
        self.assertTrue(json.loads(body)["authenticated"])
        self.assertIn("HttpOnly", headers["Set-Cookie"])
        self.assertIn("SameSite=Strict", headers["Set-Cookie"])
        self.assertIn("Max-Age=3600", headers["Set-Cookie"])
        return headers["Set-Cookie"].split(";", 1)[0]

    def test_production_ttl_remains_one_hour(self):
        # INVARIANT: le TTL court du harness navigateur est opt-in ; les
        # constructions normales et leur cookie restent fixés à une heure.
        self.assertEqual(workspace_server.SESSION_TTL_SECONDS, 3600)
        self.assertEqual(self.server.session_ttl_seconds, 3600)
        self.login()
        self.assertEqual(self.server.session_deadline, self.now + 3600)

    def test_first_login_after_two_hours_has_its_own_lifetime(self):
        self.now += 7200
        cookie = self.login()
        self.assertEqual(self.request("GET", "/api/v1/session", cookie=cookie)[0], 200)
        deadline = self.server.session_deadline
        self.assertEqual(deadline, self.now + 3600)
        self.now += 3599
        self.assertEqual(self.request("GET", "/api/v1/session", cookie=cookie)[0], 200)
        self.assertEqual(self.server.session_deadline, deadline)
        self.now += 1
        self.assertEqual(self.request("GET", "/api/v1/session", cookie=cookie)[0], 401)

    def test_reauthentication_renews_session_without_reviving_old_cookie(self):
        old_cookie = self.login()
        old_csrf = self.server.csrf
        self.now += 3601
        self.assertEqual(self.request("GET", "/api/v1/session", cookie=old_cookie)[0], 401)
        cookie = self.login()
        self.assertNotEqual(cookie, old_cookie)
        self.assertNotEqual(self.server.csrf, old_csrf)
        self.assertEqual(self.request("GET", "/api/v1/session", cookie=old_cookie)[0], 401)
        status, _, body = self.request("GET", "/api/v1/session", cookie=cookie)
        self.assertEqual(status, 200)
        self.assertEqual(json.loads(body)["csrf"], self.server.csrf)
        status, _, html = self.request("GET", "/", cookie=cookie)
        self.assertEqual(status, 200)
        self.assertNotIn(b'id="login-form"', html)
        self.assertEqual(self.server.session_deadline, self.now + 3600)

    def test_invalid_login_never_extends_or_replaces_a_session(self):
        cookie = self.login()
        before = (self.server.session, self.server.csrf, self.server.session_deadline)
        for expired in (False, True):
            if expired:
                self.now += 3601
            for code, origin in (("wrong-SPECIMEN", self.server.origin),
                                 ("code-SPECIMEN-only", "http://foreign.test")):
                status, _, _ = self.request("POST", "/api/v1/session",
                    body={"bootstrap_code": code}, origin=origin)
                self.assertEqual(status, 403)
                self.assertEqual((self.server.session, self.server.csrf,
                                  self.server.session_deadline), before)
            self.assertEqual(self.request("GET", "/api/v1/session", cookie=cookie)[0],
                             401 if expired else 200)


if __name__ == "__main__":
    unittest.main()
