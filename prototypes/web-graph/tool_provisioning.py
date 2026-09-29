"""Provisionnement contrôlé d'outils Debian dans des générations Podman immuables.

Ce module ne contient aucune route HTTP et n'accorde aucun droit au modèle. Les
méthodes ``approve_*`` constituent la frontière backend appelée par une décision
humaine authentifiée. Les appels Agent autorisés sont ``search``, ``propose``,
``get``, ``propose_integration`` et ``execute``.
"""

from __future__ import annotations

import hashlib
import json
import os
import re
import subprocess
import tempfile
import time
import uuid
from dataclasses import dataclass
from datetime import datetime, timezone
from enum import Enum
from pathlib import Path
from types import MappingProxyType
from typing import Callable, Mapping, Sequence


REQUEST_CONTRACT = "labfy.tool_provision_request.v1"
GENERATION_CONTRACT = "labfy.toolbox_generation.v1"
INTEGRATION_CONTRACT = "labfy.tool_integration_proposal.v1"
ADAPTER_CONTRACT = "labfy.declarative_adapter.v1"
CAPABILITY_CONTRACT = "labfy.capability_manifest.v1"
EXECUTION_CONTRACT = "labfy.capability_execution.v1"
BASE_IMAGE = "docker.io/library/debian:12.12-slim"

_ID = re.compile(r"[a-z][a-z0-9_.-]{2,79}\Z")
_PACKAGE = re.compile(r"[a-z0-9][a-z0-9+.-]{0,99}\Z")
_VERSION = re.compile(r"[A-Za-z0-9][A-Za-z0-9.+:~_-]{0,199}\Z")
_ARCH = re.compile(r"[a-z0-9][a-z0-9_-]{0,31}\Z")
_SAFE_BINARY = re.compile(r"[A-Za-z0-9][A-Za-z0-9_.+-]{0,79}\Z")
_DENIED_BINARIES = frozenset({
    "sh", "bash", "dash", "zsh", "sudo", "su", "podman", "docker", "runc",
    "mount", "nsenter", "unshare", "systemctl",
})
_PARSERS = frozenset({
    "STDOUT_TEXT", "STDOUT_JSON", "STDOUT_JSONL", "OUTPUT_FILE_TEXT",
    "OUTPUT_FILE_JSON",
})
_RISK_CLASSES = frozenset({"OFFLINE_READ_ONLY", "PASSIVE_PUBLIC"})
_OBJECT_TYPES = frozenset({"username", "domain", "email", "image", "file", "hash"})


class ProvisionState(str, Enum):
    PROPOSED = "PROPOSED"
    WAITING_PROVISION_APPROVAL = "WAITING_PROVISION_APPROVAL"
    PROVISION_REJECTED = "PROVISION_REJECTED"
    BUILDING = "BUILDING"
    QUARANTINED = "QUARANTINED"
    VALIDATING = "VALIDATING"
    WAITING_INTEGRATION_APPROVAL = "WAITING_INTEGRATION_APPROVAL"
    INTEGRATION_REJECTED = "INTEGRATION_REJECTED"
    ACTIVATING = "ACTIVATING"
    ACTIVE = "ACTIVE"
    FAILED = "FAILED"
    ROLLED_BACK = "ROLLED_BACK"


class ProvisioningError(ValueError):
    """Erreur de contrat explicite, sûre à transformer en réponse 4xx."""


@dataclass(frozen=True)
class PackageMetadata:
    package: str
    version: str
    architecture: str
    dependencies: tuple[str, ...]
    estimated_size: int
    repository: str = "DEBIAN_OFFICIAL"
    provider: str = "DEBIAN_APT"

    def as_dict(self) -> dict[str, object]:
        return {
            "provider": self.provider,
            "package": self.package,
            "version": self.version,
            "architecture": self.architecture,
            "dependencies": list(self.dependencies),
            "estimated_size": self.estimated_size,
            "repository": self.repository,
        }


@dataclass(frozen=True)
class BuildResult:
    image_id: str
    image_digest: str
    base_digest: str
    sbom: tuple[Mapping[str, str], ...]
    health: Mapping[str, object]
    documents: Mapping[str, str]
    image_ref: str | None = None


@dataclass(frozen=True)
class ToolboxResult:
    status: str
    returncode: int | None
    stdout: bytes
    stderr: bytes
    outputs: Mapping[str, bytes]
    argv: tuple[str, ...]


def _canonical_bytes(value: object) -> bytes:
    return json.dumps(value, ensure_ascii=False, sort_keys=True,
                      separators=(",", ":")).encode("utf-8")


def _digest(value: object) -> str:
    return hashlib.sha256(_canonical_bytes(value)).hexdigest()


def _now() -> str:
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


def _completed_bytes(value: object) -> bytes:
    if value is None:
        return b""
    return value if isinstance(value, bytes) else str(value).encode("utf-8", "replace")


def _require_exact_keys(value: Mapping[str, object], required: set[str], name: str) -> None:
    if not isinstance(value, Mapping) or set(value) != required:
        missing = sorted(required - set(value)) if isinstance(value, Mapping) else sorted(required)
        extra = sorted(set(value) - required) if isinstance(value, Mapping) else []
        raise ProvisioningError(f"{name} invalide (missing={missing}, extra={extra})")


def _safe_id(value: object, name: str) -> str:
    if not isinstance(value, str) or not _ID.fullmatch(value):
        raise ProvisioningError(f"{name} invalide")
    return value


def _uuid(value: object, name: str) -> str:
    try:
        parsed = uuid.UUID(str(value))
    except (ValueError, TypeError, AttributeError) as error:
        raise ProvisioningError(f"{name} invalide") from error
    return str(parsed)


