import copy
import json
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path
import sys

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from report_bundle import publish, verify, ReportError


def document(marker="INCLUS-SPECIMEN"):
    return {"contract":"labfy.investigation_report.v1","rule_version":"labfy.report_selection.v1",
      "investigation_id":"90000000-0000-4000-8000-000000000001","revision":"a"*64,
      "generated_at":"2026-09-26T22:00:00Z","title":"Rapport éèàçù œ € → Łukasz SPECIMEN","human_comment":marker+" Commentaire synthétique éè. Apostrophe ’ tiret — ()\\<>",
      "profile":"MINIMAL","sections":{"evidence":True,"timeline":True,"infrastructure":True},"objects":[{"id":"evidence:90000000-0000-4000-8000-000000000010","type":"email","label":"Preuve <inerte>","state":"active","value_raw":"élève@example.test","value_normalized":"élève@example.test","value_corrected":None,"provenance_kind":"source","tool_id":"outil","tool_version":"1.0","missing_provenance_reason":None,"review_state":None,"selection_role":"selected"}],
      "network":[],"timeline":[{"raw_value":"2026-09-26T22:00:00Z","category":"processing_started","object_id":"evidence:90000000-0000-4000-8000-000000000010","timezone":"UTC","precision":"second"}],
      "limitations":["Aucune authenticité déduite."]}


class BundleTest(unittest.TestCase):
    def test_publish_verify_retry_and_minimization(self):
        with tempfile.TemporaryDirectory(prefix="labfy-j9-bundle-") as name:
            root=Path(name);report_id,reused=publish(document(),root,"intention-1")
            self.assertFalse(reused);bundle=root/"exports/reports"/report_id
            self.assertTrue(verify(bundle)["valid"])
            self.assertEqual(publish(document(),root,"intention-1"),(report_id,True))
            for path in bundle.iterdir():
                self.assertNotIn("EXCLU-UNIQUE-SPECIMEN",path.read_bytes().decode("latin-1","ignore"))
            text=subprocess.run(["pdftotext",bundle/"report.pdf","-"],check=True,capture_output=True,text=True).stdout
            flattened=" ".join(text.split())
            self.assertIn("Rapport éèàçù œ € → Łukasz",flattened);self.assertIn("Commentaire synthétique éè",flattened);self.assertIn("INCLUS-SPECIMEN",flattened)
            html=(bundle/"report.html").read_text();self.assertIn("&lt;inerte&gt;",html);self.assertNotIn("<script",html.lower())

    def test_faults_preserve_complete_bundle(self):
        with tempfile.TemporaryDirectory(prefix="labfy-j9-fault-") as name:
            root=Path(name);report_id,_=publish(document(),root,"stable")
            for point in ("staging","pdf"):
                with self.assertRaises(ReportError):publish(document(point),root,"fault-"+point,fail_at=point)
                self.assertTrue(verify(root/"exports/reports"/report_id)["valid"])
            with self.assertRaises(ReportError):publish(document("published"),root,"lost-response",fail_at="published")
            recovered,_=publish(document("published"),root,"lost-response")
            self.assertTrue(verify(root/"exports/reports"/recovered)["valid"])

    def test_process_exit_after_publication_is_reconciled(self):
        with tempfile.TemporaryDirectory(prefix="labfy-j9-process-") as name:
            root=Path(name);payload=root/"document.json";payload.write_text(json.dumps(document("process")))
            script=("import json,sys;from pathlib import Path;"
                    "from report_bundle import publish;"
                    "publish(json.loads(Path(sys.argv[2]).read_text()),Path(sys.argv[1]),'process-intent',fail_at='published')")
            result=subprocess.run([sys.executable,"-c",script,str(root),str(payload)],
                cwd=Path(__file__).resolve().parents[1],capture_output=True,text=True)
            self.assertNotEqual(result.returncode,0)
            report_id,reused=publish(document("process"),root,"process-intent")
            self.assertTrue(reused);self.assertTrue(verify(root/"exports/reports"/report_id)["valid"])

    def test_verifier_rejects_tamper_missing_unknown_link_and_extra(self):
        with tempfile.TemporaryDirectory(prefix="labfy-j9-verify-") as name:
            root=Path(name);report_id,_=publish(document(),root,"verify");bundle=root/"exports/reports"/report_id
            original=(bundle/"report.html").read_bytes();(bundle/"report.html").write_bytes(original+b"x")
            self.assertFalse(verify(bundle)["valid"]);(bundle/"report.html").write_bytes(original)
            manifest=json.loads((bundle/"manifest.json").read_text());manifest["contract"]="unknown";(bundle/"manifest.json").write_text(json.dumps(manifest))
            self.assertIn("unknown_manifest_contract",verify(bundle)["errors"])
            cases=root/"cases";shutil.copytree(bundle,cases)
            (cases/"report.pdf").unlink();self.assertTrue(any(x.startswith("missing") for x in verify(cases)["errors"]))
            shutil.rmtree(cases);shutil.copytree(bundle,cases);(cases/"extra.bin").write_bytes(b"x")
            self.assertTrue(any(x.startswith("unexpected") for x in verify(cases)["errors"]))
            shutil.rmtree(cases);shutil.copytree(bundle,cases);(cases/"report.html").unlink();os.symlink("NOTICE.txt",cases/"report.html")
            self.assertTrue(any(x.startswith("symlink") or x.startswith("missing_or_link") for x in verify(cases)["errors"]))

    def test_verifier_rejects_hostile_shapes_duplicates_and_root_link(self):
        with tempfile.TemporaryDirectory(prefix="labfy-j9-hostile-") as name:
            root=Path(name);report_id,_=publish(document(),root,"shape");bundle=root/"exports/reports"/report_id
            link=root/"bundle-link";os.symlink(bundle,link)
            self.assertIn("bundle_symlink",verify(link)["errors"])
            (bundle/"manifest.json").write_text("[]")
            self.assertIn("manifest_root_type",verify(bundle)["errors"])
            report_id,_=publish(document(),root,"duplicate");bundle=root/"exports/reports"/report_id
            manifest=json.loads((bundle/"manifest.json").read_text());manifest["files"].append(copy.deepcopy(manifest["files"][0]));(bundle/"manifest.json").write_text(json.dumps(manifest))
            self.assertIn("duplicate_manifest_path",verify(bundle)["errors"])

    def test_conflicting_intention_and_page_limit(self):
        with tempfile.TemporaryDirectory(prefix="labfy-j9-limits-") as name:
            root=Path(name);publish(document(),root,"same")
            changed=document("changed")
            with self.assertRaises(ReportError):publish(changed,root,"same")
            huge=document();huge["objects"]*=1200
            with self.assertRaises(ReportError):publish(huge,root,"huge")


if __name__=="__main__":unittest.main()
