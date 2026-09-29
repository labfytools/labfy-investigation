"""Controlled Level 3 code changes prepared in isolated Git worktrees.

The service deliberately exposes repository operations, rather than a shell.  Human
decisions are persisted separately from model supplied proposal text and every path
is resolved below the detached development worktree.
"""

from __future__ import annotations

import hashlib
import json
import os
import re
import subprocess
import tempfile
import threading
import time
import uuid
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
from typing import Callable, Mapping, Sequence


CONTRACT = "labfy.code_change_proposal.v1"
STATES = frozenset({
    "PROPOSED", "WAITING_DEV_APPROVAL", "DEV_REJECTED",
    "PREPARING_WORKTREE", "EDITING", "TESTING", "READY_FOR_REVIEW",
    "WAITING_APPLY_APPROVAL", "APPLY_REJECTED", "APPLYING",
    "APPLIED_LOCAL", "FAILED", "ROLLED_BACK",
})
PROPOSAL_FIELDS = frozenset({
    "contract", "change_id", "workspace_context", "purpose",
    "why_declarative_insufficient", "affected_areas", "expected_files",
    "required_tests", "risk", "created_at", "state",
})
ALLOWED_SUFFIXES = frozenset({
    ".py", ".js", ".mjs", ".html", ".css", ".md", ".json", ".sql",
    ".c", ".h",
})
ALLOWED_BASENAMES = frozenset({"Makefile", "meson.build"})
FORBIDDEN_PARTS = frozenset({".git", ".hg", ".svn", "node_modules", "vendor"})
FORBIDDEN_NAMES = frozenset({"AGENTS.md", ".gitmodules", ".env"})
SECRET_PATTERNS = (
    re.compile(rb"-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----"),
    re.compile(rb"(?i)(?:api[_-]?key|secret|password|token)\s*[:=]\s*['\"]?[A-Za-z0-9_+/=-]{16,}"),
    re.compile(rb"\bAKIA[0-9A-Z]{16}\b"),
    re.compile(rb"\bghp_[A-Za-z0-9]{30,}\b"),
)


class CodeChangeError(RuntimeError):
    """A stable, presentation-safe service error."""

    def __init__(self, code: str, message: str):
        super().__init__(message)
        self.code = code


@dataclass(frozen=True)
class DevLimits:
    max_files: int = 25
    max_edit_operations: int = 200
    max_model_calls: int = 60
    max_test_runs: int = 30
    max_file_bytes: int = 1_000_000
    max_read_bytes: int = 128_000
    max_search_results: int = 200
    max_search_files: int = 2_000
    max_search_bytes: int = 16_000_000
    max_diff_bytes: int = 2_000_000
    max_changed_lines: int = 5_000
    max_test_output_bytes: int = 128_000
    test_timeout_seconds: int = 900
    max_wall_seconds: int = 7_200


DEFAULT_TEST_RECIPES = {
    "PY_COMPILE": ("python3", "-m", "compileall", "-q", "prototypes/web-graph"),
    "PYTHON_TARGETED": ("python3", "-m", "unittest", "-v",
                        "prototypes/web-graph/tests/test_code_change.py",
                        "prototypes/web-graph/tests/test_developer_agent.py"),
    "PYTHON_FULL": ("python3", "-m", "unittest", "discover", "-s",
                    "prototypes/web-graph/tests", "-p", "test_*.py", "-v"),
    "NODE_TEST": ("npm", "test", "--prefix", "prototypes/web-graph"),
    "NODE_CHECK": ("node", "--check", "prototypes/web-graph/public/app.js"),
    "C_BUILD": ("make", "-j8"),
    "C_TEST": ("make", "-j8", "test"),
    "SOURCE_SIZE": ("make", "check-source-size"),
    # WHY: make's default target builds only local-jobs. The canonical Firefox
    # suite also invokes these four ignored fixture binaries in a fresh
    # detached worktree; prepare them with a fixed argv before that suite.
    "WEB_FIXTURE_BUILD": ("make", "-j8", "tools/local-jobs", "tools/local-jobs-test",
                          "core-graph-demo", "eml-graph-demo", "local-toolkit-demo"),
    "FIREFOX_TARGETED": ("node", "prototypes/web-graph/tests/test_local_model_agent_browser.mjs"),
    "FIREFOX_FULL": ("npm", "run", "test:browser", "--prefix", "prototypes/web-graph"),
    "DIFF_CHECK": ("git", "diff", "--check"),
}


def _json_clone(value):
    return json.loads(json.dumps(value, ensure_ascii=False))


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


