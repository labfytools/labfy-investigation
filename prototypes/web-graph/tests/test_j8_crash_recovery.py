import json
import sqlite3
import subprocess
import tempfile
import unittest
from pathlib import Path

REPOSITORY = Path(__file__).resolve().parents[3]
BRIDGE = REPOSITORY / "tools/local-jobs-test"
KEY = "87000000-0000-4000-8000-000000000001"


class J8CrashRecoveryTest(unittest.TestCase):
    def workspace(self):
        temporary = tempfile.TemporaryDirectory(prefix="labfy-j8-crash-")
        root = Path(temporary.name)
        subprocess.run([BRIDGE, "init-j7-specimen", "--workspace", root],
                       cwd=REPOSITORY, check=True, capture_output=True, text=True)
        subprocess.run([BRIDGE, "export", "--workspace", root],
                       cwd=REPOSITORY, check=True, capture_output=True, text=True)
        planner = json.loads((root / "planner-snapshot.json").read_text())
        recommendation = next(item for item in planner["recommendations"]
                              if item["kind"] == "ANALYSIS" and item["available"])
        return temporary, root, planner["input_revision"], recommendation["id"]

    @staticmethod
    def command(root, revision, recommendation, executable="submit-plan-json"):
        return [BRIDGE, executable, "--workspace", root, "--revision", revision,
                "--profile", "LOCAL_PRUDENT", "--key", KEY,
                "--recommendations", recommendation]

    @staticmethod
    def state(root):
        jobs = sqlite3.connect(root / ".labfy/runtime/jobs.sqlite")
        business = sqlite3.connect(root / "Enquete.sqlite")
        result = {
            "plans": jobs.execute("SELECT count(*) FROM plans").fetchone()[0],
            "jobs": jobs.execute("SELECT count(*) FROM jobs").fetchone()[0],
            "attempts": jobs.execute("SELECT count(*) FROM attempts").fetchone()[0],
            "attempt_budget": jobs.execute("SELECT consumed_attempts,consumed_active_ms FROM plans").fetchone(),
            "ids": jobs.execute("SELECT plan_id,job_id,derivative_evidence_id FROM plans JOIN plan_jobs USING(plan_id) JOIN jobs USING(job_id)").fetchall(),
            "extractions": business.execute("SELECT count(*) FROM extractions").fetchone()[0],
            "foreign_keys": jobs.execute("PRAGMA foreign_key_check").fetchall(),
            "integrity": jobs.execute("PRAGMA integrity_check").fetchone()[0],
        }
        jobs.close(); business.close()
        return result

    def invoke_crash(self, root, revision, recommendation, executable, expected,
                     marker=None):
        witness = subprocess.Popen(["sleep", "30"])
        try:
            result = subprocess.run(self.command(root, revision, recommendation,
                                    executable), cwd=REPOSITORY,
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, expected, result.stderr)
            if marker:
                self.assertIn(marker, result.stderr)
            self.assertIsNone(witness.poll(), "le processus témoin a été interrompu")
        finally:
            witness.terminate(); witness.wait(timeout=5)

    def test_crash_after_admission_before_claim(self):
        temporary, root, revision, recommendation = self.workspace()
        try:
            self.invoke_crash(root, revision, recommendation,
                              "__test-crash-after-admission", 85,
                              "J8_BARRIER_ADMISSION_COMMITTED")
            admitted = self.state(root)
            self.assertEqual((admitted["plans"], admitted["jobs"], admitted["attempts"]), (1, 1, 0))
            self.assertEqual(admitted["attempt_budget"], (0, 0))
            replay = subprocess.run(self.command(root, revision, recommendation),
                                    cwd=REPOSITORY, check=True, capture_output=True, text=True)
            self.assertTrue(json.loads(replay.stdout)["reused"])
            self.assertEqual(self.state(root)["ids"], admitted["ids"])
            subprocess.run([BRIDGE, "run", "--workspace", root], cwd=REPOSITORY, check=True)
            completed = self.state(root)
            subprocess.run([BRIDGE, "run", "--workspace", root], cwd=REPOSITORY, check=True)
            self.assertEqual(self.state(root), completed)
            self.assertEqual(completed["extractions"], 1)
        finally:
            temporary.cleanup()

    def test_crash_after_claim_before_publication(self):
        temporary, root, revision, recommendation = self.workspace()
        try:
            subprocess.run(self.command(root, revision, recommendation), cwd=REPOSITORY, check=True)
            self.invoke_crash(root, revision, recommendation,
                              "__test-crash-after-claim", 84,
                              "J8_BARRIER_CLAIM_COMMITTED")
            crashed = self.state(root)
            self.assertEqual(crashed["attempts"], 1)
            self.assertEqual(crashed["attempt_budget"], (1, 30000))
            self.assertEqual(crashed["extractions"], 0)
            subprocess.run([BRIDGE, "run", "--workspace", root], cwd=REPOSITORY, check=True)
            completed = self.state(root)
            self.assertEqual(completed["attempts"], 2)
            self.assertEqual(completed["extractions"], 1)
            self.assertEqual(completed["ids"], crashed["ids"])
            self.assertEqual(completed["integrity"], "ok")
            self.assertEqual(completed["foreign_keys"], [])
        finally:
            temporary.cleanup()

    def test_crash_after_publication_before_ack(self):
        temporary, root, revision, recommendation = self.workspace()
        try:
            subprocess.run(self.command(root, revision, recommendation), cwd=REPOSITORY, check=True)
            self.invoke_crash(root, revision, recommendation, "__test-crash", 86)
            published = self.state(root)
            self.assertEqual((published["attempts"], published["extractions"]), (1, 1))
            subprocess.run([BRIDGE, "run", "--workspace", root], cwd=REPOSITORY, check=True)
            reconciled = self.state(root)
            subprocess.run([BRIDGE, "run", "--workspace", root], cwd=REPOSITORY, check=True)
            self.assertEqual(self.state(root), reconciled)
            self.assertEqual(reconciled["attempts"], 1)
            self.assertEqual(reconciled["extractions"], 1)
            self.assertEqual(reconciled["attempt_budget"], published["attempt_budget"])
            self.assertEqual(reconciled["ids"], published["ids"])
        finally:
            temporary.cleanup()


if __name__ == "__main__":
    unittest.main()
