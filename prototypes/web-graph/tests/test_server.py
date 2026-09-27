import json
import sys
import threading
import subprocess
import tempfile
import unittest
from http.client import HTTPConnection
from pathlib import Path

from jsonschema import validate

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from fixtures import generated_snapshot, scenario_events, scenario_snapshot
from server import Handler, PrototypeServer, load_core_snapshot, load_jobs_snapshot


class ServerTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = PrototypeServer(("127.0.0.1", 0), Handler)
        cls.port = cls.server.server_port
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()
        cls.thread.join()

    def request(self, method, path, headers=None):
        connection = HTTPConnection("127.0.0.1", self.port, timeout=5)
        request_headers = headers or {"Host": f"127.0.0.1:{self.port}"}
        connection.request(method, path, headers=request_headers)
        response = connection.getresponse()
        body = response.read()
        response_headers = dict(response.getheaders())
        status = response.status
        connection.close()
        return status, response_headers, body

    def test_snapshot_contract_sizes_and_latest_revision(self):
        expected_counts = (("demo", 14), ("100", 100), ("1000", 1000), ("5000", 5000))
        for size, count in expected_counts:
            status, _headers, body = self.request("GET", f"/api/v1/snapshot?size={size}")
            data = json.loads(body)
            self.assertEqual(status, 200)
            self.assertEqual(len(data["nodes"]), count)
            self.assertTrue(data["investigation"]["synthetic"])
            self.assertEqual(len({node["id"] for node in data["nodes"]}), count)

        latest = json.loads(self.request("GET", "/api/v1/snapshot?size=demo")[2])
        initial = json.loads(
            self.request("GET", "/api/v1/snapshot?size=demo&revision=initial")[2]
        )
        self.assertEqual(initial["revision"], 1)
        self.assertEqual(latest["revision"], 3)
        self.assertEqual(latest, scenario_snapshot())

        schema_path = Path(__file__).resolve().parents[1] / "contracts/snapshot-v1.schema.json"
        schema = json.loads(schema_path.read_text())
        validate(initial, schema)
        validate(latest, schema)

    def test_graph_integrity_parallel_edges_and_capability_references(self):
        data = generated_snapshot("demo")
        nodes = {node["id"]: node for node in data["nodes"]}
        self.assertTrue(
            all(edge["source"] in nodes and edge["target"] in nodes for edge in data["edges"])
        )
        self.assertGreaterEqual(
            sum(
                edge["source"] == "person-a" and edge["target"] == "username"
                for edge in data["edges"]
            ),
            2,
        )
        for capability in data["capabilities"]:
            node = nodes[capability["node_id"]]
            self.assertEqual(capability["object_ref"]["object_kind"], node["object_kind"])
            self.assertEqual(capability["object_ref"]["object_id"], node["object_id"])

    def test_security_static_root_and_read_only_methods(self):
        self.assertEqual(
            self.request("GET", "/api/v1/snapshot", {"Host": "evil.test"})[0],
            403,
        )
        self.assertEqual(
            self.request(
                "GET",
                "/api/v1/snapshot",
                {
                    "Host": f"127.0.0.1:{self.port}",
                    "Origin": "http://evil.test",
                },
            )[0],
            403,
        )
        self.assertEqual(self.request("POST", "/api/v1/snapshot")[0], 405)
        self.assertEqual(self.request("GET", "/../../etc/passwd")[0], 404)
        self.assertIn(
            "default-src 'self'",
            self.request("GET", "/")[1]["Content-Security-Policy"],
        )

    def test_sse_scenario_reconnect_unknown_cursor_and_normal_end(self):
        for cursor, event_id, revision in (("0", "1", 2), ("1", "2", 3), ("2", "3", 3)):
            status, headers, body = self.request(
                "GET",
                "/api/v1/events",
                {"Host": f"127.0.0.1:{self.port}", "Last-Event-ID": cursor},
            )
            self.assertEqual(status, 200)
            self.assertEqual(headers["Content-Type"], "text/event-stream; charset=utf-8")
            self.assertIn(f"id: {event_id}".encode(), body)
            self.assertIn(f'"revision":{revision}'.encode(), body)

        self.assertEqual(
            self.request(
                "GET",
                "/api/v1/events",
                {"Host": f"127.0.0.1:{self.port}", "Last-Event-ID": "3"},
            )[0],
            204,
        )
        unknown_body = self.request(
            "GET",
            "/api/v1/events",
            {"Host": f"127.0.0.1:{self.port}", "Last-Event-ID": "999"},
        )[2]
        self.assertIn(b'"kind":"resync_required"', unknown_body)
        self.assertIn(b"id: 3", unknown_body)
        self.assertEqual(self.request("GET", "/api/v1/events?after=invalid")[0], 400)

    def test_event_and_fixture_contracts(self):
        event_schema_path = Path(__file__).resolve().parents[1] / "contracts/event-v1.schema.json"
        event_schema = json.loads(event_schema_path.read_text())
        for event in scenario_events():
            validate(event, event_schema)

        status, headers, body = self.request("GET", "/fixture.js")
        self.assertEqual(status, 200)
        self.assertEqual(headers["Content-Type"], "text/javascript; charset=utf-8")
        self.assertIn(b'"revision":1', body)
        self.assertIn(b"labfy.web_graph.snapshot.v1", body)

    def test_invalid_snapshot_parameters(self):
        self.assertEqual(self.request("GET", "/api/v1/snapshot?size=42")[0], 400)
        self.assertEqual(
            self.request("GET", "/api/v1/snapshot?revision=unknown")[0],
            400,
        )


class CoreSnapshotServerTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix="labfy-core-server-")
        prototype = Path(__file__).resolve().parents[1]
        repository = prototype.parents[1]
        generator = repository / "tools/core_graph_demo"
        result = subprocess.run(
            [str(generator), "--output-dir", cls.temporary.name],
            cwd=repository,
            text=True,
            capture_output=True,
            timeout=30,
            check=False,
        )
        if result.returncode != 0:
            raise RuntimeError(result.stderr)
        cls.snapshot = load_core_snapshot(
            Path(cls.temporary.name) / "core-snapshot.json"
        )
        cls.server = PrototypeServer(
            ("127.0.0.1", 0), Handler, core_snapshot=cls.snapshot
        )
        cls.port = cls.server.server_port
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()
        cls.thread.join()
        cls.temporary.cleanup()

    def request(self, path):
        connection = HTTPConnection("127.0.0.1", self.port, timeout=5)
        connection.request("GET", path, headers={"Host": f"127.0.0.1:{self.port}"})
        response = connection.getresponse()
        body = response.read()
        status = response.status
        connection.close()
        return status, body

    def test_core_contract_and_no_fixture_fallback(self):
        status, body = self.request("/api/v1/snapshot")
        self.assertEqual(status, 200)
        data = json.loads(body)
        self.assertEqual(data["contract"], "labfy.web_graph.snapshot.v2")
        self.assertEqual(data["origin"], "core")
        self.assertEqual(len(data["nodes"]), 9)
        self.assertEqual(len(data["edges"]), 8)
        self.assertNotIn(b"SPECIMEN-WEB-GRAPH", body)
        schema_path = Path(__file__).resolve().parents[1] / "contracts/snapshot-v2.schema.json"
        validate(data, json.loads(schema_path.read_text()))
        self.assertEqual(self.request("/api/v1/events")[0], 204)
        self.assertEqual(self.request("/api/v1/snapshot?size=demo")[0], 400)

    def test_core_fixture_module_uses_generated_snapshot(self):
        status, body = self.request("/fixture.js")
        self.assertEqual(status, 200)
        self.assertIn(b"labfy.web_graph.snapshot.v2", body)
        self.assertIn(CORE_GRAPH_EMAIL_BYTES, body)


CORE_GRAPH_EMAIL_BYTES = b"20000000-0000-4000-8000-000000000031"


