"""Validation stricte des actions produites par le modèle local."""

from __future__ import annotations

import json


CONTRACT = "labfy.agent_model_action.v1"
MAX_ACTION_BYTES = 32 * 1024
MAX_FINAL_CHARS = 8 * 1024
MAX_ARGUMENT_DEPTH = 12


class AgentModelProtocolError(ValueError):
    """Le modèle a produit une action qui ne respecte pas le contrat."""


def _object_without_duplicates(pairs):
    value = {}
    for key, item in pairs:
        if key in value:
            raise AgentModelProtocolError("Clé JSON dupliquée")
        value[key] = item
    return value


def _validate_json_value(value, depth=0):
    if depth > MAX_ARGUMENT_DEPTH:
        raise AgentModelProtocolError("Arguments trop imbriqués")
    if value is None or isinstance(value, (str, bool, int)):
        return
    if isinstance(value, float):
        if value != value or value in (float("inf"), float("-inf")):
            raise AgentModelProtocolError("Nombre JSON non fini")
        return
    if isinstance(value, list):
        for item in value:
            _validate_json_value(item, depth + 1)
        return
    if isinstance(value, dict):
        for key, item in value.items():
            if not isinstance(key, str):
                raise AgentModelProtocolError("Clé d'argument invalide")
            _validate_json_value(item, depth + 1)
        return
    raise AgentModelProtocolError("Type d'argument JSON invalide")


def parse_model_action(raw, known_tool_ids):
    """Parse une unique action JSON et retourne un dictionnaire validé."""
    if not isinstance(raw, str):
        raise AgentModelProtocolError("Réponse modèle non textuelle")
    if len(raw.encode("utf-8")) > MAX_ACTION_BYTES:
        raise AgentModelProtocolError("Réponse modèle trop volumineuse")
    try:
        action = json.loads(
            raw,
            object_pairs_hook=_object_without_duplicates,
            parse_constant=lambda _value: (_ for _ in ()).throw(
                AgentModelProtocolError("Constante JSON invalide")
            ),
        )
    except AgentModelProtocolError:
        raise
    except (UnicodeError, json.JSONDecodeError) as error:
        raise AgentModelProtocolError("Réponse modèle non JSON") from error

    if not isinstance(action, dict):
        raise AgentModelProtocolError("Action modèle attendue")
    if action.get("contract") != CONTRACT:
        raise AgentModelProtocolError("Contrat d'action modèle invalide")

    kind = action.get("kind")
    if kind == "tool_call":
        if set(action) != {"contract", "kind", "tool_id", "arguments"}:
            raise AgentModelProtocolError("Champs tool_call inattendus")
        tool_id = action["tool_id"]
        if not isinstance(tool_id, str) or tool_id not in set(known_tool_ids):
            raise AgentModelProtocolError("Outil modèle inconnu")
        if not isinstance(action["arguments"], dict):
            raise AgentModelProtocolError("Arguments d'outil invalides")
        _validate_json_value(action["arguments"])
    elif kind == "final":
        if set(action) != {"contract", "kind", "text"}:
            raise AgentModelProtocolError("Champs final inattendus")
        text = action["text"]
        if not isinstance(text, str) or not text.strip():
            raise AgentModelProtocolError("Contenu final invalide")
        if len(text) > MAX_FINAL_CHARS or "\x00" in text:
            raise AgentModelProtocolError("Contenu final hors limites")
    else:
        # CONTRACT: aucune action d'autorisation n'existe dans le langage modèle.
        raise AgentModelProtocolError("Type d'action modèle inconnu")
    return action
