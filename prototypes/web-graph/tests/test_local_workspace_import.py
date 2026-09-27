"""Régressions synthétiques du cycle espace vide et import navigateur."""

import base64
import hashlib
import http.client
import json
import socket
import sqlite3
import subprocess
import sys
import tempfile
import threading
import time
import unittest
import uuid
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
REPOSITORY = ROOT.parents[1]
sys.path.insert(0, str(ROOT))
import workspace_server  # noqa:E402
from workspace_server import Handler, WorkspaceServer, MAX_UPLOAD_BYTES  # noqa:E402


class LocalWorkspaceImportTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="labfy-import-SPECIMEN-")
        self.workspace = Path(self.temporary.name) / "workspace"
        self.workspace.mkdir(mode=0o700)
        self.server = WorkspaceServer(("127.0.0.1", 0), Handler,
            workspace=self.workspace, bridge=REPOSITORY / "tools" / "local-jobs")
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.cookie = None

    def tearDown(self):
        self.server.shutdown(); self.server.server_close(); self.thread.join(3)
        self.temporary.cleanup()

    def request(self, method, path, body=None, *, origin=True, csrf=True,
                content_type="application/json"):
        connection = http.client.HTTPConnection("127.0.0.1", self.server.server_port, timeout=5)
        headers = {"Host": self.server.authority, "Content-Type": content_type}
        if origin: headers["Origin"] = self.server.origin
        if self.cookie: headers["Cookie"] = self.cookie
        if csrf and self.cookie: headers["X-Labfy-CSRF"] = self.server.csrf
        payload = json.dumps(body).encode() if content_type == "application/json" and body is not None else body
        connection.request(method, path, body=payload, headers=headers)
        response = connection.getresponse(); data = response.read()
        cookie = response.getheader("Set-Cookie")
        if cookie: self.cookie = cookie.split(";", 1)[0]
        connection.close()
        return response.status, json.loads(data) if data else None

    def login_create(self):
        status, _ = self.request("GET", "/", csrf=False)
        self.assertEqual(status, 303)
        status, value = self.request("POST", "/api/v1/workspace",
                                     {"title":"Dossier synthétique Ω"})
        self.assertEqual(status, 201); return value

    def login_existing_specimen(self):
        result = subprocess.run([str(REPOSITORY / "tools/local-jobs"),
            "init-specimen", "--workspace", str(self.workspace)],
            cwd=REPOSITORY, text=True, capture_output=True, check=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        status, _ = self.request("GET", "/", csrf=False)
        self.assertEqual(status, 303)
        return json.loads((self.workspace / ".labfy/runtime/specimen.json").read_text())

    def restart_server(self):
        self.server.shutdown(); self.server.server_close(); self.thread.join(3)
        self.server = WorkspaceServer(("127.0.0.1", 0), Handler,
            workspace=self.workspace, bridge=REPOSITORY / "tools" / "local-jobs")
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start(); self.cookie = None
        status, _ = self.request("GET", "/", csrf=False)
        self.assertEqual(status, 303)
        status, _ = self.request("GET", "/api/v1/session", csrf=False)
        self.assertEqual(status, 200)

    def prepare(self, name, data, declared="application/octet-stream"):
        status, intent = self.request("POST", "/api/v1/uploads",
            {"name":name,"size":str(len(data)),"declared_type":declared,
             "selection_id":str(uuid.uuid4())})
        self.assertEqual(status, 201)
        status, prepared = self.request("PUT", f"/api/v1/uploads/{intent['upload_id']}",
            data, content_type="application/octet-stream")
        self.assertEqual(status, 200); return prepared

    def create_intent(self, name, size, declared="application/octet-stream"):
        status, intent = self.request("POST", "/api/v1/uploads",
            {"name":name,"size":str(size),"declared_type":declared,
             "selection_id":str(uuid.uuid4())})
        self.assertEqual(status, 201)
        return intent

    def bind_confirmation(self, prepared, key, source, description):
        path=self.workspace/".labfy/uploads"/prepared["upload_id"]/"intent.json"
        intent=json.loads(path.read_text(encoding="utf-8"))
        intent["state"]="CONFIRMING"
        intent["confirmation"]={"contract":"labfy.local_import.intent.v1",
            "idempotency_key":key,"source":source,"description":description}
        path.write_text(json.dumps(intent,separators=(",",":")),encoding="utf-8")

    def raw_put_headers(self, upload_id, length):
        return self.raw_put_request(upload_id, [("Content-Length", str(length))])

    def raw_put_request(self, upload_id, framing_headers, body=b""):
        connection = socket.create_connection(
            ("127.0.0.1", self.server.server_port), timeout=2)
        request = (f"PUT /api/v1/uploads/{upload_id} HTTP/1.1\r\n"
            f"Host: {self.server.authority}\r\nOrigin: {self.server.origin}\r\n"
            f"Cookie: {self.cookie}\r\nX-Labfy-CSRF: {self.server.csrf}\r\n"
            "Content-Type: application/octet-stream\r\n" +
            "".join(f"{name}: {value}\r\n" for name, value in framing_headers) +
            "Connection: close\r\n\r\n")
        connection.sendall(request.encode("ascii") + body)
        return connection

    @staticmethod
    def response_status(connection):
        data = b""
        while b"\r\n" not in data:
            block = connection.recv(4096)
            if not block: break
            data += block
        return int(data.split(b" ", 2)[1])

    @staticmethod
    def response_status_and_eof(connection):
        data = b""
        while True:
            block = connection.recv(4096)
            if not block:
                break
            data += block
        return int(data.split(b" ", 2)[1]), data

    def test_p01_double_creation_is_non_destructive(self):
        self.login_create()
        protected = [self.workspace/"Enquete.sqlite",
            self.workspace/".labfy/runtime/workspace.json",
            self.workspace/".labfy/runtime/jobs.sqlite"]
        before = {path:hashlib.sha256(path.read_bytes()).hexdigest()
                  for path in protected}
        result = subprocess.run([str(REPOSITORY/"tools/local-jobs"),
            "create-workspace","--workspace",str(self.workspace),
            "--title","Autre titre SPECIMEN"], cwd=REPOSITORY,
            text=True,capture_output=True,check=False)
        self.assertNotEqual(result.returncode,0)
        after = {path:hashlib.sha256(path.read_bytes()).hexdigest()
                 for path in protected}
        self.assertEqual(before,after)

    def test_p01_partial_managed_artifact_is_preserved(self):
        partial = Path(self.temporary.name) / "partial"
        partial.mkdir(mode=0o700)
        orphan = partial / "core-snapshot.json"
        contents = b'{"contract":"foreign.SPECIMEN"}\n'
        orphan.write_bytes(contents)
        result = subprocess.run([str(REPOSITORY/"tools/local-jobs"),
            "create-workspace","--workspace",str(partial),
            "--title","Ne doit pas être créée"], cwd=REPOSITORY,
            text=True,capture_output=True,check=False)
        self.assertNotEqual(result.returncode,0)
        self.assertEqual(orphan.read_bytes(),contents)
        self.assertFalse((partial/"Enquete.sqlite").exists())
        self.assertFalse((partial/".labfy/runtime/workspace.json").exists())

    def test_evidence_preview_formats_integrity_and_route_security(self):
        manifest = self.login_existing_specimen()
        for evidence_id, kind in ((manifest["eml_id"], "email"),
                                  (manifest["image_id"], "image")):
            status, preview = self.request(
                "GET", f"/api/v1/evidence/{evidence_id}/preview")
            self.assertEqual(status, 200, preview)
            self.assertEqual(preview["kind"], kind)
            self.assertTrue(preview["integrity_valid"])
            self.assertNotIn("path", preview)
        self.assertIn("APERÇU EML", self.request("GET",
            f"/api/v1/evidence/{manifest['eml_id']}/preview")[1]["text"])
        jpeg = base64.b64decode("/9j/4AAQSkZJRgABAQAAAQABAAD/2wBDAAMCAgICAgMCAgIDAwMDBAYEBAQEBAgGBgUGCQgKCgkICQkKDA8MCgsOCwkJDRENDg8QEBEQCgwSExIQEw8QEBD/2wBDAQMDAwQDBAgEBAgQCwkLEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBD/wAARCAACAAIDAREAAhEBAxEB/8QAFAABAAAAAAAAAAAAAAAAAAAACP/EABQQAQAAAAAAAAAAAAAAAAAAAAD/xAAVAQEBAAAAAAAAAAAAAAAAAAAFB//EABQRAQAAAAAAAAAAAAAAAAAAAAD/2gAMAwEAAhEDEQA/AFgrYR//2Q==")
        prepared = self.prepare("photo SPECIMEN.jpg", jpeg, "image/jpeg")
        confirmation = {
                "idempotency_key":str(uuid.uuid4()), "source":"SPECIMEN",
                "description":"JPEG synthétique"}
        status, _ = self.request("POST",
            f"/api/v1/uploads/{prepared['upload_id']}/confirm", {
                **confirmation})
        self.assertEqual(status, 200)
        status, replay = self.request("POST",
            f"/api/v1/uploads/{prepared['upload_id']}/confirm", confirmation)
        self.assertEqual(status, 200, replay)
        status, preview = self.request("GET",
            f"/api/v1/evidence/{prepared['evidence_id']}/preview")
        self.assertEqual(status, 200, preview); self.assertEqual(preview["kind"], "image")
        status, _ = self.request("GET", f"/api/v1/evidence/{uuid.uuid4()}/preview")
        self.assertEqual(status, 409)
        other = Path(self.temporary.name) / "other-workspace"
        other.mkdir(mode=0o700)
        result = subprocess.run([str(REPOSITORY / "tools/local-jobs"),
            "init-specimen", "--workspace", str(other)], cwd=REPOSITORY,
            text=True, capture_output=True, check=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        other_manifest = json.loads(
            (other / ".labfy/runtime/specimen.json").read_text())
        status, _ = self.request("GET",
            f"/api/v1/evidence/{other_manifest['eml_id']}/preview")
        self.assertEqual(status, 409)
        status, _ = self.request("POST",
            f"/api/v1/evidence/{manifest['eml_id']}/observations/{uuid.uuid4()}/withdraw",
            {"operation_id":str(uuid.uuid4()), "expected_revision":"0",
             "author":"test", "reason":"sécurité"}, csrf=False)
        self.assertEqual(status, 403)
        original = next((self.workspace / "01_Preuves_Originales" / "Emails").iterdir())
        saved = original.read_bytes(); original.write_bytes(b"X" + saved[1:])
        status, _ = self.request("GET",
            f"/api/v1/evidence/{manifest['eml_id']}/preview")
        self.assertEqual(status, 409); original.write_bytes(saved)
        missing = original.with_suffix(".missing")
        original.replace(missing)
        status, _ = self.request("GET",
            f"/api/v1/evidence/{manifest['eml_id']}/preview")
        self.assertEqual(status, 409); missing.replace(original)

    def test_review_promotion_withdraw_replay_and_projection_refresh(self):
        manifest = self.login_existing_specimen()
        self.server.bridge_call(["enqueue"])
        self.server.bridge_call(["run"], timeout=30)
        path = f"/api/v1/evidence/{manifest['eml_id']}/observations"
        status, value = self.request("GET", path)
        self.assertEqual(status, 200, value); self.assertTrue(value["observations"])
        observation = value["observations"][0]
        common = {"operation_id":str(uuid.uuid4()),
            "expected_revision":str(observation["revision"]),
            "author":"Harness SPECIMEN", "reason":"Vérification synthétique"}
        review = {**common, "action":"decide", "verification_status":"confirmed",
                  "corrected_value":""}
        route = f"{path}/{observation['id']}/review"
        status, result = self.request("POST", route, review)
        self.assertEqual(status, 200, result); self.assertTrue(result["projections_refreshed"])
        status, replay = self.request("POST", route, review)
        self.assertEqual(status, 200, replay); self.assertTrue(replay["replayed"])
        promote = {"operation_id":str(uuid.uuid4()),
            "expected_revision":str(result["revision"]), "author":"Harness SPECIMEN",
            "reason":"Indicateur explicite", "action":"create", "entity_id":""}
        status, promoted = self.request("POST",
            f"{path}/{observation['id']}/promotion", promote)
        self.assertEqual(status, 200, promoted); self.assertTrue(promoted["entity_created"])
        self.assertIsNotNone(promoted["entity_id"])
        status, graph = self.request("GET", "/api/v1/snapshot")
        self.assertEqual(status, 200); self.assertTrue(any(
            node.get("object_id") == promoted["entity_id"] for node in graph["nodes"]))
        withdraw = {"operation_id":str(uuid.uuid4()),
            "expected_revision":str(promoted["revision"]), "author":"Harness SPECIMEN",
            "reason":"Retrait explicite"}
        status, removed = self.request("POST",
            f"{path}/{observation['id']}/withdraw", withdraw)
        self.assertEqual(status, 200, removed)
        status, observations = self.request("GET", path)
        current = next(item for item in observations["observations"]
                       if item["id"] == observation["id"])
        self.assertIsNone(current["entity_id"])
        self.restart_server()
        status, observations = self.request("GET", path)
        self.assertEqual(status, 200)
        current = next(item for item in observations["observations"]
                       if item["id"] == observation["id"])
        self.assertIsNone(current["entity_id"])
        self.assertEqual(current["revision"], removed["revision"])

    def test_p01_two_c_creators_publish_one_workspace(self):
        concurrent = Path(self.temporary.name) / "concurrent-creation"
        concurrent.mkdir(mode=0o700)
        command=[str(REPOSITORY/"tools/local-jobs"),"create-workspace",
            "--workspace",str(concurrent),"--title","Course SPECIMEN"]
        processes=[subprocess.Popen(command,cwd=REPOSITORY,
            stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True) for _ in range(2)]
        results=[process.communicate(timeout=10) for process in processes]
        self.assertEqual(sorted(process.returncode for process in processes),[0,1],results)
        manifest=json.loads((concurrent/".labfy/runtime/workspace.json").read_text())
        database=sqlite3.connect(concurrent/"Enquete.sqlite")
        self.assertEqual(database.execute("SELECT id FROM investigation").fetchone()[0],
                         manifest["investigation_id"])
        database.close()

    def test_p02_replay_revalidates_published_original_and_receipt(self):
        self.login_create()
        data=b"From: Integrity SPECIMEN <integrity@example.test>\r\n\r\nBody\r\n"
        prepared=self.prepare("integrity.eml",data,"message/rfc822")
        confirmation={"idempotency_key":str(uuid.uuid4()),
            "source":"Source SPECIMEN","description":"Description SPECIMEN"}
        status,_=self.request("POST",f"/api/v1/uploads/{prepared['upload_id']}/confirm",
                              confirmation)
        self.assertEqual(status,200)
        database=sqlite3.connect(self.workspace/"Enquete.sqlite")
        relative=database.execute("SELECT relative_path FROM preuves WHERE id=?",
                                  (prepared["evidence_id"],)).fetchone()[0]
        database.close()
        original=self.workspace/relative
        original.write_bytes(b"X"+data[1:])
        status,value=self.request("POST",f"/api/v1/uploads/{prepared['upload_id']}/confirm",
                                  confirmation)
        self.assertEqual(status,409);self.assertIn("altéré",value["message"])

    def test_p03_crash_binds_free_text_without_ambiguous_encoding(self):
        self.login_create()
        data=b"From: Crash SPECIMEN <crash2@example.test>\r\n\r\nSynthetic\r\n"
        prepared=self.prepare("crash-fields.eml",data,"message/rfc822")
        key=str(uuid.uuid4())
        source="Source A SPECIMEN\navec retour"
        description="Description A\nseconde ligne"
        self.bind_confirmation(prepared,key,source,description)
        command=[str(REPOSITORY/"tools/local-jobs-test"),
            "__test-crash-import-after-commit","--workspace",str(self.workspace),
            "--upload",prepared["upload_id"],"--key",key,
            "--evidence",prepared["evidence_id"],"--name",prepared["name"],
            "--type",prepared["recognized_type"],"--size",str(prepared["expected_size"]),
            "--sha256",prepared["sha256"],"--source",source,
            "--description",description]
        self.assertEqual(subprocess.run(command,cwd=REPOSITORY,check=False).returncode,87)
        status,_=self.request("POST",f"/api/v1/uploads/{prepared['upload_id']}/confirm",
            {"idempotency_key":key,"source":"Source B SPECIMEN",
             "description":description})
        self.assertEqual(status,409)
        database=sqlite3.connect(self.workspace/"Enquete.sqlite")
        row=database.execute("SELECT source,description FROM preuves WHERE id=?",
                             (prepared["evidence_id"],)).fetchone()
        database.close();self.assertEqual(row,(source,description))

    def test_replay_rejects_missing_original_and_truncated_receipt(self):
        self.login_create()
        data=b"From: Missing SPECIMEN <missing@example.test>\r\n\r\nBody\r\n"
        prepared=self.prepare("missing.eml",data,"message/rfc822")
        key=str(uuid.uuid4())
        confirmation={"idempotency_key":key,"source":"","description":""}
        self.assertEqual(self.request("POST",
            f"/api/v1/uploads/{prepared['upload_id']}/confirm",confirmation)[0],200)
        receipt=self.workspace/".labfy/imports"/hashlib.sha256(key.encode()).hexdigest()
        receipt.write_text("{",encoding="utf-8")
        self.assertEqual(self.request("POST",
            f"/api/v1/uploads/{prepared['upload_id']}/confirm",confirmation)[0],409)
        # Restore a structurally valid receipt, then prove that the published
        # original itself remains mandatory even though transport staging is gone.
        intent=json.loads((self.workspace/".labfy/uploads"/prepared["upload_id"]/
                           "intent.json").read_text())
        command=[str(REPOSITORY/"tools/local-jobs"),"confirm-import-json",
            "--workspace",str(self.workspace),"--upload",prepared["upload_id"],
            "--key",key,"--evidence",prepared["evidence_id"],"--name",prepared["name"],
            "--type",prepared["recognized_type"],"--size",str(prepared["expected_size"]),
            "--sha256",prepared["sha256"],"--source","","--description",""]
        # Recreate only the receipt through a normal replay after removing the
        # corrupt technical journal; no business record is recreated.
        receipt.unlink()
        self.assertEqual(subprocess.run(command,cwd=REPOSITORY,check=False).returncode,0)
        database=sqlite3.connect(self.workspace/"Enquete.sqlite")
        relative=database.execute("SELECT relative_path FROM preuves WHERE id=?",
                                  (intent["evidence_id"],)).fetchone()[0]
        database.close();(self.workspace/relative).unlink()
        self.assertNotEqual(subprocess.run(command,cwd=REPOSITORY,check=False).returncode,0)

    def test_p04_imported_upload_cannot_be_replaced(self):
        self.login_create()
        first=b"From: First SPECIMEN <first@example.test>\r\n\r\nOne!\r\n"
        prepared=self.prepare("terminal.eml",first,"message/rfc822")
        confirmation={"idempotency_key":str(uuid.uuid4()),"source":"","description":""}
        self.assertEqual(self.request("POST",
            f"/api/v1/uploads/{prepared['upload_id']}/confirm",confirmation)[0],200)
        second=b"From: Other SPECIMEN <other@example.test>\r\n\r\nTwo!\r\n"
        second=second[:len(first)].ljust(len(first),b" ")
        status,_=self.request("PUT",f"/api/v1/uploads/{prepared['upload_id']}",
                              second,content_type="application/octet-stream")
        self.assertEqual(status,400)
        database=sqlite3.connect(self.workspace/"Enquete.sqlite")
        self.assertEqual(database.execute("SELECT count(*) FROM preuves").fetchone()[0],1)
        database.close()

    def test_p05_concurrent_staging_reservation_is_atomic(self):
        self.login_create()
        first=self.create_intent("quota-a.png",100,"image/png")
        second=self.create_intent("quota-b.png",100,"image/png")
        with mock.patch.object(workspace_server,"MAX_STAGING_BYTES",150):
            left=self.raw_put_headers(first["upload_id"],100)
            deadline=time.monotonic()+2
            while self.server.active_uploads != 1 and time.monotonic()<deadline:
                time.sleep(0.01)
            self.assertEqual(self.server.active_uploads,1)
            right=self.raw_put_headers(second["upload_id"],100)
            self.assertEqual(self.response_status(right),413)
            left.sendall(b"\x89PNG\r\n\x1a\n"+b"x"*92)
            self.assertEqual(self.response_status(left),200)
            left.close();right.close()
        deadline=time.monotonic()+1
        while self.server.active_uploads and time.monotonic()<deadline:
            time.sleep(0.01)
        self.assertEqual(self.server.active_uploads,0)
        self.assertEqual(self.server.reserved_upload_bytes,0)

    def test_p06_silent_client_is_timed_out_and_releases_resources(self):
        self.login_create()
        intent=self.create_intent("silent.png",100,"image/png")
        with mock.patch.object(workspace_server,"UPLOAD_TIMEOUT_SECONDS",0.2):
            connection=self.raw_put_headers(intent["upload_id"],100)
            started=time.monotonic();status=self.response_status(connection)
            elapsed=time.monotonic()-started;connection.close()
        self.assertEqual(status,400);self.assertLess(elapsed,1.5)
        self.assertEqual(self.server.active_uploads,0)
        self.assertEqual(self.server.reserved_upload_bytes,0)

    def test_p06_trickling_client_cannot_extend_absolute_deadline(self):
        self.login_create()
        intent=self.create_intent("trickle.png",100,"image/png")
        with mock.patch.object(workspace_server,"UPLOAD_TIMEOUT_SECONDS",0.2):
            connection=self.raw_put_headers(intent["upload_id"],100)
            stop=threading.Event()
            def trickle():
                while not stop.wait(0.04):
                    try: connection.sendall(b"x")
                    except OSError: return
            sender=threading.Thread(target=trickle,daemon=True);sender.start()
            started=time.monotonic();status=self.response_status(connection)
            elapsed=time.monotonic()-started;stop.set();sender.join(1);connection.close()
        self.assertEqual(status,400);self.assertLess(elapsed,0.8)
        self.assertEqual(self.server.active_uploads,0)
        self.assertEqual(self.server.reserved_upload_bytes,0)

    def test_http_upload_framing_is_strict_and_refusals_close(self):
        self.login_create()
        cases = [
            [],
            [("Content-Length", "-1")],
            [("Content-Length", "not-a-number")],
            [("Content-Length", "8"), ("Content-Length", "9")],
            [("Transfer-Encoding", "chunked")],
            [("Transfer-Encoding", "chunked"), ("Content-Length", "8")],
            [("Content-Length", "7")],
        ]
        for index, headers in enumerate(cases):
            with self.subTest(headers=headers):
                intent=self.create_intent(f"framing-{index}.png",8,"image/png")
                connection=self.raw_put_request(intent["upload_id"],headers)
                status,_=self.response_status_and_eof(connection);connection.close()
                self.assertEqual(status,400)
                self.assertEqual(self.server.active_uploads,0)
                self.assertEqual(self.server.reserved_upload_bytes,0)

        intent=self.create_intent("same-length.png",8,"image/png")
        connection=self.raw_put_request(intent["upload_id"],
            [("Content-Length","8"),("Content-Length","8")],b"\x89PNG\r\n\x1a\n")
        self.assertEqual(self.response_status(connection),200);connection.close()

    def test_framed_upload_finishes_without_eof_and_rejects_early_eof(self):
        self.login_create()
        complete=self.create_intent("keep-open.png",8,"image/png")
        connection=self.raw_put_request(complete["upload_id"],
            [("Content-Length","8")],b"\x89PNG\r\n\x1a\n")
        started=time.monotonic();self.assertEqual(self.response_status(connection),200)
        self.assertLess(time.monotonic()-started,1);connection.close()

        truncated=self.create_intent("truncated.png",8,"image/png")
        connection=self.raw_put_request(truncated["upload_id"],
            [("Content-Length","8")],b"\x89PN")
        connection.shutdown(socket.SHUT_WR)
        status,_=self.response_status_and_eof(connection);connection.close()
        self.assertEqual(status,400)
        self.assertEqual(self.server.active_uploads,0)
        self.assertEqual(self.server.reserved_upload_bytes,0)

    def test_reservation_is_released_when_transfer_state_write_fails(self):
        self.login_create()
        intent=self.create_intent("state-failure.png",8,"image/png")
        with mock.patch.object(Handler,"_write_intent",side_effect=OSError("SPECIMEN")):
            status,_=self.request("PUT",f"/api/v1/uploads/{intent['upload_id']}",
                b"\x89PNG\r\n\x1a\n",content_type="application/octet-stream")
        self.assertEqual(status,400)
        self.assertEqual(self.server.active_uploads,0)
        self.assertEqual(self.server.reserved_upload_bytes,0)

    def test_expiration_does_not_remove_an_upload_owned_by_an_active_operation(self):
        self.login_create()
        intent=self.create_intent("owned.png",8,"image/png")
        directory=self.workspace/".labfy"/"uploads"/intent["upload_id"]
        persisted=json.loads((directory/"intent.json").read_text(encoding="utf-8"))
        persisted["created_at"]=time.time()-workspace_server.UPLOAD_TTL_SECONDS-1
        Handler._write_intent(directory,persisted)
        mutex=self.server.upload_mutex(intent["upload_id"])
        mutex.acquire()
        try:
            status,_=self.request("POST","/api/v1/uploads",{
                "name":"next.png","size":"8","declared_type":"image/png",
                "selection_id":str(uuid.uuid4())})
        finally:
            mutex.release()
        self.assertEqual(status,201)
        self.assertTrue(directory.is_dir())

    def test_server_enforces_selection_count_and_concurrent_confirmations(self):
        self.login_create()
        selection=str(uuid.uuid4())
        for index in range(8):
            status,_=self.request("POST","/api/v1/uploads",{
                "name":f"batch-{index}.png","size":"1",
                "declared_type":"image/png","selection_id":selection})
            self.assertEqual(status,201)
        status,_=self.request("POST","/api/v1/uploads",{
            "name":"batch-overflow.png","size":"1",
            "declared_type":"image/png","selection_id":selection})
        self.assertEqual(status,413)

        data=b"From: Concurrent SPECIMEN <same@example.test>\r\n\r\nBody\r\n"
        prepared=self.prepare("concurrent.eml",data,"message/rfc822")
        confirmation={"idempotency_key":str(uuid.uuid4()),
            "source":"Concurrent SPECIMEN","description":"Same"}
        barrier=threading.Barrier(3);statuses=[]
        def confirm(value):
            barrier.wait();statuses.append(self.request("POST",
                f"/api/v1/uploads/{prepared['upload_id']}/confirm",value)[0])
        threads=[threading.Thread(target=confirm,args=(confirmation,)) for _ in range(2)]
        for thread in threads: thread.start()
        barrier.wait()
        for thread in threads: thread.join(10)
        self.assertEqual(sorted(statuses),[200,200])

        other=self.prepare("divergent.eml",data,"message/rfc822")
        barrier=threading.Barrier(3);statuses=[]
        values=[{"idempotency_key":str(uuid.uuid4()),"source":"A","description":"A"},
                {"idempotency_key":str(uuid.uuid4()),"source":"B","description":"B"}]
        threads=[threading.Thread(target=confirm,args=(value,)) for value in values]
        # Point the closure at the second upload for the divergent race.
        prepared=other
        for thread in threads: thread.start()
        barrier.wait()
        for thread in threads: thread.join(10)
        self.assertEqual(sorted(statuses),[200,409])

    def test_empty_import_exact_bytes_replay_conflict_and_reopen(self):
        created = self.login_create()
        database = sqlite3.connect(self.workspace / "Enquete.sqlite")
        self.assertEqual(database.execute("SELECT count(*) FROM preuves").fetchone()[0], 0)
        eml = (b"From: A SPECIMEN <a@example.test>\r\nTo: Pivot <p@example.test>\r\n"
               b"Message-ID: <local-one@example.test>\r\n\r\nSynthetic.\r\n")
        prepared = self.prepare("message local Ω.eml", eml, "message/rfc822")
        key = str(uuid.uuid4())
        confirm = {"idempotency_key":key,"source":"Harness SPECIMEN",
                   "description":"Octets synthétiques"}
        status, imported = self.request("POST",
            f"/api/v1/uploads/{prepared['upload_id']}/confirm", confirm)
        self.assertEqual(status, 200)
        status, replay = self.request("POST",
            f"/api/v1/uploads/{prepared['upload_id']}/confirm", confirm)
        self.assertEqual(status, 200); self.assertEqual(imported["evidence_id"], replay["evidence_id"])
        row = database.execute("SELECT id,original_name,size_bytes,sha256,collected_at FROM preuves").fetchone()
        self.assertEqual(row, (prepared["evidence_id"], "message local Ω.eml", len(eml),
                               hashlib.sha256(eml).hexdigest(), None))
        stored = next((self.workspace / "01_Preuves_Originales" / "Emails").iterdir())
        self.assertEqual(stored.read_bytes(), eml)
        self.assertEqual(database.execute("SELECT count(*) FROM preuves").fetchone()[0], 1)
        duplicate = self.prepare("copie volontaire.eml", eml, "message/rfc822")
        status, second = self.request("POST",
            f"/api/v1/uploads/{duplicate['upload_id']}/confirm",
            {"idempotency_key":str(uuid.uuid4()),"source":"Harness SPECIMEN",
             "description":"Intention distincte, mêmes octets"})
        self.assertEqual(status,200);self.assertNotEqual(second["evidence_id"],imported["evidence_id"])
        self.assertEqual(database.execute("SELECT count(*) FROM preuves").fetchone()[0],2)
        self.assertEqual(created["investigation_id"], self.server.context()["investigation_id"])
        self.server.bridge_call(["export"])
        self.assertEqual(json.loads((self.workspace/"core-snapshot.json").read_text())
                         ["investigation"]["id"], created["investigation_id"])
        database.close()

    def test_security_limits_signature_cancel_and_no_publication(self):
        self.login_create()
        status, _ = self.request("POST", "/api/v1/uploads",
            {"name":"bad/path.eml","size":"1","declared_type":"message/rfc822",
             "selection_id":str(uuid.uuid4())})
        self.assertEqual(status, 400)
        status, _ = self.request("POST", "/api/v1/uploads",
            {"name":"huge.png","size":str(MAX_UPLOAD_BYTES+1),"declared_type":"image/png",
             "selection_id":str(uuid.uuid4())})
        self.assertEqual(status, 413)
        exact=b"\x89PNG\r\n\x1a\n"+b"x"*(MAX_UPLOAD_BYTES-8)
        prepared=self.prepare("exact-limit.png",exact,"image/png")
        status,_=self.request("POST",f"/api/v1/uploads/{prepared['upload_id']}/cancel",{})
        self.assertEqual(status,200)
        status, _ = self.request("POST", "/api/v1/uploads",
            {"name":"x.eml","size":"4","declared_type":"message/rfc822",
             "selection_id":str(uuid.uuid4())}, csrf=False)
        self.assertEqual(status, 403)
        status, intent = self.request("POST", "/api/v1/uploads",
            {"name":"false.png","size":"4","declared_type":"image/png",
             "selection_id":str(uuid.uuid4())})
        self.assertEqual(status, 201)
        status, _ = self.request("PUT", f"/api/v1/uploads/{intent['upload_id']}",
            b"nope", content_type="application/octet-stream")
        self.assertEqual(status, 400)
        status, intent = self.request("POST", "/api/v1/uploads",
            {"name":"cancel.eml","size":"4","declared_type":"message/rfc822",
             "selection_id":str(uuid.uuid4())})
        self.assertEqual(status, 201)
        status, _ = self.request("POST", f"/api/v1/uploads/{intent['upload_id']}/cancel", {})
        self.assertEqual(status, 200)
        database=sqlite3.connect(self.workspace/"Enquete.sqlite")
        self.assertEqual(database.execute("SELECT count(*) FROM preuves").fetchone()[0],0)
        database.close()

    def test_process_exit_after_commit_reconciles_same_uuid(self):
        self.login_create()
        data=b"From: Crash SPECIMEN <crash@example.test>\r\n\r\nSynthetic\r\n"
        prepared=self.prepare("reprise.eml",data,"message/rfc822")
        key=str(uuid.uuid4())
        self.bind_confirmation(prepared,key,"","")
        command=[str(REPOSITORY/"tools"/"local-jobs-test"),
            "__test-crash-import-after-commit","--workspace",str(self.workspace),
            "--upload",prepared["upload_id"],"--key",key,
            "--evidence",prepared["evidence_id"],"--name",prepared["name"],
            "--type",prepared["recognized_type"],"--size",str(prepared["expected_size"]),
            "--sha256",prepared["sha256"],"--source","","--description",""]
        result=subprocess.run(command,cwd=REPOSITORY,check=False)
        self.assertEqual(result.returncode,87)
        status,value=self.request("POST",f"/api/v1/uploads/{prepared['upload_id']}/confirm",
            {"idempotency_key":key,"source":"","description":""})
        self.assertEqual(status,200);self.assertEqual(value["evidence_id"],prepared["evidence_id"])
        database=sqlite3.connect(self.workspace/"Enquete.sqlite")
        self.assertEqual(database.execute("SELECT count(*) FROM preuves").fetchone()[0],1)
        database.close()

    def test_two_c_processes_replay_one_durable_intent(self):
        self.login_create()
        data=b"From: Race SPECIMEN <race@example.test>\r\n\r\nSynthetic\r\n"
        prepared=self.prepare("race.eml",data,"message/rfc822")
        key=str(uuid.uuid4()); source="Race SPECIMEN"; description="Même intention"
        self.bind_confirmation(prepared,key,source,description)
        command=[str(REPOSITORY/"tools/local-jobs"),"confirm-import-json",
            "--workspace",str(self.workspace),"--upload",prepared["upload_id"],
            "--key",key,"--evidence",prepared["evidence_id"],
            "--name",prepared["name"],"--type",prepared["recognized_type"],
            "--size",str(prepared["expected_size"]),"--sha256",prepared["sha256"],
            "--source",source,"--description",description]
        processes=[subprocess.Popen(command,cwd=REPOSITORY,
            stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True) for _ in range(2)]
        results=[process.communicate(timeout=10) for process in processes]
        self.assertEqual([process.returncode for process in processes],[0,0],results)
        self.assertTrue(all(json.loads(output)["evidence_id"]==prepared["evidence_id"]
                            for output,_ in results))
        conflict=command.copy(); conflict[-1]="Intention contradictoire"
        self.assertNotEqual(subprocess.run(conflict,cwd=REPOSITORY,
                                           capture_output=True,check=False).returncode,0)
        database=sqlite3.connect(self.workspace/"Enquete.sqlite")
        self.assertEqual(database.execute("SELECT count(*) FROM preuves").fetchone()[0],1)
        database.close()


if __name__ == "__main__":
    unittest.main()
