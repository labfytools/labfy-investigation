"""Smoke manuel complet de l'agent Qwen opérationnel sur SPECIMEN uniquement."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
import tempfile
import threading
import time
import uuid
from http.client import HTTPConnection
from pathlib import Path

PROTOTYPE = Path(__file__).resolve().parents[1]
REPOSITORY = Path(__file__).resolve().parents[3]
BRIDGE = REPOSITORY / "tools" / "local-jobs"
sys.path.insert(0, str(PROTOTYPE))

from workspace_server import Handler, WorkspaceServer  # noqa: E402
from privacy_egress import PodmanTorRuntime, TorRuntimeState  # noqa: E402


TERMINAL_STATES = {
    "COMPLETED",
    "FAILED",
    "CANCELLED",
    "MODEL_UNAVAILABLE",
    "MODEL_PROTOCOL_ERROR",
    "BUDGET_EXHAUSTED",
}
EXPECTED_TOOLS = [
    "investigation.get_overview",
    "investigation.search",
    "investigation.find_correlations",
    "tool.catalog",
    "tool.docs.read",
    "sandbox.exec",
    "agent.propose",
    "web.fetch",
]


class SmokeError(RuntimeError):
    """Erreur de smoke contrôlée."""


class QuietHandler(Handler):
    def log_message(self, _format, *_args):
        pass


class Client:
    def __init__(self, server):
        self.server = server
        self.cookie = None
        self.csrf = None
        self.workspace_id = None

    def request(self, method, path, value=None, *, mutation=False):
        if mutation:
            time.sleep(0.04)
        connection = HTTPConnection("127.0.0.1", self.server.server_port, timeout=30)
        headers = {"Host": self.server.authority}
        if self.cookie:
            headers["Cookie"] = self.cookie
        if mutation:
            headers["Origin"] = self.server.origin
            headers["X-Labfy-CSRF"] = self.csrf
        body = None
        if value is not None:
            headers["Content-Type"] = "application/json"
            body = json.dumps(
                value, ensure_ascii=False, separators=(",", ":")
            ).encode("utf-8")
        try:
            connection.request(method, path, body=body, headers=headers)
            response = connection.getresponse()
            data = response.read()
            response_headers = dict(response.getheaders())
        finally:
            connection.close()
        try:
            decoded = json.loads(data) if data else None
        except (UnicodeError, json.JSONDecodeError) as error:
            raise SmokeError(f"Réponse JSON invalide pour {path}") from error
        return response.status, response_headers, decoded

    def expect(self, method, path, statuses, value=None, *, mutation=False):
        status, headers, body = self.request(
            method, path, value, mutation=mutation
        )
        if status not in statuses:
            raise SmokeError(f"HTTP {status} inattendu pour {path}")
        return headers, body

    def open_session(self):
        status, headers, _body = self.request("GET", "/")
        if status != 303:
            raise SmokeError("Session automatique indisponible")
        cookie = headers.get("Set-Cookie")
        if not isinstance(cookie, str):
            raise SmokeError("Cookie de session absent")
        self.cookie = cookie.split(";", 1)[0]
        _headers, session = self.expect("GET", "/api/v1/session", {200})
        self.csrf = session.get("csrf")
        self.workspace_id = session.get("investigation_id")
        if not isinstance(self.csrf, str) or not self.csrf:
            raise SmokeError("CSRF absent")
        if not isinstance(self.workspace_id, str) or not self.workspace_id:
            raise SmokeError("Workspace opaque absent")


def arguments():
    parser = argparse.ArgumentParser()
    parser.add_argument("--model-path", type=Path, required=True)
    parser.add_argument("--model-sha256", required=True)
    parser.add_argument(
        "--model-id", default="Huihui-Qwen3.5-9B-Q4_K_M.gguf"
    )
    parser.add_argument("--model-timeout", type=float, default=30.0)
    parser.add_argument("--turn-timeout", type=float, default=600.0)
    parser.add_argument("--skip-tor-build", action="store_true")
    return parser.parse_args()


def verify_model(path, expected):
    # INVARIANT: seul le poids local explicite est lu hors du dépôt ; même une
    # option CLI ne transforme jamais une enquête réelle en entrée de smoke.
    forbidden = (Path.home() / "Documents" / "Investigations").absolute()
    candidate = path.expanduser().absolute()
    if candidate.is_relative_to(forbidden):
        raise SmokeError("Modèle dans une enquête réelle interdit")
    if candidate.is_symlink():
        raise SmokeError("Lien symbolique modèle refusé")
    resolved = path.expanduser().resolve(strict=True)
    if resolved.is_relative_to(forbidden):
        raise SmokeError("Modèle dans une enquête réelle interdit")
    if not resolved.is_file() or resolved.is_symlink():
        raise SmokeError("Fichier modèle invalide")
    digest = hashlib.sha256()
    with resolved.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    actual = digest.hexdigest()
    if actual != expected.lower():
        raise SmokeError("SHA-256 modèle inattendu")
    return resolved, actual


def run_bridge(workspace, *argv):
    completed = subprocess.run(
        [str(BRIDGE), *argv, "--workspace", str(workspace)],
        cwd=REPOSITORY,
        capture_output=True,
        text=True,
        timeout=30,
        check=False,
    )
    if completed.returncode != 0:
        raise SmokeError(f"Initialisation SPECIMEN refusée: {argv[0]}")


def wait_turn(client, turn_id, timeout):
    deadline = time.monotonic() + timeout
    path = f"/api/v1/agent-runtime/turns/{turn_id}"
    while time.monotonic() < deadline:
        _headers, turn = client.expect("GET", path, {200})
        state = turn.get("state")
        if state in TERMINAL_STATES:
            return turn
        if state == "AUTHORIZATION_REQUIRED":
            raise SmokeError("Autorisation inattendue dans le smoke opérationnel")
        time.sleep(0.25)
    raise SmokeError("Timeout global du turn Qwen")


def tool_sequence(client, turn_id):
    _headers, events = client.expect(
        "GET", "/api/v1/agent-runtime/events?cursor=0", {200}
    )
    return [
        event.get("payload", {}).get("tool_id")
        for event in events.get("events", [])
        if event.get("kind") == "agent.runtime.tool_requested"
        and event.get("turn_id") == turn_id
    ]


def tool_results(server, turn_ids):
    """Relit les résultats des turns d'une seule mission, sans fabriquer de sorties."""
    scope = server.agent_scope()
    events = server.agent_gateway.events(scope, 0)["events"]
    results = {}
    for event in events:
        if event.get("turn_id") not in turn_ids or event.get("kind") != "agent.tool.completed":
            continue
        payload = event["payload"]
        result = server.agent_gateway.result(scope, payload["result_id"])
        results.setdefault(payload["tool_id"], []).append(result)
    return results


