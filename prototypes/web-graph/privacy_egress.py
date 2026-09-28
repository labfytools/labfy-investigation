"""Supervision d'egress explicite, sans chemin réseau direct de secours."""

from __future__ import annotations

import ipaddress
import json
import os
import re
import subprocess
import tempfile
import time
import uuid
from dataclasses import dataclass
from enum import Enum
from pathlib import Path
from typing import Callable, Mapping, Protocol, Sequence
from urllib.parse import urljoin, urlsplit, urlunsplit


class EgressMode(str, Enum):
    PRIVACY_TOR = "PRIVACY_TOR"
    LOOPBACK_EXPLICIT = "LOOPBACK_EXPLICIT"


class EgressStatus(str, Enum):
    SUCCESS = "SUCCESS"
    PRIVACY_EGRESS_UNAVAILABLE = "PRIVACY_EGRESS_UNAVAILABLE"
    REJECTED = "REJECTED"
    TRANSPORT_ERROR = "TRANSPORT_ERROR"
    RESPONSE_LIMIT = "RESPONSE_LIMIT"


class TorRuntimeState(str, Enum):
    STOPPED = "STOPPED"
    STARTING = "STARTING"
    BOOTSTRAPPING = "BOOTSTRAPPING"
    READY = "READY"
    FAILED = "FAILED"


class ResponseLimitError(OSError):
    pass


@dataclass(frozen=True)
class TransportResponse:
    status_code: int
    headers: Mapping[str, str]
    body: bytes
    peer_ip: str


class EgressTransport(Protocol):
    def request(self, method: str, url: str, headers: Mapping[str, str],
                timeout_seconds: float, max_body_bytes: int) -> TransportResponse:
        """Effectue une requête via l'egress déjà configuré."""


@dataclass(frozen=True)
class FetchResult:
    status: EgressStatus
    final_url: str | None
    status_code: int | None
    headers: Mapping[str, str]
    body: bytes
    provenance: Mapping[str, object]


class URLValidationError(ValueError):
    pass


class CommandRunner(Protocol):
    def __call__(self, argv: Sequence[str], timeout: float) -> subprocess.CompletedProcess[str]:
        """Exécute un argv sans shell et retourne stdout/stderr bornés par l'appelant."""


def _run_command(argv: Sequence[str], timeout: float) -> subprocess.CompletedProcess[str]:
    environment = {
        "HOME": os.environ.get("HOME", "/nonexistent"),
        "PATH": os.environ.get("PATH", "/usr/bin:/bin"),
        "XDG_RUNTIME_DIR": os.environ.get("XDG_RUNTIME_DIR", ""),
    }
    return subprocess.run(
        list(argv), check=False, capture_output=True, text=True, timeout=timeout,
        env=environment,
    )


