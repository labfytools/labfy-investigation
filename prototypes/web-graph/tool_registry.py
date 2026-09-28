"""Registre opérationnel explicite des outils et documentation locale bornée."""

from __future__ import annotations

import hashlib
import os
import re
import shutil
import subprocess
from dataclasses import dataclass, field, replace
from enum import Enum
from pathlib import Path
from types import MappingProxyType
from typing import Callable, Mapping, Sequence


class RiskClass(str, Enum):
    OFFLINE_READ_ONLY = "OFFLINE_READ_ONLY"
    PASSIVE_PUBLIC = "PASSIVE_PUBLIC"
    ACTIVE_PROBE = "ACTIVE_PROBE"
    LOCAL_SENSITIVE = "LOCAL_SENSITIVE"
    INTRUSIVE_LAB_ONLY = "INTRUSIVE_LAB_ONLY"
    DENIED = "DENIED"


class NetworkRequirement(str, Enum):
    OFFLINE = "OFFLINE"
    PRIVACY_EGRESS = "PRIVACY_EGRESS"
    DIRECT = "DIRECT"


@dataclass(frozen=True)
class ExecutionProfile:
    """Limites que l'exécuteur doit imposer, jamais des conseils facultatifs."""

    timeout_seconds: float = 10.0
    max_output_bytes: int = 64 * 1024
    network: NetworkRequirement = NetworkRequirement.OFFLINE

    def __post_init__(self):
        if not 0 < self.timeout_seconds <= 300:
            raise ValueError("timeout d'outil hors limites")
        if not 1 <= self.max_output_bytes <= 16 * 1024 * 1024:
            raise ValueError("sortie maximale d'outil hors limites")


@dataclass(frozen=True)
class ToolDefinition:
    tool_id: str
    display_name: str
    binary: str
    risk_class: RiskClass
    input_contract: Mapping[str, object]
    output_contract: Mapping[str, object]
    execution_profile: ExecutionProfile = field(default_factory=ExecutionProfile)
    documentation_sources: tuple[str, ...] = ()
    documentation_available: bool = False
    execution_available: bool = False
    policy_allowed: bool = False
    detected_version: str | None = None
    availability_reason: str | None = None

    def __post_init__(self):
        if not re.fullmatch(r"[a-z][a-z0-9_.-]{2,79}", self.tool_id):
            raise ValueError("tool_id invalide")
        if not self.display_name.strip():
            raise ValueError("display_name vide")
        if not self.binary or Path(self.binary).name != self.binary or "\x00" in self.binary:
            raise ValueError("binary doit être un nom de programme explicite")
        object.__setattr__(self, "input_contract", MappingProxyType(dict(self.input_contract)))
        object.__setattr__(self, "output_contract", MappingProxyType(dict(self.output_contract)))
        object.__setattr__(self, "documentation_sources", tuple(self.documentation_sources))


@dataclass(frozen=True)
class DetectionResult:
    execution_available: bool
    version: str | None
    reason: str
    executable_path: str | None = None


class ToolRegistryError(ValueError):
    pass


class ToolDetector:
    """Détection bornée par argv ; aucun shell ni installation implicite."""

    def __init__(self, runner: Callable[..., object] = subprocess.run,
                 which: Callable[[str], str | None] = shutil.which):
        self._runner = runner
        self._which = which

    def detect(self, definition: ToolDefinition) -> DetectionResult:
        executable = self._which(definition.binary)
        if executable is None:
            return DetectionResult(False, None, "BINARY_NOT_FOUND")
        last_reason = "VERSION_PROBE_FAILED"
        for option in ("--version", "--help"):
            try:
                completed = self._runner(
                    [executable, option], check=False, capture_output=True,
                    text=False, timeout=min(definition.execution_profile.timeout_seconds, 3.0),
                    env={"PATH": "/usr/bin:/bin", "LC_ALL": "C"},
                )
            except (OSError, subprocess.SubprocessError):
                last_reason = "VERSION_PROBE_UNAVAILABLE"
                continue
            output = bytes(getattr(completed, "stdout", b"")) + bytes(
                getattr(completed, "stderr", b"")
            )
            output = output[:4096]
            if getattr(completed, "returncode", 1) == 0 and output.strip():
                version = output.decode("utf-8", "replace").splitlines()[0][:256]
                return DetectionResult(True, version, "AVAILABLE", executable)
            last_reason = "VERSION_PROBE_REJECTED"
        return DetectionResult(False, None, last_reason, executable)


