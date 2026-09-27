import assert from "node:assert/strict";
import { mkdtemp, readFile, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { spawn, spawnSync } from "node:child_process";
import puppeteer from "puppeteer-core";
const REPOSITORY = new URL("../../..", import.meta.url),
  PROTOTYPE = new URL("..", import.meta.url);
const FIREFOX_PATH = process.env.FIREFOX_PATH ?? "/usr/bin/firefox";
function ready(server) {
  return new Promise((resolve, reject) => {
    let text = "";
    const timer = setTimeout(
      () => reject(new Error("démarrage J8 expiré")),
      8000,
    );
    server.stdout.on("data", (chunk) => {
      text += chunk;
      const u = text.match(/http:\/\/127\.0\.0\.1:(\d+)\//),
        c = text.match(/Code de session éphémère : ([^\s]+)/);
      if (u && c) {
        clearTimeout(timer);
        resolve({ url: u[0], code: c[1] });
      }
    });
    server.once("exit", (code) =>
      reject(new Error(`serveur J8 arrêté (${code})`)),
    );
  });
}
function start(workspace) {
  return spawn(
    "python3",
    [
      "workspace_server.py",
      "--workspace",
      workspace,
      "--bridge",
      "../../tools/local-jobs",
      "--port",
      "0",
    ],
    {
      cwd: PROTOTYPE,
      env: { ...process.env, PYTHONUNBUFFERED: "1" },
      stdio: ["ignore", "pipe", "pipe"],
    },
  );
}
async function stop(server) {
  server.kill("SIGINT");
  await new Promise((resolve) => server.once("exit", resolve));
}
async function login(page, session) {
  await page.goto(session.url, { waitUntil: "domcontentloaded" });
  await page.type("#bootstrap-code", session.code);
  await Promise.all([
    page.waitForNavigation({ waitUntil: "domcontentloaded" }),
    page.click("#login-form button"),
  ]);
}
const workspace = await mkdtemp(join(tmpdir(), "labfy-j8-browser-")),
  profile = await mkdtemp(join(tmpdir(), "labfy-j8-firefox-"));
let browser, server;
try {
  let result = spawnSync(
    "./tools/local-jobs",
    ["init-j7-specimen", "--workspace", workspace],
    { cwd: REPOSITORY, encoding: "utf8", timeout: 30000 },
  );
  assert.equal(result.status, 0, result.stderr);
  spawnSync("./tools/local-jobs", ["export", "--workspace", workspace], {
    cwd: REPOSITORY,
    timeout: 10000,
  });
  const before = JSON.parse(
    await readFile(join(workspace, "planner-snapshot.json")),
  );
  assert.equal(
    before.recommendations.filter((x) => x.kind === "ANALYSIS" && x.available)
      .length >= 2,
    true,
  );
  server = start(workspace);
  const session = await ready(server);
  browser = await puppeteer.launch({
    browser: "firefox",
    executablePath: FIREFOX_PATH,
    headless: true,
    userDataDir: profile,
    args: ["--no-remote"],
  });
  const page = await browser.newPage();
  const errors = [];
  page.on("pageerror", (error) => errors.push(String(error)));
  page.on("console", (message) => {
    if (message.type() === "error") errors.push(message.text());
  });
  await page.setViewport({ width: 1440, height: 900 });
  await login(page, session);
  await page.waitForFunction(
    () =>
      document.querySelectorAll("#planner-list input:not(:disabled)").length >=
      2,
    { timeout: 10000 },
  );
  const refused = await page.evaluate(async () => {
    const session = await fetch("/api/v1/session").then((r) => r.json()),
      planner = await fetch("/api/v1/planner").then((r) => r.json()),
      ids = planner.recommendations
        .filter((x) => x.kind === "ANALYSIS" && x.available)
        .slice(0, 3)
        .map((x) => x.id);
    const response = await fetch("/api/v1/plans", {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
        "X-Labfy-CSRF": session.csrf,
      },
      body: JSON.stringify({
        recommendation_ids: ids,
        input_revision: planner.input_revision,
        profile_id: "SPECIMEN_SMALL",
        idempotency_key: "89000000-0000-4000-8000-000000000001",
      }),
    });
    return { status: response.status, body: await response.json() };
  });
  assert.equal(refused.status, 409);
  assert.match(refused.body.message, /budget|Profil/i);
  assert.equal(
    JSON.parse(await readFile(join(workspace, "jobs-snapshot.json"))).plans
      .length,
    0,
  );
  await page.screenshot({
    path: "/tmp/labfy-j8-budget-error.png",
    fullPage: true,
  });
  await page.click('[data-work-panel="next-panel"]');
  await page.$$eval("#planner-list input:not(:disabled)", (items) =>
    items.slice(0, 2).forEach((item) => item.click()),
  );
  await page.select("#planner-profile", "SPECIMEN_SMALL");
  await page.click("#planner-launch");
  await page.waitForFunction(
    () =>
      document
        .querySelector("#plans-list")
        ?.textContent.includes("COMPLETED_SUCCESS"),
    { timeout: 20000 },
  );
  assert.match(
    await page.$eval("#plans-list", (x) => x.textContent),
    /tentatives consommées 2/,
  );
  await page.waitForFunction(
    () =>
      document
        .querySelector("#planner-list")
        ?.textContent.includes("RESULT_ALREADY_PRESENT") ||
      document
        .querySelector("#planner-list")
        ?.textContent.includes("déjà publiée"),
    { timeout: 15000 },
  );
  await page.screenshot({ path: "/tmp/labfy-j8-wide.png", fullPage: true });
  await page.setViewport({ width: 700, height: 900 });
  await page.screenshot({ path: "/tmp/labfy-j8-narrow.png", fullPage: true });
  const snapshot = JSON.parse(
    await readFile(join(workspace, "jobs-snapshot.json")),
  );
  assert.equal(snapshot.plans.length, 1);
  assert.equal(snapshot.plans[0].state, "COMPLETED_SUCCESS");
  assert.equal(snapshot.plans[0].consumed_attempts, 2);
  const extractionBefore = spawnSync(
    "sqlite3",
    [join(workspace, "Enquete.sqlite"), "SELECT count(*) FROM extractions;"],
    { encoding: "utf8" },
  ).stdout.trim();
  await stop(server);
  server = null;
  server = start(workspace);
  const restarted = await ready(server);
  await login(page, restarted);
  await page.waitForFunction(
    () =>
      document
        .querySelector("#plans-list")
        ?.textContent.includes("COMPLETED_SUCCESS"),
    { timeout: 10000 },
  );
  const after = JSON.parse(
    await readFile(join(workspace, "jobs-snapshot.json")),
  );
  assert.deepEqual(after.plans, snapshot.plans);
  assert.equal(
    spawnSync(
      "sqlite3",
      [join(workspace, "Enquete.sqlite"), "SELECT count(*) FROM extractions;"],
      { encoding: "utf8" },
    ).stdout.trim(),
    extractionBefore,
  );
  assert.deepEqual(errors, []);
  console.log(
    "PASS navigateur Firefox : J8 A budget atomique, nominal, captures et G redémarrage idempotent",
  );
} finally {
  if (browser) await browser.close();
  if (server) await stop(server);
  await rm(profile, { recursive: true, force: true });
  await rm(workspace, { recursive: true, force: true });
}
