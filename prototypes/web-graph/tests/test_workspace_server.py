import json
import subprocess
import sys
import tempfile
import threading
import unittest
from http.client import HTTPConnection
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from workspace_server import Handler, WorkspaceServer


class WorkspaceServerTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix="labfy-j6-api-")
        cls.workspace = Path(cls.temporary.name)
        cls.repository = Path(__file__).resolve().parents[3]
        cls.bridge = cls.repository / "tools/local-jobs"
        result = subprocess.run(
            [str(cls.bridge), "init-specimen", "--workspace", str(cls.workspace)],
            cwd=cls.repository, text=True, capture_output=True, timeout=30,
            check=False,
        )
        if result.returncode:
            raise RuntimeError(result.stderr)
        subprocess.run([str(cls.bridge), "export", "--workspace", str(cls.workspace)],
                       cwd=cls.repository, timeout=10, check=True)
        cls.manifest = json.loads(
            (cls.workspace / ".labfy/runtime/specimen.json").read_text()
        )
        cls.server = WorkspaceServer(("127.0.0.1", 0), Handler,
                                     workspace=cls.workspace,
                                     bridge=cls.bridge, bootstrap="SPECIMEN-CODE")
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()
        status, headers, _ = cls.request_raw("GET", "/")
        if status != 303:
            raise RuntimeError("session J6 impossible")
        cls.cookie = headers["Set-Cookie"].split(";", 1)[0]
        status, _, body = cls.request_raw("GET", "/api/v1/session", cookie=cls.cookie)
        cls.csrf = json.loads(body)["csrf"]

    @classmethod
    def tearDownClass(cls):
        cls.server.stop_worker()
        cls.server.shutdown()
        cls.server.server_close()
        cls.thread.join()
        cls.temporary.cleanup()

    @classmethod
    def request_raw(cls, method, path, value=None, *, origin=None, cookie=None,
                    csrf=None, host=None, content_type="application/json"):
        connection = HTTPConnection("127.0.0.1", cls.server.server_port, timeout=10)
        headers = {"Host": host or cls.server.authority}
        if origin is not None:
            headers["Origin"] = origin
        if cookie is not None:
            headers["Cookie"] = cookie
        if csrf is not None:
            headers["X-Labfy-CSRF"] = csrf
        body = None
        if value is not None:
            headers["Content-Type"] = content_type
            body = value if isinstance(value, str) else json.dumps(value)
        connection.request(method, path, body=body, headers=headers)
        response = connection.getresponse()
        data = response.read()
        result = response.status, dict(response.getheaders()), data
        connection.close()
        return result

    def mutate(self, path, value):
        return self.request_raw("POST", path, value, origin=self.server.origin,
                                cookie=self.cookie, csrf=self.csrf)

    def setUp(self):
        import time
        time.sleep(0.04)

    def test_authentication_origin_csrf_and_input_rejections(self):
        self.assertEqual(self.request_raw(
            "POST", "/api/v1/session", {}, origin=self.server.origin)[0], 404)
        self.assertEqual(self.request_raw("GET", "/api/v1/jobs")[0], 401)
        self.assertEqual(self.request_raw("GET", "/api/v1/jobs", host="evil.test")[0], 403)
        self.assertEqual(self.request_raw("POST", "/api/v1/queue/pause", {},
                         origin="http://evil.test", cookie=self.cookie,
                         csrf=self.csrf)[0], 403)
        self.assertEqual(self.request_raw("POST", "/api/v1/queue/pause", {},
                         origin=self.server.origin, cookie=self.cookie)[0], 403)
        self.assertEqual(self.request_raw("GET", "/api/v1/queue/pause",
                                         cookie=self.cookie)[0], 404)
        self.assertEqual(self.request_raw("POST", "/api/v1/jobs", "{broken",
                         origin=self.server.origin, cookie=self.cookie,
                         csrf=self.csrf)[0], 400)
        unknown = {"evidence_id": self.manifest["eml_id"],
                   "capability_id": "labfy.capability.unknown.v1",
                   "idempotency_key": "83000000-0000-4000-8000-000000000001"}
        import time
        time.sleep(0.04)
        self.assertEqual(self.mutate("/api/v1/jobs", unknown)[0], 400)

    def test_transactional_idempotence_and_conflict(self):
        intent = {"evidence_id": self.manifest["eml_id"],
                  "capability_id": "labfy.capability.eml_headers.v1",
                  "idempotency_key": "83000000-0000-4000-8000-000000000011"}
        first = self.mutate("/api/v1/jobs", intent)
        self.assertEqual(first[0], 202, first[2])
        # La limite de fréquence est bornée ; attendre sa fenêtre n'est pas une
        # synchronisation de worker mais la règle publique testée ici.
        import time
        time.sleep(0.04)
        second = self.mutate("/api/v1/jobs", intent)
        self.assertEqual(second[0], 202, second[2])
        self.assertEqual(json.loads(first[2])["job_id"], json.loads(second[2])["job_id"])
        time.sleep(0.04)
        conflict = dict(intent, evidence_id=self.manifest["image_id"])
        self.assertEqual(self.mutate("/api/v1/jobs", conflict)[0], 409)

    def test_agent_tool_routes_reuse_security_and_backend_snapshot(self):
        import time
        self.assertEqual(self.request_raw(
            "GET", "/api/v1/agent-tools/catalog")[0], 401)
        status, _, body = self.request_raw(
            "GET", "/api/v1/agent-tools/catalog", cookie=self.cookie)
        self.assertEqual(status, 200, body)
        catalog = json.loads(body)
        self.assertEqual(catalog["contract"], "labfy.agent_tool_protocol.v1")
        self.assertEqual(catalog["transport"], "HTTP_POLLING")
        request = {"turn_id": "83000000-0000-4000-8000-000000000030",
                   "tool_id": "investigation.search", "input": {"query": "SPECIMEN"},
                   "context": {}, "object_refs": [],
                   "idempotency_key": "83000000-0000-4000-8000-000000000031"}
        self.assertEqual(self.request_raw(
            "POST", "/api/v1/agent-tools/calls", request,
            origin="http://evil.test", cookie=self.cookie, csrf=self.csrf)[0], 403)
        time.sleep(0.04)
        status, _, body = self.mutate("/api/v1/agent-tools/calls", request)
        self.assertEqual(status, 201, body)
        admitted = json.loads(body)
        status, _, body = self.request_raw(
            "GET", f'/api/v1/agent-tools/results/{admitted["result_id"]}',
            cookie=self.cookie)
        self.assertEqual(status, 200, body)
        result = json.loads(body)
        self.assertEqual(result["state"], "COMPLETED")
        self.assertNotEqual(result["call_id"], result["result_id"])
        self.assertEqual(result["output"]["matches"], [])
        self.assertEqual(result["object_refs"], [])

        graph = json.loads((self.workspace / "core-snapshot.json").read_text())
        node = next(item for item in graph["nodes"] if item.get("label"))
        positive = {**request,
                    "turn_id": "83000000-0000-4000-8000-000000000032",
                    "input": {"query": node["label"]},
                    "idempotency_key": "83000000-0000-4000-8000-000000000033"}
        time.sleep(0.04)
        status, _, body = self.mutate("/api/v1/agent-tools/calls", positive)
        self.assertEqual(status, 201, body)
        positive_call = json.loads(body)
        status, _, body = self.request_raw(
            "GET", f'/api/v1/agent-tools/results/{positive_call["result_id"]}',
            cookie=self.cookie)
        self.assertEqual(status, 200, body)
        positive_result = json.loads(body)
        self.assertEqual(positive_result["state"], "COMPLETED")
        self.assertIn({"object_id": node["id"]}, positive_result["object_refs"])
        self.assertTrue(any(match.get("object_id") == node["id"]
                            for match in positive_result["output"]["matches"]))
        status, _, body = self.request_raw(
            "GET", "/api/v1/agent-tools/events?cursor=0", cookie=self.cookie)
        self.assertEqual(status, 200, body)
        self.assertTrue(json.loads(body)["events"])
        time.sleep(0.04)
        status, _, body = self.mutate("/api/v1/agent-tools/turns", {
            "objective": node["label"]})
        self.assertEqual(status, 201, body)
        turn = json.loads(body)
        self.assertEqual(turn["state"], "AUTHORIZATION_REQUIRED")
        self.assertTrue(turn["turn_id"])

    def test_concurrent_posts_are_admitted_atomically(self):
        barrier = threading.Barrier(2)

        def synchronize_mutations(_handler):
            barrier.wait(timeout=2)
            return True

        self.server.last_command = 0.0
        statuses = []
        with mock.patch.object(Handler, "_mutation_allowed",
                               autospec=True,
                               side_effect=synchronize_mutations):
            threads = [threading.Thread(
                target=lambda: statuses.append(self.mutate("/api/v1/unknown", {})))
                       for _ in range(2)]
            for thread in threads:
                thread.start()
            for thread in threads:
                thread.join(3)
                self.assertFalse(thread.is_alive())
        self.assertEqual(sorted(status for status, _, _ in statuses), [404, 429])

    def test_plan_contract_rejects_stale_and_unknown_shapes(self):
        import time
        planner = json.loads((self.workspace / "planner-snapshot.json").read_text())
        available = [item for item in planner["recommendations"]
                     if item.get("available") and item.get("kind") == "ANALYSIS"]
        self.assertTrue(available)
        base = {"recommendation_ids": [available[0]["id"]],
                "input_revision": planner["input_revision"],
                "profile_id": "SPECIMEN_SMALL",
                "idempotency_key": "83000000-0000-4000-8000-000000000021"}
        time.sleep(0.04)
        malformed = dict(base, recommendation_ids="not-an-array")
        self.assertEqual(self.mutate("/api/v1/plans", malformed)[0], 400)
        time.sleep(0.04)
        stale = dict(base, input_revision="stale")
        self.assertEqual(self.mutate("/api/v1/plans", stale)[0], 409)
        time.sleep(0.04)
        first = self.mutate("/api/v1/plans", base)
        self.assertEqual(first[0], 202, first[2])
        time.sleep(0.04)
        second = self.mutate("/api/v1/plans", base)
        self.assertEqual(second[0], 202, second[2])
        self.assertEqual(json.loads(first[2])["plan_id"],
                         json.loads(second[2])["plan_id"])

    def test_report_preview_generation_download_and_limits(self):
        import time
        snapshot=json.loads((self.workspace/"core-snapshot.json").read_text())
        node_id=snapshot["nodes"][0]["id"]
        request={"object_ids":[node_id],"title":"Rapport SPECIMEN",
                 "comment":"Commentaire humain éè <inerte>","profile":"MINIMAL",
                 "sections":["evidence","timeline","infrastructure"]}
        self.assertEqual(self.request_raw("POST","/api/v1/reports/preview",request,
                                         origin=self.server.origin)[0],403)
        time.sleep(0.04)
        malformed=dict(request,object_ids="bad")
        self.assertEqual(self.mutate("/api/v1/reports/preview",malformed)[0],400)
        time.sleep(0.04)
        foreign=dict(request,object_ids=["evidence:00000000-0000-4000-8000-000000000000"])
        self.assertEqual(self.mutate("/api/v1/reports/preview",foreign)[0],409)
        time.sleep(0.04)
        status,_,body=self.mutate("/api/v1/reports/preview",request)
        self.assertEqual(status,200,body)
        document=json.loads(body);self.assertEqual(document["profile"],"MINIMAL")
        previews=list((self.workspace/".labfy/reports/previews").iterdir())
        before={path.name:path.stat().st_mtime_ns for path in previews}
        self.assertEqual(self.request_raw("GET","/api/v1/reports/preview",
                                         cookie=self.cookie)[0],404)
        self.assertEqual(before,{path.name:path.stat().st_mtime_ns for path in previews})
        time.sleep(0.04)
        status,_,body=self.mutate("/api/v1/reports",{
            "preview_revision":document["revision"],
            "idempotency_key":"j9-api-report-0001"})
        self.assertEqual(status,202,body);report_id=json.loads(body)["report_id"]
        for _ in range(100):
            time.sleep(0.03)
            status,_,body=self.request_raw("GET",f"/api/v1/reports/{report_id}",cookie=self.cookie)
            if json.loads(body)["state"]!="GENERATING":break
        self.assertEqual(json.loads(body)["state"],"READY",body)
        changed=dict(document,title="Document incompatible")
        with self.assertRaisesRegex(ValueError,"autre document"):
            self.server.start_report(changed,"j9-api-report-0001")
        for name in ("report.json","report.html","report.pdf","manifest.json","NOTICE.txt"):
            status,headers,data=self.request_raw("GET",f"/api/v1/reports/{report_id}/{name}",cookie=self.cookie)
            self.assertEqual(status,200,(name,data));self.assertTrue(data)
            self.assertIn("attachment",headers["Content-Disposition"])
        self.assertEqual(self.request_raw("GET",f"/api/v1/reports/{report_id}/../Enquete.sqlite",cookie=self.cookie)[0],404)
        status,_,data=self.request_raw("HEAD",f"/api/v1/reports/{report_id}/report.pdf",cookie=self.cookie)
        self.assertEqual(status,200);self.assertEqual(data,b"")
        evidence=next(item for item in snapshot["nodes"] if item["object_kind"]=="evidence")
        stale_request=dict(request,object_ids=[evidence["id"]],title="Aperçu périssable")
        time.sleep(0.04);status,_,body=self.mutate("/api/v1/reports/preview",stale_request)
        self.assertEqual(status,200,body);stale=json.loads(body)
        import sqlite3
        connection=sqlite3.connect(self.workspace/"Enquete.sqlite")
        connection.execute("UPDATE preuves SET description = ? WHERE id = ?",
                           ("mutation SPECIMEN après aperçu",evidence["object_id"]))
        connection.commit();connection.close();time.sleep(0.04)
        status,_,_=self.mutate("/api/v1/reports",{
            "preview_revision":stale["revision"],"idempotency_key":"j9-stale-0001"})
        self.assertEqual(status,409)


if __name__ == "__main__":
    unittest.main()
