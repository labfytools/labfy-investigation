import assert from "node:assert/strict";
import { mkdtemp, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { spawn, spawnSync } from "node:child_process";
import puppeteer from "puppeteer-core";
import { openAutomaticSession } from "./browser_automatic_session.mjs";

const REPOSITORY = new URL("../../..", import.meta.url),
  PROTOTYPE = new URL("..", import.meta.url);
const FIREFOX = process.env.FIREFOX_PATH ?? "/usr/bin/firefox";
function ready(server) {
  return new Promise((resolve, reject) => {
    let text = "";
    const timer = setTimeout(() => reject(new Error("démarrage expiré")), 8000);
    server.stdout.on("data", (chunk) => {
      text += chunk;
      const url = text.match(/http:\/\/127\.0\.0\.1:(\d+)\//);
      if (url) {
        clearTimeout(timer);
        resolve({ url: url[0] });
      }
    });
    server.once("exit", (value) =>
      reject(new Error(`serveur arrêté ${value}`)),
    );
  });
}

const workspace = await mkdtemp(join(tmpdir(), "labfy-workbench-")),
  profile = await mkdtemp(join(tmpdir(), "labfy-workbench-firefox-"));
let browser, server;
try {
  assert.equal(
    spawnSync(
      "./tools/local-jobs",
      ["init-j7-specimen", "--workspace", workspace],
      { cwd: REPOSITORY, encoding: "utf8", timeout: 30000 },
    ).status,
    0,
  );
  spawnSync("./tools/local-jobs", ["export", "--workspace", workspace], {
    cwd: REPOSITORY,
    timeout: 10000,
  });
  server = spawn(
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
  const session = await ready(server);
  browser = await puppeteer.launch({
    browser: "firefox",
    executablePath: FIREFOX,
    headless: true,
    userDataDir: profile,
    args: ["--no-remote"],
  });
  const page = await browser.newPage();
  const errors = [];
  page.on("pageerror", (error) => errors.push(String(error)));
  await page.setViewport({ width: 1440, height: 900 });
  const start = Date.now();
  await openAutomaticSession(page, session.url);
  await page.waitForSelector(".node");
  const metrics = await page.evaluate(() => ({
    nodes: document.querySelectorAll(".node").length,
    edges: document.querySelectorAll(".edge").length,
    overflow:
      document.documentElement.scrollWidth >
      document.documentElement.clientWidth,
  }));
  assert.ok(metrics.nodes > 0);
  assert.equal(metrics.overflow, false);
  await page.click(".node");
  await page.screenshot({ path: "/tmp/labfy-workbench-selected.png" });
  await page.click(".node", { button: "right" });
  await page.waitForSelector("#node-context-menu:not([hidden])");
  const menuBox = await page.$eval("#node-context-menu", (element) => {
    const r = element.getBoundingClientRect();
    return {
      left: r.left,
      top: r.top,
      right: r.right,
      bottom: r.bottom,
      w: innerWidth,
      h: innerHeight,
    };
  });
  assert.ok(
    menuBox.left >= 0 &&
      menuBox.top >= 0 &&
      menuBox.right <= menuBox.w &&
      menuBox.bottom <= menuBox.h,
  );
  await page.screenshot({ path: "/tmp/labfy-workbench-context.png" });
  await page.click('#node-context-menu [data-report-action="toggle"]');
  await page.keyboard.press("Escape");
  assert.equal(
    await page.$eval("#node-context-menu", (element) => element.hidden),
    true,
  );
  await page.click('[data-work-panel="next-panel"]');
  await page.screenshot({ path: "/tmp/labfy-workbench-planner.png" });
  await page.click('[data-panel="provenance-panel"]');
  await page.screenshot({ path: "/tmp/labfy-workbench-provenance.png" });
  await page.click('[data-work-panel="reports-panel"]');
  await page.type("#report-comment", "Brouillon conservé après actualisation");
  await page.click("#report-preview");
  await page.waitForFunction(() =>
    document
      .querySelector("#report-preview-content")
      ?.textContent.includes("Coupe"),
  );
  await page.screenshot({ path: "/tmp/labfy-workbench-report.png" });
  await page.reload({ waitUntil: "domcontentloaded" });
  await page.waitForSelector(".node");
  assert.match(
    await page.$eval("#report-comment", (element) => element.value),
    /Brouillon conservé/,
  );
  await page.evaluate(() => {
    const select = document.querySelector("#type-filter"),
      option = new Option("Absent", "__absent__");
    select.add(option);
    select.value = option.value;
    select.dispatchEvent(new Event("change", { bubbles: true }));
  });
  await page.screenshot({ path: "/tmp/labfy-workbench-empty.png" });
  assert.match(
    await page.$eval("#graph-state", (element) => element.textContent),
    /Aucun objet/,
  );
  await page.setViewport({ width: 700, height: 900 });
  await page.click("#reset");
  await page.screenshot({ path: "/tmp/labfy-workbench-narrow.png" });
  assert.equal(
    await page.evaluate(
      () =>
        document.documentElement.scrollWidth >
        document.documentElement.clientWidth,
    ),
    false,
  );
  await page.setViewport({ width: 1366, height: 768 });
  await page.evaluate(() => {
    document.documentElement.style.zoom = "200%";
  });
  assert.equal(
    await page.evaluate(
      () =>
        document.documentElement.scrollWidth >
        document.documentElement.clientWidth,
    ),
    false,
  );
  assert.deepEqual(errors, []);
  console.log(
    `PASS workbench Firefox : ${metrics.nodes} nœuds, ${metrics.edges} arêtes, interaction initiale ${Date.now() - start} ms`,
  );
} finally {
  if (browser) await browser.close();
  if (server) {
    server.kill("SIGINT");
    await new Promise((resolve) => server.once("exit", resolve));
  }
  await rm(profile, { recursive: true, force: true });
  await rm(workspace, { recursive: true, force: true });
}
