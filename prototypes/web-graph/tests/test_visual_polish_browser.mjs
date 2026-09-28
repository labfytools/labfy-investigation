import assert from "node:assert/strict";
import { mkdtemp, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { spawn, spawnSync } from "node:child_process";
import puppeteer from "puppeteer-core";
import { openAutomaticSession } from "./browser_automatic_session.mjs";

const REPOSITORY = new URL("../../..", import.meta.url);
const PROTOTYPE = new URL("..", import.meta.url);
const FIREFOX = process.env.FIREFOX_PATH ?? "/usr/bin/firefox";

function ready(server) {
  return new Promise((resolve, reject) => {
    let output = "";
    const timer = setTimeout(() => reject(new Error("démarrage expiré")), 10000);
    server.stdout.on("data", (chunk) => {
      output += chunk;
      const url = output.match(/http:\/\/127\.0\.0\.1:\d+\//);
      if (url) {
        clearTimeout(timer);
        resolve({ url: url[0] });
      }
    });
    server.once("exit", (code) => reject(new Error(`serveur arrêté (${code})`)));
  });
}

async function screenshot(page, name) {
  await page.screenshot({ path: `/tmp/labfy-visual-polish-${name}.png` });
}

async function assertNoHorizontalOverflow(page) {
  assert.deepEqual(
    await page.evaluate(() =>
      document.documentElement.scrollWidth <= document.documentElement.clientWidth,
    ),
    true,
  );
}

const workspace = await mkdtemp(join(tmpdir(), "labfy-visual-polish-"));
const profile = await mkdtemp(join(tmpdir(), "labfy-visual-polish-firefox-"));
let browser;
let server;
try {
  const init = spawnSync(
    "./tools/local-jobs",
    ["init-j7-specimen", "--workspace", workspace],
    { cwd: REPOSITORY, encoding: "utf8", timeout: 30000 },
  );
  assert.equal(init.status, 0, init.stderr);
  spawnSync("./tools/local-jobs", ["export", "--workspace", workspace], {
    cwd: REPOSITORY, timeout: 10000,
  });
  server = spawn("python3", [
    "workspace_server.py", "--workspace", workspace, "--bridge",
    "../../tools/local-jobs", "--port", "0",
  ], {
    cwd: PROTOTYPE,
    env: { ...process.env, PYTHONUNBUFFERED: "1" },
    stdio: ["ignore", "pipe", "pipe"],
  });
  const session = await ready(server);
  browser = await puppeteer.launch({
    browser: "firefox", executablePath: FIREFOX, headless: true,
    userDataDir: profile, args: ["--no-remote"],
  });
  const page = await browser.newPage();
  const errors = [];
  page.on("pageerror", (error) => errors.push(String(error)));
  await page.setViewport({ width: 1440, height: 900 });
  await openAutomaticSession(page, session.url);
  await page.waitForSelector(".node");
  await assertNoHorizontalOverflow(page);
  await screenshot(page, "1440-network");

  await page.focus("#agent-prompt");
  await page.type("#agent-prompt", "Prépare une piste SPECIMEN sans lancer d’action.");
  await page.keyboard.down("Control");
  await page.keyboard.press("Enter");
  await page.keyboard.up("Control");
  assert.equal(await page.$eval("#agent-prompt", (element) => element.value),
    "Prépare une piste SPECIMEN sans lancer d’action.");
  await page.waitForFunction(() => document.querySelector(".agent-conversation")?.textContent
    .includes("Prépare une piste SPECIMEN"));
  assert.match(
    await page.$eval(".agent-conversation", (element) => element.textContent),
    /Prépare une piste SPECIMEN/,
  );
  await page.type("#agent-prompt", "Brouillon SPECIMEN à conserver pendant le défilement.");
  await page.$eval(".agent-scroll", (element) => { element.scrollTop = element.scrollHeight; });
  assert.deepEqual(
    await page.$eval("#agent-prompt", (element) => ({
      value: element.value,
      visible: element.getBoundingClientRect().bottom > 0,
    })),
    { value: "Prépare une piste SPECIMEN sans lancer d’action.Brouillon SPECIMEN à conserver pendant le défilement.", visible: true },
  );
  await page.click('[data-pane="agent-panel"]');
  await page.click('[data-pane="agent-panel"]');
  assert.equal(
    await page.$eval("#agent-prompt", (element) => element.value),
    "Prépare une piste SPECIMEN sans lancer d’action.Brouillon SPECIMEN à conserver pendant le défilement.",
  );
  await screenshot(page, "1440-agent");

  await page.click('[data-work-panel="research-panel"]');
  await screenshot(page, "1440-authorization");
  await page.select("#activity-filter", "TOOL");
  assert.ok(await page.$$eval("[data-activity-type]:not([hidden])", (items) => items.length) >= 1);
  await page.click("#activity-expert-toggle");
  await page.click('[data-activity-type="TOOL"] summary');
  await screenshot(page, "1440-activity");

  await page.click(".node");
  await page.click('[data-work-panel="provenance-drawer-panel"]');
  assert.match(
    await page.$eval("#drawer-provenance-summary", (element) => element.textContent),
    /Provenance disponible/,
  );
  await screenshot(page, "1440-provenance");

  const firstNode = await page.$(".node");
  const box = await firstNode.boundingBox();
  assert.ok(box);
  await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
  await page.mouse.down();
  await page.mouse.move(box.x + box.width / 2 + 54, box.y + box.height / 2 + 28, { steps: 5 });
  await page.mouse.up();
  assert.ok(Object.keys(await page.evaluate(() => window.__LABFY_TEST__.getState().pinned)).length > 0);
  await screenshot(page, "1440-pinned-selection");

  const agentSeparator = await page.$("#agent-resizer");
  const separatorBox = await agentSeparator.boundingBox();
  assert.ok(separatorBox);
  await page.mouse.move(separatorBox.x + 2, separatorBox.y + 80);
  await page.mouse.down();
  await page.mouse.move(separatorBox.x + 42, separatorBox.y + 80, { steps: 3 });
  await page.mouse.up();
  const layoutBeforeReload = await page.evaluate(() => localStorage.getItem("labfy-layout:agent"));
  assert.ok(Number(layoutBeforeReload) >= 208);
  await page.reload({ waitUntil: "domcontentloaded" });
  await page.waitForSelector(".node");
  assert.equal(await page.evaluate(() => localStorage.getItem("labfy-layout:agent")), layoutBeforeReload);

  await page.click("#graph-fullscreen");
  assert.equal(await page.$eval(".workbench", (element) => element.classList.contains("graph-priority")), true);
  await screenshot(page, "1440-graph-priority");
  await page.keyboard.press("Escape");
  assert.equal(await page.$eval(".workbench", (element) => element.classList.contains("graph-priority")), false);

  for (const [width, height, name] of [[1280, 800, "1280"], [1024, 768, "1024"], [700, 900, "700"]]) {
    await page.setViewport({ width, height });
    await page.waitForFunction(() => document.querySelector("#graph-panel").getBoundingClientRect().height > 0);
    await assertNoHorizontalOverflow(page);
    await screenshot(page, name);
  }
  await page.click("#work-panel-toggle");
  await screenshot(page, "700-drawer-collapsed");
  await page.click("#work-panel-toggle");

  await page.evaluate(() => window.__LABFY_TEST__.processEventData("{invalid"));
  assert.match(await page.$eval("#graph-state", (element) => element.textContent), /JSON illisible/);
  await screenshot(page, "700-error");
  assert.deepEqual(errors, []);
  console.log("PASS visual polish Firefox : 11 captures, panels, persistance, prompt, graphe et erreur");
} finally {
  if (browser) await browser.close();
  if (server) {
    server.kill("SIGINT");
    await new Promise((resolve) => server.once("exit", resolve));
  }
  await rm(profile, { recursive: true, force: true });
  await rm(workspace, { recursive: true, force: true });
}
