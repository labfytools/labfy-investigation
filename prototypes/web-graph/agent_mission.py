"""Mission d'agent bornée, démarrée et redimensionnée explicitement par un humain."""

from __future__ import annotations

import copy
import uuid

from investigation_context import InvestigationContextError, validate_object_ref


MISSION_CONTRACT = "labfy.agent_mission.v1"
ADMISSION_CONTRACT = "labfy.agent_mission_admission.v1"
MAX_SCOPE_REFS = 32
MAX_PIVOTS = 16
MAX_CONTACTS = 25
MAX_DURATION_SECONDS = 3600
MAX_TOOL_CALLS = 64
ALLOWED_MISSION_RISKS = {"LOCAL_READ_ONLY", "PASSIVE_PUBLIC"}
ACTIVE_RISKS = {"PUBLIC_ACTIVE", "AUTHORIZED_INTRUSIVE", "PROHIBITED"}
NETWORK_PROFILES = {"OFFLINE", "PASSIVE_PUBLIC"}


class AgentMissionError(ValueError):
    """La mission, son scope ou son budget a été refusé."""


def _text(value, field, maximum):
    if (
        not isinstance(value, str)
        or not value.strip()
        or len(value) > maximum
        or "\x00" in value
    ):
        raise AgentMissionError(f"{field} invalide")
    return value


def _limit(value, field, maximum, *, allow_zero=False):
    minimum = 0 if allow_zero else 1
    if isinstance(value, bool) or not isinstance(value, int) or not minimum <= value <= maximum:
        raise AgentMissionError(f"{field} hors limites")
    return value


