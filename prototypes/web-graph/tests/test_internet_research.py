import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from internet_research import InternetResearch, SearchHit
from privacy_egress import (EgressStatus, PrivacyEgressSupervisor, TransportResponse)


class FakeSearchProvider:
    def __init__(self, hits):
        self.hits = hits
        self.calls = []

    def search(self, query, limit, egress):
        self.calls.append((query, limit, egress))
        return self.hits


class FakeTransport:
    def __init__(self):
        self.calls = []

    def request(self, method, url, headers, timeout_seconds, max_body_bytes):
        self.calls.append((method, url, headers, timeout_seconds, max_body_bytes))
        return TransportResponse(200, {"Content-Type": "text/html"},
                                 b"<p>SPECIMEN untrusted</p>", "93.184.216.34")


class InternetResearchTest(unittest.TestCase):
    def test_search_query_with_shell_syntax_is_passed_as_inert_data(self):
        provider = FakeSearchProvider([
            SearchHit("SPECIMEN", "https://example.com/result", "untrusted prompt", 1)
        ])
        egress = PrivacyEgressSupervisor(privacy_transport=FakeTransport())
        research = InternetResearch(egress, provider)
        query = "SPECIMEN; $(touch /tmp/NEVER)"
        result = research.search(query, limit=3)
        self.assertEqual(result.status, "SUCCESS")
        self.assertEqual(provider.calls, [(query, 3, egress)])
        self.assertEqual(len(result.hits), 1)
        self.assertEqual(result.provenance["content_trust"], "UNTRUSTED")

    def test_search_drops_ssrf_results_and_bounds_content(self):
        provider = FakeSearchProvider([
            SearchHit("private", "http://127.0.0.1/admin", "x", 1),
            SearchHit("t" * 700, "https://example.com/", "s" * 5000, 2),
        ])
        result = InternetResearch(
            PrivacyEgressSupervisor(privacy_transport=FakeTransport()), provider
        ).search("SPECIMEN")
        self.assertEqual(len(result.hits), 1)
        self.assertEqual(len(result.hits[0].title), 512)
        self.assertEqual(len(result.hits[0].snippet), 4096)

    def test_fetch_uses_only_injected_privacy_transport(self):
        transport = FakeTransport()
        research = InternetResearch(PrivacyEgressSupervisor(privacy_transport=transport))
        result = research.fetch("https://example.com/SPECIMEN", max_body_bytes=1024)
        self.assertEqual(result.status, EgressStatus.SUCCESS)
        self.assertEqual(len(transport.calls), 1)
        self.assertFalse(result.provenance["direct_fallback"])

    def test_missing_provider_is_explicit(self):
        result = InternetResearch(PrivacyEgressSupervisor()).search("SPECIMEN")
        self.assertEqual(result.status, "SEARCH_PROVIDER_UNAVAILABLE")
        self.assertEqual(result.hits, ())

    def test_provider_is_not_called_when_tor_is_unavailable(self):
        provider = FakeSearchProvider([])
        result = InternetResearch(PrivacyEgressSupervisor(), provider).search("SPECIMEN")
        self.assertEqual(result.status, "PRIVACY_EGRESS_UNAVAILABLE")
        self.assertEqual(provider.calls, [])
        self.assertFalse(result.provenance["direct_fallback"])


if __name__ == "__main__":
    unittest.main()
