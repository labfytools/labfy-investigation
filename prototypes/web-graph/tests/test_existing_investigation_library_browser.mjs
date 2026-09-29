import assert from "node:assert/strict";
import { spawn, spawnSync } from "node:child_process";
import { once } from "node:events";
import { mkdtemp, mkdir, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import puppeteer from "puppeteer-core";
import { openAutomaticSession } from "./browser_automatic_session.mjs";

const prototype = new URL("..", import.meta.url);
const repository = new URL("../../..", import.meta.url);
const root = await mkdtemp(join(tmpdir(), "labfy-existing-browser-SPECIMEN-"));
const library = join(root, "library-SPECIMEN");
const runtime = join(root, "runtime");
const state = join(root, "state");
const profile = join(root, "firefox-profile");
await mkdir(library, { mode: 0o700 });
await mkdir(runtime, { mode: 0o700 });
await mkdir(state, { mode: 0o700 });

const bridge = new URL("../../../tools/local-jobs", import.meta.url).pathname;
for (const [name, title] of [
  ["Existing-A", "ALPHA-SPECIMEN-ONLY <script>window.hostile = true</script>"],
  ["Existing-B", "BRAVO-SPECIMEN-ONLY"],
]) {
  const created = spawnSync(bridge, ["create-workspace", "--workspace",
    join(library, name), "--title", title], {
    cwd: repository, encoding: "utf8",
  });
  assert.equal(created.status, 0, created.stderr);
}

let server;
let browser;
try {
  server = spawn("python3", ["web_app.py", "serve", "--library", library,
    "--lazy-library", "--port", "0"], {
    cwd: prototype,
    env: { ...process.env, XDG_RUNTIME_DIR: runtime, XDG_STATE_HOME: state,
      PYTHONUNBUFFERED: "1" },
    stdio: ["ignore", "pipe", "pipe"],
  });
  const origin = await new Promise((resolve, reject) => {
    let output = "";
    const timer = setTimeout(() => reject(new Error("démarrage expiré")), 15000);
    server.stdout.on("data", (chunk) => {
      output += chunk.toString();
      const match = output.match(/Labfy Web : (http:\/\/127\.0\.0\.1:\d+)\//);
      if (match) { clearTimeout(timer); resolve(match[1]); }
    });
    server.once("exit", (code) => reject(new Error(`serveur arrêté : ${code}`)));
  });
  browser = await puppeteer.launch({ browser: "firefox", executablePath: "/usr/bin/firefox",
    headless: true, userDataDir: profile, args: ["--no-remote"] });
  const page = await browser.newPage();
  await page.setViewport({ width: 1360, height: 980 });
  await openAutomaticSession(page, origin, "existing library SPECIMEN");

  await page.click("#library-load");
  await page.waitForSelector("#existing-discovery:not([hidden])");
  await page.click("#library-discover");
  await page.waitForFunction(() =>
    document.querySelectorAll(".existing-candidate").length === 2);
  assert.equal(await page.evaluate(() => window.hostile), undefined);
  assert.equal(await page.$eval(".existing-candidate h3", (node) => node.querySelector("script")),
    null);

  const clickCandidate = async (name) => {
    await page.evaluate((wanted) => {
      const card = [...document.querySelectorAll(".existing-candidate")]
        .find((item) => item.querySelector("h3")?.textContent === wanted);
      const button = card?.querySelector("button[data-candidate-id]");
      if (!button) throw new Error(`candidat ${wanted} non enregistrable`);
      button.click();
    }, name);
  };
  const clickWorkspace = async (marker) => {
    await page.evaluate((wanted) => {
      const card = [...document.querySelectorAll(".workspace-card")]
        .find((item) => item.querySelector("h3")?.textContent.includes(wanted));
      const button = card?.querySelector("button[data-workspace-id]");
      if (!button) throw new Error(`workspace ${wanted} absent`);
      button.click();
    }, marker);
  };

  await clickCandidate("Existing-A");
  await page.waitForFunction(() => [...document.querySelectorAll("#workspace-list h3")]
    .some((node) => node.textContent.includes("ALPHA-SPECIMEN-ONLY")));
  await clickWorkspace("ALPHA-SPECIMEN-ONLY");
  await page.waitForSelector("#workbench-shell:not([hidden])");
  await page.waitForFunction(() => document.querySelector("#agent-send")?.disabled === false);
  const workspaceA = await page.evaluate(() => window.__LABFY_TEST__.getState().workspaceId);
  assert.ok(workspaceA);
  await page.evaluate(() => {
    const card = document.createElement("li");
    card.textContent = "ALPHA-SPECIMEN-ONLY stale Agent card";
    document.querySelector(".agent-conversation").append(card);
    document.querySelector("#agent-proposal-list").append(card.cloneNode(true));
    document.querySelector("#agent-toolbox-list").append(card.cloneNode(true));
    document.querySelector("#agent-code-changes-list").append(card.cloneNode(true));
  });

  await page.click("#close-workspace");
  await page.waitForSelector("#library-home:not([hidden])");
  assert.equal(await page.evaluate(() => window.__LABFY_TEST__.getState().workspaceId), null);
  assert.equal(await page.$eval("#agent-send", (button) => button.disabled), true);
  assert.equal(await page.$eval("#agent-status", (node) => node.textContent),
    "Ouvrez une enquête pour utiliser Qwen.");

  await page.click("#library-discover");
  await page.waitForFunction(() => [...document.querySelectorAll(".existing-candidate h3")]
    .some((node) => node.textContent === "Existing-B"));
  await clickCandidate("Existing-B");
  await page.waitForFunction(() => [...document.querySelectorAll("#workspace-list h3")]
    .some((node) => node.textContent === "BRAVO-SPECIMEN-ONLY"));
  await clickWorkspace("BRAVO-SPECIMEN-ONLY");
  await page.waitForSelector("#workbench-shell:not([hidden])");
  await page.waitForFunction(() => window.__LABFY_TEST__.getState().workspaceId !== null);
  const workspaceB = await page.evaluate(() => window.__LABFY_TEST__.getState().workspaceId);
  assert.notEqual(workspaceB, workspaceA);
  assert.equal(await page.$eval("#investigation-title", (node) => node.textContent),
    "BRAVO-SPECIMEN-ONLY");
  for (const selector of [".agent-conversation", "#agent-proposal-list",
    "#agent-toolbox-list", "#agent-code-changes-list", ".activity-stream"]) {
    assert.equal((await page.$eval(selector, (node) => node.textContent))
      .includes("ALPHA-SPECIMEN-ONLY"), false, selector);
  }
  const activeState = await page.evaluate(() => window.__LABFY_TEST__.getState());
  assert.deepEqual(activeState.visibleNodeIds, []);
  assert.equal(activeState.selectedNodeId, null);
  console.log("PASS Firefox bibliothèque existante : register/open/close A→B isolé SPECIMEN");
} finally {
  if (browser) await browser.close();
  if (server?.exitCode === null) {
    server.kill("SIGTERM");
    await once(server, "exit");
  }
  await rm(root, { recursive: true, force: true });
}
