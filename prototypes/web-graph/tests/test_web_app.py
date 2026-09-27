"""Régressions synthétiques du lanceur 8081 et de la bibliothèque Web."""

from __future__ import annotations

import http.client
import json
import os
import socket
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from pathlib import Path
from unittest import mock

WEB = Path(__file__).resolve().parents[1]
ROOT = WEB.parents[1]
sys.path.insert(0, str(WEB))

import web_app  # noqa: E402
from web_library import LibraryError, WebLibrary  # noqa: E402
from workspace_server import Handler, WorkspaceServer  # noqa: E402


class SyntheticEnvironment(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="labfy-web-SPECIMEN-")
        self.root = Path(self.temporary.name)
        self.runtime = self.root / "runtime"
        self.state = self.root / "state"
        self.runtime.mkdir(mode=0o700)
        self.state.mkdir(mode=0o700)
        self.environment = {**os.environ, "XDG_RUNTIME_DIR": str(self.runtime),
                            "XDG_STATE_HOME": str(self.state)}
        self.bridge = self.root / "fake-local-jobs"
        self.bridge.write_text("""#!/usr/bin/env python3
import json,sys
from pathlib import Path
args=sys.argv[1:]
workspace=Path(args[args.index('--workspace')+1])
if args[0]=='create-workspace':
    title=args[args.index('--title')+1]
    (workspace/'.labfy/runtime').mkdir(parents=True,exist_ok=True)
    (workspace/'Enquete.sqlite').write_bytes(b'SPECIMEN')
    manifest={'contract':'labfy.local_workspace.v1','investigation_id':'00000000-0000-4000-8000-000000000001','title':title,'mode':'local_experimental'}
    (workspace/'.labfy/runtime/workspace.json').write_text(json.dumps(manifest))
elif args[0]=='export':
    pass
else:
    print('commande SPECIMEN inconnue',file=sys.stderr)
    sys.exit(2)
""", encoding="utf-8")
        self.bridge.chmod(0o700)

    def tearDown(self):
        # Les tests de cycle doivent toujours arrêter leur propre serveur.
        subprocess.run([sys.executable, str(WEB / "web_app.py"), "stop"],
                       env=self.environment, capture_output=True, timeout=8)
        self.temporary.cleanup()


class LauncherTest(SyntheticEnvironment):
    def command(self, *arguments, timeout=8):
        return subprocess.run([sys.executable, str(WEB / "web_app.py"), *arguments],
            cwd=self.root, env=self.environment, text=True, capture_output=True,
            timeout=timeout, check=False)

    def test_default_and_invalid_ports(self):
        parser = web_app.build_parser()
        arguments = parser.parse_args(["serve", "--workspace", str(self.root / "w")])
        self.assertEqual(arguments.port, 8081)
        for port in ("-1", "65536", "abc"):
            with self.assertRaises(SystemExit):
                parser.parse_args(["serve", "--workspace", str(self.root / "w"),
                                   "--port", port])
        self.assertEqual(parser.parse_args(["serve", "--workspace",
            str(self.root / "w"), "--port", "0"]).port, 0)

    def test_xdg_start_status_auto_session_double_start_and_stop(self):
        workspace = self.root / "workspace"
        started = self.command("start", "--workspace", str(workspace), "--port", "0",
                               "--bridge", str(self.bridge))
        self.assertEqual(started.returncode, 0, started.stderr)
        self.assertIn("Labfy Web : http://127.0.0.1:", started.stdout)
        self.assertNotIn("Code de session", started.stdout)
        instance = (self.runtime / web_app.APP_DIRECTORY / "instance.json")
        self.assertTrue(instance.is_file())
        self.assertIn('"automatic_session":true', instance.read_text(encoding="utf-8"))
        self.assertEqual(instance.stat().st_mode & 0o777, 0o600)
        self.assertEqual((self.runtime / web_app.APP_DIRECTORY).stat().st_mode & 0o777,
                         0o700)
        config = self.state / web_app.APP_DIRECTORY / "config.json"
        self.assertEqual(config.stat().st_mode & 0o777, 0o600)
        status = self.command("status")
        self.assertEqual(status.returncode, 0, status.stderr)
        self.assertIn("actif", status.stdout)
        duplicate = self.command("start", "--workspace", str(workspace), "--port", "0",
                                 "--bridge", str(self.bridge))
        self.assertEqual(duplicate.returncode, 0, duplicate.stderr)
        self.assertIn("déjà active", duplicate.stderr)
        stopped = self.command("stop")
        self.assertEqual(stopped.returncode, 0, stopped.stderr)
        self.assertFalse(instance.exists())

    def test_occupied_port_does_not_disturb_witness(self):
        witness = socket.socket()
        witness.bind(("127.0.0.1", 0))
        witness.listen()
        port = witness.getsockname()[1]
        result = self.command("serve", "--workspace", str(self.root / "occupied"),
            "--port", str(port), "--bridge", str(self.bridge))
        self.assertNotEqual(result.returncode, 0)
        witness.settimeout(0.2)
        probe = socket.create_connection(("127.0.0.1", port), timeout=1)
        accepted, _ = witness.accept()
        probe.close()
        accepted.close()
        witness.close()

    def test_stale_state_never_signals_unconfirmed_pid(self):
        with mock.patch.dict(os.environ, self.environment, clear=True):
            files = web_app.InstanceFiles()
            web_app._atomic_json(files.instance, {
                "pid": os.getpid(), "proc_start": web_app._proc_start(os.getpid()),
                "port": 1, "instance_id": "foreign", "config_id": "foreign"})
            with mock.patch.object(web_app.os, "kill") as kill:
                self.assertEqual(web_app.stop(None), 1)
                kill.assert_not_called()


