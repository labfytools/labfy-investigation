import assert from "node:assert/strict";
import { mkdtemp, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { spawn } from "node:child_process";
import puppeteer from "puppeteer-core";

const PROTOTYPE = new URL("..", import.meta.url);
const FIREFOX = process.env.FIREFOX_PATH ?? "/usr/bin/firefox";
const SERVER_PROGRAM = String.raw`
import json, signal, sys, threading
from pathlib import Path
from web_library import WebLibrary
from workspace_server import Handler, WorkspaceServer

root = Path(sys.argv[1])
bridge = Path(sys.argv[2])
library = WebLibrary(root, bridge)
inactive = root / ".inactive"
inactive.mkdir(mode=0o700, exist_ok=True)
# CONTRACT: expiration réelle et courte, injectée uniquement dans ce processus
# de test. Aucune route ne permet de modifier l'horloge ou le TTL.
server = WorkspaceServer(("127.0.0.1", 0), Handler, workspace=inactive,
    bridge=bridge, bootstrap="session-browser-SPECIMEN", library=library,
    instance_id="session-browser-SPECIMEN", config_id="session-browser-SPECIMEN",
    session_ttl_seconds=5)
print(json.dumps({"origin": server.origin,
    "code": "session-browser-SPECIMEN"}), flush=True)
signal.signal(signal.SIGTERM,
    lambda _signum, _frame: threading.Thread(target=server.shutdown,
                                              daemon=True).start())
try:
    server.serve_forever()
finally:
    server.server_close()
`;

function startServer(library, runtime, state) {
  return spawn("python3", ["-c", SERVER_PROGRAM, library,
    "../../tools/local-jobs"], {
    cwd: PROTOTYPE,
    env: { ...process.env, PYTHONUNBUFFERED: "1", XDG_RUNTIME_DIR: runtime,
      XDG_STATE_HOME: state },
    stdio: ["ignore", "pipe", "pipe"],
  });
}

function ready(server) {
  return new Promise((resolve, reject) => {
    let output = "";
    let errors = "";
    const timer = setTimeout(() => reject(new Error(
      `démarrage Web expiré: ${errors}`)), 10000);
    server.stderr.on("data", (chunk) => { errors += chunk; });
    server.stdout.on("data", (chunk) => {
      output += chunk;
      const newline = output.indexOf("\n");
      if (newline < 0) return;
      clearTimeout(timer);
      resolve(JSON.parse(output.slice(0, newline)));
    });
    server.once("exit", (code) => reject(new Error(
      `serveur arrêté (${code}): ${errors}`)));
  });
}

async function stopServer(server) {
  if (!server || server.exitCode !== null) return;
  const exited = new Promise((resolve) => server.once("exit", resolve));
  server.kill("SIGTERM");
  await exited;
}

async function login(page, session) {
  await page.goto(session.origin, { waitUntil: "domcontentloaded" });
  await page.waitForSelector("#login-form");
  await page.type("#bootstrap-code", session.code);
  await Promise.all([
    page.waitForNavigation({ waitUntil: "domcontentloaded" }),
    page.click("#login-form button"),
  ]);
  await page.waitForSelector("#library-home:not([hidden])");
}

async function createAndOpenSpecimen(page) {
  const title = "Enquête session Firefox SPECIMEN";
  await page.type("#workspace-title", title);
  await page.click("#workspace-create-form button");
  await page.waitForFunction((expected) => [...document.querySelectorAll(
    ".workspace-card h3")].some((heading) => heading.textContent === expected), {}, title);
  await page.click(".workspace-card [data-workspace-id]");
  await page.waitForFunction(() => document.querySelector("#connection")
    ?.textContent.includes("Ouverte"), { timeout: 15000 });
  return page.evaluate(() => window.__LABFY_TEST__.getState().workspaceId);
}

function delay(milliseconds) {
  return new Promise((resolve) => setTimeout(resolve, milliseconds));
}

const library = await mkdtemp(join(tmpdir(), "labfy-session-library-SPECIMEN-"));
const profile = await mkdtemp(join(tmpdir(), "labfy-session-firefox-SPECIMEN-"));
const runtime = await mkdtemp(join(tmpdir(), "labfy-session-runtime-SPECIMEN-"));
const state = await mkdtemp(join(tmpdir(), "labfy-session-state-SPECIMEN-"));
let browser;
let server;
try {
  server = startServer(library, runtime, state);
  const session = await ready(server);
  assert.notEqual(new URL(session.origin).port, "8081");
  assert.notEqual(new URL(session.origin).port, "8080");
  // CONTRACT: le serveur peut être déjà ancien quand Firefox soumet son premier
  // formulaire. La connexion réussie doit alors commencer une nouvelle durée.
  await delay(5500);
  browser = await puppeteer.launch({ browser: "firefox", executablePath: FIREFOX,
    headless: true, userDataDir: profile, args: ["--no-remote"] });
  const page = await browser.newPage();
  const pageErrors = [];
  const unauthorized = [];
  page.on("pageerror", (error) => pageErrors.push(String(error)));
  page.on("response", (response) => {
    if (response.status() === 401 && new URL(response.url()).pathname.startsWith("/api/"))
      unauthorized.push(response.url());
  });

  await login(page, session);
  const workspaceId = await createAndOpenSpecimen(page);
  assert.ok(workspaceId);
  await page.$eval("#report-comment", (input) => {
    input.value = "Brouillon conservé après expiration SPECIMEN";
    input.dispatchEvent(new Event("input", { bubbles: true }));
  });
  const before = await page.evaluate(async () => {
    const response = await fetch("/api/v1/session", { cache: "no-store" });
    return response.json();
  });
  const oldCookie = (await page.cookies()).find((cookie) =>
    cookie.name.startsWith("labfy_session_"));
  assert.ok(oldCookie);

  // WHY: un polling réel doit découvrir l'expiration et arrêter ses trois
  // pairs ; le test n'appelle aucune route d'administration.
  await page.waitForSelector("#session-reconnect", { timeout: 10000 });
  const expiredState = await page.evaluate(() => window.__LABFY_TEST__.getState());
  assert.equal(expiredState.sessionExpired, true);
  assert.equal(expiredState.refreshTimerCount, 0);
  assert.equal(expiredState.hasCsrf, false);
  assert.equal(expiredState.workspaceId, null);
  assert.equal(await page.$eval("#workbench-shell", (item) => item.hidden), true);
  assert.match(await page.$eval("#library-connection", (item) => item.textContent),
    /travail en cours a été arrêté/i);
  assert.equal(await page.$$eval("#session-reconnect", (items) => items.length), 1);
  const requestCount = unauthorized.length;
  await new Promise((resolve) => setTimeout(resolve, 1400));
  assert.equal(unauthorized.length, requestCount,
    "aucune rafale API après la première transition d'expiration");
  assert.equal(requestCount, 1, "un seul polling observe le 401 avant l'annulation");
  assert.equal(await page.evaluate(() => Object.keys(sessionStorage)
    .some((key) => key.startsWith("labfy-report-draft:"))), true);

  await page.click("#session-reconnect");
  await page.waitForSelector("#login-form");
  await page.type("#bootstrap-code", session.code);
  await Promise.all([
    page.waitForNavigation({ waitUntil: "domcontentloaded" }),
    page.click("#login-form button"),
  ]);
  await page.waitForSelector("#library-home:not([hidden])");
  const renewed = await page.evaluate(async () => {
    const response = await fetch("/api/v1/session", { cache: "no-store" });
    return response.json();
  });
  assert.notEqual(renewed.csrf, before.csrf);
  assert.equal(await page.$eval(".workspace-card h3", (item) => item.textContent),
    "Enquête session Firefox SPECIMEN");
  await page.click(`.workspace-card [data-workspace-id="${workspaceId}"]`);
  await page.waitForFunction((expected) =>
    window.__LABFY_TEST__.getState().workspaceId === expected, {}, workspaceId);

  const staleHeaders = { Cookie: `${oldCookie.name}=${oldCookie.value}` };
  assert.equal((await fetch(`${session.origin}/api/v1/session`,
    { headers: staleHeaders })).status, 401);
  assert.equal((await fetch(`${session.origin}/api/v1/queue/pause`, {
    method: "POST",
    headers: { ...staleHeaders, Origin: session.origin,
      "Content-Type": "application/json", "X-Labfy-CSRF": before.csrf },
    body: "{}",
  })).status, 403);
  assert.deepEqual(pageErrors, []);
  console.log("PASS Firefox session : expiration réelle, arrêt unique, reconnexion, " +
    "contexte SPECIMEN et rotation cookie/CSRF");
} finally {
  if (browser) await browser.close();
  await stopServer(server);
  await Promise.all([library, profile, runtime, state].map((path) =>
    rm(path, { recursive: true, force: true })));
}