def completed_output(results, tool_id):
    values = results.get(tool_id, [])
    if not values or values[-1].get("state") != "COMPLETED":
        diagnostic = values[-1].get("diagnostic") if values else "absent"
        raise SmokeError(f"{tool_id} non achevé: {diagnostic}")
    return values[-1]["output"]


def ensure_tor_image(skip_build):
    """Réutilise l'image locale ; le build reste une action explicite du smoke."""
    image = PodmanTorRuntime.IMAGE
    exists = subprocess.run(
        ["/usr/bin/podman", "image", "exists", image],
        check=False, capture_output=True, timeout=15,
    )
    if exists.returncode == 0:
        return image
    if skip_build:
        raise SmokeError("Image Tor locale absente avec --skip-tor-build")
    from manual_privacy_tor_smoke import build_image
    build_image(image)
    return image


def run_model_turn(client, objective, timeout):
    """Une étape de la même mission, exécutée entièrement par AgentRuntime."""
    if len(objective) > 512:
        raise SmokeError("Objectif de turn trop long")
    _headers, started = client.expect("POST", "/api/v1/agent-runtime/turns", {202},
        {"objective": objective, "idempotency_key": str(uuid.uuid4())}, mutation=True)
    turn_id = started.get("turn_id")
    if not isinstance(turn_id, str):
        raise SmokeError("Turn Qwen absent")
    final = wait_turn(client, turn_id, timeout)
    sequence = tool_sequence(client, turn_id)
    if final.get("state") != "COMPLETED" or not isinstance(final.get("final"), str) or not final["final"].strip():
        raise SmokeError(f"Turn {turn_id} non COMPLETED: {final.get('state')}; "
                         f"diagnostic={final.get('diagnostic')}; outils={sequence}")
    return turn_id, final, sequence


