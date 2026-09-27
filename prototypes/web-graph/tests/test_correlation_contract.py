import json,sqlite3,subprocess,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3];BRIDGE=ROOT/"tools/local-jobs"
class CorrelationContractTest(unittest.TestCase):
 def test_revision_content_and_references(self):
  with tempfile.TemporaryDirectory(prefix="labfy-j7-contract-") as name:
   root=Path(name);self.assertEqual(subprocess.run([BRIDGE,"init-j7-specimen","--workspace",root],cwd=ROOT).returncode,0)
   subprocess.run([BRIDGE,"export","--workspace",root],cwd=ROOT,check=True)
   planner=json.loads((root/"planner-snapshot.json").read_text());ids=[x["id"] for x in planner["recommendations"] if x.get("available") and x.get("kind")=="ANALYSIS"]
   subprocess.run([BRIDGE,"submit-plan-json","--workspace",root,"--revision",planner["input_revision"],"--profile","LOCAL_PRUDENT","--key","85000000-0000-4000-8000-000000000001","--recommendations",",".join(ids)],cwd=ROOT,check=True)
   subprocess.run([BRIDGE,"run","--workspace",root],cwd=ROOT,check=True);subprocess.run([BRIDGE,"export","--workspace",root],cwd=ROOT,check=True)
   first=json.loads((root/"correlation-snapshot.json").read_text());self.assertTrue(first["complete"])
   groups={g["id"] for g in first["groups"]}
   self.assertTrue(all(c["source"] in groups and c["target"] in groups for c in first["connections"]))
   db=sqlite3.connect(root/"Enquete.sqlite");row=db.execute("SELECT id,verification_status FROM evidence_entity_observations ORDER BY id LIMIT 1").fetchone();new="confirmed" if row[1]!="confirmed" else "proposed";db.execute("UPDATE evidence_entity_observations SET verification_status=? WHERE id=?",(new,row[0]));db.commit();db.close()
   subprocess.run([BRIDGE,"export","--workspace",root],cwd=ROOT,check=True);second=json.loads((root/"correlation-snapshot.json").read_text());self.assertNotEqual(first["revision"],second["revision"])
   db=sqlite3.connect(root/"Enquete.sqlite");db.execute("UPDATE evidence_entity_observations SET source_header=coalesce(source_header,'')||'-changed' WHERE id=?",(row[0],));db.commit();db.close()
   subprocess.run([BRIDGE,"export","--workspace",root],cwd=ROOT,check=True);third=json.loads((root/"correlation-snapshot.json").read_text());self.assertNotEqual(second["revision"],third["revision"])
if __name__=="__main__":unittest.main()