class DebianAptProvider:
    """Résout apt dans une image Debian dédiée, jamais via apt sur l'hôte."""

    OFFICIAL_MARKERS = ("deb.debian.org/debian", "security.debian.org/debian-security")

    def __init__(self, runner: Callable[..., object] = subprocess.run,
                 max_dependencies: int = 256, max_estimated_size: int = 512 * 1024 * 1024,
                 podman_binary: str = "podman", base_image: str = BASE_IMAGE):
        self._runner = runner
        self.max_dependencies = max_dependencies
        self.max_estimated_size = max_estimated_size
        self.podman_binary = podman_binary
        self.base_image = base_image
        self.metadata_image = "localhost/labfy-debian-apt-metadata:12.12"
        self._metadata_ready = False

    def _run_host(self, argv: Sequence[str], timeout: float = 10) -> object:
        try:
            return self._runner(
                list(argv), check=False, capture_output=True, text=False, timeout=timeout,
                env={"PATH": "/usr/bin:/bin", "LC_ALL": "C", "LANG": "C"},
            )
        except (OSError, subprocess.SubprocessError) as error:
            raise ProvisioningError("métadonnées Debian indisponibles") from error

    def _ensure_metadata_image(self) -> None:
        if self._metadata_ready:
            return
        inspect = self._run_host((self.podman_binary, "image", "exists", self.metadata_image))
        if int(getattr(inspect, "returncode", 1)) != 0:
            containerfile = (
                f"FROM {self.base_image}\n"
                "RUN apt-get update\n"
                "ENTRYPOINT [\"apt-cache\"]\n"
            )
            with tempfile.TemporaryDirectory(prefix="labfy-apt-metadata-") as temporary:
                path = Path(temporary) / "Containerfile"
                path.write_text(containerfile, encoding="utf-8")
                path.chmod(0o600)
                built = self._run_host((
                    self.podman_binary, "build", "--format", "oci", "--tag",
                    self.metadata_image, "--file", str(path), temporary,
                ), 300)
            if int(getattr(built, "returncode", 1)) != 0:
                raise ProvisioningError("image de métadonnées Debian indisponible")
        self._metadata_ready = True

    def _apt_cache(self, arguments: Sequence[str]) -> object:
        self._ensure_metadata_image()
        return self._run_host((
            self.podman_binary, "run", "--rm", "--network", "none", "--read-only",
            "--cap-drop", "ALL", "--security-opt", "no-new-privileges", "--pids-limit",
            "64", "--memory", "256m", self.metadata_image, *arguments,
        ))

    @staticmethod
    def _fields(content: bytes) -> dict[str, str]:
        fields: dict[str, str] = {}
        current = None
        for raw_line in content.decode("utf-8", "replace").splitlines():
            if raw_line.startswith((" ", "\t")) and current:
                fields[current] += " " + raw_line.strip()
            elif ":" in raw_line:
                current, value = raw_line.split(":", 1)
                fields[current] = value.strip()
        return fields

    @staticmethod
    def _dependencies(value: str) -> tuple[str, ...]:
        names = []
        for group in filter(None, (part.strip() for part in value.split(","))):
            alternative = group.split("|", 1)[0].strip()
            name = alternative.split("(", 1)[0].strip().split(":", 1)[0]
            if not _PACKAGE.fullmatch(name):
                raise ProvisioningError("dépendance Debian invalide")
            names.append(name)
        return tuple(dict.fromkeys(names))

    def search(self, query: str, limit: int = 10) -> tuple[dict[str, object], ...]:
        if not isinstance(query, str) or not _PACKAGE.fullmatch(query) or not 1 <= limit <= 25:
            raise ProvisioningError("recherche de package invalide")
        completed = self._apt_cache(("search", "--names-only", f"^{query}"))
        if int(getattr(completed, "returncode", 1)) != 0:
            raise ProvisioningError("recherche Debian refusée")
        results = []
        for line in _completed_bytes(getattr(completed, "stdout", b"")).decode(
                "utf-8", "replace").splitlines():
            package, separator, description = line.partition(" - ")
            if separator and _PACKAGE.fullmatch(package) and len(description) <= 500:
                results.append({"provider": "DEBIAN_APT", "package": package,
                                "description": description})
            if len(results) == limit:
                break
        return tuple(results)

    def resolve(self, package: str) -> PackageMetadata:
        if not isinstance(package, str) or not _PACKAGE.fullmatch(package):
            raise ProvisioningError("nom de package hostile ou invalide")
        policy = self._apt_cache(("policy", package))
        policy_text = _completed_bytes(getattr(policy, "stdout", b"")).decode("utf-8", "replace")
        if int(getattr(policy, "returncode", 1)) != 0 or not any(
                marker in policy_text for marker in self.OFFICIAL_MARKERS):
            raise ProvisioningError("package absent d'un dépôt Debian officiel")
        candidate = None
        for line in policy_text.splitlines():
            if line.strip().startswith("Candidate:"):
                candidate = line.split(":", 1)[1].strip()
                break
        if not candidate or candidate == "(none)" or not _VERSION.fullmatch(candidate):
            raise ProvisioningError("version Debian candidate invalide")
        show = self._apt_cache(("show", "--no-all-versions", f"{package}={candidate}"))
        if int(getattr(show, "returncode", 1)) != 0:
            raise ProvisioningError("métadonnées exactes Debian indisponibles")
        fields = self._fields(_completed_bytes(getattr(show, "stdout", b"")))
        if fields.get("Package") != package or fields.get("Version") != candidate:
            raise ProvisioningError("métadonnées Debian incohérentes")
        architecture = fields.get("Architecture", "")
        if not _ARCH.fullmatch(architecture):
            raise ProvisioningError("architecture Debian invalide")
        dependencies = self._dependencies(fields.get("Depends", ""))
        if len(dependencies) > self.max_dependencies:
            raise ProvisioningError("trop de dépendances Debian")
        try:
            estimated_size = int(fields.get("Installed-Size", "0")) * 1024
        except ValueError as error:
            raise ProvisioningError("taille Debian invalide") from error
        if not 0 < estimated_size <= self.max_estimated_size:
            raise ProvisioningError("taille Debian hors limites")
        return PackageMetadata(package, candidate, architecture, dependencies, estimated_size)


