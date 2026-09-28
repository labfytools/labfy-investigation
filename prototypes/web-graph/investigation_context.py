"""Contexte borné d'enquête et corrélations déterministes candidates."""

from __future__ import annotations

import ipaddress
import re
import uuid
from collections import defaultdict
from urllib.parse import urlsplit, urlunsplit


CONTEXT_CONTRACT = "labfy.investigation_context.v1"
CORRELATION_CONTRACT = "labfy.correlation_candidates.v1"
MAX_ENTITY_TYPES = 32
MAX_SELECTION_REFS = 32
MAX_RECENT_ACTIVITY = 20
MAX_ACTIVITY_REFS = 8
MAX_CORRELATION_OBJECTS = 256
MAX_ATTRIBUTES_PER_OBJECT = 32

_OBJECT_NAMESPACE = re.compile(r"^[a-z][a-z0-9_.-]{0,31}$")
_DOMAIN = re.compile(r"^(?=.{1,253}$)(?:[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?\.)+[a-z0-9]{2,63}$")
_EMAIL = re.compile(r"^[^\s@]{1,64}@[^\s@]{1,253}$")
_USERNAME = re.compile(r"^[a-z0-9_.-]{2,64}$")
_BIC = re.compile(r"^[A-Z]{6}[A-Z0-9]{2}(?:[A-Z0-9]{3})?$")
_HASH = re.compile(r"^(?:[0-9a-f]{32}|[0-9a-f]{40}|[0-9a-f]{64}|[0-9a-f]{96}|[0-9a-f]{128})$")
_STRUCTURED = re.compile(r"^[A-Za-z0-9][A-Za-z0-9:._/-]{1,127}$")
_RAW_PATH = re.compile(r"(?:file://|(?:^|\s)/(?!/)[^\s]+|[A-Za-z]:\\)")
_ALLOWED_CORRELATION_KINDS = {
    "email", "domain", "phone", "username", "url", "ip", "iban", "bic",
    "hash", "structured_identifier", "provenance", "observation",
}


class InvestigationContextError(ValueError):
    """Entrée refusée à la frontière du contexte d'enquête."""


def validate_object_ref(value):
    """Retourne une copie d'une référence d'objet stable et fermée."""
    if not isinstance(value, dict) or set(value) != {"object_id"}:
        raise InvestigationContextError("Référence d'objet invalide")
    object_id = value["object_id"]
    if not isinstance(object_id, str) or not object_id or len(object_id) > 96:
        raise InvestigationContextError("Identifiant d'objet invalide")
    parts = object_id.split(":", 1)
    if len(parts) == 2 and not _OBJECT_NAMESPACE.fullmatch(parts[0]):
        raise InvestigationContextError("Espace de noms d'objet invalide")
    try:
        uuid.UUID(parts[-1])
    except (ValueError, AttributeError) as error:
        raise InvestigationContextError("Identifiant d'objet invalide") from error
    return {"object_id": object_id}


def _bounded_text(value, field, maximum, *, allow_empty=False):
    if not isinstance(value, str) or "\x00" in value or len(value) > maximum:
        raise InvestigationContextError(f"{field} invalide")
    if not allow_empty and not value.strip():
        raise InvestigationContextError(f"{field} invalide")
    return value


def _display_text(value, field, maximum):
    value = _bounded_text(value, field, maximum)
    if _RAW_PATH.search(value):
        raise InvestigationContextError(f"{field} contient un chemin interdit")
    return value


def _bounded_count(value, field):
    if isinstance(value, bool) or not isinstance(value, int) or value < 0 or value > 2**63 - 1:
        raise InvestigationContextError(f"{field} invalide")
    return value


def _owned_ref(workspace_id, value, ownership_resolver):
    ref = validate_object_ref(value)
    try:
        owned = ownership_resolver(workspace_id, dict(ref))
    except Exception as error:
        raise InvestigationContextError("Vérification d'appartenance impossible") from error
    if owned is not True:
        raise InvestigationContextError("Objet absent de l'espace sélectionné")
    return ref