class CurlSocksTransport:
    """Transport HTTP(S) par SOCKS5h : la résolution DNS est faite par Tor."""

    uses_remote_dns = True

    def __init__(self, host: str, port: int, curl_path: str = "/usr/bin/curl",
                 runner: CommandRunner = _run_command):
        if host != "127.0.0.1" or not 1 <= port <= 65535:
            raise ValueError("endpoint SOCKS invalide")
        self._proxy = f"socks5h://{host}:{port}"
        self._curl_path = curl_path
        self._runner = runner

    def request(self, method: str, url: str, headers: Mapping[str, str],
                timeout_seconds: float, max_body_bytes: int) -> TransportResponse:
        with tempfile.TemporaryDirectory(prefix="labfy-tor-fetch-") as directory:
            root = Path(directory)
            body_path = root / "body"
            header_path = root / "headers"
            argv = [
                self._curl_path, "--silent", "--show-error", "--request", method,
                "--proxy", self._proxy, "--proto", "=http,https",
                "--proto-redir", "=http,https", "--max-redirs", "0",
                "--connect-timeout", str(min(timeout_seconds, 30.0)),
                "--max-time", str(timeout_seconds), "--max-filesize", str(max_body_bytes),
                "--dump-header", str(header_path), "--output", str(body_path),
                "--write-out", "%{http_code}\n%{remote_ip}",
            ]
            for name, value in headers.items():
                argv.extend(("--header", f"{name}: {value}"))
            argv.extend(("--", url))
            completed = self._runner(argv, timeout_seconds + 5.0)
            body = body_path.read_bytes() if body_path.is_file() else b""
            if completed.returncode == 63 or len(body) > max_body_bytes:
                raise ResponseLimitError("réponse supérieure au plafond")
            if completed.returncode != 0:
                raise OSError("échec du transport SOCKS Tor")
            output = completed.stdout.splitlines()
            if len(output) != 2 or not output[0].isdigit():
                raise OSError("métadonnées curl invalides")
            peer_ip = output[1].strip()
            if not ipaddress.ip_address(peer_ip).is_loopback:
                raise OSError("curl n'a pas joint le proxy loopback attendu")
            return TransportResponse(
                int(output[0]), self._read_headers(header_path), body, peer_ip,
            )

    def check_tor_egress(self, url: str, timeout_seconds: float = 20.0) -> bool:
        response = self.request("GET", url, {"Accept": "application/json"},
                                timeout_seconds, 4096)
        try:
            payload = json.loads(response.body.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError):
            return False
        # L'adresse éventuellement renvoyée n'est ni conservée, ni journalisée.
        return response.status_code == 200 and payload.get("IsTor") is True

    @staticmethod
    def _read_headers(path: Path) -> dict[str, str]:
        if not path.is_file() or path.stat().st_size > 64 * 1024:
            raise OSError("en-têtes absents ou trop volumineux")
        headers: dict[str, str] = {}
        for line in path.read_text(encoding="iso-8859-1").splitlines()[1:]:
            if not line or ":" not in line:
                continue
            name, value = line.split(":", 1)
            headers[name.strip()] = value.strip()
        return headers


