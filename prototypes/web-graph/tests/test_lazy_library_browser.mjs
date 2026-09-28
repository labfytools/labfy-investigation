import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { once } from "node:events";
import { mkdtemp, mkdir, rm, stat } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import puppeteer from "puppeteer-core";
import { openAutomaticSession } from "./browser_automatic_session.mjs";

const prototype = new URL("..", import.meta.url);
const root = await mkdtemp(join(tmpdir(), "labfy-lazy-browser-SPECIMEN-"));
const library = join(root, "library-SPECIMEN");
const runtime = join(root, "runtime");
const state = join(root, "state");
const profile = join(root, "firefox-profile");
await mkdir(runtime, { mode: 0o700 });
await mkdir(state, { mode: 0o700 });

let server;
let browser;
try {
  // CONTRACT: the only library passed to this owned server is a fresh SPECIMEN
  // path. The first browser navigation must leave it absent on disk.
  server = spawn("python3", ["web_app.py", "serve", "--library", library,
    "--lazy-library", "--port", "0"], {
    cwd: prototype,
    env: { ...process.env, XDG_RUNTIME_DIR: runtime, XDG_STATE_HOME: state,
      PYTHONUNBUFFERED: "1" },
    stdio: ["ignore", "pipe", "pipe"],
  });
  const origin = await new Promise((resolve, reject) => {
    let output = "";
    const timer = setTimeout(() => reject(new Error("démarrage lazy expiré")), 15000);
    server.stdout.on("data", (chunk) => {
      output += chunk.toString();
      const match = output.match(/Labfy Web : (http:\/\/127\.0\.0\.1:\d+)\//);
      if (match) {
        clearTimeout(timer);
        resolve(match[1]);
      }
    });
    server.once("exit", (code) => reject(new Error(`serveur lazy arrêté : ${code}`)));
  });
  browser = await puppeteer.launch({ browser: "firefox", executablePath: "/usr/bin/firefox",
    headless: true, userDataDir: profile, args: ["--no-remote"] });
  const page = await browser.newPage();
  await page.setViewport({ width: 1360, height: 980 });
  const libraryRequests = [];
  page.on("request", (request) => {
    if (new URL(request.url()).pathname === "/api/v1/library")
      libraryRequests.push(request.url());
  });
  const session = await openAutomaticSession(page, origin, "lazy SPECIMEN");
  assert.equal(session.library_load_required, true);
  assert.equal(session.active_workspace_id, null);
  assert.equal(libraryRequests.length, 0);
  await assert.rejects(stat(library), { code: "ENOENT" });
  assert.equal(await page.$eval("#library-load", (button) => button.hidden), false);
  assert.equal(await page.$eval("#workspace-create", (section) => section.hidden), true);

  await page.click("#library-load");
  await page.waitForSelector("#workspace-create:not([hidden])");
  assert.equal(libraryRequests.length, 1);
  assert.equal((await stat(library)).isDirectory(), true);
  await page.type("#workspace-title", "Enquête lazy SPECIMEN");
  await page.click("#workspace-create-form button");
  await page.waitForSelector(".workspace-card [data-workspace-id]");
  await page.click(".workspace-card [data-workspace-id]");
  await page.waitForSelector("#workbench-shell:not([hidden])");
  assert.equal(await page.$eval("#investigation-title", (node) => node.textContent),
    "Enquête lazy SPECIMEN");
  console.log("PASS Firefox lazy library : aucun accès avant clic, ouverture explicite SPECIMEN");
} finally {
  if (browser) await browser.close();
  if (server?.exitCode === null) {
    server.kill("SIGTERM");
    await once(server, "exit");
  }
  await rm(root, { recursive: true, force: true });
}
