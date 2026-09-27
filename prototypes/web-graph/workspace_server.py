#!/usr/bin/env python3
"""Poste de travail J6 loopback, protégé, devant le service de commande C."""

from __future__ import annotations

import argparse
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
from http import HTTPStatus
from http.cookies import SimpleCookie
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlsplit
from report_bundle import publish as publish_report, verify as verify_report, ReportError

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
                 bootstrap: str, library=None, instance_id=None, config_id=None,
                 session_ttl_seconds=SESSION_TTL_SECONDS):
        if (not isinstance(session_ttl_seconds, int) or
                isinstance(session_ttl_seconds, bool) or session_ttl_seconds <= 0):
            raise ValueError("Durée de session invalide")
        super().__init__(address, handler)
        self.workspace = workspace.resolve()
        self.bridge = bridge.resolve()
        self.authority = f"127.0.0.1:{self.server_port}"
        self.origin = f"http://{self.authority}"
        self.bootstrap = bootstrap
        self.library = library
        self.active_workspace_id = None
        self.library_generation = (library.snapshot(None)["generation"]
                                   if library is not None else 0)
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
        self.last_command = 0.0
        self.report_tasks = {}
        self.report_lock = threading.Lock()
        self.report_threads = set()
        self.max_report_tasks = 2
        self.upload_lock = threading.Lock()
        self.active_uploads = 0
        self.reserved_upload_bytes = 0
        self.upload_mutexes = {}

    def library_snapshot(self):
        if self.library is None:
            raise ValueError("Mode bibliothèque inactif")
        value = self.library.snapshot(self.active_workspace_id)
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
        with self.lock:
            if (self.active_workspace_id is not None and
                    self.active_workspace_id != workspace_id):
                raise ValueError("Une autre enquête est déjà active")
            workspace, entry, generation = self.library.open(
                workspace_id, expected_generation)
            if self.active_workspace_id is None:
                previous = self.workspace
                self.workspace = workspace
                try:
                    # CONTRACT: l'ouverture valide aussi le pipeline C existant ;
                    # le serveur Web ne reconstruit aucune projection lui-même.
                    self.bridge_call(["export"])
                    self.context()
                except Exception:
                    self.workspace = previous
                    raise
                self.active_workspace_id = workspace_id
            self.library_generation = generation
        return {"contract": "labfy.web_library.open.v1",
                "workspace_id": workspace_id, "title": entry["title"],
                "state": "READY", "generation": generation}

    def upload_mutex(self, upload_id):
        """Return the process-local owner lock for one persisted upload."""
        with self.upload_lock:
            return self.upload_mutexes.setdefault(upload_id, threading.Lock())

    @property
    def context_path(self):
        generic = self.workspace / ".labfy" / "runtime" / "workspace.json"
        specimen = self.workspace / ".labfy" / "runtime" / "specimen.json"
        return generic if generic.is_file() else specimen

    def context(self):
        context_path = self.context_path
        database = self.workspace / "Enquete.sqlite"
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

    def bridge_call(self, arguments, timeout=8):
        command = [str(self.bridge), arguments[0], "--workspace",
                   str(self.workspace), *arguments[1:]]
        result = subprocess.run(command, cwd=self.bridge.parents[1], text=True,
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
        return self.headers.get("Host") == self.server.authority

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
                    self.headers.get("Origin") == self.server.origin and
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

    def _library_open_body(self):
        value = self._json_body()
        if (not isinstance(value, dict) or
                set(value) != {"workspace_id", "expected_generation"} or
                not isinstance(value["workspace_id"], str) or
                not isinstance(value["expected_generation"], int) or
                isinstance(value["expected_generation"], bool)):
            raise TypeError("Demande d'ouverture de bibliothèque invalide")
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
        if not self._authenticated():
            if path in {"/", "/login.js", "/styles.css"}:
                self._serve_static("/login.html" if path == "/" else path)
                return
            self._error(HTTPStatus.UNAUTHORIZED, "session_required",
                        "Session locale requise")
            return
        if path == "/api/v1/session":
            try: context = self.server.context()
            except (OSError,ValueError,json.JSONDecodeError) as error:
                self._error(HTTPStatus.CONFLICT,"workspace_incomplete",str(error));return
            self._json(HTTPStatus.OK, {"contract": "labfy.workspace.session.v1",
                "csrf": self.server.csrf, "origin": self.server.origin,
                "workspace_state": "READY" if context else "EMPTY",
                "investigation_id": context.get("investigation_id") if context else None,
                "title": context.get("title", "SPECIMEN") if context else None,
                "mode": context.get("mode", "specimen") if context else "local_experimental",
                "library_mode": self.server.library is not None,
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
            self._open_session()
            return
        if not self._mutation_allowed():
            self._error(HTTPStatus.FORBIDDEN, "mutation_rejected",
                        "Session, Origin ou preuve CSRF invalide")
            return
        now = time.monotonic()
        if (not path.startswith("/api/v1/uploads") and
                now - self.server.last_command < 0.03):
            self._error(HTTPStatus.TOO_MANY_REQUESTS, "rate_limited",
                        "Commandes trop rapprochées")
            return
        self.server.last_command = now
        try:
            if path == "/api/v1/library/workspaces":
                value = self._body({"title", "idempotency_key"})
                response = self.server.create_library_workspace(
                    value["title"], value["idempotency_key"])
                self._json(HTTPStatus.OK if response["replayed"] else HTTPStatus.CREATED,
                           response)
                return
            if path == "/api/v1/library/open":
                value = self._library_open_body()
                self._json(HTTPStatus.OK, self.server.open_library_workspace(
                    value["workspace_id"], value["expected_generation"]))
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
        except OverflowError as error:
            self._error(HTTPStatus.REQUEST_ENTITY_TOO_LARGE, "body_too_large", str(error))
        except (TypeError, json.JSONDecodeError) as error:
            self._error(HTTPStatus.BAD_REQUEST, "invalid_request", str(error))
        except (ValueError, subprocess.TimeoutExpired) as error:
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

    def _open_session(self):
        if self.headers.get("Origin") != self.server.origin:
            self._error(HTTPStatus.FORBIDDEN, "origin_rejected", "Origin exact obligatoire")
            return
        try:
            value = self._body({"bootstrap_code"})
        except (TypeError, OverflowError) as error:
            self._error(HTTPStatus.BAD_REQUEST, "invalid_request", str(error))
            return
        if not hmac.compare_digest(value["bootstrap_code"], self.server.bootstrap):
            self._error(HTTPStatus.FORBIDDEN, "bootstrap_rejected", "Code éphémère invalide")
            return
        with self.server.session_lock:
            # Une reconnexion renouvelle les secrets ; les anciens cookies
            # expirés ne doivent pas redevenir valables.
            self.server.session = secrets.token_urlsafe(32)
            self.server.csrf = secrets.token_urlsafe(32)
            self.server.session_deadline = (time.monotonic() +
                                            self.server.session_ttl_seconds)
            cookie = (f"{self.server.cookie_name}={self.server.session}; "
                      "HttpOnly; SameSite=Strict; "
                      f"Path=/; Max-Age={self.server.session_ttl_seconds}")
        self._json(HTTPStatus.OK, {"contract": "labfy.workspace.session.v1",
                                   "authenticated": True}, cookie=cookie)

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
    args = parser.parse_args()
    bootstrap = secrets.token_urlsafe(12)
    server = WorkspaceServer(("127.0.0.1", args.port), Handler,
                             workspace=args.workspace, bridge=args.bridge,
                             bootstrap=bootstrap)
    print(f"Labfy J6 : {server.origin}/", flush=True)
    print(f"Code de session éphémère : {bootstrap}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.stop_worker()
        server.server_close()


if __name__ == "__main__":
    main()
