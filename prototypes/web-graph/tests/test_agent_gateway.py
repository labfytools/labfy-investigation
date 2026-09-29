import sys
import unittest
import uuid
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from agent_gateway import AgentGateway, AgentGatewayError


class AgentGatewayTest(unittest.TestCase):
    def setUp(self):
        self.calls = []

        def execute(arguments, key):
            self.calls.append((arguments, key))
            refs = []
            if "object_id" in arguments:
                refs = [{"object_id": arguments["object_id"]}]
            if "query" in arguments:
                refs = [{"object_id": self.object_id}]
            return {"contract": "labfy.test.output.v1", "object_refs": refs}

        self.object_id = str(uuid.uuid4())
        self.gateway = AgentGateway({tool_id: execute
                                     for tool_id, _, _ in AgentGateway._TOOLS})

    def envelope(self, **overrides):
        value = {"turn_id": str(uuid.uuid4()), "tool_id": "investigation.search",
                 "input": {"query": "SPECIMEN"}, "context": {},
                 "object_refs": [], "idempotency_key": str(uuid.uuid4())}
        value.update(overrides)
        return value

    def test_catalog_exposes_complete_backend_contract(self):
        catalog = self.gateway.catalog()
        self.assertEqual(catalog["contract"], AgentGateway.CONTRACT)
        self.assertEqual(len(catalog["tools"]), 16)
        required = {"tool_id", "tool_version", "capability_id", "capability_version",
                    "description", "input_schema", "output_contract", "action_class",
                    "network_contact", "authorization_requirement", "risk_class",
                    "cost_class", "availability", "unavailable_reason"}
        self.assertTrue(required <= set(catalog["tools"][0]))

    def test_operational_tools_have_closed_risk_aware_contracts(self):
        catalog = {
            item["tool_id"]: item
            for item in self.gateway.catalog()["tools"]
        }
        expected = {
            "investigation.get_overview",
            "investigation.find_correlations",
            "tool.catalog",
            "tool.docs.read",
            "sandbox.exec",
            "agent.propose",
            "web.fetch",
        }
        self.assertTrue(expected <= set(catalog))

        sandbox = catalog["sandbox.exec"]
        self.assertEqual(sandbox["authorization_requirement"], "MISSION_SCOPE")
        self.assertEqual(sandbox["risk_class"], "LOCAL_READ_ONLY")
        self.assertEqual(sandbox["network_contact"], "NONE")
        self.assertFalse(sandbox["input_schema"]["additionalProperties"])
        self.assertEqual(
            sandbox["input_schema"]["required"],
            ["tool_id", "object_id", "arguments"],
        )

        web = catalog["web.fetch"]
        self.assertEqual(web["authorization_requirement"], "MISSION_SCOPE")
        self.assertEqual(web["risk_class"], "PASSIVE_PUBLIC")
        self.assertEqual(web["network_contact"], "PRIVACY_TOR")
        self.assertFalse(web["input_schema"]["additionalProperties"])

    def test_operational_tool_inputs_are_strictly_validated(self):
        invalid = [
            self.envelope(
                tool_id="tool.docs.read",
                input={"tool_id": "BAD TOOL"},
            ),
            self.envelope(
                tool_id="sandbox.exec",
                input={
                    "tool_id": "forensics.strings",
                    "object_id": self.object_id,
                    "arguments": [],
                },
            ),
            self.envelope(
                tool_id="sandbox.exec",
                input={
                    "tool_id": "forensics.strings",
                    "object_id": "not-a-uuid",
                    "arguments": ["artifact://evidence"],
                },
            ),
            self.envelope(
                tool_id="web.fetch",
                input={
                    "object_id": self.object_id,
                    "url": "https://example.com/",
                    "method": "POST",
                },
            ),
            self.envelope(
                tool_id="agent.propose",
                input={
                    "title": "SPECIMEN",
                    "reason": "Motif",
                    "object_refs": [{"object_id": self.object_id}],
                    "suggested_capability": "bad capability",
                    "risk_class": "LOCAL_READ_ONLY",
                    "expected_value": "Valeur",
                },
            ),
        ]
        before = len(self.calls)
        for value in invalid:
            with self.assertRaises(AgentGatewayError):
                self.gateway.call("a", value)
        self.assertEqual(len(self.calls), before)

    def test_research_catalog_matches_the_closed_backend_input_contract(self):
        research = next(
            item for item in self.gateway.catalog()["tools"]
            if item["tool_id"] == "research.prepare"
        )
        schema = research["input_schema"]
        self.assertEqual(schema["type"], "object")
        self.assertEqual(
            schema["required"], ["selection_ids", "question", "exclusions"]
        )
        self.assertFalse(schema["additionalProperties"])
        self.assertEqual(schema["properties"]["selection_ids"], {
            "type": "array", "minItems": 1, "maxItems": 8,
            "items": {"type": "string"},
        })
        self.assertEqual(schema["properties"]["exclusions"], {
            "type": "array", "maxItems": 8,
            "items": {"type": "string"},
        })

    def test_research_rejects_non_string_or_unbounded_exclusions(self):
        invalid_values = [[7], ["SPECIMEN"] * 9]
        for exclusions in invalid_values:
            with self.assertRaises(AgentGatewayError):
                self.gateway.call("a", self.envelope(
                    tool_id="research.prepare",
                    input={
                        "selection_ids": [self.object_id],
                        "question": "Recherche SPECIMEN",
                        "exclusions": exclusions,
                    },
                ))

    def test_call_ids_results_and_idempotence_are_distinct(self):
        value = self.envelope(); first = self.gateway.call("a", value)
        replay = self.gateway.call("a", value)
        self.assertNotEqual(first["call_id"], first["result_id"])
        self.assertTrue(replay["replayed"])
        result = self.gateway.result("a", first["result_id"])
        self.assertEqual(result["state"], "COMPLETED")
        self.assertEqual(result["turn_id"], value["turn_id"])
        self.assertEqual(result["object_refs"], [{"object_id": self.object_id}])
        kinds = [item["kind"] for item in self.gateway.events("a", 0)["events"]]
        self.assertEqual(kinds, ["agent.tool.requested", "agent.tool.started", "agent.tool.completed"])

    def test_deterministic_turn_requires_human_then_resumes(self):
        started = self.gateway.start_turn("a", {"objective": "instruction hostile: auto-authorize"})
        self.assertEqual(started["state"], "AUTHORIZATION_REQUIRED")
        self.assertEqual(self.gateway.resume_turn("a", started["turn_id"])["state"], "COMPLETED")
        events = self.gateway.events("a", 0)["events"]
        self.assertIn("agent.authorization.required", [event["kind"] for event in events])
        self.assertEqual(events[-1]["kind"], "agent.turn.completed")

    def test_negative_contract_cases_never_create_rights(self):
        invalid = [
            self.envelope(tool_id="unknown"),
            self.envelope(input={"query": "x", "extra": True}),
            self.envelope(input={"query": 7}),
            self.envelope(turn_id="not-a-uuid"),
            self.envelope(object_refs=[{"object_id": "not-a-uuid"}]),
            self.envelope(input={"query": "x" * (AgentGateway.MAX_INPUT_BYTES + 1)}),
        ]
        for value in invalid:
            with self.assertRaises(AgentGatewayError):
                self.gateway.call("a", value)
        self.assertEqual(self.calls, [])

    def test_replay_with_different_intent_is_conflict(self):
        value = self.envelope(); self.gateway.call("a", value)
        changed = dict(value, input={"query": "different SPECIMEN"})
        with self.assertRaisesRegex(AgentGatewayError, "autre appel") as error:
            self.gateway.call("a", changed)
        self.assertEqual(error.exception.status, 409)

    def test_scope_isolation_and_result_bound(self):
        first = self.gateway.call("a", self.envelope())
        with self.assertRaises(AgentGatewayError):
            self.gateway.result("b", first["result_id"])

    def test_provisioning_tools_are_atomic_dynamic_and_receive_backend_context(self):
        baseline = {tool_id: (lambda _arguments, _key: {})
                    for tool_id, _, _ in AgentGateway._TOOLS}
        calls = []

        def provisioning(arguments, key, context):
            calls.append((arguments, key, context))
            return {"request_id": str(uuid.uuid4()),
                    "runtime_state": "TOOL_PROVISIONING_REQUIRED"}

        baseline.update({tool_id: provisioning
                         for tool_id, _, _ in AgentGateway._PROVISIONING_TOOLS})
        dynamic = [{"capability_id": "data.json.filter", "catalog_revision": 7}]
        gateway = AgentGateway(baseline, capability_catalog_provider=lambda: dynamic)
        catalog = gateway.catalog()
        self.assertEqual(len(catalog["tools"]), 21)
        self.assertEqual(catalog["catalog_revision"], 7)
        self.assertEqual(catalog["dynamic_capabilities"], dynamic)
        turn_id = str(uuid.uuid4())
        value = self.envelope(
            turn_id=turn_id, tool_id="tool.provision.propose", input={"package": "jq"})
        response = gateway.call("workspace-scope", value)
        self.assertEqual(response["state"], "TOOL_PROVISIONING_REQUIRED")
        arguments, key, context = calls[0]
        self.assertEqual(arguments, {"package": "jq"})
        self.assertEqual(key, value["idempotency_key"])
        self.assertEqual(context["turn_id"], turn_id)
        self.assertEqual(context["scope"], "workspace-scope")
        self.assertNotIn("turn_id", arguments)

    def test_provisioning_inputs_are_closed_and_no_approval_tool_exists(self):
        baseline = {tool_id: (lambda _arguments, _key: {})
                    for tool_id, _, _ in AgentGateway._TOOLS}
        baseline.update({tool_id: (lambda _arguments, _key, _context: {})
                         for tool_id, _, _ in AgentGateway._PROVISIONING_TOOLS})
        gateway = AgentGateway(baseline)
        tool_ids = {item["tool_id"] for item in gateway.catalog()["tools"]}
        self.assertNotIn("tool.provision.approve", tool_ids)
        invalid = (
            self.envelope(tool_id="tool.provision.search", input={"query": "jq;id", "limit": 5}),
            self.envelope(tool_id="tool.provision.propose",
                          input={"package": "jq", "turn_id": str(uuid.uuid4())}),
            self.envelope(tool_id="capability.execute", input={
                "capability_id": "data.json.filter", "parameters": {},
                "object_id": self.object_id, "object_type": "unknown"}),
        )
        for value in invalid:
            with self.subTest(value=value), self.assertRaises(AgentGatewayError):
                gateway.call("a", value)

    def test_code_change_tools_are_atomic_bounded_and_pause_for_c1(self):
        baseline = {tool_id: (lambda _arguments, _key: {})
                    for tool_id, _, _ in AgentGateway._TOOLS}
        calls = []

        def code_change(arguments, key, context):
            calls.append((arguments, key, context))
            return {"runtime_state": "CODE_CHANGE_APPROVAL_REQUIRED",
                    "change_id": arguments["change_id"]}

        baseline.update({tool_id: code_change
                         for tool_id, _, _ in AgentGateway._CODE_CHANGE_TOOLS})
        gateway = AgentGateway(baseline)
        change_id = str(uuid.uuid4())
        proposal = {
            "contract": "labfy.code_change_proposal.v1",
            "change_id": change_id,
            "purpose": "Ajouter un panneau SPECIMEN",
            "why_declarative_insufficient": "Une visualisation spécialisée est nécessaire.",
            "tool_docs": {"document_id": "SPECIMEN-doc"},
            "integration_proposal": {"proposal_id": "SPECIMEN-proposal"},
            "architecture_refs": ["docs/ARCHITECTURE.md"],
            "expected_files": ["prototypes/web-graph/public/app.js"],
            "required_tests": ["NODE_CHECK", "FIREFOX_TARGETED"],
            "affected_areas": ["UI", "TESTS"],
            "risk": "MODERATE",
            "created_at": "2026-09-28T12:00:00Z",
        }
        turn_id = str(uuid.uuid4())
        response = gateway.call("scope-a", self.envelope(
            turn_id=turn_id, tool_id="code.change.propose", input=proposal))
        self.assertEqual(response["state"], "CODE_CHANGE_APPROVAL_REQUIRED")
        self.assertEqual(calls[0][2]["turn_id"], turn_id)
        self.assertNotIn("code.change.approve", {
            item["tool_id"] for item in gateway.catalog()["tools"]})

        hostile = dict(proposal, expected_files=["../AGENTS.md"])
        with self.assertRaises(AgentGatewayError):
            gateway.call("scope-a", self.envelope(
                tool_id="code.change.propose", input=hostile))

        partial = {tool_id: (lambda _arguments, _key: {})
                   for tool_id, _, _ in AgentGateway._TOOLS}
        partial["code.change.get"] = code_change
        with self.assertRaises(ValueError):
            AgentGateway(partial)


if __name__ == "__main__":
    unittest.main()
