import assert from "node:assert/strict";
import http from "node:http";
import { mkdtemp, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { spawnSync } from "node:child_process";
import puppeteer from "puppeteer-core";
import { openAutomaticSession } from "./browser_automatic_session.mjs";

const REPOSITORY = new URL("../../..", import.meta.url);
const PROTOTYPE = new URL("..", import.meta.url);
const FIREFOX = process.env.FIREFOX_PATH ?? "/usr/bin/firefox";

function startLauncher(library, runtime, state) {
  const result = spawnSync("./tools/labfy", ["--library", library, "--port", "0"], {
    cwd: REPOSITORY,
    env: { ...process.env, XDG_RUNTIME_DIR: runtime, XDG_STATE_HOME: state },
    encoding: "utf8",
  });
  assert.equal(result.status, 0, result.stderr);
  const match = result.stdout.match(/Labfy Web : (http:\/\/127\.0\.0\.1:\d+)\//);
  assert.ok(match, `origine du lanceur absente: ${result.stdout}`);
  return match[1];
}

function stopLauncher(runtime, state) {
  const result = spawnSync("python3", ["web_app.py", "stop"], {
    cwd: PROTOTYPE,
    env: { ...process.env, XDG_RUNTIME_DIR: runtime, XDG_STATE_HOME: state },
    encoding: "utf8",
  });
  assert.equal(result.status, 0, result.stderr);
}

function requestEvilOrigin(origin, cookie, csrf) {
  const body = JSON.stringify({ title: "Refusé SPECIMEN",
    idempotency_key: "87000000-0000-4000-8000-000000000002" });
  return new Promise((resolve, reject) => {
    const target = new URL("/api/v1/library/workspaces", origin);
    const request = http.request(target, { method: "POST", headers: {
      Cookie: `${cookie.name}=${cookie.value}`, Origin: "http://evil.example",
      "Content-Type": "application/json", "Content-Length": Buffer.byteLength(body),
      "X-Labfy-CSRF": csrf } }, (response) => {
      response.resume();
      response.once("end", () => resolve(response.statusCode));
    });
    request.once("error", reject);
    request.end(body);
  });
}

const library = await mkdtemp(join(tmpdir(), "labfy-direct-library-SPECIMEN-"));
const profile = await mkdtemp(join(tmpdir(), "labfy-direct-firefox-SPECIMEN-"));
const runtime = await mkdtemp(join(tmpdir(), "labfy-direct-runtime-SPECIMEN-"));
const state = await mkdtemp(join(tmpdir(), "labfy-direct-state-SPECIMEN-"));
let browser;
let started = false;
try {
  const origin = startLauncher(library, runtime, state);
  started = true;
  browser = await puppeteer.launch({ browser: "firefox", executablePath: FIREFOX,
    headless: true, userDataDir: profile, args: ["--no-remote"] });
  const page = await browser.newPage();
  const session = await openAutomaticSession(page, origin);
  assert.equal(await page.$("#login-form"), null);
  assert.equal(await page.$("#bootstrap-code"), null);
  assert.equal((await page.goto(`${origin}/login.html`)).status(), 404);
  assert.equal((await page.goto(`${origin}/login.js`)).status(), 404);
  await page.goto(origin, { waitUntil: "domcontentloaded" });
  const valid = await page.evaluate(async ({ csrf }) => {
    const valid = await fetch("/api/v1/library/workspaces", {
      method: "POST", headers: { "Content-Type": "application/json",
        "X-Labfy-CSRF": csrf },
      body: JSON.stringify({ title: "Direct SPECIMEN",
        idempotency_key: "87000000-0000-4000-8000-000000000001" }),
    });
    return valid.status;
  }, session);
  assert.equal(valid, 201);
  const cookie = (await page.cookies()).find((item) => item.name.startsWith("labfy_session_"));
  assert.ok(cookie);
  assert.equal(await requestEvilOrigin(origin, cookie, session.csrf), 403);
  console.log("PASS session directe Firefox : lanceur, workbench, CSRF, Origin et arrêt");
} finally {
  if (browser) await browser.close();
  if (started) stopLauncher(runtime, state);
  await Promise.all([library, profile, runtime, state].map((path) =>
    rm(path, { recursive: true, force: true })));
}
