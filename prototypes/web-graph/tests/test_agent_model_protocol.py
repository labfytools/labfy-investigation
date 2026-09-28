import json
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from agent_model_protocol import CONTRACT, AgentModelProtocolError, parse_model_action


class AgentModelProtocolTest(unittest.TestCase):
    def test_accepts_only_known_tool_call_and_final(self):
        tool = {"contract": CONTRACT, "kind": "tool_call",
                "tool_id": "graph.get_node", "arguments": {"object_id": "x"}}
        final = {"contract": CONTRACT, "kind": "final", "text": "Terminé."}
        self.assertEqual(parse_model_action(json.dumps(tool), {"graph.get_node"}), tool)
        self.assertEqual(parse_model_action(json.dumps(final), set()), final)

    def test_rejects_unknown_tool_extra_fields_and_authorization(self):
        invalid = [
            {"contract": CONTRACT, "kind": "tool_call", "tool_id": "shell",
             "arguments": {}},
            {"contract": CONTRACT, "kind": "final", "text": "ok", "grant": True},
            {"contract": CONTRACT, "kind": "authorize", "text": "oui"},
        ]
        for action in invalid:
            with self.subTest(action=action), self.assertRaises(AgentModelProtocolError):
                parse_model_action(json.dumps(action), {"graph.get_node"})

    def test_rejects_markdown_trailing_text_and_duplicate_keys(self):
        invalid = [
            '```json\n{"contract":"labfy.agent_model_action.v1"}\n```',
            '{"contract":"labfy.agent_model_action.v1"} parasite',
            '{"contract":"labfy.agent_model_action.v1","kind":"final",'
            '"text":"a","text":"b"}',
            '[{"contract":"labfy.agent_model_action.v1"}]',
        ]
        for raw in invalid:
            with self.subTest(raw=raw), self.assertRaises(AgentModelProtocolError):
                parse_model_action(raw, set())

    def test_rejects_non_finite_and_over_nested_arguments(self):
        non_finite = (f'{{"contract":"{CONTRACT}","kind":"tool_call",'
                      '"tool_id":"known","arguments":{"value":NaN}}}')
        nested = value = {}
        for _ in range(15):
            value["next"] = {}
            value = value["next"]
        with self.assertRaises(AgentModelProtocolError):
            parse_model_action(non_finite, {"known"})
        with self.assertRaises(AgentModelProtocolError):
            parse_model_action(json.dumps({"contract": CONTRACT, "kind": "tool_call",
                                           "tool_id": "known", "arguments": nested}),
                               {"known"})


if __name__ == "__main__":
    unittest.main()
