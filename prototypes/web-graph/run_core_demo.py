#!/usr/bin/env python3
"""Génère une fixture C privée puis sert exclusivement son snapshot."""

from __future__ import annotations

import argparse
import json
import subprocess
import tempfile
from pathlib import Path

from server import load_core_snapshot, serve

ROOT = Path(__file__).resolve().parents[2]
GENERATOR = ROOT / "tools" / "core_graph_demo"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()
    if not GENERATOR.is_file():
        parser.error("générateur absent ; exécuter `make core-graph-demo`")
    with tempfile.TemporaryDirectory(prefix="labfy-core-graph-specimen-") as directory:
        # WHY: exécutable et argv sont fixes ; aucun paramètre HTTP ne peut
        # choisir une base, une commande ou un chemin de preuve.
        try:
            result = subprocess.run(
                [str(GENERATOR), "--output-dir", directory],
                cwd=ROOT,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                timeout=30,
                check=False,
            )
        except subprocess.TimeoutExpired:
            serve(args.port, core_error="Le générateur C a dépassé 30 secondes")
            return
        if result.returncode != 0:
            diagnostic = result.stderr.strip()[:2048] or "échec sans diagnostic"
            serve(args.port, core_error=f"Le générateur C a échoué : {diagnostic}")
            return
        snapshot_path = Path(directory) / "core-snapshot.json"
        try:
            snapshot = load_core_snapshot(snapshot_path)
        except (OSError, ValueError, json.JSONDecodeError) as error:
            serve(args.port, core_error=f"Snapshot cœur refusé : {error}")
            return
        serve(args.port, core_snapshot=snapshot)


if __name__ == "__main__":
    main()
