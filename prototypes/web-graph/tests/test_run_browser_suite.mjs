import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { Writable } from "node:stream";
import test from "node:test";
import {
  create_interruption_controller,
  supervise_process,
} from "./owned_process_supervisor.mjs";
import { run_browser_suite, summarize_browser_results } from "./run_browser_suite.mjs";

const SHORT = { timeout_ms: 1000, term_grace_ms: 40, kill_grace_ms: 120 };

function null_output() {
  return new Writable({ write(_chunk, _encoding, callback) { callback(); } });
}

async function wait_for(predicate, timeout_ms = 1000) {
  const deadline = Date.now() + timeout_ms;
  while (Date.now() < deadline) {
    if (await predicate()) return;
    await new Promise((resolve) => setTimeout(resolve, 10));
  }
  assert.fail("condition non satisfaite avant la limite");
}

async function process_is_running(pid) {
  try {
    const stat = await readFile(`/proc/${pid}/stat`, "utf8");
    return stat.slice(stat.lastIndexOf(")") + 2).split(/\s+/)[0] !== "Z";
  } catch (error) {
    if (error.code === "ENOENT") return false;
    throw error;
  }
}

test("classifie succès, code non nul, signal et lancement impossible", async () => {
  const silent = null_output();
  const common = { ...SHORT, stdout: silent, stderr: silent };

  const success = await supervise_process({
    ...common, command: process.execPath, args: ["-e", "process.exit(0)"],
  });
  const exit_code = await supervise_process({
    ...common, command: process.execPath, args: ["-e", "process.exit(17)"],
  });
  const signaled = await supervise_process({
    ...common,
    command: process.execPath,
    args: ["-e", "process.kill(process.pid, 'SIGUSR2')"],
  });
  const launch_error = await supervise_process({
    ...common, command: "/chemin/SPECIMEN/inexistant", args: [],
  });

  assert.deepEqual(
    [success.status, exit_code.status, signaled.status, launch_error.status],
    ["SUCCESS", "EXIT_CODE", "SIGNALED", "LAUNCH_ERROR"],
  );
  assert.equal(exit_code.code, 17);
  assert.equal(signaled.signal, "SIGUSR2");
  assert.match(launch_error.error, /ENOENT/);
});

test("chaque verdict négatif impose un code global non nul", () => {
  for (const status of ["MISSING", "LAUNCH_ERROR", "EXIT_CODE", "SIGNALED", "TIMEOUT", "INTERRUPTED"]) {
    assert.equal(summarize_browser_results([{ status }], 1).exit_code, 1, status);
  }
  assert.equal(summarize_browser_results([{ status: "SUCCESS" }], 1).exit_code, 0);
});

test("le timeout escalade à SIGKILL quand le scénario ignore SIGTERM", async () => {
  const started = Date.now();
  const result = await supervise_process({
    command: process.execPath,
    args: ["-e", "process.on('SIGTERM', () => {}); setInterval(() => {}, 1000)"],
    timeout_ms: 80,
    term_grace_ms: 40,
    kill_grace_ms: 120,
    stdout: null_output(),
    stderr: null_output(),
  });

  assert.equal(result.status, "TIMEOUT");
  assert.ok(Date.now() - started < 600, "le délai total doit rester borné");
});

test("un descendant qui garde les pipes ouverts est nettoyé sans bloquer close", async () => {
  const directory = await mkdtemp(join(tmpdir(), "labfy-runner-SPECIMEN-"));
  const pid_file = join(directory, "descendant.pid");
  const child_source = [
    "const { spawn } = require('node:child_process');",
    "const { writeFileSync } = require('node:fs');",
    "const descendant = spawn(process.execPath, ['-e', `process.on('SIGTERM', () => {}); setInterval(() => {}, 1000)`], { stdio: 'inherit' });",
    "writeFileSync(process.argv[1], String(descendant.pid));",
    "process.exit(0);",
  ].join("\n");

  try {
    const started = Date.now();
    const result = await supervise_process({
      command: process.execPath,
      args: ["-e", child_source, pid_file],
      cwd: directory,
      ...SHORT,
      stdout: null_output(),
      stderr: null_output(),
    });
    const descendant_pid = Number.parseInt(await readFile(pid_file, "utf8"), 10);

    assert.equal(result.status, "SUCCESS");
    assert.ok(Date.now() - started < 600, "les pipes hérités ne doivent pas retenir le verdict");
    await wait_for(async () => !(await process_is_running(descendant_pid)));
  } finally {
    await rm(directory, { recursive: true, force: true });
  }
});

test("le nettoyage du groupe possédé épargne un témoin indépendant", async () => {
  const witness = spawn(process.execPath, ["-e", "setInterval(() => {}, 1000)"], {
    detached: true,
    stdio: "ignore",
  });
  await new Promise((resolve, reject) => {
    witness.once("spawn", resolve);
    witness.once("error", reject);
  });

  try {
    const result = await supervise_process({
      command: process.execPath,
      args: ["-e", "process.on('SIGTERM', () => {}); setInterval(() => {}, 1000)"],
      timeout_ms: 80,
      term_grace_ms: 40,
      kill_grace_ms: 120,
      stdout: null_output(),
      stderr: null_output(),
    });
    assert.equal(result.status, "TIMEOUT");
    assert.equal(await process_is_running(witness.pid), true);
  } finally {
    try { process.kill(-witness.pid, "SIGKILL"); } catch (error) {
      if (error.code !== "ESRCH") throw error;
    }
  }
});

test("SIGINT interrompt le scénario courant et produit un verdict unique", async () => {
  const interruption = create_interruption_controller(["SIGINT"]);
  const pending = supervise_process({
    command: process.execPath,
    args: ["-e", "setInterval(() => {}, 1000)"],
    ...SHORT,
    abort_signal: interruption.signal,
    stdout: null_output(),
    stderr: null_output(),
  });
  setTimeout(() => process.kill(process.pid, "SIGINT"), 60);

  try {
    const result = await pending;
    assert.equal(result.status, "INTERRUPTED");
    assert.equal(result.signal, "SIGINT");
  } finally {
    interruption.dispose();
  }
});

test("le runner classe MISSING, ne lance pas la suite après interruption et échoue globalement", async () => {
  const directory = await mkdtemp(join(tmpdir(), "labfy-suite-SPECIMEN-"));
  const first = join(directory, "first.mjs");
  const forbidden = join(directory, "forbidden.mjs");
  const marker = join(directory, "forbidden-ran");
  await writeFile(first, "setInterval(() => {}, 1000);\n", "utf8");
  await writeFile(forbidden, `import { writeFileSync } from 'node:fs'; writeFileSync(${JSON.stringify(marker)}, 'bad');\n`, "utf8");
  const interruption = create_interruption_controller(["SIGINT"]);
  const output = { log() {}, error() {} };
  setTimeout(() => process.kill(process.pid, "SIGINT"), 60);

  try {
    const summary = await run_browser_suite({
      scenario_names: ["missing.mjs", "first.mjs", "forbidden.mjs"],
      directory,
      timeout_ms: 1000,
      term_grace_ms: 40,
      kill_grace_ms: 120,
      interruption,
      output,
    });
    assert.deepEqual(summary.results.map(({ status }) => status), ["MISSING", "INTERRUPTED"]);
    assert.equal(summary.exit_code, 1);
    assert.equal(await readFile(marker, "utf8").then(() => true, () => false), false);
  } finally {
    interruption.dispose();
    await rm(directory, { recursive: true, force: true });
  }
});
