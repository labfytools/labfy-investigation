import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
import uuid
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from agent_model_protocol import CONTRACT as ACTION_CONTRACT
from code_change import CodeChangeService, DevLimits
from developer_agent import (DEVELOPER_CONTEXT_CONTRACT, DEV_TOOL_IDS,
                             PROPOSAL_OUTPUT_CONTRACT, DeveloperAgent,
                             DeveloperAgentError)


def git(repo, *args):
    return subprocess.run(("git", *args), cwd=repo, check=True, text=True,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE).stdout.strip()


def tool(tool_id, arguments):
    return json.dumps({"contract": ACTION_CONTRACT, "kind": "tool_call",
                       "tool_id": tool_id, "arguments": arguments})


class ScriptedLocalModelClient:
    def __init__(self, responses):
        self.responses = list(responses)
        self.calls = []

    def complete(self, messages):
        self.calls.append(json.loads(json.dumps(messages)))
        if not self.responses:
            raise AssertionError("unexpected model inference")
        return self.responses.pop(0)


class DeveloperAgentTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="labfy-developer-agent-SPECIMEN-")
        root = Path(self.temporary.name)
        self.repo = root / "repo"
        self.state = root / "state"
        self.repo.mkdir()
        git(self.repo, "init", "-q")
        git(self.repo, "config", "user.email", "developer@example.invalid")
        git(self.repo, "config", "user.name", "Developer SPECIMEN")
        (self.repo / "app.py").write_text("VALUE = 'before-SPECIMEN'\n", encoding="utf-8")
        (self.repo / "test_specimen.py").write_text(
            "import unittest\nimport app\n\nclass T(unittest.TestCase):\n"
            "    def test_value(self):\n"
            "        self.assertEqual(app.VALUE, 'after-SPECIMEN')\n\n"
            "if __name__ == '__main__':\n    unittest.main()\n", encoding="utf-8")
        git(self.repo, "add", "app.py", "test_specimen.py")
        git(self.repo, "commit", "-qm", "SPECIMEN baseline")
        self.service = CodeChangeService(
            self.repo, self.state,
            test_recipes={"SPECIMEN_TEST": ("python3", "test_specimen.py")})

    def tearDown(self):
        subprocess.run(("git", "worktree", "prune"), cwd=self.repo,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
        self.temporary.cleanup()

    @staticmethod
    def key():
        return str(uuid.uuid4())

    def context(self):
        return {
            "contract": DEVELOPER_CONTEXT_CONTRACT,
            "change_id": "developer-SPECIMEN",
            "purpose": "Modifier une valeur synthétique avec un workflow spécialisé",
            "tool_docs": {"summary": "Documentation technique SPECIMEN"},
            "integration_proposal": {"level": "CODE_CHANGE", "source": "SPECIMEN"},
            "architecture_refs": ["docs/architecture/AGENT_TOOL_PROTOCOL.md"],
            "expected_files": ["app.py"],
            "required_tests": ["SPECIMEN_TEST"],
            "affected_areas": ["backend"],
            "risk": "LOW_SPECIMEN",
            "created_at": "2026-09-28T10:00:00Z",
        }

    def proposal_response(self):
        return json.dumps({
            "contract": PROPOSAL_OUTPUT_CONTRACT,
            "purpose": "Modifier une valeur synthétique avec un workflow spécialisé",
            "why_declarative_insufficient": (
                "La logique spécialisée modifie le backend; ni un manifest ni un adapter "
                "déclaratif ne peuvent implémenter ce comportement."
            ),
            "affected_areas": ["backend"],
            "expected_files": ["app.py"],
            "required_tests": ["SPECIMEN_TEST"],
            "risk": "LOW_SPECIMEN",
        })

    def proposed(self, client=None):
        client = client or ScriptedLocalModelClient([self.proposal_response()])
        agent = DeveloperAgent(client, self.service)
        proposal = agent.propose(self.context(), idempotency_key=self.key())
        self.assertEqual(proposal["state"], "WAITING_DEV_APPROVAL")
        return agent, client

    def test_proposal_context_is_technical_and_backend_owns_workspace_context(self):
        agent, client = self.proposed()
        sent = json.loads(client.calls[0][1]["content"])
        self.assertNotIn("workspace_context", sent)
        self.assertNotIn("investigation", sent)
        stored = self.service.get("developer-SPECIMEN")
        self.assertEqual(stored["workspace_context"],
                         "Developer Agent: contexte technique isolé, sans donnée d'enquête")
        self.assertFalse(any("approve" in tool_id or "shell" in tool_id
                             for tool_id in DEV_TOOL_IDS))

    def test_c1_pauses_without_another_model_inference(self):
        agent, client = self.proposed()
        before = len(client.calls)
        paused = agent.continue_development("developer-SPECIMEN")
        self.assertEqual(paused["state"], "PAUSED_C1")
        self.assertEqual(paused["reason"], "CODE_CHANGE_APPROVAL_REQUIRED")
        self.assertEqual(len(client.calls), before)

    def test_resumes_after_c1_reads_edits_tests_and_pauses_at_c2(self):
        proposal_client = ScriptedLocalModelClient([self.proposal_response()])
        agent = DeveloperAgent(proposal_client, self.service)
        agent.propose(self.context(), idempotency_key=self.key())
        self.service.approve_prepare("developer-SPECIMEN", actor="human-SPECIMEN",
                                     idempotency_key=self.key())
        original = "VALUE = 'before-SPECIMEN'\n"
        digest = hashlib.sha256(original.encode()).hexdigest()
        development_client = ScriptedLocalModelClient([
            tool("dev.repo.read", {"path": "app.py"}),
            tool("dev.repo.edit", {
                "path": "app.py", "expected_sha256": digest,
                "replacements": [{"old": "before-SPECIMEN", "new": "after-SPECIMEN"}],
            }),
            tool("dev.test.run", {"recipe_id": "SPECIMEN_TEST"}),
            tool("dev.diff.get", {}),
            tool("dev.change.ready", {"idempotency_key": self.key()}),
        ])
        agent = DeveloperAgent(development_client, self.service)
        result = agent.continue_development("developer-SPECIMEN")
        self.assertEqual(result["state"], "PAUSED_C2")
        self.assertEqual(result["reason"], "CODE_CHANGE_APPLY_REQUIRED")
        stored = self.service.get("developer-SPECIMEN")
        self.assertEqual(stored["state"], "WAITING_APPLY_APPROVAL")
        self.assertEqual(stored["tests"][-1]["recipe_id"], "SPECIMEN_TEST")
        self.assertEqual(stored["model_calls"], 5)
        self.assertEqual((self.repo / "app.py").read_text(), original)
        worktree = self.state / "dev-worktrees" / "developer-SPECIMEN" / "app.py"
        self.assertIn("after-SPECIMEN", worktree.read_text())
        # Tool responses are returned as untrusted JSON messages, not executed prompts.
        self.assertEqual(len(development_client.calls), 5)

    def test_unknown_shell_tool_is_rejected_by_protocol(self):
        self.proposed()
        self.service.approve_prepare("developer-SPECIMEN", actor="human-SPECIMEN",
                                     idempotency_key=self.key())
        client = ScriptedLocalModelClient([tool("shell.exec", {"command": "id"})])
        agent = DeveloperAgent(client, self.service)
        with self.assertRaises(DeveloperAgentError) as raised:
            agent.continue_development("developer-SPECIMEN")
        self.assertEqual(raised.exception.code, "INVALID_MODEL_ACTION")

    def test_passed_tests_require_diff_before_more_repository_reads(self):
        self.proposed()
        self.service.approve_prepare("developer-SPECIMEN", actor="human-SPECIMEN",
                                     idempotency_key=self.key())
        source = "VALUE = 'before-SPECIMEN'\n"
        client = ScriptedLocalModelClient([
            tool("dev.repo.read", {"path": "app.py"}),
            tool("dev.repo.edit", {"path": "app.py",
                "expected_sha256": hashlib.sha256(source.encode()).hexdigest(),
                "replacements": [{"old": "before-SPECIMEN", "new": "after-SPECIMEN"}]}),
            tool("dev.test.run", {"recipe_id": "SPECIMEN_TEST"}),
            tool("dev.repo.read", {"path": "app.py"}),
            tool("dev.diff.get", {}),
            tool("dev.change.ready", {"idempotency_key": self.key()}),
        ])
        agent = DeveloperAgent(client, self.service)
        self.assertEqual(agent.continue_development("developer-SPECIMEN")["state"],
                         "PAUSED_C2")
        self.assertEqual(self.service.get("developer-SPECIMEN")["state"],
                         "WAITING_APPLY_APPROVAL")

    def test_model_cannot_expand_approved_proposal_scope(self):
        output = json.loads(self.proposal_response())
        output["expected_files"].append("surprise.py")
        agent = DeveloperAgent(ScriptedLocalModelClient([
            json.dumps(output), json.dumps(output)]), self.service)
        with self.assertRaises(DeveloperAgentError) as raised:
            agent.propose(self.context(), idempotency_key=self.key())
        self.assertEqual(raised.exception.code, "SCOPE_EXPANSION_REQUIRED")
        self.assertEqual(self.service.list_changes(), [])

    def test_model_call_budget_survives_agent_restart(self):
        limited = CodeChangeService(
            self.repo, self.state,
            test_recipes={"SPECIMEN_TEST": ("python3", "test_specimen.py")},
            limits=DevLimits(max_model_calls=1))
        proposal_client = ScriptedLocalModelClient([self.proposal_response()])
        DeveloperAgent(proposal_client, limited).propose(self.context(), idempotency_key=self.key())
        limited.approve_prepare("developer-SPECIMEN", actor="human-SPECIMEN",
                                idempotency_key=self.key())
        first = DeveloperAgent(ScriptedLocalModelClient([
            tool("dev.repo.read", {"path": "app.py"}),
            json.dumps({"contract": ACTION_CONTRACT, "kind": "final", "text": "pause"}),
        ]), limited, max_model_calls=1)
        with self.assertRaises(DeveloperAgentError) as exhausted:
            # The first call succeeds; the per-run budget ends immediately afterwards.
            first.continue_development("developer-SPECIMEN")
        self.assertEqual(exhausted.exception.code, "MODEL_CALL_BUDGET_EXCEEDED")
        restarted = DeveloperAgent(ScriptedLocalModelClient([
            json.dumps({"contract": ACTION_CONTRACT, "kind": "final", "text": "retry"})
        ]), CodeChangeService(
            self.repo, self.state,
            test_recipes={"SPECIMEN_TEST": ("python3", "test_specimen.py")},
            limits=DevLimits(max_model_calls=1)))
        with self.assertRaises(DeveloperAgentError) as persisted:
            restarted.continue_development("developer-SPECIMEN")
        self.assertEqual(persisted.exception.code, "DEV_BUDGET_EXCEEDED")


if __name__ == "__main__":
    unittest.main()
