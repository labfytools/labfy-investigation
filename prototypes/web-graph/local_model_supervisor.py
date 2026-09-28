"""Cycle de vie borné d'un llama-server local possédé par Labfy.

Ce module ne recherche jamais un processus existant : il ne peut arrêter que
le PID qu'il a créé. Ainsi, fermer le poste Web ne peut pas affecter un modèle
lancé par une autre application.
"""

from __future__ import annotations

import json
import os
import signal
import socket
import subprocess
import time
from dataclasses import dataclass
from pathlib import Path
from urllib.request import Request, urlopen


class LocalModelSupervisorError(RuntimeError):
    """Erreur stable d'admission ou de démarrage du modèle local."""


@dataclass(frozen=True)
class LocalModelConfig:
    """Configuration explicite, jamais un chemin personnel versionné."""

    model_path: Path
    model_id: str
    context: int = 4096
    parallel: int = 1
    reasoning: str = "off"

    @classmethod
    def from_mapping(cls, value):
        if not isinstance(value, dict):
            raise LocalModelSupervisorError("Configuration modèle invalide")
        model = value.get("model")
        if not isinstance(model, dict):
            raise LocalModelSupervisorError("Section modèle absente")
        path = model.get("model_path")
        model_id = model.get("model_id")
        context = model.get("context", 4096)
        parallel = model.get("parallel", 1)
        reasoning = model.get("reasoning", "off")
        if (not isinstance(path, str) or not path or "\x00" in path or
                not isinstance(model_id, str) or not model_id.strip() or
                not isinstance(context, int) or isinstance(context, bool) or
                not 512 <= context <= 8192 or not isinstance(parallel, int) or
                isinstance(parallel, bool) or not 1 <= parallel <= 2 or
                reasoning != "off"):
            raise LocalModelSupervisorError("Valeurs modèle hors contrat")
        resolved = Path(path).expanduser().resolve()
        if not resolved.is_file() or resolved.is_symlink():
            raise LocalModelSupervisorError("Fichier modèle indisponible")
        return cls(resolved, model_id.strip(), context, parallel, reasoning)


def load_xdg_config(path=None):
    """Charge une configuration utilisateur opt-in sans la créer ni l'écraser."""
    if path is None:
        root = Path(os.environ.get("XDG_CONFIG_HOME", Path.home() / ".config"))
        path = root / "labfy-investigation" / "agent.json"
    candidate = Path(path)
    if not candidate.is_file() or candidate.is_symlink():
        raise LocalModelSupervisorError("Configuration modèle locale absente")
    if candidate.stat().st_size > 16 * 1024:
        raise LocalModelSupervisorError("Configuration modèle trop volumineuse")
    try:
        value = json.loads(candidate.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise LocalModelSupervisorError("Configuration modèle illisible") from error
    return LocalModelConfig.from_mapping(value)


class LocalModelSupervisor:
    """Démarre un seul serveur loopback et publie READY après /health=200."""

    START_TIMEOUT_SECONDS = 90.0
    STOP_TIMEOUT_SECONDS = 8.0

    def __init__(self, config, *, executable="/usr/bin/llama-server"):
        if not isinstance(config, LocalModelConfig):
            raise LocalModelSupervisorError("Configuration superviseur invalide")
        executable_path = Path(executable)
        if not executable_path.is_file() or not os.access(executable_path, os.X_OK):
            raise LocalModelSupervisorError("llama-server indisponible")
        self.config = config
        self.executable = str(executable_path)
        self.process = None
        self.started_at = None
        self.endpoint = None
        self.reason = None

    @staticmethod
    def _reserve_loopback_port():
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
            listener.bind(("127.0.0.1", 0))
            return listener.getsockname()[1]

    def _command(self, port):
        if not isinstance(port, int) or isinstance(port, bool) or not 1 <= port <= 65535:
            raise LocalModelSupervisorError("Port modèle invalide")
        return [
            self.executable,
            "--model", str(self.config.model_path),
            "--host", "127.0.0.1",
            "--port", str(port),
            "--ctx-size", str(self.config.context),
            "--parallel", str(self.config.parallel),
            "--reasoning", self.config.reasoning,
            "--no-warmup",
        ]

    def start(self):
        if self.process is not None and self.process.poll() is None:
            return self.status()
        port = self._reserve_loopback_port()
        self.endpoint = f"http://127.0.0.1:{port}"
        argv = self._command(port)
        # INVARIANT: argv est fixe et toutes les valeurs viennent d'une config
        # validée. Ni modèle ni UI ne peut injecter un fragment de shell.
        self.process = subprocess.Popen(
            argv, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL, start_new_session=True,
            env={"PATH": "/usr/bin:/bin", "LANG": "C.UTF-8", "TZ": "UTC"},
        )
        self.started_at = time.time()
        deadline = time.monotonic() + self.START_TIMEOUT_SECONDS
        while time.monotonic() < deadline:
            if self.process.poll() is not None:
                self.reason = "llama-server s'est arrêté avant son healthcheck"
                break
            try:
                request = Request(f"{self.endpoint}/health", method="GET")
                with urlopen(request, timeout=1.0) as response:  # nosec B310: fixed loopback URL
                    if response.status == 200:
                        self.reason = None
                        return self.status()
            except OSError:
                pass
            time.sleep(0.15)
        self.stop()
        raise LocalModelSupervisorError(self.reason or "Healthcheck Qwen expiré")

    def status(self):
        running = self.process is not None and self.process.poll() is None
        return {
            "contract": "labfy.local_model_supervisor.status.v1",
            "state": "READY" if running and self.reason is None else "UNAVAILABLE",
            "endpoint": self.endpoint if running else None,
            "model": self.config.model_id,
            "pid": self.process.pid if running else None,
            "started_at": self.started_at if running else None,
            "reason": self.reason,
        }

    def stop(self):
        process = self.process
        if process is None:
            return
        if process.poll() is None:
            try:
                os.killpg(process.pid, signal.SIGTERM)
                process.wait(timeout=self.STOP_TIMEOUT_SECONDS)
            except (OSError, subprocess.TimeoutExpired):
                try:
                    os.killpg(process.pid, signal.SIGKILL)
                except OSError:
                    pass
                try:
                    process.wait(timeout=self.STOP_TIMEOUT_SECONDS)
                except subprocess.TimeoutExpired:
                    pass
        self.process = None
