"""Contrat borné des outils et agent de démonstration déterministe."""

from __future__ import annotations

import copy
import json
import re
import threading
import time
import uuid
from collections import OrderedDict, deque


class AgentGatewayError(ValueError):
    """Erreur publique stable du protocole HTTP."""

    def __init__(self, message, *, code="agent_tool_invalid", status=400):
        super().__init__(message)
        self.code = code
        self.status = status


class AgentGateway:
    """Etat éphémère par serveur ; les callbacks restent propriétaires métier."""

    CONTRACT = "labfy.agent_tool_protocol.v1"
    MAX_CALLS = 64
    MAX_EVENTS = 128
    MAX_RESULT_BYTES = 256 * 1024
    MAX_INPUT_BYTES = 8 * 1024
    MAX_TURNS = 32

    # CONTRACT: ces descripteurs viennent du backend. L'UI ne déduit ni le
    # risque ni l'admission depuis un nom d'outil.
    _TOOLS = (
        ("investigation.get_overview", "Lire le contexte borné de l'enquête", "empty"),
        ("investigation.search", "Rechercher dans l'enquête", "query"),
        ("investigation.find_correlations", "Chercher des corrélations déterministes", "empty"),
        ("graph.get_node", "Lire un objet du graphe", "object_id"),
        ("graph.get_neighbors", "Lire les voisins d'un objet", "object_id"),
        ("evidence.get_summary", "Lire le résumé d'une preuve", "object_id"),
        ("evidence.read_excerpt", "Lire un extrait borné", "object_id"),
        ("provenance.trace", "Remonter la provenance", "object_id"),
        ("jobs.get", "Lire les jobs", "job_id"),
        ("tool.catalog", "Lister les outils locaux bornés", "empty"),
        ("tool.docs.read", "Lire l'aide locale bornée d'un outil", "tool_id"),
        ("sandbox.exec", "Exécuter un outil offline dans le sandbox", "sandbox"),
        ("agent.propose", "Créer une proposition d'enquête candidate", "proposal"),
        ("web.fetch", "Lire une ressource publique via Privacy Tor", "web_fetch"),
        ("research.get_state", "Lire l'état de recherche", "empty"),
        ("research.prepare", "Préparer une recherche", "research"),
    )

    def __init__(self, executors):
        expected_tools = {tool_id for tool_id, _, _ in self._TOOLS}
        if set(executors) != expected_tools:
            raise ValueError("Exécuteurs du gateway incomplets")

        self._executors = dict(executors)
        self._calls = OrderedDict()
        self._results = OrderedDict()
        self._turns = OrderedDict()
        self._events = deque(maxlen=self.MAX_EVENTS)
        self._sequence = 0
        self._lock = threading.RLock()

    def catalog(self):
        tools = []
        for tool_id, description, schema_kind in self._TOOLS:
            tools.append(self._catalog_tool(tool_id, description, schema_kind))
        return {
            "contract": self.CONTRACT,
            "version": 1,
            "transport": "HTTP_POLLING",
            "tools": tools,
        }

    def _catalog_tool(self, tool_id, description, schema_kind):
        authorization_required = tool_id == "research.prepare"
        mission_required = tool_id in {"sandbox.exec", "agent.propose", "web.fetch"}
        network_contact = "PRIVACY_TOR" if tool_id == "web.fetch" else "NONE"
        if tool_id == "web.fetch":
            risk_class = "PASSIVE_PUBLIC"
        elif tool_id in {"sandbox.exec", "agent.propose"}:
            risk_class = "LOCAL_READ_ONLY"
        else:
            risk_class = "MODERATE" if authorization_required else "LOW"
        return {
            "tool_id": tool_id,
            "tool_version": "1",
            "capability_id": f"labfy.capability.{tool_id}.v1",
            "capability_version": "1",
            "title": description,
            "description": description,
            "input_schema": {
                "type": "object",
                "properties": self._input_properties(schema_kind),
                "required": self._input_required(schema_kind),
                # CONTRACT: the model sees the exact closed input shape that
                # _validate accepts. The catalog never advertises permissive
                # fields that the capability would later reject.
                "additionalProperties": False,
            },
            "output_contract": "labfy.agent_tool_result.v1",
            "action_class": (
                "PREPARE_AUTHORIZATION" if authorization_required
                else "MISSION_TOOL" if mission_required
                else "READ_ONLY"
            ),
            "network_contact": network_contact,
            "authorization_requirement": (
                "HUMAN_GRANT" if authorization_required
                else "MISSION_SCOPE" if mission_required
                else "NONE"
            ),
            "risk_class": risk_class,
            "cost_class": "BOUNDED",
            "availability": "AVAILABLE",
            "unavailable_reason": None,
        }

    @staticmethod
    def _input_properties(schema_kind):
        if schema_kind == "query":
            return {"query": {"type": "string", "maxLength": 512}}
        if schema_kind == "object_id":
            return {"object_id": {"type": "string", "format": "uuid"}}
        if schema_kind == "job_id":
            return {"job_id": {"type": "string", "format": "uuid"}}
        if schema_kind == "tool_id":
            return {"tool_id": {"type": "string", "maxLength": 80}}
        if schema_kind == "sandbox":
            return {
                "tool_id": {"type": "string", "maxLength": 80},
                "object_id": {"type": "string", "maxLength": 96},
                "arguments": {
                    "type": "array", "minItems": 1, "maxItems": 16,
                    "items": {"type": "string", "maxLength": 512},
                },
            }
        if schema_kind == "proposal":
            object_ref = {
                "type": "object",
                "properties": {"object_id": {"type": "string", "maxLength": 96}},
                "required": ["object_id"],
                "additionalProperties": False,
            }
            return {
                "title": {"type": "string", "maxLength": 160},
                "reason": {"type": "string", "maxLength": 1024},
                "object_refs": {
                    "type": "array", "minItems": 1, "maxItems": 16,
                    "items": object_ref,
                },
                "suggested_capability": {"type": "string", "maxLength": 128},
                "risk_class": {
                    "type": "string",
                    "enum": ["LOCAL_READ_ONLY", "PASSIVE_PUBLIC", "PUBLIC_ACTIVE",
                             "AUTHORIZED_INTRUSIVE", "PROHIBITED"],
                },
                "expected_value": {"type": "string", "maxLength": 512},
            }
        if schema_kind == "web_fetch":
            return {
                "object_id": {"type": "string", "maxLength": 96},
                "url": {"type": "string", "maxLength": 4096},
                "method": {"type": "string", "enum": ["GET", "HEAD"]},
            }
        if schema_kind == "research":
            return {
                "selection_ids": {
                    "type": "array", "minItems": 1, "maxItems": 8,
                    "items": {"type": "string"},
                },
                "question": {"type": "string", "maxLength": 512},
                "exclusions": {
                    "type": "array", "maxItems": 8,
                    "items": {"type": "string"},
                },
            }
        return {}

    @staticmethod
    def _input_required(schema_kind):
        if schema_kind == "query":
            return ["query"]
        if schema_kind == "object_id":
            return ["object_id"]
        if schema_kind == "job_id":
            return ["job_id"]
        if schema_kind == "tool_id":
            return ["tool_id"]
        if schema_kind == "sandbox":
            return ["tool_id", "object_id", "arguments"]
        if schema_kind == "proposal":
            return [
                "title", "reason", "object_refs", "suggested_capability",
                "risk_class", "expected_value",
            ]
        if schema_kind == "web_fetch":
            return ["object_id", "url", "method"]
        if schema_kind == "research":
            return ["selection_ids", "question", "exclusions"]
        return []

    def start_turn(self, scope, value):
        if not isinstance(value, dict) or set(value) != {"objective"}:
            raise AgentGatewayError("Enveloppe de turn inattendue")

        objective = value["objective"]
        if (
            not isinstance(objective, str)
            or not objective.strip()
            or len(objective) > 512
            or "\x00" in objective
        ):
            raise AgentGatewayError("Objectif invalide")

        turn_id = str(uuid.uuid4())
        with self._lock:
            self._turns[(scope, turn_id)] = {
                "turn_id": turn_id,
                "objective": objective,
                "state": "RUNNING",
                "object_refs": [],
            }
            self._trim(self._turns, self.MAX_TURNS)
            self._event(
                scope,
                "agent.turn.started",
                turn_id,
                None,
                [],
                {"objective": objective},
            )
            self._event(
                scope,
                "agent.plan.updated",
                turn_id,
                None,
                [],
                {"steps": [
                    "investigation.search",
                    "graph.get_node",
                    "graph.get_neighbors",
                    "provenance.trace",
                    "research.prepare",
                ]},
            )

        # WHY: le scénario est fixe et ne traite jamais l'objectif comme une
        # instruction de contrôle ; il ne peut donc ni appeler un faux outil
        # ni s'autoriser.
        search = self._call(
            scope,
            turn_id,
            "investigation.search",
            {"query": objective},
            [],
        )
        refs = self.result(scope, search["result_id"])["object_refs"][:1]
        if refs:
            object_id = refs[0]["object_id"]
            for tool_id in (
                "graph.get_node",
                "graph.get_neighbors",
                "provenance.trace",
            ):
                self._call(scope, turn_id, tool_id, {"object_id": object_id}, refs)

        prepared = self._call(
            scope,
            turn_id,
            "research.prepare",
            {
                "selection_ids": [item["object_id"] for item in refs],
                "question": objective,
                "exclusions": [],
            },
            refs,
        )
        prepared_result = self.result(scope, prepared["result_id"])
        with self._lock:
            turn = self._turns[(scope, turn_id)]
            turn.update({
                "state": "AUTHORIZATION_REQUIRED",
                "object_refs": refs,
                "research_result_id": prepared["result_id"],
            })
            self._event(
                scope,
                "agent.authorization.required",
                turn_id,
                prepared["call_id"],
                refs,
                {
                    "reason": "human_grant_required",
                    "result_id": prepared["result_id"],
                },
            )
        return {
            "contract": self.CONTRACT,
            "turn_id": turn_id,
            "state": "AUTHORIZATION_REQUIRED",
            # CONTRACT: this is the same prepared C plan returned by the
            # gateway result, not a frontend reconstruction or model output.
            "research_plan": prepared_result["output"],
        }

    def resume_turn(self, scope, turn_id):
        self._uuid(turn_id, "Identifiant de turn invalide")
        with self._lock:
            turn = self._turns.get((scope, turn_id))
            if turn is None:
                raise AgentGatewayError(
                    "Turn inconnu dans cet espace",
                    code="agent_turn_unknown",
                    status=404,
                )
            if turn["state"] != "AUTHORIZATION_REQUIRED":
                raise AgentGatewayError(
                    "Turn non repris",
                    code="agent_turn_not_waiting",
                    status=409,
                )
            refs = copy.deepcopy(turn["object_refs"])

        self._call(scope, turn_id, "research.get_state", {}, refs)
        with self._lock:
            turn["state"] = "COMPLETED"
            self._event(
                scope,
                "agent.turn.completed",
                turn_id,
                None,
                refs,
                {"summary": "Recherche autorisée et état relu par le backend."},
            )
        return {
            "contract": self.CONTRACT,
            "turn_id": turn_id,
            "state": "COMPLETED",
        }

    def call(self, scope, value):
        required = {
            "turn_id",
            "tool_id",
            "input",
            "context",
            "object_refs",
            "idempotency_key",
        }
        if not isinstance(value, dict) or set(value) != required:
            raise AgentGatewayError("Champs d'appel d'outil inattendus")

        self._uuid(value["turn_id"], "Identifiant de turn invalide")
        return self._call(
            scope,
            value["turn_id"],
            value["tool_id"],
            value["input"],
            value["object_refs"],
            value["idempotency_key"],
            value["context"],
        )

    def _call(self, scope, turn_id, tool_id, arguments, object_refs,
              key=None, context=None):
        if key is None:
            key = str(uuid.uuid4())
        self._validate(tool_id, arguments, object_refs, key, context)

        fingerprint = json.dumps(
            [turn_id, tool_id, arguments, object_refs, context],
            sort_keys=True,
            separators=(",", ":"),
        )
        call_key = (scope, key)
        with self._lock:
            previous = self._calls.get(call_key)
            if previous is not None:
                if previous["fingerprint"] != fingerprint:
                    raise AgentGatewayError(
                        "Clé d'idempotence déjà utilisée pour un autre appel",
                        code="agent_tool_idempotency_conflict",
                        status=409,
                    )
                response = copy.deepcopy(previous["response"])
                response["replayed"] = True
                return response

            self._trim(self._calls, self.MAX_CALLS - 1)
            if len(self._calls) >= self.MAX_CALLS:
                raise AgentGatewayError(
                    "Capacité d'appels temporairement épuisée",
                    code="agent_tool_capacity_exhausted",
                    status=429,
                )

            call_id = str(uuid.uuid4())
            result_id = str(uuid.uuid4())
            self._calls[call_key] = {
                "fingerprint": fingerprint,
                "response": None,
            }
            self._event(
                scope,
                "agent.tool.requested",
                turn_id,
                call_id,
                object_refs,
                {"tool_id": tool_id},
            )
            self._event(
                scope,
                "agent.tool.started",
                turn_id,
                call_id,
                object_refs,
                {"tool_id": tool_id},
            )

        try:
            output = self._executors[tool_id](copy.deepcopy(arguments), key)
            state = (
                "AUTHORIZATION_REQUIRED"
                if tool_id == "research.prepare"
                else "COMPLETED"
            )
            result_refs = object_refs or output.get("object_refs", [])
            result = {
                "contract": "labfy.agent_tool_result.v1",
                "result_id": result_id,
                "turn_id": turn_id,
                "call_id": call_id,
                "tool_id": tool_id,
                "state": state,
                "output": output,
                "object_refs": result_refs,
                "artifacts": [],
                "observations": [],
                "provenance_refs": output.get("provenance_refs", []),
                "diagnostic": None,
            }
            encoded_result = json.dumps(result, ensure_ascii=False).encode()
            if len(encoded_result) > self.MAX_RESULT_BYTES:
                raise AgentGatewayError(
                    "Résultat d'outil trop volumineux",
                    code="agent_tool_result_too_large",
                    status=413,
                )
        except AgentGatewayError:
            raise
        except Exception as error:
            result = {
                "contract": "labfy.agent_tool_result.v1",
                "result_id": result_id,
                "turn_id": turn_id,
                "call_id": call_id,
                "tool_id": tool_id,
                "state": "FAILED",
                "output": {},
                "object_refs": object_refs,
                "artifacts": [],
                "observations": [],
                "provenance_refs": [],
                "diagnostic": str(error)[:512],
            }

        response = {
            "contract": self.CONTRACT,
            "turn_id": turn_id,
            "call_id": call_id,
            "result_id": result_id,
            "tool_id": tool_id,
            "state": result["state"],
            "replayed": False,
        }
        with self._lock:
            self._calls[call_key]["response"] = response
            self._results[(scope, result_id)] = result
            self._trim(self._results, self.MAX_CALLS)
            self._event(
                scope,
                "agent.tool.completed",
                turn_id,
                call_id,
                result["object_refs"],
                {
                    "tool_id": tool_id,
                    "result_id": result_id,
                    "state": result["state"],
                },
            )
        return copy.deepcopy(response)

    def result(self, scope, result_id):
        self._uuid(result_id, "Identifiant de résultat invalide")
        with self._lock:
            value = self._results.get((scope, result_id))
            if value is None:
                raise AgentGatewayError(
                    "Résultat inconnu dans cet espace",
                    code="agent_tool_result_unknown",
                    status=404,
                )
            return copy.deepcopy(value)

    def events(self, scope, cursor):
        if (
            not isinstance(cursor, int)
            or isinstance(cursor, bool)
            or cursor < 0
        ):
            raise AgentGatewayError("Curseur d'événements invalide")

        with self._lock:
            events = [
                copy.deepcopy(event)
                for event in self._events
                if event["scope"] == scope and event["sequence"] > cursor
            ]
            for event in events:
                event.pop("scope")
            return {
                "contract": self.CONTRACT,
                "transport": "HTTP_POLLING",
                "cursor": self._sequence,
                "events": events,
            }

    def _event(self, scope, kind, turn_id, call_id, refs, payload):
        self._sequence += 1
        self._events.append({
            "contract": self.CONTRACT,
            "sequence": self._sequence,
            "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "scope": scope,
            "kind": kind,
            "turn_id": turn_id,
            "call_id": call_id,
            "object_refs": copy.deepcopy(refs),
            "payload": copy.deepcopy(payload),
        })

    def _validate(self, tool_id, arguments, refs, key, context):
        if tool_id not in self._executors:
            raise AgentGatewayError("Outil inconnu")
        self._uuid(key, "Clé d'idempotence invalide")
        if (
            not isinstance(arguments, dict)
            or not isinstance(context if context is not None else {}, dict)
            or not isinstance(refs, list)
        ):
            raise AgentGatewayError("Entrée d'outil invalide")
        encoded_arguments = json.dumps(arguments, ensure_ascii=False).encode()
        if len(encoded_arguments) > self.MAX_INPUT_BYTES:
            raise AgentGatewayError("Entrée d'outil trop volumineuse", status=413)

        self._validate_refs(refs)
        if tool_id in {
            "investigation.get_overview",
            "investigation.find_correlations",
            "tool.catalog",
            "research.get_state",
        } and arguments:
            raise AgentGatewayError("Cet outil n'accepte aucun argument")
        if tool_id == "investigation.search":
            self._validate_search(arguments)
        elif tool_id in {
            "graph.get_node",
            "graph.get_neighbors",
            "evidence.get_summary",
            "evidence.read_excerpt",
            "provenance.trace",
        }:
            self._validate_object_input(arguments)
        elif tool_id == "jobs.get":
            self._validate_job_input(arguments)
        elif tool_id == "tool.docs.read":
            self._validate_tool_id_input(arguments)
        elif tool_id == "sandbox.exec":
            self._validate_sandbox_input(arguments)
        elif tool_id == "agent.propose":
            self._validate_proposal_input(arguments)
        elif tool_id == "web.fetch":
            self._validate_web_fetch_input(arguments)
        elif tool_id == "research.prepare":
            self._validate_research_input(arguments)

    def _validate_refs(self, refs):
        for ref in refs:
            if not isinstance(ref, dict) or set(ref) != {"object_id"}:
                raise AgentGatewayError("Référence d'objet invalide")
            self._object_ref(ref["object_id"], "Référence d'objet invalide")

    @staticmethod
    def _validate_search(arguments):
        query = arguments.get("query")
        if (
            set(arguments) != {"query"}
            or not isinstance(query, str)
            or not query.strip()
            or len(query) > 512
        ):
            raise AgentGatewayError("Recherche invalide")

    def _validate_object_input(self, arguments):
        if set(arguments) != {"object_id"}:
            raise AgentGatewayError("Objet attendu")
        self._object_ref(arguments["object_id"], "UUID objet invalide")

    def _validate_job_input(self, arguments):
        if set(arguments) != {"job_id"}:
            raise AgentGatewayError("Job attendu")
        self._uuid(arguments["job_id"], "UUID job invalide")

    @staticmethod
    def _validate_tool_id_input(arguments):
        tool_id = arguments.get("tool_id")
        if (
            set(arguments) != {"tool_id"}
            or not isinstance(tool_id, str)
            or not re.fullmatch(r"[a-z][a-z0-9_.-]{2,79}", tool_id)
        ):
            raise AgentGatewayError("Identifiant d'outil invalide")

    def _validate_sandbox_input(self, arguments):
        if set(arguments) != {"tool_id", "object_id", "arguments"}:
            raise AgentGatewayError("Entrée sandbox invalide")
        self._validate_tool_id_input({"tool_id": arguments.get("tool_id")})
        self._object_ref(arguments.get("object_id"), "Objet sandbox invalide")
        argv = arguments.get("arguments")
        if (
            not isinstance(argv, list)
            or not argv
            or len(argv) > 16
            or any(
                not isinstance(item, str)
                or len(item) > 512
                or "\x00" in item
                for item in argv
            )
        ):
            raise AgentGatewayError("Arguments sandbox invalides")

    def _validate_proposal_input(self, arguments):
        expected = {
            "title", "reason", "object_refs", "suggested_capability",
            "risk_class", "expected_value",
        }
        if not isinstance(arguments, dict) or set(arguments) != expected:
            raise AgentGatewayError("Proposition invalide")
        limits = {
            "title": 160,
            "reason": 1024,
            "suggested_capability": 128,
            "expected_value": 512,
        }
        for field, limit in limits.items():
            value = arguments.get(field)
            if (
                not isinstance(value, str)
                or not value.strip()
                or len(value) > limit
                or "\x00" in value
            ):
                raise AgentGatewayError("Proposition invalide")
        if not re.fullmatch(
            r"[a-z][a-z0-9_.-]{2,127}", arguments["suggested_capability"]
        ):
            raise AgentGatewayError("Capability proposée invalide")
        if arguments.get("risk_class") not in {
            "LOCAL_READ_ONLY", "PASSIVE_PUBLIC", "PUBLIC_ACTIVE",
            "AUTHORIZED_INTRUSIVE", "PROHIBITED",
        }:
            raise AgentGatewayError("Risque proposé invalide")
        refs = arguments.get("object_refs")
        if not isinstance(refs, list) or not refs or len(refs) > 16:
            raise AgentGatewayError("Références de proposition invalides")
        self._validate_refs(refs)

    def _validate_web_fetch_input(self, arguments):
        if set(arguments) != {"object_id", "url", "method"}:
            raise AgentGatewayError("Entrée Web invalide")
        self._object_ref(arguments.get("object_id"), "Objet Web invalide")
        url = arguments.get("url")
        if (
            not isinstance(url, str)
            or not url
            or len(url) > 4096
            or "\x00" in url
            or arguments.get("method") not in {"GET", "HEAD"}
        ):
            raise AgentGatewayError("Requête Web invalide")

    def _validate_research_input(self, arguments):
        selection = arguments.get("selection_ids")
        if (
            set(arguments) != {"selection_ids", "question", "exclusions"}
            or not isinstance(selection, list)
            or not selection
            or len(selection) > 8
        ):
            raise AgentGatewayError("Sélection de recherche invalide")
        for object_id in selection:
            self._object_ref(object_id, "UUID objet invalide")

        question = arguments.get("question")
        if (
            not isinstance(question, str)
            or not question.strip()
            or len(question) > 512
            or not isinstance(arguments.get("exclusions"), list)
            or len(arguments["exclusions"]) > 8
        ):
            raise AgentGatewayError("Question de recherche invalide")
        if any(not isinstance(item, str) for item in arguments["exclusions"]):
            raise AgentGatewayError("Exclusions de recherche invalides")

    @staticmethod
    def _uuid(value, message):
        if not isinstance(value, str):
            raise AgentGatewayError(message)
        try:
            uuid.UUID(value)
        except ValueError as error:
            raise AgentGatewayError(message) from error

    @staticmethod
    def _object_ref(value, message):
        # CONTRACT: le graphe conserve son espace de noms (par ex. evidence:),
        # mais la partie identité reste un UUID validé avant toute délégation.
        if not isinstance(value, str):
            raise AgentGatewayError(message)
        AgentGateway._uuid(value.split(":", 1)[-1], message)

    @staticmethod
    def _trim(mapping, limit):
        while len(mapping) > limit:
            mapping.popitem(last=False)
