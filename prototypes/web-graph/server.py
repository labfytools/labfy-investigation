#!/usr/bin/env python3
"""Serveur loopback expérimental pour les seules fixtures Web Graph."""

from __future__ import annotations

import argparse
import json
import mimetypes
import time
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlsplit

from fixtures import generated_snapshot, scenario_events

ROOT = Path(__file__).resolve().parent
PUBLIC = ROOT / "public"
ALLOWED_HOSTS = {"127.0.0.1", "localhost", "[::1]"}
EVENTS_BY_CURSOR = {str(index): event for index, event in enumerate(scenario_events())}
FINAL_CURSOR = scenario_events()[-1]["id"]
MAX_CORE_SNAPSHOT_BYTES = 1024 * 1024
MAX_JOBS_SNAPSHOT_BYTES = 256 * 1024


def load_jobs_snapshot(path_value):
    """Valide l'export C borné sans jamais ouvrir SQLite depuis Python."""
    path = Path(path_value).resolve()
    if path.name != "jobs-snapshot.json" or path.stat().st_size > MAX_JOBS_SNAPSHOT_BYTES:
        raise ValueError("snapshot jobs absent ou trop volumineux")
    value = json.loads(path.read_text(encoding="utf-8"))
    if value.get("contract") != "labfy.local_jobs.snapshot.v1" or not isinstance(value.get("jobs"), list):
        raise ValueError("contrat du snapshot jobs invalide")
    return value


def load_core_snapshot(path_value):
    """Charge uniquement l'artefact voisin du manifeste produit par le binaire C."""
    path = Path(path_value).resolve()
    manifest_path = path.parent / "generation-manifest.json"
    jobs_manifest = path.parent / ".labfy" / "runtime" / "specimen.json"
    if not manifest_path.is_file() and jobs_manifest.is_file():
        manifest_path = jobs_manifest
    if path.name != "core-snapshot.json" or not manifest_path.is_file():
        raise ValueError("snapshot cœur hors du répertoire de génération attendu")
    if path.stat().st_size > MAX_CORE_SNAPSHOT_BYTES:
        raise ValueError("snapshot cœur trop volumineux")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    snapshot = json.loads(path.read_text(encoding="utf-8"))
    accepted_contracts = {
        "labfy.core_graph.specimen_manifest.v1": "labfy.web_graph.snapshot.v2",
        "labfy.eml_graph.specimen_manifest.v1": "labfy.web_graph.snapshot.v3",
        "labfy.local_toolkit.specimen_manifest.v1": "labfy.web_graph.snapshot.v3",
        "labfy.local_jobs.specimen.v1": "labfy.web_graph.snapshot.v3",
    }
    expected_snapshot_contract = accepted_contracts.get(manifest.get("contract"))
    if (
        expected_snapshot_contract is None
        or manifest.get("synthetic") is not True
        or manifest.get("snapshot_file") != path.name
        or manifest.get("snapshot_contract", expected_snapshot_contract)
        != expected_snapshot_contract
        or snapshot.get("contract") != expected_snapshot_contract
        or snapshot.get("origin") != "core"
        or snapshot.get("investigation", {}).get("synthetic") is not True
    ):
        raise ValueError("contrat du snapshot cœur ou du manifeste invalide")
    return snapshot


class PrototypeServer(ThreadingHTTPServer):
    """Les threads de clients SSE ne doivent pas bloquer l'arrêt du prototype."""

    daemon_threads = True
    block_on_close = False

    def __init__(self, address, handler, *, core_snapshot=None, core_error=None, jobs_path=None):
        super().__init__(address, handler)
        self.core_snapshot = core_snapshot
        self.core_error = core_error
        self.jobs_path = jobs_path


