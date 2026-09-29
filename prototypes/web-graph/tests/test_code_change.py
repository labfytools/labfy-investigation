import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
import uuid
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from code_change import CONTRACT, CodeChangeError, CodeChangeService, DevLimits


def git(repo, *args):
    return subprocess.run(("git", *args), cwd=repo, check=True, text=True,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE).stdout.strip()


class CodeChangeServiceTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="labfy-code-change-SPECIMEN-")
        root = Path(self.temporary.name)
        self.repo = root / "repo"
        self.state = root / "state"
        self.repo.mkdir()
        git(self.repo, "init", "-q")
        git(self.repo, "config", "user.email", "specimen@example.invalid")
        git(self.repo, "config", "user.name", "SPECIMEN Developer")
        (self.repo / "app.py").write_text("VALUE = 'before-SPECIMEN'\n", encoding="utf-8")
        (self.repo / "test_specimen.py").write_text(
            "import unittest\nimport app\n\n"
            "class SpecimenTest(unittest.TestCase):\n"
            "    def test_value(self):\n"
            "        self.assertEqual(app.VALUE, 'after-SPECIMEN')\n\n"
            "if __name__ == '__main__':\n    unittest.main()\n", encoding="utf-8")
        git(self.repo, "add", "app.py", "test_specimen.py")
        git(self.repo, "commit", "-qm", "SPECIMEN baseline")
        self.base_sha = git(self.repo, "rev-parse", "HEAD")
        self.recipes = {"SPECIMEN_TEST": ("python3", "test_specimen.py")}
        self.service = CodeChangeService(self.repo, self.state, test_recipes=self.recipes,
                                         limits=DevLimits(test_timeout_seconds=10))

    def tearDown(self):
        # Remove registered detached worktrees before TemporaryDirectory cleanup.
        subprocess.run(("git", "worktree", "prune"), cwd=self.repo,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
        self.temporary.cleanup()

    def proposal(self, change_id="change-SPECIMEN", expected_files=None):
        return {
            "contract": CONTRACT,
            "change_id": change_id,
            "workspace_context": "Contexte technique SPECIMEN sans donnée d'enquête",
            "purpose": "Ajouter le comportement synthétique dédié au test",
            "why_declarative_insufficient": (
                "Un panneau spécialisé et une logique backend sont requis; "
                "un manifest et l'adapter déclaratif ne peuvent pas les exprimer."
            ),
            "affected_areas": ["backend"],
            "expected_files": expected_files or ["app.py"],
            "required_tests": ["SPECIMEN_TEST"],
            "risk": "LOW_SPECIMEN",
            "created_at": "2026-09-28T10:00:00Z",
            "state": "PROPOSED",
        }

    @staticmethod
    def key():
        return str(uuid.uuid4())

    def prepare(self, change_id="change-SPECIMEN", expected_files=None):
        created = self.service.propose(self.proposal(change_id, expected_files),
                                       idempotency_key=self.key())
        self.assertEqual(created["state"], "WAITING_DEV_APPROVAL")
        prepared = self.service.approve_prepare(change_id, actor="human-SPECIMEN",
                                                idempotency_key=self.key())
        self.assertEqual(prepared["state"], "EDITING")
        return prepared

    def edit_app(self, change_id="change-SPECIMEN"):
        read = self.service.dev_read(change_id, "app.py")
        digest = hashlib.sha256(read["content"].encode()).hexdigest()
        return self.service.dev_edit(
            change_id, "app.py", expected_sha256=digest,
            replacements=[{"old": "before-SPECIMEN", "new": "after-SPECIMEN"}],
        )

    def ready(self, change_id="change-SPECIMEN"):
        self.edit_app(change_id)
        result = self.service.run_test(change_id, "SPECIMEN_TEST")
        self.assertTrue(result["passed"], result["output"])
        return self.service.ready_for_review(change_id, idempotency_key=self.key())

    def assert_error(self, code, callable_, *args, **kwargs):
        with self.assertRaises(CodeChangeError) as raised:
            callable_(*args, **kwargs)
        self.assertEqual(raised.exception.code, code)

    def test_strict_proposal_and_declarative_insufficiency(self):
        proposal = self.proposal()
        proposal["unexpected"] = True
        self.assert_error("INVALID_PROPOSAL", self.service.propose, proposal,
                          idempotency_key=self.key())
        proposal = self.proposal()
        proposal["why_declarative_insufficient"] = "N/A"
        result = self.service.propose(proposal, idempotency_key=self.key())
        self.assertEqual(result["classification"], "DECLARATIVE_SUFFICIENT")
        self.assertEqual(self.service.list_changes(), [])

    def test_default_backend_enforces_full_recipes_by_changed_surface(self):
        production = CodeChangeService(self.repo, self.state / "production-recipes")
        ui = self.proposal("ui-SPECIMEN", ["prototypes/web-graph/public/app.js"])
        ui["required_tests"] = ["NODE_CHECK", "FIREFOX_TARGETED"]
        created = production.propose(ui, idempotency_key=self.key())
        self.assertEqual(set(created["proposed_tests"]), {"NODE_CHECK", "FIREFOX_TARGETED"})
        self.assertTrue({"NODE_CHECK", "NODE_TEST", "WEB_FIXTURE_BUILD",
                         "FIREFOX_TARGETED", "FIREFOX_FULL",
                         "DIFF_CHECK"}.issubset(created["required_tests"]))

        python = self.proposal("python-SPECIMEN", ["prototypes/web-graph/service.py"])
        python["required_tests"] = ["PYTHON_TARGETED"]
        created = production.propose(python, idempotency_key=self.key())
        self.assertTrue({"PY_COMPILE", "PYTHON_TARGETED", "PYTHON_FULL", "DIFF_CHECK"}
                        .issubset(created["required_tests"]))

        c_change = self.proposal("c-SPECIMEN", ["src/specimen.c"])
        c_change["required_tests"] = ["C_BUILD"]
        created = production.propose(c_change, idempotency_key=self.key())
        self.assertTrue({"C_BUILD", "SOURCE_SIZE", "C_TEST", "DIFF_CHECK"}
                        .issubset(created["required_tests"]))

    def test_gate_c1_is_human_idempotent_and_persistent(self):
        self.service.propose(self.proposal(), idempotency_key=self.key())
        self.assert_error("MODEL_SELF_APPROVAL", self.service.approve_prepare,
                          "change-SPECIMEN", actor="qwen", idempotency_key=self.key())
        key = self.key()
        first = self.service.approve_prepare("change-SPECIMEN", actor="human-SPECIMEN",
                                             idempotency_key=key)
        replay = self.service.approve_prepare("change-SPECIMEN", actor="human-SPECIMEN",
                                              idempotency_key=key)
        self.assertEqual(first, replay)
        restarted = CodeChangeService(self.repo, self.state, test_recipes=self.recipes)
        self.assertEqual(restarted.get("change-SPECIMEN")["state"], "EDITING")
        self.assertEqual((self.repo / "app.py").read_text(), "VALUE = 'before-SPECIMEN'\n")

    def test_reject_flows_are_terminal_and_persist_decision(self):
        self.service.propose(self.proposal(), idempotency_key=self.key())
        rejected = self.service.reject_prepare(
            "change-SPECIMEN", actor="human-SPECIMEN", reason="Périmètre trop large",
            idempotency_key=self.key())
        self.assertEqual(rejected["state"], "DEV_REJECTED")
        self.assertEqual(rejected["decisions"][-1]["gate"], "C1")

        self.prepare("second-SPECIMEN")
        self.ready("second-SPECIMEN")
        rejected = self.service.reject_apply(
            "second-SPECIMEN", actor="human-SPECIMEN", reason="Revue non concluante",
            idempotency_key=self.key())
        self.assertEqual(rejected["state"], "APPLY_REJECTED")
        self.assertEqual(git(self.repo, "status", "--porcelain"), "")

    def test_confined_structured_read_search_edit_create_and_delete(self):
        self.prepare(expected_files=["app.py", "created.py"])
        found = self.service.dev_search("change-SPECIMEN", "before-SPECIMEN",
                                        paths=["app.py"])
        self.assertEqual(found["matches"][0]["line"], 1)
        repository_found = self.service.dev_search("change-SPECIMEN", "SPECIMEN",
                                                   paths=["."])
        self.assertGreaterEqual(len(repository_found["matches"]), 1)
        self.assert_error("PATH_DENIED", self.service.dev_read,
                          "change-SPECIMEN", "../outside.py")
        self.assert_error("PATH_DENIED", self.service.dev_create,
                          "change-SPECIMEN", ".git/config", "bad")
        self.assert_error("SCOPE_EXPANSION_REQUIRED", self.service.dev_create,
                          "change-SPECIMEN", "surprise.py", "SPECIMEN")
        created = self.service.dev_create("change-SPECIMEN", "created.py", "SPECIMEN = True\n")
        self.assertEqual(len(created["sha256"]), 64)
        deleted = self.service.dev_delete_owned("change-SPECIMEN", "created.py")
        self.assertTrue(deleted["deleted"])
        self.assert_error("DELETE_DENIED", self.service.dev_delete_owned,
                          "change-SPECIMEN", "app.py")

    def test_symlink_binary_and_edit_conflict_are_refused(self):
        self.prepare(expected_files=["app.py", "link.py"])
        worktree = self.state / "dev-worktrees" / "change-SPECIMEN"
        (worktree / "link.py").symlink_to("app.py")
        self.assert_error("SYMLINK_DENIED", self.service.dev_read,
                          "change-SPECIMEN", "link.py")
        self.assert_error("EDIT_CONFLICT", self.service.dev_edit,
                          "change-SPECIMEN", "app.py", expected_sha256="0" * 64,
                          replacements=[{"old": "before-SPECIMEN", "new": "after"}])

    def test_allowlisted_test_diff_and_secret_scan(self):
        self.prepare()
        self.edit_app()
        self.assert_error("UNKNOWN_TEST_RECIPE", self.service.run_test,
                          "change-SPECIMEN", "SHELL_ANYTHING")
        self.assert_error("TEST_REQUIREMENTS_UNMET", self.service.ready_for_review,
                          "change-SPECIMEN", idempotency_key=self.key())
        self.assertTrue(self.service.run_test("change-SPECIMEN", "SPECIMEN_TEST")["passed"])
        review = self.service.ready_for_review("change-SPECIMEN", idempotency_key=self.key())
        self.assertEqual(review["state"], "WAITING_APPLY_APPROVAL")
        diff = self.service.get_diff("change-SPECIMEN")
        self.assertIn("after-SPECIMEN", diff["patch"])
        self.assertEqual(diff["sha256"], review["patch_digest"])

        other = "secret-SPECIMEN"
        self.prepare(other, ["secret.py"])
        self.service.dev_create(other, "secret.py", "api_key = 'A' * 40\n")
        # Use a literal value so the simple scanner has evidence in the file.
        worktree = self.state / "dev-worktrees" / other
        (worktree / "secret.py").write_text("api_key='ABCDEFGHIJKLMNOPQRSTUVWXYZ123456'\n")
        self.assert_error("SECRET_SCAN_FAILED", self.service.get_diff, other)

    def test_edit_after_passed_test_invalidates_review_and_preview(self):
        self.prepare()
        self.edit_app()
        self.assertTrue(self.service.run_test("change-SPECIMEN", "SPECIMEN_TEST")["passed"])
        self.service.set_preview_metadata("change-SPECIMEN",
            {"url": "http://127.0.0.1:43123/preview", "notes": "SPECIMEN"},
            idempotency_key=self.key())
        read = self.service.dev_read("change-SPECIMEN", "app.py")
        self.service.dev_edit("change-SPECIMEN", "app.py", expected_sha256=read["sha256"],
            replacements=[{"old": "after-SPECIMEN", "new": "newer-SPECIMEN"}])
        record = self.service.get("change-SPECIMEN")
        self.assertEqual(record["tests"], [])
        self.assertEqual(record["preview"], {})
        self.assert_error("TEST_REQUIREMENTS_UNMET", self.service.ready_for_review,
                          "change-SPECIMEN", idempotency_key=self.key())

    def test_preview_accepts_only_loopback_metadata(self):
        self.prepare()
        self.assert_error("INVALID_PREVIEW", self.service.set_preview_metadata,
                          "change-SPECIMEN", {"url": "https://example.invalid"},
                          idempotency_key=self.key())
        value = self.service.set_preview_metadata(
            "change-SPECIMEN",
            {"url": "http://127.0.0.1:43123/preview", "screenshots": [],
             "notes": "SPECIMEN"}, idempotency_key=self.key())
        self.assertEqual(value["preview"]["notes"], "SPECIMEN")

    def test_stale_base_and_dirty_main_block_apply(self):
        self.prepare()
        self.ready()
        (self.repo / "unrelated.py").write_text("SPECIMEN = True\n")
        self.assert_error("DIRTY_MAIN", self.service.approve_apply,
                          "change-SPECIMEN", actor="human-SPECIMEN",
                          idempotency_key=self.key())
        (self.repo / "unrelated.py").unlink()

        # A distinct prepared change sees the new commit as a stale baseline.
        (self.repo / "other.py").write_text("SPECIMEN = True\n")
        git(self.repo, "add", "other.py")
        git(self.repo, "commit", "-qm", "SPECIMEN advance")
        self.assert_error("STALE_BASE", self.service.approve_apply,
                          "change-SPECIMEN", actor="human-SPECIMEN",
                          idempotency_key=self.key())

    def test_apply_and_rollback_in_temp_repo_without_commit(self):
        self.prepare()
        self.ready()
        applied = self.service.approve_apply(
            "change-SPECIMEN", actor="human-SPECIMEN", idempotency_key=self.key())
        self.assertEqual(applied["state"], "APPLIED_LOCAL")
        self.assertEqual(git(self.repo, "rev-parse", "HEAD"), self.base_sha)
        self.assertIn("after-SPECIMEN", (self.repo / "app.py").read_text())
        self.assertTrue(git(self.repo, "status", "--porcelain"))
        rolled = self.service.rollback(
            "change-SPECIMEN", actor="human-SPECIMEN", idempotency_key=self.key())
        self.assertEqual(rolled["state"], "ROLLED_BACK")
        self.assertEqual((self.repo / "app.py").read_text(), "VALUE = 'before-SPECIMEN'\n")
        self.assertEqual(git(self.repo, "status", "--porcelain"), "")

    def test_new_file_patch_is_applied_and_removed_by_rollback(self):
        self.prepare(expected_files=["app.py", "created.py"])
        self.edit_app()
        self.service.dev_create("change-SPECIMEN", "created.py", "SPECIMEN = True\n")
        self.assertTrue(self.service.run_test("change-SPECIMEN", "SPECIMEN_TEST")["passed"])
        ready = self.service.ready_for_review("change-SPECIMEN", idempotency_key=self.key())
        self.assertIn("created.py", ready["files_modified"])
        self.assertIn("created.py", self.service.get_diff("change-SPECIMEN")["stat"])
        self.service.approve_apply("change-SPECIMEN", actor="human-SPECIMEN",
                                   idempotency_key=self.key())
        self.assertTrue((self.repo / "created.py").exists())
        self.service.rollback("change-SPECIMEN", actor="human-SPECIMEN",
                              idempotency_key=self.key())
        self.assertFalse((self.repo / "created.py").exists())

    def test_crash_recovery_never_replays_apply(self):
        self.service.propose(self.proposal(), idempotency_key=self.key())
        path = self.state / "code-changes" / "change-SPECIMEN.json"
        record = json.loads(path.read_text())
        record["state"] = "APPLYING"
        path.write_text(json.dumps(record), encoding="utf-8")
        restarted = CodeChangeService(self.repo, self.state, test_recipes=self.recipes)
        recovered = restarted.get("change-SPECIMEN")
        self.assertEqual(recovered["state"], "FAILED")
        self.assertEqual(recovered["failure"], "APPLY_INTERRUPTED_REVIEW_REQUIRED")
        self.assertEqual(git(self.repo, "status", "--porcelain"), "")

    def test_crash_recovery_reverses_a_fully_applied_patch(self):
        self.prepare()
        self.ready()
        state_path = self.state / "code-changes" / "change-SPECIMEN.json"
        record = json.loads(state_path.read_text())
        patch = Path(record["patch_path"]).read_bytes()
        subprocess.run(("git", "apply", "--binary", "-"), cwd=self.repo, input=patch,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
        record["state"] = "APPLYING"
        state_path.write_text(json.dumps(record), encoding="utf-8")
        restarted = CodeChangeService(self.repo, self.state, test_recipes=self.recipes)
        recovered = restarted.get("change-SPECIMEN")
        self.assertEqual(recovered["state"], "ROLLED_BACK")
        self.assertEqual(recovered["failure"], "APPLY_INTERRUPTED_ROLLED_BACK")
        self.assertEqual((self.repo / "app.py").read_text(), "VALUE = 'before-SPECIMEN'\n")
        self.assertEqual(git(self.repo, "status", "--porcelain"), "")


if __name__ == "__main__":
    unittest.main()