class GenerationStore:
    """Store JSON privé et atomique sous la racine XDG dédiée."""

    def __init__(self, root: Path):
        self.root = Path(root)
        if self.root.exists() and self.root.is_symlink():
            raise ProvisioningError("racine toolbox symlinkée refusée")
        self.root.mkdir(mode=0o700, parents=True, exist_ok=True)
        os.chmod(self.root, 0o700)
        self.generations = self.root / "generations"
        self.requests = self.root / "requests"
        self.integrations = self.root / "integrations"
        for directory in (self.generations, self.requests, self.integrations):
            directory.mkdir(mode=0o700, exist_ok=True)
            if directory.is_symlink():
                raise ProvisioningError("répertoire toolbox symlinké refusé")
            os.chmod(directory, 0o700)
        if not (self.root / "state.json").exists():
            self.write_state({"contract": "labfy.toolbox_state.v1", "current_generation": None,
                              "catalog_revision": 0})

    def _atomic_json(self, path: Path, value: object) -> None:
        if path.exists() and path.is_symlink():
            raise ProvisioningError("fichier toolbox symlinké refusé")
        payload = _canonical_bytes(value) + b"\n"
        descriptor, temporary = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
        try:
            os.fchmod(descriptor, 0o600)
            with os.fdopen(descriptor, "wb") as stream:
                stream.write(payload)
                stream.flush()
                os.fsync(stream.fileno())
            os.replace(temporary, path)
            os.chmod(path, 0o600)
        finally:
            try:
                os.unlink(temporary)
            except FileNotFoundError:
                pass

    @staticmethod
    def _read_json(path: Path) -> dict[str, object]:
        if path.is_symlink() or not path.is_file():
            raise ProvisioningError("état toolbox indisponible")
        try:
            value = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, ValueError) as error:
            raise ProvisioningError("état toolbox corrompu") from error
        if not isinstance(value, dict):
            raise ProvisioningError("état toolbox invalide")
        return value

    def read_state(self) -> dict[str, object]:
        return self._read_json(self.root / "state.json")

    def write_state(self, state: Mapping[str, object]) -> None:
        self._atomic_json(self.root / "state.json", dict(state))

    def request_path(self, request_id: str) -> Path:
        return self.requests / f"{_uuid(request_id, 'request_id')}.json"

    def write_request(self, request: Mapping[str, object]) -> None:
        self._atomic_json(self.request_path(str(request["request_id"])), dict(request))

    def read_request(self, request_id: str) -> dict[str, object]:
        return self._read_json(self.request_path(request_id))

    def list_requests(self) -> tuple[dict[str, object], ...]:
        return tuple(self._read_json(path) for path in sorted(self.requests.glob("*.json"))
                     if path.is_file() and not path.is_symlink())

    def generation_path(self, generation_id: int) -> Path:
        if not isinstance(generation_id, int) or not 1 <= generation_id <= 999999:
            raise ProvisioningError("generation_id invalide")
        return self.generations / f"{generation_id:06d}.json"

    def write_generation(self, generation: Mapping[str, object]) -> None:
        value = dict(generation)
        value["record_digest"] = _digest({key: item for key, item in value.items()
                                          if key != "record_digest"})
        self._atomic_json(self.generation_path(int(value["generation_id"])), value)

    def read_generation(self, generation_id: int) -> dict[str, object]:
        value = self._read_json(self.generation_path(generation_id))
        digest = value.get("record_digest")
        if digest != _digest({key: item for key, item in value.items() if key != "record_digest"}):
            raise ProvisioningError("manifest de génération altéré")
        return value

    def list_generations(self) -> tuple[dict[str, object], ...]:
        return tuple(self.read_generation(int(path.stem))
                     for path in sorted(self.generations.glob("*.json"))
                     if path.is_file() and not path.is_symlink())

    def write_integration(self, tool_id: str, proposal: Mapping[str, object]) -> None:
        self._atomic_json(self.integrations / f"{_safe_id(tool_id, 'tool_id')}.json",
                          dict(proposal))

    def read_integrations(self) -> tuple[dict[str, object], ...]:
        return tuple(self._read_json(path) for path in sorted(self.integrations.glob("*.json"))
                     if path.is_file() and not path.is_symlink())


class ToolboxExec:
    """Exécuteur Podman rootless offline, sans montage home ou dépôt."""

    def __init__(self, runner: Callable[..., object] = subprocess.run,
                 podman_binary: str = "podman"):
        self._runner = runner
        self.podman_binary = podman_binary

    def execute(self, image: str, binary: str, arguments: Sequence[str], *,
                artifacts: Mapping[str, bytes] | None = None,
                output_names: Sequence[str] = (), timeout: float = 30,
                max_output_bytes: int = 64 * 1024) -> ToolboxResult:
        if not isinstance(image, str) or not re.fullmatch(
                r"localhost/labfy-investigation-toolbox(?:-[a-z0-9]{8,32})?:[1-9][0-9]*", image):
            raise ProvisioningError("image toolbox invalide")
        if not _SAFE_BINARY.fullmatch(binary) or binary in _DENIED_BINARIES:
            raise ProvisioningError("binaire toolbox refusé")
        if not 0 < timeout <= 300 or not 1 <= max_output_bytes <= 16 * 1024 * 1024:
            raise ProvisioningError("limites d'exécution invalides")
        artifact_map = dict(artifacts or {})
        if any(not _SAFE_BINARY.fullmatch(name) or not isinstance(content, bytes)
               for name, content in artifact_map.items()):
            raise ProvisioningError("artefact toolbox invalide")
        if len(set(output_names)) != len(output_names) or any(
                not _SAFE_BINARY.fullmatch(name) for name in output_names):
            raise ProvisioningError("sorties toolbox invalides")
        for argument in arguments:
            if not isinstance(argument, str) or "\x00" in argument or argument.startswith(("/home", "~")):
                raise ProvisioningError("argument toolbox invalide")
        with tempfile.TemporaryDirectory(prefix="labfy-toolbox-") as temporary:
            root = Path(temporary)
            input_directory = root / "input"
            output_directory = root / "output"
            input_directory.mkdir(mode=0o700)
            output_directory.mkdir(mode=0o700)
            for name, content in artifact_map.items():
                path = input_directory / name
                path.write_bytes(content)
                path.chmod(0o400)
            argv = self._argv(image, binary, arguments, input_directory, output_directory)
            try:
                completed = self._runner(
                    argv, check=False, capture_output=True, text=False, timeout=timeout,
                    env={"PATH": "/usr/bin:/bin", "LC_ALL": "C", "LANG": "C"},
                )
            except subprocess.TimeoutExpired as error:
                return ToolboxResult("TIMEOUT", None, _completed_bytes(error.stdout)[:max_output_bytes],
                                     _completed_bytes(error.stderr)[:max_output_bytes], {}, tuple(argv))
            except OSError:
                return ToolboxResult("UNAVAILABLE", None, b"", b"", {}, tuple(argv))
            stdout = _completed_bytes(getattr(completed, "stdout", b""))
            stderr = _completed_bytes(getattr(completed, "stderr", b""))
            outputs: dict[str, bytes] = {}
            for name in output_names:
                path = output_directory / name
                if path.is_file() and not path.is_symlink():
                    outputs[name] = path.read_bytes()[:max_output_bytes]
            total = len(stdout) + len(stderr) + sum(map(len, outputs.values()))
            status = "SUCCESS" if int(getattr(completed, "returncode", 1)) == 0 else "FAILED"
            if total > max_output_bytes:
                status = "OUTPUT_LIMIT"
            remaining = max_output_bytes
            stdout = stdout[:remaining]
            remaining -= len(stdout)
            stderr = stderr[:remaining]
            remaining -= len(stderr)
            for name in sorted(outputs):
                outputs[name] = outputs[name][:remaining]
                remaining -= len(outputs[name])
            return ToolboxResult(status, int(getattr(completed, "returncode", 1)), stdout,
                                 stderr, MappingProxyType(outputs), tuple(argv))

    def _argv(self, image: str, binary: str, arguments: Sequence[str],
              input_directory: Path, output_directory: Path) -> list[str]:
        return [
            self.podman_binary, "run", "--rm", "--network", "none", "--read-only",
            "--cap-drop", "ALL", "--security-opt", "no-new-privileges", "--pids-limit", "64",
            "--memory", "256m", "--userns", "keep-id", "--user",
            f"{os.getuid()}:{os.getgid()}", "--env", "HOME=/tmp",
            "--mount", f"type=bind,src={input_directory},dst=/input,ro=true",
            "--mount", f"type=bind,src={output_directory},dst=/output,rw=true",
            "--workdir", "/output", image, binary, *arguments,
        ]


