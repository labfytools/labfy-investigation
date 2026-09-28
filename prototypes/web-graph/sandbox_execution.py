"""Exécution offline dans une sandbox bwrap obligatoire, sans repli hôte."""

from __future__ import annotations

import os
import re
import shutil
import subprocess
import tempfile
import time
from dataclasses import dataclass
from enum import Enum
from pathlib import Path, PurePosixPath
from typing import Callable, Mapping, Sequence

from tool_registry import NetworkRequirement, ToolRegistry, ToolRegistryError


class SandboxStatus(str, Enum):
    SUCCESS = "SUCCESS"
    FAILED = "FAILED"
    TIMEOUT = "TIMEOUT"
    OUTPUT_LIMIT = "OUTPUT_LIMIT"
    UNAVAILABLE = "UNAVAILABLE"
    REJECTED = "REJECTED"


@dataclass(frozen=True)
class ArtifactInput:
    artifact_id: str
    content: bytes


@dataclass(frozen=True)
class SandboxResult:
    status: SandboxStatus
    returncode: int | None
    stdout: bytes
    stderr: bytes
    outputs: Mapping[str, bytes]
    provenance: Mapping[str, object]


class SandboxContractError(ValueError):
    pass


class SandboxExec:
    """Construit exclusivement des appels bwrap structurés et offline."""

    def __init__(self, registry: ToolRegistry, runner: Callable[..., object] = subprocess.run,
                 which: Callable[[str], str | None] = shutil.which,
                 bwrap_binary: str = "bwrap"):
        self._registry = registry
        self._runner = runner
        self._which = which
        self._bwrap_binary = bwrap_binary

    @staticmethod
    def _valid_name(value: str) -> bool:
        return bool(re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_.-]{0,79}", value))

    def _materialize_argument(self, argument: str, artifact_ids: set[str],
                              output_names: set[str]) -> str:
        if not isinstance(argument, str) or "\x00" in argument:
            raise SandboxContractError("argument invalide")
        if argument.startswith("artifact://"):
            artifact_id = argument.removeprefix("artifact://")
            if artifact_id not in artifact_ids:
                raise SandboxContractError("artefact opaque inconnu")
            return f"/input/{artifact_id}"
        if argument.startswith("output://"):
            output_name = argument.removeprefix("output://")
            if output_name not in output_names:
                raise SandboxContractError("sortie déclarée inconnue")
            return f"/output/{output_name}"
        path = PurePosixPath(argument)
        # CONTRACT: un appelant ne peut jamais injecter un chemin hôte brut.
        if path.is_absolute() or ".." in path.parts or argument.startswith("~"):
            raise SandboxContractError("chemin brut ou traversée refusé")
        return argument

    def execute(self, tool_id: str, arguments: Sequence[str],
                artifacts: Sequence[ArtifactInput] = (),
                output_names: Sequence[str] = ()) -> SandboxResult:
        started = time.monotonic()
        try:
            definition = self._registry.executable_definition(tool_id)
        except ToolRegistryError as error:
            return self._result(SandboxStatus.REJECTED, tool_id, started, reason=str(error))
        if definition.execution_profile.network != NetworkRequirement.OFFLINE:
            return self._result(
                SandboxStatus.REJECTED, tool_id, started, reason="NETWORK_PROFILE_REJECTED"
            )
        bwrap = self._which(self._bwrap_binary)
        executable = self._which(definition.binary)
        if bwrap is None or executable is None:
            return self._result(
                SandboxStatus.UNAVAILABLE, tool_id, started, reason="ISOLATION_UNAVAILABLE"
            )

        artifact_map: dict[str, bytes] = {}
        for artifact in artifacts:
            if not self._valid_name(artifact.artifact_id) or not isinstance(artifact.content, bytes):
                raise SandboxContractError("artefact invalide")
            if artifact.artifact_id in artifact_map:
                raise SandboxContractError("artefact dupliqué")
            artifact_map[artifact.artifact_id] = artifact.content
        output_set = set(output_names)
        if len(output_set) != len(output_names) or any(
                not self._valid_name(name) for name in output_names):
            raise SandboxContractError("sorties invalides")
        materialized = [
            self._materialize_argument(value, set(artifact_map), output_set) for value in arguments
        ]

        with tempfile.TemporaryDirectory(prefix="labfy-sandbox-") as directory:
            root = Path(directory)
            input_directory = root / "input"
            output_directory = root / "output"
            input_directory.mkdir(mode=0o700)
            output_directory.mkdir(mode=0o700)
            for artifact_id, content in artifact_map.items():
                artifact_path = input_directory / artifact_id
                artifact_path.write_bytes(content)
                artifact_path.chmod(0o400)

            argv = self._bwrap_argv(
                bwrap, executable, input_directory, output_directory, materialized
            )
            try:
                completed = self._runner(
                    argv,
                    check=False,
                    capture_output=True,
                    timeout=definition.execution_profile.timeout_seconds,
                    env={"PATH": "/usr/bin:/bin", "LC_ALL": "C", "LANG": "C"},
                )
            except subprocess.TimeoutExpired as error:
                stdout = self._as_bytes(error.stdout)
                stderr = self._as_bytes(error.stderr)
                return self._bounded_result(
                    SandboxStatus.TIMEOUT, tool_id, started, None, stdout, stderr, {},
                    definition.execution_profile.max_output_bytes, "TIMEOUT",
                )
            except OSError:
                return self._result(
                    SandboxStatus.UNAVAILABLE, tool_id, started, reason="ISOLATION_START_FAILED"
                )

            stdout = self._as_bytes(getattr(completed, "stdout", b""))
            stderr = self._as_bytes(getattr(completed, "stderr", b""))
            outputs = self._read_outputs(output_directory, output_set,
                                         definition.execution_profile.max_output_bytes)
            returncode = int(getattr(completed, "returncode", 1))
            status = SandboxStatus.SUCCESS if returncode == 0 else SandboxStatus.FAILED
            return self._bounded_result(
                status, tool_id, started, returncode, stdout, stderr, outputs,
                definition.execution_profile.max_output_bytes, "COMPLETED",
            )

    @staticmethod
    def _bwrap_argv(bwrap: str, executable: str, input_directory: Path,
                    output_directory: Path, arguments: Sequence[str]) -> list[str]:
        argv = [
            bwrap, "--die-with-parent", "--new-session", "--unshare-all", "--unshare-net",
            "--clearenv", "--setenv", "PATH", "/usr/bin:/bin", "--setenv", "LC_ALL", "C",
            "--proc", "/proc", "--dev", "/dev", "--tmpfs", "/tmp",
        ]
        # WHY: les bibliothèques du binaire sont visibles en lecture seule, sans exposer le home.
        for system_path in ("/usr", "/bin", "/lib", "/lib64"):
            if Path(system_path).exists():
                argv.extend(("--ro-bind", system_path, system_path))
        argv.extend((
            "--ro-bind", str(input_directory), "/input",
            "--bind", str(output_directory), "/output",
            "--chdir", "/output", "--", executable, *arguments,
        ))
        return argv

    @staticmethod
    def _as_bytes(value) -> bytes:
        if value is None:
            return b""
        return value if isinstance(value, bytes) else str(value).encode("utf-8", "replace")

    @staticmethod
    def _read_outputs(directory: Path, names: set[str], max_bytes: int) -> dict[str, bytes]:
        outputs = {}
        for name in sorted(names):
            path = directory / name
            if path.is_file() and not path.is_symlink():
                with path.open("rb") as stream:
                    outputs[name] = stream.read(max_bytes + 1)
        return outputs

    def _bounded_result(self, status: SandboxStatus, tool_id: str, started: float,
                        returncode: int | None, stdout: bytes, stderr: bytes,
                        outputs: Mapping[str, bytes], limit: int, reason: str) -> SandboxResult:
        total = len(stdout) + len(stderr) + sum(len(value) for value in outputs.values())
        if total > limit:
            status = SandboxStatus.OUTPUT_LIMIT
            reason = "OUTPUT_LIMIT"
        remaining = limit
        bounded_stdout = stdout[:remaining]
        remaining -= len(bounded_stdout)
        bounded_stderr = stderr[:remaining]
        remaining -= len(bounded_stderr)
        bounded_outputs = {}
        for name, content in outputs.items():
            bounded_outputs[name] = content[:remaining]
            remaining -= len(bounded_outputs[name])
        return SandboxResult(
            status, returncode, bounded_stdout, bounded_stderr, bounded_outputs,
            self._provenance(tool_id, started, reason, total),
        )

    def _result(self, status: SandboxStatus, tool_id: str, started: float,
                reason: str) -> SandboxResult:
        return SandboxResult(
            status, None, b"", b"", {}, self._provenance(tool_id, started, reason, 0)
        )

    @staticmethod
    def _provenance(tool_id: str, started: float, reason: str,
                    observed_output_bytes: int) -> dict[str, object]:
        return {
            "contract": "labfy.sandbox_execution.v1",
            "tool_id": tool_id,
            "isolation": "bwrap",
            "network": "OFFLINE",
            "reason": reason,
            "duration_ms": max(0, round((time.monotonic() - started) * 1000)),
            "observed_output_bytes": observed_output_bytes,
        }
