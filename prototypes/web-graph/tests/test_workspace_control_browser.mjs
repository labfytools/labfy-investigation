import assert from "node:assert/strict";
import { mkdtemp, readFile, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { spawn, spawnSync } from "node:child_process";
import puppeteer from "puppeteer-core";

const REPOSITORY = new URL("../../..", import.meta.url);
const PROTOTYPE = new URL("..", import.meta.url);
const FIREFOX_PATH = process.env.FIREFOX_PATH ?? "/usr/bin/firefox";

function waitForServer(server) {
  return new Promise((resolve, reject) => {
    let output = "";
    const timeout = setTimeout(() => reject(new Error("démarrage J6 expiré")), 8000);
    server.stdout.on("data", (chunk) => {
      output += chunk.toString();
      const url = output.match(/http:\/\/127\.0\.0\.1:(\d+)\//);
      const code = output.match(/Code de session éphémère : ([^\s]+)/);
      if (url && code) {
        clearTimeout(timeout);
        resolve({ url: url[0], code: code[1] });
      }
    });
    server.once("exit", (code) => reject(new Error(`serveur J6 arrêté (${code})`)));
  });
}

async function waitForJob(page, capability, state) {
  await page.waitForFunction((expectedCapability, expectedState) =>
    [...document.querySelectorAll("#jobs-list li")].some((item) =>
      item.textContent.includes(expectedCapability) && item.dataset.jobState === expectedState),
  { timeout: 15000 }, capability, state);
}

async function clickEvidenceCapability(page, evidenceId, capabilityId) {
  await page.$eval(`[data-id="evidence:${evidenceId}"]`, (item) =>
    item.dispatchEvent(new MouseEvent("click", { bubbles: true })));
  await page.$eval(`[data-capability-id="${capabilityId}"]`, (item) => item.click());
}

const workspace = await mkdtemp(join(tmpdir(), "labfy-j6-browser-"));
const profile = await mkdtemp(join(tmpdir(), "labfy-j6-firefox-"));
let browser;
let server;
try {
  const initialized = spawnSync("./tools/local-jobs",
    ["init-j7-specimen", "--workspace", workspace],
    { cwd: REPOSITORY, encoding: "utf8", timeout: 30000 });
  assert.equal(initialized.status, 0, initialized.stderr);
  spawnSync("./tools/local-jobs", ["export", "--workspace", workspace],
    { cwd: REPOSITORY, encoding: "utf8", timeout: 10000 });
  const manifest = JSON.parse(await readFile(join(workspace, ".labfy/runtime/specimen.json")));
  const j7Manifest = JSON.parse(await readFile(join(workspace, ".labfy/runtime/j7-specimen.json")));
  server = spawn("python3", ["workspace_server.py", "--workspace", workspace,
    "--bridge", "../../tools/local-jobs", "--port", "0"], {
    cwd: PROTOTYPE, env: { ...process.env, PYTHONUNBUFFERED: "1" },
    stdio: ["ignore", "pipe", "pipe"],
  });
  const session = await waitForServer(server);
  browser = await puppeteer.launch({ browser: "firefox", executablePath: FIREFOX_PATH,
    headless: true, userDataDir: profile, args: ["--no-remote"] });
  const page = await browser.newPage();
  await page.setViewport({ width: 1440, height: 900 });
  await page.goto(session.url, { waitUntil: "domcontentloaded" });
  assert.ok(await page.$("#login-form"));
  await page.type("#bootstrap-code", session.code);
  await Promise.all([page.waitForNavigation({ waitUntil: "domcontentloaded" }),
    page.click("#login-form button")]);
  await page.waitForSelector(`[data-id="evidence:${manifest.eml_id}"]`);
  assert.match(await page.$eval("#mode-badge", (item) => item.textContent), /Poste local/);

  await page.click("#queue-pause");
  await clickEvidenceCapability(page, manifest.eml_id,
    "labfy.capability.eml_headers.v1");
  await waitForJob(page, "eml_headers", "QUEUED");
  await page.click("#queue-resume");
  await waitForJob(page, "eml_headers", "COMPLETED");
  await page.waitForFunction(() =>
    [...document.querySelectorAll(".node")].some((item) => item.dataset.id.startsWith("extraction:")),
  { timeout: 15000 });

  let completedEml = 1;
  for (const evidenceId of j7Manifest.evidence_ids.slice(2)) {
    await clickEvidenceCapability(page, evidenceId,
      "labfy.capability.eml_headers.v1");
    completedEml++;
    await page.waitForFunction((expected) =>
      [...document.querySelectorAll("#jobs-list li")].filter((item) =>
        item.textContent.includes("eml_headers") &&
        item.dataset.jobState === "COMPLETED").length >= expected,
    { timeout: 15000 }, completedEml);
  }
  await page.waitForFunction(() =>
    document.querySelector("#correlations-list")?.textContent.includes("alice@example.test"),
  { timeout: 15000 });
  assert.match(await page.$eval("#correlations-list", (item) => item.textContent),
    /alice@example\.test/);

  await clickEvidenceCapability(page, manifest.image_id,
    "labfy.capability.exif_metadata.v1");
  await waitForJob(page, "exif_metadata", "COMPLETED");
  await page.waitForFunction(() =>
    document.querySelectorAll('.node[data-id^="extraction:"]').length === 6,
  { timeout: 15000 });
  await page.$eval("#jobs-list", (list) => {
    const item = [...list.querySelectorAll("li")].find((candidate) =>
      candidate.textContent.includes("exif_metadata") &&
      candidate.dataset.jobState === "COMPLETED");
    item.querySelector("button").click();
  });
  await page.waitForFunction(() => document.querySelector("#details").textContent.includes("Alice SPECIMEN"),
    { timeout: 15000 });
  assert.equal(await page.evaluate(() => window.__LABFY_J4_XSS__), undefined);

  await page.click("#queue-pause");
  await clickEvidenceCapability(page, manifest.image_id,
    "labfy.capability.exif_metadata.v1");
  await waitForJob(page, "exif_metadata", "QUEUED");
  assert.ok(await page.$eval("#jobs-list", (list) => {
    const buttons = list.querySelectorAll("li[data-job-state='QUEUED'] button");
    buttons[buttons.length - 1]?.click();
    return buttons.length > 0;
  }));
  await page.waitForFunction(() =>
    [...document.querySelectorAll("#jobs-list li")].some((item) => item.dataset.jobState === "CANCELLED"));
  await page.click("#queue-stop");
  await page.click("#queue-resume");

  await page.screenshot({ path: "/tmp/labfy-j6-workspace-wide.png", fullPage: true });
  await page.setViewport({ width: 700, height: 900 });
  await page.screenshot({ path: "/tmp/labfy-j6-workspace-narrow.png", fullPage: true });

  const database = spawnSync("sqlite3", [join(workspace, "Enquete.sqlite"),
    "SELECT count(*) FROM extractions;"], { encoding: "utf8" });
  assert.equal(database.stdout.trim(), "6");
  console.log("PASS navigateur Firefox : J6 conservé et scénario J7 C/Web multi-preuves corrélé");
} finally {
  if (browser) await browser.close();
  if (server) {
    server.kill("SIGINT");
    await new Promise((resolve) => server.once("exit", resolve));
  }
  await rm(profile, { recursive: true, force: true });
  await rm(workspace, { recursive: true, force: true });
}
