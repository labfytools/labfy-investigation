"""Contrats SPECIMEN de découverte et d’adoption d’enquêtes existantes."""
import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
import uuid
from pathlib import Path
from unittest import mock
WEB = Path(__file__).resolve().parents[1]
ROOT = WEB.parents[1]
sys.path.insert(0, str(WEB))
from web_library import LibraryError, WebLibrary, REGISTRY_CONTRACT

class ExistingLibraryTest(unittest.TestCase):

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='labfy-existing-SPECIMEN-')
        self.root = Path(self.temp.name) / 'library'
        self.root.mkdir(mode=0o700)
        self.bridge = ROOT / 'tools/local-jobs'

    def tearDown(self):
        self.temp.cleanup()

    def workspace(self, name, title):
        path = self.root / name
        path.mkdir()
        result = subprocess.run([str(self.bridge), 'create-workspace', '--workspace', str(path), '--title', title], cwd=ROOT, text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        return path

    @staticmethod
    def digest(path):
        return hashlib.sha256(path.read_bytes()).hexdigest()

    def test_discovery_is_direct_read_only_opaque_and_bounded(self):
        valid = self.workspace('Existing-A', 'SPECIMEN Existing A')
        (self.root / 'ordinary.txt').write_text('SPECIMEN', encoding='utf-8')
        nested = self.root / 'container'
        nested.mkdir()
        (nested / 'Nested').mkdir()
        (self.root / '.hidden').mkdir()
        (self.root / 'link').symlink_to(valid, target_is_directory=True)
        library = WebLibrary(self.root, self.bridge, lazy=True)
        before = {p.relative_to(self.root).as_posix() for p in self.root.rglob('*')}
        result = library.discover_existing()
        after = {p.relative_to(self.root).as_posix() for p in self.root.rglob('*')}
        self.assertEqual(before, after)
        self.assertNotIn('Nested', [c['display_name'] for c in result['candidates']])
        self.assertNotIn('.hidden', [c['display_name'] for c in result['candidates']])
        candidate = next((c for c in result['candidates'] if c['display_name'] == 'Existing-A'))
        self.assertEqual(candidate['state'], 'READY_TO_REGISTER')
        self.assertNotIn(str(self.root), json.dumps(result))
        self.assertEqual(next((c for c in result['candidates'] if c['display_name'] == 'link'))['state'], 'INVALID')

    def test_register_existing_no_copy_idempotence_and_open(self):
        path = self.workspace('Existing-B', 'SPECIMEN Existing B')
        database = path / 'Enquete.sqlite'
        before = self.digest(database)
        library = WebLibrary(self.root, self.bridge)
        discovery = library.discover_existing()
        candidate = next((c for c in discovery['candidates'] if c['display_name'] == 'Existing-B'))
        result = library.register_existing(candidate['candidate_id'], discovery['generation'], 'register-b', True)
        self.assertFalse(result['replayed'])
        self.assertEqual(before, self.digest(database))
        self.assertFalse((library.workspaces / result['workspace_id']).exists())
        replay = library.register_existing(
            candidate['candidate_id'],
            discovery['generation'],
            'register-b',
            True,
        )
        self.assertTrue(replay['replayed'])
        with self.assertRaisesRegex(LibraryError, 'autre intention'):
            library.register_existing(
                candidate['candidate_id'],
                result['generation'],
                'register-b',
                True,
            )
        opened, entry, generation = library.open(result['workspace_id'], result['generation'])
        self.assertEqual(opened, path)
        self.assertEqual(entry['storage_kind'], 'EXISTING')
        self.assertEqual(generation, 1)
        registry = json.loads(library.registry_path.read_text(encoding='utf-8'))
        self.assertEqual(registry['contract'], REGISTRY_CONTRACT)
        self.assertEqual(registry['entries'][0]['storage_ref'], 'Existing-B')

    def test_metadata_only_adoption_preserves_database(self):
        path = self.workspace('Existing-Metadata', 'SPECIMEN Metadata')
        manifest = path / '.labfy/runtime/workspace.json'
        manifest.unlink()
        before = self.digest(path / 'Enquete.sqlite')
        library = WebLibrary(self.root, self.bridge)
        discovery = library.discover_existing()
        candidate = discovery['candidates'][0]
        self.assertEqual(candidate['state'], 'NEEDS_METADATA_MIGRATION')
        result = library.register_existing(candidate['candidate_id'], 0, 'metadata-key', True)
        self.assertEqual(before, self.digest(path / 'Enquete.sqlite'))
        self.assertTrue(manifest.is_file())
        self.assertEqual(json.loads(manifest.read_text())['investigation_id'], result['workspace_id'])

    def test_metadata_adoption_resumes_after_manifest_publication(self):
        path = self.workspace('Existing-Metadata-Crash', 'SPECIMEN Metadata Crash')
        manifest = path / '.labfy/runtime/workspace.json'
        manifest.unlink()
        database = path / 'Enquete.sqlite'
        before = self.digest(database)
        library = WebLibrary(self.root, self.bridge)
        discovery = library.discover_existing()
        candidate = discovery['candidates'][0]
        write_registry = library._write_registry
        with mock.patch.object(
                library,
                '_write_registry',
                side_effect=OSError('crash après manifeste SPECIMEN')):
            with self.assertRaisesRegex(OSError, 'après manifeste'):
                library.register_existing(
                    candidate['candidate_id'],
                    discovery['generation'],
                    'metadata-crash-key',
                    True,
                )
        self.assertTrue(manifest.is_file())
        self.assertEqual(self.digest(database), before)
        with mock.patch.object(library, '_write_registry', wraps=write_registry):
            resumed = library.register_existing(
                candidate['candidate_id'],
                discovery['generation'],
                'metadata-crash-key',
                True,
            )
        self.assertFalse(resumed['replayed'])
        self.assertEqual(resumed['generation'], 1)
        self.assertEqual(self.digest(database), before)
        self.assertFalse((library.workspaces / resumed['workspace_id']).exists())

    def test_metadata_adoption_requires_compatible_jobstore(self):
        path = self.workspace('Existing-No-Jobs', 'SPECIMEN no jobs')
        (path / '.labfy/runtime/workspace.json').unlink()
        (path / '.labfy/runtime/jobs.sqlite').unlink()
        candidate = WebLibrary(self.root, self.bridge).discover_existing()['candidates'][0]
        self.assertEqual(candidate['state'], 'INVALID')
        self.assertIn('JobStore', candidate['reason'])

    def test_metadata_parent_symlinks_are_refused(self):
        for name, component in (('Existing-Labfy-Link', '.labfy'), ('Existing-Runtime-Link', '.labfy/runtime')):
            with self.subTest(component=component):
                path = self.workspace(name, f'SPECIMEN {name}')
                target = self.root / f'target-{name}'
                target.mkdir()
                selected = path / component
                if selected.name == 'runtime':
                    selected.rename(path / '.labfy/runtime-real')
                else:
                    selected.rename(path / '.labfy-real')
                selected.symlink_to(target, target_is_directory=True)
        candidates = WebLibrary(self.root, self.bridge).discover_existing()['candidates']
        states = {item['display_name']: item['state'] for item in candidates}
        self.assertEqual(states['Existing-Labfy-Link'], 'INVALID')
        self.assertEqual(states['Existing-Runtime-Link'], 'INVALID')

    def test_candidate_change_and_confirmation_are_rejected(self):
        path = self.workspace('Existing-C', 'SPECIMEN Existing C')
        library = WebLibrary(self.root, self.bridge)
        discovery = library.discover_existing()
        candidate = discovery['candidates'][0]
        with self.assertRaisesRegex(LibraryError, 'Confirmation'):
            library.register_existing(candidate['candidate_id'], 0, 'key-c', False)
        (path / '.labfy/runtime/workspace.json').write_text('{}', encoding='utf-8')
        with self.assertRaisesRegex(LibraryError, 'CANDIDATE_CHANGED'):
            library.register_existing(candidate['candidate_id'], 0, 'key-c', True)

    def test_dangling_registry_intent_and_manifest_symlinks_are_invalid(self):
        library = WebLibrary(self.root, self.bridge)
        library.registry_path.symlink_to(self.root / 'missing-registry')
        with self.assertRaisesRegex(LibraryError, 'Registre.*invalide'):
            library.snapshot()
        library.registry_path.unlink()
        library.creation_intent_path.symlink_to(self.root / 'missing-intent')
        with self.assertRaisesRegex(LibraryError, 'Intention.*invalide'):
            library.create('SPECIMEN dangling', 'dangling-key')
        library.creation_intent_path.unlink()
        path = self.workspace('Existing-Dangling', 'SPECIMEN dangling manifest')
        manifest = path / '.labfy/runtime/workspace.json'
        manifest.unlink()
        manifest.symlink_to(path / 'missing-manifest')
        candidate = library.discover_existing()['candidates'][0]
        self.assertEqual(candidate['state'], 'INVALID')
        self.assertEqual(candidate['reason'], "Manifeste d'enquête invalide")

    def test_candidate_reason_never_exposes_exception_path(self):
        path = self.workspace('Existing-Private', 'SPECIMEN private')
        library = WebLibrary(self.root, self.bridge)
        private = '/private/SPECIMEN-secret/path'
        with mock.patch.object(library, '_validate_workspace', side_effect=OSError(private)):
            result = library.discover_existing()
        encoded = json.dumps(result)
        self.assertNotIn(private, encoded)
        self.assertEqual(result['candidates'][0]['reason'], 'Dossier invalide ou incompatible')

    def test_validator_output_is_bounded(self):
        bridge = self.root / 'noisy-validator'
        bridge.write_text("#!/usr/bin/env python3\nimport sys\nsys.stdout.write('x' * 70000)\n", encoding='utf-8')
        bridge.chmod(0o700)
        library = WebLibrary(self.root, bridge)
        with self.assertRaisesRegex(LibraryError, 'trop volumineuse'):
            library._run_validator(self.root)

    def test_registered_symlink_is_invalid_without_breaking_snapshot(self):
        library = WebLibrary(self.root, self.bridge)
        created = library.create('SPECIMEN managed symlink', 'managed-link')
        workspace = library.workspaces / created['workspace_id']
        target = self.root / 'managed-real'
        workspace.rename(target)
        workspace.symlink_to(target, target_is_directory=True)
        snapshot = library.snapshot()
        self.assertEqual(snapshot['entries'][0]['state'], 'INVALID')

    def test_v1_read_missing_entry_and_v2_migration_on_mutation(self):
        library = WebLibrary(self.root, self.bridge)
        workspace_id = str(uuid.uuid4())
        entry = {'workspace_id': workspace_id, 'title': 'SPECIMEN absent', 'idempotency_key': 'old-key', 'request_hash': 'a' * 64}
        library.registry_path.write_text(json.dumps({'contract': 'labfy.web_library.registry.v1', 'generation': 4, 'entries': [entry]}), encoding='utf-8')
        snapshot = library.snapshot()
        self.assertEqual(snapshot['entries'][0]['state'], 'MISSING')
        self.assertEqual(json.loads(library.registry_path.read_text())['contract'], 'labfy.web_library.registry.v1')
        created = library.create('SPECIMEN managed', 'new-key')
        self.assertEqual(created['generation'], 5)
        self.assertEqual(json.loads(library.registry_path.read_text())['contract'], REGISTRY_CONTRACT)

    def test_v2_registry_is_read_without_rewrite(self):
        library = WebLibrary(self.root, self.bridge)
        identifier = str(uuid.uuid4())
        entry = {
            'workspace_id': identifier,
            'title': 'SPECIMEN v2 absent',
            'storage_kind': 'EXISTING',
            'storage_ref': 'Existing-V2',
            'idempotency_key': 'v2-key',
            'request_hash': '2' * 64,
        }
        registry = {
            'contract': REGISTRY_CONTRACT,
            'generation': 7,
            'entries': [entry],
        }
        library.registry_path.write_text(json.dumps(registry), encoding='utf-8')
        before = library.registry_path.read_bytes()
        snapshot = library.snapshot()
        self.assertEqual(snapshot['generation'], 7)
        self.assertEqual(snapshot['entries'][0]['state'], 'MISSING')
        self.assertEqual(library.registry_path.read_bytes(), before)

    def test_duplicate_workspace_id_is_rejected(self):
        library = WebLibrary(self.root, self.bridge)
        identifier = str(uuid.uuid4())
        entries = [
            self._existing_entry(identifier, 'Existing-One', 'key-one'),
            self._existing_entry(identifier, 'Existing-Two', 'key-two'),
        ]
        self._write_registry(library, entries)
        with self.assertRaisesRegex(LibraryError, 'dupliquée'):
            library.snapshot()

    def test_duplicate_storage_ref_is_rejected(self):
        library = WebLibrary(self.root, self.bridge)
        entries = [
            self._existing_entry(str(uuid.uuid4()), 'Existing-Same', 'key-one'),
            self._existing_entry(str(uuid.uuid4()), 'Existing-Same', 'key-two'),
        ]
        self._write_registry(library, entries)
        with self.assertRaisesRegex(LibraryError, 'dupliquée'):
            library.snapshot()

    def test_malformed_and_symlink_registry_are_rejected(self):
        library = WebLibrary(self.root, self.bridge)
        library.registry_path.write_text('{malformed', encoding='utf-8')
        with self.assertRaisesRegex(LibraryError, 'illisible'):
            library.snapshot()
        library.registry_path.unlink()
        target = self.root / 'registry-target.json'
        target.write_text(json.dumps({'contract': REGISTRY_CONTRACT}), encoding='utf-8')
        library.registry_path.symlink_to(target)
        with self.assertRaisesRegex(LibraryError, 'invalide'):
            library.snapshot()

    def test_control_long_names_and_candidate_cap(self):
        control = self.root / 'control\nname'
        control.mkdir()
        long_name = self.root / ('L' * 201)
        long_name.mkdir()
        library = WebLibrary(self.root, self.bridge)
        candidates = library.discover_existing()['candidates']
        states = {item['display_name']: item['state'] for item in candidates}
        self.assertEqual(states['control\nname'], 'INVALID')
        self.assertIn('L' * 200, states)
        self.assertEqual(states['L' * 200], 'INVALID')

        for index in range(260):
            (self.root / f'cap-{index:03d}').mkdir()
        capped = library.discover_existing()['candidates']
        self.assertEqual(len(capped), 256)

    def test_crash_after_temp_write_keeps_registry_restartable(self):
        library = WebLibrary(self.root, self.bridge)
        library.registry_path.write_text(json.dumps({'contract': REGISTRY_CONTRACT, 'generation': 0, 'entries': []}), encoding='utf-8')
        before = library.registry_path.read_bytes()
        with mock.patch.object(Path, 'replace', side_effect=OSError('crash SPECIMEN')):
            with self.assertRaisesRegex(OSError, 'crash'):
                library.create('SPECIMEN crash', 'crash-key')
        self.assertEqual(library.registry_path.read_bytes(), before)
        self.assertEqual(WebLibrary(self.root, self.bridge).snapshot()['generation'], 0)

    @staticmethod
    def _existing_entry(identifier, storage_ref, key):
        return {
            'workspace_id': identifier,
            'title': f'SPECIMEN {storage_ref}',
            'storage_kind': 'EXISTING',
            'storage_ref': storage_ref,
            'idempotency_key': key,
            'request_hash': hashlib.sha256(key.encode()).hexdigest(),
        }

    @staticmethod
    def _write_registry(library, entries):
        value = {
            'contract': REGISTRY_CONTRACT,
            'generation': 1,
            'entries': entries,
        }
        library.registry_path.write_text(json.dumps(value), encoding='utf-8')

if __name__ == '__main__':
    unittest.main()
