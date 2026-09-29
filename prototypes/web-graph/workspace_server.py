#!/usr/bin/env python3
"""Poste de travail J6 loopback, protégé, devant le service de commande C."""

from __future__ import annotations

import argparse
import base64
import hashlib
import hmac
import json
import mimetypes
import os
import secrets
import shutil
import signal
import socket
import subprocess
import threading
import time
import uuid
from datetime import datetime, timezone
from http import HTTPStatus
from http.cookies import SimpleCookie
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlsplit
from agent_gateway import AgentGateway, AgentGatewayError
from agent_mission import AgentMission, AgentMissionError
from agent_proposals import AgentProposalError, AgentProposalService
from agent_runtime import AgentRuntime, AgentRuntimeError
from code_change import CodeChangeError, CodeChangeService
from developer_agent import DeveloperAgent, DeveloperAgentError
from local_model_client import LocalModelClient
from local_model_supervisor import (
    LocalModelSupervisor,
    LocalModelSupervisorError,
    load_xdg_config,
)
from report_bundle import publish as publish_report, verify as verify_report, ReportError
from investigation_context import InvestigationContext, find_correlation_candidates
from internet_research import InternetResearch
from privacy_egress import EgressStatus, PodmanTorRuntime, PrivacyEgressSupervisor
from sandbox_execution import ArtifactInput, SandboxExec, SandboxStatus
from tool_registry import (
    ExecutionProfile, NetworkRequirement, RiskClass, ToolDefinition,
    ToolDocumentationBroker, ToolRegistry, ToolRegistryError,
)
from tool_provisioning import ProvisioningError, ToolProvisioningService
from tool_integration_examples import example_for as integration_example_for

ROOT = Path(__file__).resolve().parent
PUBLIC = ROOT / "public"
MAX_BODY = 4096
SESSION_TTL_SECONDS = 3600
MAX_EXPORT = 1024 * 1024
MAX_PLAN_ITEMS = 8
MAX_UPLOAD_BYTES = 4 * 1024 * 1024
MAX_STAGING_BYTES = 64 * 1024 * 1024
UPLOAD_TTL_SECONDS = 30 * 60
UPLOAD_TIMEOUT_SECONDS = 30
MAX_ACTIVE_UPLOADS = 2
MAX_SELECTION_FILES = 8
MAX_SELECTION_BYTES = 16 * 1024 * 1024
ALLOWED_CAPABILITIES = {
    "labfy.capability.eml_headers.v1",
    "labfy.capability.exif_metadata.v1",
}


