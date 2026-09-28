"""Propositions d'agent présentables, sans effet métier ni autorisation."""

from __future__ import annotations

import hashlib
import json
import re
import uuid

from investigation_context import InvestigationContextError, validate_object_ref


PROPOSAL_CONTRACT = "labfy.agent_proposal.v1"
DECISION_CONTRACT = "labfy.agent_proposal_decision.v1"
MAX_PROPOSAL_REFS = 16
RISK_CLASSES = {
    "LOCAL_READ_ONLY",
    "PASSIVE_PUBLIC",
    "PUBLIC_ACTIVE",
    "AUTHORIZED_INTRUSIVE",
    "PROHIBITED",
}
_CAPABILITY_ID = re.compile(r"^[a-z][a-z0-9_.-]{2,127}$")


class AgentProposalError(ValueError):
    """Une proposition ou sa décision ne respecte pas le contrat fermé."""


def _text(value, field, maximum):
    if (
        not isinstance(value, str)
        or not value.strip()
        or len(value) > maximum
        or "\x00" in value
    ):
        raise AgentProposalError(f"{field} invalide")
    return value


def _owned_refs(workspace_id, values, ownership_resolver):
    if not isinstance(values, list) or len(values) > MAX_PROPOSAL_REFS:
        raise AgentProposalError("Références de proposition hors limites")
    safe = []
    for value in values:
        try:
            ref = validate_object_ref(value)
            owned = ownership_resolver(workspace_id, dict(ref))
        except (InvestigationContextError, Exception) as error:
            raise AgentProposalError("Vérification de référence impossible") from error
        if owned is not True:
            raise AgentProposalError("Référence hors du workspace")
        safe.append(ref)
    if len({item["object_id"] for item in safe}) != len(safe):
        raise AgentProposalError("Référence de proposition dupliquée")
    return safe


class AgentProposalService:
    """Valide et enregistre des choix de présentation uniquement."""

    def __init__(self, ownership_resolver):
        if not callable(ownership_resolver):
            raise AgentProposalError("Resolver d'appartenance requis")
        self._ownership_resolver = ownership_resolver

    def validate(self, workspace_id, value):
        """Retourne une proposition render-friendly dépourvue d'action implicite."""
        if not isinstance(workspace_id, str) or not workspace_id or len(workspace_id) > 128:
            raise AgentProposalError("Workspace invalide")
        expected = {
            "title", "reason", "object_refs", "suggested_capability",
            "risk_class", "expected_value",
        }
        if not isinstance(value, dict) or set(value) != expected:
            raise AgentProposalError("Schéma de proposition invalide")
        capability = _text(value["suggested_capability"], "Capability", 128)
        if not _CAPABILITY_ID.fullmatch(capability):
            raise AgentProposalError("Capability suggérée invalide")
        risk_class = value["risk_class"]
        if risk_class not in RISK_CLASSES:
            raise AgentProposalError("Classe de risque invalide")
        return {
            "contract": PROPOSAL_CONTRACT,
            "title": _text(value["title"], "Titre", 160),
            "reason": _text(value["reason"], "Motif", 1024),
            "object_refs": _owned_refs(
                workspace_id, value["object_refs"], self._ownership_resolver
            ),
            "suggested_capability": capability,
            "risk_class": risk_class,
            "expected_value": _text(value["expected_value"], "Valeur attendue", 512),
            # INVARIANT: une proposition ne représente ni un fait ni une admission de policy.
            "status": "CANDIDATE",
        }

    def record_decision(self, workspace_id, proposal, decision):
        """Crée une trace de choix ; APPROVED signifie seulement « retenue par l'humain »."""
        safe_proposal = self._validated_output(workspace_id, proposal)
        if not isinstance(decision, dict) or set(decision) != {
            "decision_id", "decision", "reason", "decided_by", "decided_at",
        }:
            raise AgentProposalError("Schéma de décision invalide")
        try:
            decision_id = str(uuid.UUID(decision["decision_id"]))
        except (ValueError, TypeError, AttributeError) as error:
            raise AgentProposalError("Identifiant de décision invalide") from error
        outcome = decision["decision"]
        if outcome not in {"APPROVED", "REFUSED"}:
            raise AgentProposalError("Décision de proposition invalide")
        canonical = json.dumps(
            safe_proposal, ensure_ascii=False, sort_keys=True, separators=(",", ":")
        ).encode("utf-8")
        return {
            "contract": DECISION_CONTRACT,
            "decision_id": decision_id,
            "proposal_digest": hashlib.sha256(canonical).hexdigest(),
            "decision": outcome,
            "reason": _text(decision["reason"], "Motif de décision", 512),
            "decided_by": _text(decision["decided_by"], "Auteur de décision", 128),
            "decided_at": _text(decision["decided_at"], "Date de décision", 64),
            "object_refs": [dict(item) for item in safe_proposal["object_refs"]],
            "suggested_capability": safe_proposal["suggested_capability"],
            # CONTRACT: même APPROVED n'est jamais un grant et ne contourne pas Policy.
            "policy_grant_created": False,
            "effect": "PROPOSAL_DECISION_ONLY",
        }

    def _validated_output(self, workspace_id, proposal):
        if not isinstance(proposal, dict) or proposal.get("contract") != PROPOSAL_CONTRACT:
            raise AgentProposalError("Proposition validée attendue")
        expected = {
            "contract", "title", "reason", "object_refs", "suggested_capability",
            "risk_class", "expected_value", "status",
        }
        if set(proposal) != expected or proposal.get("status") != "CANDIDATE":
            raise AgentProposalError("Proposition validée altérée")
        return self.validate(workspace_id, {
            key: proposal[key]
            for key in (
                "title", "reason", "object_refs", "suggested_capability",
                "risk_class", "expected_value",
            )
        })
