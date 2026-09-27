#!/usr/bin/env python3
"""Exécute le parcours C J4 puis sert son snapshot strictement en lecture seule."""

from __future__ import annotations

import argparse
import json
import subprocess
import tempfile
from pathlib import Path

from server import load_core_snapshot, serve

ROOT = Path(__file__).resolve().parents[2]
GENERATOR = ROOT / "tools" / "local_toolkit_demo"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()
    if not GENERATOR.is_file():
        parser.error("générateur absent ; exécuter `make local-toolkit-demo`")
    with tempfile.TemporaryDirectory(prefix="labfy-local-toolkit-specimen-") as directory:
        try:
            result = subprocess.run(
                [str(GENERATOR), "--output-dir", directory],
                cwd=ROOT,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                timeout=60,
                check=False,
            )
        except subprocess.TimeoutExpired:
            serve(args.port, core_error="Le lanceur local J4 a dépassé 60 secondes")
            return
        if result.returncode != 0:
            diagnostic = result.stderr.strip()[:2048] or "échec sans diagnostic"
            serve(args.port, core_error=f"Le lanceur local J4 a échoué : {diagnostic}")
            return
        try:
            snapshot = load_core_snapshot(Path(directory) / "core-snapshot.json")
        except (OSError, ValueError, json.JSONDecodeError) as error:
            serve(args.port, core_error=f"Snapshot J4 refusé : {error}")
            return
        serve(args.port, core_snapshot=snapshot)


if __name__ == "__main__":
    main()