class PodmanTorRuntime:
    """Possède un unique conteneur Tor rootless et refuse toute ressource étrangère."""

    OWNER_LABEL = "io.labfy.privacy-egress.owner"
    IMAGE = "localhost/labfy-privacy-tor:1"
    HEALTH_URL = "https://check.torproject.org/api/ip"

    def __init__(self, image: str = IMAGE, podman_path: str = "/usr/bin/podman",
                 curl_path: str = "/usr/bin/curl", runner: CommandRunner = _run_command,
                 sleeper: Callable[[float], None] = time.sleep):
        self.state = TorRuntimeState.STOPPED
        self.reason = "NOT_STARTED"
        self.transport: CurlSocksTransport | None = None
        self._image = image
        self._podman = podman_path
        self._curl = curl_path
        self._runner = runner
        self._sleeper = sleeper
        self._owner = uuid.uuid4().hex
        self._container_name = f"labfy-privacy-tor-{self._owner[:12]}"
        self._created = False

    @property
    def container_name(self) -> str:
        return self._container_name

    def start(self, bootstrap_timeout: float = 90.0) -> CurlSocksTransport:
        if self.state != TorRuntimeState.STOPPED:
            raise RuntimeError("runtime Tor déjà démarré")
        if not 1.0 <= bootstrap_timeout <= 300.0:
            raise ValueError("délai bootstrap invalide")
        self.state = TorRuntimeState.STARTING
        try:
            info = self._call(
                [self._podman, "info", "--format", "{{.Host.Security.Rootless}}"], 10.0,
            )
            if info.returncode != 0 or info.stdout.strip().casefold() != "true":
                raise OSError("Podman rootless indisponible")
            started = self._call(self._container_argv(), 30.0)
            if started.returncode != 0:
                raise OSError("création du conteneur Tor impossible")
            self._created = True
            self.state = TorRuntimeState.BOOTSTRAPPING
            self._wait_for_bootstrap(bootstrap_timeout)
            port = self._published_port()
            transport = CurlSocksTransport("127.0.0.1", port, self._curl, self._runner)
            if not transport.check_tor_egress(self.HEALTH_URL):
                raise OSError("contrôle d'egress Tor négatif")
            self.transport = transport
            self.state = TorRuntimeState.READY
            self.reason = "TOR_EGRESS_VERIFIED"
            return transport
        except (OSError, subprocess.TimeoutExpired, ValueError) as error:
            self.reason = str(error)[:256]
            self.state = TorRuntimeState.FAILED
            self._remove_owned()
            raise OSError(self.reason) from error

    def close(self) -> None:
        self.transport = None
        self._remove_owned()
        self.state = TorRuntimeState.STOPPED
        self.reason = "STOPPED"

    def _container_argv(self) -> list[str]:
        return [
            self._podman, "run", "--detach", "--name", self._container_name,
            "--label", f"{self.OWNER_LABEL}={self._owner}",
            "--publish", "127.0.0.1::9050", "--read-only", "--cap-drop", "ALL",
            "--security-opt", "no-new-privileges", "--pids-limit", "128",
            "--memory", "256m", "--tmpfs",
            "/tmp:rw,noexec,nosuid,nodev,size=80m,mode=1777",
            self._image,
        ]

    def _wait_for_bootstrap(self, timeout_seconds: float) -> None:
        deadline = time.monotonic() + timeout_seconds
        while time.monotonic() < deadline:
            status = self._call(
                [self._podman, "inspect", "--format", "{{.State.Running}}",
                 self._container_name], 5.0,
            )
            if status.returncode != 0 or status.stdout.strip().casefold() != "true":
                raise OSError("conteneur Tor arrêté pendant le bootstrap")
            logs = self._call([self._podman, "logs", "--tail", "80", self._container_name], 5.0)
            if "Bootstrapped 100% (done)" in (logs.stdout + logs.stderr):
                return
            self._sleeper(0.25)
        raise OSError("bootstrap Tor expiré")

    def _published_port(self) -> int:
        result = self._call(
            [self._podman, "port", self._container_name, "9050/tcp"], 5.0,
        )
        match = re.fullmatch(r"127\.0\.0\.1:(\d+)\s*", result.stdout)
        if result.returncode != 0 or match is None:
            raise OSError("publication SOCKS non loopback ou absente")
        port = int(match.group(1))
        if not 1 <= port <= 65535:
            raise OSError("port SOCKS invalide")
        return port

    def _remove_owned(self) -> None:
        if not self._created:
            return
        inspected = self._call(
            [self._podman, "inspect", "--format",
             f'{{{{ index .Config.Labels "{self.OWNER_LABEL}" }}}}', self._container_name], 5.0,
        )
        if inspected.returncode == 0 and inspected.stdout.strip() == self._owner:
            removed = self._call([self._podman, "rm", "--force", self._container_name], 15.0)
            if removed.returncode == 0:
                self._created = False
        elif inspected.returncode == 0:
            self._created = False

    def _call(self, argv: Sequence[str], timeout: float) -> subprocess.CompletedProcess[str]:
        return self._runner(argv, timeout)