def verify_outputs(server, client, turn_ids, final, sequence, refs, tor_runtime,
                   workspace, initial_grants):
    """Exige les résultats gateway de la même mission et leur provenance."""
    results = tool_results(server, turn_ids)
    if final.get("state") != "COMPLETED":
        raise SmokeError(
            f"Turn {final.get('state')}: {final.get('diagnostic')}; outils={sequence}"
        )
    if not isinstance(final.get("final"), str) or not final["final"].strip():
        raise SmokeError("Final Qwen absent")
    if any(tool not in sequence for tool in EXPECTED_TOOLS):
        raise SmokeError(f"Outils manquants: {sorted(set(EXPECTED_TOOLS) - set(sequence))}")
    for tool in EXPECTED_TOOLS:
        completed_output(results, tool)

    overview = completed_output(results, "investigation.get_overview")
    search = completed_output(results, "investigation.search")
    correlation = completed_output(results, "investigation.find_correlations")
    catalog = completed_output(results, "tool.catalog")
    docs = completed_output(results, "tool.docs.read")
    sandbox = completed_output(results, "sandbox.exec")
    proposal_output = completed_output(results, "agent.propose")
    web = completed_output(results, "web.fetch")
    owned = {ref["object_id"] for ref in refs}
    if overview.get("contract") != "labfy.investigation_context.v1" or overview.get("counts", {}).get("evidence", 0) < 2:
        raise SmokeError("Overview structuré SPECIMEN invalide")
    if not search.get("object_refs") or not any(ref["object_id"] in owned for ref in search["object_refs"]):
        raise SmokeError("Recherche interne sans objet SPECIMEN")
    candidates = correlation.get("candidates", [])
    if not candidates or not any(len(set(ref["object_id"] for ref in item.get("object_refs", []))) >= 2 for item in candidates):
        raise SmokeError("Candidate correlation multi-source absente")
    tools = catalog.get("tools", [])
    if catalog.get("contract") != "labfy.agent_tool_catalog.v1" or not any(item.get("tool_id") == "forensics.strings" and item.get("execution_available") for item in tools):
        raise SmokeError("ToolRegistry réel indisponible")
    if docs.get("tool_id") != "forensics.strings" or docs.get("trust") != "UNTRUSTED_DATA" or not docs.get("document_id"):
        raise SmokeError("Documentation outil non vérifiée")
    provenance = sandbox.get("provenance", {})
    if sandbox.get("status") != "SUCCESS" or provenance.get("isolation") != "bwrap" or provenance.get("network") != "OFFLINE" or not sandbox.get("object_refs") or not all(ref["object_id"] in owned for ref in sandbox["object_refs"]):
        raise SmokeError("Sandbox réel offline non prouvé")
    if (not isinstance(sandbox.get("stdout"), str)
            or str(workspace) in sandbox["stdout"]
            or len(sandbox["stdout"].encode()) > 64 * 1024):
        raise SmokeError("Sortie sandbox invalide ou non bornée")
    if not sandbox["stdout"]:
        raise SmokeError("Analyse sandbox sans sortie de l'artefact matérialisé")
    if proposal_output.get("proposal", {}).get("status") != "CANDIDATE":
        raise SmokeError("Proposition candidate absente du résultat tool")
    _headers, proposals = client.expect("GET", "/api/v1/agent-proposals", {200})
    proposal = next((item for item in proposals.get("proposals", []) if item.get("proposal_id") == proposal_output.get("proposal_id")), None)
    if proposal is None or proposal.get("decision") is not None or proposal["proposal"].get("risk_class") != "LOCAL_READ_ONLY" or not all(ref["object_id"] in owned for ref in proposal["proposal"]["object_refs"]):
        raise SmokeError("Proposition hors scope ou autorisation inattendue")
    web_provenance = web.get("provenance", {})
    if web.get("status") != "SUCCESS" or web.get("content_trust") != "UNTRUSTED_DATA" or web_provenance.get("mode") != "PRIVACY_TOR" or web_provenance.get("direct_fallback") is not False:
        raise SmokeError("Contact Internet Privacy Tor non prouvé")
    if tor_runtime.state != TorRuntimeState.READY or tor_runtime.reason != "TOR_EGRESS_VERIFIED":
        raise SmokeError("Tor non READY à la fin du turn")
    _headers, current = client.expect("GET", "/api/v1/agent-mission/current", {200})
    used = current.get("mission", {}).get("used", {})
    if used.get("contacts") != 1 or used.get("tool_calls") != 3:
        raise SmokeError("Budgets mission non consommés selon le contrat")
    _headers, research_after = client.expect("GET", "/api/v1/research", {200})
    final_grants = research_after.get("grants")
    if not isinstance(final_grants, list) or final_grants != initial_grants:
        raise SmokeError("Grant de policy créé par le modèle ou la proposition")
    return proposal, used, web_provenance, final_grants, provenance