class WorkspaceServer(ThreadingHTTPServer):
    daemon_threads = True
    block_on_close = False

    def __init__(self, address, handler, *, workspace: Path, bridge: Path,
                 bootstrap: str = "", library=None, instance_id=None, config_id=None,
                 session_ttl_seconds=SESSION_TTL_SECONDS,
                 research_fixture_authority=None, automatic_session=False,
                 agent_mode="deterministic-demo", agent_endpoint=None,
                 agent_model=None, agent_timeout=10.0, agent_autostart=False,
                 agent_config_path=None, public_local_origin=None,
                 tool_provisioning=None, code_changes=None, developer_agent=None):
        if (not isinstance(session_ttl_seconds, int) or
                isinstance(session_ttl_seconds, bool) or session_ttl_seconds <= 0):
            raise ValueError("Durée de session invalide")
        if public_local_origin not in (None, "http://invest.labfy"):
            raise ValueError("Origine locale publique invalide")
        super().__init__(address, handler)
        self.workspace = workspace.resolve()
        self.inactive_workspace = self.workspace
        self.bridge = bridge.resolve()
        self.authority = f"127.0.0.1:{self.server_port}"
        self.origin = f"http://{self.authority}"
        # CONTRACT: the only alternate browser origin is the explicitly chosen
        # local nginx hostname. It never authorizes a forwarded host or LAN bind.
        self.public_local_origin = public_local_origin
        # CONTRACT: le poste Web ne possède plus de parcours code. Toute
        # première navigation loopback établit seulement un cookie HttpOnly ;
        # Host, Origin et CSRF restent exigés pour chaque mutation.
        self.automatic_session = True
        self.library = library
        self.active_workspace_id = None
        # CONTRACT: the persistent lazy service must not enumerate or read the
        # configured library during server construction. Its initial generation
        # is only a placeholder until the user explicitly requests the list.
        self.library_generation = (library.snapshot(None)["generation"]
                                   if library is not None and not library.lazy else 0)
        self.instance_id = instance_id or secrets.token_urlsafe(18)
        self.config_id = config_id or hashlib.sha256(
            str(self.workspace).encode()).hexdigest()
        self.cookie_name = f"labfy_session_{self.instance_id[:12]}"
        self.session = secrets.token_urlsafe(32)
        self.csrf = secrets.token_urlsafe(32)
        self.session_lock = threading.RLock()
        # CONTRACT: la production conserve 3600 s. L'injection constructeur
        # permet seulement aux tests d'observer une vraie expiration sans route
        # d'administration ni manipulation de l'horloge du processus.
        self.session_ttl_seconds = session_ttl_seconds
        # La durée commence à la connexion, jamais au démarrage du serveur.
        self.session_deadline = 0.0
        self.worker = None
        self.worker_supervisor = None
        self.worker_supervisor_stop = threading.Event()
        self.lock = threading.Lock()
        self.workspace_lifecycle_lock = threading.RLock()
        self.command_rate_lock = threading.Lock()
        self.last_command = 0.0
        self.report_tasks = {}
        self.report_lock = threading.Lock()
        self.report_threads = set()
        self.max_report_tasks = 2
        self.upload_lock = threading.Lock()
        self.active_uploads = 0
        self.reserved_upload_bytes = 0
        self.upload_mutexes = {}
        self.agent_operation_lock = threading.RLock()
        self.agent_missions = {}
        self.agent_proposals = {}
        self.agent_operation_replays = {}
        self.library_close_replays = {}
        # CONTRACT: les magasins rootless sont ouverts uniquement sur demande.
        # Le démarrage lazy du service ne lit aucune enquête de la library.
        self._tool_provisioning = tool_provisioning
        self._code_changes = code_changes
        self.tooling_enabled = (agent_mode == "local-model" or agent_autostart or
                                tool_provisioning is not None or code_changes is not None or
                                developer_agent is not None)
        self.tooling_lock = threading.RLock()
        self.tooling_threads = set()
        self.code_change_turns = {}
        self.developer_agent = developer_agent
        self.developer_agent_client = None
        if self._tool_provisioning is not None:
            # INVARIANT: even an injected store uses the WorkspaceServer's
            # current mission/policy authority for every capability execution.
            self._tool_provisioning.policy_check = self._dynamic_capability_policy
        self.agent_proposal_service = AgentProposalService(self._agent_ref_owned)
        self.agent_tool_registry = self._build_agent_tool_registry()
        self.agent_tool_docs = ToolDocumentationBroker()
        self.agent_sandbox = SandboxExec(self.agent_tool_registry)
        self.agent_privacy_egress = None
        self.agent_internet_research = None
        # CONTRACT: cette autorité de laboratoire n'est ni une donnée UI ni un
        # réglage persistant. Le bridge C applique encore sa propre whitelist.
        self.research_fixture_authority = research_fixture_authority
        # CONTRACT: WorkspaceServer possède l'unique gateway éphémère. Les
        # callbacks réutilisent ses lectures/commandes existantes ; le gateway
        # n'accède ni à SQLite, ni au shell, ni à un stockage parallèle.
        self.agent_gateway = AgentGateway({
            "investigation.get_overview": self._agent_get_overview,
            "investigation.search": self._agent_search,
            "investigation.find_correlations": self._agent_find_correlations,
            "graph.get_node": self._agent_get_node,
            "graph.get_neighbors": self._agent_get_neighbors,
            "evidence.get_summary": self._agent_evidence_summary,
            "evidence.read_excerpt": self._agent_evidence_excerpt,
            "provenance.trace": self._agent_provenance,
            "jobs.get": self._agent_get_job,
            "tool.catalog": self._agent_tool_catalog,
            "tool.docs.read": self._agent_tool_docs_read,
            "sandbox.exec": self._agent_sandbox_exec,
            "agent.propose": self._agent_propose,
            "web.fetch": self._agent_web_fetch,
            "research.get_state": self._agent_research_state,
            "research.prepare": self._agent_prepare_research,
            "tool.provision.search": self._agent_tool_provision_search,
            "tool.provision.propose": self._agent_tool_provision_propose,
            "tool.provision.get": self._agent_tool_provision_get,
            "tool.integration.propose": self._agent_tool_integration_propose,
            "capability.execute": self._agent_capability_execute,
            "code.change.propose": self._agent_code_change_propose,
            "code.change.get": self._agent_code_change_get,
        }, capability_catalog_provider=lambda: (
            self._tool_provisioning.capability_catalog()
            if self._tool_provisioning is not None else ()))
        self.agent_runtime = None
        self.agent_runtime_reason = None
        self.local_model_supervisor = None
        # WHY: an automatic model is opt-in and uses only an XDG configuration
        # supplied by the operator. No versioned source contains a model path.
        if agent_autostart:
            try:
                config = load_xdg_config(agent_config_path)
                self.local_model_supervisor = LocalModelSupervisor(config)
                ready = self.local_model_supervisor.start()
                agent_mode = "local-model"
                agent_endpoint = ready["endpoint"]
                agent_model = ready["model"]
            except LocalModelSupervisorError as error:
                agent_mode = "local-model"
                self.agent_runtime_reason = str(error)
        self.agent_mode = agent_mode
        self.agent_model = agent_model
        if agent_mode == "local-model":
            try:
                client = LocalModelClient(agent_endpoint, agent_model, timeout=agent_timeout)
                self.developer_agent_client = client
                self.agent_runtime = AgentRuntime(
                    client,
                    self._agent_runtime_gateway,
                    self.agent_gateway.catalog()["tools"],
                    max_model_calls=16, max_tool_calls=16,
                    context_provider=self._agent_investigation_context,
                )
            except ValueError as error:
                # CONTRACT: an invalid local endpoint never causes a fallback
                # to another origin or to the deterministic model.
                self.agent_runtime_reason = str(error)
        elif agent_mode != "deterministic-demo":
            raise ValueError("Mode agent invalide")

    @property
    def tool_provisioning(self):
        with self.tooling_lock:
            if self._tool_provisioning is None:
                self._tool_provisioning = ToolProvisioningService(
                    policy_check=self._dynamic_capability_policy)
            return self._tool_provisioning

    @property
    def code_changes(self):
        with self.tooling_lock:
            if self._code_changes is None:
                state_home = Path(os.environ.get("XDG_STATE_HOME", Path.home() / ".local/state"))
                self._code_changes = CodeChangeService(
                    ROOT.parents[1], state_home / "labfy-investigation")
            return self._code_changes

    def _start_tooling_task(self, name, callback):
        """Run one bounded blocking build/model/test operation off the HTTP thread."""
        with self.tooling_lock:
            if self.tooling_threads:
                raise ValueError("Une opération toolbox/développement est déjà en cours")

            def run():
                try:
                    callback()
                finally:
                    with self.tooling_lock:
                        self.tooling_threads.discard(threading.current_thread())

            thread = threading.Thread(target=run, name=name, daemon=False)
            self.tooling_threads.add(thread)
            thread.start()

    def _resume_after_tooling(self, turn_id):
        if self.agent_runtime is None:
            return
        try:
            self.resume_agent_runtime(turn_id)
        except (AgentRuntimeError, ProvisioningError, CodeChangeError,
                AgentMissionError, ValueError):
            # CONTRACT: an operator may cancel the turn while a bounded build
            # or developer test is running. Cancellation never reopens it.
            pass

    def _ensure_developer_agent(self):
        with self.tooling_lock:
            if self.developer_agent is None:
                if self.developer_agent_client is None:
                    raise ValueError("Modèle développeur local indisponible")
                self.developer_agent = DeveloperAgent(
                    self.developer_agent_client, self.code_changes)
            return self.developer_agent

    def _tooling_call_mission(self, call_context):
        workspace_id, entry = self._current_active_agent_mission()
        if call_context.get("scope") != self.agent_scope():
            raise AgentMissionError("Scope d'appel Agent incohérent")
        return workspace_id, entry["mission"].snapshot()["mission_id"]

    def _admit_tooling_request(self):
        # CONTRACT: a toolbox or code proposal is still scoped to one object
        # chosen by the human mission. AgentMission rejects empty ref sets.
        _workspace_id, entry = self._current_active_agent_mission()
        refs = entry["mission"].snapshot()["scoped_refs"]
        if not refs:
            raise AgentMissionError("Objet de mission requis")
        return self._admit_agent_attempt("LOCAL_READ_ONLY", [refs[0]])

    def _agent_tool_provision_search(self, arguments, _key, _call_context):
        return {"packages": list(self.tool_provisioning.search(
            arguments["query"], arguments["limit"]))}

    def _agent_tool_provision_propose(self, arguments, key, call_context):
        workspace_id, mission_id = self._tooling_call_mission(call_context)
        self._admit_tooling_request()
        return self.tool_provisioning.propose(
            arguments["package"], workspace_id=workspace_id, mission_id=mission_id,
            turn_id=call_context["turn_id"], idempotency_key=key)

    def _agent_tool_provision_get(self, arguments, _key, _call_context):
        request = self.tool_provisioning.get(arguments["request_id"])
        if request.get("workspace_id") != self.agent_workspace_id():
            raise PermissionError("Demande d'un autre workspace")
        if request["state"] in {"QUARANTINED", "WAITING_INTEGRATION_APPROVAL", "ACTIVE"}:
            return {**request,
                    "documentation_tool_id": "debian." + request["package"]["package"],
                    "declarative_example_id": ("jq.username.v1"
                        if integration_example_for(request) is not None else None),
                    "code_change_example_id": "specimen.username.ui.v1"}
        return request

    def _agent_tool_integration_propose(self, arguments, key, call_context):
        workspace_id, mission_id = self._tooling_call_mission(call_context)
        request = self.tool_provisioning.get(arguments["request_id"])
        if request.get("workspace_id") != workspace_id or request.get("mission_id") != mission_id:
            raise PermissionError("Intégration hors mission")
        self._admit_tooling_request()
        proposal = arguments["proposal"]
        if proposal == {"example_id": "jq.username.v1"}:
            proposal = integration_example_for(request)
            if proposal is None:
                raise ValueError("Exemple déclaratif indisponible")
        return self.tool_provisioning.propose_integration(
            arguments["request_id"], proposal, idempotency_key=key)

    def _dynamic_node(self, object_id):
        matches = [node for node in self._agent_graph().get("nodes", [])
                   if isinstance(node, dict) and
                   object_id in {node.get("id"), node.get("object_id")}]
        if len(matches) != 1:
            raise ValueError("Objet dynamique absent ou ambigu")
        return matches[0]

    def _dynamic_capability_policy(self, capability, context):
        try:
            workspace_id, entry = self._current_active_agent_mission()
            if context["workspace_id"] != workspace_id or context["mission_id"] != \
                    entry["mission"].snapshot()["mission_id"]:
                return False
            node = self._dynamic_node(context["object_id"])
            if node.get("object_kind") != context["object_type"]:
                return False
            if capability["capability_id"] not in {
                    item["capability_id"] for item in self.tool_provisioning.capability_catalog(
                        context["object_type"])}:
                return False
            self._admit_agent_attempt("LOCAL_READ_ONLY", [{"object_id": node["id"]}])
            return True
        except (AgentMissionError, ProvisioningError, OSError, ValueError):
            return False

    def execute_dynamic_capability(self, capability_id, parameters, object_id, turn_id, key):
        workspace_id, entry = self._current_active_agent_mission()
        node = self._dynamic_node(object_id)
        matches = [item for item in self.tool_provisioning.capability_catalog()
                   if item["capability_id"] == capability_id]
        if len(matches) != 1:
            raise ValueError("Capability dynamique inactive")
        manifest = matches[0]
        object_type = node.get("object_kind")
        if object_type not in manifest["applicable_object_types"]:
            raise ValueError("Capability inapplicable à l'objet")
        parameter = manifest["input_binding"]["object_value_parameter"]
        value = node.get("label")
        if not isinstance(value, str) or not value or len(value) > 4096:
            raise ValueError("Valeur de l'objet indisponible")
        if parameter in parameters and parameters[parameter] != value:
            raise ValueError("La valeur de l'objet doit venir du backend")
        bound = {**parameters, parameter: value}
        return self.tool_provisioning.execute(capability_id, bound,
            mission_context={"workspace_id": workspace_id,
                             "mission_id": entry["mission"].snapshot()["mission_id"],
                             "turn_id": turn_id, "object_type": object_type,
                             "object_id": node["id"]},
            idempotency_key=key)

    def _agent_capability_execute(self, arguments, key, call_context):
        self._tooling_call_mission(call_context)
        node = self._dynamic_node(arguments["object_id"])
        if arguments["object_type"] != node.get("object_kind"):
            raise ValueError("Type d'objet incompatible")
        return self.execute_dynamic_capability(arguments["capability_id"],
            arguments["parameters"], node["id"], call_context["turn_id"], key)

    def _agent_code_change_propose(self, arguments, key, call_context):
        workspace_id, _mission_id = self._tooling_call_mission(call_context)
        self._admit_tooling_request()
        example = arguments.get("example_id") == "specimen.username.ui.v1"
        request_id = (arguments.get("request_id") if example else
                      arguments["integration_proposal"].get("request_id"))
        if not isinstance(request_id, str):
            raise ValueError("Intégration technique source requise")
        request = self.tool_provisioning.get(request_id)
        if request.get("workspace_id") != workspace_id:
            raise PermissionError("Intégration d'un autre workspace")
        docs = dict(self.tool_provisioning.documents(request_id))
        if example:
            path = "prototypes/web-graph/public/app.js"
            context = {
                "contract": "labfy.developer_context.v1",
                "change_id": str(uuid.uuid5(uuid.NAMESPACE_URL,
                                             request_id + ":specimen.username.ui.v1")),
                "purpose": ("Dans le worktree SPECIMEN, modifier renderActions dans "
                            "public/app.js : pour la capability focus-neighborhood sur "
                            "un nœud username, afficher Explorer ce pseudo à la place "
                            "de capability.intent. La ligne exacte à trouver est "
                            "button.textContent = capability.intent; . Remplacer cette ligne "
                            "par button.textContent = capability.id === "
                            "\"focus-neighborhood\" && node.object_kind === \"username\" "
                            "? \"Explorer ce pseudo\" : capability.intent; . Préserver "
                            "tous les autres objets et identifiants de capability."),
                "tool_docs": {"package": request["package"]["package"],
                              "help_excerpt": docs.get("help", "")[:500]},
                "integration_proposal": {"request_id": request_id,
                                         "kind": "declarative_insufficient"},
                "architecture_refs": ["docs/architecture/WEB_WORKSPACE_CONTROL.md"],
                "expected_files": [path],
                "required_tests": ["NODE_CHECK", "NODE_TEST", "FIREFOX_TARGETED",
                                   "FIREFOX_FULL", "DIFF_CHECK"],
                "affected_areas": ["Web graph action labels"],
                "risk": "LOW",
                "created_at": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
            }
        else:
            context = {**arguments, "contract": "labfy.developer_context.v1"}
            context.pop("why_declarative_insufficient", None)
            context["tool_docs"] = docs
        result = self._ensure_developer_agent().propose(context, idempotency_key=key)
        if result.get("change_id"):
            self.code_change_turns[result["change_id"]] = {
                "workspace_id": workspace_id,
                "turn_id": call_context["turn_id"],
            }
        return ({**result, "runtime_state": "CODE_CHANGE_APPROVAL_REQUIRED"}
                if result.get("state") == "WAITING_DEV_APPROVAL" else result)

    def _agent_code_change_get(self, arguments, _key, _call_context):
        binding = self.code_change_turns.get(arguments["change_id"])
        if binding is None or binding["workspace_id"] != self.agent_workspace_id():
            raise PermissionError("Changement de code d'un autre workspace")
        return self._code_change_model_summary(
            self.code_changes.get(arguments["change_id"]))

    def code_change_turn(self, change_id):
        binding = self.code_change_turns.get(change_id)
        if binding is None and self.library is None:
            return ""
        if binding is None or binding["workspace_id"] != self.agent_workspace_id():
            raise PermissionError("Changement de code d'un autre workspace")
        return binding["turn_id"]

    @staticmethod
    def _code_change_model_summary(record):
        # CONTRACT: Qwen receives durable references and bounded test states,
        # never the captured Firefox/build logs or a copy of the source diff.
        return {
            "contract": "labfy.code_change_model_summary.v1",
            "change_id": record["change_id"], "state": record["state"],
            "base_sha": record.get("base_sha"),
            "patch_digest": record.get("patch_digest"),
            "expected_files": record.get("expected_files", []),
            "tests": [{"recipe_id": item["recipe_id"],
                       "passed": item["passed"], "returncode": item["returncode"]}
                      for item in record.get("tests", [])],
            "post_apply_tests": [{"recipe_id": item["recipe_id"],
                                  "passed": item["passed"],
                                  "returncode": item["returncode"]}
                                 for item in record.get("post_apply_tests", [])],
            "preview": record.get("preview", {}),
        }

    def dynamic_capability_manifests(self):
        """Project active manifests onto verified backend graph object references."""
        workspace_id = self.agent_workspace_id()
        graph = self._agent_graph()
        manifests = self.tool_provisioning.capability_catalog()
        catalog = [{"id": item["capability_id"], "intent": item["intent"],
                    "network_contact": item["network_contact"],
                    "experimental": True}
                   for item in manifests]
        applications = []
        for node in graph.get("nodes", []):
            if not isinstance(node, dict) or not isinstance(node.get("label"), str):
                continue
            for item in manifests:
                if node.get("object_kind") not in item["applicable_object_types"]:
                    continue
                applications.append({
                    "node_id": node["id"],
                    "object_ref": {"investigation_id": workspace_id,
                                   "object_kind": node["object_kind"],
                                   "object_id": node["object_id"]},
                    "capability_id": item["capability_id"],
                    "available": item["availability"] == "AVAILABLE",
                    "reason": item["reason"],
                    "dynamic": True,
                })
                if len(applications) >= 256:
                    break
            if len(applications) >= 256:
                break
        return {"contract": "labfy.capability_manifest_list.v1",
                "catalog": catalog, "applications": applications,
                "catalog_revision": self.tool_provisioning.store.read_state()["catalog_revision"]}

    def agent_workspace_id(self):
        """Return the opaque identity of the currently opened workspace."""
        if self.library is not None:
            if self.active_workspace_id is None:
                raise AgentMissionError("Aucun workspace actif")
            return self.active_workspace_id
        context = self.context()
        if context is None:
            raise AgentMissionError("Aucun workspace actif")
        return context["investigation_id"]

    def _agent_ref_owned(self, workspace_id, ref):
        if workspace_id != self.agent_workspace_id():
            return False
        known = {
            node.get("id") for node in self._agent_graph().get("nodes", [])
            if isinstance(node, dict)
        }
        return ref["object_id"] in known

    @staticmethod
    def _agent_operation_digest(value):
        return hashlib.sha256(json.dumps(
            value, ensure_ascii=False, sort_keys=True, separators=(",", ":"),
        ).encode("utf-8")).hexdigest()

    def _agent_idempotent(self, workspace_id, operation, key, intent, callback):
        try:
            key = str(uuid.UUID(key))
        except (ValueError, TypeError, AttributeError) as error:
            raise AgentMissionError("Clé d’idempotence invalide") from error
        replay_key = (workspace_id, operation, key)
        digest = self._agent_operation_digest(intent)
        existing = self.agent_operation_replays.get(replay_key)
        if existing is not None:
            if existing[0] != digest:
                raise AgentMissionError(
                    "Clé d’idempotence déjà utilisée pour une autre intention"
                )
            return existing[1], True
        response = callback()
        self.agent_operation_replays[replay_key] = (digest, response)
        return response, False

    def agent_mission_current(self):
        workspace_id = self.agent_workspace_id()
        with self.agent_operation_lock:
            entry = self.agent_missions.get(workspace_id)
            if entry is None:
                return {
                    "contract": "labfy.agent_mission_state.v1",
                    "workspace_id": workspace_id,
                    "state": "INACTIVE",
                    "mission": None,
                    "elapsed_seconds": 0,
                }
            elapsed = max(0, int(time.monotonic() - entry["started_monotonic"]))
            return {
                "contract": "labfy.agent_mission_state.v1",
                "workspace_id": workspace_id,
                "state": entry["state"],
                "mission": entry["mission"].snapshot(),
                "elapsed_seconds": elapsed,
            }

    def start_agent_mission(self, workspace_id, specification, key, confirmed):
        self._require_agent_workspace(workspace_id)
        intent = {"specification": specification, "human_confirmed": confirmed}
        with self.agent_operation_lock:
            def start():
                current = self.agent_missions.get(workspace_id)
                if current is not None and current["state"] == "ACTIVE":
                    raise AgentMissionError("Une mission est déjà active")
                mission = AgentMission.start_human(
                    workspace_id, specification, self._agent_ref_owned,
                    lambda selected, safe: confirmed is True and selected == workspace_id,
                )
                self.agent_missions[workspace_id] = {
                    "mission": mission,
                    "state": "ACTIVE",
                    "started_monotonic": time.monotonic(),
                }
                return self.agent_mission_current()

            response, replayed = self._agent_idempotent(
                workspace_id, "mission.start", key, intent, start
            )
            return {**response, "replayed": replayed}

    def cancel_agent_mission(self, workspace_id, mission_id, key, confirmed):
        self._require_agent_workspace(workspace_id)
        intent = {"mission_id": mission_id, "human_confirmed": confirmed}
        with self.agent_operation_lock:
            def cancel():
                entry = self._active_agent_mission(workspace_id, mission_id)
                if confirmed is not True:
                    raise AgentMissionError("Annulation humaine explicite requise")
                entry["state"] = "CANCELLED"
                return self.agent_mission_current()

            response, replayed = self._agent_idempotent(
                workspace_id, "mission.cancel", key, intent, cancel
            )
            return {**response, "replayed": replayed}

    def rescope_agent_mission(self, workspace_id, mission_id, scoped_refs, pivots,
                              key, confirmed):
        self._require_agent_workspace(workspace_id)
        intent = {"mission_id": mission_id, "scoped_refs": scoped_refs,
                  "pivots": pivots, "human_confirmed": confirmed}
        with self.agent_operation_lock:
            def rescope():
                entry = self._active_agent_mission(workspace_id, mission_id)
                entry["mission"].rescope(
                    scoped_refs, pivots,
                    lambda current, proposal: confirmed is True,
                )
                return self.agent_mission_current()

            response, replayed = self._agent_idempotent(
                workspace_id, "mission.rescope", key, intent, rescope
            )
            return {**response, "replayed": replayed}

    def _require_agent_workspace(self, workspace_id):
        if workspace_id != self.agent_workspace_id():
            raise PermissionError("Workspace agent différent du workspace actif")

    def _active_agent_mission(self, workspace_id, mission_id):
        entry = self.agent_missions.get(workspace_id)
        if (entry is None or entry["state"] != "ACTIVE" or
                entry["mission"].snapshot()["mission_id"] != mission_id):
            raise AgentMissionError("Mission active inconnue")
        return entry

    def list_agent_proposals(self):
        workspace_id = self.agent_workspace_id()
        with self.agent_operation_lock:
            values = list(self.agent_proposals.get(workspace_id, {}).values())
            return {
                "contract": "labfy.agent_proposal_list.v1",
                "workspace_id": workspace_id,
                "proposals": [json.loads(json.dumps(item)) for item in values],
            }

    def create_agent_proposal(self, workspace_id, mission_id, proposal, key):
        self._require_agent_workspace(workspace_id)
        intent = {"mission_id": mission_id, "proposal": proposal}
        with self.agent_operation_lock:
            def create():
                self._active_agent_mission(workspace_id, mission_id)
                safe = self.agent_proposal_service.validate(workspace_id, proposal)
                proposal_id = str(uuid.uuid4())
                record = {"proposal_id": proposal_id, "mission_id": mission_id,
                          "proposal": safe, "decision": None}
                self.agent_proposals.setdefault(workspace_id, {})[proposal_id] = record
                return json.loads(json.dumps(record))

            response, replayed = self._agent_idempotent(
                workspace_id, "proposal.create", key, intent, create
            )
            return {"contract": "labfy.agent_proposal_record.v1",
                    **response, "replayed": replayed}

    def decide_agent_proposal(self, workspace_id, proposal_id, outcome, decision, key):
        self._require_agent_workspace(workspace_id)
        intent = {"proposal_id": proposal_id, "outcome": outcome,
                  "decision": decision}
        with self.agent_operation_lock:
            def decide():
                record = self.agent_proposals.get(workspace_id, {}).get(proposal_id)
                if record is None:
                    raise AgentProposalError("Proposition inconnue dans ce workspace")
                if record["decision"] is not None:
                    raise AgentProposalError("Proposition déjà décidée")
                trace = self.agent_proposal_service.record_decision(
                    workspace_id, record["proposal"],
                    {**decision, "decision": outcome},
                )
                record["decision"] = trace
                return json.loads(json.dumps(record))

            response, replayed = self._agent_idempotent(
                workspace_id, f"proposal.{outcome.lower()}", key, intent, decide
            )
            return {"contract": "labfy.agent_proposal_record.v1",
                    **response, "replayed": replayed}

    @staticmethod
    def _build_agent_tool_registry():
        definitions = (
            ToolDefinition(
                "forensics.strings",
                "GNU strings",
                "strings",
                RiskClass.OFFLINE_READ_ONLY,
                {"arguments": "argv", "input": "artifact"},
                {"stdout": "text"},
                ExecutionProfile(
                    timeout_seconds=10.0,
                    max_output_bytes=64 * 1024,
                    network=NetworkRequirement.OFFLINE,
                ),
                ("--help",),
                documentation_available=True,
                policy_allowed=True,
            ),
            ToolDefinition(
                "forensics.exiftool",
                "ExifTool metadata reader",
                "exiftool",
                RiskClass.OFFLINE_READ_ONLY,
                {"arguments": "argv", "input": "artifact"},
                {"stdout": "text"},
                ExecutionProfile(
                    timeout_seconds=15.0,
                    max_output_bytes=128 * 1024,
                    network=NetworkRequirement.OFFLINE,
                ),
                ("--help",),
                documentation_available=True,
                policy_allowed=True,
            ),
        )
        registry = ToolRegistry(definitions)
        for definition in registry.list_tools():
            registry.detect(definition.tool_id)
        return registry

    def _current_active_agent_mission(self):
        workspace_id = self.agent_workspace_id()
        entry = self.agent_missions.get(workspace_id)
        if entry is None or entry["state"] != "ACTIVE":
            raise AgentMissionError("Mission Agent active requise")
        return workspace_id, entry

    @staticmethod
    def _agent_policy_admission(snapshot, attempt):
        risk = attempt.get("risk_class")
        network_contact = attempt.get("network_contact")
        if risk == "LOCAL_READ_ONLY":
            return network_contact is False and attempt.get("contacts") == 0
        if risk == "PASSIVE_PUBLIC":
            return (
                network_contact is True
                and snapshot.get("network_profile") == "PASSIVE_PUBLIC"
                and attempt.get("contacts") == 1
            )
        return False

    def _admit_agent_attempt(self, risk_class, object_refs, *, network_contact=False):
        _workspace_id, entry = self._current_active_agent_mission()
        return entry["mission"].admit_attempt(
            {
                "risk_class": risk_class,
                "network_contact": network_contact,
                "object_refs": object_refs,
                "contacts": 1 if network_contact else 0,
                "duration_seconds": 0,
                "tool_calls": 1,
            },
            self._agent_policy_admission,
        )

    def _agent_get_overview(self, _arguments, _key):
        return self._agent_investigation_context(self.agent_scope())

    def _agent_find_correlations(self, _arguments, _key):
        objects = []
        for node in self._agent_graph().get("nodes", [])[:64]:
            if not isinstance(node, dict) or node.get("group") != "evidence":
                continue
            object_id = node.get("id")
            if not isinstance(object_id, str):
                continue
            naked_id = object_id.split(":", 1)[-1]
            try:
                preview = json.loads(self.bridge_call(
                    ["evidence-preview-json", "--evidence", naked_id], timeout=15
                ))
            except (OSError, ValueError, json.JSONDecodeError, subprocess.TimeoutExpired):
                continue
            digest = preview.get("sha256")
            if not isinstance(digest, str):
                continue
            ref = {"object_id": object_id}
            objects.append({
                "object_ref": ref,
                "attributes": [{
                    "kind": "hash",
                    "value": digest,
                    "source_refs": [ref],
                }],
            })
        result = find_correlation_candidates(objects)
        refs = {}
        for candidate in result.get("candidates", []):
            for ref in candidate.get("object_refs", []):
                refs[ref["object_id"]] = ref
        return {**result, "object_refs": list(refs.values())[:32]}

    def _agent_tool_catalog(self, _arguments, _key):
        tools = []
        for definition in self.agent_tool_registry.list_tools():
            tools.append({
                "tool_id": definition.tool_id,
                "display_name": definition.display_name,
                "binary": definition.binary,
                "risk_class": definition.risk_class.value,
                "network": definition.execution_profile.network.value,
                "documentation_available": definition.documentation_available,
                "execution_available": definition.execution_available,
                "policy_allowed": definition.policy_allowed,
                "detected_version": definition.detected_version,
                "availability_reason": definition.availability_reason,
            })
        if self._tool_provisioning is not None:
            for request in self._tool_provisioning.list_requests():
                if request.get("workspace_id") != self.agent_workspace_id() or \
                        request["state"] not in {"QUARANTINED", "WAITING_INTEGRATION_APPROVAL",
                                                 "ACTIVE"}:
                    continue
                package = request["package"]["package"]
                tools.append({"tool_id": f"debian.{package}",
                              "display_name": package, "binary": None,
                              "risk_class": "OFFLINE_READ_ONLY", "network": "OFFLINE",
                              "documentation_available": True,
                              "execution_available": request["state"] == "ACTIVE",
                              "policy_allowed": request["state"] == "ACTIVE",
                              "detected_version": request["package"]["version"],
                              "availability_reason": request["state"]})
        return {
            "contract": "labfy.agent_tool_catalog.v1",
            "tools": tools,
            "object_refs": [],
        }

    def _agent_tool_docs_read(self, arguments, _key):
        requested = arguments["tool_id"]
        if requested.startswith("debian.") and self._tool_provisioning is not None:
            package = requested.removeprefix("debian.")
            matches = [item for item in self._tool_provisioning.list_requests()
                       if item.get("workspace_id") == self.agent_workspace_id() and
                       item["package"]["package"] == package and
                       item["state"] in {"QUARANTINED", "WAITING_INTEGRATION_APPROVAL", "ACTIVE"}]
            if len(matches) != 1:
                raise ToolRegistryError("documentation toolbox indisponible")
            docs = self._tool_provisioning.documents(matches[0]["request_id"])
            content = str(docs.get("help", "UNTRUSTED_DATA\n"))[:1500]
            return {"contract": "labfy.agent_tool_document.v1",
                    "tool_id": requested, "document_id": matches[0]["request_id"],
                    "trust": "UNTRUSTED_DATA", "content": content,
                    "request_id": matches[0]["request_id"],
                    "declarative_examples": [{"example_id": "jq.username.v1",
                                               "capability_id": "data.jq.username"}],
                    "object_refs": []}
        definition = self.agent_tool_registry.get(arguments["tool_id"])
        if not definition.documentation_available:
            raise ToolRegistryError("documentation indisponible")
        executable = shutil.which(definition.binary)
        if executable is None:
            raise ToolRegistryError("outil indisponible")
        completed = subprocess.run(
            [executable, "--help"],
            check=False,
            capture_output=True,
            timeout=min(definition.execution_profile.timeout_seconds, 5.0),
            env={"PATH": "/usr/bin:/bin", "LC_ALL": "C", "LANG": "C"},
        )
        content = (bytes(completed.stdout) + bytes(completed.stderr))[:64 * 1024]
        if not content:
            completed = subprocess.run(
                [executable, "--version"],
                check=False,
                capture_output=True,
                timeout=3.0,
                env={"PATH": "/usr/bin:/bin", "LC_ALL": "C", "LANG": "C"},
            )
            content = (bytes(completed.stdout) + bytes(completed.stderr))[:64 * 1024]
        if not content:
            raise ToolRegistryError("documentation locale vide")
        document_id = self.agent_tool_docs.store(
            definition.tool_id, f"{definition.binary}:local-help", content
        )
        return {
            "contract": "labfy.agent_tool_document.v1",
            "tool_id": definition.tool_id,
            "document_id": document_id,
            "trust": "UNTRUSTED_DATA",
            "content": content.decode("utf-8", "replace"),
            "object_refs": [],
        }

    def _agent_preview_artifact(self, object_id):
        node = self._agent_node(object_id)
        if node.get("group") != "evidence":
            raise ValueError("Le sandbox exige une preuve")
        naked_id = object_id.split(":", 1)[-1]
        preview = json.loads(self.bridge_call(
            ["evidence-preview-json", "--evidence", naked_id], timeout=15
        ))
        if preview.get("integrity_valid") is not True:
            raise ValueError("Intégrité de preuve invalide")
        text = preview.get("text")
        if isinstance(text, str):
            content = text.encode("utf-8")
        else:
            image = preview.get("image_png_base64")
            if not isinstance(image, str):
                raise ValueError("Aperçu de preuve non matérialisable")
            try:
                content = base64.b64decode(image, validate=True)
            except ValueError as error:
                raise ValueError("Aperçu encodé invalide") from error
        if len(content) > MAX_UPLOAD_BYTES:
            raise ValueError("Aperçu de preuve trop volumineux")
        return content

    def _agent_sandbox_exec(self, arguments, _key):
        ref = {"object_id": arguments["object_id"]}
        self._admit_agent_attempt("LOCAL_READ_ONLY", [ref])
        content = self._agent_preview_artifact(arguments["object_id"])
        result = self.agent_sandbox.execute(
            arguments["tool_id"],
            arguments["arguments"],
            artifacts=[ArtifactInput("evidence", content)],
        )
        if result.status != SandboxStatus.SUCCESS:
            raise ValueError(f"Sandbox refusé: {result.status.value}")
        return {
            "contract": "labfy.agent_sandbox_result.v1",
            "status": result.status.value,
            "stdout": result.stdout.decode("utf-8", "replace"),
            "stderr": result.stderr.decode("utf-8", "replace"),
            "provenance": dict(result.provenance),
            "object_refs": [ref],
        }

    def _agent_propose(self, arguments, key):
        workspace_id, entry = self._current_active_agent_mission()
        refs = arguments["object_refs"]
        self._admit_agent_attempt("LOCAL_READ_ONLY", refs)
        mission_id = entry["mission"].snapshot()["mission_id"]
        return self.create_agent_proposal(
            workspace_id,
            mission_id,
            arguments,
            key,
        )

    def _ensure_agent_privacy_egress(self):
        with self.agent_operation_lock:
            if (
                self.agent_privacy_egress is not None
                and self.agent_privacy_egress.is_available()
            ):
                return self.agent_internet_research
            runtime = PodmanTorRuntime()
            supervisor = PrivacyEgressSupervisor.start_owned_tor(
                runtime, bootstrap_timeout=120.0
            )
            self.agent_privacy_egress = supervisor
            self.agent_internet_research = InternetResearch(supervisor)
            return self.agent_internet_research

    def _agent_web_fetch(self, arguments, _key):
        ref = {"object_id": arguments["object_id"]}
        self._admit_agent_attempt(
            "PASSIVE_PUBLIC", [ref], network_contact=True
        )
        research = self._ensure_agent_privacy_egress()
        result = research.fetch(
            arguments["url"],
            method=arguments["method"],
            max_body_bytes=64 * 1024,
        )
        if result.status != EgressStatus.SUCCESS:
            raise ValueError(f"Recherche Internet refusée: {result.status.value}")
        return {
            "contract": "labfy.agent_web_fetch_result.v1",
            "status": result.status.value,
            "final_url": result.final_url,
            "status_code": result.status_code,
            "headers": dict(result.headers),
            "content": result.body.decode("utf-8", "replace"),
            "content_trust": "UNTRUSTED_DATA",
            "provenance": dict(result.provenance),
            "object_refs": [ref],
        }

    def _agent_runtime_gateway(self, request):
        """Execute only through the existing capability/policy boundary."""
        scope = self.agent_scope()
        response = self.agent_gateway.call(scope, request)
        return self.agent_gateway.result(scope, response["result_id"])

    def _agent_investigation_context(self, _scope):
        """Return only the bounded projection allowed into a model turn."""
        workspace_id = self.agent_workspace_id()
        graph = self._agent_graph()
        context = self.context()
        if context is None:
            raise AgentMissionError("Aucun workspace actif")
        nodes = graph.get("nodes", [])
        known = {item.get("id") for item in nodes if isinstance(item, dict)}

        def owned(candidate_workspace, ref):
            return candidate_workspace == workspace_id and ref["object_id"] in known

        builder = InvestigationContext(lambda value: value == workspace_id, owned)
        return builder.build(workspace_id, {
            "workspace_id": workspace_id,
            "title": context.get("title", "Enquête locale"),
            "revision": graph.get("revision", 0),
            "counts": {
                "evidence": sum(
                    1 for node in nodes
                    if node.get("group") == "evidence"
                    or node.get("object_kind") == "evidence"
                ),
                "objects": len(nodes),
                "observations": sum(
                    1 for node in nodes
                    if node.get("group") == "observation"
                    or node.get("object_kind") == "observation"
                ),
            },
            "entity_types": sorted({str(node.get("type", "unknown")) for node in nodes}),
            "current_graph_selection": [],
            "recent_activity": [],
        })

    def agent_runtime_status(self):
        if self.agent_mode != "local-model":
            return {
                "contract": "labfy.agent_runtime.status.v1",
                "mode": "DETERMINISTIC_DEMO", "configured": False,
                "available": True, "provider": "deterministic-demo",
                "endpoint_kind": "none", "model": None, "reason": None,
            }
        supervisor = (self.local_model_supervisor.status()
                      if self.local_model_supervisor is not None else None)
        return {
            "contract": "labfy.agent_runtime.status.v1",
            "mode": "LOCAL_MODEL", "configured": self.agent_runtime is not None,
            "available": self.agent_runtime is not None,
            "provider": "openai-compatible-local", "endpoint_kind": "loopback",
            "model": self.agent_model, "reason": self.agent_runtime_reason,
            "supervisor": supervisor,
        }

    def resume_agent_runtime(self, turn_id):
        if self.agent_runtime is None:
            raise AgentRuntimeError("Modèle local indisponible", status=503)

        current = self.agent_runtime.status(self.agent_scope(), turn_id)
        pending = current.get("pending_call")
        if not isinstance(pending, dict):
            raise AgentRuntimeError("Aucune décision externe en attente", status=409)
        pending_tool = pending["request"]["tool_id"]
        original = pending["result"].get("output", {})
        request_id = original.get("request_id")
        change_id = original.get("change_id")
        if pending_tool in {"tool.provision.propose", "tool.integration.propose"}:
            record = self.tool_provisioning.get(request_id)
            if (record.get("workspace_id") != self.agent_workspace_id() or
                    record.get("turn_id") != turn_id):
                raise AgentRuntimeError("Décision d'un autre turn refusée", status=403)
            acceptable = ({"QUARANTINED", "PROVISION_REJECTED", "FAILED"}
                          if pending_tool == "tool.provision.propose"
                          else {"ACTIVE", "INTEGRATION_REJECTED"})
            if record["state"] not in acceptable:
                raise AgentRuntimeError("Décision toolbox encore en attente", status=409)
        elif pending_tool == "code.change.propose":
            self.code_change_turn(change_id)
            record = self.code_changes.get(change_id)
            if record["state"] not in {"WAITING_APPLY_APPROVAL", "DEV_REJECTED",
                                       "APPLIED_LOCAL", "APPLY_REJECTED", "FAILED"}:
                raise AgentRuntimeError("Décision développeur encore en attente", status=409)

        def authorization_callback(_pending_call):
            if pending_tool in {"tool.provision.propose", "tool.integration.propose"}:
                value = self.tool_provisioning.get(request_id)
                # WHY: Gate A libère Qwen pour proposer le manifest en
                # quarantaine ; Gate B libère l'exécution de la capability.
                return {"contract": "labfy.agent_external_decision.v1",
                        "state": "COMPLETED", "output": {
                            **value,
                            "declarative_example": integration_example_for(value),
                            "documentation_tool_id": "debian." + value["package"]["package"],
                        }}
            if pending_tool == "code.change.propose":
                value = self.code_changes.get(change_id)
                return {"contract": "labfy.agent_external_decision.v1",
                        "state": ("CODE_CHANGE_APPLY_REQUIRED"
                                  if value["state"] == "WAITING_APPLY_APPROVAL"
                                  else "COMPLETED"),
                        "output": self._code_change_model_summary(value)}
            # INVARIANT: only a persisted grant in the active workspace can
            # resume the paused model; a model phrase or an HTTP retry cannot.
            snapshot = json.loads(self.bridge_call(["research-snapshot-json"], timeout=15))
            grants = snapshot.get("grants")
            if not isinstance(grants, list) or not any(
                    isinstance(item, dict) and item.get("grant_id") and
                    item.get("revoked_at") is None for item in grants):
                raise ValueError("Aucun grant de recherche actif dans cet espace")
            return self._agent_runtime_gateway({
                "turn_id": turn_id, "tool_id": "research.get_state", "input": {},
                "context": {"source": "labfy.agent_runtime.resume.v1"},
                "object_refs": [], "idempotency_key": str(uuid.uuid4()),
            })

        return self.agent_runtime.resume_turn(self.agent_scope(), turn_id,
                                              authorization_callback)

    def library_snapshot(self):
        if self.library is None:
            raise ValueError("Mode bibliothèque inactif")
        value = self.library.snapshot(self.active_workspace_id)
        self.library_generation = value["generation"]
        return value

    def discover_existing_library_workspaces(self):
        if self.library is None:
            raise ValueError("Mode bibliothèque inactif")
        value = self.library.discover_existing()
        self.library_generation = value["generation"]
        return value

    def register_existing_library_workspace(self, candidate_id, expected_generation,
                                            idempotency_key, human_confirmed):
        if self.library is None:
            raise ValueError("Mode bibliothèque inactif")
        value = self.library.register_existing(
            candidate_id, expected_generation, idempotency_key, human_confirmed)
        self.library_generation = value["generation"]
        return value

    def create_library_workspace(self, title, idempotency_key):
        if self.library is None:
            raise ValueError("Mode bibliothèque inactif")
        value = self.library.create(title, idempotency_key)
        self.library_generation = value["generation"]
        return value

    def open_library_workspace(self, workspace_id, expected_generation):
        if self.library is None:
            raise ValueError("Mode bibliothèque inactif")
        with self.workspace_lifecycle_lock:
            if (self.active_workspace_id is not None and
                    self.active_workspace_id != workspace_id):
                raise ValueError("Une autre enquête est déjà active")
            workspace, entry, generation = self.library.open(
                workspace_id, expected_generation)
            if self.active_workspace_id is None:
                # INVARIANT: validation and export target the candidate directly;
                # no concurrent request can observe a half-open self.workspace.
                self.bridge_call(["export"], workspace=workspace)
                self.context(workspace=workspace)
                self.workspace = workspace
                self.active_workspace_id = workspace_id
            self.library_generation = generation
        return {"contract": "labfy.web_library.open.v1",
                "workspace_id": workspace_id, "title": entry["title"],
                "state": "READY", "generation": generation}

    def close_library_workspace(self, workspace_id, expected_generation,
                                idempotency_key, human_confirmed):
        if self.library is None:
            raise ValueError("Mode bibliothèque inactif")
        try:
            key = str(uuid.UUID(idempotency_key))
        except (ValueError, TypeError, AttributeError) as error:
            raise TypeError("Clé d'idempotence de fermeture invalide") from error
        intent = {"workspace_id": workspace_id,
                  "expected_generation": expected_generation,
                  "human_confirmed": human_confirmed}
        digest = self._agent_operation_digest(intent)
        with self.workspace_lifecycle_lock:
            replay = self.library_close_replays.get(key)
            if replay is not None:
                if replay[0] != digest:
                    raise ValueError("Clé d'idempotence déjà utilisée pour une autre fermeture")
                return {**replay[1], "replayed": True}
            if human_confirmed is not True:
                raise PermissionError("Confirmation humaine requise")
            if self.active_workspace_id is None:
                raise ValueError("Aucune enquête active")
            if workspace_id != self.active_workspace_id:
                raise PermissionError("Workspace différent du workspace actif")
            if expected_generation != self.library_generation:
                raise ValueError("Projection de bibliothèque périmée")
            scope = self.agent_scope()
            if self.agent_runtime is not None:
                self.agent_runtime.cancel_scope(scope, timeout=5.0)
            with self.agent_operation_lock:
                mission = self.agent_missions.get(workspace_id)
                if mission is not None and mission["state"] == "ACTIVE":
                    mission["state"] = "CANCELLED"
            # INVARIANT: only after every owned turn is terminal may the active
            # workspace identity and filesystem root become inactive together.
            self.active_workspace_id = None
            self.workspace = self.inactive_workspace
            response = {
                "contract": "labfy.web_library.close.v1",
                "workspace_id": workspace_id,
                "state": "CLOSED",
                "active_workspace_id": None,
                "generation": self.library_generation,
                "replayed": False,
            }
            self.library_close_replays[key] = (digest, response)
            return response

    def upload_mutex(self, upload_id):
        """Return the process-local owner lock for one persisted upload."""
        with self.upload_lock:
            return self.upload_mutexes.setdefault(upload_id, threading.Lock())

    def _context_path(self, workspace):
        generic = workspace / ".labfy" / "runtime" / "workspace.json"
        specimen = workspace / ".labfy" / "runtime" / "specimen.json"
        return generic if generic.is_file() else specimen

    @property
    def context_path(self):
        return self._context_path(self.workspace)

    def context(self, workspace=None):
        workspace = self.workspace if workspace is None else Path(workspace).resolve()
        context_path = self._context_path(workspace)
        database = workspace / "Enquete.sqlite"
        if not context_path.is_file():
            if database.exists():
                raise ValueError("Espace incomplet : base présente sans manifeste")
            return None
        if not database.is_file() or database.is_symlink():
            raise ValueError("Espace incomplet : base absente ou invalide")
        value = json.loads(context_path.read_text(encoding="utf-8"))
        if (value.get("contract") not in {"labfy.local_workspace.v1",
                "labfy.local_jobs.specimen.v1"} or
                not isinstance(value.get("investigation_id"), str)):
            raise ValueError("Manifeste d’espace invalide")
        return value

    def agent_scope(self):
        return f"{self.config_id}:{self.agent_workspace_id()}"

    def _agent_read_export(self, name, contracts):
        path = self.workspace / name
        if path.stat().st_size > MAX_EXPORT:
            raise ValueError("export trop volumineux")
        value = json.loads(path.read_text(encoding="utf-8"))
        if value.get("contract") not in contracts:
            raise ValueError("contrat d’export inattendu")
        return value

    def _agent_read_graph(self, arguments, _key):
        # INVARIANT: la réponse est la projection backend courante ; le fake
        # agent ne fabrique ni n'enrichit aucun nœud.
        return self._agent_read_export("core-snapshot.json", {
            "labfy.web_graph.snapshot.v2", "labfy.web_graph.snapshot.v3"})

    def _agent_read_jobs(self, arguments, _key):
        return self._agent_read_export("jobs-snapshot.json", {
            "labfy.local_jobs.snapshot.v1"})

    def _agent_graph(self):
        return self._agent_read_graph({}, "agent-read")

    @staticmethod
    def _agent_ref(node):
        return {"object_id": node["id"]}

    def _agent_node(self, object_id):
        value = next((node for node in self._agent_graph().get("nodes", [])
                      if node.get("id") == object_id), None)
        if value is None:
            raise ValueError("Objet absent de l'espace actif")
        return value

    def _agent_search(self, arguments, _key):
        # CONTRACT: recherche locale dans la projection validée, jamais dans
        # SQLite ni dans une instruction contenue dans l'objectif utilisateur.
        query = arguments["query"].casefold()
        nodes = self._agent_graph().get("nodes", [])
        matched = [
            node for node in nodes
            if query in json.dumps(node, ensure_ascii=False).casefold()
        ]
        context = self.context()
        investigation_matches = []
        if context is not None and query in context.get("title", "").casefold():
            investigation_matches.append({
                "object_kind": "investigation",
                "workspace_id": self.agent_workspace_id(),
                "label": context["title"],
            })
        return {
            "matches": investigation_matches + [
                {
                    "object_id": node["id"],
                    "label": node.get("label", node["id"]),
                }
                for node in matched[:8 - len(investigation_matches)]
            ],
            "object_refs": [self._agent_ref(node)
                            for node in matched[:8 - len(investigation_matches)]],
        }

    def _agent_get_node(self, arguments, _key):
        node = self._agent_node(arguments["object_id"])
        return {
            "node": node,
            "object_refs": [self._agent_ref(node)],
        }

    def _agent_get_neighbors(self, arguments, _key):
        graph = self._agent_graph()
        object_id = arguments["object_id"]
        node_ids = {node.get("id") for node in graph.get("nodes", [])}
        if object_id not in node_ids:
            raise ValueError("Objet absent de l'espace actif")
        edges = [
            edge for edge in graph.get("edges", [])
            if edge.get("source") == object_id or edge.get("target") == object_id
        ]
        neighbor_ids = {
            edge.get("target") if edge.get("source") == object_id
            else edge.get("source")
            for edge in edges
        }
        refs = [
            {"object_id": item}
            for item in neighbor_ids
            if item in node_ids
        ]
        return {
            "edges": edges[:32],
            "object_refs": refs[:32],
        }

    def _agent_evidence_summary(self, arguments, _key):
        node = self._agent_node(arguments["object_id"])
        return {
            "summary": {
                key: node[key]
                for key in ("id", "label", "type")
                if key in node
            },
            "object_refs": [self._agent_ref(node)],
        }

    def _agent_evidence_excerpt(self, arguments, _key):
        node = self._agent_node(arguments["object_id"])
        # INVARIANT: les exports ne transportent pas d'original de preuve ;
        # l'extrait est donc une description bornée de la projection.
        excerpt = json.dumps(
            {
                key: node.get(key)
                for key in ("label", "type", "state")
            },
            ensure_ascii=False,
        )[:512]
        return {
            "excerpt": excerpt,
            "object_refs": [self._agent_ref(node)],
        }

    def _agent_provenance(self, arguments, _key):
        neighbors = self._agent_get_neighbors(arguments, _key)
        return {
            "object_refs": [{"object_id": arguments["object_id"]}],
            "provenance_refs": neighbors["object_refs"],
        }

    def _agent_get_job(self, arguments, _key):
        jobs = self._agent_read_jobs({}, _key).get("jobs", [])
        job = next(
            (item for item in jobs if item.get("id") == arguments["job_id"]),
            None,
        )
        if job is None:
            raise ValueError("Job absent de l'espace actif")
        return {
            "job": job,
            "object_refs": [],
        }

    def _agent_research_state(self, _arguments, _key):
        output = self.bridge_call(["research-snapshot-json"], timeout=15)
        return json.loads(output)

    def _agent_prepare_research(self, arguments, key):
        graph = self._agent_read_graph({}, key)
        revision = graph.get("revision")
        if (not isinstance(revision, int) or isinstance(revision, bool) or
                revision < 0):
            raise ValueError("Révision du snapshot cœur invalide")
        output = self.bridge_call([
            "research-prepare-json", "--selection",
            # CONTRACT: the research boundary consumes the stable graph node
            # identifiers catalogued as object_refs, including their namespace.
            # WHY: stripping it produces an identifier absent from the core
            # snapshot and makes Agent calls diverge from the human Web flow.
            ",".join(arguments["selection_ids"]), "--question",
            arguments["question"], "--exclusions",
            ",".join(arguments["exclusions"]), "--revision", str(revision),
            "--key", key], timeout=15)
        # WHY: the Agent UI reads the same research projection as the human
        # flow; publishing before the authorization card is rendered prevents
        # a stale empty panel from becoming a second, misleading state.
        self.bridge_call(["export"], timeout=15)
        return json.loads(output)

    def bridge_call(self, arguments, timeout=8, workspace=None):
        workspace = self.workspace if workspace is None else Path(workspace).resolve()
        command = [str(self.bridge), arguments[0], "--workspace",
                   str(workspace), *arguments[1:]]
        environment = os.environ.copy()
        if self.research_fixture_authority is not None:
            environment["LABFY_RESEARCH_FIXTURE_AUTHORITY"] = \
                self.research_fixture_authority
        result = subprocess.run(command, cwd=self.bridge.parents[1], text=True,
                                env=environment,
                                capture_output=True, timeout=timeout, check=False)
        if result.returncode != 0:
            message = result.stderr.strip().splitlines()[-1] if result.stderr.strip() else "Commande C refusée"
            raise ValueError(message.removeprefix("local-jobs: "))
        return result.stdout

    def start_worker(self):
        with self.lock:
            if self.worker_supervisor is not None and self.worker_supervisor.is_alive():
                return
            self.worker_supervisor_stop.clear()
            self.worker_supervisor = threading.Thread(
                target=self._supervise_worker, daemon=True)
            self.worker_supervisor.start()

    def _supervise_worker(self):
        while True:
            process = subprocess.Popen(
                [str(self.bridge), "run", "--workspace", str(self.workspace)],
                cwd=self.bridge.parents[1], stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL, start_new_session=True,
            )
            with self.lock:
                self.worker = process
            process.wait()
            try:
                self.bridge_call(["export"])
                snapshot = json.loads((self.workspace / "jobs-snapshot.json").read_text())
                pending = any(job.get("state") in {"QUEUED", "RETRY_WAIT"}
                              for job in snapshot.get("jobs", []))
            except (OSError, ValueError, json.JSONDecodeError,
                    subprocess.TimeoutExpired):
                pending = False
            if not pending or self.worker_supervisor_stop.is_set():
                break
        with self.lock:
            self.worker = None

    def stop_worker(self):
        self.worker_supervisor_stop.set()
        with self.lock:
            process = self.worker
        if process is not None and process.poll() is None:
            os.killpg(process.pid, signal.SIGTERM)
            try:
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait(timeout=2)

    def start_report(self, document, intention):
        hashlib = __import__("hashlib")
        report_id = hashlib.sha256(
            (document["investigation_id"] + "\0" + intention).encode()).hexdigest()[:32]
        fingerprint = hashlib.sha256(json.dumps(document, ensure_ascii=False,
            sort_keys=True, separators=(",", ":")).encode() + b"\n").hexdigest()
        with self.report_lock:
            current = self.report_tasks.get(report_id)
            if current and current["state"] in {"GENERATING", "READY"}:
                if current.get("fingerprint") != fingerprint:
                    raise ReportError("Intention déjà utilisée avec un autre document")
                return report_id
            active = sum(task["state"] == "GENERATING" for task in self.report_tasks.values())
            if active >= self.max_report_tasks:
                raise ReportError("Limite de générations simultanées atteinte")
            self.report_tasks[report_id] = {"state": "GENERATING", "error": None,
                                            "fingerprint": fingerprint}
        def render():
            try:
                actual, reused = publish_report(document, self.workspace, intention)
                state = {"state": "READY", "error": None, "reused": reused,
                         "fingerprint": fingerprint}
                if actual != report_id:
                    raise ReportError("Identité publiée incohérente")
            except (OSError, ReportError, subprocess.TimeoutExpired) as error:
                state = {"state": "FAILED", "error": str(error),
                         "fingerprint": fingerprint}
            with self.report_lock:
                self.report_tasks[report_id] = state
                self.report_threads.discard(threading.current_thread())
        thread = threading.Thread(target=render, name="labfy-report-render")
        with self.report_lock:
            self.report_threads.add(thread)
        thread.start()
        return report_id

    def server_close(self):
        with self.tooling_lock:
            tooling_threads = list(self.tooling_threads)
        for thread in tooling_threads:
            thread.join(timeout=300)
        if self.agent_runtime is not None:
            self.agent_runtime.close()
        if self.local_model_supervisor is not None:
            # INVARIANT: the supervisor stops only the process it created.
            self.local_model_supervisor.stop()
        if self.agent_privacy_egress is not None:
            self.agent_privacy_egress.close()
            self.agent_privacy_egress = None
            self.agent_internet_research = None
        with self.report_lock:
            threads = list(self.report_threads)
        for thread in threads:
            thread.join(timeout=30)
        super().server_close()


