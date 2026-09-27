import { spawn } from "node:child_process";

function report(message) {
  if (typeof process.send !== "function") {
    process.exit(70);
    return;
  }
  process.send(message, () => process.exit(0));
}

let launch;
try {
  launch = JSON.parse(Buffer.from(process.argv[2] ?? "", "base64url").toString("utf8"));
} catch (error) {
  report({ type: "host_error", message: error.message });
}

if (launch) {
  let settled = false;
  const child = spawn(launch.command, launch.args, {
    cwd: launch.cwd,
    env: process.env,
    stdio: ["ignore", "inherit", "inherit"],
  });

  child.once("error", (error) => {
    if (settled) return;
    settled = true;
    report({ type: "launch_error", message: error.message, code: error.code ?? null });
  });
  child.once("exit", (code, signal) => {
    if (settled) return;
    settled = true;
    report({ type: "result", code, signal });
  });
}
