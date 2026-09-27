#!/usr/bin/env python3
"""Lancement du poste Web sur une destination locale explicitement choisie."""

from __future__ import annotations

import argparse
import subprocess
from pathlib import Path

from workspace_server import main as serve_main


def prepare(workspace: Path, bridge: Path):
    workspace.mkdir(mode=0o700, parents=True, exist_ok=True)
    # CONTRACT: créer le dossier technique autorisé n'est pas créer une enquête.
    # Le POST authentifié /api/v1/workspace appelle seul le cœur C.
    manifest = workspace / ".labfy" / "runtime" / "workspace.json"
    specimen = workspace / ".labfy" / "runtime" / "specimen.json"
    if manifest.exists() or specimen.exists():
        result = subprocess.run([str(bridge), "export", "--workspace", str(workspace)],
                                cwd=bridge.parents[1], text=True,
                                capture_output=True, timeout=10, check=False)
        if result.returncode != 0:
            raise SystemExit(result.stderr.strip() or "Ouverture C impossible")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--workspace", type=Path, required=True)
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()
    repository = Path(__file__).resolve().parents[2]
    bridge = repository / "tools" / "local-jobs"
    prepare(args.workspace.resolve(), bridge)
    import sys
    sys.argv = [sys.argv[0], "--workspace", str(args.workspace.resolve()),
                "--bridge", str(bridge), "--port", str(args.port)]
    serve_main()
