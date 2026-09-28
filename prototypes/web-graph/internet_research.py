"""Façade de recherche Web bornée sur providers et egress explicitement injectés."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Mapping, Protocol, Sequence

from privacy_egress import EgressMode, FetchResult, PrivacyEgressSupervisor, URLValidationError


@dataclass(frozen=True)
class SearchHit:
    title: str
    url: str
    snippet: str
    provider_rank: int


class SearchProvider(Protocol):
    def search(self, query: str, limit: int,
               egress: PrivacyEgressSupervisor) -> Sequence[SearchHit]:
        """Recherche via l'egress Privacy transmis, sans transport parallèle."""


@dataclass(frozen=True)
class SearchResult:
    status: str
    hits: tuple[SearchHit, ...]
    provenance: Mapping[str, object]


class InternetResearch:
    """N'accorde aucune autorité aux snippets, pages ou instructions retournées."""

    def __init__(self, egress: PrivacyEgressSupervisor,
                 search_provider: SearchProvider | None = None):
        self._egress = egress
        self._search_provider = search_provider

    def search(self, query: str, limit: int = 10) -> SearchResult:
        if not isinstance(query, str) or query != query.strip() or not query:
            return self._search_failure("QUERY_REJECTED")
        if len(query) > 512 or any(ord(character) < 0x20 for character in query):
            return self._search_failure("QUERY_REJECTED")
        if not 1 <= limit <= 50:
            return self._search_failure("LIMIT_REJECTED")
        if self._search_provider is None:
            return self._search_failure("SEARCH_PROVIDER_UNAVAILABLE")
        if not self._egress.is_available(EgressMode.PRIVACY_TOR):
            return self._search_failure("PRIVACY_EGRESS_UNAVAILABLE")
        try:
            raw_hits = self._search_provider.search(query, limit, self._egress)
        except (OSError, ValueError):
            return self._search_failure("SEARCH_PROVIDER_ERROR")
        hits = []
        for index, hit in enumerate(raw_hits):
            if index >= limit or not isinstance(hit, SearchHit):
                break
            try:
                url = self._egress.validate_url(hit.url, allow_loopback=False)
            except URLValidationError:
                continue
            title = self._bounded_text(hit.title, 512)
            snippet = self._bounded_text(hit.snippet, 4096)
            if title is None or snippet is None:
                continue
            hits.append(SearchHit(title, url, snippet, hit.provider_rank))
        return SearchResult(
            "SUCCESS", tuple(hits),
            {
                "contract": "labfy.internet_search.v1",
                "query": query,
                "requested_limit": limit,
                "returned_count": len(hits),
                "content_trust": "UNTRUSTED",
                "provider_injected": True,
                "direct_fallback": False,
            },
        )

    def fetch(self, url: str, method: str = "GET", max_body_bytes: int = 1024 * 1024,
              mode: EgressMode = EgressMode.PRIVACY_TOR) -> FetchResult:
        return self._egress.fetch(
            url, method=method, mode=mode, max_body_bytes=max_body_bytes
        )

    @staticmethod
    def _bounded_text(value: str, limit: int) -> str | None:
        if not isinstance(value, str) or "\x00" in value:
            return None
        return value[:limit]

    @staticmethod
    def _search_failure(reason: str) -> SearchResult:
        return SearchResult(
            reason, (),
            {
                "contract": "labfy.internet_search.v1",
                "reason": reason,
                "content_trust": "UNTRUSTED",
                "provider_injected": False,
                "direct_fallback": False,
            },
        )
