#!/usr/bin/env python3
"""Smoke manuel réel Privacy Tor, exclusivement sur example.com et check.torproject.org."""

import argparse
import json
import subprocess
import sys
from pathlib import Path

PROTOTYPE = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROTOTYPE))
from privacy_egress import EgressStatus, PodmanTorRuntime, PrivacyEgressSupervisor


def build_image(image: str) -> None:
    context = PROTOTYPE / "privacy-egress"
    completed = subprocess.run(
        ["/usr/bin/podman", "build", "--tag", image, "--file",
         str(context / "Containerfile"), str(context)],
        check=False, text=True,
    )
    if completed.returncode != 0:
        raise RuntimeError("construction de l'image Tor impossible")


def main() -> int:
    parser = argparse.ArgumentParser(description="Smoke réel rootless Podman/Tor de Labfy")
    parser.add_argument("--image", default=PodmanTorRuntime.IMAGE)
    parser.add_argument("--skip-build", action="store_true",
                        help="utiliser une image locale déjà construite")
    arguments = parser.parse_args()
    runtime = PodmanTorRuntime(image=arguments.image)
    supervisor = None
    try:
        if not arguments.skip_build:
            build_image(arguments.image)
        supervisor = PrivacyEgressSupervisor.start_owned_tor(runtime)
        result = supervisor.fetch(
            "https://example.com/", method="HEAD", timeout_seconds=20.0,
            max_body_bytes=4096, max_redirects=0,
        )
        ready_state = runtime.state.value
        ready_reason = runtime.reason
        supervisor.close()
        down_result = supervisor.fetch("https://example.com/", method="HEAD")
        summary = {
            "contract": "labfy.privacy_egress.manual_smoke.v1",
            "runtime_state": ready_state,
            "runtime_reason": ready_reason,
            "fetch_status": result.status.value,
            "direct_fallback": result.provenance["direct_fallback"],
            "tor_down_status": down_result.status.value,
            "tor_down_direct_fallback": down_result.provenance["direct_fallback"],
            "target": "https://example.com/",
        }
        print(json.dumps(summary, ensure_ascii=False, sort_keys=True))
        success = (
            result.status == EgressStatus.SUCCESS
            and down_result.status == EgressStatus.PRIVACY_EGRESS_UNAVAILABLE
            and not result.provenance["direct_fallback"]
            and not down_result.provenance["direct_fallback"]
        )
        return 0 if success else 1
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        print(json.dumps({
            "contract": "labfy.privacy_egress.manual_smoke.v1",
            "runtime_state": runtime.state.value,
            "error": str(error)[:256],
            "direct_fallback": False,
        }, ensure_ascii=False, sort_keys=True), file=sys.stderr)
        return 1
    finally:
        if supervisor is not None:
            supervisor.close()
        else:
            runtime.close()


if __name__ == "__main__":
    raise SystemExit(main())