class Handler(BaseHTTPRequestHandler):
    server_version = "LabfyPrototype/0.2"

    def _security_headers(self):
        self.send_header(
            "Content-Security-Policy",
            "default-src 'self'; script-src 'self'; style-src 'self'; "
            "img-src 'self' data:; connect-src 'self'; object-src 'none'; "
            "base-uri 'none'; frame-ancestors 'none'",
        )
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Referrer-Policy", "no-referrer")
        self.send_header("Cache-Control", "no-store")

    def _origin_allowed(self):
        host = self.headers.get("Host", "")
        name = host.rsplit(":", 1)[0] if not host.startswith("[") else host.split("]", 1)[0] + "]"
        if name not in ALLOWED_HOSTS:
            return False
        origin = self.headers.get("Origin")
        return origin is None or origin in {
            f"http://{host}",
            f"http://localhost:{self.server.server_port}",
            f"http://127.0.0.1:{self.server.server_port}",
        }

    def _error(self, status, code, message):
        body = json.dumps(
            {"contract": "labfy.web_graph.error.v1", "error": code, "message": message},
            ensure_ascii=False,
        ).encode()
        self.send_response(status)
        self._security_headers()
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(body)

    def do_GET(self):
        if not self._origin_allowed():
            self._error(HTTPStatus.FORBIDDEN, "request_rejected", "Host ou Origin non autorisé")
            return
        parsed = urlsplit(self.path)
        if parsed.path == "/api/v1/snapshot":
            self._serve_snapshot(parsed.query)
            return
        if parsed.path == "/fixture.js":
            self._serve_initial_fixture()
            return
        if parsed.path == "/api/v1/events":
            self._serve_event(parsed.query)
            return
        if parsed.path == "/api/v1/jobs":
            if self.server.jobs_path is None:
                self._error(HTTPStatus.NOT_FOUND, "jobs_unavailable", "Aucun export jobs configuré")
                return
            try:
                self._json(load_jobs_snapshot(self.server.jobs_path))
            except (OSError, ValueError, json.JSONDecodeError) as error:
                self._error(HTTPStatus.SERVICE_UNAVAILABLE, "jobs_export_invalid", str(error))
            return
        self._serve_static(parsed.path)

    def do_HEAD(self):
        self.do_GET()

    def do_POST(self):
        self._error(HTTPStatus.METHOD_NOT_ALLOWED, "read_only", "Prototype en lecture seule")

    do_PUT = do_POST
    do_PATCH = do_POST
    do_DELETE = do_POST

    def _serve_snapshot(self, query):
        if self.server.core_error is not None:
            self._error(HTTPStatus.SERVICE_UNAVAILABLE, "core_export_failed", self.server.core_error)
            return
        if self.server.core_snapshot is not None:
            if query:
                self._error(HTTPStatus.BAD_REQUEST, "core_snapshot_static", "Le snapshot cœur est statique")
                return
            self._json(self.server.core_snapshot)
            return
        parameters = parse_qs(query)
        size = parameters.get("size", ["demo"])[0]
        revision = parameters.get("revision", ["latest"])[0]
        if revision not in {"initial", "latest"}:
            self._error(HTTPStatus.BAD_REQUEST, "invalid_revision", "Révision autorisée : initial ou latest")
            return
        try:
            snapshot = generated_snapshot(size, latest=revision == "latest")
        except (ValueError, OverflowError):
            self._error(HTTPStatus.BAD_REQUEST, "invalid_size", "Taille autorisée : demo, 100, 1000 ou 5000")
            return
        self._json(snapshot)

    def _serve_initial_fixture(self):
        if self.server.core_error is not None:
            value = {
                "contract": "labfy.web_graph.error.v1",
                "error": "core_export_failed",
                "message": self.server.core_error,
            }
        elif self.server.core_snapshot is not None:
            value = self.server.core_snapshot
        else:
            value = generated_snapshot("demo")
        body = (
            "export default "
            + json.dumps(value, ensure_ascii=False, separators=(",", ":"))
            + ";\n"
        ).encode()
        self._bytes(HTTPStatus.OK, "text/javascript; charset=utf-8", body)

    def _serve_event(self, query):
        if self.server.core_snapshot is not None or self.server.core_error is not None:
            self.send_response(HTTPStatus.NO_CONTENT)
            self._security_headers()
            self.end_headers()
            return
        parameters = parse_qs(query)
        cursor = self.headers.get("Last-Event-ID") or parameters.get("after", ["0"])[0]
        if not cursor.isdigit():
            self._error(HTTPStatus.BAD_REQUEST, "invalid_event_cursor", "Curseur d'événement invalide")
            return
        if cursor == FINAL_CURSOR:
            # CONTRACT: 204 demande à EventSource de ne plus reconnecter après
            # la fin normale, même si le client n'a pas fermé assez vite.
            self.send_response(HTTPStatus.NO_CONTENT)
            self._security_headers()
            self.end_headers()
            return
        event = EVENTS_BY_CURSOR.get(cursor)
        if event is None:
            event = {
                "contract": "labfy.web_graph.event.v1",
                "id": FINAL_CURSOR,
                "base_revision": 3,
                "revision": 3,
                "kind": "resync_required",
                "node_id": None,
                "changes": {},
                "message": "Curseur inconnu : snapshot de rattrapage requis",
            }
        if parameters.get("pace") == ["demo"]:
            # WHY: ce délai borné laisse le temps de manipuler la fixture avant
            # la mise à jour, afin de démontrer la conservation du contexte.
            time.sleep(1.2 if cursor == "0" else 0.25)
        body = (
            "retry: 150\n"
            f"id: {event['id']}\n"
            "event: graph-update\n"
            f"data: {json.dumps(event, ensure_ascii=False, separators=(',', ':'))}\n\n"
        ).encode()
        self._bytes(HTTPStatus.OK, "text/event-stream; charset=utf-8", body)

    def _json(self, value):
        body = json.dumps(value, ensure_ascii=False, separators=(",", ":")).encode()
        self._bytes(HTTPStatus.OK, "application/json; charset=utf-8", body)

    def _bytes(self, status, content_type, body):
        self.send_response(status)
        self._security_headers()
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        if self.command != "HEAD":
            try:
                self.wfile.write(body)
            except (BrokenPipeError, ConnectionResetError):
                # Un onglet fermé ou EventSource annulé ne doit pas polluer le
                # journal ni empêcher l'arrêt des autres clients.
                self.close_connection = True

    def _serve_static(self, request_path):
        relative = "index.html" if request_path == "/" else request_path.lstrip("/")
        candidate = (PUBLIC / relative).resolve()
        if PUBLIC not in candidate.parents and candidate != PUBLIC:
            self._error(HTTPStatus.NOT_FOUND, "not_found", "Ressource absente")
            return
        if not candidate.is_file():
            self._error(HTTPStatus.NOT_FOUND, "not_found", "Ressource absente")
            return
        self._bytes(
            HTTPStatus.OK,
            mimetypes.guess_type(candidate.name)[0] or "application/octet-stream",
            candidate.read_bytes(),
        )

    def log_message(self, fmt, *args):
        print(f"prototype: {self.address_string()} {fmt % args}")


def serve(port, *, core_snapshot=None, core_error=None, jobs_path=None):
    server = PrototypeServer(
        ("127.0.0.1", port),
        Handler,
        core_snapshot=core_snapshot,
        core_error=core_error,
        jobs_path=jobs_path,
    )
    mode = "snapshot cœur C" if core_snapshot is not None else "fixture J2"
    if core_error is not None:
        mode = "erreur d'export cœur"
    print(f"Prototype Labfy ({mode}) : http://127.0.0.1:{server.server_port}/", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.shutdown()
        server.server_close()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--core-snapshot", type=Path)
    parser.add_argument("--core-error")
    parser.add_argument("--jobs-snapshot", type=Path)
    args = parser.parse_args()
    if args.core_snapshot is not None and args.core_error is not None:
        parser.error("--core-snapshot et --core-error sont exclusifs")
    snapshot = load_core_snapshot(args.core_snapshot) if args.core_snapshot else None
    if args.jobs_snapshot is not None:
        load_jobs_snapshot(args.jobs_snapshot)
    serve(args.port, core_snapshot=snapshot, core_error=args.core_error,
          jobs_path=args.jobs_snapshot)


if __name__ == "__main__":
    main()
