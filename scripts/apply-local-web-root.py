#!/usr/bin/env python3
"""Ajoute le seul hôte Labfy local à nginx et hosts, puis valide et recharge."""

from __future__ import annotations

import os
import secrets
import subprocess
import sys
from pathlib import Path

NGINX = Path("/etc/nginx/nginx.conf")
HOSTS = Path("/etc/hosts")
BLOCK = Path(__file__).resolve().parents[1] / "packaging/nginx/invest.labfy.conf"


def render_nginx(current: str, block: str) -> str:
    # CONTRACT: preserve the existing Trainlog server byte for byte. A second
    # differing invest.labfy block is a conflict, not an invitation to replace it.
    if "server_name trainlog.perf;" not in current:
        raise ValueError("bloc Trainlog attendu absent")
    if "server_name invest.labfy;" in current:
        if block.strip() not in current:
            raise ValueError("bloc invest.labfy existant différent")
        return current
    if not current.rstrip().endswith("}"):
        raise ValueError("fin du bloc http nginx inattendue")
    end = len(current.rstrip()) - 1
    return current[:end] + "\n" + block.rstrip() + "\n" + current[end:]


def render_hosts(current: str) -> str:
    # INVARIANT: a conflicting resolver entry must be reconciled by the owner;
    # never append a competing mapping that depends on resolver order.
    found = False
    for line in current.splitlines():
        fields = line.split("#", 1)[0].split()
        if "invest.labfy" in fields[1:]:
            if fields[0] != "127.0.0.1":
                raise ValueError("résolution invest.labfy conflictuelle")
            found = True
    if found:
        return current
    return current.rstrip("\n") + "\n127.0.0.1 invest.labfy\n"


def write_atomic(path: Path, data: bytes) -> None:
    temporary = path.with_name(path.name + ".labfy-new-" + secrets.token_hex(8))
    descriptor = os.open(temporary, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o644)
    try:
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        temporary.replace(path)
    finally:
        temporary.unlink(missing_ok=True)


def save_backup(path: Path, data: bytes) -> Path:
    backup = path.with_name(path.name + ".labfy-pre-invest-" + secrets.token_hex(8))
    descriptor = os.open(backup, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
    with os.fdopen(descriptor, "wb") as stream:
        stream.write(data)
        stream.flush()
        os.fsync(stream.fileno())
    return backup


def run(argv: list[str]) -> None:
    subprocess.run(argv, check=True)


def main() -> int:
    if os.geteuid() != 0:
        raise ValueError("exécution root requise")
    if any(path.is_symlink() or not path.is_file() for path in (NGINX, HOSTS)):
        raise ValueError("fichier système non régulier ou lien symbolique")
    before_nginx = NGINX.read_bytes()
    before_hosts = HOSTS.read_bytes()
    nginx = render_nginx(before_nginx.decode(), BLOCK.read_text())
    hosts = render_hosts(before_hosts.decode())
    if nginx.encode() == before_nginx and hosts.encode() == before_hosts:
        run(["/usr/bin/nginx", "-t"])
        print("configuration déjà appliquée et valide")
        return 0
    backups = []
    try:
        if nginx.encode() != before_nginx:
            backups.append(save_backup(NGINX, before_nginx))
        if hosts.encode() != before_hosts:
            backups.append(save_backup(HOSTS, before_hosts))
        if nginx.encode() != before_nginx:
            write_atomic(NGINX, nginx.encode())
        if hosts.encode() != before_hosts:
            write_atomic(HOSTS, hosts.encode())
        # CONTRACT: nginx is reloaded only after the real root-owned config
        # passes nginx -t; failed validation restores both original files.
        run(["/usr/bin/nginx", "-t"])
        run(["/usr/bin/systemctl", "reload", "nginx"])
    except Exception:
        if backups:
            write_atomic(NGINX, before_nginx)
            write_atomic(HOSTS, before_hosts)
        raise
    print("Configuration appliquée. Sauvegardes :", *(str(path) for path in backups))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"Application root refusée : {error}", file=sys.stderr)
        sys.exit(1)