class PodmanToolboxBuilder:
    """Builder dont le Containerfile est entièrement possédé par le backend."""

    def __init__(self, runner: Callable[..., object] = subprocess.run,
                 toolbox_exec: ToolboxExec | None = None, base_image: str = BASE_IMAGE,
                 podman_binary: str = "podman",
                 image_prefix: str = "localhost/labfy-investigation-toolbox"):
        self._runner = runner
        self.toolbox_exec = toolbox_exec or ToolboxExec(runner, podman_binary)
        self.base_image = base_image
        self.podman_binary = podman_binary
        if not re.fullmatch(r"localhost/labfy-investigation-toolbox(?:-[a-z0-9]{8,32})?",
                            image_prefix):
            raise ValueError("Préfixe d'image toolbox invalide")
        self.image_prefix = image_prefix

    def _run(self, argv: Sequence[str], timeout: float = 300) -> object:
        try:
            return self._runner(
                list(argv), check=False, capture_output=True, text=False, timeout=timeout,
                env={"PATH": "/usr/bin:/bin", "LC_ALL": "C", "LANG": "C"},
            )
        except (OSError, subprocess.SubprocessError) as error:
            raise ProvisioningError("Podman indisponible") from error

    def build(self, generation_id: int, packages: Sequence[PackageMetadata]) -> BuildResult:
        if not packages or len(packages) > 16:
            raise ProvisioningError("nombre de packages hors limites")
        specs = []
        for package in packages:
            if not _PACKAGE.fullmatch(package.package) or not _VERSION.fullmatch(package.version):
                raise ProvisioningError("package exact invalide")
            specs.append(f"{package.package}={package.version}")
        tag = f"{self.image_prefix}:{generation_id}"
        containerfile = self._containerfile(specs)
        with tempfile.TemporaryDirectory(prefix="labfy-toolbox-build-") as temporary:
            path = Path(temporary) / "Containerfile"
            path.write_text(containerfile, encoding="utf-8")
            path.chmod(0o600)
            built = self._run((self.podman_binary, "build", "--format", "oci", "--tag", tag,
                               "--file", str(path), temporary), 600)
        if int(getattr(built, "returncode", 1)) != 0:
            raise ProvisioningError("construction toolbox échouée")
        inspect = self._run((self.podman_binary, "image", "inspect", tag, "--format", "json"))
        try:
            details = json.loads(_completed_bytes(getattr(inspect, "stdout", b"")))
            item = details[0]
            image_id = str(item["Id"])
            digests = item.get("Digest") or item.get("RepoDigests") or []
            image_digest = digests if isinstance(digests, str) else str(digests[0])
        except (ValueError, KeyError, IndexError, TypeError) as error:
            raise ProvisioningError("identité image Podman invalide") from error
        base_inspect = self._run((self.podman_binary, "image", "inspect", self.base_image,
                                  "--format", "{{.Digest}}"))
        base_digest = _completed_bytes(getattr(base_inspect, "stdout", b"")).decode().strip()
        if not base_digest:
            raise ProvisioningError("digest image de base indisponible")
        sbom_result = self.toolbox_exec.execute(
            tag, "dpkg-query", ("-W", "-f=${binary:Package}\t${Version}\t${Architecture}\n"),
            timeout=30, max_output_bytes=4 * 1024 * 1024,
        )
        if sbom_result.status != "SUCCESS":
            raise ProvisioningError("capture SBOM échouée")
        sbom = []
        for line in sbom_result.stdout.decode("utf-8", "replace").splitlines():
            fields = line.split("\t")
            if len(fields) == 3:
                sbom.append({"package": fields[0], "version": fields[1],
                             "architecture": fields[2]})
        if not sbom:
            raise ProvisioningError("SBOM vide")
        files_result = self.toolbox_exec.execute(
            tag, "dpkg-query", ("-L", packages[-1].package), timeout=10,
            max_output_bytes=512 * 1024,
        )
        if files_result.status != "SUCCESS":
            raise ProvisioningError("inventaire du package échoué")
        installed_files = files_result.stdout.decode("utf-8", "replace").splitlines()
        binaries = [Path(path).name for path in installed_files
                    if (path.startswith("/usr/bin/") or path.startswith("/bin/"))
                    and _SAFE_BINARY.fullmatch(Path(path).name)
                    and Path(path).name not in _DENIED_BINARIES]
        if not binaries:
            raise ProvisioningError("aucun binaire exécutable admissible dans le package")
        binary = sorted(set(binaries))[0]
        version = self.toolbox_exec.execute(tag, binary, ("--version",), timeout=5)
        help_result = self.toolbox_exec.execute(tag, binary, ("--help",), timeout=5,
                                                max_output_bytes=256 * 1024)
        health = {"version": version.status, "help": help_result.status,
                  "healthy": version.status == "SUCCESS" or help_result.status == "SUCCESS"}
        if not health["healthy"]:
            raise ProvisioningError("healthcheck toolbox échoué")
        documents = {
            "help": "UNTRUSTED_DATA\n" + help_result.stdout.decode("utf-8", "replace"),
            "installed-files": "UNTRUSTED_DATA\n" + "\n".join(installed_files[:2048]),
        }
        documentation_paths = [path for path in installed_files
                               if path.startswith(f"/usr/share/doc/{packages[-1].package}/")
                               and (path.endswith("copyright") or path.endswith(".txt"))]
        for index, path in enumerate(documentation_paths[:3]):
            captured = self.toolbox_exec.execute(
                tag, "cat", (path,), timeout=5, max_output_bytes=256 * 1024)
            if captured.status == "SUCCESS":
                documents[f"doc-{index}"] = (
                    "UNTRUSTED_DATA\n" + captured.stdout.decode("utf-8", "replace"))
        man_paths = [path for path in installed_files
                     if path.startswith("/usr/share/man/") and path.endswith(".gz")]
        for index, path in enumerate(man_paths[:2]):
            captured = self.toolbox_exec.execute(
                tag, "zcat", (path,), timeout=5, max_output_bytes=256 * 1024)
            if captured.status == "SUCCESS":
                documents[f"man-{index}"] = (
                    "UNTRUSTED_DATA\n" + captured.stdout.decode("utf-8", "replace"))
        return BuildResult(image_id, image_digest, base_digest, tuple(sbom), health,
                           documents, tag)

    def _containerfile(self, package_specs: Sequence[str]) -> str:
        # Les seules interpolations ont déjà passé les grammaires Debian strictes.
        packages = " ".join(package_specs)
        return (
            f"FROM {self.base_image}\n"
            "ENV DEBIAN_FRONTEND=noninteractive\n"
            "RUN apt-get update \\\n && apt-get install -y --no-install-recommends " + packages + " \\\n && rm -rf /var/lib/apt/lists/*\n"
            "USER 65534:65534\n"
        )