def main():
    args = arguments()
    summary = {"contract": "labfy.manual_operational_agent_smoke.v1", "ok": False,
               "phase": "startup", "cleanup": False}
    server = None
    server_thread = None
    tor_runtime = None
    workspace = None
    started = time.monotonic()
    try:
        if not 0.1 <= args.model_timeout <= 120 or not 30 <= args.turn_timeout <= 900:
            raise SmokeError("Délais hors limites")
        model_path, model_sha256 = verify_model(args.model_path, args.model_sha256)
        summary["model_sha256"] = model_sha256
        summary["phase"] = "fixture"
        with tempfile.TemporaryDirectory(prefix="labfy-Qwen-SPECIMEN-") as directory:
            root = Path(directory).resolve()
            workspace = root / "workspace"
            if "SPECIMEN" not in root.name or not root.is_relative_to(Path(tempfile.gettempdir()).resolve()):
                raise SmokeError("Workspace non SPECIMEN ou hors temporaire")
            config = root / "agent.json"
            run_bridge(workspace, "init-j7-specimen")
            run_bridge(workspace, "export")
            config.write_text(json.dumps({"model": {"model_path": str(model_path),
                "model_id": args.model_id, "context": 4096, "parallel": 1,
                "reasoning": "off"}}, separators=(",", ":")), encoding="utf-8")
            config.chmod(0o600)
            summary["phase"] = "model_server"
            server = WorkspaceServer(("127.0.0.1", 0), QuietHandler,
                workspace=workspace, bridge=BRIDGE, agent_autostart=True,
                agent_config_path=config, agent_timeout=args.model_timeout)
            server_thread = threading.Thread(target=server.serve_forever, daemon=True)
            server_thread.start()
            client = Client(server)
            client.open_session()
            model_status = server.agent_runtime_status()
            if model_status.get("available") is not True or model_status.get("supervisor", {}).get("state") != "READY":
                raise SmokeError("LocalModelSupervisor non READY")
            summary["model_health"] = True
            summary["model_supervisor"] = True
            summary["workspace_specimen"] = True
            graph = server._agent_graph()
            refs = [{"object_id": node["id"]} for node in graph.get("nodes", [])
                    if node.get("group") == "evidence"]
            if len(refs) < 2 or not all(server._agent_ref_owned(client.workspace_id, ref) for ref in refs):
                raise SmokeError("Object refs SPECIMEN invalides")

            summary["phase"] = "mission"
            _headers, mission_state = client.expect("POST", "/api/v1/agent-mission/start", {201}, {
                "workspace_id": client.workspace_id,
                "specification": {"goal": "Enquêter sur les pivots SPECIMEN",
                    "scoped_refs": refs, "pivots": refs[:1],
                    "allowed_risk_classes": ["LOCAL_READ_ONLY", "PASSIVE_PUBLIC"],
                    "network_profile": "PASSIVE_PUBLIC", "max_contacts": 2,
                    "max_duration_seconds": 600, "max_tool_calls": 3},
                "human_confirmed": True, "idempotency_key": str(uuid.uuid4())}, mutation=True)
            if mission_state.get("state") != "ACTIVE":
                raise SmokeError("Mission human-started inactive")
            if mission_state.get("mission", {}).get("used", {}).get("contacts") != 0:
                raise SmokeError("Budget de contacts non nul au démarrage")
            summary["mission_id"] = mission_state.get("mission", {}).get("mission_id")
            summary["mission"] = bool(summary["mission_id"])
            summary["mission_scope_refs"] = len(refs)
            summary["mission_network_profile"] = mission_state["mission"]["network_profile"]
            _headers, research_before = client.expect("GET", "/api/v1/research", {200})
            initial_grants = research_before.get("grants")
            if not isinstance(initial_grants, list) or initial_grants:
                raise SmokeError("Fixture SPECIMEN contient un grant préexistant")

            summary["phase"] = "tor_bootstrap"
            summary["tor_image"] = ensure_tor_image(args.skip_tor_build)
            server._ensure_agent_privacy_egress()
            tor_resources = server.agent_privacy_egress._owned_resources
            tor_runtime = next((item for item in tor_resources if isinstance(item, PodmanTorRuntime)), None)
            if tor_runtime is None or tor_runtime.state != TorRuntimeState.READY or tor_runtime.reason != "TOR_EGRESS_VERIFIED":
                raise SmokeError("Tor live sans egress vérifié")
            summary["privacy_tor"] = True
            summary["tor_runtime_reason"] = tor_runtime.reason
            summary["tor_container"] = tor_runtime.container_name

            # WHY: les turns successifs partagent le même WorkspaceServer et
            # la même mission. La référence transmise aux étapes suivantes
            # provient exclusivement du résultat search du premier turn.
            turn_ids = []
            finals = []
            sequence = []
            objectives = [(
                "SPECIMEN. Un seul appel JSON par réponse, attends son résultat. "
                "Appelle investigation.get_overview, puis investigation.search "
                "avec query SPECIMEN, puis investigation.find_correlations, "
                "puis final bref. Aucun autre outil."
            )]
            for index, objective in enumerate(objectives):
                summary["phase"] = f"agent_turn_{index + 1}"
                turn_id, final, actions = run_model_turn(client, objective, args.turn_timeout)
                turn_ids.append(turn_id)
                finals.append(final)
                sequence.extend(actions)
            search = completed_output(tool_results(server, turn_ids), "investigation.search")
            selected_ref = next((ref for ref in search.get("object_refs", [])
                                 if ref in refs), None)
            if selected_ref is None:
                raise SmokeError("Search Qwen sans référence SPECIMEN utilisable")
            selected_id = selected_ref["object_id"]
            objectives = [
                ("Continue la même mission SPECIMEN. Un seul appel JSON par réponse. "
                 "Appelle tool.catalog, puis tool.docs.read pour forensics.strings, "
                 "puis sandbox.exec avec tool_id forensics.strings, object_id "
                 f"{selected_id}, arguments [artifact://evidence]. Puis final bref."),
                ("Continue la même mission SPECIMEN. Un seul appel JSON par réponse. "
                 "D'abord agent.propose avec object_refs exactement "
                 f"{json.dumps([selected_ref])}, risk_class LOCAL_READ_ONLY, "
                 "suggested_capability investigation.find_correlations et tous "
                 "les champs requis. Puis web.fetch avec object_id exactement "
                 f"{selected_id}, url https://example.com/, method HEAD via Tor. "
                 "Conserve le préfixe evidence:. Puis final bref."),
            ]
            for index, objective in enumerate(objectives, start=2):
                summary["phase"] = f"agent_turn_{index}"
                turn_id, final, actions = run_model_turn(client, objective, args.turn_timeout)
                turn_ids.append(turn_id)
                finals.append(final)
                sequence.extend(actions)
                _headers, mission_current = client.expect(
                    "GET", "/api/v1/agent-mission/current", {200})
                if mission_current.get("mission", {}).get("mission_id") != summary["mission_id"]:
                    raise SmokeError("Continuité de mission perdue entre turns")
            summary.update({"turn_id": turn_ids[-1], "turn_ids": turn_ids,
                "state": finals[-1]["state"], "action_sequence": sequence,
                "model_calls": sum(item["budgets"]["model_calls"] for item in finals),
                "tool_calls": sum(item["budgets"]["tool_calls"] for item in finals)})
            summary["phase"] = "result_verification"
            proposal, used, web_provenance, final_grants, sandbox_provenance = verify_outputs(
                server, client, turn_ids, finals[-1], sequence, refs, tor_runtime,
                workspace, initial_grants)
            summary.update({"proposal_id": proposal["proposal_id"],
                "proposal_risk": proposal["proposal"]["risk_class"],
                "proposal_creates_grant": len(final_grants) > len(initial_grants),
                "model_self_authorization": bool(final_grants),
                "network_contacts": used["contacts"], "mission_tool_calls": used["tool_calls"],
                "direct_fallback": web_provenance["direct_fallback"],
                "host_shell": sandbox_provenance.get("isolation") != "bwrap",
                "overview": True, "internal_search": True, "correlation": True,
                "tool_registry": True, "tool_docs": True, "sandbox_offline": True,
                "proposal": True, "passive_web": True, "final_present": True})
    except (SmokeError, OSError, ValueError, subprocess.TimeoutExpired) as error:
        summary["error_type"] = type(error).__name__
        summary["error"] = str(error)[:512]
    finally:
        if server is not None:
            try:
                server.stop_worker()
                server.shutdown()
                server.server_close()
            except (OSError, RuntimeError) as error:
                summary["cleanup_error"] = str(error)[:256]
        if server_thread is not None:
            server_thread.join(timeout=5)
        model_stopped = (server is None or server.local_model_supervisor is None or
                         server.local_model_supervisor.process is None)
        tor_stopped = (tor_runtime is None or (tor_runtime.state == TorRuntimeState.STOPPED
                                              and not tor_runtime._created))
        summary["cleanup"] = bool(model_stopped and tor_stopped and
                                   (server_thread is None or not server_thread.is_alive())
                                   and (workspace is None or not workspace.exists()))
        summary["elapsed_seconds"] = round(time.monotonic() - started, 3)
    summary["ok"] = bool(summary.get("final_present") and summary["cleanup"] and
                         summary.get("state") == "COMPLETED")
    if summary["ok"]:
        summary.pop("phase", None)
    print(json.dumps(summary, ensure_ascii=False, sort_keys=True))
    return 0 if summary["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
