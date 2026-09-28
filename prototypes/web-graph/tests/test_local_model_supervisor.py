import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from local_model_supervisor import (LocalModelConfig, LocalModelSupervisor,
                                    LocalModelSupervisorError, load_xdg_config)


class LocalModelSupervisorConfigTest(unittest.TestCase):
    def test_explicit_config_never_accepts_missing_model(self):
        with tempfile.TemporaryDirectory(prefix="labfy-supervisor-SPECIMEN-") as directory:
            config = Path(directory) / "agent.json"
            config.write_text(json.dumps({"model": {
                "model_path": str(Path(directory) / "missing.gguf"),
                "model_id": "qwen-SPECIMEN",
            }}), encoding="utf-8")
            with self.assertRaises(LocalModelSupervisorError):
                load_xdg_config(config)

    def test_supervisor_command_passes_reasoning_off(self):
        with tempfile.TemporaryDirectory(prefix="labfy-supervisor-SPECIMEN-") as directory:
            model = Path(directory) / "model.gguf"
            model.write_bytes(b"SPECIMEN")
            config = LocalModelConfig(model, "qwen-SPECIMEN", 4096, 1, "off")
            supervisor = LocalModelSupervisor(config, executable="/usr/bin/true")
            argv = supervisor._command(32123)
            self.assertEqual(argv[0], "/usr/bin/true")
            self.assertIn("--reasoning", argv)
            self.assertEqual(argv[argv.index("--reasoning") + 1], "off")
            self.assertIn("--no-warmup", argv)
            self.assertEqual(argv[argv.index("--host") + 1], "127.0.0.1")

    def test_config_accepts_real_regular_specimen_file_only(self):
        with tempfile.TemporaryDirectory(prefix="labfy-supervisor-SPECIMEN-") as directory:
            root = Path(directory)
            model = root / "model.gguf"
            model.write_bytes(b"SPECIMEN")
            config = root / "agent.json"
            config.write_text(json.dumps({"model": {
                "model_path": str(model), "model_id": "qwen-SPECIMEN",
                "context": 4096, "parallel": 1, "reasoning": "off",
            }}), encoding="utf-8")
            value = load_xdg_config(config)
            self.assertIsInstance(value, LocalModelConfig)
            self.assertEqual(value.model_id, "qwen-SPECIMEN")


if __name__ == "__main__":
    unittest.main()