class InvestigationContext:
    """Construit une vue structurée sans lire directement stockage ou fichiers."""

    def __init__(self, workspace_resolver, ownership_resolver):
        if not callable(workspace_resolver) or not callable(ownership_resolver):
            raise InvestigationContextError("Resolvers de workspace et d'appartenance requis")
        self._workspace_resolver = workspace_resolver
        self._ownership_resolver = ownership_resolver

    def build(self, selected_workspace_id, snapshot):
        """Valide le snapshot du workspace explicitement sélectionné."""
        _bounded_text(selected_workspace_id, "Workspace sélectionné", 128)
        expected = {
            "workspace_id", "title", "revision", "counts", "entity_types",
            "current_graph_selection", "recent_activity",
        }
        if not isinstance(snapshot, dict) or set(snapshot) != expected:
            raise InvestigationContextError("Snapshot de contexte invalide")
        workspace_id = _bounded_text(snapshot["workspace_id"], "Workspace", 128)
        if workspace_id != selected_workspace_id:
            raise InvestigationContextError("Le snapshot appartient à un autre workspace")
        try:
            selected = self._workspace_resolver(workspace_id)
        except Exception as error:
            raise InvestigationContextError("Vérification du workspace impossible") from error
        if selected is not True:
            raise InvestigationContextError("Workspace non sélectionné ou inaccessible")

        counts = snapshot["counts"]
        if not isinstance(counts, dict) or set(counts) != {
            "evidence", "objects", "observations",
        }:
            raise InvestigationContextError("Compteurs de contexte invalides")
        safe_counts = {
            key: _bounded_count(counts[key], f"Compteur {key}")
            for key in ("evidence", "objects", "observations")
        }
        revision = snapshot["revision"]
        if not isinstance(revision, (str, int)) or isinstance(revision, bool):
            raise InvestigationContextError("Révision invalide")
        if isinstance(revision, str):
            revision = _bounded_text(revision, "Révision", 128)
        elif revision < 0:
            raise InvestigationContextError("Révision invalide")

        entity_types = snapshot["entity_types"]
        if not isinstance(entity_types, list) or len(entity_types) > MAX_ENTITY_TYPES:
            raise InvestigationContextError("Types d'entités hors limites")
        safe_types = sorted({
            _display_text(item, "Type d'entité", 64) for item in entity_types
        }, key=str.casefold)
        selection = self._validate_refs(
            workspace_id, snapshot["current_graph_selection"], MAX_SELECTION_REFS
        )
        activity = self._validate_activity(workspace_id, snapshot["recent_activity"])
        return {
            "contract": CONTEXT_CONTRACT,
            "workspace_id": workspace_id,
            "title": _display_text(snapshot["title"], "Titre", 256),
            "revision": revision,
            "counts": safe_counts,
            "entity_types": safe_types,
            "current_graph_selection": selection,
            "recent_activity": activity,
        }

    def _validate_refs(self, workspace_id, values, maximum):
        if not isinstance(values, list) or len(values) > maximum:
            raise InvestigationContextError("Références hors limites")
        refs = [_owned_ref(workspace_id, value, self._ownership_resolver) for value in values]
        if len({item["object_id"] for item in refs}) != len(refs):
            raise InvestigationContextError("Référence d'objet dupliquée")
        return refs

    def _validate_activity(self, workspace_id, values):
        if not isinstance(values, list):
            raise InvestigationContextError("Activité récente invalide")
        safe = []
        # CONTRACT: l'appelant peut fournir davantage d'activité ; seule la fenêtre récente bornée sort.
        for item in values[:MAX_RECENT_ACTIVITY]:
            if not isinstance(item, dict) or set(item) != {
                "timestamp", "kind", "summary", "object_refs",
            }:
                raise InvestigationContextError("Entrée d'activité invalide")
            safe.append({
                "timestamp": _bounded_text(item["timestamp"], "Horodatage", 64),
                "kind": _display_text(item["kind"], "Type d'activité", 64),
                "summary": _display_text(item["summary"], "Résumé d'activité", 512),
                "object_refs": self._validate_refs(
                    workspace_id, item["object_refs"], MAX_ACTIVITY_REFS
                ),
            })
        return safe


