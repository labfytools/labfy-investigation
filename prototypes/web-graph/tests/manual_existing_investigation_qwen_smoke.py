#!/usr/bin/env python3
"""Smoke manuel Qwen A/B sur deux enquêtes existantes SPECIMEN temporaires."""

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
BRIDGE = REPOSITORY / "tools/local-jobs"
sys.path.insert(0, str(PROTOTYPE))

from web_library import WebLibrary  # noqa: E402
from workspace_server import Handler, WorkspaceServer  # noqa: E402


TERMINAL = {"COMPLETED", "FAILED", "CANCELLED", "MODEL_UNAVAILABLE",
            "MODEL_PROTOCOL_ERROR", "BUDGET_EXHAUSTED"}


class SmokeError(RuntimeError):
    """Erreur bornée du smoke SPECIMEN."""


class QuietHandler(Handler):
    def log_message(self, _format, *_args):
        pass


def arguments():
    parser = argparse.ArgumentParser()
    parser.add_argument("--model-path", type=Path, required=True)
    parser.add_argument("--model-sha256", required=True)
    parser.add_argument("--model-id", default="Huihui-Qwen3.5-9B-Q4_K_M.gguf")
    parser.add_argument("--model-timeout", type=float, default=90.0)
    parser.add_argument("--turn-timeout", type=float, default=600.0)
    return parser.parse_args()


def verify_model(path, expected):
    forbidden = (Path.home() / "Documents" / "Investigations").absolute()
    candidate = path.expanduser().absolute()
    if candidate.is_relative_to(forbidden) or candidate.is_symlink():
        raise SmokeError("Chemin modèle interdit")
    resolved = candidate.resolve(strict=True)
    if resolved.is_relative_to(forbidden) or not resolved.is_file():
        raise SmokeError("Fichier modèle invalide")
    digest = hashlib.sha256()
    with resolved.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    if digest.hexdigest() != expected.lower():
        raise SmokeError("SHA-256 modèle inattendu")
    return resolved


def bridge_create(workspace, title):
    result = subprocess.run(
        [str(BRIDGE), "create-workspace", "--workspace", str(workspace),
         "--title", title], cwd=REPOSITORY, capture_output=True, text=True,
        timeout=30, check=False,
    )
    if result.returncode:
        raise SmokeError("Création du workspace SPECIMEN refusée")


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


class Client:
    def __init__(self, server):
        self.server = server
        self.cookie = None
        self.csrf = None

    def request(self, method, path, value=None, *, mutation=False):
        if mutation:
            time.sleep(0.04)
        connection = HTTPConnection("127.0.0.1", self.server.server_port, timeout=95)
        headers = {"Host": self.server.authority}
        if self.cookie:
            headers["Cookie"] = self.cookie
        if mutation:
            headers.update(Origin=self.server.origin, **{"X-Labfy-CSRF": self.csrf})
        body = None
        if value is not None:
            headers["Content-Type"] = "application/json"
            body = json.dumps(value, ensure_ascii=False,
                              separators=(",", ":")).encode("utf-8")
        connection.request(method, path, body=body, headers=headers)
        response = connection.getresponse()
        data = response.read()
        result = response.status, dict(response.getheaders()), (
            json.loads(data) if data else None)
        connection.close()
        return result

    def expect(self, method, path, statuses, value=None, *, mutation=False):
        status, headers, body = self.request(method, path, value, mutation=mutation)
        if status not in statuses:
            message = body.get("message") if isinstance(body, dict) else None
            raise SmokeError(f"HTTP {status} inattendu pour {path}: {message}")
        return headers, body

    def open_session(self):
        status, headers, _body = self.request("GET", "/")
        if status != 303 or "Set-Cookie" not in headers:
            raise SmokeError("Session locale indisponible")
        self.cookie = headers["Set-Cookie"].split(";", 1)[0]
        _headers, session = self.expect("GET", "/api/v1/session", {200})
        self.csrf = session["csrf"]


def wait_turn(client, turn_id, timeout):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        _headers, turn = client.expect(
            "GET", f"/api/v1/agent-runtime/turns/{turn_id}", {200})
        if turn.get("state") in TERMINAL:
            return turn
        time.sleep(0.2)
    raise SmokeError("Timeout global du turn Qwen")


