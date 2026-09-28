#!/usr/bin/env python3
"""Lanceur local explicite et gestionnaire d'instance du poste Web Labfy."""

from __future__ import annotations

import argparse
import fcntl
import hashlib
import http.client
import json
import os
import select
import secrets
import signal
import subprocess
import sys
import threading
import time
import webbrowser
from pathlib import Path

from web_library import WebLibrary
from workspace_server import Handler, WorkspaceServer

DEFAULT_PORT = 8081
APP_DIRECTORY = "labfy-investigation-web"


def valid_port(value):
    try:
        port = int(value)
    except (TypeError, ValueError) as error:
        raise argparse.ArgumentTypeError("le port doit être un entier") from error
    if not 0 <= port <= 65535:
        raise argparse.ArgumentTypeError("le port doit être compris entre 0 et 65535")
    return port


def _private_directory(path: Path):
    absolute = Path(os.path.abspath(path))
    for component in reversed((absolute, *absolute.parents)):
        if component.exists() and component.is_symlink():
            raise RuntimeError(f"Lien symbolique interdit dans l'état : {component}")
    path = absolute
    if path.exists() and (path.is_symlink() or not path.is_dir()):
        raise RuntimeError(f"Répertoire d'état non sûr : {path}")
    path.mkdir(mode=0o700, parents=True, exist_ok=True)
    if path.stat().st_uid != os.getuid():
        raise RuntimeError(f"Répertoire d'état d'un autre utilisateur : {path}")
    os.chmod(path, 0o700)
    return path


def runtime_directory(env=None):
    env = os.environ if env is None else env
    base = env.get("XDG_RUNTIME_DIR")
    if base:
        root = Path(base)
        if root.is_symlink() or not root.is_dir() or root.stat().st_uid != os.getuid():
            raise RuntimeError("XDG_RUNTIME_DIR non sûr")
    else:
        # WHY: le repli est propre à l'UID et non partageable dans /tmp.
        root = Path(f"/tmp/labfy-runtime-{os.getuid()}")
    return _private_directory(root / APP_DIRECTORY)


def state_directory(env=None):
    env = os.environ if env is None else env
    base = Path(env.get("XDG_STATE_HOME", Path.home() / ".local" / "state"))
    return _private_directory(base / APP_DIRECTORY)


def _atomic_json(path, value):
    temporary = path.with_name(f".{path.name}-{secrets.token_hex(8)}.tmp")
    descriptor = os.open(temporary, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
                         0o600)
    try:
        payload = (json.dumps(value, ensure_ascii=False, sort_keys=True,
                              separators=(",", ":")) + "\n").encode()
        os.write(descriptor, payload)
        os.fsync(descriptor)
    finally:
        os.close(descriptor)
    temporary.replace(path)


def _read_private_json(path):
    if path.is_symlink() or not path.is_file() or path.stat().st_uid != os.getuid():
        raise RuntimeError("Fichier d'instance absent ou non sûr")
    return json.loads(path.read_text(encoding="utf-8"))


def _proc_start(pid):
    try:
        # /proc/PID/stat place le nom entre parenthèses ; le champ 22 est le 20e
        # champ après la dernière parenthèse.
        fields = Path(f"/proc/{pid}/stat").read_text(encoding="ascii").rsplit(")", 1)[1].split()
        return fields[19]
    except (OSError, IndexError):
        return None


def _config(selection, bridge):
    mode, path = selection
    value = {"mode": mode, "path": str(path.absolute()), "bridge": str(bridge.resolve())}
    digest = hashlib.sha256(json.dumps(value, sort_keys=True,
        separators=(",", ":")).encode()).hexdigest()
    return value, digest


def _health(state, timeout=0.8):
    try:
        connection = http.client.HTTPConnection("127.0.0.1", state["port"], timeout=timeout)
        connection.request("GET", "/healthz", headers={"Host": f"127.0.0.1:{state['port']}"})
        response = connection.getresponse()
        value = json.loads(response.read())
        connection.close()
        return (response.status == 200 and value.get("instance_id") == state["instance_id"] and
                value.get("config_id") == state["config_id"])
    except (OSError, ValueError, KeyError, json.JSONDecodeError):
        return False


def _identity_valid(state):
    return (isinstance(state, dict) and isinstance(state.get("pid"), int) and
            not isinstance(state.get("pid"), bool) and state["pid"] > 1 and
            isinstance(state.get("proc_start"), str) and
            state.get("proc_start") == _proc_start(state["pid"]) and _health(state))


class InstanceFiles:
    def __init__(self):
        self.runtime = runtime_directory()
        self.state_root = state_directory()
        self.instance = self.runtime / "instance.json"
        self.config = self.state_root / "config.json"
        self.lifecycle = self.runtime / "lifecycle.lock"
        self.owner = self.runtime / "instance.lock"

    def lifecycle_lock(self):
        descriptor = os.open(self.lifecycle, os.O_RDWR | os.O_CREAT | os.O_NOFOLLOW, 0o600)
        fcntl.flock(descriptor, fcntl.LOCK_EX)
        return descriptor

    def clear_stale(self):
        for path in (self.instance,):
            try:
                path.unlink()
            except FileNotFoundError:
                pass