class EmlSnapshotContractTest(unittest.TestCase):
    def test_eml_snapshot_v3_is_generated_by_c_and_validates(self):
        prototype = Path(__file__).resolve().parents[1]
        repository = prototype.parents[1]
        with tempfile.TemporaryDirectory(prefix="labfy-eml-server-") as directory:
            result = subprocess.run(
                [
                    str(repository / "tools/eml_graph_demo"),
                    "--output-dir",
                    directory,
                ],
                cwd=repository,
                text=True,
                capture_output=True,
                timeout=45,
                check=False,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            snapshot = load_core_snapshot(Path(directory) / "core-snapshot.json")
            schema_path = prototype / "contracts/snapshot-v3.schema.json"
            validate(snapshot, json.loads(schema_path.read_text()))
            self.assertEqual(snapshot["contract"], "labfy.web_graph.snapshot.v3")
            self.assertEqual(
                len([node for node in snapshot["nodes"] if node["object_kind"] == "extraction"]),
                2,
            )
            self.assertTrue(
                any(
                    node.get("details", {}).get("value_normalized")
                    == "elodie@atelier.test"
                    for node in snapshot["nodes"]
                )
            )


class LocalJobsRecoveryTest(unittest.TestCase):
    def test_real_crash_recovery_and_read_only_export(self):
        prototype = Path(__file__).resolve().parents[1]
        repository = prototype.parents[1]
        with tempfile.TemporaryDirectory(prefix="labfy-j5-server-") as directory:
            # CONTRACT: le crash synthétique n'existe jamais dans le binaire de
            # production ; ce scénario doit viser l'exécutable de test dédié.
            command = repository / "tools/local-jobs-test"
            result = subprocess.run(
                [str(command), "demo", "--workspace", directory],
                cwd=repository,
                text=True,
                capture_output=True,
                timeout=45,
                check=False,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            jobs_path = Path(directory) / "jobs-snapshot.json"
            jobs = load_jobs_snapshot(jobs_path)
            self.assertEqual([job["state"] for job in jobs["jobs"]],
                             ["COMPLETED", "COMPLETED"])
            self.assertIn("recovered_after_publish",
                          [job["diagnostic"] for job in jobs["jobs"]])
            before = subprocess.check_output(
                ["sqlite3", str(Path(directory) / "Enquete.sqlite"),
                 "SELECT count(*) FROM extractions;"], text=True
            ).strip()
            rerun = subprocess.run(
                [str(command), "run", "--workspace", directory],
                cwd=repository, capture_output=True, text=True, check=False
            )
            self.assertEqual(rerun.returncode, 0, rerun.stderr)
            after = subprocess.check_output(
                ["sqlite3", str(Path(directory) / "Enquete.sqlite"),
                 "SELECT count(*) FROM extractions;"], text=True
            ).strip()
            self.assertEqual(before, "2")
            self.assertEqual(after, before)

            server = PrototypeServer(("127.0.0.1", 0), Handler,
                                     jobs_path=jobs_path)
            thread = threading.Thread(target=server.serve_forever, daemon=True)
            thread.start()
            try:
                connection = HTTPConnection("127.0.0.1", server.server_port, timeout=5)
                connection.request("GET", "/api/v1/jobs",
                                   headers={"Host": f"127.0.0.1:{server.server_port}"})
                response = connection.getresponse()
                body = json.loads(response.read())
                self.assertEqual(response.status, 200)
                self.assertEqual(body["contract"], "labfy.local_jobs.snapshot.v1")
                connection.close()
            finally:
                server.shutdown()
                server.server_close()
                thread.join()
            snapshot = load_core_snapshot(Path(directory) / "core-snapshot.json")
            schema_path = prototype / "contracts/snapshot-v3.schema.json"
            validate(snapshot, json.loads(schema_path.read_text()))
            self.assertEqual(snapshot["contract"], "labfy.web_graph.snapshot.v3")
            self.assertEqual(
                len([node for node in snapshot["nodes"] if node["object_kind"] == "extraction"]),
                2,
            )


if __name__ == "__main__":
    unittest.main()
