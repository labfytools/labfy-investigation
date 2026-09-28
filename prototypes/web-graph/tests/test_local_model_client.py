import json
import sys
import threading
import unittest
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from local_model_client import (LocalModelClient, LocalModelResponseError,
                                LocalModelUnavailable)


class FakeModelHandler(BaseHTTPRequestHandler):
    response_status = 200
    response_type = "application/json; charset=utf-8"
    response_value = {"choices": [{"message": {"content": "model-output"}}]}
    requests = []

    def do_POST(self):
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length)
        type(self).requests.append((self.path, self.headers, body))
        encoded = json.dumps(type(self).response_value).encode()
        self.send_response(type(self).response_status)
        self.send_header("Content-Type", type(self).response_type)
        self.send_header("Content-Length", str(len(encoded)))
        self.end_headers()
        self.wfile.write(encoded)

    def log_message(self, _format, *_args):
        pass


class LocalModelClientTest(unittest.TestCase):
    def setUp(self):
        FakeModelHandler.response_status = 200
        FakeModelHandler.response_type = "application/json; charset=utf-8"
        FakeModelHandler.response_value = {
            "choices": [{"message": {"content": "model-output"}}]
        }
        FakeModelHandler.requests = []
        self.server = ThreadingHTTPServer(("127.0.0.1", 0), FakeModelHandler)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.endpoint = f"http://127.0.0.1:{self.server.server_port}"

    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(timeout=2)

    def test_posts_openai_compatible_request(self):
        client = LocalModelClient(self.endpoint, "SPECIMEN-model", timeout=1)
        content = client.complete([{"role": "user", "content": "SPECIMEN"}])
        self.assertEqual(content, "model-output")
        path, headers, body = FakeModelHandler.requests[0]
        self.assertEqual(path, "/v1/chat/completions")
        self.assertEqual(headers["Content-Type"], "application/json")
        value = json.loads(body)
        self.assertEqual(value["model"], "SPECIMEN-model")
        self.assertFalse(value["stream"])

    def test_accepts_long_local_inference_timeout_but_keeps_a_hard_ceiling(self):
        client = LocalModelClient(self.endpoint, "model", timeout=120)
        self.assertEqual(client.timeout, 120.0)
        with self.assertRaises(ValueError):
            LocalModelClient(self.endpoint, "model", timeout=120.1)

    def test_accepts_only_canonical_loopback_endpoint(self):
        valid = ["http://127.0.0.1:1234", "http://[::1]:1234/"]
        invalid = [
            "https://127.0.0.1:1234", "http://localhost:1234",
            "http://0.0.0.0:1234", "http://127.0.0.1",
            "http://user@127.0.0.1:1234", "http://127.0.0.1:1234/path",
            "http://127.0.0.1:1234?token=x", "http://127.0.0.1:1234/#fragment",
            "HTTP://127.0.0.1:1234", "http://127.0.0.1:1234\n",
        ]
        for endpoint in valid:
            LocalModelClient(endpoint, "model")
        for endpoint in invalid:
            with self.subTest(endpoint=endpoint), self.assertRaises(ValueError):
                LocalModelClient(endpoint, "model")

    def test_rejects_redirect_bad_content_type_and_large_body(self):
        client = LocalModelClient(self.endpoint, "model", timeout=1,
                                  max_response_bytes=1024)
        FakeModelHandler.response_status = 302
        with self.assertRaises(LocalModelUnavailable):
            client.complete([{"role": "user", "content": "x"}])
        FakeModelHandler.response_status = 200
        FakeModelHandler.response_type = "text/plain"
        with self.assertRaises(LocalModelResponseError):
            client.complete([{"role": "user", "content": "x"}])
        FakeModelHandler.response_type = "application/json"
        FakeModelHandler.response_value = {"padding": "x" * 2048}
        with self.assertRaises(LocalModelResponseError):
            client.complete([{"role": "user", "content": "x"}])

    def test_rejects_malformed_openai_response(self):
        FakeModelHandler.response_value = {"choices": []}
        with self.assertRaises(LocalModelResponseError):
            LocalModelClient(self.endpoint, "model", timeout=1).complete(
                [{"role": "user", "content": "x"}]
            )


if __name__ == "__main__":
    unittest.main()
