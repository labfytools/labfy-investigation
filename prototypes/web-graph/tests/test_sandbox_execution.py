import os
import subprocess
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from sandbox_execution import ArtifactInput, SandboxContractError, SandboxExec, SandboxStatus
from tool_registry import (ExecutionProfile, NetworkRequirement, RiskClass, ToolDefinition,
                           ToolRegistry)


def registry(max_output=1024):
    definition = ToolDefinition(
        tool_id="specimen.reader.v1", display_name="SPECIMEN reader", binary="specimen-reader",
        risk_class=RiskClass.OFFLINE_READ_ONLY,
        input_contract={"type": "object"}, output_contract={"type": "object"},
        execution_profile=ExecutionProfile(2, max_output, NetworkRequirement.OFFLINE),
        execution_available=True, policy_allowed=True,
    )
    return ToolRegistry((definition,))


class RecordingRunner:
    def __init__(self, stdout=b"ok", stderr=b"", returncode=0):
        self.stdout = stdout
        self.stderr = stderr
        self.returncode = returncode
        self.calls = []

    def __call__(self, argv, **kwargs):
        self.calls.append((argv, kwargs))
        return subprocess.CompletedProcess(argv, self.returncode, self.stdout, self.stderr)


class SandboxExecutionTest(unittest.TestCase):
    def test_unknown_tool_and_missing_bwrap_never_fall_back(self):
        runner = RecordingRunner()
        sandbox = SandboxExec(registry(), runner=runner, which=lambda _name: None)
        self.assertEqual(sandbox.execute("unknown.tool", []).status, SandboxStatus.REJECTED)
        self.assertEqual(sandbox.execute("specimen.reader.v1", []).status,
                         SandboxStatus.UNAVAILABLE)
        self.assertEqual(runner.calls, [])

    def test_shell_string_is_one_inert_argv_and_environment_has_no_secret(self):
        runner = RecordingRunner()

        def which(name):
            return "/usr/bin/bwrap" if name == "bwrap" else "/usr/bin/specimen-reader"

        os.environ["LABFY_SPECIMEN_SECRET"] = "must-not-leak"
        try:
            sandbox = SandboxExec(registry(), runner=runner, which=which)
            result = sandbox.execute(
                "specimen.reader.v1", ["--label", "x; touch /tmp/NEVER", "artifact://sample"],
                artifacts=[ArtifactInput("sample", b"SPECIMEN")],
            )
        finally:
            os.environ.pop("LABFY_SPECIMEN_SECRET", None)
        self.assertEqual(result.status, SandboxStatus.SUCCESS)
        argv, kwargs = runner.calls[0]
        self.assertIn("x; touch /tmp/NEVER", argv)
        self.assertNotIn("shell", kwargs)
        self.assertNotIn("LABFY_SPECIMEN_SECRET", kwargs["env"])
        self.assertIn("--unshare-net", argv)
        self.assertNotIn(str(Path.home()), argv)

    def test_raw_paths_traversal_and_nul_are_rejected(self):
        sandbox = SandboxExec(
            registry(), runner=RecordingRunner(),
            which=lambda name: f"/usr/bin/{name}",
        )
        for argument in ("/etc/shadow", "../foreign-workspace/item", "bad\x00value", "~/secret"):
            with self.subTest(argument=argument), self.assertRaises(SandboxContractError):
                sandbox.execute("specimen.reader.v1", [argument])

    def test_combined_output_limit_is_enforced(self):
        runner = RecordingRunner(stdout=b"a" * 40, stderr=b"b" * 40)
        sandbox = SandboxExec(registry(max_output=64), runner=runner,
                              which=lambda name: f"/usr/bin/{name}")
        result = sandbox.execute("specimen.reader.v1", [])
        self.assertEqual(result.status, SandboxStatus.OUTPUT_LIMIT)
        self.assertEqual(len(result.stdout) + len(result.stderr), 64)
        self.assertEqual(result.provenance["observed_output_bytes"], 80)


if __name__ == "__main__":
    unittest.main()