class PrivacyEgressSupervisor:
    """Utilise uniquement un transport injecté ; il n'instancie aucun client direct."""

    def __init__(self, privacy_transport: EgressTransport | None = None,
                 loopback_transport: EgressTransport | None = None):
        self._privacy_transport = privacy_transport
        self._loopback_transport = loopback_transport
        self._owned_resources: list[object] = []

    @classmethod
    def start_owned_tor(cls, runtime: PodmanTorRuntime | None = None,
                        bootstrap_timeout: float = 90.0) -> "PrivacyEgressSupervisor":
        owned_runtime = runtime or PodmanTorRuntime()
        transport = owned_runtime.start(bootstrap_timeout=bootstrap_timeout)
        supervisor = cls(privacy_transport=transport)
        supervisor.register_owned_resource(owned_runtime)
        return supervisor

    def register_owned_resource(self, resource: object) -> None:
        """Enregistre seulement une ressource créée par ce superviseur/caller dédié."""
        if resource not in self._owned_resources:
            self._owned_resources.append(resource)

    def is_available(self, mode: EgressMode = EgressMode.PRIVACY_TOR) -> bool:
        transport = (
            self._privacy_transport if mode == EgressMode.PRIVACY_TOR else self._loopback_transport
        )
        return transport is not None

    def close(self) -> None:
        for resource in reversed(self._owned_resources):
            close = getattr(resource, "close", None)
            if callable(close):
                close()
        self._owned_resources.clear()
        self._privacy_transport = None
        self._loopback_transport = None

    @staticmethod
    def validate_url(url: str, allow_loopback: bool = False) -> str:
        if not isinstance(url, str) or not url or len(url) > 4096 or "\x00" in url:
            raise URLValidationError("URL invalide")
        if url != url.strip() or any(ord(character) < 0x20 for character in url):
            raise URLValidationError("URL contient des contrôles")
        parsed = urlsplit(url)
        if parsed.scheme not in ("http", "https") or not parsed.netloc:
            raise URLValidationError("seuls HTTP et HTTPS sont permis")
        if parsed.username is not None or parsed.password is not None or parsed.fragment:
            raise URLValidationError("userinfo ou fragment refusé")
        try:
            port = parsed.port
        except ValueError as error:
            raise URLValidationError("port invalide") from error
        if port is not None and not 1 <= port <= 65535:
            raise URLValidationError("port invalide")
        hostname = (parsed.hostname or "").rstrip(".").casefold()
        if not hostname or len(hostname) > 253:
            raise URLValidationError("hôte invalide")
        blocked_names = {
            "localhost", "localhost.localdomain", "metadata.google.internal",
            "metadata.aws.internal", "instance-data.ec2.internal",
        }
        if hostname in blocked_names or hostname.endswith(".localhost"):
            if not (allow_loopback and hostname == "localhost"):
                raise URLValidationError("hôte local ou metadata refusé")
        try:
            address = ipaddress.ip_address(hostname.strip("[]"))
        except ValueError:
            if not re.fullmatch(r"[a-z0-9](?:[a-z0-9.-]{0,251}[a-z0-9])?", hostname):
                raise URLValidationError("nom d'hôte invalide")
        else:
            PrivacyEgressSupervisor._validate_address(address, allow_loopback)
        canonical_host = f"[{hostname}]" if ":" in hostname else hostname
        if port is not None:
            canonical_host += f":{port}"
        return urlunsplit((parsed.scheme, canonical_host, parsed.path or "/", parsed.query, ""))

    @staticmethod
    def _validate_address(address: ipaddress.IPv4Address | ipaddress.IPv6Address,
                          allow_loopback: bool) -> None:
        if allow_loopback and address.is_loopback:
            return
        # INVARIANT: certains runtimes qualifient le multicast de global ; chaque classe est refusée.
        if (not address.is_global or address.is_loopback or address.is_private
                or address.is_link_local or address.is_multicast or address.is_reserved
                or address.is_unspecified):
            raise URLValidationError("adresse non publique refusée")

    def fetch(self, url: str, method: str = "GET", mode: EgressMode = EgressMode.PRIVACY_TOR,
              timeout_seconds: float = 15.0, max_body_bytes: int = 1024 * 1024,
              max_redirects: int = 3) -> FetchResult:
        if method not in ("GET", "HEAD"):
            return self._failure(EgressStatus.REJECTED, mode, "METHOD_REJECTED")
        if not 0 < timeout_seconds <= 120 or not 0 <= max_redirects <= 10:
            return self._failure(EgressStatus.REJECTED, mode, "LIMITS_INVALID")
        if not 1 <= max_body_bytes <= 16 * 1024 * 1024:
            return self._failure(EgressStatus.REJECTED, mode, "LIMITS_INVALID")
        transport = (
            self._privacy_transport if mode == EgressMode.PRIVACY_TOR else self._loopback_transport
        )
        if transport is None:
            return self._failure(
                EgressStatus.PRIVACY_EGRESS_UNAVAILABLE, mode, "PRIVACY_EGRESS_UNAVAILABLE"
            )
        allow_loopback = mode == EgressMode.LOOPBACK_EXPLICIT
        try:
            current_url = self.validate_url(url, allow_loopback)
        except URLValidationError:
            return self._failure(EgressStatus.REJECTED, mode, "URL_REJECTED")
        chain = []
        headers = {"Accept": "*/*", "User-Agent": "Labfy-Research/1"}
        for redirect_count in range(max_redirects + 1):
            try:
                response = transport.request(
                    method, current_url, headers, timeout_seconds, max_body_bytes + 1
                )
                peer = ipaddress.ip_address(response.peer_ip)
                remote_dns_proxy = bool(getattr(transport, "uses_remote_dns", False))
                if remote_dns_proxy:
                    if mode != EgressMode.PRIVACY_TOR or not peer.is_loopback:
                        raise URLValidationError("proxy privacy inattendu")
                else:
                    self._validate_address(peer, allow_loopback)
            except ResponseLimitError:
                return FetchResult(
                    EgressStatus.RESPONSE_LIMIT, current_url, None, {}, b"",
                    self._provenance(mode, "RESPONSE_LIMIT", chain),
                )
            except (OSError, ValueError, URLValidationError):
                return self._failure(EgressStatus.TRANSPORT_ERROR, mode, "TRANSPORT_REJECTED", chain)
            body = bytes(response.body)
            if len(body) > max_body_bytes:
                return FetchResult(
                    EgressStatus.RESPONSE_LIMIT, current_url, response.status_code, {}, b"",
                    self._provenance(mode, "RESPONSE_LIMIT", chain),
                )
            chain.append({
                "url": current_url,
                "status_code": response.status_code,
                "peer_ip": "privacy-proxy" if remote_dns_proxy else str(peer),
            })
            location = self._header(response.headers, "location")
            if response.status_code in (301, 302, 303, 307, 308) and location:
                if redirect_count == max_redirects:
                    return self._failure(EgressStatus.REJECTED, mode, "REDIRECT_LIMIT", chain)
                try:
                    current_url = self.validate_url(urljoin(current_url, location), allow_loopback)
                except URLValidationError:
                    return self._failure(EgressStatus.REJECTED, mode, "REDIRECT_REJECTED", chain)
                continue
            safe_headers = {
                key.lower(): str(value)[:1024] for key, value in response.headers.items()
                if key.lower() in ("content-type", "content-length", "last-modified", "etag")
            }
            return FetchResult(
                EgressStatus.SUCCESS, current_url, response.status_code, safe_headers,
                b"" if method == "HEAD" else body,
                self._provenance(mode, "COMPLETED", chain),
            )
        return self._failure(EgressStatus.REJECTED, mode, "REDIRECT_LIMIT", chain)

    @staticmethod
    def _header(headers: Mapping[str, str], name: str) -> str | None:
        for key, value in headers.items():
            if key.casefold() == name:
                return str(value)
        return None

    def _failure(self, status: EgressStatus, mode: EgressMode, reason: str,
                 chain: list[dict[str, object]] | None = None) -> FetchResult:
        return FetchResult(status, None, None, {}, b"", self._provenance(mode, reason, chain or []))

    @staticmethod
    def _provenance(mode: EgressMode, reason: str,
                    chain: list[dict[str, object]]) -> dict[str, object]:
        return {
            "contract": "labfy.privacy_egress.v1",
            "mode": mode.value,
            "reason": reason,
            "redirect_chain": tuple(chain),
            "direct_fallback": False,
        }