def gateway_search_output(server, turn_id):
    scope = server.agent_scope()
    events = server.agent_gateway.events(scope, 0).get("events", [])
    event = next((item for item in reversed(events)
                  if item.get("turn_id") == turn_id and
                  item.get("kind") == "agent.tool.completed" and
                  item.get("payload", {}).get("tool_id") == "investigation.search"), None)
    if event is None:
        raise SmokeError("Résultat gateway search absent")
    result = server.agent_gateway.result(scope, event["payload"]["result_id"])
    if result.get("state") != "COMPLETED" or not isinstance(result.get("output"), dict):
        raise SmokeError("Résultat gateway search non COMPLETED")
    return result["output"]


def run_turn(server, client, marker, timeout):
    objective = (f"Analyse l’enquête {marker}. Appelle investigation.get_overview, "
                 f"puis investigation.search avec query {marker}, puis donne un "
                 "bilan factuel court. Attends chaque résultat avant de continuer.")
    _headers, started = client.expect(
        "POST", "/api/v1/agent-runtime/turns", {202},
        {"objective": objective, "idempotency_key": str(uuid.uuid4())}, mutation=True)
    turn_id = started.get("turn_id")
    if not isinstance(turn_id, str):
        raise SmokeError("Identifiant de turn absent")
    final = wait_turn(client, turn_id, timeout)
    _headers, events = client.expect(
        "GET", "/api/v1/agent-runtime/events?cursor=0", {200})
    tools = [event.get("payload", {}).get("tool_id")
             for event in events.get("events", [])
             if event.get("turn_id") == turn_id and
             event.get("kind") == "agent.runtime.tool_requested"]
    if final.get("state") != "COMPLETED" or not str(final.get("final", "")).strip():
        raise SmokeError(f"Turn Qwen non terminé: {final.get('state')}")
    if "investigation.search" not in tools:
        raise SmokeError("investigation.search absent du turn réel")
    search = gateway_search_output(server, turn_id)
    if not any(marker in json.dumps(item, ensure_ascii=False)
               for item in search.get("matches", [])):
        raise SmokeError("Marqueur actif absent du résultat search réel")
    if marker not in final["final"]:
        raise SmokeError("Marqueur du contexte actif absent du bilan Qwen")
    return turn_id, final, tools, search


def candidate(discovery, name):
    value = next((item for item in discovery["candidates"]
                  if item["display_name"] == name), None)
    if value is None or value["state"] != "READY_TO_REGISTER":
        raise SmokeError(f"Candidat {name} non prêt")
    return value


