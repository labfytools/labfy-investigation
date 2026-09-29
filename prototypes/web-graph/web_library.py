#!/usr/bin/env python3
"""Bibliothèque locale privée d'espaces Labfy explicitement enregistrés."""
from __future__ import annotations
import fcntl
import hashlib
import heapq
import json
import os
import selectors
import secrets
import subprocess
import threading
import time
import uuid
from pathlib import Path
MAX_ENTRIES = 256
MAX_CANDIDATES = 256
REGISTRY_CONTRACT_V1 = 'labfy.web_library.registry.v1'
REGISTRY_CONTRACT = 'labfy.web_library.registry.v2'
CREATION_INTENT_CONTRACT = 'labfy.web_library.creation_intent.v1'
WORKSPACE_CONTRACT = 'labfy.local_workspace.v1'
VALIDATION_CONTRACT = 'labfy.local_workspace.validation.v1'

class LibraryError(ValueError):
    """Refus d'une opération de bibliothèque sans effet implicite sur le disque."""

class WebLibrary:

    def __init__(self, root: Path, bridge: Path, *, lazy=False):
        if not isinstance(lazy, bool):
            raise TypeError('Mode de bibliothèque invalide')
        self.root, self.bridge, self.lazy = (Path(root).absolute(), Path(bridge).resolve(), lazy)
        self.workspaces, self.registry_path = (self.root / 'workspaces', self.root / 'registry.json')
        self.creation_intent_path, self.lock_path = (self.root / 'creation-intent.json', self.root / 'registry.lock')
        self._prepare_lock, self._candidate_lock = (threading.Lock(), threading.Lock())
        self._ready, self._candidates = (False, {})
        if not lazy:
            self._ensure_ready()

    def _ensure_ready(self):
        # INVARIANT: lazy construction performs no filesystem inspection.
        with self._prepare_lock:
            if self._ready:
                return
            for part in reversed((self.root, *self.root.parents)):
                if part.exists() and part.is_symlink():
                    raise LibraryError('Lien symbolique interdit dans la bibliothèque')
            if self.root.exists():
                self._assert_dir(self.root)
            else:
                self.root.mkdir(mode=0o700, parents=True)
            os.chmod(self.root, 0o700)
            self.workspaces.mkdir(mode=0o700, exist_ok=True)
            self._assert_dir(self.workspaces)
            self._ready = True

    @staticmethod
    def _assert_dir(path):
        if path.is_symlink() or not path.is_dir():
            raise LibraryError('Répertoire de bibliothèque invalide')

    def _locked(self):
        self._ensure_ready()
        fd = os.open(self.lock_path, os.O_RDWR | os.O_CREAT | os.O_NOFOLLOW, 0o600)
        os.fchmod(fd, 0o600)
        fcntl.flock(fd, fcntl.LOCK_EX)
        return fd

    @staticmethod
    def _valid_uuid(value):
        try:
            return isinstance(value, str) and str(uuid.UUID(value)) == value
        except (ValueError, TypeError, AttributeError):
            return False

    @staticmethod
    def _valid_title(value):
        return isinstance(value, str) and 0 < len(value) <= 200 and (value == value.strip()) and (not any((ord(c) < 32 for c in value)))

    @staticmethod
    def _valid_key(value):
        return isinstance(value, str) and 0 < len(value) <= 128 and (not any((ord(c) < 32 for c in value)))

    @staticmethod
    def _valid_generation(value):
        return isinstance(value, int) and (not isinstance(value, bool)) and (value >= 0)

    @staticmethod
    def _valid_child(value):
        return isinstance(value, str) and 0 < len(value) <= 200 and (Path(value).name == value) and (not value.startswith('.')) and (not any((ord(c) < 32 for c in value)))

    def _normalize_entry(self, raw, contract):
        base = {'workspace_id', 'title', 'idempotency_key', 'request_hash'}
        expected = base if contract == REGISTRY_CONTRACT_V1 else base | {'storage_kind', 'storage_ref'}
        if not isinstance(raw, dict) or set(raw) != expected:
            return None
        item = dict(raw)
        if contract == REGISTRY_CONTRACT_V1:
            item.update(storage_kind='MANAGED', storage_ref=f"workspaces/{raw.get('workspace_id', '')}")
        if not self._valid_uuid(item.get('workspace_id')) or not self._valid_title(item.get('title')) or (not self._valid_key(item.get('idempotency_key'))) or (not isinstance(item.get('request_hash'), str)) or (len(item['request_hash']) != 64):
            return None
        if item.get('storage_kind') == 'MANAGED':
            return item if item.get('storage_ref') == f"workspaces/{item['workspace_id']}" else None
        if item.get('storage_kind') == 'EXISTING' and self._valid_child(item.get('storage_ref')):
            return item
        return None

    def _read_registry(self):
        if self.registry_path.is_symlink() or not self.registry_path.is_file():
            if not self.registry_path.exists() and not self.registry_path.is_symlink():
                return {'contract': REGISTRY_CONTRACT, 'generation': 0, 'entries': []}
            raise LibraryError('Registre de bibliothèque invalide')
        try:
            value = json.loads(self.registry_path.read_text(encoding='utf-8'))
        except (OSError, UnicodeError, json.JSONDecodeError) as error:
            raise LibraryError('Registre de bibliothèque illisible') from error
        contract = value.get('contract') if isinstance(value, dict) else None
        if contract not in {REGISTRY_CONTRACT_V1, REGISTRY_CONTRACT} or not self._valid_generation(value.get('generation')) or (not isinstance(value.get('entries'), list)) or (len(value['entries']) > MAX_ENTRIES):
            raise LibraryError('Registre de bibliothèque mal formé')
        entries = []
        ids = set()
        refs = set()
        keys = set()
        for raw in value['entries']:
            item = self._normalize_entry(raw, contract)
            if item is None or item['workspace_id'] in ids or item['storage_ref'] in refs or (item['idempotency_key'] in keys):
                raise LibraryError('Entrée de bibliothèque invalide ou dupliquée')
            ids.add(item['workspace_id'])
            refs.add(item['storage_ref'])
            keys.add(item['idempotency_key'])
            entries.append(item)
        return {'contract': contract, 'generation': value['generation'], 'entries': entries}

    def _write_json(self, destination, value, prefix):
        temp = destination.parent / f'.{prefix}-{uuid.uuid4().hex}.tmp'
        # INVARIANT: registry/manifest publication is temp + fsync + replace +
        # parent fsync, so a crash exposes either the old or the new JSON.
        fd = os.open(temp, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
        try:
            data = (json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(',', ':')) + '\n').encode()
            offset = 0
            while offset < len(data):
                offset += os.write(fd, data[offset:])
            os.fsync(fd)
        except Exception:
            try:
                temp.unlink()
            except FileNotFoundError:
                pass
            raise
        finally:
            os.close(fd)
        temp.replace(destination)
        self._sync_dir(destination.parent)

    @staticmethod
    def _sync_dir(path):
        fd = os.open(path, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
        try:
            os.fsync(fd)
        finally:
            os.close(fd)

    def _write_registry(self, registry):
        self._write_json(self.registry_path, {'contract': REGISTRY_CONTRACT, 'generation': registry['generation'], 'entries': registry['entries']}, 'registry')

    def _read_intent(self):
        if self.creation_intent_path.is_symlink() or not self.creation_intent_path.is_file():
            if (not self.creation_intent_path.exists() and
                    not self.creation_intent_path.is_symlink()):
                return None
            raise LibraryError('Intention de création invalide')
        try:
            value = json.loads(self.creation_intent_path.read_text(encoding='utf-8'))
        except (OSError, json.JSONDecodeError) as error:
            raise LibraryError('Intention de création illisible') from error
        raw = {k: v for k, v in value.items() if k != 'contract'}
        if value.get('contract') != CREATION_INTENT_CONTRACT or self._normalize_entry(raw, REGISTRY_CONTRACT_V1) is None:
            raise LibraryError('Intention de création mal formée')
        return raw

    def _clear_intent(self):
        try:
            self.creation_intent_path.unlink()
        except FileNotFoundError:
            return
        self._sync_dir(self.root)

    def _entry_path(self, item, require=False):
        path = self.workspaces / item['workspace_id'] if item['storage_kind'] == 'MANAGED' else self.root / item['storage_ref']
        parent = self.workspaces.resolve() if item['storage_kind'] == 'MANAGED' else self.root.resolve()
        if require:
            self._assert_dir(path)
            if path.resolve().parent != parent:
                raise LibraryError('Espace hors bibliothèque')
        return path

    def _workspace_path(self, workspace_id, *, require_exists):
        if not self._valid_uuid(workspace_id):
            raise LibraryError("Identifiant d'enquête invalide")
        return self._entry_path({'workspace_id': workspace_id, 'storage_kind': 'MANAGED'}, require_exists)

    @staticmethod
    def _bridge_error(result, fallback):
        return result.stderr.strip().splitlines()[-1].removeprefix('local-jobs: ')[:300] if result.stderr.strip() else fallback

    def _validate_workspace(self, path, require_manifest=True):
        # WHY: Python checks confinement and delegates database/jobstore
        # compatibility to the strictly read-only C bridge.
        database = path / 'Enquete.sqlite'
        if database.is_symlink() or not database.is_file():
            raise LibraryError("Base d'enquête absente ou invalide")
        labfy = path / '.labfy'
        runtime = labfy / 'runtime'
        jobs = runtime / 'jobs.sqlite'
        for directory in (labfy, runtime):
            if directory.is_symlink() or not directory.is_dir():
                raise LibraryError('Répertoire de métadonnées invalide')
        if jobs.is_symlink() or not jobs.is_file():
            raise LibraryError('JobStore compatible absent ou invalide')
        before = self._stat(database)
        result = self._run_validator(path)
        if result.returncode:
            raise LibraryError(self._bridge_error(result, "Base d'enquête incompatible"))
        try:
            core = json.loads(result.stdout)
        except json.JSONDecodeError as error:
            raise LibraryError('Validation C illisible') from error
        if core.get('contract') != VALIDATION_CONTRACT or not self._valid_uuid(core.get('workspace_id')) or (not self._valid_title(core.get('title'))):
            raise LibraryError('Validation C invalide')
        if before != self._stat(database):
            raise LibraryError('CANDIDATE_CHANGED : relancez la recherche')
        manifest = path / '.labfy/runtime/workspace.json'
        if manifest.is_symlink():
            raise LibraryError("Manifeste d'enquête invalide")
        if not manifest.exists():
            if require_manifest:
                raise LibraryError("Métadonnées d'enquête absentes")
            return {**core, 'manifest': False}
        if not manifest.is_file():
            raise LibraryError("Manifeste d'enquête invalide")
        try:
            data = json.loads(manifest.read_text(encoding='utf-8'))
        except (OSError, UnicodeError, json.JSONDecodeError) as error:
            raise LibraryError("Manifeste d'enquête invalide") from error
        if set(data) != {'contract', 'version', 'investigation_id', 'title', 'mode', 'database'} or data.get('contract') != WORKSPACE_CONTRACT or data.get('version') != 1 or (data.get('database') != 'Enquete.sqlite') or (data.get('mode') != 'local_experimental') or (data.get('investigation_id') != core['workspace_id']) or (data.get('title') != core['title']):
            raise LibraryError("Manifeste d'enquête incohérent")
        return {**core, 'manifest': True}

    def _run_validator(self, path):
        arguments = [str(self.bridge), 'validate-workspace-json',
                     '--workspace', str(path)]
        process = subprocess.Popen(
            arguments,
            cwd=self.bridge.parents[1],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        streams = selectors.DefaultSelector()
        streams.register(process.stdout, selectors.EVENT_READ, 'stdout')
        streams.register(process.stderr, selectors.EVENT_READ, 'stderr')
        output = {'stdout': bytearray(), 'stderr': bytearray()}
        deadline = time.monotonic() + 30
        try:
            while streams.get_map():
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise LibraryError('Validation C hors délai')
                ready = streams.select(remaining)
                if not ready:
                    raise LibraryError('Validation C hors délai')
                for key, _events in ready:
                    chunk = os.read(key.fileobj.fileno(), 4096)
                    if not chunk:
                        streams.unregister(key.fileobj)
                        continue
                    target = output[key.data]
                    if len(target) + len(chunk) > 64 * 1024:
                        raise LibraryError('Sortie de validation C trop volumineuse')
                    target.extend(chunk)
            returncode = process.wait(timeout=max(0.1, deadline - time.monotonic()))
        except Exception:
            process.kill()
            process.wait()
            raise
        finally:
            streams.close()
            process.stdout.close()
            process.stderr.close()
        return subprocess.CompletedProcess(
            arguments,
            returncode,
            output['stdout'].decode('utf-8', errors='replace'),
            output['stderr'].decode('utf-8', errors='replace'),
        )

    @staticmethod
    def _stat(path):
        value = path.stat(follow_symlinks=False)
        return (value.st_dev, value.st_ino, value.st_size, value.st_mtime_ns, value.st_ctime_ns)

    def _signature(self, path):
        manifest = path / '.labfy/runtime/workspace.json'
        return (self._stat(path), self._stat(path / 'Enquete.sqlite'), self._stat(manifest) if manifest.exists() else None)

    @staticmethod
    def _validate_created_workspace(path):
        database = path / 'Enquete.sqlite'
        manifest = path / '.labfy/runtime/workspace.json'
        if database.is_symlink() or not database.is_file() or manifest.is_symlink() or (not manifest.is_file()):
            raise LibraryError("Le bridge C n'a pas publié un espace valide")
        try:
            data = json.loads(manifest.read_text(encoding='utf-8'))
        except (OSError, json.JSONDecodeError) as error:
            raise LibraryError("Manifeste d'enquête invalide") from error
        if data.get('contract') != WORKSPACE_CONTRACT:
            raise LibraryError("Contrat de manifeste d'enquête inattendu")

    def _validate_managed(self, path):
        self._validate_created_workspace(path)
        data = json.loads((path / '.labfy/runtime/workspace.json').read_text(encoding='utf-8'))
        if not self._valid_uuid(data.get('investigation_id')) or not self._valid_title(data.get('title')):
            raise LibraryError("Manifeste d'enquête invalide")
        return {'workspace_id': data['investigation_id'], 'title': data['title']}

    def _state(self, item):
        path = self._entry_path(item)
        if path.is_symlink():
            return 'INVALID'
        if not path.exists():
            return 'MISSING'
        try:
            self._entry_path(item, True)
            info = self._validate_managed(path) if item['storage_kind'] == 'MANAGED' else self._validate_workspace(path)
            if item['storage_kind'] == 'EXISTING' and (info['workspace_id'] != item['workspace_id'] or info['title'] != item['title']):
                raise LibraryError('incohérent')
            return 'READY'
        except (LibraryError, OSError, subprocess.TimeoutExpired):
            return 'INVALID'

    def snapshot(self, active_workspace_id=None):
        fd = self._locked()
        try:
            registry = self._read_registry()
        finally:
            os.close(fd)
        entries = [{'workspace_id': e['workspace_id'], 'title': e['title'], 'storage_kind': e['storage_kind'], 'state': self._state(e)} for e in registry['entries']]
        entries.sort(key=lambda e: (e['title'].casefold(), e['workspace_id']))
        return {'contract': 'labfy.web_library.v1', 'entries': entries, 'active_workspace_id': active_workspace_id, 'generation': registry['generation']}

    @staticmethod
    def _response(item, generation, replayed):
        return {'contract': 'labfy.web_library.workspace.v1', 'workspace_id': item['workspace_id'], 'title': item['title'], 'state': 'READY', 'generation': generation, 'replayed': replayed}

    def create(self, title, idempotency_key):
        if not self._valid_title(title):
            raise TypeError("Titre d'enquête invalide")
        if not self._valid_key(idempotency_key):
            raise TypeError("Clé d'idempotence invalide")
        request_hash = hashlib.sha256(title.encode()).hexdigest()
        fd = self._locked()
        try:
            registry = self._read_registry()
            intent = self._read_intent()
            replay = next((e for e in registry['entries'] if e['idempotency_key'] == idempotency_key), None)
            if replay:
                if replay['request_hash'] != request_hash or replay['storage_kind'] != 'MANAGED':
                    raise LibraryError("Clé d'idempotence déjà liée à un autre titre")
                if intent and intent['workspace_id'] == replay['workspace_id']:
                    self._clear_intent()
                return self._response(replay, registry['generation'], True)
            if len(registry['entries']) >= MAX_ENTRIES:
                raise LibraryError('Bibliothèque limitée à 256 enquêtes')
            if intent is None:
                intent = {'workspace_id': str(uuid.uuid4()), 'title': title, 'idempotency_key': idempotency_key, 'request_hash': request_hash}
                self._write_json(self.creation_intent_path, {'contract': CREATION_INTENT_CONTRACT, **intent}, 'creation-intent')
            elif intent['idempotency_key'] != idempotency_key or intent['request_hash'] != request_hash:
                raise LibraryError("Une création interrompue doit être reprise avec sa clé d'idempotence")
            path = self._workspace_path(intent['workspace_id'], require_exists=False)
            ready = False
            if path.exists():
                self._workspace_path(intent['workspace_id'], require_exists=True)
                try:
                    self._validate_created_workspace(path)
                    ready = True
                except LibraryError:
                    pass
            else:
                path.mkdir(mode=448)
            if not ready:
                result = subprocess.run([str(self.bridge), 'create-workspace', '--workspace', str(path), '--title', title], cwd=self.bridge.parents[1], text=True, capture_output=True, timeout=30, check=False)
                if result.returncode:
                    raise LibraryError(self._bridge_error(result, 'Création C refusée'))
                self._validate_created_workspace(path)
            item = {**intent, 'storage_kind': 'MANAGED', 'storage_ref': f"workspaces/{intent['workspace_id']}"}
            registry['entries'].append(item)
            registry['generation'] += 1
            self._write_registry(registry)
            self._clear_intent()
            return self._response(item, registry['generation'], False)
        finally:
            os.close(fd)

    @staticmethod
    def _candidate(candidate_id, name, workspace_id, title, state, reason):
        return {'candidate_id': candidate_id, 'display_name': name[:200], 'workspace_id': workspace_id, 'title': title, 'state': state, 'reason': reason}

    def _inspect(self, path, candidate_id):
        if path.is_symlink() or not path.is_dir() or path.resolve().parent != self.root.resolve():
            return (self._candidate(candidate_id, path.name, None, None, 'INVALID', 'Lien symbolique ou dossier hors bibliothèque'), None)
        specimen = path / '.labfy/runtime/specimen.json'
        if specimen.exists() or specimen.is_symlink():
            return (self._candidate(candidate_id, path.name, None, None, 'UNSUPPORTED_LEGACY_LAYOUT', 'Disposition SPECIMEN historique non enregistrable'), None)
        try:
            info = self._validate_workspace(path, False)
            signature = self._signature(path)
        except (LibraryError, OSError, subprocess.TimeoutExpired) as error:
            reason = self._safe_candidate_reason(error)
            return (self._candidate(candidate_id, path.name, None, None,
                                    'INVALID', reason), None)
        state = 'READY_TO_REGISTER' if info['manifest'] else 'NEEDS_METADATA_MIGRATION'
        reason = 'Dossier valide prêt à enregistrer' if info['manifest'] else 'Base compatible ; métadonnées à créer, base inchangée'
        return (self._candidate(candidate_id, path.name, info['workspace_id'], info['title'], state, reason), signature)

    @staticmethod
    def _safe_candidate_reason(error):
        allowed = {
            "Base d'enquête absente ou invalide",
            'Répertoire de métadonnées invalide',
            'JobStore compatible absent ou invalide',
            "Manifeste d'enquête invalide",
            "Manifeste d'enquête incohérent",
            'Validation C invalide',
            'Validation C illisible',
            'Validation C hors délai',
            'Sortie de validation C trop volumineuse',
        }
        message = str(error)
        return message if message in allowed else 'Dossier invalide ou incompatible'

    def discover_existing(self):
        # CONTRACT: discovery creates no directory, lock file or metadata.
        if not self.root.exists():
            return {'contract': 'labfy.web_library.discovery.v1', 'generation': 0, 'candidates': []}
        self._assert_dir(self.root)
        registry = self._read_registry()
        registered = {e['storage_ref']: e for e in registry['entries'] if e['storage_kind'] == 'EXISTING'}

        def discoverable_names():
            # INVARIANT: nsmallest retains at most MAX_CANDIDATES names even
            # when the library root contains many direct children.
            with os.scandir(self.root) as iterator:
                for entry in iterator:
                    if entry.name in {'workspaces', 'registry.json', 'registry.lock', 'creation-intent.json'} or entry.name.startswith('.'):
                        continue
                    if entry.is_dir(follow_symlinks=False) or entry.is_symlink():
                        yield entry.name
        cache = {}
        public = []
        names = heapq.nsmallest(MAX_CANDIDATES, discoverable_names(), key=str.casefold)
        for name in names:
            cid = secrets.token_urlsafe(24)
            if not self._valid_child(name):
                result, signature = (self._candidate(cid, name, None, None, 'INVALID', 'Nom de dossier invalide'), None)
            else:
                result, signature = self._inspect(self.root / name, cid)
            if name in registered and result['workspace_id'] == registered[name]['workspace_id']:
                result.update(state='ALREADY_REGISTERED', reason='Enquête déjà enregistrée')
            cache[cid] = {'name': name, 'signature': signature, 'state': result['state'], 'workspace_id': result['workspace_id'], 'title': result['title']}
            public.append(result)
        with self._candidate_lock:
            self._candidates = cache
        return {'contract': 'labfy.web_library.discovery.v1', 'generation': registry['generation'], 'candidates': public}

    def _write_manifest(self, path, workspace_id, title):
        for directory in (path / '.labfy', path / '.labfy/runtime'):
            if directory.exists():
                self._assert_dir(directory)
            else:
                directory.mkdir(mode=0o700)
                self._sync_dir(directory.parent)
        manifest = path / '.labfy/runtime/workspace.json'
        if manifest.exists() or manifest.is_symlink():
            raise LibraryError('Manifeste existant non remplaçable')
        self._write_json(manifest, {'contract': WORKSPACE_CONTRACT, 'version': 1, 'investigation_id': workspace_id, 'title': title, 'mode': 'local_experimental', 'database': 'Enquete.sqlite'}, 'workspace')

    def register_existing(self, candidate_id, expected_generation, idempotency_key, human_confirmed):
        if not isinstance(candidate_id, str) or not candidate_id or len(candidate_id) > 128:
            raise TypeError('Identifiant de candidat invalide')
        if not self._valid_generation(expected_generation):
            raise TypeError('Génération attendue invalide')
        if not self._valid_key(idempotency_key):
            raise TypeError("Clé d'idempotence invalide")
        if human_confirmed is not True:
            raise LibraryError('Confirmation humaine requise')
        with self._candidate_lock:
            cached = dict(self._candidates.get(candidate_id, {}))
        if not cached:
            raise LibraryError('Candidat inconnu ; relancez la recherche')
        fd = self._locked()
        try:
            registry = self._read_registry()
            intention = {
                'candidate_id': candidate_id,
                'name': cached['name'],
                'workspace_id': cached['workspace_id'],
                'title': cached['title'],
                'expected_generation': expected_generation,
                'human_confirmed': True,
            }
            request_hash = hashlib.sha256(
                json.dumps(
                    intention,
                    sort_keys=True,
                    separators=(',', ':'),
                ).encode()
            ).hexdigest()
            replay = next((e for e in registry['entries'] if e['idempotency_key'] == idempotency_key), None)
            if replay:
                if replay['request_hash'] != request_hash:
                    raise LibraryError("Clé d'idempotence déjà liée à une autre intention")
                return self._response(replay, registry['generation'], True)
            if expected_generation != registry['generation']:
                raise LibraryError('Projection de bibliothèque périmée')
            if cached['state'] not in {'READY_TO_REGISTER', 'NEEDS_METADATA_MIGRATION'}:
                raise LibraryError('Candidat non enregistrable')
            path = self.root / cached['name']
            current, signature = self._inspect(path, candidate_id)
            identity_matches = (
                current['workspace_id'] == cached['workspace_id'] and
                current['title'] == cached['title']
            )
            metadata_recovery = (
                cached['state'] == 'NEEDS_METADATA_MIGRATION' and
                current['state'] == 'READY_TO_REGISTER' and
                identity_matches and
                signature is not None and
                cached['signature'] is not None and
                signature[1] == cached['signature'][1]
            )
            unchanged_candidate = (
                signature == cached['signature'] and
                current['state'] == cached['state'] and
                identity_matches
            )
            if not unchanged_candidate and not metadata_recovery:
                raise LibraryError('CANDIDATE_CHANGED : relancez la recherche')
            if any((e['workspace_id'] == cached['workspace_id'] or e['storage_ref'] == cached['name'] for e in registry['entries'])):
                raise LibraryError('Enquête ou dossier déjà enregistré')
            if len(registry['entries']) >= MAX_ENTRIES:
                raise LibraryError('Bibliothèque limitée à 256 enquêtes')
            if cached['state'] == 'NEEDS_METADATA_MIGRATION' and not metadata_recovery:
                self._write_manifest(path, cached['workspace_id'], cached['title'])
                self._validate_workspace(path)
            item = {'workspace_id': cached['workspace_id'], 'title': cached['title'], 'storage_kind': 'EXISTING', 'storage_ref': cached['name'], 'idempotency_key': idempotency_key, 'request_hash': request_hash}
            registry['entries'].append(item)
            registry['generation'] += 1
            self._write_registry(registry)
            return self._response(item, registry['generation'], False)
        finally:
            os.close(fd)

    def open(self, workspace_id, expected_generation):
        if not self._valid_generation(expected_generation):
            raise TypeError('Génération attendue invalide')
        fd = self._locked()
        try:
            registry = self._read_registry()
            if expected_generation != registry['generation']:
                raise LibraryError('Projection de bibliothèque périmée')
            item = next((e for e in registry['entries'] if e['workspace_id'] == workspace_id), None)
            if item is None:
                raise LibraryError('Enquête absente de la bibliothèque')
            path = self._entry_path(item, True)
            info = self._validate_managed(path) if item['storage_kind'] == 'MANAGED' else self._validate_workspace(path)
            if item['storage_kind'] == 'EXISTING' and (info['workspace_id'] != item['workspace_id'] or info['title'] != item['title']):
                raise LibraryError('Enquête enregistrée invalide')
            return (path, item, registry['generation'])
        finally:
            os.close(fd)