class ToolProvisioningService:
    """Orchestre Gate A, quarantaine, Gate B, activation et exécution."""

    def __init__(self, data_root: Path | None = None, provider: object | None = None,
                 builder: object | None = None, executor: object | None = None,
                 clock: Callable[[], str] | None = None,
                 policy_check: Callable[[Mapping[str, object], Mapping[str, object]], bool] | None = None):
        if data_root is None:
            xdg_data = os.environ.get("XDG_DATA_HOME")
            data_home = Path(xdg_data) if xdg_data else Path.home() / ".local" / "share"
            data_root = data_home / "labfy-investigation" / "toolbox"
        self.store = GenerationStore(Path(data_root))
        self.provider = provider or DebianAptProvider()
        self.executor = executor or ToolboxExec()
        self.builder = builder or PodmanToolboxBuilder(toolbox_exec=self.executor)
        self.clock = clock or _now
        self.policy_check = policy_check or (lambda _capability, _context: False)
        self._verify_persisted_state()

    def search(self, query: str, limit: int = 10) -> tuple[dict[str, object], ...]:
        return tuple(self.provider.search(query, limit))

    def propose(self, package: str, *, workspace_id: str, mission_id: str, turn_id: str,
                idempotency_key: str) -> dict[str, object]:
        workspace_id = _uuid(workspace_id, "workspace_id")
        mission_id = _uuid(mission_id, "mission_id")
        turn_id = _uuid(turn_id, "turn_id")
        key = _uuid(idempotency_key, "idempotency_key")
        for existing in self.store.list_requests():
            if existing.get("idempotency_key") == key:
                return existing
            if existing.get("package", {}).get("package") == package and existing.get("state") in {
                    ProvisionState.WAITING_PROVISION_APPROVAL.value, ProvisionState.BUILDING.value,
                    ProvisionState.QUARANTINED.value, ProvisionState.WAITING_INTEGRATION_APPROVAL.value,
                    ProvisionState.ACTIVE.value}:
                raise ProvisioningError("package déjà proposé ou actif")
        metadata = self.provider.resolve(package)
        if len(metadata.dependencies) > 256 or metadata.estimated_size > 512 * 1024 * 1024:
            raise ProvisioningError("budget de provisionnement dépassé")
        request = {
            "contract": REQUEST_CONTRACT, "request_id": str(uuid.uuid4()),
            "state": ProvisionState.WAITING_PROVISION_APPROVAL.value,
            "workspace_id": workspace_id, "mission_id": mission_id, "turn_id": turn_id,
            "idempotency_key": key,
            "created_at": self.clock(), "package": metadata.as_dict(), "generation_id": None,
            "provision_decision": None, "integration_decision": None,
            "integration": None, "diagnostic": None,
            "runtime_state": "TOOL_PROVISIONING_REQUIRED",
        }
        self.store.write_request(request)
        return request

    def get(self, request_id: str) -> dict[str, object]:
        return self.store.read_request(request_id)

    def list_requests(self) -> tuple[dict[str, object], ...]:
        return self.store.list_requests()

    def approve_provision(self, request_id: str, *, decision_id: str, actor: str,
                          human_confirmed: bool) -> dict[str, object]:
        request = self.get(request_id)
        self._require_state(request, ProvisionState.WAITING_PROVISION_APPROVAL)
        request["provision_decision"] = self._human_decision(
            decision_id, actor, human_confirmed, "APPROVED")
        request["state"] = ProvisionState.BUILDING.value
        self.store.write_request(request)
        return request

    def reject_provision(self, request_id: str, *, decision_id: str, actor: str,
                         human_confirmed: bool, reason: str = "") -> dict[str, object]:
        request = self.get(request_id)
        self._require_state(request, ProvisionState.WAITING_PROVISION_APPROVAL)
        request["provision_decision"] = self._human_decision(
            decision_id, actor, human_confirmed, "REJECTED", reason)
        request["state"] = ProvisionState.PROVISION_REJECTED.value
        request["runtime_state"] = "COMPLETED"
        self.store.write_request(request)
        return request

    def build(self, request_id: str) -> dict[str, object]:
        request = self.get(request_id)
        self._require_state(request, ProvisionState.BUILDING)
        state = self.store.read_state()
        parent = state.get("current_generation")
        generation_id = max((int(item["generation_id"])
                             for item in self.store.list_generations()), default=0) + 1
        package = request["package"]
        metadata = PackageMetadata(
            str(package["package"]), str(package["version"]), str(package["architecture"]),
            tuple(package["dependencies"]), int(package["estimated_size"]),
            str(package["repository"]), str(package["provider"]),
        )
        try:
            result = self.builder.build(generation_id, (metadata,))
            sbom = [dict(item) for item in result.sbom]
            generation = {
                "contract": GENERATION_CONTRACT, "generation_id": generation_id,
                "parent": parent, "created_at": self.clock(), "base_image": BASE_IMAGE,
                "base_digest": result.base_digest, "packages": [metadata.as_dict()],
                "image_id": result.image_id, "image_digest": result.image_digest,
                "image_ref": result.image_ref or \
                    f"localhost/labfy-investigation-toolbox:{generation_id}",
                "sbom": sbom, "sbom_digest": _digest(sbom), "health": dict(result.health),
                "documents": {name: "UNTRUSTED_DATA\n" + text.removeprefix("UNTRUSTED_DATA\n")
                              for name, text in result.documents.items()},
                "tool_integrations": [], "state": ProvisionState.QUARANTINED.value,
            }
            self.store.write_generation(generation)
            request["generation_id"] = generation_id
            request["state"] = ProvisionState.QUARANTINED.value
            request["runtime_state"] = "TOOL_INTEGRATION_REQUIRED"
            self.store.write_request(request)
            return request
        except Exception as error:
            request["state"] = ProvisionState.FAILED.value
            request["runtime_state"] = "COMPLETED"
            request["diagnostic"] = str(error)[:500]
            self.store.write_request(request)
            if isinstance(error, ProvisioningError):
                raise
            raise ProvisioningError("construction toolbox échouée") from error

    def documents(self, request_id: str) -> Mapping[str, str]:
        request = self.get(request_id)
        if request["state"] not in {ProvisionState.QUARANTINED.value,
                                    ProvisionState.WAITING_INTEGRATION_APPROVAL.value,
                                    ProvisionState.ACTIVE.value}:
            raise ProvisioningError("documentation indisponible dans cet état")
        generation = self.store.read_generation(int(request["generation_id"]))
        return MappingProxyType(dict(generation["documents"]))

    def propose_integration(self, request_id: str, proposal: Mapping[str, object], *,
                            idempotency_key: str) -> dict[str, object]:
        request = self.get(request_id)
        if request["state"] == ProvisionState.WAITING_INTEGRATION_APPROVAL.value and \
                request.get("integration_idempotency_key") == str(idempotency_key):
            return request
        self._require_state(request, ProvisionState.QUARANTINED)
        key = _uuid(idempotency_key, "idempotency_key")
        validated = validate_integration_proposal(proposal, request)
        generation = self.store.read_generation(int(request["generation_id"]))
        installed_files = str(generation["documents"].get("installed-files", "")).splitlines()
        binary = validated["adapter"]["binary"]
        if f"/usr/bin/{binary}" not in installed_files and f"/bin/{binary}" not in installed_files:
            raise ProvisioningError("binaire adapter absent du package provisionné")
        active_ids = {capability["capability_id"] for capability in self.capability_catalog()}
        for capability in validated["adapter"]["capabilities"]:
            if capability["capability_id"] in active_ids:
                raise ProvisioningError("capability_id déjà actif")
        for integration in self.store.read_integrations():
            if integration["adapter"]["tool_id"] == validated["adapter"]["tool_id"]:
                raise ProvisioningError("tool_id déjà actif")
        request["integration"] = validated
        request["integration_idempotency_key"] = key
        request["state"] = ProvisionState.WAITING_INTEGRATION_APPROVAL.value
        request["runtime_state"] = "TOOL_INTEGRATION_REQUIRED"
        self.store.write_request(request)
        return request

    def approve_integration(self, request_id: str, *, decision_id: str, actor: str,
                            human_confirmed: bool) -> dict[str, object]:
        request = self.get(request_id)
        self._require_state(request, ProvisionState.WAITING_INTEGRATION_APPROVAL)
        request["integration_decision"] = self._human_decision(
            decision_id, actor, human_confirmed, "APPROVED")
        request["state"] = ProvisionState.ACTIVATING.value
        self.store.write_request(request)
        return request

    def reject_integration(self, request_id: str, *, decision_id: str, actor: str,
                           human_confirmed: bool, reason: str = "") -> dict[str, object]:
        request = self.get(request_id)
        self._require_state(request, ProvisionState.WAITING_INTEGRATION_APPROVAL)
        request["integration_decision"] = self._human_decision(
            decision_id, actor, human_confirmed, "REJECTED", reason)
        request["state"] = ProvisionState.INTEGRATION_REJECTED.value
        request["runtime_state"] = "COMPLETED"
        self.store.write_request(request)
        return request

    def activate(self, request_id: str) -> dict[str, object]:
        request = self.get(request_id)
        self._require_state(request, ProvisionState.ACTIVATING)
        proposal = validate_integration_proposal(request["integration"], request)
        tool_id = str(proposal["adapter"]["tool_id"])
        self.store.write_integration(tool_id, proposal)
        generation = self.store.read_generation(int(request["generation_id"]))
        generation["tool_integrations"] = sorted(set(generation["tool_integrations"] + [tool_id]))
        generation["state"] = ProvisionState.ACTIVE.value
        self.store.write_generation(generation)
        state = self.store.read_state()
        state["current_generation"] = request["generation_id"]
        state["catalog_revision"] = int(state["catalog_revision"]) + 1
        self.store.write_state(state)
        request["state"] = ProvisionState.ACTIVE.value
        request["runtime_state"] = "COMPLETED"
        self.store.write_request(request)
        return request

    def capability_catalog(self, object_type: str | None = None) -> tuple[dict[str, object], ...]:
        if object_type is not None and object_type not in _OBJECT_TYPES:
            raise ProvisioningError("type d'objet inconnu")
        state = self.store.read_state()
        current = state.get("current_generation")
        capabilities = []
        for proposal in self.store.read_integrations():
            for capability in proposal["adapter"]["capabilities"]:
                if capability["generation_id"] != current:
                    continue
                if object_type is None or object_type in capability["applicable_object_types"]:
                    item = dict(capability)
                    item["catalog_revision"] = state["catalog_revision"]
                    capabilities.append(item)
        return tuple(sorted(capabilities, key=lambda item: str(item["capability_id"])))

    def execute(self, capability_id: str, parameters: Mapping[str, object], *,
                artifacts: Mapping[str, bytes] | None = None,
                mission_context: Mapping[str, object], idempotency_key: str) -> dict[str, object]:
        _safe_id(capability_id, "capability_id")
        _uuid(idempotency_key, "idempotency_key")
        self._validate_mission_context(mission_context)
        matches = [capability for capability in self.capability_catalog()
                   if capability["capability_id"] == capability_id]
        if not matches:
            raise ProvisioningError("capability inconnue ou génération inactive")
        capability = matches[0]
        if capability["availability"] != "AVAILABLE" or capability["network_contact"] != "NONE":
            raise ProvisioningError("capability indisponible ou réseau refusé")
        if mission_context.get("object_type") not in capability["applicable_object_types"]:
            raise ProvisioningError("capability inapplicable à l'objet")
        if not self.policy_check(capability, mission_context):
            raise ProvisioningError("capability refusée par la policy")
        proposal = next(item for item in self.store.read_integrations()
                        if item["adapter"]["tool_id"] == capability["tool_id"])
        adapter = proposal["adapter"]
        argv, output_names = materialize_argv(adapter, parameters, artifacts or {})
        generation = self.store.read_generation(int(capability["generation_id"]))
        profile = adapter["execution_profile"]
        result = self.executor.execute(
            generation["image_ref"], adapter["binary"], argv, artifacts=artifacts or {},
            output_names=output_names, timeout=float(profile["timeout_seconds"]),
            max_output_bytes=int(profile["max_output_bytes"]),
        )
        return {
            "contract": EXECUTION_CONTRACT, "capability_id": capability_id,
            "tool_id": capability["tool_id"], "generation_id": capability["generation_id"],
            "mission_id": mission_context["mission_id"], "turn_id": mission_context["turn_id"],
            "status": result.status, "returncode": result.returncode,
            "stdout": result.stdout.decode("utf-8", "replace"),
            "stderr": result.stderr.decode("utf-8", "replace"),
            "outputs": {name: value.decode("utf-8", "replace")
                        for name, value in result.outputs.items()},
        }

    def rollback(self, generation_id: int) -> dict[str, object]:
        generation = self.store.read_generation(generation_id)
        state = self.store.read_state()
        if state.get("current_generation") != generation_id:
            raise ProvisioningError("seule la génération courante peut être rollbackée")
        parent = generation.get("parent")
        generation["state"] = ProvisionState.ROLLED_BACK.value
        self.store.write_generation(generation)
        state["current_generation"] = parent
        state["catalog_revision"] = int(state["catalog_revision"]) + 1
        self.store.write_state(state)
        for request in self.store.list_requests():
            if request.get("generation_id") == generation_id:
                request["state"] = ProvisionState.ROLLED_BACK.value
                self.store.write_request(request)
        return state

    def _verify_persisted_state(self) -> None:
        state = self.store.read_state()
        _require_exact_keys(state, {"contract", "current_generation", "catalog_revision"}, "état")
        if state["contract"] != "labfy.toolbox_state.v1" or not isinstance(
                state["catalog_revision"], int):
            raise ProvisioningError("état toolbox incompatible")
        current = state["current_generation"]
        if current is not None:
            generation = self.store.read_generation(int(current))
            if generation.get("state") != ProvisionState.ACTIVE.value:
                raise ProvisioningError("génération courante non active")
        request_ids = set()
        for request in self.store.list_requests():
            if request.get("contract") != REQUEST_CONTRACT or request.get("request_id") in request_ids:
                raise ProvisioningError("requête persistée invalide")
            _uuid(request.get("workspace_id"), "workspace_id persisté")
            _uuid(request.get("mission_id"), "mission_id persisté")
            _uuid(request.get("turn_id"), "turn_id persisté")
            request_ids.add(request["request_id"])
        for proposal in self.store.read_integrations():
            tool_id = proposal.get("adapter", {}).get("tool_id") if isinstance(proposal, dict) else None
            matching = [request for request in self.store.list_requests()
                        if isinstance(request.get("integration"), Mapping) and
                        request["integration"].get("adapter", {}).get("tool_id") == tool_id]
            if not matching:
                raise ProvisioningError("intégration orpheline ou altérée")
            validate_integration_proposal(proposal, matching[0])

    @staticmethod
    def _require_state(request: Mapping[str, object], state: ProvisionState) -> None:
        if request.get("state") != state.value:
            raise ProvisioningError(f"transition invalide depuis {request.get('state')}")

    def _human_decision(self, decision_id: str, actor: str, confirmed: bool,
                        outcome: str, reason: str = "") -> dict[str, object]:
        if actor != "human" or confirmed is not True:
            raise ProvisioningError("décision humaine externe requise")
        if not isinstance(reason, str) or len(reason) > 500:
            raise ProvisioningError("raison de décision invalide")
        return {"decision_id": _uuid(decision_id, "decision_id"), "actor": actor,
                "human_confirmed": True, "outcome": outcome, "reason": reason,
                "decided_at": self.clock()}

    @staticmethod
    def _validate_mission_context(context: Mapping[str, object]) -> None:
        _require_exact_keys(context, {"workspace_id", "mission_id", "turn_id", "object_type",
                                      "object_id"}, "mission_context")
        _uuid(context["workspace_id"], "workspace_id")
        _uuid(context["mission_id"], "mission_id")
        _uuid(context["turn_id"], "turn_id")
        if context["object_type"] not in _OBJECT_TYPES:
            raise ProvisioningError("object_type invalide")
        if not isinstance(context["object_id"], str) or not context["object_id"]:
            raise ProvisioningError("object_id invalide")