class LibraryTest(SyntheticEnvironment):
    def setUp(self):
        super().setUp()
        self.library = WebLibrary(self.root / "library", self.bridge)

    def _creation_intent_workspace_id(self):
        intent = json.loads(self.library.creation_intent_path.read_text(encoding="utf-8"))
        return intent["workspace_id"]

    def _assert_resumed_once(self, expected_workspace_id, response):
        self.assertEqual(response["workspace_id"], expected_workspace_id)
        self.assertEqual(len(list(self.library.workspaces.iterdir())), 1)
        self.assertEqual(len(self.library.snapshot()["entries"]), 1)
        self.assertFalse(self.library.creation_intent_path.exists())

    def test_empty_create_replay_conflict_and_symlink_rejection(self):
        self.assertEqual(self.library.snapshot()["entries"], [])
        created = self.library.create("Enquête SPECIMEN", "intent-1")
        self.assertFalse(created["replayed"])
        replay = self.library.create("Enquête SPECIMEN", "intent-1")
        self.assertTrue(replay["replayed"])
        self.assertEqual(replay["workspace_id"], created["workspace_id"])
        with self.assertRaisesRegex(LibraryError, "autre titre"):
            self.library.create("Titre contradictoire", "intent-1")
        workspace = self.library.workspaces / created["workspace_id"]
        moved = self.root / "moved-workspace"
        workspace.rename(moved)
        workspace.symlink_to(moved, target_is_directory=True)
        with self.assertRaisesRegex(LibraryError, "invalide"):
            self.library.snapshot()

    def test_create_resumes_same_workspace_after_bridge_interruption(self):
        run = subprocess.run
        calls = 0

        def interrupt_after_bridge(*arguments, **keywords):
            nonlocal calls
            calls += 1
            result = run(*arguments, **keywords)
            raise OSError("interruption SPECIMEN après bridge")

        with mock.patch("web_library.subprocess.run", side_effect=interrupt_after_bridge):
            with self.assertRaisesRegex(OSError, "après bridge"):
                self.library.create("Reprise bridge SPECIMEN", "resume-bridge")
            workspace_id = self._creation_intent_workspace_id()
            self.library = WebLibrary(self.library.root, self.bridge)
            resumed = self.library.create("Reprise bridge SPECIMEN", "resume-bridge")

        self.assertEqual(calls, 1)
        self._assert_resumed_once(workspace_id, resumed)

    def test_create_resumes_same_workspace_after_validation_interruption(self):
        validate = WebLibrary._validate_created_workspace
        validations = 0

        def interrupt_after_validation(workspace):
            nonlocal validations
            validate(workspace)
            validations += 1
            if validations == 1:
                raise OSError("interruption SPECIMEN après validation")

        with mock.patch.object(WebLibrary, "_validate_created_workspace",
                               side_effect=interrupt_after_validation):
            with self.assertRaisesRegex(OSError, "après validation"):
                self.library.create("Reprise validation SPECIMEN", "resume-validation")
            workspace_id = self._creation_intent_workspace_id()

        self.library = WebLibrary(self.library.root, self.bridge)
        with mock.patch("web_library.subprocess.run") as bridge:
            resumed = self.library.create(
                "Reprise validation SPECIMEN", "resume-validation")
        bridge.assert_not_called()
        self._assert_resumed_once(workspace_id, resumed)

    def test_create_republishes_same_workspace_after_registry_write_failure(self):
        with mock.patch.object(self.library, "_write_registry",
                               side_effect=OSError("écriture registre SPECIMEN")):
            with self.assertRaisesRegex(OSError, "écriture registre"):
                self.library.create("Reprise registre SPECIMEN", "resume-registry")
            workspace_id = self._creation_intent_workspace_id()

        self.library = WebLibrary(self.library.root, self.bridge)
        with mock.patch("web_library.subprocess.run") as bridge:
            resumed = self.library.create("Reprise registre SPECIMEN", "resume-registry")
        bridge.assert_not_called()
        self._assert_resumed_once(workspace_id, resumed)

    def test_create_finishes_after_late_registry_publication_failure(self):
        write_registry = self.library._write_registry

        def interrupt_after_publication(registry):
            write_registry(registry)
            raise OSError("interruption SPECIMEN après publication")

        with mock.patch.object(self.library, "_write_registry",
                               side_effect=interrupt_after_publication):
            with self.assertRaisesRegex(OSError, "après publication"):
                self.library.create("Publication SPECIMEN", "resume-published")
            workspace_id = self._creation_intent_workspace_id()

        self.library = WebLibrary(self.library.root, self.bridge)
        with mock.patch("web_library.subprocess.run") as bridge:
            resumed = self.library.create("Publication SPECIMEN", "resume-published")
        bridge.assert_not_called()
        self.assertTrue(resumed["replayed"])
        self._assert_resumed_once(workspace_id, resumed)

    def test_authenticated_routes_generation_stale_and_busy(self):
        server = WorkspaceServer(("127.0.0.1", 0), Handler,
            workspace=self.library.root / ".inactive", bridge=self.bridge,
            bootstrap="SPECIMEN-bootstrap", library=self.library,
            instance_id="SPECIMEN-instance", config_id="SPECIMEN-config")
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        cookie = None

        def request(method, path, body=None, *, authenticated=True, csrf=True):
            nonlocal cookie
            connection = http.client.HTTPConnection("127.0.0.1", server.server_port,
                                                     timeout=3)
            headers = {"Host": server.authority}
            if method == "POST":
                headers.update({"Origin": server.origin, "Content-Type": "application/json"})
                if csrf: headers["X-Labfy-CSRF"] = server.csrf
            if authenticated and cookie: headers["Cookie"] = cookie
            payload = json.dumps(body) if body is not None else None
            connection.request(method, path, body=payload, headers=headers)
            response = connection.getresponse()
            data = response.read()
            if response.getheader("Set-Cookie"):
                cookie = response.getheader("Set-Cookie").split(";", 1)[0]
            result = response.status, (json.loads(data) if data else {})
            connection.close()
            return result

        try:
            self.assertEqual(request("GET", "/api/v1/library", authenticated=False)[0], 401)
            self.assertEqual(request("GET", "/", authenticated=False)[0], 303)
            self.assertEqual(request("POST", "/api/v1/library/workspaces",
                {"title": "Sans CSRF", "idempotency_key": "x"}, csrf=False)[0], 403)
            status, empty = request("GET", "/api/v1/library")
            self.assertEqual(status, 200)
            self.assertEqual(empty["generation"], 0)
            status, first = request("POST", "/api/v1/library/workspaces",
                {"title": "Première SPECIMEN", "idempotency_key": "create-1"})
            self.assertEqual(status, 201, first)
            time.sleep(0.04)
            status, replay = request("POST", "/api/v1/library/workspaces",
                {"title": "Première SPECIMEN", "idempotency_key": "create-1"})
            self.assertEqual(status, 200)
            self.assertTrue(replay["replayed"])
            time.sleep(0.04)
            status, second = request("POST", "/api/v1/library/workspaces",
                {"title": "Seconde SPECIMEN", "idempotency_key": "create-2"})
            self.assertEqual(status, 201, second)
            time.sleep(0.04)
            stale = request("POST", "/api/v1/library/open", {
                "workspace_id": first["workspace_id"], "expected_generation": 1})
            self.assertEqual(stale[0], 409)
            time.sleep(0.04)
            opened = request("POST", "/api/v1/library/open", {
                "workspace_id": first["workspace_id"],
                "expected_generation": second["generation"]})
            self.assertEqual(opened[0], 200, opened)
            time.sleep(0.04)
            busy = request("POST", "/api/v1/library/open", {
                "workspace_id": second["workspace_id"],
                "expected_generation": second["generation"]})
            self.assertEqual(busy[0], 409)
            session = request("GET", "/api/v1/session")[1]
            self.assertTrue(session["library_mode"])
            self.assertEqual(session["active_workspace_id"], first["workspace_id"])
            self.assertEqual(session["generation"], second["generation"])
        finally:
            server.shutdown()
            server.server_close()
            thread.join(3)


if __name__ == "__main__":
    unittest.main()
