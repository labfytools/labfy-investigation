"""Contrat borné des outils et agent de démonstration déterministe."""

from __future__ import annotations

import copy
import json
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
        ("investigation.search", "Rechercher dans l'enquête", "query"),
        ("graph.get_node", "Lire un objet du graphe", "object_id"),
        ("graph.get_neighbors", "Lire les voisins d'un objet", "object_id"),
        ("evidence.get_summary", "Lire le résumé d'une preuve", "object_id"),
        ("evidence.read_excerpt", "Lire un extrait borné", "object_id"),
        ("provenance.trace", "Remonter la provenance", "object_id"),
        ("jobs.get", "Lire les jobs", "job_id"),
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
            },
            "output_contract": "labfy.agent_tool_result.v1",
            "action_class": (
                "PREPARE_AUTHORIZATION" if authorization_required else "READ_ONLY"
            ),
            "network_contact": "NONE",
            "authorization_requirement": (
                "HUMAN_GRANT" if authorization_required else "NONE"
            ),
            "risk_class": "MODERATE" if authorization_required else "LOW",
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
        if schema_kind == "research":
            return {
                "selection_ids": {"type": "array", "maxItems": 8},
                "question": {"type": "string", "maxLength": 512},
                "exclusions": {"type": "array", "maxItems": 8},
            }
        return {}

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
        if tool_id == "research.get_state" and arguments:
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
        ):
            raise AgentGatewayError("Question de recherche invalide")

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