def validate_integration_proposal(proposal: Mapping[str, object],
                                  request: Mapping[str, object]) -> dict[str, object]:
    required = {"contract", "proposal_id", "request_id", "risk", "adapter"}
    _require_exact_keys(proposal, required, "proposition d'intégration")
    if proposal["contract"] != INTEGRATION_CONTRACT:
        raise ProvisioningError("contrat d'intégration incompatible")
    _uuid(proposal["proposal_id"], "proposal_id")
    if proposal["request_id"] != request["request_id"]:
        raise ProvisioningError("proposition liée à une autre requête")
    if proposal["risk"] not in _RISK_CLASSES:
        raise ProvisioningError("risque d'intégration refusé")
    adapter = validate_adapter(proposal["adapter"], request)
    normalized = dict(proposal)
    normalized["adapter"] = adapter
    return normalized


def validate_adapter(adapter: Mapping[str, object], request: Mapping[str, object]) -> dict[str, object]:
    required = {"contract", "tool_id", "binary", "risk_class", "execution_backend",
                "input_contract", "output_contract", "execution_profile", "argv_template",
                "parser", "capabilities"}
    _require_exact_keys(adapter, required, "adapter")
    if adapter["contract"] != ADAPTER_CONTRACT or adapter["execution_backend"] != "TOOLBOX_PODMAN":
        raise ProvisioningError("adapter incompatible")
    tool_id = _safe_id(adapter["tool_id"], "tool_id")
    binary = adapter["binary"]
    if not isinstance(binary, str) or not _SAFE_BINARY.fullmatch(binary) or binary in _DENIED_BINARIES:
        raise ProvisioningError("binaire adapter refusé")
    if adapter["risk_class"] not in _RISK_CLASSES:
        raise ProvisioningError("risk_class adapter refusée")
    validate_contract_schema(adapter["input_contract"], "input_contract")
    validate_contract_schema(adapter["output_contract"], "output_contract")
    profile = adapter["execution_profile"]
    _require_exact_keys(profile, {"timeout_seconds", "max_output_bytes", "network"},
                        "execution_profile")
    if profile["network"] != "OFFLINE" or not isinstance(profile["timeout_seconds"], (int, float)) \
            or not 0 < profile["timeout_seconds"] <= 300 or not isinstance(
                profile["max_output_bytes"], int) or not 1 <= profile["max_output_bytes"] <= 16 * 1024 * 1024:
        raise ProvisioningError("profil d'exécution refusé")
    template = adapter["argv_template"]
    if not isinstance(template, list) or not 1 <= len(template) <= 64:
        raise ProvisioningError("argv_template invalide")
    parameter_names = set(adapter["input_contract"].get("parameters", {}))
    for token in template:
        validate_argv_token(token, parameter_names)
        if (token["kind"] == "parameter" and not token["flag"] and
                adapter["input_contract"]["parameters"][token["name"]]["type"] == "boolean"):
            raise ProvisioningError("booléen positionnel refusé")
    parser = adapter["parser"]
    _require_exact_keys(parser, {"kind"}, "parser")
    if parser["kind"] not in _PARSERS:
        raise ProvisioningError("parser généré ou inconnu refusé")
    capabilities = adapter["capabilities"]
    if not isinstance(capabilities, list) or not 1 <= len(capabilities) <= 16:
        raise ProvisioningError("capabilities invalides")
    normalized = dict(adapter)
    normalized_capabilities = [validate_capability(item, tool_id, request, adapter)
                               for item in capabilities]
    ids = [item["capability_id"] for item in normalized_capabilities]
    if len(ids) != len(set(ids)):
        raise ProvisioningError("capability_id dupliqué")
    normalized["capabilities"] = normalized_capabilities
    return normalized