class CodeChangeService:
    """Persistent authority for bounded developer worktree operations."""

    def __init__(
        self,
        repo_root: Path | str,
        state_root: Path | str,
        *,
        test_recipes: Mapping[str, Sequence[str]] | None = None,
        clock: Callable[[], float] | None = None,
        limits: DevLimits | None = None,
    ):
        self.repo_root = Path(repo_root).resolve(strict=True)
        self.state_root = Path(state_root).resolve()
        self.worktree_root = self.state_root / "dev-worktrees"
        self.record_root = self.state_root / "code-changes"
        self.patch_root = self.state_root / "patches"
        self.clock = clock or time.time
        self.limits = limits or DevLimits()
        recipes = test_recipes if test_recipes is not None else DEFAULT_TEST_RECIPES
        self.enforce_surface_tests = test_recipes is None
        self.test_recipes = self._validate_recipes(recipes)
        self._lock = threading.RLock()
        if self._git("rev-parse", "--show-toplevel").stdout.strip() != str(self.repo_root):
            raise CodeChangeError("INVALID_REPOSITORY", "repo_root must be a Git worktree root")
        for directory in (self.state_root, self.worktree_root, self.record_root, self.patch_root):
            directory.mkdir(mode=0o700, parents=True, exist_ok=True)
            os.chmod(directory, 0o700)
        self._recover_interrupted_states()

    @staticmethod
    def _validate_recipes(recipes):
        result = {}
        for recipe_id, argv in recipes.items():
            if (not isinstance(recipe_id, str) or not re.fullmatch(r"[A-Z][A-Z0-9_]{1,63}", recipe_id)
                    or not isinstance(argv, (list, tuple)) or not argv
                    or any(not isinstance(arg, str) or "\x00" in arg for arg in argv)):
                raise CodeChangeError("INVALID_TEST_RECIPE", "test recipes require fixed backend argv")
            result[recipe_id] = tuple(argv)
        return result

    def _git(self, *args, cwd=None, input_bytes=None, check=True, timeout=60):
        try:
            return subprocess.run(
                ("git", *args), cwd=str(cwd or self.repo_root), input=input_bytes,
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=check,
                timeout=timeout, text=input_bytes is None,
            )
        except (subprocess.CalledProcessError, subprocess.TimeoutExpired, OSError) as exc:
            stderr = getattr(exc, "stderr", b"") or b""
            if isinstance(stderr, bytes):
                stderr = stderr.decode("utf-8", "replace")
            raise CodeChangeError("GIT_FAILED", str(stderr).strip() or "Git operation failed") from exc

    def _record_path(self, change_id):
        self._validate_change_id(change_id)
        return self.record_root / f"{change_id}.json"

    @staticmethod
    def _validate_change_id(change_id):
        if not isinstance(change_id, str) or not re.fullmatch(r"[a-zA-Z0-9][a-zA-Z0-9._-]{0,63}", change_id):
            raise CodeChangeError("INVALID_CHANGE_ID", "invalid change_id")

    def _load(self, change_id):
        path = self._record_path(change_id)
        try:
            value = json.loads(path.read_text(encoding="utf-8"))
        except FileNotFoundError as exc:
            raise CodeChangeError("NOT_FOUND", "unknown code change") from exc
        except (OSError, json.JSONDecodeError) as exc:
            raise CodeChangeError("STATE_CORRUPT", "code change state is unreadable") from exc
        if value.get("contract") != CONTRACT or value.get("state") not in STATES:
            raise CodeChangeError("STATE_CORRUPT", "code change state has an invalid contract")
        return value

    def _save(self, record):
        path = self._record_path(record["change_id"])
        data = json.dumps(record, ensure_ascii=False, sort_keys=True, indent=2).encode("utf-8") + b"\n"
        fd, temporary = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
        try:
            os.fchmod(fd, 0o600)
            with os.fdopen(fd, "wb") as stream:
                stream.write(data)
                stream.flush()
                os.fsync(stream.fileno())
            os.replace(temporary, path)
            os.chmod(path, 0o600)
        finally:
            if os.path.exists(temporary):
                os.unlink(temporary)

    def _recover_interrupted_states(self):
        for path in self.record_root.glob("*.json"):
            try:
                record = json.loads(path.read_text(encoding="utf-8"))
            except (OSError, json.JSONDecodeError):
                continue
            state = record.get("state")
            if state == "PREPARING_WORKTREE":
                worktree = Path(record.get("worktree_path", ""))
                record["state"] = "EDITING" if worktree.is_dir() else "FAILED"
                record["failure"] = None if worktree.is_dir() else "PREPARE_INTERRUPTED"
                self._save(record)
            elif state == "TESTING":
                record["state"] = "EDITING"
                record["recovery"] = "TEST_INTERRUPTED"
                self._save(record)
            elif state == "APPLYING":
                record["state"], record["failure"] = self._recover_apply(record)
                self._save(record)

    def _recover_apply(self, record):
        """Never replay apply; reverse it only when Git proves it fully landed."""
        patch_path = Path(record.get("patch_path", ""))
        if not patch_path.is_file() or self._git("rev-parse", "HEAD").stdout.strip() != record.get("base_sha"):
            return "FAILED", "APPLY_INTERRUPTED_REVIEW_REQUIRED"
        patch = patch_path.read_bytes()
        if _sha256(patch) != record.get("patch_digest"):
            return "FAILED", "APPLY_INTERRUPTED_REVIEW_REQUIRED"
        reverse = self._git("apply", "--check", "--reverse", "--binary", "-",
                            input_bytes=patch, check=False)
        if reverse.returncode == 0:
            applied = self._git("apply", "--reverse", "--binary", "-", input_bytes=patch,
                                check=False)
            if applied.returncode == 0:
                return "ROLLED_BACK", "APPLY_INTERRUPTED_ROLLED_BACK"
        return "FAILED", "APPLY_INTERRUPTED_REVIEW_REQUIRED"

    def _now(self):
        return int(self.clock())

    def _check_idempotency(self, record, action, key, fingerprint):
        if not isinstance(key, str) or len(key) > 128:
            raise CodeChangeError("INVALID_IDEMPOTENCY_KEY", "idempotency key is required")
        try:
            uuid.UUID(key)
        except (ValueError, AttributeError) as exc:
            raise CodeChangeError("INVALID_IDEMPOTENCY_KEY", "idempotency key must be a UUID") from exc
        previous = record.setdefault("idempotency", {}).get(key)
        if previous:
            if previous != {"action": action, "fingerprint": fingerprint}:
                raise CodeChangeError("IDEMPOTENCY_CONFLICT", "idempotency key was reused")
            return True
        record["idempotency"][key] = {"action": action, "fingerprint": fingerprint}
        return False

    @staticmethod
    def _human(actor):
        if not isinstance(actor, str) or not actor.strip() or len(actor) > 128:
            raise CodeChangeError("HUMAN_DECISION_REQUIRED", "a human actor is required")
        if actor.strip().lower() in {"qwen", "model", "agent", "developer_agent"}:
            raise CodeChangeError("MODEL_SELF_APPROVAL", "the model cannot approve its own change")
        return actor.strip()

    def propose(self, proposal: Mapping, *, idempotency_key: str | None = None):
        with self._lock:
            if not isinstance(proposal, Mapping) or set(proposal) != PROPOSAL_FIELDS:
                raise CodeChangeError("INVALID_PROPOSAL", "proposal fields do not match the strict contract")
            value = _json_clone(dict(proposal))
            if value["contract"] != CONTRACT or value["state"] not in {"PROPOSED", "WAITING_DEV_APPROVAL"}:
                raise CodeChangeError("INVALID_PROPOSAL", "invalid proposal contract or initial state")
            self._validate_change_id(value["change_id"])
            for name in ("workspace_context", "purpose", "why_declarative_insufficient", "risk", "created_at"):
                if not isinstance(value[name], str) or not value[name].strip() or len(value[name]) > 4000:
                    raise CodeChangeError("INVALID_PROPOSAL", f"{name} must be a non-empty bounded string")
            reason = value["why_declarative_insufficient"].strip()
            if len(reason) < 20 or reason.upper() in {"N/A", "NONE", "UNKNOWN"}:
                return {"classification": "DECLARATIVE_SUFFICIENT", "accepted": False,
                        "reason": "declarative insufficiency was not demonstrated"}
            for name in ("affected_areas", "expected_files", "required_tests"):
                if (not isinstance(value[name], list) or not value[name]
                        or any(not isinstance(item, str) or not item.strip() for item in value[name])):
                    raise CodeChangeError("INVALID_PROPOSAL", f"{name} must be a non-empty string list")
            if len(value["expected_files"]) > self.limits.max_files or len(set(value["expected_files"])) != len(value["expected_files"]):
                raise CodeChangeError("SCOPE_LIMIT", "expected file scope exceeds the configured limit")
            value["expected_files"] = [self._validate_relative_path(path).as_posix()
                                       for path in value["expected_files"]]
            unknown_tests = set(value["required_tests"]) - self.test_recipes.keys()
            if unknown_tests:
                raise CodeChangeError("UNKNOWN_TEST_RECIPE", "proposal requests an unknown test recipe")
            value["proposed_tests"] = list(value["required_tests"])
            if self.enforce_surface_tests:
                value["required_tests"] = self._surface_test_requirements(
                    value["expected_files"], value["required_tests"])
            path = self._record_path(value["change_id"])
            if path.exists():
                existing = self._load(value["change_id"])
                if existing.get("proposal_fingerprint") == _sha256(json.dumps(value, sort_keys=True).encode()):
                    return self._public(existing)
                raise CodeChangeError("CHANGE_ID_CONFLICT", "change_id already exists")
            if idempotency_key is None:
                idempotency_key = str(uuid.uuid4())
            base_sha = self._git("rev-parse", "HEAD").stdout.strip()
            value.update({
                "state": "WAITING_DEV_APPROVAL", "base_sha": base_sha,
                "proposal_fingerprint": _sha256(json.dumps(value, sort_keys=True).encode()),
                "idempotency": {}, "decisions": [], "owned_files": [],
                "edit_operations": 0, "model_calls": 0, "test_runs": 0, "tests": [],
                "preview": None, "created_epoch": self._now(),
            })
            self._check_idempotency(value, "propose", idempotency_key, value["proposal_fingerprint"])
            self._save(value)
            return self._public(value)

    @staticmethod
    def _surface_test_requirements(paths, proposed):
        required = set(proposed)
        required.add("DIFF_CHECK")
        suffixes = {PurePosixPath(path).suffix.lower() for path in paths}
        basenames = {PurePosixPath(path).name for path in paths}
        if suffixes & {".py"}:
            required.update({"PY_COMPILE", "PYTHON_TARGETED", "PYTHON_FULL"})
        if suffixes & {".js", ".mjs", ".html", ".css"}:
            required.update({"NODE_CHECK", "NODE_TEST", "WEB_FIXTURE_BUILD",
                             "FIREFOX_TARGETED", "FIREFOX_FULL"})
        if suffixes & {".c", ".h"} or basenames & {"Makefile", "meson.build"}:
            required.update({"C_BUILD", "SOURCE_SIZE", "C_TEST"})
        return [recipe_id for recipe_id in DEFAULT_TEST_RECIPES if recipe_id in required]

    def approve_prepare(self, change_id, *, actor, idempotency_key=None, decision_id=None):
        key = idempotency_key or decision_id
        with self._lock:
            record = self._load(change_id)
            fingerprint = _sha256(f"approve_prepare:{self._human(actor)}".encode())
            if self._check_idempotency(record, "approve_prepare", key, fingerprint):
                return self._public(record)
            if record["state"] != "WAITING_DEV_APPROVAL":
                raise CodeChangeError("INVALID_STATE", "prepare approval is not currently accepted")
            record["decisions"].append({"gate": "C1", "decision": "APPROVED",
                                        "actor": actor, "at": self._now(), "idempotency_key": key})
            record["state"] = "PREPARING_WORKTREE"
            worktree = self.worktree_root / change_id
            record["worktree_path"] = str(worktree)
            self._save(record)
            try:
                if worktree.exists():
                    raise CodeChangeError("WORKTREE_EXISTS", "development worktree path already exists")
                self._git("worktree", "add", "--detach", str(worktree), record["base_sha"], timeout=120)
                actual = self._git("rev-parse", "HEAD", cwd=worktree).stdout.strip()
                if actual != record["base_sha"]:
                    raise CodeChangeError("STALE_BASE", "development worktree has the wrong baseline")
                record["state"] = "EDITING"
                record["started_at"] = self._now()
                self._save(record)
            except Exception as exc:
                record["state"] = "FAILED"
                record["failure"] = getattr(exc, "code", "PREPARE_FAILED")
                self._save(record)
                raise
            return self._public(record)

    def reject_prepare(self, change_id, *, actor, reason, idempotency_key=None, decision_id=None):
        return self._reject(change_id, "C1", "WAITING_DEV_APPROVAL", "DEV_REJECTED",
                            actor, reason, idempotency_key or decision_id)

    def _reject(self, change_id, gate, expected, target, actor, reason, key):
        with self._lock:
            record = self._load(change_id)
            actor = self._human(actor)
            if not isinstance(reason, str) or not reason.strip() or len(reason) > 2000:
                raise CodeChangeError("INVALID_REASON", "a bounded rejection reason is required")
            fingerprint = _sha256(f"{gate}:{actor}:{reason}".encode())
            if self._check_idempotency(record, f"reject_{gate}", key, fingerprint):
                return self._public(record)
            if record["state"] != expected:
                raise CodeChangeError("INVALID_STATE", "rejection is not currently accepted")
            record["decisions"].append({"gate": gate, "decision": "REJECTED", "actor": actor,
                                        "reason": reason, "at": self._now(), "idempotency_key": key})
            record["state"] = target
            self._save(record)
            return self._public(record)

    def _validate_relative_path(self, path):
        if not isinstance(path, str) or not path or "\x00" in path or "\\" in path:
            raise CodeChangeError("INVALID_PATH", "path must be a repository-relative POSIX path")
        pure = PurePosixPath(path)
        if (pure.is_absolute() or any(part in {"", ".", ".."} for part in pure.parts)
                or any(part in FORBIDDEN_PARTS for part in pure.parts)
                or pure.name in FORBIDDEN_NAMES):
            raise CodeChangeError("PATH_DENIED", "path is outside the allowed development scope")
        if pure.name not in ALLOWED_BASENAMES and pure.suffix.lower() not in ALLOWED_SUFFIXES:
            raise CodeChangeError("FILE_TYPE_DENIED", "file type is not allowed for code changes")
        return pure

    def _worktree_path(self, record, relative, *, must_exist=False):
        if record["state"] not in {"EDITING", "TESTING", "READY_FOR_REVIEW", "WAITING_APPLY_APPROVAL",
                                    "APPLY_REJECTED", "APPLIED_LOCAL", "ROLLED_BACK"}:
            raise CodeChangeError("INVALID_STATE", "development worktree is unavailable")
        pure = self._validate_relative_path(relative)
        root = Path(record["worktree_path"]).resolve(strict=True)
        expected_root = (self.worktree_root / record["change_id"]).resolve(strict=True)
        if root != expected_root:
            raise CodeChangeError("STATE_TAMPERED", "persisted worktree path is invalid")
        candidate = root.joinpath(*pure.parts)
        cursor = root
        for part in pure.parts:
            cursor = cursor / part
            if cursor.is_symlink():
                raise CodeChangeError("SYMLINK_DENIED", "symlinks are not allowed in developer paths")
        if must_exist and not candidate.exists():
            raise CodeChangeError("NOT_FOUND", "development file does not exist")
        resolved_parent = candidate.parent.resolve(strict=False)
        if resolved_parent != root and root not in resolved_parent.parents:
            raise CodeChangeError("PATH_ESCAPE", "development path escapes the worktree")
        return candidate, pure.as_posix()

    def _check_budget(self, record):
        if self._now() - record["started_at"] > self.limits.max_wall_seconds:
            raise CodeChangeError("DEV_BUDGET_EXCEEDED", "developer wall-clock budget exceeded")

    def dev_read(self, change_id, path, *, offset=0, limit=None):
        with self._lock:
            record = self._load(change_id)
            self._check_budget(record)
            candidate, relative = self._worktree_path(record, path, must_exist=True)
            if not candidate.is_file():
                raise CodeChangeError("NOT_REGULAR_FILE", "developer read requires a regular file")
            if candidate.stat().st_size > self.limits.max_file_bytes:
                raise CodeChangeError("FILE_LIMIT", "developer read exceeds the file limit")
            if not isinstance(offset, int) or offset < 0:
                raise CodeChangeError("INVALID_RANGE", "offset must be non-negative")
            limit = self.limits.max_read_bytes if limit is None else limit
            if not isinstance(limit, int) or limit < 1 or limit > self.limits.max_read_bytes:
                raise CodeChangeError("INVALID_RANGE", "read limit exceeds the configured maximum")
            with candidate.open("rb") as stream:
                stream.seek(offset)
                data = stream.read(limit + 1)
            if b"\x00" in data:
                raise CodeChangeError("BINARY_DENIED", "binary repository files are not exposed")
            return {"path": relative, "offset": offset,
                    "content": data[:limit].decode("utf-8", "strict"), "truncated": len(data) > limit,
                    "sha256": _sha256(candidate.read_bytes())}

    def dev_search(self, change_id, query, *, paths=None, max_results=None):
        with self._lock:
            record = self._load(change_id)
            self._check_budget(record)
            if not isinstance(query, str) or not query or len(query) > 500:
                raise CodeChangeError("INVALID_QUERY", "search query must be a bounded literal string")
            maximum = self.limits.max_search_results if max_results is None else max_results
            if not isinstance(maximum, int) or maximum < 1 or maximum > self.limits.max_search_results:
                raise CodeChangeError("INVALID_RANGE", "search result limit is invalid")
            roots = paths or [""]
            files = []
            for path in roots:
                candidate, relative = self._search_path(record, path)
                if candidate.is_file():
                    files.append((candidate, relative))
                elif candidate.is_dir():
                    for child in candidate.rglob("*"):
                        if len(files) >= self.limits.max_search_files:
                            break
                        if child.is_file() and not child.is_symlink() and ".git" not in child.parts:
                            files.append((child, child.relative_to(record["worktree_path"]).as_posix()))
            matches = []
            bytes_examined = 0
            for candidate, relative in files:
                size = candidate.stat().st_size
                if size > self.limits.max_file_bytes:
                    continue
                bytes_examined += size
                if bytes_examined > self.limits.max_search_bytes:
                    return {"matches": matches, "truncated": True}
                try:
                    lines = candidate.read_text(encoding="utf-8").splitlines(keepends=True)
                except (UnicodeDecodeError, OSError):
                    continue
                byte_offset = 0
                for number, line in enumerate(lines, 1):
                    if query in line:
                        matches.append({"path": relative, "line": number,
                                        "byte_offset": byte_offset,
                                        "text": line.rstrip("\r\n")[:1000]})
                        if len(matches) == maximum:
                            return {"matches": matches, "truncated": True}
                    byte_offset += len(line.encode("utf-8"))
            return {"matches": matches, "truncated": False}

    def _search_path(self, record, relative):
        if not isinstance(relative, str) or "\x00" in relative or "\\" in relative:
            raise CodeChangeError("INVALID_PATH", "invalid repository search path")
        pure = PurePosixPath(relative or ".")
        if pure.is_absolute() or ".." in pure.parts or any(part in FORBIDDEN_PARTS for part in pure.parts):
            raise CodeChangeError("PATH_DENIED", "search path escapes the development worktree")
        root = Path(record["worktree_path"]).resolve(strict=True)
        candidate = root if relative in {"", "."} else root.joinpath(*pure.parts)
        cursor = root
        for part in (() if relative in {"", "."} else pure.parts):
            cursor = cursor / part
            if cursor.is_symlink():
                raise CodeChangeError("SYMLINK_DENIED", "symlinks are not allowed in search paths")
        if not candidate.exists():
            raise CodeChangeError("NOT_FOUND", "repository search path does not exist")
        resolved = candidate.resolve(strict=True)
        if resolved != root and root not in resolved.parents:
            raise CodeChangeError("PATH_ESCAPE", "search path escapes the worktree")
        if resolved.is_file():
            self._validate_relative_path(resolved.relative_to(root).as_posix())
        return resolved, "" if resolved == root else resolved.relative_to(root).as_posix()

    def _ensure_editable(self, record, relative):
        if record["state"] != "EDITING":
            raise CodeChangeError("INVALID_STATE", "edits require EDITING state")
        self._check_budget(record)
        if relative not in record["expected_files"]:
            raise CodeChangeError("SCOPE_EXPANSION_REQUIRED", "path was not approved at Gate C1")
        if record["edit_operations"] >= self.limits.max_edit_operations:
            raise CodeChangeError("DEV_BUDGET_EXCEEDED", "edit operation budget exceeded")

    def _write_file(self, candidate, data, *, mode=0o644):
        if len(data) > self.limits.max_file_bytes or b"\x00" in data:
            raise CodeChangeError("FILE_LIMIT", "binary or oversized developer file refused")
        candidate.parent.mkdir(mode=0o755, parents=True, exist_ok=True)
        fd, temporary = tempfile.mkstemp(prefix=f".{candidate.name}.", dir=candidate.parent)
        try:
            os.fchmod(fd, mode)
            with os.fdopen(fd, "wb") as stream:
                stream.write(data)
                stream.flush()
                os.fsync(stream.fileno())
            os.replace(temporary, candidate)
        finally:
            if os.path.exists(temporary):
                os.unlink(temporary)

    def dev_edit(self, change_id, path, *, expected_sha256, replacements):
        with self._lock:
            record = self._load(change_id)
            candidate, relative = self._worktree_path(record, path, must_exist=True)
            self._ensure_editable(record, relative)
            if not candidate.is_file() or candidate.is_symlink():
                raise CodeChangeError("NOT_REGULAR_FILE", "edit requires a regular non-symlink file")
            original = candidate.read_bytes()
            if _sha256(original) != expected_sha256:
                raise CodeChangeError("EDIT_CONFLICT", "file changed since it was read")
            if b"\x00" in original:
                raise CodeChangeError("BINARY_DENIED", "binary edits are not allowed")
            if not isinstance(replacements, list) or not replacements:
                raise CodeChangeError("INVALID_EDIT", "structured replacements are required")
            text = original.decode("utf-8", "strict")
            for operation in replacements:
                if not isinstance(operation, Mapping) or set(operation) != {"old", "new"}:
                    raise CodeChangeError("INVALID_EDIT", "replacement requires old and new strings")
                old, new = operation["old"], operation["new"]
                if not isinstance(old, str) or not old or not isinstance(new, str):
                    raise CodeChangeError("INVALID_EDIT", "replacement values must be strings")
                if old == new:
                    raise CodeChangeError("NOOP_EDIT", "replacement must change the file")
                if text.count(old) != 1:
                    raise CodeChangeError("EDIT_CONFLICT", "replacement anchor must occur exactly once")
                text = text.replace(old, new, 1)
            mode = candidate.stat().st_mode & 0o777
            self._write_file(candidate, text.encode("utf-8"), mode=mode)
            record["edit_operations"] += len(replacements)
            if relative not in record["owned_files"]:
                record["owned_files"].append(relative)
            # INVARIANT: every source mutation invalidates prior test evidence
            # and any preview produced from the previous worktree contents.
            record["tests"] = []
            record["preview"] = {}
            self._save(record)
            return {"path": relative, "sha256": _sha256(candidate.read_bytes())}

    def dev_create(self, change_id, path, content):
        with self._lock:
            record = self._load(change_id)
            candidate, relative = self._worktree_path(record, path)
            self._ensure_editable(record, relative)
            if candidate.exists() or candidate.is_symlink():
                raise CodeChangeError("ALREADY_EXISTS", "developer create cannot overwrite a path")
            if not isinstance(content, str):
                raise CodeChangeError("INVALID_EDIT", "created file content must be UTF-8 text")
            self._write_file(candidate, content.encode("utf-8"))
            record["tests"] = []
            record["preview"] = {}
            record["edit_operations"] += 1
            record["owned_files"].append(relative)
            self._save(record)
            return {"path": relative, "sha256": _sha256(candidate.read_bytes())}

    def dev_delete_owned(self, change_id, path):
        with self._lock:
            record = self._load(change_id)
            candidate, relative = self._worktree_path(record, path, must_exist=True)
            self._ensure_editable(record, relative)
            if relative not in record["owned_files"] or not candidate.is_file() or candidate.is_symlink():
                raise CodeChangeError("DELETE_DENIED", "only change-owned regular files may be deleted")
            candidate.unlink()
            record["tests"] = []
            record["preview"] = {}
            record["edit_operations"] += 1
            self._save(record)
            return {"path": relative, "deleted": True}

    def run_test(self, change_id, recipe_id):
        with self._lock:
            record = self._load(change_id)
            if record["state"] != "EDITING":
                raise CodeChangeError("INVALID_STATE", "tests require EDITING state")
            self._check_budget(record)
            if recipe_id not in self.test_recipes:
                raise CodeChangeError("UNKNOWN_TEST_RECIPE", "test recipe is not allowlisted")
            if record["test_runs"] >= self.limits.max_test_runs:
                raise CodeChangeError("DEV_BUDGET_EXCEEDED", "test run budget exceeded")
            record["state"] = "TESTING"
            record["test_runs"] += 1
            self._save(record)
            started = self._now()
            try:
                completed = subprocess.run(
                    self.test_recipes[recipe_id], cwd=record["worktree_path"],
                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=self.limits.test_timeout_seconds,
                    check=False, env=self._test_environment(),
                )
                raw = completed.stdout or b""
                result = {"recipe_id": recipe_id, "passed": completed.returncode == 0,
                          "returncode": completed.returncode,
                          "output": raw[:self.limits.max_test_output_bytes].decode("utf-8", "replace"),
                          "output_truncated": len(raw) > self.limits.max_test_output_bytes,
                          "started_at": started, "finished_at": self._now()}
            except subprocess.TimeoutExpired as exc:
                raw = exc.stdout or b""
                result = {"recipe_id": recipe_id, "passed": False, "returncode": None,
                          "output": raw[:self.limits.max_test_output_bytes].decode("utf-8", "replace"),
                          "output_truncated": len(raw) > self.limits.max_test_output_bytes,
                          "timed_out": True, "started_at": started, "finished_at": self._now()}
            record = self._load(change_id)
            record["tests"].append(result)
            record["state"] = "EDITING"
            self._save(record)
            return _json_clone(result)

    def consume_model_call(self, change_id):
        """Persist one Developer Agent inference against the global change budget."""
        with self._lock:
            record = self._load(change_id)
            if record["state"] != "EDITING":
                raise CodeChangeError("INVALID_STATE", "model inference requires EDITING state")
            self._check_budget(record)
            if record.get("model_calls", 0) >= self.limits.max_model_calls:
                raise CodeChangeError("DEV_BUDGET_EXCEEDED", "model call budget exceeded")
            record["model_calls"] = record.get("model_calls", 0) + 1
            self._save(record)
            return record["model_calls"]

    @staticmethod
    def _test_environment():
        allowed = ("PATH", "LANG", "LC_ALL", "TZ", "TERM", "TMPDIR")
        env = {name: os.environ[name] for name in allowed if name in os.environ}
        env.update({"HOME": tempfile.gettempdir(), "LABFY_SPECIMEN": "1", "NO_COLOR": "1",
                    "PYTHONDONTWRITEBYTECODE": "1"})
        return env

    def _changed_files(self, record):
        output = self._git("status", "--porcelain=v1", "-z", "--untracked-files=all",
                           cwd=record["worktree_path"]).stdout
        entries = output.split("\x00")
        files = []
        for entry in entries:
            if not entry:
                continue
            status, path = entry[:2], entry[3:]
            if status[0] in "RC" or status[1] in "RC":
                raise CodeChangeError("RENAME_DENIED", "renames and copies are not supported in V1")
            relative = self._validate_relative_path(path).as_posix()
            if relative not in files:
                files.append(relative)
        if len(files) > self.limits.max_files:
            raise CodeChangeError("SCOPE_LIMIT", "changed file count exceeds configured limit")
        if any(path not in record["expected_files"] for path in files):
            raise CodeChangeError("SCOPE_EXPANSION_REQUIRED", "diff contains a path outside Gate C1 scope")
        return files

    def _build_patch(self, record):
        root = Path(record["worktree_path"])
        tracked = self._git("diff", "--binary", "--no-ext-diff", record["base_sha"], "--",
                            cwd=root).stdout.encode("utf-8")
        chunks = [tracked]
        tracked_names = set(self._git("ls-files", cwd=root).stdout.splitlines())
        for relative in self._changed_files(record):
            candidate = root / relative
            if relative in tracked_names or not candidate.exists():
                continue
            completed = subprocess.run(
                ("git", "diff", "--binary", "--no-index", "--", "/dev/null", relative),
                cwd=root, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False,
            )
            if completed.returncode not in (0, 1):
                raise CodeChangeError("GIT_FAILED", "could not produce new-file patch")
            chunks.append(completed.stdout)
        patch = b"".join(chunks)
        if len(patch) > self.limits.max_diff_bytes:
            raise CodeChangeError("DIFF_LIMIT", "diff byte limit exceeded")
        changed_lines = sum(1 for line in patch.splitlines()
                            if (line.startswith(b"+") or line.startswith(b"-"))
                            and not line.startswith((b"+++", b"---")))
        if changed_lines > self.limits.max_changed_lines:
            raise CodeChangeError("DIFF_LIMIT", "changed line limit exceeded")
        for line in patch.splitlines():
            if line.startswith(b"+") and not line.startswith(b"+++") and line.rstrip(b" \t") != line:
                raise CodeChangeError("DIFF_CHECK_FAILED", "added line has trailing whitespace")
        return patch

    def _review(self, record):
        files = self._changed_files(record)
        if not files:
            raise CodeChangeError("EMPTY_CHANGE", "code change has no diff")
        patch = self._build_patch(record)
        check = self._git("diff", "--check", cwd=record["worktree_path"], check=False)
        if check.returncode != 0:
            raise CodeChangeError("DIFF_CHECK_FAILED", check.stderr.strip() or check.stdout.strip())
        findings = []
        for relative in files:
            candidate = Path(record["worktree_path"]) / relative
            if not candidate.exists():
                continue
            data = candidate.read_bytes()
            if len(data) > self.limits.max_file_bytes or b"\x00" in data:
                raise CodeChangeError("BINARY_DENIED", "binary or oversized diff refused")
            for pattern in SECRET_PATTERNS:
                if pattern.search(data):
                    findings.append(relative)
                    break
        if findings:
            raise CodeChangeError("SECRET_SCAN_FAILED", "obvious secret pattern found in changed files")
        return files, patch

    def get_diff(self, change_id, *, max_bytes=None):
        with self._lock:
            record = self._load(change_id)
            files, patch = self._review(record)
            maximum = self.limits.max_diff_bytes if max_bytes is None else max_bytes
            if not isinstance(maximum, int) or maximum < 1 or maximum > self.limits.max_diff_bytes:
                raise CodeChangeError("INVALID_RANGE", "diff limit is invalid")
            stat_run = subprocess.run(("git", "apply", "--stat", "-"), input=patch,
                                      stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
            if stat_run.returncode != 0:
                raise CodeChangeError("GIT_FAILED", "could not summarize archived patch")
            stat_result = stat_run.stdout.decode("utf-8", "replace")
            return {"files": files, "stat": stat_result, "patch": patch[:maximum].decode("utf-8", "replace"),
                    "truncated": len(patch) > maximum, "sha256": _sha256(patch)}

    def set_preview_metadata(self, change_id, metadata, *, idempotency_key=None):
        with self._lock:
            record = self._load(change_id)
            if record["state"] not in {"EDITING", "WAITING_APPLY_APPROVAL"}:
                raise CodeChangeError("INVALID_STATE", "preview metadata requires an editable or reviewable change")
            if (not isinstance(metadata, Mapping) or set(metadata) - {"url", "screenshots", "notes"}
                    or not isinstance(metadata.get("url", ""), str)
                    or metadata.get("url") and not re.fullmatch(r"http://(?:127\.0\.0\.1|localhost):[0-9]{1,5}(?:/[^\s]*)?", metadata["url"])):
                raise CodeChangeError("INVALID_PREVIEW", "preview metadata must reference loopback only")
            value = _json_clone(dict(metadata))
            fingerprint = _sha256(json.dumps(value, sort_keys=True).encode())
            key = idempotency_key or str(uuid.uuid4())
            if self._check_idempotency(record, "set_preview", key, fingerprint):
                return self._public(record)
            record["preview"] = value
            self._save(record)
            return self._public(record)

    def get_preview(self, change_id):
        return self.get(change_id).get("preview")

    def ready_for_review(self, change_id, *, idempotency_key=None):
        with self._lock:
            record = self._load(change_id)
            if record["state"] in {"READY_FOR_REVIEW", "WAITING_APPLY_APPROVAL"}:
                key = idempotency_key or str(uuid.uuid4())
                self._check_idempotency(record, "ready_for_review", key,
                                        record.get("patch_digest", ""))
                self._save(record)
                return self._public(record)
            if record["state"] != "EDITING":
                raise CodeChangeError("INVALID_STATE", "change is not editable")
            latest = {}
            for result in record["tests"]:
                latest[result["recipe_id"]] = result
            missing = [test for test in record["required_tests"]
                       if test not in latest or not latest[test]["passed"]]
            if missing:
                raise CodeChangeError("TEST_REQUIREMENTS_UNMET", "required tests have not passed")
            files, patch = self._review(record)
            key = idempotency_key or str(uuid.uuid4())
            self._check_idempotency(record, "ready_for_review", key, _sha256(patch))
            patch_path = self.patch_root / f"{change_id}.patch"
            self._write_private(patch_path, patch)
            record.update({"state": "WAITING_APPLY_APPROVAL", "files_modified": files,
                           "patch_path": str(patch_path), "patch_digest": _sha256(patch),
                           "review_ready_at": self._now(), "security_checks": {"secret_scan": "PASS",
                                                                                "diff_check": "PASS"}})
            self._save(record)
            return self._public(record)

    @staticmethod
    def _write_private(path, data):
        fd, temporary = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
        try:
            os.fchmod(fd, 0o600)
            with os.fdopen(fd, "wb") as stream:
                stream.write(data)
                stream.flush()
                os.fsync(stream.fileno())
            os.replace(temporary, path)
        finally:
            if os.path.exists(temporary):
                os.unlink(temporary)

    def approve_apply(self, change_id, *, actor, idempotency_key=None, decision_id=None):
        key = idempotency_key or decision_id
        with self._lock:
            record = self._load(change_id)
            actor = self._human(actor)
            fingerprint = _sha256(f"approve_apply:{actor}".encode())
            if self._check_idempotency(record, "approve_apply", key, fingerprint):
                return self._public(record)
            if record["state"] != "WAITING_APPLY_APPROVAL":
                raise CodeChangeError("INVALID_STATE", "apply approval is not currently accepted")
            self._assert_clean_base(record)
            patch_path = Path(record["patch_path"]).resolve(strict=True)
            if patch_path != (self.patch_root / f"{change_id}.patch").resolve(strict=True):
                raise CodeChangeError("STATE_TAMPERED", "persisted patch path is invalid")
            patch = patch_path.read_bytes()
            if _sha256(patch) != record["patch_digest"]:
                raise CodeChangeError("PATCH_TAMPERED", "archived patch digest mismatch")
            self._validate_patch_paths(patch, record["files_modified"])
            self._git("apply", "--check", "--binary", "-", input_bytes=patch)
            record["state"] = "APPLYING"
            record["decisions"].append({"gate": "C2", "decision": "APPROVED", "actor": actor,
                                        "at": self._now(), "idempotency_key": key})
            self._save(record)
            try:
                self._git("apply", "--binary", "-", input_bytes=patch)
                record["applied_file_digests"] = self._file_digests(self.repo_root, record["files_modified"])
                post_tests = []
                for recipe_id in record["required_tests"]:
                    post_tests.append(self._run_recipe_at(recipe_id, self.repo_root))
                    if not post_tests[-1]["passed"]:
                        raise CodeChangeError("POST_APPLY_TEST_FAILED", f"{recipe_id} failed after apply")
                record["post_apply_tests"] = post_tests
                record["state"] = "APPLIED_LOCAL"
                record["applied_at"] = self._now()
                self._save(record)
                return self._public(record)
            except Exception:
                # CONTRACT: a failed post-apply recipe is still evidence.
                # Preserve its bounded output before compensation so an
                # operator can distinguish code failure from test setup.
                if "post_tests" in locals():
                    record["post_apply_tests"] = post_tests
                reverse = subprocess.run(("git", "apply", "--check", "--reverse", "--binary", "-"),
                                         cwd=self.repo_root, input=patch, stdout=subprocess.PIPE,
                                         stderr=subprocess.PIPE, check=False)
                if reverse.returncode == 0:
                    subprocess.run(("git", "apply", "--reverse", "--binary", "-"), cwd=self.repo_root,
                                   input=patch, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
                    record["state"] = "ROLLED_BACK"
                    record["failure"] = "APPLY_OR_POST_TEST_FAILED"
                else:
                    record["state"] = "FAILED"
                    record["failure"] = "APPLY_FAILED_MANUAL_RECOVERY_REQUIRED"
                self._save(record)
                raise

    def _run_recipe_at(self, recipe_id, cwd):
        completed = subprocess.run(self.test_recipes[recipe_id], cwd=cwd, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, timeout=self.limits.test_timeout_seconds,
                                   check=False, env=self._test_environment())
        raw = completed.stdout or b""
        return {"recipe_id": recipe_id, "passed": completed.returncode == 0,
                "returncode": completed.returncode,
                "output": raw[:self.limits.max_test_output_bytes].decode("utf-8", "replace"),
                "output_truncated": len(raw) > self.limits.max_test_output_bytes}

    def _assert_clean_base(self, record):
        if self._git("rev-parse", "HEAD").stdout.strip() != record["base_sha"]:
            raise CodeChangeError("STALE_BASE", "main HEAD no longer matches the approved baseline")
        if self._git("status", "--porcelain=v1", "--untracked-files=all").stdout:
            raise CodeChangeError("DIRTY_MAIN", "main worktree must be clean before apply")

    def _validate_patch_paths(self, patch, expected_files):
        completed = subprocess.run(("git", "apply", "--numstat", "-z", "-"),
                                   cwd=self.repo_root, input=patch, stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE, check=False)
        if completed.returncode != 0:
            raise CodeChangeError("INVALID_PATCH", "archived patch cannot be inspected")
        paths = []
        for entry in completed.stdout.split(b"\x00"):
            if not entry:
                continue
            try:
                _added, _deleted, raw_path = entry.split(b"\t", 2)
                relative = raw_path.decode("utf-8", "strict")
            except (ValueError, UnicodeDecodeError) as exc:
                raise CodeChangeError("INVALID_PATCH", "archived patch path is invalid") from exc
            paths.append(self._validate_relative_path(relative).as_posix())
        if set(paths) != set(expected_files) or len(paths) != len(set(paths)):
            raise CodeChangeError("PATCH_SCOPE_MISMATCH", "archived patch paths differ from review scope")

    @staticmethod
    def _file_digests(root, paths):
        result = {}
        for relative in paths:
            candidate = root / relative
            result[relative] = _sha256(candidate.read_bytes()) if candidate.is_file() else None
        return result

    def reject_apply(self, change_id, *, actor, reason, idempotency_key=None, decision_id=None):
        return self._reject(change_id, "C2", "WAITING_APPLY_APPROVAL", "APPLY_REJECTED",
                            actor, reason, idempotency_key or decision_id)

    def rollback(self, change_id, *, actor, idempotency_key=None, decision_id=None):
        key = idempotency_key or decision_id
        with self._lock:
            record = self._load(change_id)
            actor = self._human(actor)
            fingerprint = _sha256(f"rollback:{actor}".encode())
            if self._check_idempotency(record, "rollback", key, fingerprint):
                return self._public(record)
            if record["state"] != "APPLIED_LOCAL":
                raise CodeChangeError("INVALID_STATE", "only an applied local change may be rolled back")
            if self._git("rev-parse", "HEAD").stdout.strip() != record["base_sha"]:
                raise CodeChangeError("STALE_BASE", "main HEAD changed after local apply")
            if self._file_digests(self.repo_root, record["files_modified"]) != record["applied_file_digests"]:
                raise CodeChangeError("ROLLBACK_CONFLICT", "applied files changed after local apply")
            patch = Path(record["patch_path"]).read_bytes()
            self._git("apply", "--check", "--reverse", "--binary", "-", input_bytes=patch)
            self._git("apply", "--reverse", "--binary", "-", input_bytes=patch)
            record["state"] = "ROLLED_BACK"
            record["rolled_back_at"] = self._now()
            record["decisions"].append({"gate": "ROLLBACK", "decision": "APPROVED", "actor": actor,
                                        "at": self._now(), "idempotency_key": key})
            self._save(record)
            return self._public(record)

    def get(self, change_id):
        with self._lock:
            return self._public(self._load(change_id))

    def list_changes(self):
        with self._lock:
            records = []
            for path in sorted(self.record_root.glob("*.json")):
                try:
                    records.append(self._public(self._load(path.stem)))
                except CodeChangeError:
                    continue
            return records

    @staticmethod
    def _public(record):
        value = _json_clone(record)
        value.pop("idempotency", None)
        value.pop("worktree_path", None)
        value.pop("patch_path", None)
        return value


__all__ = ["CONTRACT", "STATES", "CodeChangeError", "CodeChangeService", "DevLimits"]