def _normalize(kind, value, scheme=None):
    if not isinstance(value, str) or not value.strip() or len(value) > 2048 or "\x00" in value:
        return None
    raw = value.strip()
    if kind == "email":
        normalized = raw.casefold()
        if not _EMAIL.fullmatch(normalized):
            return None
        local, domain = normalized.rsplit("@", 1)
        domain = _normalize("domain", domain)
        return f"{local}@{domain}" if domain else None
    if kind == "domain":
        try:
            normalized = raw.rstrip(".").encode("idna").decode("ascii").lower()
        except UnicodeError:
            return None
        return normalized if _DOMAIN.fullmatch(normalized) else None
    if kind == "phone":
        normalized = re.sub(r"[\s().-]", "", raw)
        return normalized if re.fullmatch(r"\+[1-9][0-9]{7,14}", normalized) else None
    if kind == "username":
        normalized = raw.removeprefix("@").casefold()
        return normalized if _USERNAME.fullmatch(normalized) else None
    if kind == "url":
        try:
            parsed = urlsplit(raw)
            host = parsed.hostname.encode("idna").decode("ascii").lower() if parsed.hostname else ""
            port = parsed.port
        except (UnicodeError, ValueError):
            return None
        if parsed.scheme.lower() not in {"http", "https"} or not host:
            return None
        default_port = port is None or (parsed.scheme.lower(), port) in {("http", 80), ("https", 443)}
        netloc = host if default_port else f"{host}:{port}"
        path = parsed.path or "/"
        return urlunsplit((parsed.scheme.lower(), netloc, path, parsed.query, ""))
    if kind == "ip":
        try:
            return str(ipaddress.ip_address(raw))
        except ValueError:
            return None
    if kind == "iban":
        normalized = re.sub(r"\s", "", raw).upper()
        return normalized if re.fullmatch(r"[A-Z]{2}[0-9]{2}[A-Z0-9]{11,30}", normalized) else None
    if kind == "bic":
        normalized = raw.upper()
        return normalized if _BIC.fullmatch(normalized) else None
    if kind == "hash":
        normalized = raw.lower()
        return normalized if _HASH.fullmatch(normalized) else None
    if kind == "structured_identifier":
        if not isinstance(scheme, str) or not re.fullmatch(r"[a-z][a-z0-9_.-]{1,31}", scheme):
            return None
        return f"{scheme}:{raw}" if _STRUCTURED.fullmatch(raw) else None
    if kind in {"provenance", "observation"}:
        try:
            return validate_object_ref({"object_id": raw})["object_id"]
        except InvestigationContextError:
            return None
    return None


def find_correlation_candidates(objects):
    """Retourne des rapprochements exacts ; aucun résultat n'est persisté ni confirmé."""
    if not isinstance(objects, list) or len(objects) > MAX_CORRELATION_OBJECTS:
        raise InvestigationContextError("Corpus de corrélation hors limites")
    indexed = defaultdict(list)
    for item in objects:
        if not isinstance(item, dict) or set(item) != {"object_ref", "attributes"}:
            raise InvestigationContextError("Objet de corrélation invalide")
        object_ref = validate_object_ref(item["object_ref"])
        attributes = item["attributes"]
        if not isinstance(attributes, list) or len(attributes) > MAX_ATTRIBUTES_PER_OBJECT:
            raise InvestigationContextError("Attributs de corrélation hors limites")
        for attribute in attributes:
            if not isinstance(attribute, dict) or not {"kind", "value", "source_refs"} <= set(attribute):
                raise InvestigationContextError("Attribut de corrélation invalide")
            if set(attribute) - {"kind", "value", "source_refs", "scheme"}:
                raise InvestigationContextError("Champ de corrélation inattendu")
            kind = attribute["kind"]
            if kind not in _ALLOWED_CORRELATION_KINDS:
                raise InvestigationContextError("Type de corrélation interdit")
            normalized = _normalize(kind, attribute["value"], attribute.get("scheme"))
            if normalized is None:
                continue
            source_refs = attribute["source_refs"]
            if not isinstance(source_refs, list) or not source_refs or len(source_refs) > 8:
                raise InvestigationContextError("Sources de corrélation invalides")
            safe_sources = tuple(validate_object_ref(ref)["object_id"] for ref in source_refs)
            indexed[(kind, normalized)].append((object_ref["object_id"], safe_sources))

    candidates = []
    for (kind, normalized), matches in sorted(indexed.items()):
        by_object = defaultdict(set)
        for object_id, sources in matches:
            by_object[object_id].update(sources)
        object_ids = sorted(by_object)
        for index, left in enumerate(object_ids):
            for right in object_ids[index + 1:]:
                source_ids = sorted(by_object[left] | by_object[right])
                candidates.append({
                    "object_refs": [{"object_id": left}, {"object_id": right}],
                    "reason": {"kind": kind, "normalized_value": normalized},
                    "source_refs": [{"object_id": item} for item in source_ids],
                    "confidence_kind": "EXACT_NORMALIZED_MATCH",
                })
    return {"contract": CORRELATION_CONTRACT, "candidates": candidates}