def main():
    args = arguments()
    results = {
        "lazy_start_no_scan": False, "discovery": False, "register_a": False,
        "no_copy_a": False, "open_a": False, "qwen_a_context": False,
        "qwen_a_search": False, "b_hidden_from_a": False, "close_a": False,
        "qwen_disabled_closed": False, "old_a_resume_rejected": False,
        "register_b": False, "open_b": False, "qwen_b_context": False,
        "qwen_b_search": False, "a_hidden_from_b": False, "cleanup": False,
    }
    security = {"absolute_path_exposed": False, "model_filesystem_access": False,
                "model_sqlite_access": False, "symlink_followed": False,
                "cross_workspace_leak": False,
                "automatic_real_library_scan": False,
                "real_investigation_access": False}
    server = None
    thread = None
    turn_a = None
    turn_b = None
    tools_a = []
    tools_b = []
    gateway_marker_a = False
    gateway_marker_b = False
    registered_workspace_a = None
    registered_workspace_b = None
    context_workspace_a = None
    context_workspace_b = None
    try:
        if not 0.1 <= args.model_timeout <= 90 or not 30 <= args.turn_timeout <= 600:
            raise SmokeError("Timeouts hors contrat")
        model = verify_model(args.model_path, args.model_sha256)
        with tempfile.TemporaryDirectory(
                prefix="labfy-existing-Qwen-SPECIMEN-") as directory:
            root = Path(directory).resolve()
            library_root = root / "SPECIMEN_LIBRARY"
            library_root.mkdir(mode=0o700)
            workspace_a = library_root / "Existing-A"
            workspace_b = library_root / "Existing-B"
            bridge_create(workspace_a, "ALPHA-SPECIMEN-ONLY")
            bridge_create(workspace_b, "BRAVO-SPECIMEN-ONLY")
            hash_a = sha256(workspace_a / "Enquete.sqlite")
            config = root / "agent.json"
            config.write_text(json.dumps({"model": {"model_path": str(model),
                "model_id": args.model_id, "context": 4096, "parallel": 1,
                "reasoning": "off"}}, separators=(",", ":")), encoding="utf-8")
            config.chmod(0o600)
            inactive = root / "inactive"
            inactive.mkdir(mode=0o700)
            library = WebLibrary(library_root, BRIDGE, lazy=True)
            results["lazy_start_no_scan"] = not (library_root / "registry.json").exists()
            server = WorkspaceServer(
                ("127.0.0.1", 0), QuietHandler, workspace=inactive, bridge=BRIDGE,
                library=library, agent_autostart=True, agent_config_path=config,
                agent_timeout=args.model_timeout,
            )
            thread = threading.Thread(target=server.serve_forever, daemon=True)
            thread.start()
            client = Client(server)
            client.open_session()
            model_status = server.agent_runtime_status()
            if model_status.get("available") is not True:
                raise SmokeError(
                    "Qwen temporaire non READY: " + str(model_status.get("reason"))[:180])

            _headers, discovery = client.expect(
                "POST", "/api/v1/library/discover-existing", {200}, {}, mutation=True)
            encoded = json.dumps(discovery, ensure_ascii=False)
            security["absolute_path_exposed"] = str(root) in encoded
            a = candidate(discovery, "Existing-A")
            b = candidate(discovery, "Existing-B")
            results["discovery"] = not (library_root / "registry.json").exists()
            _headers, registered_a = client.expect(
                "POST", "/api/v1/library/register-existing", {201}, {
                    "candidate_id": a["candidate_id"],
                    "expected_generation": discovery["generation"],
                    "idempotency_key": str(uuid.uuid4()), "human_confirmed": True,
                }, mutation=True)
            results["register_a"] = registered_a["state"] == "READY"
            registered_workspace_a = registered_a["workspace_id"]
            results["no_copy_a"] = (sha256(workspace_a / "Enquete.sqlite") == hash_a and
                not (library_root / "workspaces" / registered_a["workspace_id"]).exists())
            _headers, opened_a = client.expect(
                "POST", "/api/v1/library/open", {200}, {
                    "workspace_id": registered_a["workspace_id"],
                    "expected_generation": registered_a["generation"],
                }, mutation=True)
            results["open_a"] = server.active_workspace_id == registered_a["workspace_id"]
            context_a = server._agent_investigation_context(server.agent_scope())
            context_workspace_a = context_a["workspace_id"]
            results["qwen_a_context"] = (context_a["workspace_id"] == registered_a["workspace_id"]
                                         and context_a["title"] == "ALPHA-SPECIMEN-ONLY")
            turn_a, final_a, tools_a, search_a = run_turn(
                server, client, "ALPHA-SPECIMEN-ONLY", args.turn_timeout)
            results["qwen_a_search"] = ("investigation.search" in tools_a and
                any("ALPHA-SPECIMEN-ONLY" in json.dumps(item, ensure_ascii=False)
                    for item in search_a["matches"]))
            gateway_marker_a = results["qwen_a_search"]
            hidden_b = server._agent_search({"query": "BRAVO-SPECIMEN-ONLY"},
                                            "smoke-a-hidden")
            results["b_hidden_from_a"] = (not hidden_b["matches"] and
                "BRAVO-SPECIMEN-ONLY" not in final_a["final"])

            _headers, closed = client.expect(
                "POST", "/api/v1/library/close", {200}, {
                    "workspace_id": registered_a["workspace_id"],
                    "expected_generation": opened_a["generation"],
                    "idempotency_key": str(uuid.uuid4()), "human_confirmed": True,
                }, mutation=True)
            results["close_a"] = closed["active_workspace_id"] is None
            status, _headers, _body = client.request(
                "POST", "/api/v1/agent-runtime/turns", {
                    "objective": "SPECIMEN fermé", "idempotency_key": str(uuid.uuid4())},
                mutation=True)
            results["qwen_disabled_closed"] = status == 409
            status, _headers, _body = client.request(
                "POST", f"/api/v1/agent-runtime/turns/{turn_a}/resume", {}, mutation=True)
            results["old_a_resume_rejected"] = status in {403, 404, 409}

            _headers, discovery_b = client.expect(
                "POST", "/api/v1/library/discover-existing", {200}, {}, mutation=True)
            b = candidate(discovery_b, "Existing-B")
            _headers, registered_b = client.expect(
                "POST", "/api/v1/library/register-existing", {201}, {
                    "candidate_id": b["candidate_id"],
                    "expected_generation": discovery_b["generation"],
                    "idempotency_key": str(uuid.uuid4()), "human_confirmed": True,
                }, mutation=True)
            results["register_b"] = registered_b["state"] == "READY"
            registered_workspace_b = registered_b["workspace_id"]
            _headers, _opened_b = client.expect(
                "POST", "/api/v1/library/open", {200}, {
                    "workspace_id": registered_b["workspace_id"],
                    "expected_generation": registered_b["generation"],
                }, mutation=True)
            results["open_b"] = server.active_workspace_id == registered_b["workspace_id"]
            context_b = server._agent_investigation_context(server.agent_scope())
            context_workspace_b = context_b["workspace_id"]
            results["qwen_b_context"] = (context_b["workspace_id"] == registered_b["workspace_id"]
                                         and context_b["title"] == "BRAVO-SPECIMEN-ONLY")
            turn_b, final_b, tools_b, search_b = run_turn(
                server, client, "BRAVO-SPECIMEN-ONLY", args.turn_timeout)
            results["qwen_b_search"] = ("investigation.search" in tools_b and
                any("BRAVO-SPECIMEN-ONLY" in json.dumps(item, ensure_ascii=False)
                    for item in search_b["matches"]))
            gateway_marker_b = results["qwen_b_search"]
            hidden_a = server._agent_search({"query": "ALPHA-SPECIMEN-ONLY"},
                                            "smoke-b-hidden")
            results["a_hidden_from_b"] = (not hidden_a["matches"] and
                "ALPHA-SPECIMEN-ONLY" not in final_b["final"])
            security["cross_workspace_leak"] = not (
                results["b_hidden_from_a"] and results["a_hidden_from_b"])
            terminal_a, terminal_b = final_a["state"], final_b["state"]
    except (SmokeError, OSError, ValueError, subprocess.TimeoutExpired) as error:
        print(f"ERROR={type(error).__name__}:{str(error)[:300]}")
        terminal_a = terminal_b = "UNKNOWN"
    finally:
        if server is not None:
            try:
                server.shutdown()
                server.server_close()
            except (OSError, RuntimeError):
                pass
        if thread is not None:
            thread.join(timeout=5)
        results["cleanup"] = bool(
            server is None or server.local_model_supervisor is None or
            server.local_model_supervisor.process is None)

    print("REAL_EXISTING_INVESTIGATION_QWEN_SMOKE")
    print(f"model_sha256={args.model_sha256.lower()}")
    print(f"turn_a_id={turn_a}")
    print(f"turn_a_tools={','.join(str(item) for item in tools_a)}")
    print(f"turn_a_gateway_search_marker={'true' if gateway_marker_a else 'false'}")
    print(f"registered_workspace_a={registered_workspace_a}")
    print(f"context_workspace_a={context_workspace_a}")
    print(f"turn_b_id={turn_b}")
    print(f"turn_b_tools={','.join(str(item) for item in tools_b)}")
    print(f"turn_b_gateway_search_marker={'true' if gateway_marker_b else 'false'}")
    print(f"registered_workspace_b={registered_workspace_b}")
    print(f"context_workspace_b={context_workspace_b}")
    for name, passed in results.items():
        print(f"{name}={'PASS' if passed else 'FAIL'}")
    print(f"terminal_a={terminal_a}")
    print(f"terminal_b={terminal_b}")
    print("SECURITY")
    for name, exposed in security.items():
        print(f"{name}={'true' if exposed else 'false'}")
    return 0 if all(results.values()) and terminal_a == terminal_b == "COMPLETED" and not any(
        security.values()) else 1


if __name__ == "__main__":
    raise SystemExit(main())
