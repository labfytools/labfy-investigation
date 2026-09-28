#!/usr/bin/env python3
"""Smoke manuel d'un vrai modèle local via les frontières HTTP Labfy."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
import sys
import tempfile
import threading
import time
import uuid
from http.client import HTTPConnection
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from workspace_server import Handler, WorkspaceServer


REPOSITORY = Path(__file__).resolve().parents[3]
BRIDGE = REPOSITORY / "tools" / "local-jobs"
TERMINAL_STATES = {
    "COMPLETED", "FAILED", "CANCELLED", "MODEL_UNAVAILABLE",
    "MODEL_PROTOCOL_ERROR", "BUDGET_EXHAUSTED",
}


class SmokeError(RuntimeError):
    """Erreur contrôlée dont le message ne reprend aucun contenu non fiable."""


class SpecimenProvider(BaseHTTPRequestHandler):
    """Fournisseur réseau synthétique, local et comptabilisé."""

    contacts = []
    contacts_lock = threading.Lock()
    protocol_version = "HTTP/1.1"

    def do_GET(self):
        with type(self).contacts_lock:
            type(self).contacts.append(self.path)
        body = json.dumps({
            "contract": "labfy.fixture.provider.v1",
            "status": "SPECIMEN",
        }, separators=(",", ":")).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, _format, *_args):
        pass


class QuietHandler(Handler):
    """Handler produit sans journal HTTP parasite dans le résumé JSON."""

    def log_message(self, _format, *_args):
        pass


def parse_arguments():
    parser = argparse.ArgumentParser(
        description="Valide manuellement un vrai modèle Qwen local avec un workspace SPECIMEN.")
    parser.add_argument("--endpoint", required=True,
                        help="endpoint OpenAI-compatible loopback déjà démarré")
    parser.add_argument("--model", required=True,
                        help="identifiant de modèle envoyé à l'endpoint")
    parser.add_argument("--model-path", type=Path,
                        help="fichier modèle local à vérifier sans l'exécuter")
    parser.add_argument("--model-sha256",
                        help="SHA-256 attendu du fichier passé avec --model-path")
    parser.add_argument("--model-timeout", type=float, default=30.0,
                        help="délai d'une inférence, en secondes (défaut: 30)")
    parser.add_argument("--turn-timeout", type=float, default=180.0,
                        help="délai de chaque phase du turn (défaut: 180)")
    return parser.parse_args()


def verify_model_file(path, expected_hash):
    if expected_hash is not None and path is None:
        raise SmokeError("--model-sha256 exige --model-path")
    if path is None:
        return {"path_checked": False, "hash_checked": False}
    try:
        resolved = path.expanduser().resolve(strict=True)
    except OSError as error:
        raise SmokeError("fichier modèle inaccessible") from error
    if not resolved.is_file():
        raise SmokeError("le chemin modèle n'est pas un fichier")
    digest = hashlib.sha256()
    try:
        with resolved.open("rb") as stream:
            for block in iter(lambda: stream.read(1024 * 1024), b""):
                digest.update(block)
    except OSError as error:
        raise SmokeError("lecture du fichier modèle impossible") from error
    actual_hash = digest.hexdigest()
    if expected_hash is not None:
        normalized = expected_hash.lower()
        if not re.fullmatch(r"[0-9a-f]{64}", normalized):
            raise SmokeError("SHA-256 attendu invalide")
        if actual_hash != normalized:
            raise SmokeError("SHA-256 du modèle différent")
    return {"path_checked": True, "hash_checked": expected_hash is not None,
            "sha256": actual_hash}


def run_bridge(workspace, *arguments):
    result = subprocess.run(
        [str(BRIDGE), *arguments, "--workspace", str(workspace)],
        cwd=REPOSITORY, text=True, capture_output=True, timeout=30, check=False,
    )
    if result.returncode != 0:
        raise SmokeError(f"initialisation SPECIMEN refusée ({arguments[0]})")


class Client:
    def __init__(self, server):
        self.server = server
        self.cookie = None
        self.csrf = None

    def request(self, method, path, value=None, *, mutation=False):
        if mutation:
            # CONTRACT: la temporisation respecte la limite publique sans la
            # contourner ni modifier l'horloge ou l'état interne du serveur.
            time.sleep(0.04)
        connection = HTTPConnection("127.0.0.1", self.server.server_port, timeout=30)
        headers = {"Host": self.server.authority}
        if self.cookie is not None:
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
        except OSError as error:
            raise SmokeError("requête HTTP locale impossible") from error
        finally:
            connection.close()
        try:
            decoded = json.loads(data) if data else None
        except (UnicodeError, json.JSONDecodeError) as error:
            raise SmokeError(f"réponse JSON invalide pour {path}") from error
        return response.status, response_headers, decoded

    def expect(self, method, path, statuses, value=None, *, mutation=False):
        status, headers, body = self.request(
            method, path, value, mutation=mutation)
        if status not in statuses:
            error_value = body.get("error") if isinstance(body, dict) else None
            code = error_value.get("code") if isinstance(error_value, dict) else None
            message = error_value.get("message") if isinstance(error_value, dict) else None
            suffix = f", code={code}" if isinstance(code, str) else ""
            if isinstance(message, str) and message:
                suffix += f", message={message[:160]}"
            elif isinstance(error_value, str) and error_value:
                suffix += f", error={error_value[:160]}"
            raise SmokeError(f"HTTP {status} inattendu pour {path}{suffix}")
        return headers, body

    def open_session(self):
        status, headers, _body = self.request("GET", "/")
        if status != 303 or "Set-Cookie" not in headers:
            raise SmokeError("session automatique indisponible")
        self.cookie = headers["Set-Cookie"].split(";", 1)[0]
        _headers, session = self.expect("GET", "/api/v1/session", {200})
        self.csrf = session.get("csrf")
        if not isinstance(self.csrf, str) or not self.csrf:
            raise SmokeError("preuve CSRF absente")


def wait_for_turn(client, turn_id, wanted, timeout):
    deadline = time.monotonic() + timeout
    path = f"/api/v1/agent-runtime/turns/{turn_id}"
    while time.monotonic() < deadline:
        _headers, turn = client.expect("GET", path, {200})
        state = turn.get("state")
        if state in wanted:
            return turn
        if state in TERMINAL_STATES:
            raise SmokeError(f"turn terminé prématurément dans l'état {state}")
        time.sleep(0.1)
    raise SmokeError("délai du turn dépassé")


def prepared_plan(waiting):
    pending = waiting.get("pending_call")
    result = pending.get("result") if isinstance(pending, dict) else None
    output = result.get("output") if isinstance(result, dict) else None
    actions = output.get("actions") if isinstance(output, dict) else None
    if (not isinstance(output, dict) or not isinstance(actions, list) or
            not actions or not isinstance(output.get("plan_id"), str) or
            not isinstance(output.get("input_revision"), int)):
        raise SmokeError("plan de recherche absent de la pause")
    return output


def exercise(client, turn_timeout):
    # WHY: l'object_ref vient du résultat backend non fiable et le modèle doit
    # le retransmettre ; gateway, policy et grant revalident encore l'action.
    objective = (
        "Effectue exactement deux appels d'outils. 1: investigation.search avec "
        "query SPECIMEN. 2: research.prepare avec exactement le premier object_ref "
        "retourné, question Validation réseau SPECIMEN, exclusions vides. "
        "Ne réponds pas final avant le résultat de research.prepare."
    )
    _headers, started = client.expect(
        "POST", "/api/v1/agent-runtime/turns", {202},
        {"objective": objective, "idempotency_key": str(uuid.uuid4())}, mutation=True)
    turn_id = started.get("turn_id")
    if not isinstance(turn_id, str):
        raise SmokeError("identifiant de turn absent")
    waiting = wait_for_turn(client, turn_id, {"AUTHORIZATION_REQUIRED"}, turn_timeout)
    _headers, runtime_events = client.expect(
        "GET", "/api/v1/agent-runtime/events?cursor=0", {200})
    action_sequence = [
        event.get("payload", {}).get("tool_id")
        for event in runtime_events.get("events", [])
        if event.get("kind") == "agent.runtime.tool_requested"
    ]
    try:
        search_index = action_sequence.index("investigation.search")
        prepare_index = action_sequence.index("research.prepare")
    except ValueError as error:
        raise SmokeError("boucle search/prepare incomplète") from error
    if search_index >= prepare_index:
        raise SmokeError("research.prepare a précédé la recherche locale")
    plan = prepared_plan(waiting)
    action = plan["actions"][0]
    decisions = [{
        "action_id": item["action_id"],
        "decision": "AUTHORIZE" if item is action else "DEFER",
    } for item in plan["actions"]]
    _headers, grants = client.expect(
        "POST", "/api/v1/research/grants", {200}, {
            "plan_id": plan["plan_id"],
            "input_revision": plan["input_revision"],
            "selected_action_ids": [action["action_id"]],
            "decisions": decisions,
            "exclusions": [],
            "idempotency_key": str(uuid.uuid4()),
        }, mutation=True)
    grant_values = grants.get("grants") if isinstance(grants, dict) else None
    if not isinstance(grant_values, list) or not grant_values:
        raise SmokeError("grant persisté absent")
    grant_id = grant_values[0].get("grant_id")
    if not isinstance(grant_id, str):
        raise SmokeError("identifiant du grant persisté absent")
    grant_revision = grants.get("input_revision")
    if not isinstance(grant_revision, int):
        raise SmokeError("révision du grant persisté absente")
    _headers, persisted_research = client.expect("GET", "/api/v1/research", {200})
    persisted_grants = persisted_research.get("grants", [])
    if not any(item.get("grant_id") == grant_id for item in persisted_grants):
        raise SmokeError("décision humaine non relue depuis l'état de recherche")
    _headers, campaign = client.expect(
        "POST", "/api/v1/research/campaigns", {200}, {
            "grant_id": grant_id,
            "input_revision": grant_revision,
            "action_ids": [action["action_id"]],
            "idempotency_key": str(uuid.uuid4()),
        }, mutation=True)
    # CONTRACT: la reprise cible le turn initial ; aucune seconde consigne
    # modèle ne peut se substituer à la décision humaine persistée ci-dessus.
    _headers, resumed = client.expect(
        "POST", f"/api/v1/agent-runtime/turns/{turn_id}/resume", {202}, {},
        mutation=True)
    if resumed.get("turn_id") != turn_id:
        raise SmokeError("la reprise a changé de turn")
    completed = wait_for_turn(client, turn_id, {"COMPLETED"}, turn_timeout)
    with SpecimenProvider.contacts_lock:
        contacts = list(SpecimenProvider.contacts)
    if len(contacts) != 1:
        raise SmokeError("nombre de contacts fournisseur inattendu")
    if not isinstance(completed.get("final"), str) or not completed["final"].strip():
        raise SmokeError("bilan final absent")
    return {
        "turn_id": turn_id,
        "state": completed["state"],
        "same_turn_resumed": True,
        "tool_calls": completed.get("budgets", {}).get("tool_calls"),
        "model_calls": completed.get("budgets", {}).get("model_calls"),
        "final_present": True,
        "action_sequence": action_sequence,
        "grant_persisted": True,
        "research_state": campaign.get("state"),
        "provider_contacts": len(contacts),
    }


def main():
    args = parse_arguments()
    model_file = verify_model_file(args.model_path, args.model_sha256)
    if not 0.1 <= args.model_timeout <= 30.0:
        raise SmokeError("--model-timeout doit être compris entre 0.1 et 30")
    if args.turn_timeout <= 0:
        raise SmokeError("--turn-timeout doit être positif")
    provider = None
    provider_thread = None
    server = None
    server_thread = None
    with tempfile.TemporaryDirectory(prefix="labfy-real-qwen-SPECIMEN-") as directory:
        workspace = Path(directory) / "workspace"
        run_bridge(workspace, "init-j7-specimen")
        run_bridge(workspace, "export")
        SpecimenProvider.contacts = []
        provider = ThreadingHTTPServer(("127.0.0.1", 0), SpecimenProvider)
        provider_thread = threading.Thread(target=provider.serve_forever, daemon=True)
        provider_thread.start()
        try:
            server = WorkspaceServer(
                ("127.0.0.1", 0), QuietHandler, workspace=workspace, bridge=BRIDGE,
                research_fixture_authority=f"127.0.0.1:{provider.server_port}",
                agent_mode="local-model", agent_endpoint=args.endpoint,
                agent_model=args.model, agent_timeout=args.model_timeout)
            server_thread = threading.Thread(target=server.serve_forever, daemon=True)
            server_thread.start()
            client = Client(server)
            client.open_session()
            summary = exercise(client, args.turn_timeout)
        finally:
            # WHY: chaque ressource arrêtée a été créée par ce processus ; le
            # harnais n'utilise ni pkill ni port partagé pour découvrir autrui.
            if server is not None:
                server.stop_worker()
                if server.agent_runtime is not None:
                    server.agent_runtime.close()
                server.shutdown()
                server.server_close()
            if server_thread is not None:
                server_thread.join(timeout=5)
            if provider is not None:
                provider.shutdown()
                provider.server_close()
            if provider_thread is not None:
                provider_thread.join(timeout=5)
    print(json.dumps({
        "contract": "labfy.manual_real_local_model_smoke.v1",
        "ok": True,
        "model_configured": True,
        "model_file": model_file,
        **summary,
    }, ensure_ascii=False, sort_keys=True, separators=(",", ":")))


if __name__ == "__main__":
    try:
        main()
    except SmokeError as error:
        print(json.dumps({
            "contract": "labfy.manual_real_local_model_smoke.v1",
            "ok": False,
            "error_type": type(error).__name__,
            "error": str(error),
        }, ensure_ascii=False, sort_keys=True, separators=(",", ":")), file=sys.stderr)
        raise SystemExit(1)
    except (KeyError, IndexError, TypeError, ValueError):
        # CONTRACT: une donnée modèle/backend inattendue ne doit jamais être
        # recopiée dans le diagnostic machine du harnais.
        print(json.dumps({
            "contract": "labfy.manual_real_local_model_smoke.v1",
            "ok": False,
            "error_type": "unexpected_control_shape",
        }, sort_keys=True, separators=(",", ":")), file=sys.stderr)
        raise SystemExit(1)