def _selection(args):
    if args.library is not None:
        return "library", args.library.absolute()
    return "workspace", args.workspace.absolute()


def _handshake(fd, value):
    if fd is None:
        return
    with os.fdopen(fd, "w", encoding="utf-8", closefd=True) as output:
        output.write(json.dumps(value, ensure_ascii=False, separators=(",", ":")) + "\n")
        output.flush()


def serve(args):
    files = InstanceFiles()
    owner = os.open(files.owner, os.O_RDWR | os.O_CREAT | os.O_NOFOLLOW, 0o600)
    try:
        fcntl.flock(owner, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        os.close(owner)
        _handshake(args.handshake_fd, {"ok": False, "error": "Une instance est déjà active"})
        return 1
    selection = _selection(args)
    config, config_id = _config(selection, args.bridge)
    instance_id = secrets.token_urlsafe(24)
    library = None
    if selection[0] == "library":
        library = WebLibrary(selection[1], args.bridge)
        workspace = library.root / ".inactive"
        workspace.mkdir(mode=0o700, exist_ok=True)
    else:
        workspace = selection[1]
        workspace.mkdir(mode=0o700, parents=True, exist_ok=True)
    try:
        server = WorkspaceServer(("127.0.0.1", args.port), Handler,
            workspace=workspace, bridge=args.bridge, bootstrap="",
            library=library, instance_id=instance_id, config_id=config_id,
            automatic_session=args.automatic_session, agent_mode=args.agent_mode,
            agent_endpoint=args.agent_endpoint, agent_model=args.agent_model,
            agent_timeout=args.agent_timeout, agent_autostart=args.agent_autostart,
            agent_config_path=args.agent_config_path)
    except OSError as error:
        _handshake(args.handshake_fd, {"ok": False,
                                      "error": f"Port indisponible : {error}"})
        os.close(owner)
        return 1
    state = {"contract": "labfy.web_instance.v2", "pid": os.getpid(),
             "proc_start": _proc_start(os.getpid()), "instance_id": instance_id,
             "config_id": config_id, "port": server.server_port, "config": config,
             "automatic_session": True}
    try:
        # CONTRACT: la configuration durable ne contient jamais le secret
        # d'amorçage, qui reste exclusivement dans le runtime privé.
        _atomic_json(files.config, {"contract": "labfy.web_config.v1",
                                   "config_id": config_id, **config})
        _atomic_json(files.instance, state)
        answer = {"ok": True, "origin": server.origin,
                  "instance_id": instance_id}
        _handshake(args.handshake_fd, answer)
        if args.handshake_fd is None:
            print(f"Labfy Web : {server.origin}/", flush=True)
        # INVARIANT: SIGTERM n'arrête que cette instance déjà authentifiée
        # par le lanceur ; shutdown s'exécute hors du gestionnaire de signal.
        def request_shutdown(_signum, _frame):
            threading.Thread(target=server.shutdown, daemon=True).start()
        signal.signal(signal.SIGTERM, request_shutdown)
        server.serve_forever()
    finally:
        server.stop_worker()
        server.server_close()
        try:
            current = _read_private_json(files.instance)
            if current.get("instance_id") == instance_id:
                files.clear_stale()
        except (OSError, RuntimeError, json.JSONDecodeError):
            pass
        os.close(owner)
    return 0


def start(args):
    files = InstanceFiles()
    lifecycle = files.lifecycle_lock()
    try:
        try:
            current = _read_private_json(files.instance)
        except (OSError, RuntimeError, json.JSONDecodeError):
            current = None
        if current is not None and _identity_valid(current):
            origin = f"http://127.0.0.1:{current['port']}/"
            if (current.get("automatic_session") is True and
                    current.get("contract") == "labfy.web_instance.v2"):
                if args.open_browser:
                    webbrowser.open(origin)
                print(f"Instance déjà active sur {origin}", file=sys.stderr)
                return 0
            # CONTRACT: une instance authentifiée mais d'ancien profil ne peut
            # jamais être rejointe : elle ramènerait le parcours code retiré.
            print(f"Instance incompatible sur {origin} ; remplacement contrôlé",
                  file=sys.stderr)
            os.kill(current["pid"], signal.SIGTERM)
            deadline = time.monotonic() + 5
            while (time.monotonic() < deadline and
                   _proc_start(current["pid"]) == current["proc_start"]):
                time.sleep(0.05)
            if _proc_start(current["pid"]) == current["proc_start"]:
                print("Arrêt de l'instance incompatible expiré", file=sys.stderr)
                return 1
            files.clear_stale()
        files.clear_stale()
        read_fd, write_fd = os.pipe()
        command = [sys.executable, str(Path(__file__).resolve()), "serve",
                   "--port", str(args.port), "--bridge", str(args.bridge),
                   "--handshake-fd", str(write_fd)]
        command.append("--automatic-session")
        command.extend(("--agent-mode", args.agent_mode))
        if args.agent_autostart:
            command.append("--agent-autostart")
        if args.agent_config_path is not None:
            command.extend(("--agent-config-path", str(args.agent_config_path)))
        if args.agent_endpoint is not None:
            command.extend(("--agent-endpoint", args.agent_endpoint))
        if args.agent_model is not None:
            command.extend(("--agent-model", args.agent_model))
        command.extend(("--agent-timeout", str(args.agent_timeout)))
        mode, path = _selection(args)
        command.extend((f"--{mode}", str(path)))
        process = subprocess.Popen(command, stdin=subprocess.DEVNULL,
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            start_new_session=True, pass_fds=(write_fd,), close_fds=True)
        os.close(write_fd)
        ready, _, _ = select.select((read_fd,), (), (), 10)
        if not ready:
            process.terminate()
            process.wait(timeout=3)
            os.close(read_fd)
            print("Démarrage expiré", file=sys.stderr)
            return 1
        with os.fdopen(read_fd, "r", encoding="utf-8") as pipe:
            line = pipe.readline()
        if not line:
            print("Le serveur n'a pas publié son état", file=sys.stderr)
            return 1
        response = json.loads(line)
        if not response.get("ok"):
            print(response.get("error", "Démarrage refusé"), file=sys.stderr)
            return 1
        print(f"Labfy Web : {response['origin']}/")
        if args.open_browser:
            webbrowser.open(f"{response['origin']}/")
        return 0
    finally:
        os.close(lifecycle)


def status(_args):
    files = InstanceFiles()
    try:
        state = _read_private_json(files.instance)
    except (OSError, RuntimeError, json.JSONDecodeError):
        print("arrêté")
        return 1
    if not _identity_valid(state):
        print("état périmé")
        return 1
    print(f"actif pid={state['pid']} origin=http://127.0.0.1:{state['port']}/")
    return 0


def stop(_args):
    files = InstanceFiles()
    lifecycle = files.lifecycle_lock()
    try:
        try:
            state = _read_private_json(files.instance)
        except (OSError, RuntimeError, json.JSONDecodeError):
            print("Aucune instance active", file=sys.stderr)
            return 1
        # CONTRACT: ne jamais signaler un PID sur la seule foi d'un fichier d'état.
        if not _identity_valid(state):
            print("Instance périmée ou identité non confirmée", file=sys.stderr)
            return 1
        os.kill(state["pid"], signal.SIGTERM)
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline and _proc_start(state["pid"]) == state["proc_start"]:
            time.sleep(0.05)
        if _proc_start(state["pid"]) == state["proc_start"]:
            print("Arrêt expiré ; aucun signal forcé envoyé", file=sys.stderr)
            return 1
        files.clear_stale()
        print("arrêté")
        return 0
    finally:
        os.close(lifecycle)


def build_parser():
    parser = argparse.ArgumentParser(description="Poste Web local Labfy")
    subparsers = parser.add_subparsers(dest="command", required=True)
    repository = Path(__file__).resolve().parents[2]
    for name, function in (("serve", serve), ("start", start)):
        command = subparsers.add_parser(name)
        targets = command.add_mutually_exclusive_group(required=True)
        targets.add_argument("--library", type=Path)
        targets.add_argument("--workspace", type=Path)
        command.add_argument("--port", type=valid_port, default=DEFAULT_PORT)
        command.add_argument("--bridge", type=Path, default=repository / "tools/local-jobs")
        command.add_argument("--automatic-session", action="store_true",
                             help="établit une session locale sans code visible")
        command.add_argument("--agent-mode", choices=("deterministic-demo", "local-model"),
                             default=os.environ.get("LABFY_AGENT_MODE", "deterministic-demo"))
        command.add_argument("--agent-endpoint",
                             default=os.environ.get("LABFY_AGENT_MODEL_ENDPOINT"))
        command.add_argument("--agent-model", default=os.environ.get("LABFY_AGENT_MODEL_ID"))
        command.add_argument("--agent-timeout", type=float,
                             default=float(os.environ.get(
                                 "LABFY_AGENT_MODEL_TIMEOUT_SECONDS", "10")))
        command.add_argument("--agent-autostart", action="store_true",
                             help="démarre le seul llama-server configuré dans XDG")
        command.add_argument("--agent-config-path", type=Path,
                             help="configuration agent explicite et non versionnée")
        if name == "start":
            command.add_argument("--open-browser", action="store_true",
                                 help="ouvre l'interface locale après vérification")
        if name == "serve":
            command.add_argument("--handshake-fd", type=int, help=argparse.SUPPRESS)
        command.set_defaults(function=function)
    for name, function in (("status", status), ("stop", stop)):
        command = subparsers.add_parser(name)
        command.set_defaults(function=function)
    help_command = subparsers.add_parser("help")
    help_command.set_defaults(function=lambda _args: (parser.print_help() or 0))
    return parser


def main(argv=None):
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        return args.function(args)
    except (RuntimeError, ValueError, OSError, json.JSONDecodeError) as error:
        print(f"web-app: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
