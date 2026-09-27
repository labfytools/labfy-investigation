#!/usr/bin/env python3
"""Bibliothèque locale privée d'espaces Labfy explicitement enregistrés."""

from __future__ import annotations

import fcntl
import hashlib
import json
import os
import subprocess
import uuid
from pathlib import Path

MAX_ENTRIES = 256
REGISTRY_CONTRACT = "labfy.web_library.registry.v1"
CREATION_INTENT_CONTRACT = "labfy.web_library.creation_intent.v1"


class LibraryError(ValueError):
    """Refus d'une opération de bibliothèque sans effet implicite sur le disque."""


class WebLibrary:
    def __init__(self, root: Path, bridge: Path):
        self.root = Path(root).absolute()
        self.bridge = Path(bridge).resolve()
        self._prepare_private_root()
        self.workspaces = self.root / "workspaces"
        self.workspaces.mkdir(mode=0o700, exist_ok=True)
        self._assert_plain_directory(self.workspaces)
        self.registry_path = self.root / "registry.json"
        self.creation_intent_path = self.root / "creation-intent.json"
        self.lock_path = self.root / "registry.lock"

    def _prepare_private_root(self):
        for component in reversed((self.root, *self.root.parents)):
            if component.exists() and component.is_symlink():
                raise LibraryError(
                    f"Lien symbolique interdit dans la bibliothèque : {component}")
        if self.root.exists():
            self._assert_plain_directory(self.root)
        else:
            self.root.mkdir(mode=0o700, parents=True)
        os.chmod(self.root, 0o700)

    @staticmethod
    def _assert_plain_directory(path: Path):
        # INVARIANT: aucun composant géré ne peut rediriger la bibliothèque.
        if path.is_symlink() or not path.is_dir():
            raise LibraryError(f"Répertoire de bibliothèque invalide : {path}")

    def _locked(self):
        descriptor = os.open(self.lock_path, os.O_RDWR | os.O_CREAT | os.O_NOFOLLOW, 0o600)
        os.fchmod(descriptor, 0o600)
        fcntl.flock(descriptor, fcntl.LOCK_EX)
        return descriptor

    def _empty_registry(self):
        return {"contract": REGISTRY_CONTRACT, "generation": 0, "entries": []}

    def _read_registry(self):
        if not self.registry_path.exists():
            return self._empty_registry()
        if self.registry_path.is_symlink() or not self.registry_path.is_file():
            raise LibraryError("Registre de bibliothèque invalide")
        try:
            value = json.loads(self.registry_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as error:
            raise LibraryError("Registre de bibliothèque illisible") from error
        if (not isinstance(value, dict) or value.get("contract") != REGISTRY_CONTRACT or
                not isinstance(value.get("generation"), int) or
                not isinstance(value.get("entries"), list) or
                len(value["entries"]) > MAX_ENTRIES):
            raise LibraryError("Registre de bibliothèque mal formé")
        seen = set()
        for entry in value["entries"]:
            if not self._valid_entry(entry) or entry["workspace_id"] in seen:
                raise LibraryError("Entrée de bibliothèque invalide")
            seen.add(entry["workspace_id"])
            self._workspace_path(entry["workspace_id"], require_exists=True)
        return value

    @staticmethod
    def _valid_entry(entry):
        if not isinstance(entry, dict) or set(entry) != {
                "workspace_id", "title", "idempotency_key", "request_hash"}:
            return False
        try:
            uuid.UUID(entry["workspace_id"])
        except (ValueError, TypeError, AttributeError):
            return False
        return (isinstance(entry["title"], str) and 0 < len(entry["title"]) <= 200 and
                isinstance(entry["idempotency_key"], str) and
                0 < len(entry["idempotency_key"]) <= 128 and
                isinstance(entry["request_hash"], str) and
                len(entry["request_hash"]) == 64)

    def _write_registry(self, value):
        self._write_json(self.registry_path, value, "registry")

    def _write_json(self, destination, value, temporary_prefix):
        temporary = self.root / f".{temporary_prefix}-{uuid.uuid4().hex}.tmp"
        descriptor = os.open(temporary, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
                             0o600)
        try:
            data = (json.dumps(value, ensure_ascii=False, sort_keys=True,
                               separators=(",", ":")) + "\n").encode()
            os.write(descriptor, data)
            os.fsync(descriptor)
        finally:
            os.close(descriptor)
        temporary.replace(destination)
        self._sync_root()

    def _sync_root(self):
        directory = os.open(self.root, os.O_RDONLY | os.O_DIRECTORY)
        try:
            os.fsync(directory)
        finally:
            os.close(directory)

    def _read_creation_intent(self):
        if not self.creation_intent_path.exists():
            return None
        if (self.creation_intent_path.is_symlink() or
                not self.creation_intent_path.is_file()):
            raise LibraryError("Intention de création invalide")
        try:
            value = json.loads(self.creation_intent_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as error:
            raise LibraryError("Intention de création illisible") from error
        if (not isinstance(value, dict) or
                value.get("contract") != CREATION_INTENT_CONTRACT or
                not self._valid_entry({key: item for key, item in value.items()
                                       if key != "contract"}) or
                set(value) != {"contract", "workspace_id", "title",
                               "idempotency_key", "request_hash"}):
            raise LibraryError("Intention de création mal formée")
        return {key: item for key, item in value.items() if key != "contract"}

    def _write_creation_intent(self, entry):
        self._write_json(self.creation_intent_path,
                         {"contract": CREATION_INTENT_CONTRACT, **entry},
                         "creation-intent")

    def _clear_creation_intent(self):
        try:
            self.creation_intent_path.unlink()
        except FileNotFoundError:
            return
        self._sync_root()

    def _workspace_path(self, workspace_id, *, require_exists):
        try:
            normalized = str(uuid.UUID(workspace_id))
        except (ValueError, TypeError, AttributeError) as error:
            raise LibraryError("Identifiant d'enquête invalide") from error
        if normalized != workspace_id:
            raise LibraryError("Identifiant d'enquête non canonique")
        candidate = self.workspaces / normalized
        if require_exists:
            self._assert_plain_directory(candidate)
            # CONTRACT: un espace enregistré est un enfant direct, jamais un chemin exploré.
            if candidate.resolve().parent != self.workspaces.resolve():
                raise LibraryError("Espace hors bibliothèque")
        return candidate

    @staticmethod
    def _public_entry(entry):
        return {"workspace_id": entry["workspace_id"], "title": entry["title"],
                "state": "READY"}

    def snapshot(self, active_workspace_id=None):
        descriptor = self._locked()
        try:
            registry = self._read_registry()
        finally:
            os.close(descriptor)
        entries = sorted((self._public_entry(item) for item in registry["entries"]),
                         key=lambda item: (item["title"].casefold(), item["workspace_id"]))
        return {"contract": "labfy.web_library.v1", "entries": entries,
                "active_workspace_id": active_workspace_id,
                "generation": registry["generation"]}

    def create(self, title, idempotency_key):
        if (not isinstance(title, str) or not title.strip() or len(title) > 200 or
                any(ord(character) < 32 for character in title)):
            raise TypeError("Titre d'enquête invalide")
        if (not isinstance(idempotency_key, str) or not idempotency_key or
                len(idempotency_key) > 128):
            raise TypeError("Clé d'idempotence invalide")
        title = title.strip()
        request_hash = hashlib.sha256(title.encode("utf-8")).hexdigest()
        descriptor = self._locked()
        try:
            registry = self._read_registry()
            intent = self._read_creation_intent()
            if intent is not None:
                published = next((item for item in registry["entries"]
                                  if item["workspace_id"] == intent["workspace_id"]), None)
                if published is not None:
                    if published != intent:
                        raise LibraryError("Intention de création incohérente avec le registre")
                    self._clear_creation_intent()
                    intent = None
            replay = next((item for item in registry["entries"]
                           if item["idempotency_key"] == idempotency_key), None)
            if replay is not None:
                if replay["request_hash"] != request_hash:
                    raise LibraryError("Clé d'idempotence déjà liée à un autre titre")
                return self._response(replay, registry["generation"], True)
            if len(registry["entries"]) >= MAX_ENTRIES:
                raise LibraryError("Bibliothèque limitée à 256 enquêtes")
            if intent is None:
                entry = {"workspace_id": str(uuid.uuid4()), "title": title,
                         "idempotency_key": idempotency_key,
                         "request_hash": request_hash}
                self._write_creation_intent(entry)
                workspace = self._workspace_path(entry["workspace_id"],
                                                 require_exists=False)
                workspace.mkdir(mode=0o700)
                workspace_ready = False
            else:
                if (intent["idempotency_key"] != idempotency_key or
                        intent["request_hash"] != request_hash):
                    raise LibraryError(
                        "Une création interrompue doit être reprise avec sa clé d'idempotence")
                entry = intent
                workspace = self._workspace_path(entry["workspace_id"],
                                                 require_exists=False)
                if not workspace.exists():
                    workspace.mkdir(mode=0o700)
                    workspace_ready = False
                else:
                    self._workspace_path(entry["workspace_id"], require_exists=True)
                    try:
                        self._validate_created_workspace(workspace)
                        workspace_ready = True
                    except LibraryError:
                        workspace_ready = False
            if not workspace_ready:
                # CONTRACT: le bridge C existant reste l'unique créateur de l'enquête.
                result = subprocess.run(
                    [str(self.bridge), "create-workspace", "--workspace", str(workspace),
                     "--title", title], cwd=self.bridge.parents[1], text=True,
                    capture_output=True, timeout=30, check=False)
                if result.returncode != 0:
                    message = (result.stderr.strip().splitlines()[-1]
                               if result.stderr.strip() else "Création C refusée")
                    raise LibraryError(message.removeprefix("local-jobs: "))
                self._validate_created_workspace(workspace)
            registry["entries"].append(entry)
            registry["generation"] += 1
            self._write_registry(registry)
            self._clear_creation_intent()
            return self._response(entry, registry["generation"], False)
        finally:
            os.close(descriptor)

    @staticmethod
    def _validate_created_workspace(workspace):
        database = workspace / "Enquete.sqlite"
        manifest = workspace / ".labfy" / "runtime" / "workspace.json"
        if (database.is_symlink() or not database.is_file() or manifest.is_symlink() or
                not manifest.is_file()):
            raise LibraryError("Le bridge C n'a pas publié un espace valide")
        try:
            value = json.loads(manifest.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as error:
            raise LibraryError("Manifeste d'enquête invalide") from error
        if value.get("contract") != "labfy.local_workspace.v1":
            raise LibraryError("Contrat de manifeste d'enquête inattendu")

    @staticmethod
    def _response(entry, generation, replayed):
        return {"contract": "labfy.web_library.workspace.v1",
                **WebLibrary._public_entry(entry), "generation": generation,
                "replayed": replayed}

    def open(self, workspace_id, expected_generation):
        if not isinstance(expected_generation, int) or isinstance(expected_generation, bool):
            raise TypeError("Génération attendue invalide")
        descriptor = self._locked()
        try:
            registry = self._read_registry()
            if expected_generation != registry["generation"]:
                raise LibraryError("Projection de bibliothèque périmée")
            entry = next((item for item in registry["entries"]
                          if item["workspace_id"] == workspace_id), None)
            if entry is None:
                raise LibraryError("Enquête absente de la bibliothèque")
            workspace = self._workspace_path(workspace_id, require_exists=True)
            self._validate_created_workspace(workspace)
            return workspace, entry, registry["generation"]
        finally:
            os.close(descriptor)
