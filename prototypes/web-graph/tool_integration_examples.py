"""Bounded declarative examples for a newly quarantined Debian tool.

Examples are data. They never grant activation or execute a binary, and the
normal integration validator still checks the package inventory and digest.
"""

from __future__ import annotations

import hashlib
import json
import uuid

from tool_provisioning import (ADAPTER_CONTRACT, CAPABILITY_CONTRACT,
                               INTEGRATION_CONTRACT)


def example_for(request):
    """Return a jq username adapter example when that exact package is installed."""
    if request.get("state") not in {"QUARANTINED", "WAITING_INTEGRATION_APPROVAL"} \
            or request.get("package", {}).get("package") != "jq":
        return None
    parameters = {"username": {"type": "string", "max_length": 128,
                               "required": True}}
    adapter = {
        "contract": ADAPTER_CONTRACT,
        "tool_id": "data.jq.username",
        "binary": "jq",
        "risk_class": "OFFLINE_READ_ONLY",
        "execution_backend": "TOOLBOX_PODMAN",
        "input_contract": {"parameters": parameters, "artifacts": []},
        "output_contract": {"outputs": []},
        "execution_profile": {"timeout_seconds": 10, "max_output_bytes": 4096,
                              "network": "OFFLINE"},
        # CONTRACT: these are literal argv tokens, never a command line or
        # shell fragment. The value is bound from the backend graph object.
        "argv_template": [
            {"kind": "literal", "value": "-n"},
            {"kind": "literal", "value": "--arg"},
            {"kind": "literal", "value": "username"},
            {"kind": "parameter", "name": "username", "flag": ""},
            {"kind": "literal", "value": "{username:$username}"},
        ],
        "parser": {"kind": "STDOUT_JSON"},
    }
    digest = hashlib.sha256(json.dumps(
        adapter, sort_keys=True, ensure_ascii=False,
        separators=(",", ":")).encode()).hexdigest()
    adapter["capabilities"] = [{
        "contract": CAPABILITY_CONTRACT,
        "capability_id": "data.jq.username",
        "tool_id": adapter["tool_id"],
        "title": "Structurer un pseudo",
        "category": "Analyse locale",
        "intent": "Structurer ce pseudo avec jq",
        "applicable_object_types": ["username"],
        "input_binding": {"object_value_parameter": "username"},
        "parameters_schema": parameters,
        "output_kind": "JSON",
        "risk_class": "OFFLINE_READ_ONLY",
        "network_contact": "NONE",
        "authorization": "MISSION_POLICY",
        "availability": "AVAILABLE",
        "reason": "Outil offline activé après Gate B",
        "icon_key": "braces",
        "adapter_digest": digest,
        "generation_id": request["generation_id"],
    }]
    return {"contract": INTEGRATION_CONTRACT,
            "proposal_id": str(uuid.uuid5(uuid.NAMESPACE_URL, request["request_id"])),
            "request_id": request["request_id"],
            "risk": "OFFLINE_READ_ONLY", "adapter": adapter}
