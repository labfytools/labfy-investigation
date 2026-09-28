import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from privacy_egress import (EgressMode, EgressStatus, PrivacyEgressSupervisor,
                            TransportResponse, URLValidationError)


class FakeTransport:
    def __init__(self, responses):
        self.responses = list(responses)
        self.calls = []

    def request(self, method, url, headers, timeout_seconds, max_body_bytes):
        self.calls.append((method, url, headers, timeout_seconds, max_body_bytes))
        return self.responses.pop(0)


class PrivacyEgressTest(unittest.TestCase):
    def test_blocks_ssrf_address_families_and_non_http_schemes(self):
        blocked = [
            "http://127.0.0.1/", "http://[::1]/", "http://10.0.0.1/",
            "http://169.254.169.254/latest/meta-data/", "http://224.0.0.1/",
            "http://192.0.2.1/", "file:///etc/passwd", "http://localhost/",
            "http://metadata.google.internal/",
        ]
        for url in blocked:
            with self.subTest(url=url), self.assertRaises(URLValidationError):
                PrivacyEgressSupervisor.validate_url(url)

    def test_privacy_unavailable_has_no_direct_fallback(self):
        result = PrivacyEgressSupervisor().fetch("https://example.com/")
        self.assertEqual(result.status, EgressStatus.PRIVACY_EGRESS_UNAVAILABLE)
        self.assertEqual(result.provenance["reason"], "PRIVACY_EGRESS_UNAVAILABLE")
        self.assertFalse(result.provenance["direct_fallback"])

    def test_fake_privacy_fetch_is_bounded_and_has_provenance(self):
        transport = FakeTransport([
            TransportResponse(200, {"Content-Type": "text/plain", "Set-Cookie": "secret"},
                              b"SPECIMEN", "93.184.216.34")
        ])
        supervisor = PrivacyEgressSupervisor(privacy_transport=transport)
        result = supervisor.fetch("https://example.com/item?q=SPECIMEN")
        self.assertEqual(result.status, EgressStatus.SUCCESS)
        self.assertEqual(result.body, b"SPECIMEN")
        self.assertNotIn("set-cookie", result.headers)
        self.assertEqual(result.provenance["mode"], "PRIVACY_TOR")
        self.assertFalse(result.provenance["direct_fallback"])
        self.assertEqual(len(transport.calls), 1)

    def test_redirect_to_private_address_is_rejected_before_second_request(self):
        transport = FakeTransport([
            TransportResponse(302, {"Location": "http://169.254.169.254/metadata"}, b"",
                              "93.184.216.34")
        ])
        result = PrivacyEgressSupervisor(privacy_transport=transport).fetch(
            "https://example.com/redirect"
        )
        self.assertEqual(result.status, EgressStatus.REJECTED)
        self.assertEqual(result.provenance["reason"], "REDIRECT_REJECTED")
        self.assertEqual(len(transport.calls), 1)

    def test_loopback_requires_explicit_mode_and_transport(self):
        transport = FakeTransport([
            TransportResponse(200, {}, b"SPECIMEN", "127.0.0.1")
        ])
        supervisor = PrivacyEgressSupervisor(loopback_transport=transport)
        denied = supervisor.fetch("http://127.0.0.1:8080/")
        allowed = supervisor.fetch(
            "http://127.0.0.1:8080/", mode=EgressMode.LOOPBACK_EXPLICIT
        )
        self.assertEqual(denied.status, EgressStatus.PRIVACY_EGRESS_UNAVAILABLE)
        self.assertEqual(allowed.status, EgressStatus.SUCCESS)


if __name__ == "__main__":
    unittest.main()
