import json
import sqlite3
import subprocess
import tempfile
import unittest
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

REPOSITORY = Path(__file__).resolve().parents[3]
BRIDGE = REPOSITORY / "tools/local-jobs"

class JobStoreMigrationTest(unittest.TestCase):
    def make_workspace(self):
        temporary=tempfile.TemporaryDirectory(prefix="labfy-job-migration-")
        root=Path(temporary.name)
        result=subprocess.run([BRIDGE,"init-specimen","--workspace",root],cwd=REPOSITORY,text=True,capture_output=True)
        self.assertEqual(result.returncode,0,result.stderr)
        return temporary,root

    def test_concurrent_same_admission_is_single_plan(self):
        temporary,root=self.make_workspace()
        try:
            subprocess.run([BRIDGE,"export","--workspace",root],cwd=REPOSITORY,check=True)
            planner=json.loads((root/"planner-snapshot.json").read_text())
            ids=[item["id"] for item in planner["recommendations"]
                 if item.get("kind")=="ANALYSIS" and item.get("available")][:1]
            command=[BRIDGE,"submit-plan-json","--workspace",root,"--revision",
                     planner["input_revision"],"--profile","SPECIMEN_SMALL","--key",
                     "84000000-0000-4000-8000-000000000001","--recommendations",",".join(ids)]
            def invoke(): return subprocess.run(command,cwd=REPOSITORY,text=True,capture_output=True)
            with ThreadPoolExecutor(max_workers=2) as executor:
                results=list(executor.map(lambda _:invoke(),range(2)))
            self.assertEqual([item.returncode for item in results],[0,0],
                             [item.stderr for item in results])
            payloads=[json.loads(item.stdout) for item in results]
            self.assertEqual(payloads[0]["plan_id"],payloads[1]["plan_id"])
            db=sqlite3.connect(root/".labfy/runtime/jobs.sqlite")
            self.assertEqual(db.execute("SELECT count(*) FROM plans").fetchone()[0],1)
            self.assertEqual(db.execute("SELECT count(*) FROM jobs").fetchone()[0],1);db.close()
        finally: temporary.cleanup()

if __name__ == "__main__": unittest.main()
