#!/usr/bin/env python3
"""Exécute l'analyse EML C synthétique puis sert son snapshot en lecture seule."""

from __future__ import annotations

import argparse
import json
import subprocess
import tempfile
from pathlib import Path

from server import load_core_snapshot, serve

ROOT = Path(__file__).resolve().parents[2]
GENERATOR = ROOT / "tools" / "eml_graph_demo"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--variant", choices=("default", "alternate"), default="default")
    args = parser.parse_args()
    if not GENERATOR.is_file():
        parser.error("générateur absent ; exécuter `make eml-graph-demo`")
    with tempfile.TemporaryDirectory(prefix="labfy-eml-graph-specimen-") as directory:
        # CONTRACT: le binaire et ses options sont fixes et aucune entrée HTTP
        # ne peut désigner une enquête, une preuve ou une commande arbitraire.
        try:
            result = subprocess.run(
                [str(GENERATOR), "--output-dir", directory, "--variant", args.variant],
                cwd=ROOT,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                timeout=45,
                check=False,
            )
        except subprocess.TimeoutExpired:
            serve(args.port, core_error="L'analyse EML C a dépassé 45 secondes")
            return
        if result.returncode != 0:
            diagnostic = result.stderr.strip()[:2048] or "échec sans diagnostic"
            serve(args.port, core_error=f"L'analyse EML C a échoué : {diagnostic}")
            return
        try:
            snapshot = load_core_snapshot(Path(directory) / "core-snapshot.json")
        except (OSError, ValueError, json.JSONDecodeError) as error:
            serve(args.port, core_error=f"Snapshot EML refusé : {error}")
            return
        serve(args.port, core_snapshot=snapshot)


if __name__ == "__main__":
    main()