class ToolRegistry:
    def __init__(self, definitions: Sequence[ToolDefinition] = ()):
        self._definitions: dict[str, ToolDefinition] = {}
        for definition in definitions:
            self.register(definition)

    def register(self, definition: ToolDefinition) -> None:
        # INVARIANT: le binaire ne sert jamais à fabriquer l'identité stable.
        if definition.tool_id in self._definitions:
            raise ToolRegistryError("tool_id déjà enregistré")
        self._definitions[definition.tool_id] = definition

    def get(self, tool_id: str) -> ToolDefinition:
        try:
            return self._definitions[tool_id]
        except KeyError as error:
            raise ToolRegistryError("outil inconnu") from error

    def list_tools(self) -> tuple[ToolDefinition, ...]:
        return tuple(self._definitions[key] for key in sorted(self._definitions))

    def detect(self, tool_id: str, detector: ToolDetector | None = None) -> ToolDefinition:
        definition = self.get(tool_id)
        result = (detector or ToolDetector()).detect(definition)
        updated = replace(
            definition,
            execution_available=result.execution_available,
            detected_version=result.version,
            availability_reason=result.reason,
        )
        self._definitions[tool_id] = updated
        return updated

    def executable_definition(self, tool_id: str) -> ToolDefinition:
        definition = self.get(tool_id)
        if not definition.execution_available:
            raise ToolRegistryError("outil indisponible")
        if not definition.policy_allowed or definition.risk_class == RiskClass.DENIED:
            raise ToolRegistryError("outil refusé par la policy")
        return definition


class DocumentationError(ValueError):
    pass


class ToolDocumentationBroker:
    """Cache XDG local ; son contenu reste non fiable et non exécutable."""

    def __init__(self, cache_root: Path | None = None, max_document_bytes: int = 256 * 1024):
        if not 1 <= max_document_bytes <= 4 * 1024 * 1024:
            raise ValueError("limite documentaire invalide")
        xdg_cache = os.environ.get("XDG_CACHE_HOME")
        default_root = Path(xdg_cache) if xdg_cache else Path.home() / ".cache"
        self.cache_root = cache_root or default_root / "labfy-investigation" / "tool-docs-v1"
        self.max_document_bytes = max_document_bytes

    def _tool_directory(self, tool_id: str) -> Path:
        if not re.fullmatch(r"[a-z][a-z0-9_.-]{2,79}", tool_id):
            raise DocumentationError("tool_id documentaire invalide")
        directory = self.cache_root / tool_id
        if directory.is_symlink():
            raise DocumentationError("cache documentaire symlinké refusé")
        return directory

    def store(self, tool_id: str, source_id: str, content: bytes) -> str:
        if not isinstance(content, bytes) or len(content) > self.max_document_bytes:
            raise DocumentationError("document hors limites")
        digest = hashlib.sha256(source_id.encode("utf-8") + b"\0" + content).hexdigest()
        directory = self._tool_directory(tool_id)
        directory.mkdir(mode=0o700, parents=True, exist_ok=True)
        if directory.is_symlink() or not directory.is_dir():
            raise DocumentationError("cache documentaire invalide")
        path = directory / f"{digest}.txt"
        flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL | getattr(os, "O_NOFOLLOW", 0)
        try:
            descriptor = os.open(path, flags, 0o600)
        except FileExistsError:
            return digest
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(content)
        return digest

    def read(self, tool_id: str, document_id: str) -> str:
        if not re.fullmatch(r"[0-9a-f]{64}", document_id):
            raise DocumentationError("identifiant documentaire invalide")
        path = self._tool_directory(tool_id) / f"{document_id}.txt"
        if path.is_symlink():
            raise DocumentationError("document symlinké refusé")
        try:
            content = path.read_bytes()
        except (OSError, ValueError) as error:
            raise DocumentationError("document indisponible") from error
        if len(content) > self.max_document_bytes:
            raise DocumentationError("document hors limites")
        return content.decode("utf-8", "replace")

    def search(self, tool_id: str, query: str, limit: int = 10) -> tuple[dict[str, object], ...]:
        if not query.strip() or len(query) > 256 or not 1 <= limit <= 50:
            raise DocumentationError("recherche documentaire invalide")
        directory = self._tool_directory(tool_id)
        if not directory.is_dir():
            return ()
        needle = query.casefold()
        results = []
        for path in sorted(directory.glob("[0-9a-f]" * 64 + ".txt")):
            text = self.read(tool_id, path.stem)
            position = text.casefold().find(needle)
            if position >= 0:
                start = max(0, position - 80)
                results.append({"document_id": path.stem, "excerpt": text[start:start + 240]})
                if len(results) == limit:
                    break
        return tuple(results)