def validate_contract_schema(schema: object, name: str) -> None:
    if not isinstance(schema, Mapping) or set(schema) - {"parameters", "artifacts", "outputs"}:
        raise ProvisioningError(f"{name} invalide")
    parameters = schema.get("parameters", {})
    if not isinstance(parameters, Mapping) or len(parameters) > 32:
        raise ProvisioningError("parameters_schema invalide")
    for parameter, definition in parameters.items():
        if not _SAFE_BINARY.fullmatch(str(parameter)) or not isinstance(definition, Mapping):
            raise ProvisioningError("paramètre invalide")
        parameter_type = definition.get("type")
        allowed_keys = {"type", "required"}
        if parameter_type == "enum":
            allowed_keys.add("values")
            if not isinstance(definition.get("values"), list) or not 1 <= len(definition["values"]) <= 32:
                raise ProvisioningError("enum invalide")
        elif parameter_type == "string":
            allowed_keys.add("max_length")
            if not isinstance(definition.get("max_length"), int) or not 1 <= definition["max_length"] <= 4096:
                raise ProvisioningError("string non bornée")
        elif parameter_type == "integer":
            allowed_keys.update({"minimum", "maximum"})
            if not all(isinstance(definition.get(key), int) for key in ("minimum", "maximum")) \
                    or definition["minimum"] > definition["maximum"]:
                raise ProvisioningError("integer non borné")
        elif parameter_type != "boolean":
            raise ProvisioningError("type de paramètre refusé")
        if set(definition) - allowed_keys or not isinstance(definition.get("required", False), bool):
            raise ProvisioningError("schéma de paramètre invalide")
    for key in ("artifacts", "outputs"):
        values = schema.get(key, [])
        if not isinstance(values, list) or len(values) > 16 or any(
                not isinstance(value, str) or not _SAFE_BINARY.fullmatch(value) for value in values):
            raise ProvisioningError(f"{key} invalides")