class AgentMission:
    """Porte le scope et les compteurs ; la Policy reste une dépendance externe."""

    def __init__(self, workspace_id, mission_id, specification, ownership_resolver):
        self._workspace_id = workspace_id
        self._mission_id = mission_id
        self._ownership_resolver = ownership_resolver
        self._goal = specification["goal"]
        self._scoped_refs = specification["scoped_refs"]
        self._pivots = specification["pivots"]
        self._allowed_risk_classes = specification["allowed_risk_classes"]
        self._network_profile = specification["network_profile"]
        self._limits = specification["limits"]
        self._used = {"contacts": 0, "duration_seconds": 0, "tool_calls": 0}
        self._attempt_sequence = 0

    @classmethod
    def start_human(
        cls,
        selected_workspace_id,
        specification,
        ownership_resolver,
        human_start_check,
    ):
        """Démarre seulement après preuve injectée d'une intention humaine explicite."""
        workspace_id = _text(selected_workspace_id, "Workspace sélectionné", 128)
        if not callable(ownership_resolver) or not callable(human_start_check):
            raise AgentMissionError("Callbacks de démarrage requis")
        expected = {
            "goal", "scoped_refs", "pivots", "allowed_risk_classes",
            "network_profile", "max_contacts", "max_duration_seconds",
            "max_tool_calls",
        }
        if not isinstance(specification, dict) or set(specification) != expected:
            raise AgentMissionError("Spécification de mission invalide")
        safe = cls._validate_specification(workspace_id, specification, ownership_resolver)
        # CONTRACT: ce callback appartient à la frontière humaine ; le texte du but ne peut le remplacer.
        try:
            human_confirmed = human_start_check(workspace_id, copy.deepcopy(safe))
        except Exception as error:
            raise AgentMissionError("Confirmation humaine impossible") from error
        if human_confirmed is not True:
            raise AgentMissionError("Démarrage humain explicite requis")
        return cls(workspace_id, str(uuid.uuid4()), safe, ownership_resolver)

    @staticmethod
    def _validate_specification(workspace_id, value, ownership_resolver):
        risk_classes = value["allowed_risk_classes"]
        if (
            not isinstance(risk_classes, list)
            or not risk_classes
            or len(risk_classes) > len(ALLOWED_MISSION_RISKS)
            or len(set(risk_classes)) != len(risk_classes)
            or not set(risk_classes) <= ALLOWED_MISSION_RISKS
        ):
            # INVARIANT: une mission V1 ne transforme jamais une classe active en droit d'action.
            raise AgentMissionError("Classes de risque de mission invalides")
        network_profile = value["network_profile"]
        if network_profile not in NETWORK_PROFILES:
            raise AgentMissionError("Profil réseau invalide")
        if network_profile == "OFFLINE" and "PASSIVE_PUBLIC" in risk_classes:
            raise AgentMissionError("Le profil hors ligne interdit le réseau passif")
        max_contacts = _limit(
            value["max_contacts"], "Nombre de contacts", MAX_CONTACTS,
            allow_zero=network_profile == "OFFLINE",
        )
        if network_profile == "OFFLINE" and max_contacts != 0:
            raise AgentMissionError("Une mission hors ligne ne contacte aucun service")
        return {
            "goal": _text(value["goal"], "But de mission", 1024),
            "scoped_refs": AgentMission._owned_refs(
                workspace_id, value["scoped_refs"], MAX_SCOPE_REFS, ownership_resolver
            ),
            "pivots": AgentMission._owned_refs(
                workspace_id, value["pivots"], MAX_PIVOTS, ownership_resolver
            ),
            "allowed_risk_classes": sorted(risk_classes),
            "network_profile": network_profile,
            "limits": {
                "contacts": max_contacts,
                "duration_seconds": _limit(
                    value["max_duration_seconds"],
                    "Durée de mission",
                    MAX_DURATION_SECONDS,
                ),
                "tool_calls": _limit(
                    value["max_tool_calls"], "Appels d'outils", MAX_TOOL_CALLS
                ),
            },
        }

    @staticmethod
    def _owned_refs(workspace_id, values, maximum, ownership_resolver):
        if not isinstance(values, list) or not values or len(values) > maximum:
            raise AgentMissionError("Scope de mission hors limites")
        safe = []
        for value in values:
            try:
                ref = validate_object_ref(value)
                owned = ownership_resolver(workspace_id, dict(ref))
            except Exception as error:
                raise AgentMissionError("Vérification du scope impossible") from error
            if owned is not True:
                raise AgentMissionError("Référence hors du workspace")
            safe.append(ref)
        if len({item["object_id"] for item in safe}) != len(safe):
            raise AgentMissionError("Référence de scope dupliquée")
        return safe

    def snapshot(self):
        """Retourne une copie sérialisable du scope et des budgets."""
        remaining = {
            key: self._limits[key] - self._used[key]
            for key in self._limits
        }
        return {
            "contract": MISSION_CONTRACT,
            "mission_id": self._mission_id,
            "workspace_id": self._workspace_id,
            "goal": self._goal,
            "scoped_refs": copy.deepcopy(self._scoped_refs),
            "pivots": copy.deepcopy(self._pivots),
            "allowed_risk_classes": list(self._allowed_risk_classes),
            "network_profile": self._network_profile,
            "limits": dict(self._limits),
            "used": dict(self._used),
            "remaining": remaining,
        }

    def rescope(self, scoped_refs, pivots, human_rescope_check):
        """Remplace le scope après contrôle d'appartenance et confirmation humaine dédiée."""
        if not callable(human_rescope_check):
            raise AgentMissionError("Confirmation humaine de rescope requise")
        new_scope = self._owned_refs(
            self._workspace_id, scoped_refs, MAX_SCOPE_REFS, self._ownership_resolver
        )
        new_pivots = self._owned_refs(
            self._workspace_id, pivots, MAX_PIVOTS, self._ownership_resolver
        )
        proposal = {"scoped_refs": new_scope, "pivots": new_pivots}
        try:
            confirmed = human_rescope_check(self.snapshot(), copy.deepcopy(proposal))
        except Exception as error:
            raise AgentMissionError("Confirmation du rescope impossible") from error
        if confirmed is not True:
            raise AgentMissionError("Rescope humain explicite requis")
        self._scoped_refs = new_scope
        self._pivots = new_pivots
        return self.snapshot()

    def admit_attempt(self, attempt, policy_admission):
        """Consomme un budget seulement après une décision Policy injectée pour cette tentative."""
        if not callable(policy_admission):
            raise AgentMissionError("Admission Policy requise")
        expected = {
            "risk_class", "network_contact", "object_refs", "contacts",
            "duration_seconds", "tool_calls",
        }
        if not isinstance(attempt, dict) or set(attempt) != expected:
            raise AgentMissionError("Tentative de mission invalide")
        risk_class = attempt["risk_class"]
        if risk_class in ACTIVE_RISKS or risk_class not in self._allowed_risk_classes:
            raise AgentMissionError("Classe de risque non admise par la mission")
        network_contact = attempt["network_contact"]
        if not isinstance(network_contact, bool):
            raise AgentMissionError("Indicateur de contact réseau invalide")
        if network_contact and (
            risk_class != "PASSIVE_PUBLIC" or self._network_profile != "PASSIVE_PUBLIC"
        ):
            raise AgentMissionError("Contact réseau incompatible avec la mission")
        refs = self._owned_refs(
            self._workspace_id, attempt["object_refs"], MAX_SCOPE_REFS,
            self._ownership_resolver,
        )
        allowed_ids = {
            item["object_id"] for item in self._scoped_refs + self._pivots
        }
        if not {item["object_id"] for item in refs} <= allowed_ids:
            raise AgentMissionError("Tentative hors du scope de mission")
        requested = {
            "contacts": _limit(
                attempt["contacts"], "Contacts de tentative", MAX_CONTACTS,
                allow_zero=True,
            ),
            "duration_seconds": _limit(
                attempt["duration_seconds"], "Durée de tentative",
                MAX_DURATION_SECONDS, allow_zero=True,
            ),
            "tool_calls": _limit(
                attempt["tool_calls"], "Appels de tentative", MAX_TOOL_CALLS,
                allow_zero=True,
            ),
        }
        if not network_contact and requested["contacts"] != 0:
            raise AgentMissionError("Contacts déclarés sans contact réseau")
        if any(self._used[key] + requested[key] > self._limits[key] for key in requested):
            raise AgentMissionError("Budget de mission épuisé")
        safe_attempt = {
            "risk_class": risk_class,
            "network_contact": network_contact,
            "object_refs": refs,
            **requested,
        }
        try:
            admitted = policy_admission(self.snapshot(), copy.deepcopy(safe_attempt))
        except Exception as error:
            raise AgentMissionError("Décision Policy impossible") from error
        if admitted is not True:
            raise AgentMissionError("Tentative refusée par Policy")
        for key, amount in requested.items():
            self._used[key] += amount
        self._attempt_sequence += 1
        return {
            "contract": ADMISSION_CONTRACT,
            "mission_id": self._mission_id,
            "attempt_sequence": self._attempt_sequence,
            "admitted": True,
            "policy_checked": True,
            "object_refs": copy.deepcopy(refs),
            "consumed": requested,
            "remaining": self.snapshot()["remaining"],
        }
