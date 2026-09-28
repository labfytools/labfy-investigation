import json
import sys
import threading
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from agent_model_protocol import CONTRACT
from agent_runtime import AgentRuntime, AgentRuntimeError
from local_model_client import LocalModelUnavailable


def action(kind, **values):
    return json.dumps({"contract": CONTRACT, "kind": kind, **values})


class FakeModel:
    def __init__(self, outputs):
        self.outputs = list(outputs)
        self.messages = []

    def complete(self, messages):
        self.messages.append(messages)
        value = self.outputs.pop(0)
        if isinstance(value, Exception):
            raise value
        return value


class AgentRuntimeTest(unittest.TestCase):
    def make_runtime(self, outputs, executor=None, **limits):
        self.requests = []
        self.model = FakeModel(outputs)

        def default_executor(request):
            self.requests.append(request)
            return {"contract": "labfy.agent_tool_result.v1", "state": "COMPLETED",
                    "output": {"text": "SPECIMEN tool data"}}

        runtime = AgentRuntime(self.model, executor or default_executor,
                               {"investigation.search", "research.prepare"}, **limits)
        self.addCleanup(runtime.close)
        return runtime

    @staticmethod
    def start(runtime, key="start-1", objective="Trouver SPECIMEN"):
        return runtime.start_turn("scope-a", {"objective": objective,
                                               "idempotency_key": key})

    def test_tool_loop_wraps_untrusted_result_then_completes(self):
        runtime = self.make_runtime([
            action("tool_call", tool_id="investigation.search",
                   arguments={"query": "SPECIMEN"}),
            action("final", text="Synthèse SPECIMEN"),
        ])
        started = self.start(runtime)
        completed = runtime.wait_for_state("scope-a", started["turn_id"], "COMPLETED")
        self.assertEqual(completed["final"], "Synthèse SPECIMEN")
        self.assertEqual(completed["budgets"], {"model_calls": 2, "tool_calls": 1})
        wrapped = json.loads(self.model.messages[1][-1]["content"])
        self.assertEqual(wrapped["trust"], "UNTRUSTED_DATA")
        self.assertEqual(self.requests[0]["tool_id"], "investigation.search")

    def test_authorization_pauses_until_explicit_resume_callback(self):
        gateway_calls = []

        def executor(request):
            gateway_calls.append(request)
            return {"state": "AUTHORIZATION_REQUIRED", "result_id": "SPECIMEN-pending"}

        runtime = self.make_runtime([
            action("tool_call", tool_id="research.prepare", arguments={}),
            action("final", text="Autorisation traitée"),
        ], executor=executor)
        started = self.start(runtime)
        waiting = runtime.wait_for_state("scope-a", started["turn_id"],
                                         "AUTHORIZATION_REQUIRED")
        self.assertEqual(len(self.model.messages), 1)
        self.assertEqual(waiting["pending_call"]["result"]["state"],
                         "AUTHORIZATION_REQUIRED")
        callbacks = []

        def resume(pending):
            callbacks.append(pending)
            return {"state": "COMPLETED", "output": {"authorized": "SPECIMEN"}}

        runtime.resume_turn("scope-a", started["turn_id"], resume)
        completed = runtime.wait_for_state("scope-a", started["turn_id"], "COMPLETED")
        self.assertEqual(completed["final"], "Autorisation traitée")
        self.assertEqual(len(callbacks), 1)

    def test_never_interprets_model_authorization_or_unknown_tool(self):
        runtime = self.make_runtime([
            action("tool_call", tool_id="shell", arguments={}),
            action("tool_call", tool_id="shell", arguments={}),
        ])
        started = self.start(runtime)
        failed = runtime.wait_for_state("scope-a", started["turn_id"],
                                        "MODEL_PROTOCOL_ERROR")
        self.assertEqual(failed["budgets"]["tool_calls"], 0)
        self.assertEqual(self.requests, [])

    def test_model_unavailable_and_budget_exhaustion_are_distinct(self):
        unavailable = self.make_runtime([LocalModelUnavailable("offline")])
        first = self.start(unavailable, key="unavailable")
        state = unavailable.wait_for_state("scope-a", first["turn_id"],
                                           "MODEL_UNAVAILABLE")
        self.assertIn("offline", state["diagnostic"])
        budget = self.make_runtime([
            action("tool_call", tool_id="investigation.search", arguments={"query": "x"})
        ], max_model_calls=1)
        second = self.start(budget, key="budget")
        exhausted = budget.wait_for_state("scope-a", second["turn_id"],
                                          "BUDGET_EXHAUSTED")
        self.assertEqual(exhausted["budgets"]["model_calls"], 1)

    def test_start_is_idempotent_and_conflicting_objective_is_rejected(self):
        runtime = self.make_runtime([action("final", text="ok")])
        first = self.start(runtime, key="same")
        replay = self.start(runtime, key="same")
        self.assertEqual(first["turn_id"], replay["turn_id"])
        with self.assertRaises(AgentRuntimeError) as caught:
            self.start(runtime, key="same", objective="Autre objectif")
        self.assertEqual(caught.exception.status, 409)

    def test_cancel_running_inference_wins_over_late_model_result(self):
        entered = threading.Event()
        release = threading.Event()

        class BlockingModel:
            def complete(self, _messages):
                entered.set()
                release.wait(2)
                return action("final", text="trop tard")

        runtime = AgentRuntime(BlockingModel(), lambda _request: {},
                               {"investigation.search"})
        self.addCleanup(runtime.close)
        started = self.start(runtime, key="cancel")
        self.assertTrue(entered.wait(1))
        cancelled = runtime.cancel_turn("scope-a", started["turn_id"])
        self.assertEqual(cancelled["state"], "RUNNING")
        release.set()
        final = runtime.wait_for_state("scope-a", started["turn_id"], "CANCELLED")
        self.assertIsNone(final["final"])

    def test_events_are_bounded_and_scope_filtered(self):
        runtime = self.make_runtime([action("final", text="ok")], max_events=2)
        started = self.start(runtime)
        runtime.wait_for_state("scope-a", started["turn_id"], "COMPLETED")
        events = runtime.events("scope-a", 0)
        self.assertLessEqual(len(events["events"]), 2)
        self.assertEqual(runtime.events("other", 0)["events"], [])


if __name__ == "__main__":
    unittest.main()