def validate_argv_token(token: object, parameter_names: set[str]) -> None:
    if not isinstance(token, Mapping) or token.get("kind") not in {
            "literal", "artifact", "parameter", "output"}:
        raise ProvisioningError("token argv invalide")
    kind = token["kind"]
    required = {"kind", "value"} if kind == "literal" else {"kind", "name"}
    if kind == "parameter":
        required |= {"flag"}
    _require_exact_keys(token, required, "token argv")
    value = token.get("value") if kind == "literal" else token.get("name")
    if not isinstance(value, str) or "\x00" in value or len(value) > 4096:
        raise ProvisioningError("valeur argv invalide")
    if kind == "literal" and (value.startswith(("/home", "~")) or ".." in Path(value).parts):
        raise ProvisioningError("chemin argv refusé")
    if kind != "literal" and not _SAFE_BINARY.fullmatch(value):
        raise ProvisioningError("nom argv invalide")
    if kind == "parameter":
        if value not in parameter_names or not isinstance(token["flag"], str) \
                or (token["flag"] and not re.fullmatch(
                    r"--[a-z][a-z0-9-]{0,39}", token["flag"])):
            raise ProvisioningError("liaison de paramètre argv invalide")


def validate_capability(capability: Mapping[str, object], tool_id: str,
                        request: Mapping[str, object], adapter: Mapping[str, object]) -> dict[str, object]:
    required = {"contract", "capability_id", "tool_id", "title", "category", "intent",
                "applicable_object_types", "input_binding", "parameters_schema", "output_kind",
                "risk_class", "network_contact", "authorization", "availability", "reason",
                "icon_key", "adapter_digest", "generation_id"}
    _require_exact_keys(capability, required, "capability manifest")
    if capability["contract"] != CAPABILITY_CONTRACT or capability["tool_id"] != tool_id:
        raise ProvisioningError("capability incompatible")
    _safe_id(capability["capability_id"], "capability_id")
    for key in ("title", "category", "intent", "output_kind", "authorization", "icon_key"):
        if not isinstance(capability[key], str) or not capability[key] or len(capability[key]) > 160:
            raise ProvisioningError(f"champ capability {key} invalide")
    if any(marker in str(capability[key]).lower() for key in ("title", "category", "intent", "icon_key")
           for marker in ("<script", "javascript:", "<style", "<html")):
        raise ProvisioningError("code UI dans manifest refusé")
    object_types = capability["applicable_object_types"]
    if not isinstance(object_types, list) or not object_types or any(
            value not in _OBJECT_TYPES for value in object_types):
        raise ProvisioningError("applicable_object_types invalides")
    if not isinstance(capability["input_binding"], Mapping) or set(capability["input_binding"]) != {
            "object_value_parameter"}:
        raise ProvisioningError("input_binding invalide")
    if capability["input_binding"]["object_value_parameter"] not in adapter["input_contract"].get(
            "parameters", {}):
        raise ProvisioningError("input_binding non déclaré")
    if capability["parameters_schema"] != adapter["input_contract"].get("parameters", {}):
        raise ProvisioningError("parameters_schema incohérent")
    if capability["risk_class"] not in _RISK_CLASSES or capability["network_contact"] != "NONE" \
            or capability["availability"] != "AVAILABLE":
        raise ProvisioningError("capability réseau/risque/disponibilité refusée")
    if not isinstance(capability["reason"], str) or len(capability["reason"]) > 500:
        raise ProvisioningError("reason invalide")
    if capability["generation_id"] != request["generation_id"]:
        raise ProvisioningError("generation_id capability invalide")
    expected_digest = _digest({key: value for key, value in adapter.items() if key != "capabilities"})
    if capability["adapter_digest"] != expected_digest:
        raise ProvisioningError("adapter_digest invalide")
    return dict(capability)


def materialize_argv(adapter: Mapping[str, object], parameters: Mapping[str, object],
                     artifacts: Mapping[str, bytes]) -> tuple[list[str], tuple[str, ...]]:
    if not isinstance(parameters, Mapping):
        raise ProvisioningError("parameters invalides")
    schema = adapter["input_contract"].get("parameters", {})
    if set(parameters) - set(schema):
        raise ProvisioningError("paramètre non déclaré")
    for name, definition in schema.items():
        if definition.get("required", False) and name not in parameters:
            raise ProvisioningError("paramètre requis absent")
        if name in parameters:
            _validate_parameter(parameters[name], definition)
    declared_artifacts = set(adapter["input_contract"].get("artifacts", []))
    if set(artifacts) - declared_artifacts:
        raise ProvisioningError("artefact non déclaré")
    argv: list[str] = []
    outputs = tuple(adapter["output_contract"].get("outputs", []))
    for token in adapter["argv_template"]:
        kind = token["kind"]
        if kind == "literal":
            argv.append(token["value"])
        elif kind == "artifact":
            if token["name"] not in artifacts:
                raise ProvisioningError("artefact argv absent")
            argv.append(f"/input/{token['name']}")
        elif kind == "output":
            if token["name"] not in outputs:
                raise ProvisioningError("sortie argv inconnue")
            argv.append(f"/output/{token['name']}")
        else:
            name = token["name"]
            if name not in parameters:
                continue
            value = parameters[name]
            if schema[name]["type"] == "boolean":
                if value:
                    if not token["flag"]:
                        raise ProvisioningError("booléen positionnel refusé")
                    argv.append(token["flag"])
            else:
                if token["flag"]:
                    argv.append(token["flag"])
                argv.append(str(value))
    return argv, outputs


def _validate_parameter(value: object, definition: Mapping[str, object]) -> None:
    kind = definition["type"]
    if kind == "enum" and (not isinstance(value, str) or value not in definition["values"]):
        raise ProvisioningError("valeur enum invalide")
    if kind == "string" and (not isinstance(value, str) or "\x00" in value or
                             len(value) > definition["max_length"]):
        raise ProvisioningError("valeur string invalide")
    if kind == "integer" and (isinstance(value, bool) or not isinstance(value, int) or
                              not definition["minimum"] <= value <= definition["maximum"]):
        raise ProvisioningError("valeur integer invalide")
    if kind == "boolean" and not isinstance(value, bool):
        raise ProvisioningError("valeur boolean invalide")
