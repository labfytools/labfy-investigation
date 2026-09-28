import json
import subprocess
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from privacy_egress import (CurlSocksTransport, EgressStatus, PodmanTorRuntime,
                            PrivacyEgressSupervisor, TorRuntimeState,
                            TransportResponse)


class FakeCommandRunner:
    def __init__(self, owner_matches=True, rootless=True):
        self.calls = []
        self.owner_matches = owner_matches
        self.rootless = rootless
        self.owner = None

    def __call__(self, argv, timeout):
        command = list(argv)
        self.calls.append((command, timeout))
        if command[0].endswith("curl"):
            header_path = Path(command[command.index("--dump-header") + 1])
            body_path = Path(command[command.index("--output") + 1])
            header_path.write_text("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n",
                                   encoding="iso-8859-1")
            body_path.write_bytes(json.dumps({"IsTor": True, "IP": "not-retained"}).encode())
            return subprocess.CompletedProcess(command, 0, "200\n127.0.0.1", "")
        if command[1:3] == ["info", "--format"]:
            return subprocess.CompletedProcess(command, 0, "true\n" if self.rootless else "false\n", "")
        if command[1] == "run":
            label = command[command.index("--label") + 1]
            self.owner = label.split("=", 1)[1]
            return subprocess.CompletedProcess(command, 0, "container-id\n", "")
        if command[1] == "logs":
            return subprocess.CompletedProcess(command, 0, "Bootstrapped 100% (done)\n", "")
        if command[1] == "port":
            return subprocess.CompletedProcess(command, 0, "127.0.0.1:42137\n", "")
        if command[1] == "inspect" and command[-2].endswith("Rootless}}"):
            raise AssertionError("unexpected inspect")
        if command[1] == "inspect" and command[-2] == "{{.State.Running}}":
            return subprocess.CompletedProcess(command, 0, "true\n", "")
        if command[1] == "inspect":
            value = self.owner if self.owner_matches else "foreign-owner"
            return subprocess.CompletedProcess(command, 0, value + "\n", "")
        if command[1:3] == ["rm", "--force"]:
            return subprocess.CompletedProcess(command, 0, "", "")
        raise AssertionError(f"commande inattendue: {command}")


class OversizeTransport:
    def request(self, method, url, headers, timeout_seconds, max_body_bytes):
        return TransportResponse(200, {}, b"x" * max_body_bytes, "93.184.216.34")


class PrivacyTorRuntimeTest(unittest.TestCase):
    def test_owned_rootless_container_reaches_ready_and_cleanup_is_targeted(self):
        runner = FakeCommandRunner()
        runtime = PodmanTorRuntime(runner=runner, sleeper=lambda _delay: None)
        transport = runtime.start(bootstrap_timeout=2.0)
        self.assertIsInstance(transport, CurlSocksTransport)
        self.assertEqual(runtime.state, TorRuntimeState.READY)
        self.assertEqual(runtime.reason, "TOR_EGRESS_VERIFIED")

        run = next(call for call, _timeout in runner.calls if call[1] == "run")
        self.assertIn("127.0.0.1::9050", run)
        self.assertIn("--read-only", run)
        self.assertEqual(run[run.index("--cap-drop") + 1], "ALL")
        self.assertNotIn("--privileged", run)
        self.assertNotIn("--network", run)
        self.assertNotIn("--volume", run)
        self.assertNotIn("-v", run)
        tmpfs = [run[index + 1] for index, value in enumerate(run) if value == "--tmpfs"]
        self.assertEqual(tmpfs, ["/tmp:rw,noexec,nosuid,nodev,size=80m,mode=1777"])
        self.assertFalse(any("uid=" in value or "gid=" in value for value in tmpfs))
        self.assertTrue(any(item.startswith(PodmanTorRuntime.OWNER_LABEL + "=") for item in run))

        curl = next(call for call, _timeout in runner.calls if call[0].endswith("curl"))
        self.assertTrue(curl[curl.index("--proxy") + 1].startswith("socks5h://127.0.0.1:"))
        runtime.close()
        removes = [call for call, _timeout in runner.calls if call[1:3] == ["rm", "--force"]]
        self.assertEqual(removes, [["/usr/bin/podman", "rm", "--force", runtime.container_name]])
        self.assertEqual(runtime.state, TorRuntimeState.STOPPED)

    def test_supervisor_close_makes_privacy_egress_unavailable(self):
        runner = FakeCommandRunner()
        runtime = PodmanTorRuntime(runner=runner, sleeper=lambda _delay: None)
        supervisor = PrivacyEgressSupervisor.start_owned_tor(runtime, bootstrap_timeout=2.0)
        self.assertTrue(supervisor.is_available())
        supervisor.close()
        self.assertFalse(supervisor.is_available())
        result = supervisor.fetch("https://example.com/")
        self.assertEqual(result.status, EgressStatus.PRIVACY_EGRESS_UNAVAILABLE)
        self.assertFalse(result.provenance["direct_fallback"])

    def test_foreign_label_is_never_removed(self):
        runner = FakeCommandRunner(owner_matches=False)
        runtime = PodmanTorRuntime(runner=runner, sleeper=lambda _delay: None)
        runtime.start(bootstrap_timeout=2.0)
        runtime.close()
        self.assertFalse(any(call[1:3] == ["rm", "--force"] for call, _timeout in runner.calls))

    def test_tor_down_has_no_direct_fallback(self):
        runner = FakeCommandRunner(rootless=False)
        runtime = PodmanTorRuntime(runner=runner)
        with self.assertRaisesRegex(OSError, "rootless indisponible"):
            runtime.start(bootstrap_timeout=2.0)
        self.assertEqual(runtime.state, TorRuntimeState.FAILED)
        result = PrivacyEgressSupervisor().fetch("https://example.com/")
        self.assertEqual(result.status, EgressStatus.PRIVACY_EGRESS_UNAVAILABLE)
        self.assertFalse(result.provenance["direct_fallback"])
        self.assertFalse(any(call[0].endswith("curl") for call, _timeout in runner.calls))

    def test_ssrf_redirect_and_oversize_remain_rejected(self):
        supervisor = PrivacyEgressSupervisor(privacy_transport=OversizeTransport())
        blocked = supervisor.fetch("http://169.254.169.254/latest/meta-data/")
        oversized = supervisor.fetch("https://example.com/", max_body_bytes=8)
        self.assertEqual(blocked.status, EgressStatus.REJECTED)
        self.assertEqual(oversized.status, EgressStatus.RESPONSE_LIMIT)
        self.assertFalse(blocked.provenance["direct_fallback"])
        self.assertFalse(oversized.provenance["direct_fallback"])


if __name__ == "__main__":
    unittest.main()
