import { spawn } from "node:child_process";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";

const host_path = fileURLToPath(new URL("owned_process_host.mjs", import.meta.url));

export const PROCESS_STATUSES = Object.freeze([
  "MISSING", "LAUNCH_ERROR", "EXIT_CODE", "SIGNALED", "TIMEOUT", "INTERRUPTED", "SUCCESS",
]);

function positive_integer(value, name) {
  if (!Number.isSafeInteger(value) || value <= 0) {
    throw new TypeError(`${name} doit être un entier positif.`);
  }
}

function read_linux_identity(pid) {
  const stat = readFileSync(`/proc/${pid}/stat`, "utf8");
  const suffix = stat.slice(stat.lastIndexOf(")") + 2).trim().split(/\s+/);
  return {
    pid,
    process_group_id: Number.parseInt(suffix[2], 10),
    session_id: Number.parseInt(suffix[3], 10),
    start_time: suffix[19],
  };
}

function validate_owned_session(child) {
  const identity = read_linux_identity(child.pid);
  if (identity.process_group_id !== child.pid || identity.session_id !== child.pid) {
    throw new Error(`session détachée invalide pour le PID ${child.pid}`);
  }
  return identity;
}

function signal_owned_group(ownership, signal) {
  if (!ownership.active) return false;

  // INVARIANT: Linux cannot reuse a PGID/SID while a member of this owned
  // session remains. ESRCH closes ownership permanently, so later PID reuse
  // can never become a signal target of this supervisor.
  try {
    process.kill(-ownership.identity.process_group_id, signal);
    return true;
  } catch (error) {
    if (error.code === "ESRCH") {
      ownership.active = false;
      return false;
    }
    throw error;
  }
}

function group_exists(ownership) {
  return signal_owned_group(ownership, 0);
}

function delay(milliseconds) {
  return new Promise((resolve) => setTimeout(resolve, milliseconds));
}

async function stop_owned_group(ownership, term_grace_ms, kill_grace_ms) {
  if (!ownership || !group_exists(ownership)) return;
  signal_owned_group(ownership, "SIGTERM");
  await delay(term_grace_ms);
  if (!group_exists(ownership)) return;
  signal_owned_group(ownership, "SIGKILL");

  const deadline = Date.now() + kill_grace_ms;
  while (Date.now() < deadline && group_exists(ownership)) {
    await delay(Math.min(10, Math.max(1, deadline - Date.now())));
  }
  // WHY: a non-reaped zombie can keep kill(0) true. The verdict remains
  // bounded and ownership is retired instead of ever targeting this PGID again.
  if (group_exists(ownership)) ownership.active = false;
}

function destroy_pipes(child) {
  child.stdout?.unpipe();
  child.stderr?.unpipe();
  child.stdout?.destroy();
  child.stderr?.destroy();
  if (child.connected) child.disconnect();
}

function status_from_exit(code, signal) {
  if (signal !== null) return "SIGNALED";
  if (code !== 0) return "EXIT_CODE";
  return "SUCCESS";
}

export function create_interruption_controller(signals = ["SIGINT", "SIGTERM", "SIGHUP"]) {
  const controller = new AbortController();
  let interrupted_signal = null;
  const handlers = new Map();

  for (const signal of signals) {
    const handler = () => {
      if (controller.signal.aborted) return;
      interrupted_signal = signal;
      controller.abort(signal);
    };
    handlers.set(signal, handler);
    process.on(signal, handler);
  }

  return {
    signal: controller.signal,
    get interrupted_signal() { return interrupted_signal; },
    dispose() {
      for (const [signal, handler] of handlers) process.off(signal, handler);
    },
  };
}

export function supervise_process({
  command,
  args = [],
  cwd = process.cwd(),
  timeout_ms,
  term_grace_ms = 1000,
  kill_grace_ms = 1000,
  abort_signal,
  stdout = process.stdout,
  stderr = process.stderr,
}) {
  positive_integer(timeout_ms, "timeout_ms");
  positive_integer(term_grace_ms, "term_grace_ms");
  positive_integer(kill_grace_ms, "kill_grace_ms");

  return new Promise((resolve) => {
    let ownership = null;
    let target_result = null;
    let stopping_status = null;
    let settled = false;
    let cleanup_started = false;
    let child;
    let timeout;

    const payload = Buffer.from(JSON.stringify({ command, args, cwd }), "utf8").toString("base64url");

    const finish = async (result) => {
      if (settled) return;
      settled = true;
      clearTimeout(timeout);
      abort_signal?.removeEventListener("abort", on_abort);
      destroy_pipes(child);
      await stop_owned_group(ownership, term_grace_ms, kill_grace_ms);
      resolve(result);
    };

    const begin_cleanup = (result) => {
      if (cleanup_started) return;
      cleanup_started = true;
      void finish(result);
    };

    const request_stop = (status, interrupted_signal = null) => {
      if (stopping_status) return;
      stopping_status = status;
      begin_cleanup({
        status,
        code: target_result?.code ?? null,
        signal: target_result?.signal ?? interrupted_signal,
        error: null,
      });
    };

    const on_abort = () => request_stop("INTERRUPTED", abort_signal.reason ?? null);

    try {
      child = spawn(process.execPath, [host_path, payload], {
        cwd,
        env: process.env,
        detached: true,
        stdio: ["ignore", "pipe", "pipe", "ipc"],
      });
    } catch (error) {
      resolve({ status: "LAUNCH_ERROR", code: null, signal: null, error: error.message });
      return;
    }

    // CONTRACT: spawn() returns only after the detached child has performed
    // setsid(2). Capture and validate that fresh identity synchronously, before
    // a pre-aborted signal or a very short timeout can request cleanup.
    try {
      ownership = { identity: validate_owned_session(child), active: true };
    } catch (error) {
      child.kill("SIGKILL");
      destroy_pipes(child);
      resolve({ status: "LAUNCH_ERROR", code: null, signal: null, error: error.message });
      return;
    }

    child.stdout.pipe(stdout, { end: false });
    child.stderr.pipe(stderr, { end: false });

    child.on("message", (message) => {
      if (settled || !message || typeof message !== "object") return;
      if (message.type === "launch_error" || message.type === "host_error") {
        begin_cleanup({
          status: "LAUNCH_ERROR", code: null, signal: null,
          error: message.message ?? "échec de lancement",
        });
        return;
      }
      if (message.type === "result") {
        target_result = { code: message.code ?? null, signal: message.signal ?? null };
        begin_cleanup({
          status: status_from_exit(target_result.code, target_result.signal),
          ...target_result,
          error: null,
        });
      }
    });

    child.once("error", (error) => {
      begin_cleanup({ status: "LAUNCH_ERROR", code: null, signal: null, error: error.message });
    });
    child.once("exit", (code, signal) => {
      if (settled || cleanup_started || target_result) return;
      setImmediate(() => {
        if (settled || cleanup_started || target_result) return;
        begin_cleanup({ status: status_from_exit(code, signal), code, signal, error: null });
      });
    });

    timeout = setTimeout(() => request_stop("TIMEOUT"), timeout_ms);
    abort_signal?.addEventListener("abort", on_abort, { once: true });
    if (abort_signal?.aborted) on_abort();
  });
}