class Handler(BaseHTTPRequestHandler):
    server_version = "LabfyWorkspace/0.1"

    def _headers(self):
        self.send_header("Content-Security-Policy",
                         "default-src 'self'; script-src 'self'; style-src 'self'; "
                         "img-src 'self' data:; connect-src 'self'; object-src 'none'; "
                         "base-uri 'none'; frame-ancestors 'none'; form-action 'self'")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Referrer-Policy", "no-referrer")
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Frame-Options", "DENY")

    def _json(self, status, value, *, cookie=None):
        body = json.dumps(value, ensure_ascii=False, separators=(",", ":")).encode()
        self.send_response(status)
        self._headers()
        self.send_header("Content-Type", "application/json; charset=utf-8")
        if cookie:
            self.send_header("Set-Cookie", cookie)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(body)

    def _error(self, status, code, message):
        self._json(status, {"contract": "labfy.workspace.error.v1",
                            "error": code, "message": message})

    def _request_valid(self):
        host = self.headers.get("Host")
        return (host == self.server.authority or
                (self.server.public_local_origin == "http://invest.labfy" and
                 host == "invest.labfy" and self.client_address[0] == "127.0.0.1"))

    def _request_origin(self):
        # INVARIANT: an accepted Origin is derived from the validated Host,
        # never from Forwarded or X-Forwarded-* supplied by a client.
        if self.headers.get("Host") == "invest.labfy":
            return self.server.public_local_origin
        return self.server.origin

    def _authenticated(self):
        with self.server.session_lock:
            if time.monotonic() >= self.server.session_deadline:
                return False
            cookie = SimpleCookie(self.headers.get("Cookie", ""))
            value = cookie.get(self.server.cookie_name)
            return value is not None and hmac.compare_digest(
                value.value, self.server.session)

    def _mutation_allowed(self):
        with self.server.session_lock:
            return (self._authenticated() and
                    self.headers.get("Origin") == self._request_origin() and
                    hmac.compare_digest(self.headers.get("X-Labfy-CSRF", ""),
                                        self.server.csrf))

    def _content_length(self, maximum, *, expected=None):
        """Validate HTTP/1.1 framing before a route consumes its body."""
        transfer_encoding = self.headers.get_all("Transfer-Encoding") or []
        if transfer_encoding:
            self.close_connection = True
            raise TypeError("Transfer-Encoding non pris en charge")
        fields = self.headers.get_all("Content-Length") or []
        values = [item.strip() for field in fields for item in field.split(",")]
        if not values or any(not item or not all("0" <= char <= "9"
                                                for char in item)
                             for item in values):
            self.close_connection = True
            raise TypeError("Content-Length invalide")
        lengths = [int(item) for item in values]
        if len(set(lengths)) != 1:
            self.close_connection = True
            raise TypeError("Content-Length dupliqués contradictoires")
        length = lengths[0]
        if length > maximum:
            self.close_connection = True
            raise OverflowError("Corps trop volumineux")
        if expected is not None and length != expected:
            self.close_connection = True
            raise ValueError("Longueur reçue incohérente")
        return length

    def _read_exact_body(self, length):
        """Read one framed body under one absolute monotonic deadline.

        Activity never extends the deadline, and EOF before Content-Length is
        an explicit truncated request rather than a partial successful body.
        """
        deadline = time.monotonic() + UPLOAD_TIMEOUT_SECONDS
        previous_timeout = self.connection.gettimeout()
        body = bytearray()
        try:
            while len(body) < length:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise TimeoutError("Réception expirée après 30 s")
                self.connection.settimeout(remaining)
                try:
                    # read1 performs at most one raw socket read, so a trickling
                    # peer cannot keep an internal buffered read alive forever.
                    block = self.rfile.read1(min(65536, length - len(body)))
                except socket.timeout as error:
                    self.close_connection = True
                    raise TypeError("Réception expirée après 30 s") from error
                if not block:
                    self.close_connection = True
                    raise TypeError("Corps HTTP tronqué")
                body.extend(block)
        finally:
            self.connection.settimeout(previous_timeout)
        return bytes(body)

    def _json_body(self):
        if self.headers.get("Content-Type") != "application/json":
            raise TypeError("Content-Type application/json obligatoire")
        length = self._content_length(MAX_BODY)
        try:
            return json.loads(self._read_exact_body(length))
        except (UnicodeDecodeError, json.JSONDecodeError) as error:
            raise TypeError("JSON invalide") from error

    def _body(self, allowed):
        value = self._json_body()
        if not isinstance(value, dict) or set(value) != set(allowed):
            raise TypeError("Champs JSON inattendus")
        if any(not isinstance(item, str) for item in value.values()):
            raise TypeError("Tous les champs JSON doivent être textuels")
        if any(len(item) == 0 or len(item) > 512 for item in value.values()):
            raise TypeError("Champ JSON vide ou trop long")
        return value

    def _human_decision_body(self):
        value = self._json_body()
        expected = {"decision_id", "idempotency_key", "actor",
                    "human_confirmed", "reason"}
        if not isinstance(value, dict) or set(value) != expected:
            raise TypeError("Champs de décision inattendus")
        for name in ("decision_id", "idempotency_key"):
            if not isinstance(value[name], str):
                raise TypeError("Identifiant de décision invalide")
            uuid.UUID(value[name])
        if (value["actor"] != "human" or value["human_confirmed"] is not True or
                not isinstance(value["reason"], str) or len(value["reason"]) > 500):
            raise PermissionError("Décision humaine authentifiée requise")
        return value

    def _library_open_body(self):
        value = self._json_body()
        if (not isinstance(value, dict) or
                set(value) != {"workspace_id", "expected_generation"} or
                not isinstance(value["workspace_id"], str) or
                not isinstance(value["expected_generation"], int) or
                isinstance(value["expected_generation"], bool)):
            raise TypeError("Demande d'ouverture de bibliothèque invalide")
        return value

    def _library_register_body(self):
        value = self._json_body()
        if (not isinstance(value, dict) or set(value) != {
                "candidate_id", "expected_generation", "idempotency_key",
                "human_confirmed"} or
                not isinstance(value["candidate_id"], str) or
                not 0 < len(value["candidate_id"]) <= 256 or
                not isinstance(value["expected_generation"], int) or
                isinstance(value["expected_generation"], bool) or
                not isinstance(value["idempotency_key"], str) or
                value["human_confirmed"] is not True):
            raise TypeError("Demande d'enregistrement invalide")
        try:
            uuid.UUID(value["idempotency_key"])
        except ValueError as error:
            raise TypeError("Clé d'idempotence invalide") from error
        return value

    def _library_close_body(self):
        value = self._json_body()
        if (not isinstance(value, dict) or set(value) != {
                "workspace_id", "expected_generation", "idempotency_key",
                "human_confirmed"} or
                not isinstance(value["workspace_id"], str) or
                not isinstance(value["expected_generation"], int) or
                isinstance(value["expected_generation"], bool) or
                not isinstance(value["idempotency_key"], str) or
                not isinstance(value["human_confirmed"], bool)):
            raise TypeError("Demande de fermeture invalide")
        return value

    def _agent_operation_body(self, operation):
        value = self._json_body()
        shapes = {
            "mission.start": {"workspace_id", "specification", "human_confirmed",
                              "idempotency_key"},
            "mission.cancel": {"workspace_id", "mission_id", "human_confirmed",
                               "idempotency_key"},
            "mission.rescope": {"workspace_id", "mission_id", "scoped_refs",
                                "pivots", "human_confirmed", "idempotency_key"},
            "proposal.create": {"workspace_id", "mission_id", "proposal",
                                "idempotency_key"},
            "proposal.decide": {"workspace_id", "decision_id", "reason",
                                "decided_by", "decided_at", "idempotency_key"},
        }
        if not isinstance(value, dict) or set(value) != shapes[operation]:
            raise TypeError("Schéma d’opération agent invalide")
        for name in shapes[operation] & {
                "workspace_id", "mission_id", "decision_id", "reason",
                "decided_by", "decided_at", "idempotency_key"}:
            item = value[name]
            if (not isinstance(item, str) or not item or len(item) > 512 or
                    "\x00" in item):
                raise TypeError("Champ d’opération agent invalide")
        if ("human_confirmed" in value and
                not isinstance(value["human_confirmed"], bool)):
            raise TypeError("Confirmation humaine invalide")
        return value

    def _plan_body(self):
        value = self._json_body()
        if not isinstance(value, dict) or set(value) != {
                "recommendation_ids", "input_revision", "profile_id",
                "idempotency_key"}:
            raise TypeError("Champs JSON inattendus")
        ids = value["recommendation_ids"]
        if (not isinstance(ids, list) or not ids or len(ids) > MAX_PLAN_ITEMS or
                any(not isinstance(item, str) or not item or len(item) > 96
                    for item in ids)):
            raise TypeError("Sélection de recommandations invalide")
        if len(set(ids)) != len(ids):
            value["recommendation_ids"] = list(dict.fromkeys(ids))
        for name in ("input_revision", "profile_id", "idempotency_key"):
            if not isinstance(value[name], str) or not value[name] or len(value[name]) > 128:
                raise TypeError("Champ de plan invalide")
        return value

    def _research_body(self, kind):
        value = self._json_body()
        shapes = {
            "prepare": {"selection_ids", "question", "exclusions",
                        "idempotency_key"},
            "grant": {"plan_id", "input_revision", "selected_action_ids",
                      "decisions", "exclusions", "idempotency_key"},
            "campaign": {"grant_id", "input_revision", "action_ids",
                         "idempotency_key"},
            "revoke": {"grant_id"},
        }
        if not isinstance(value, dict) or set(value) != shapes[kind]:
            raise TypeError("Champs de recherche inattendus")
        if kind == "grant":
            decisions = value["decisions"]
            if (not isinstance(decisions, list) or not decisions or
                    len(decisions) > 8 or any(not isinstance(item, dict) or
                    set(item) != {"action_id", "decision"}
                    for item in decisions)):
                raise TypeError("Décisions de recherche invalides")
            decision_ids = []
            for item in decisions:
                if (not isinstance(item["action_id"], str) or
                        not isinstance(item["decision"], str) or
                        item["decision"] not in {"AUTHORIZE", "DEFER", "REFUSE"}):
                    raise TypeError("Décision de recherche invalide")
                try:
                    uuid.UUID(item["action_id"])
                except ValueError as error:
                    raise TypeError("Identifiant de décision invalide") from error
                decision_ids.append(item["action_id"])
            if len(set(decision_ids)) != len(decision_ids):
                raise TypeError("Décision de recherche dupliquée")
            authorized = {item["action_id"] for item in decisions
                          if item["decision"] == "AUTHORIZE"}
            if not set(value["selected_action_ids"]) <= authorized:
                raise TypeError("Action sélectionnée sans autorisation")
        list_names = shapes[kind] & {
            "selection_ids", "exclusions", "selected_action_ids", "action_ids"}
        for name in list_names:
            items = value[name]
            if (not isinstance(items, list) or len(items) > 8 or
                    (name != "exclusions" and not items) or
                    len(set(items)) != len(items) or
                    any(not isinstance(item, str) or not item or
                        len(item) > 160 or "," in item for item in items)):
                raise TypeError("Liste de recherche invalide")
        for name, item in value.items():
            if name in list_names or name == "decisions":
                continue
            if name == "input_revision":
                if not isinstance(item, int) or isinstance(item, bool) or item < 0:
                    raise TypeError("Révision de recherche invalide")
            elif (not isinstance(item, str) or not item or len(item) > 512 or
                  "\x00" in item):
                raise TypeError("Champ de recherche invalide")
        for name in shapes[kind] & {"plan_id", "grant_id", "idempotency_key"}:
            try:
                uuid.UUID(value[name])
            except ValueError as error:
                raise TypeError("Identifiant de recherche invalide") from error
        return value

    def _report_preview_body(self):
        value = self._json_body()
        if not isinstance(value, dict) or set(value) != {"object_ids", "title",
                "comment", "profile", "sections"}:
            raise TypeError("Champs de prévisualisation inattendus")
        ids=value["object_ids"];sections=value["sections"]
        if (not isinstance(ids,list) or not ids or len(ids)>64 or
                len(set(ids))!=len(ids) or any(not isinstance(item,str) or
                not item or len(item)>160 for item in ids)):
            raise TypeError("Sélection de rapport invalide")
        if (not isinstance(sections,list) or not sections or
                not set(sections)<={"evidence","timeline","infrastructure"}):
            raise TypeError("Sections de rapport invalides")
        if (value["profile"]!="MINIMAL" or not isinstance(value["title"],str) or
                not 0<len(value["title"])<=160 or not isinstance(value["comment"],str) or
                len(value["comment"])>1000):
            raise TypeError("Profil ou rédaction de rapport invalide")
        return value

    def _import_confirmation_body(self):
        value=self._json_body()
        if not isinstance(value,dict) or set(value)!={"idempotency_key","source","description"}:
            raise TypeError("Champs de confirmation inattendus")
        if (not isinstance(value["idempotency_key"],str) or
            not isinstance(value["source"],str) or not isinstance(value["description"],str) or
            not value["idempotency_key"] or len(value["idempotency_key"])>128 or
            len(value["source"])>512 or len(value["description"])>512):
            raise TypeError("Métadonnées de confirmation invalides")
        return value

    def _review_body(self, fields):
        value = self._json_body()
        expected = {"operation_id", "expected_revision", "author", "reason", *fields}
        if not isinstance(value, dict) or set(value) != expected:
            raise TypeError("Champs de revue inattendus")
        if any(not isinstance(item, str) for item in value.values()):
            raise TypeError("Tous les champs de revue doivent être textuels")
        if (any(len(value[name]) == 0 or len(value[name]) > 512
                for name in {"operation_id", "expected_revision", "author", "reason"}) or
                any(len(value[name]) > 512 for name in fields)):
            raise TypeError("Champ de revue vide ou trop long")
        try:
            uuid.UUID(value["operation_id"])
            revision = int(value["expected_revision"])
        except ValueError as error:
            raise TypeError("UUID d’opération ou révision invalide") from error
        if revision < 0:
            raise TypeError("Révision négative interdite")
        return value

    def _review_command(self, evidence_id, observation_id, value, action,
                        *, status="", corrected="", entity=""):
        for identifier in (evidence_id, observation_id):
            try:
                uuid.UUID(identifier)
            except ValueError as error:
                raise TypeError("UUID de preuve ou d’observation invalide") from error
        arguments = ["review-observation-json", "--evidence", evidence_id,
                     "--observation", observation_id,
                     "--operation", value["operation_id"],
                     "--revision", value["expected_revision"],
                     "--action", action, "--author", value["author"],
                     "--reason", value["reason"]]
        for option, item in (("--status", status), ("--corrected", corrected),
                             ("--entity", entity)):
            if item:
                arguments.extend((option, item))
        return json.loads(self.server.bridge_call(arguments, timeout=15))

    def do_GET(self):
        if not self._request_valid():
            self._error(HTTPStatus.FORBIDDEN, "host_rejected", "Autorité HTTP refusée")
            return
        parsed = urlsplit(self.path)
        path = parsed.path
        if path == "/healthz":
            self._json(HTTPStatus.OK, {"contract": "labfy.workspace.health.v1",
                "status": "ready", "instance_id": self.server.instance_id,
                "config_id": self.server.config_id})
            return
        if path == "/favicon.ico":
            # CONTRACT: les navigateurs demandent implicitement cette icône ;
            # l'absence d'asset ne doit pas transformer un parcours sain en 404.
            self.send_response(HTTPStatus.NO_CONTENT)
            self._headers()
            self.end_headers()
            return
        if path in {"/login.html", "/login.js"}:
            self._error(HTTPStatus.NOT_FOUND, "route_unknown", "Route inconnue")
            return
        if not self._authenticated():
            if path == "/":
                self._open_automatic_session()
                return
            self._error(HTTPStatus.UNAUTHORIZED, "session_required",
                        "Session locale requise")
            return
        if path == "/api/v1/session":
            try: context = self.server.context()
            except (OSError,ValueError,json.JSONDecodeError) as error:
                self._error(HTTPStatus.CONFLICT,"workspace_incomplete",str(error));return
            self._json(HTTPStatus.OK, {"contract": "labfy.workspace.session.v1",
                "csrf": self.server.csrf, "origin": self._request_origin(),
                "workspace_state": "READY" if context else "EMPTY",
                "investigation_id": context.get("investigation_id") if context else None,
                "title": context.get("title", "SPECIMEN") if context else None,
                "mode": context.get("mode", "specimen") if context else "local_experimental",
                "library_mode": self.server.library is not None,
                "library_load_required": (self.server.library is not None and
                                          self.server.library.lazy),
                "active_workspace_id": self.server.active_workspace_id,
                "generation": self.server.library_generation})
        elif path == "/api/v1/library":
            try:
                self._json(HTTPStatus.OK, self.server.library_snapshot())
            except (OSError, ValueError, json.JSONDecodeError) as error:
                self._error(HTTPStatus.CONFLICT, "library_unavailable", str(error))
        elif path.startswith("/api/v1/evidence/"):
            parts = path.split("/")
            if len(parts) != 6 or parts[5] not in {"preview", "observations"}:
                self._error(HTTPStatus.NOT_FOUND, "route_unknown", "Route inconnue")
                return
            evidence_id = parts[4]
            try:
                uuid.UUID(evidence_id)
                arguments = (["evidence-preview-json", "--evidence", evidence_id]
                    if parts[5] == "preview" else
                    ["observations-json", "--evidence", evidence_id])
                query = parse_qs(parsed.query, strict_parsing=True) if parsed.query else {}
                if parts[5] == "preview" and query:
                    raise TypeError("Paramètre d’aperçu inattendu")
                if parts[5] == "observations":
                    if set(query) - {"extraction_id"} or any(len(v) != 1 for v in query.values()):
                        raise TypeError("Paramètre d’observation inattendu")
                    extraction = query.get("extraction_id", [None])[0]
                    if extraction is not None:
                        uuid.UUID(extraction)
                        arguments.extend(("--extraction", extraction))
                self._json(HTTPStatus.OK, json.loads(self.server.bridge_call(arguments)))
            except (TypeError, ValueError, json.JSONDecodeError,
                    subprocess.TimeoutExpired) as error:
                self._error(HTTPStatus.CONFLICT, "evidence_unavailable", str(error))
        elif path == "/api/v1/snapshot":
            self._serve_export("core-snapshot.json", {
                "labfy.web_graph.snapshot.v2", "labfy.web_graph.snapshot.v3"})
        elif path == "/api/v1/jobs":
            self._serve_export("jobs-snapshot.json", "labfy.local_jobs.snapshot.v1")
        elif path == "/api/v1/correlations":
            self._serve_export("correlation-snapshot.json",
                               "labfy.local_correlation.snapshot.v1")
        elif path == "/api/v1/planner":
            self._serve_export("planner-snapshot.json",
                               "labfy.local_planner.snapshot.v1")
        elif path == "/api/v1/research":
            try:
                value = json.loads(self.server.bridge_call(
                    ["research-snapshot-json"]))
                self._json(HTTPStatus.OK, value)
            except (ValueError, json.JSONDecodeError,
                    subprocess.TimeoutExpired) as error:
                self._error(HTTPStatus.CONFLICT, "research_unavailable", str(error))
        elif path == "/api/v1/agent-tools/catalog":
            self._json(HTTPStatus.OK, self.server.agent_gateway.catalog())
        elif path == "/api/v1/tool-provisioning":
            if not self.server.tooling_enabled:
                self._error(HTTPStatus.NOT_FOUND, "route_unknown", "Route inconnue")
                return
            try:
                workspace_id = self.server.agent_workspace_id()
                service = self.server.tool_provisioning
                requests = [item for item in service.list_requests()
                            if item.get("workspace_id") == workspace_id]
                self._json(HTTPStatus.OK, {
                    "contract": "labfy.tool_provisioning_list.v1",
                    "requests": requests,
                    "catalog_revision": service.store.read_state()["catalog_revision"],
                })
            except (AgentMissionError, ProvisioningError, OSError, ValueError) as error:
                self._error(HTTPStatus.CONFLICT, "tool_provisioning_unavailable", str(error))
        elif path == "/api/v1/capability-manifests":
            if not self.server.tooling_enabled:
                self._error(HTTPStatus.NOT_FOUND, "route_unknown", "Route inconnue")
                return
            try:
                self._json(HTTPStatus.OK, self.server.dynamic_capability_manifests())
            except (AgentMissionError, ProvisioningError, OSError, ValueError) as error:
                self._error(HTTPStatus.CONFLICT, "capability_catalog_unavailable", str(error))
        elif path == "/api/v1/code-changes":
            if not self.server.tooling_enabled:
                self._error(HTTPStatus.NOT_FOUND, "route_unknown", "Route inconnue")
                return
            try:
                workspace_id = self.server.agent_workspace_id()
                changes = self.server.code_changes.list_changes()
                if self.server.library is not None:
                    changes = [item for item in changes
                               if self.server.code_change_turns.get(
                                   item.get("change_id"), {}).get("workspace_id") ==
                               workspace_id]
                self._json(HTTPStatus.OK, {"contract": "labfy.code_change_list.v1",
                    "changes": changes})
            except (CodeChangeError, OSError, ValueError) as error:
                self._error(HTTPStatus.CONFLICT, "code_changes_unavailable", str(error))
        elif path.startswith("/api/v1/code-changes/") and path.endswith("/diff"):
            if not self.server.tooling_enabled:
                self._error(HTTPStatus.NOT_FOUND, "route_unknown", "Route inconnue")
                return
            parts = path.split("/")
            if len(parts) != 6:
                self._error(HTTPStatus.NOT_FOUND, "route_unknown", "Route inconnue")
                return
            try:
                self.server.code_change_turn(parts[4])
                difference = self.server.code_changes.get_diff(parts[4])
                self._json(HTTPStatus.OK, {"contract": "labfy.code_change_diff.v1",
                    **difference})
            except (CodeChangeError, OSError, ValueError) as error:
                self._error(HTTPStatus.CONFLICT, "code_change_diff_unavailable", str(error))
        elif path == "/api/v1/agent-runtime/status":
            self._json(HTTPStatus.OK, self.server.agent_runtime_status())
        elif path == "/api/v1/agent-mission/current":
            try:
                self._json(HTTPStatus.OK, self.server.agent_mission_current())
            except (AgentMissionError, OSError, ValueError,
                    json.JSONDecodeError) as error:
                self._error(HTTPStatus.CONFLICT, "agent_mission_unavailable", str(error))
        elif path == "/api/v1/agent-proposals":
            try:
                self._json(HTTPStatus.OK, self.server.list_agent_proposals())
            except (AgentProposalError, OSError, ValueError,
                    json.JSONDecodeError) as error:
                self._error(HTTPStatus.CONFLICT, "agent_proposals_unavailable", str(error))
        elif path == "/api/v1/agent-runtime/events":
            try:
                if self.server.agent_runtime is None:
                    raise AgentRuntimeError("Modèle local indisponible", status=503)
                query = parse_qs(parsed.query, strict_parsing=True) if parsed.query else {}
                if set(query) != {"cursor"} or len(query["cursor"]) != 1:
                    raise AgentRuntimeError("Paramètre d’événements inattendu")
                raw_cursor = query["cursor"][0]
                if not raw_cursor.isascii() or not raw_cursor.isdecimal():
                    raise AgentRuntimeError("Curseur runtime invalide")
                self._json(HTTPStatus.OK, self.server.agent_runtime.events(
                    self.server.agent_scope(), int(raw_cursor)))
            except AgentRuntimeError as error:
                self._error(error.status, error.code, str(error))
        elif path.startswith("/api/v1/agent-runtime/turns/"):
            parts = path.split("/")
            if len(parts) != 6:
                self._error(HTTPStatus.NOT_FOUND, "route_unknown", "Route inconnue")
                return
            try:
                if self.server.agent_runtime is None:
                    raise AgentRuntimeError("Modèle local indisponible", status=503)
                self._json(HTTPStatus.OK, self.server.agent_runtime.status(
                    self.server.agent_scope(), parts[5]))
            except AgentRuntimeError as error:
                self._error(error.status, error.code, str(error))
        elif path == "/api/v1/agent-tools/events":
            try:
                query = parse_qs(parsed.query, strict_parsing=True) if parsed.query else {}
                if set(query) - {"cursor"} or any(len(items) != 1
                                                   for items in query.values()):
                    raise AgentGatewayError("Paramètre d’événements inattendu")
                raw_cursor = query.get("cursor", ["0"])[0]
                if not raw_cursor.isascii() or not raw_cursor.isdecimal():
                    raise AgentGatewayError("Curseur d’événements invalide")
                self._json(HTTPStatus.OK, self.server.agent_gateway.events(
                    self.server.agent_scope(), int(raw_cursor)))
            except AgentGatewayError as error:
                self._error(error.status, error.code, str(error))
            except (OSError, ValueError, json.JSONDecodeError) as error:
                self._error(HTTPStatus.CONFLICT, "agent_tool_unavailable", str(error))
        elif path.startswith("/api/v1/agent-tools/results/"):
            parts = path.split("/")
            if len(parts) != 6:
                self._error(HTTPStatus.NOT_FOUND, "route_unknown", "Route inconnue")
                return
            try:
                self._json(HTTPStatus.OK, self.server.agent_gateway.result(
                    self.server.agent_scope(), parts[5]))
            except AgentGatewayError as error:
                self._error(error.status, error.code, str(error))
        elif path.startswith("/api/v1/reports/"):
            parts=path.split("/")
            report_id=parts[4] if len(parts)>4 else ""
            if not (len(report_id)==32 and all(c in "0123456789abcdef" for c in report_id)):
                self._error(HTTPStatus.NOT_FOUND,"report_unknown","Rapport inconnu");return
            if len(parts)==5:
                with self.server.report_lock:
                    task=self.server.report_tasks.get(report_id)
                bundle=self.server.workspace/"exports"/"reports"/report_id
                if task is None and bundle.is_dir():
                    check=verify_report(bundle);task={"state":"READY" if check["valid"] else "CORRUPTED","error":None if check["valid"] else ", ".join(check["errors"])}
                if task is None:self._error(HTTPStatus.NOT_FOUND,"report_unknown","Rapport inconnu")
                else:self._json(HTTPStatus.OK,{"contract":"labfy.report_status.v1","report_id":report_id,**task})
            elif len(parts)==6 and parts[5] in {"report.json","report.html","report.pdf","manifest.json","NOTICE.txt"}:
                bundle=self.server.workspace/"exports"/"reports"/report_id
                check=verify_report(bundle)
                if not check["valid"]:self._error(HTTPStatus.CONFLICT,"report_corrupted","Rapport absent ou altéré");return
                target=bundle/parts[5];types={"report.json":"application/json","report.html":"text/html; charset=utf-8","report.pdf":"application/pdf","manifest.json":"application/json","NOTICE.txt":"text/plain; charset=utf-8"}
                self.send_response(HTTPStatus.OK);self._headers();self.send_header("Content-Type",types[parts[5]]);self.send_header("Content-Disposition",f'attachment; filename="{parts[5]}"');data=target.read_bytes();self.send_header("Content-Length",str(len(data)));self.end_headers()
                if self.command != "HEAD": self.wfile.write(data)
            else:self._error(HTTPStatus.NOT_FOUND,"report_file_unknown","Fichier de rapport inconnu")
        elif path == "/fixture.js":
            self._serve_fixture()
        elif path.startswith("/api/"):
            self._error(HTTPStatus.NOT_FOUND, "route_unknown", "Route inconnue")
        else:
            self._serve_static(path)

    def do_HEAD(self):
        self.do_GET()

    def do_POST(self):
        if not self._request_valid():
            self._error(HTTPStatus.FORBIDDEN, "host_rejected", "Autorité HTTP refusée")
            return
        path = urlsplit(self.path).path
        if path == "/api/v1/session":
            self._error(HTTPStatus.NOT_FOUND, "route_unknown", "Route inconnue")
            return
        if not self._mutation_allowed():
            self._error(HTTPStatus.FORBIDDEN, "mutation_rejected",
                        "Session, Origin ou preuve CSRF invalide")
            return
        now = time.monotonic()
        with self.server.command_rate_lock:
            if (not path.startswith("/api/v1/uploads") and
                    now - self.server.last_command < 0.03):
                self._error(HTTPStatus.TOO_MANY_REQUESTS, "rate_limited",
                            "Commandes trop rapprochées")
                return
            self.server.last_command = now
        try:
            if path == "/api/v1/capabilities/execute":
                if not self.server.tooling_enabled:
                    raise ValueError("Toolbox indisponible")
                value = self._body({"capability_id", "object_id", "idempotency_key"})
                response = self.server.execute_dynamic_capability(
                    value["capability_id"], {}, value["object_id"],
                    str(uuid.uuid4()), value["idempotency_key"])
                self._json(HTTPStatus.OK, response)
                return
            if (path.startswith("/api/v1/tool-provisioning/") or
                    path.startswith("/api/v1/tool-integrations/")):
                if not self.server.tooling_enabled:
                    raise ValueError("Toolbox indisponible")
                parts = path.split("/")
                if len(parts) != 6:
                    raise TypeError("Route de décision toolbox invalide")
                value = self._human_decision_body()
                service = self.server.tool_provisioning
                record = service.get(parts[4])
                if record.get("workspace_id") != self.server.agent_workspace_id():
                    raise PermissionError("Demande d'un autre workspace")
                common = {"decision_id": value["decision_id"], "actor": "human",
                          "human_confirmed": True}
                action = parts[5]
                if parts[3] == "tool-provisioning" and action in {"approve", "approve-provision"}:
                    response = service.approve_provision(parts[4], **common)
                    def finish_build():
                        service.build(parts[4])
                        self.server._resume_after_tooling(record["turn_id"])
                    self.server._start_tooling_task("labfy-toolbox-build",
                        finish_build)
                elif parts[3] == "tool-provisioning" and action in {"reject", "reject-provision"}:
                    response = service.reject_provision(parts[4], reason=value["reason"], **common)
                    self.server._resume_after_tooling(record["turn_id"])
                elif parts[3] == "tool-integrations" and action == "approve-integration":
                    response = service.approve_integration(parts[4], **common)
                    response = service.activate(parts[4])
                    self.server._resume_after_tooling(record["turn_id"])
                elif parts[3] == "tool-integrations" and action == "reject-integration":
                    response = service.reject_integration(parts[4], reason=value["reason"], **common)
                    self.server._resume_after_tooling(record["turn_id"])
                else:
                    raise TypeError("Action toolbox inconnue")
                self._json(HTTPStatus.ACCEPTED, response)
                return
            if path.startswith("/api/v1/code-changes/"):
                if not self.server.tooling_enabled:
                    raise ValueError("Developer Agent indisponible")
                parts = path.split("/")
                if len(parts) != 6:
                    raise TypeError("Route de décision de code invalide")
                value = self._human_decision_body()
                service = self.server.code_changes
                turn_id = self.server.code_change_turn(parts[4])
                common = {"actor": "human", "decision_id": value["decision_id"],
                          "idempotency_key": value["idempotency_key"]}
                if parts[5] == "approve-prepare":
                    agent = self.server._ensure_developer_agent()
                    response = service.approve_prepare(parts[4], **common)
                    def finish_development():
                        agent.continue_development(parts[4])
                        self.server._resume_after_tooling(turn_id)
                    self.server._start_tooling_task("labfy-developer-agent",
                        finish_development)
                elif parts[5] == "reject-prepare":
                    response = service.reject_prepare(parts[4], reason=value["reason"], **common)
                    self.server._resume_after_tooling(turn_id)
                elif parts[5] == "approve-apply":
                    def finish_apply():
                        service.approve_apply(parts[4], **common)
                        self.server._resume_after_tooling(turn_id)
                    self.server._start_tooling_task("labfy-code-apply",
                        finish_apply)
                    response = {"contract": "labfy.code_change_apply_task.v1",
                                "change_id": parts[4], "state": "ACCEPTED"}
                elif parts[5] == "reject-apply":
                    response = service.reject_apply(parts[4], reason=value["reason"], **common)
                    self.server._resume_after_tooling(turn_id)
                else:
                    raise TypeError("Action de code inconnue")
                self._json(HTTPStatus.ACCEPTED, response)
                return
            if path == "/api/v1/agent-tools/calls":
                response = self.server.agent_gateway.call(
                    self.server.agent_scope(), self._json_body())
                self._json(HTTPStatus.OK if response["replayed"]
                           else HTTPStatus.CREATED, response)
                return
            if path == "/api/v1/agent-mission/start":
                value = self._agent_operation_body("mission.start")
                response = self.server.start_agent_mission(
                    value["workspace_id"], value["specification"],
                    value["idempotency_key"], value["human_confirmed"],
                )
                self._json(HTTPStatus.OK if response["replayed"]
                           else HTTPStatus.CREATED, response)
                return
            if path == "/api/v1/agent-mission/cancel":
                value = self._agent_operation_body("mission.cancel")
                response = self.server.cancel_agent_mission(
                    value["workspace_id"], value["mission_id"],
                    value["idempotency_key"], value["human_confirmed"],
                )
                self._json(HTTPStatus.OK, response)
                return
            if path == "/api/v1/agent-mission/rescope":
                value = self._agent_operation_body("mission.rescope")
                response = self.server.rescope_agent_mission(
                    value["workspace_id"], value["mission_id"],
                    value["scoped_refs"], value["pivots"],
                    value["idempotency_key"], value["human_confirmed"],
                )
                self._json(HTTPStatus.OK, response)
                return
            if path == "/api/v1/agent-proposals":
                value = self._agent_operation_body("proposal.create")
                response = self.server.create_agent_proposal(
                    value["workspace_id"], value["mission_id"],
                    value["proposal"], value["idempotency_key"],
                )
                self._json(HTTPStatus.OK if response["replayed"]
                           else HTTPStatus.CREATED, response)
                return
            if (path.startswith("/api/v1/agent-proposals/") and
                    path.endswith(("/approve", "/reject"))):
                parts = path.split("/")
                if len(parts) != 6:
                    raise TypeError("Route de décision de proposition invalide")
                value = self._agent_operation_body("proposal.decide")
                outcome = "APPROVED" if parts[5] == "approve" else "REFUSED"
                response = self.server.decide_agent_proposal(
                    value["workspace_id"], parts[4], outcome,
                    {name: value[name] for name in (
                        "decision_id", "reason", "decided_by", "decided_at")},
                    value["idempotency_key"],
                )
                self._json(HTTPStatus.OK, response)
                return
            if path == "/api/v1/agent-runtime/turns":
                self.server.agent_workspace_id()
                if self.server.agent_runtime is None:
                    raise AgentRuntimeError("Modèle local indisponible", status=503)
                response = self.server.agent_runtime.start_turn(
                    self.server.agent_scope(), self._json_body())
                self._json(HTTPStatus.OK if response.get("replayed") else HTTPStatus.ACCEPTED,
                           response)
                return
            if (path.startswith("/api/v1/agent-runtime/turns/") and
                    path.endswith("/resume")):
                parts = path.split("/")
                if len(parts) != 7:
                    raise ValueError("Route de reprise runtime invalide")
                self.server.agent_workspace_id()
                self._body(set())
                self._json(HTTPStatus.ACCEPTED,
                           self.server.resume_agent_runtime(parts[5]))
                return
            if (path.startswith("/api/v1/agent-runtime/turns/") and
                    path.endswith("/cancel")):
                parts = path.split("/")
                if len(parts) != 7:
                    raise ValueError("Route d’annulation runtime invalide")
                self.server.agent_workspace_id()
                if self.server.agent_runtime is None:
                    raise AgentRuntimeError("Modèle local indisponible", status=503)
                self._body(set())
                self._json(HTTPStatus.OK, self.server.agent_runtime.cancel_turn(
                    self.server.agent_scope(), parts[5]))
                return
            if path == "/api/v1/agent-tools/turns":
                response = self.server.agent_gateway.start_turn(
                    self.server.agent_scope(), self._json_body())
                self._json(HTTPStatus.CREATED, response)
                return
            if (path.startswith("/api/v1/agent-tools/turns/") and
                    path.endswith("/resume")):
                parts = path.split("/")
                if len(parts) != 7:
                    raise ValueError("Route de reprise invalide")
                response = self.server.agent_gateway.resume_turn(
                    self.server.agent_scope(), parts[5])
                self._json(HTTPStatus.OK, response)
                return
            if path == "/api/v1/library/workspaces":
                value = self._body({"title", "idempotency_key"})
                response = self.server.create_library_workspace(
                    value["title"], value["idempotency_key"])
                self._json(HTTPStatus.OK if response["replayed"] else HTTPStatus.CREATED,
                           response)
                return
            if path == "/api/v1/library/discover-existing":
                self._body(set())
                self._json(HTTPStatus.OK,
                           self.server.discover_existing_library_workspaces())
                return
            if path == "/api/v1/library/register-existing":
                value = self._library_register_body()
                response = self.server.register_existing_library_workspace(
                    value["candidate_id"], value["expected_generation"],
                    value["idempotency_key"], value["human_confirmed"])
                self._json(HTTPStatus.OK if response["replayed"]
                           else HTTPStatus.CREATED, response)
                return
            if path == "/api/v1/library/open":
                value = self._library_open_body()
                self._json(HTTPStatus.OK, self.server.open_library_workspace(
                    value["workspace_id"], value["expected_generation"]))
                return
            if path == "/api/v1/library/close":
                value = self._library_close_body()
                self._json(HTTPStatus.OK, self.server.close_library_workspace(
                    value["workspace_id"], value["expected_generation"],
                    value["idempotency_key"], value["human_confirmed"]))
                return
            if path == "/api/v1/workspace":
                if self.server.library is not None:
                    raise ValueError("Utiliser la création explicite de la bibliothèque")
                value = self._body({"title"})
                if self.server.context() is not None:
                    raise ValueError("L’espace est déjà créé")
                self.server.bridge_call(["create-workspace", "--title", value["title"]],
                                        timeout=30)
                context = self.server.context()
                self._json(HTTPStatus.CREATED, {"contract":"labfy.local_workspace.v1",
                    "state":"READY", "investigation_id":context["investigation_id"],
                    "title":context["title"]})
                return
            if path == "/api/v1/uploads":
                self._create_upload()
                return
            if path.startswith("/api/v1/uploads/") and path.endswith("/confirm"):
                self._confirm_upload(path.split("/")[4])
                return
            if path.startswith("/api/v1/uploads/") and path.endswith("/cancel"):
                self._cancel_upload(path.split("/")[4])
                return
            if path.startswith("/api/v1/evidence/"):
                parts = path.split("/")
                if len(parts) != 8 or parts[5] != "observations":
                    self._error(HTTPStatus.NOT_FOUND, "route_unknown", "Route inconnue")
                    return
                evidence_id, observation_id, operation = parts[4], parts[6], parts[7]
                if operation == "review":
                    value = self._review_body({"action", "verification_status",
                                               "corrected_value"})
                    if value["action"] == "decide":
                        if value["verification_status"] not in {
                                "proposed", "confirmed", "rejected", "conflicted"}:
                            raise TypeError("Décision de revue invalide")
                        result = self._review_command(evidence_id, observation_id,
                            value, "decide", status=value["verification_status"])
                    elif value["action"] == "correct" and value["corrected_value"]:
                        result = self._review_command(evidence_id, observation_id,
                            value, "correct", corrected=value["corrected_value"])
                    else:
                        raise TypeError("Action de revue invalide")
                elif operation == "promotion":
                    value = self._review_body({"action", "entity_id"})
                    if value["action"] == "create" and not value["entity_id"]:
                        result = self._review_command(evidence_id, observation_id,
                                                      value, "promote_create")
                    elif value["action"] == "attach":
                        uuid.UUID(value["entity_id"])
                        result = self._review_command(evidence_id, observation_id,
                            value, "promote_attach", entity=value["entity_id"])
                    else:
                        raise TypeError("Promotion invalide")
                elif operation == "withdraw":
                    value = self._review_body(set())
                    result = self._review_command(evidence_id, observation_id,
                                                  value, "withdraw")
                else:
                    self._error(HTTPStatus.NOT_FOUND, "route_unknown", "Route inconnue")
                    return
                self._json(HTTPStatus.OK, result)
                return
            if path == "/api/v1/plans":
                value = self._plan_body()
                lookup = json.loads(self.server.bridge_call([
                    "lookup-plan-json", "--revision", value["input_revision"],
                    "--profile", value["profile_id"], "--key",
                    value["idempotency_key"], "--recommendations",
                    ",".join(value["recommendation_ids"])]))
                if lookup.get("found"):
                    self._json(HTTPStatus.ACCEPTED, lookup)
                    return
                planner = json.loads((self.server.workspace /
                                      "planner-snapshot.json").read_text())
                if planner.get("input_revision") != value["input_revision"]:
                    raise ValueError("Projection du planner périmée")
                profiles = {item["id"]: item for item in planner["profiles"]}
                profile = profiles.get(value["profile_id"])
                if profile is None or len(value["recommendation_ids"]) > profile["max_analyses"]:
                    raise ValueError("Profil de budget inconnu ou insuffisant")
                recommendations = {item["id"]: item for item in
                                   planner["recommendations"]}
                total_bytes = 0
                for recommendation_id in value["recommendation_ids"]:
                    item = recommendations.get(recommendation_id)
                    if item is None or not item.get("available"):
                        raise ValueError("Recommandation absente ou indisponible")
                    total_bytes += item["source_size"]
                    if total_bytes > profile["max_source_bytes"]:
                        raise ValueError("Budget d'octets insuffisant")
                output = self.server.bridge_call([
                    "submit-plan-json", "--revision", value["input_revision"],
                    "--profile", value["profile_id"], "--key",
                    value["idempotency_key"], "--recommendations",
                    ",".join(value["recommendation_ids"])])
                self.server.bridge_call(["export"])
                self.server.start_worker()
                self._json(HTTPStatus.ACCEPTED, json.loads(output))
                return
            if path == "/api/v1/research/prepare":
                value = self._research_body("prepare")
                graph = json.loads((self.server.workspace /
                                    "core-snapshot.json").read_text())
                revision = graph.get("revision")
                if (not isinstance(revision, int) or isinstance(revision, bool)
                        or revision < 0):
                    raise ValueError("Révision du snapshot cœur invalide")
                output = self.server.bridge_call(["research-prepare-json",
                    "--selection", ",".join(value["selection_ids"]),
                    "--question", value["question"], "--exclusions",
                    ",".join(value["exclusions"]), "--revision",
                    str(revision), "--key",
                    value["idempotency_key"]], timeout=15)
                self._json(HTTPStatus.OK, json.loads(output)); return
            if path == "/api/v1/research/grants":
                value = self._research_body("grant")
                output = self.server.bridge_call(["research-grant-json",
                    "--plan", value["plan_id"], "--revision",
                    str(value["input_revision"]), "--actions",
                    ",".join(value["selected_action_ids"]), "--decisions",
                    ",".join(f'{item["action_id"]}={item["decision"]}'
                             for item in value["decisions"]), "--exclusions",
                    ",".join(value["exclusions"]), "--key",
                    value["idempotency_key"]], timeout=15)
                self._json(HTTPStatus.OK, json.loads(output)); return
            if path == "/api/v1/research/campaigns":
                value = self._research_body("campaign")
                output = self.server.bridge_call(["research-campaign-json",
                    "--grant", value["grant_id"], "--revision",
                    str(value["input_revision"]), "--actions",
                    ",".join(value["action_ids"]), "--key",
                    value["idempotency_key"]], timeout=20)
                self._json(HTTPStatus.OK, json.loads(output)); return
            if path == "/api/v1/research/revoke":
                value = self._research_body("revoke")
                output = self.server.bridge_call(["research-revoke-json",
                    "--grant", value["grant_id"]])
                self._json(HTTPStatus.OK, json.loads(output)); return
            if path == "/api/v1/reports/preview":
                value=self._report_preview_body();generated=time.strftime("%Y-%m-%dT%H:%M:%SZ",time.gmtime())
                output=self.server.bridge_call(["prepare-report-json","--objects",",".join(value["object_ids"]),"--title",value["title"],"--comment",value["comment"],"--generated-at",generated,"--sections",",".join(value["sections"])],timeout=15)
                document=json.loads(output);preview_dir=self.server.workspace/".labfy"/"reports"/"previews";preview_dir.mkdir(mode=0o700,parents=True,exist_ok=True)
                preview=(preview_dir/(document["revision"]+".json"));preview.write_text(json.dumps({"request":value,"document":document},ensure_ascii=False,sort_keys=True,separators=(",",":")),encoding="utf-8");os.chmod(preview,0o600)
                self._json(HTTPStatus.OK,document);return
            if path == "/api/v1/reports":
                value=self._body({"preview_revision","idempotency_key"})
                if not (len(value["preview_revision"])==64 and all(c in "0123456789abcdef" for c in value["preview_revision"])):
                    raise TypeError("Révision de prévisualisation invalide")
                preview=self.server.workspace/".labfy"/"reports"/"previews"/(value["preview_revision"]+".json")
                envelope=json.loads(preview.read_text(encoding="utf-8"));document=envelope["document"];request=envelope["request"]
                current=json.loads(self.server.bridge_call(["prepare-report-json","--objects",",".join(request["object_ids"]),"--title",request["title"],"--comment",request["comment"],"--generated-at",document["generated_at"],"--sections",",".join(request["sections"])],timeout=15))
                if document.get("revision")!=value["preview_revision"] or current.get("revision")!=value["preview_revision"]:raise ValueError("Prévisualisation périmée")
                report_id=self.server.start_report(document,value["idempotency_key"])
                self._json(HTTPStatus.ACCEPTED,{"contract":"labfy.report_admission.v1","report_id":report_id,"state":"GENERATING"});return
            if path == "/api/v1/jobs":
                value = self._body({"evidence_id", "capability_id",
                                    "idempotency_key"})
                if value["capability_id"] not in ALLOWED_CAPABILITIES:
                    raise TypeError("Capability non autorisée")
                output = self.server.bridge_call([
                    "submit-json", "--evidence", value["evidence_id"],
                    "--capability", value["capability_id"], "--key",
                    value["idempotency_key"],
                ])
                response = json.loads(output)
                self.server.bridge_call(["export"])
                self.server.start_worker()
                self._json(HTTPStatus.ACCEPTED, response)
                return
            if path in {"/api/v1/queue/pause", "/api/v1/queue/resume",
                        "/api/v1/queue/stop"}:
                self._body(set())
                command = path.rsplit("/", 1)[-1]
                self.server.bridge_call([command])
                if command == "stop":
                    self.server.stop_worker()
                self.server.bridge_call(["export"])
                if command == "resume":
                    self.server.start_worker()
                self._json(HTTPStatus.OK, {"contract": "labfy.workspace.control.v1",
                                           "control": command, "accepted": True})
                return
            if path.startswith("/api/v1/jobs/") and path.endswith("/cancel"):
                self._body(set())
                job_id = path.split("/")[4]
                result = json.loads(self.server.bridge_call(["cancel", job_id]))
                if result.get("was_running"):
                    self.server.stop_worker()
                    self.server.start_worker()
                self.server.bridge_call(["export"])
                self._json(HTTPStatus.OK, {"contract": "labfy.workspace.control.v1",
                                           "control": "cancel", "job_id": job_id,
                                           "accepted": True})
                return
            self._error(HTTPStatus.NOT_FOUND, "route_unknown", "Route inconnue")
        except (AgentGatewayError, AgentRuntimeError) as error:
            self._error(error.status, error.code, str(error))
        except (ProvisioningError, CodeChangeError, DeveloperAgentError) as error:
            self._error(HTTPStatus.CONFLICT, getattr(error, "code", "tooling_rejected"), str(error))
        except PermissionError as error:
            self._error(HTTPStatus.FORBIDDEN, "agent_workspace_rejected", str(error))
        except (AgentMissionError, AgentProposalError) as error:
            self._error(HTTPStatus.CONFLICT, "agent_operation_rejected", str(error))
        except OverflowError as error:
            self._error(HTTPStatus.REQUEST_ENTITY_TOO_LARGE, "body_too_large", str(error))
        except (TypeError, json.JSONDecodeError) as error:
            self._error(HTTPStatus.BAD_REQUEST, "invalid_request", str(error))
        except (OSError, ValueError, subprocess.TimeoutExpired) as error:
            self._error(HTTPStatus.CONFLICT, "command_rejected", str(error))

    def do_PUT(self):
        if not self._request_valid() or not self._mutation_allowed():
            self._error(HTTPStatus.FORBIDDEN, "mutation_rejected",
                        "Session, Origin ou preuve CSRF invalide")
            return
        path = urlsplit(self.path).path
        if not path.startswith("/api/v1/uploads/") or path.count("/") != 4:
            self._error(HTTPStatus.NOT_FOUND, "route_unknown", "Route inconnue")
            return
        upload_id = path.split("/")[4]
        try:
            self._receive_upload(upload_id)
        except OverflowError as error:
            self.close_connection = True
            self._error(HTTPStatus.REQUEST_ENTITY_TOO_LARGE, "upload_too_large", str(error))
        except (TypeError, ValueError, OSError, TimeoutError) as error:
            self.close_connection = True
            self._error(HTTPStatus.BAD_REQUEST, "upload_rejected", str(error))

    def _upload_dir(self, upload_id):
        if len(upload_id) != 36 or not all(c in "0123456789abcdef-" for c in upload_id):
            raise ValueError("Identifiant de réception invalide")
        return self.server.workspace / ".labfy" / "uploads" / upload_id

    @staticmethod
    def _write_intent(directory, intent):
        """Publish a private state transition without exposing partial JSON."""
        temporary = directory / "intent.json.part"
        temporary.write_text(json.dumps(intent, ensure_ascii=False, sort_keys=True,
                                        separators=(",", ":")), encoding="utf-8")
        os.chmod(temporary, 0o600)
        temporary.replace(directory / "intent.json")

    @staticmethod
    def _read_intent(directory):
        path = directory / "intent.json"
        if path.is_symlink() or not path.is_file():
            raise ValueError("Intention de réception absente ou invalide")
        value = json.loads(path.read_text(encoding="utf-8"))
        if not isinstance(value, dict) or value.get("contract") != "labfy.local_upload.v1":
            raise ValueError("Intention de réception mal formée")
        return value

    def _staging_bytes_locked(self, staging_root):
        total = self.server.reserved_upload_bytes
        if staging_root.is_dir():
            for candidate in staging_root.glob("*/payload"):
                try:
                    if candidate.is_file() and not candidate.is_symlink():
                        total += candidate.stat().st_size
                except OSError:
                    continue
        return total

    def _expire_uploads(self, staging_root):
        """Remove abandoned mutable uploads without racing their current owner."""
        if not staging_root.is_dir():
            return
        for candidate in staging_root.iterdir():
            mutex = self.server.upload_mutex(candidate.name)
            if not mutex.acquire(blocking=False):
                continue
            try:
                abandoned = self._read_intent(candidate)
                if (abandoned.get("state") in
                        {"RECEIVING", "FAILED", "CANCELLED", "EXPIRED"} and
                        time.time() - float(abandoned["created_at"]) >
                        UPLOAD_TTL_SECONDS):
                    shutil.rmtree(candidate)
            except (OSError, ValueError, KeyError, json.JSONDecodeError):
                continue
            finally:
                mutex.release()

    def _create_upload(self):
        if self.server.context() is None:
            raise ValueError("Créer l’enquête avant l’import")
        staging_root=self.server.workspace/".labfy"/"uploads"
        # WHY: les abandons ne doivent pas consommer indéfiniment le plafond
        # local. Une publication IMPORTED n'est jamais supprimée ici.
        self._expire_uploads(staging_root)
        value = self._body({"name", "size", "declared_type", "selection_id"})
        name = value["name"]
        if any(ch in name for ch in ("/", "\\", "\0")) or any(ord(ch) < 32 for ch in name):
            raise TypeError("Nom original dangereux")
        try: size = int(value["size"])
        except ValueError as error: raise TypeError("Taille invalide") from error
        if size < 0 or size > MAX_UPLOAD_BYTES:
            raise OverflowError("Limite de 4 Mio par fichier")
        try: uuid.UUID(value["selection_id"])
        except ValueError as error: raise TypeError("Identité de sélection invalide") from error
        upload_id = str(uuid.uuid4())
        directory = self._upload_dir(upload_id)
        # CONTRACT: selection count and bytes are admitted under the same lock
        # that publishes the new intent, so concurrent requests cannot evade it.
        with self.server.upload_lock:
            selected=[]
            if staging_root.is_dir():
                for candidate in staging_root.iterdir():
                    try:
                        item=self._read_intent(candidate)
                        if item.get("selection_id")==value["selection_id"]:
                            selected.append(item)
                    except (OSError,ValueError,json.JSONDecodeError):
                        continue
            if len(selected) >= MAX_SELECTION_FILES or sum(
                    int(item["expected_size"]) for item in selected)+size > MAX_SELECTION_BYTES:
                raise OverflowError("Sélection limitée à 8 fichiers et 16 Mio")
            directory.mkdir(mode=0o700, parents=True)
            intent = {"contract":"labfy.local_upload.v1", "upload_id":upload_id,
                  "workspace_id":self.server.context()["investigation_id"],
                  "selection_id":value["selection_id"],
                  "name":name, "expected_size":size,
                  "declared_type":value["declared_type"], "state":"RECEIVING",
                  "created_at":time.time(), "evidence_id":None,
                  "sha256":None, "recognized_type":None}
            self._write_intent(directory, intent)
        self._json(HTTPStatus.CREATED,intent)

    def _receive_upload(self, upload_id):
        if self.headers.get("Content-Type") != "application/octet-stream":
            raise TypeError("Content-Type application/octet-stream obligatoire")
        directory=self._upload_dir(upload_id)
        temporary=directory/"payload.part"; digest=hashlib.sha256(); received=0
        reserved = False
        with self.server.upload_mutex(upload_id):
            intent=self._read_intent(directory)
            if intent["workspace_id"] != self.server.context()["investigation_id"]:
                raise ValueError("Réception d’un autre espace")
            if intent.get("state") != "RECEIVING":
                raise ValueError("Cette intention de réception est déjà figée")
            length = self._content_length(MAX_UPLOAD_BYTES,
                                          expected=intent["expected_size"])
            staging_root=directory.parent
            # CONTRACT: places and bytes are one atomic admission decision; a
            # concurrent receiver cannot spend the same staging balance.
            with self.server.upload_lock:
                if self.server.active_uploads >= MAX_ACTIVE_UPLOADS:
                    raise ValueError("Deux réceptions sont déjà actives")
                if self._staging_bytes_locked(staging_root) + length > MAX_STAGING_BYTES:
                    raise OverflowError("Staging limité à 64 Mio")
                self.server.active_uploads += 1
                self.server.reserved_upload_bytes += length
                reserved = True
            try:
                intent["state"] = "TRANSFERRING"
                self._write_intent(directory, intent)
                with temporary.open("xb") as output:
                    os.chmod(temporary,0o600)
                    body = self._read_exact_body(length)
                    while received < length:
                        block = body[received:received + 65536]
                        output.write(block);digest.update(block);received+=len(block)
                with temporary.open("rb") as uploaded:
                    head=uploaded.read(4096)
                suffix=Path(intent["name"]).suffix.lower()
                if suffix==".png" and head.startswith(b"\x89PNG\r\n\x1a\n"): recognized="photo"
                elif suffix in {".jpg",".jpeg"} and head.startswith(b"\xff\xd8\xff"): recognized="photo"
                elif suffix==".eml" and b"\r\n\r\n" in head and any(
                        head.lower().startswith(prefix) for prefix in
                        (b"from:", b"return-path:", b"received:", b"date:")):
                    recognized="email"
                else: raise ValueError("Format ou structure non pris en charge")
                temporary.replace(directory/"payload")
                intent.update(state="PREPARED",sha256=digest.hexdigest(),
                              recognized_type=recognized,evidence_id=str(uuid.uuid4()))
                self._write_intent(directory, intent)
                self._json(HTTPStatus.OK,intent)
            except Exception:
                intent["state"] = "FAILED"
                self._write_intent(directory, intent)
                raise
            finally:
                if reserved:
                    with self.server.upload_lock:
                        self.server.active_uploads -= 1
                        self.server.reserved_upload_bytes -= length
                    reserved = False
                if temporary.exists(): temporary.unlink()

    def _confirm_upload(self, upload_id):
        value=self._import_confirmation_body()
        directory=self._upload_dir(upload_id)
        with self.server.upload_mutex(upload_id):
            intent=self._read_intent(directory)
            if intent["state"] not in {"PREPARED","CONFIRMING","IMPORTED"}:
                raise ValueError("Réception non préparée")
            # CONTRACT: confirmation metadata becomes immutable before the C
            # publication can have any effect, including its crash window.
            confirmation={"contract":"labfy.local_import.intent.v1",
                "idempotency_key":value["idempotency_key"],
                "source":value["source"],"description":value["description"]}
            previous=intent.get("confirmation")
            if previous is not None and previous != confirmation:
                raise ValueError("Intention de confirmation contradictoire")
            intent["confirmation"]=confirmation
            if intent["state"] != "IMPORTED": intent["state"]="CONFIRMING"
            self._write_intent(directory,intent)
            output=self.server.bridge_call(["confirm-import-json","--upload",upload_id,
              "--key",value["idempotency_key"],"--evidence",intent["evidence_id"],
              "--name",intent["name"],"--type",intent["recognized_type"],
              "--size",str(intent["expected_size"]),"--sha256",intent["sha256"],
              "--source",value["source"],"--description",value["description"]],timeout=30)
            intent["state"]="IMPORTED";self._write_intent(directory,intent)
            try: (directory/"payload").unlink()
            except FileNotFoundError: pass
            self._json(HTTPStatus.OK,json.loads(output))

    def _cancel_upload(self, upload_id):
        self._body(set());directory=self._upload_dir(upload_id)
        with self.server.upload_mutex(upload_id):
            intent=self._read_intent(directory)
            if intent["state"] in {"CONFIRMING","IMPORTED"}:
                raise ValueError("Une publication engagée ne peut pas être annulée")
            intent["state"]="CANCELLED";self._write_intent(directory,intent)
            for name in ("payload","payload.part"):
                try:(directory/name).unlink()
                except FileNotFoundError:pass
            self._json(HTTPStatus.OK,{"contract":"labfy.local_upload.v1","state":"CANCELLED"})

    def _open_automatic_session(self):
        with self.server.session_lock:
            self.server.session = secrets.token_urlsafe(32)
            self.server.csrf = secrets.token_urlsafe(32)
            self.server.session_deadline = (time.monotonic() +
                                            self.server.session_ttl_seconds)
            cookie = (f"{self.server.cookie_name}={self.server.session}; "
                      "HttpOnly; SameSite=Strict; "
                      f"Path=/; Max-Age={self.server.session_ttl_seconds}")
        # INVARIANT: aucun secret n'est inclus dans l'URL ou dans le document;
        # l'application récupère seulement le CSRF depuis la même origine.
        self.send_response(HTTPStatus.SEE_OTHER)
        self._headers()
        self.send_header("Set-Cookie", cookie)
        self.send_header("Location", "/")
        self.send_header("Content-Length", "0")
        self.end_headers()

    def _serve_export(self, name, contract):
        path = self.server.workspace / name
        try:
            if path.stat().st_size > MAX_EXPORT:
                raise ValueError("export trop volumineux")
            value = json.loads(path.read_text(encoding="utf-8"))
            accepted = contract if isinstance(contract, set) else {contract}
            if value.get("contract") not in accepted:
                raise ValueError("contrat d'export inattendu")
            self._json(HTTPStatus.OK, value)
        except (OSError, ValueError, json.JSONDecodeError):
            self._error(HTTPStatus.SERVICE_UNAVAILABLE, "export_unavailable",
                        "Dernier export C valide indisponible")

    def _serve_fixture(self):
        self._error(HTTPStatus.GONE, "module_export_removed",
                    "Utiliser /api/v1/snapshot en JSON validé")

    def _serve_static(self, request_path):
        relative = "index.html" if request_path == "/" else request_path.lstrip("/")
        candidate = (PUBLIC / relative).resolve()
        if (PUBLIC not in candidate.parents and candidate != PUBLIC) or not candidate.is_file():
            self._error(HTTPStatus.NOT_FOUND, "not_found", "Ressource absente")
            return
        self._bytes(HTTPStatus.OK,
                    mimetypes.guess_type(candidate.name)[0] or "application/octet-stream",
                    candidate.read_bytes())

    def _bytes(self, status, content_type, body):
        self.send_response(status)
        self._headers()
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(body)

    def log_message(self, fmt, *args):
        print(f"workspace: {self.address_string()} {fmt % args}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--workspace", type=Path, required=True)
    parser.add_argument("--bridge", type=Path, required=True)
    def port(value):
        parsed = int(value)
        if not 0 <= parsed <= 65535:
            raise argparse.ArgumentTypeError("port hors limites")
        return parsed
    parser.add_argument("--port", type=port, default=8081)
    parser.add_argument("--agent-mode", choices=("deterministic-demo", "local-model"),
                        default=os.environ.get("LABFY_AGENT_MODE", "deterministic-demo"))
    parser.add_argument("--agent-endpoint", default=os.environ.get("LABFY_AGENT_MODEL_ENDPOINT"))
    parser.add_argument("--agent-model", default=os.environ.get("LABFY_AGENT_MODEL_ID"))
    parser.add_argument("--agent-timeout", type=float,
                        default=float(os.environ.get("LABFY_AGENT_MODEL_TIMEOUT_SECONDS", "10")))
    parser.add_argument("--agent-autostart", action="store_true",
                        help="démarre uniquement le llama-server configuré dans XDG")
    parser.add_argument("--agent-config-path", type=Path,
                        help="configuration agent XDG explicite, non versionnée")
    args = parser.parse_args()
    server = WorkspaceServer(("127.0.0.1", args.port), Handler,
                             workspace=args.workspace, bridge=args.bridge,
                             bootstrap="", agent_mode=args.agent_mode,
                             agent_endpoint=args.agent_endpoint, agent_model=args.agent_model,
                             agent_timeout=args.agent_timeout,
                             agent_autostart=args.agent_autostart,
                             agent_config_path=args.agent_config_path)
    print(f"Labfy J6 : {server.origin}/", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.stop_worker()
        server.server_close()


if __name__ == "__main__":
    main()
