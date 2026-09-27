import assert from "node:assert/strict";
import { mkdtemp, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { spawn, spawnSync } from "node:child_process";

import puppeteer from "puppeteer-core";

const FIREFOX_PATH = process.env.FIREFOX_PATH ?? "/usr/bin/firefox";
const REPOSITORY = new URL("../../..", import.meta.url);
const PROTOTYPE = new URL("..", import.meta.url);
const PERSON = "entity:10000000-0000-4000-8000-000000000031";
const EMAIL = "entity:20000000-0000-4000-8000-000000000031";
const DOMAIN = "entity:20000000-0000-4000-8000-000000000032";
const EVIDENCE = "evidence:30000000-0000-4000-8000-000000000031";
const EXECUTION = "execution:50000000-0000-4000-8000-000000000031";

function waitForServer(server) {
  return new Promise((resolve, reject) => {
    let output = "";
    const timeout = setTimeout(() => reject(new Error("démarrage serveur expiré")), 5000);
    server.stdout.on("data", (chunk) => {
      output += chunk.toString();
      const match = output.match(/http:\/\/127\.0\.0\.1:(\d+)\//);
      if (!match) return;
      clearTimeout(timeout);
      resolve(`http://127.0.0.1:${match[1]}/`);
    });
    server.once("exit", (code) => {
      clearTimeout(timeout);
      reject(new Error(`serveur cœur arrêté prématurément (${code})`));
    });
  });
}

async function clickNode(page, id) {
  await page.$eval(`[data-id="${id}"]`, (element) => {
    element.dispatchEvent(new MouseEvent("click", { bubbles: true }));
  });
}

async function dragNode(page, id, deltaX, deltaY) {
  const node = await page.$(`[data-id="${id}"]`);
  const box = await node.boundingBox();
  assert.ok(box, `boîte du nœud ${id}`);
  const x = box.x + box.width / 2, y = box.y + box.height / 2;
  await page.mouse.move(x, y); await page.mouse.down();
  // Firefox livre les PointerEvents intermédiaires de manière fiable ; un seul
  // saut ne prouve pas le contrat de position épinglée du renderer SVG.
  await page.mouse.move(x + deltaX / 2, y + deltaY / 2, { steps: 3 });
  await page.mouse.move(x + deltaX, y + deltaY, { steps: 3 }); await page.mouse.up();
}

async function inspectCoreMode(page) {
  await page.waitForSelector(`[data-id="${PERSON}"]`);
  const initial = await page.evaluate(() => window.__LABFY_TEST__.getState());
  assert.equal(initial.coreMode, true);
  assert.equal(initial.contract, "labfy.web_graph.snapshot.v2");
  assert.equal(initial.visibleNodeIds.length, 9);
  assert.equal(await page.$$eval(".edge-group", (items) => items.length), 8);
  assert.match(await page.$eval("#mode-badge", (item) => item.textContent), /cœur C/);
  assert.match(await page.$eval("#connection", (item) => item.textContent), /Lecture seule/);

  for (const id of [PERSON, EVIDENCE, EXECUTION]) {
    await clickNode(page, id);
    assert.equal((await page.evaluate(() => window.__LABFY_TEST__.getState())).selectedNodeId, id);
  }
  const observationIds = await page.$$eval('.node[data-type="pseudonym"]', (items) =>
    items.map((item) => item.dataset.id),
  );
  assert.equal(observationIds.length, 1);
  await clickNode(page, observationIds[0]);
  assert.match(await page.$eval("#details", (item) => item.textContent), /unpromoted/);
  assert.match(await page.$eval("#details", (item) => item.textContent), /provenance incomplète/);

  await clickNode(page, EMAIL);
  const origins = await page.$$eval("#provenance-details [data-origin-id]", (items) =>
    items.map((item) => item.dataset.originId),
  );
  assert.ok(origins.length >= 2, "l'e-mail conserve observation et exécution comme origines");

  await clickNode(page, DOMAIN);
  assert.deepEqual(
    await page.$$eval("#actions [data-capability-id]", (items) =>
      items.map((item) => [item.dataset.capabilityId, item.disabled]),
    ),
    [["focus-neighborhood", false], ["rdap", true]],
  );

  await page.focus("#search");
  await page.keyboard.type("Élodie");
  await page.$eval("#search-button", (item) => item.click());
  assert.equal((await page.evaluate(() => window.__LABFY_TEST__.getState())).selectedNodeId, PERSON);
  await page.$eval('[data-capability-id="focus-neighborhood"]', (item) => item.click());
  assert.equal((await page.evaluate(() => window.__LABFY_TEST__.getState())).focusNodeId, PERSON);
  await page.$eval("#back", (item) => item.click());

  await page.select("#type-filter", "missing-type").catch(() => {});
  await page.$eval("#type-filter", (select) => {
    select.add(new Option("missing-type", "missing-type"));
    select.value = "missing-type";
    select.dispatchEvent(new Event("change", { bubbles: true }));
  });
  assert.equal((await page.evaluate(() => window.__LABFY_TEST__.getState())).visibleNodeIds.length, 0);
  await page.$eval("#reset", (item) => item.click());
  await dragNode(page, PERSON, 70, 45);
  assert.ok((await page.evaluate(() => window.__LABFY_TEST__.getState())).pinned[PERSON]);

  await page.setViewport({ width: 700, height: 900 });
  assert.equal(
    (await page.$eval("main", (item) => getComputedStyle(item).gridTemplateColumns)).split(" ").length,
    1,
  );
  await page.screenshot({ path: "/tmp/labfy-core-graph-narrow.png", fullPage: true });
  await page.setViewport({ width: 1440, height: 900 });
  await page.screenshot({ path: "/tmp/labfy-core-graph-wide.png", fullPage: true });
  assert.equal(await page.evaluate(() => window.__LABFY_XSS__), undefined);
  assert.equal(initial.reconnectCount, 0);
}

async function inspectExportError(browser) {
  const server = spawn("python3", ["server.py", "--port", "0", "--core-error", "Export C volontairement indisponible"], {
    cwd: PROTOTYPE,
    env: { ...process.env, PYTHONUNBUFFERED: "1" },
    stdio: ["ignore", "pipe", "pipe"],
  });
  try {
    const url = await waitForServer(server);
    const page = await browser.newPage();
    await page.goto(url, { waitUntil: "domcontentloaded" });
    assert.match(await page.$eval("#connection", (item) => item.textContent), /Erreur d'export cœur/);
    assert.equal(await page.$$eval(".node", (items) => items.length), 0);
    assert.equal((await page.evaluate(() => window.__LABFY_TEST__.getState())).coreMode, true);
    await page.screenshot({ path: "/tmp/labfy-core-graph-error.png", fullPage: true });
    await page.close();
  } finally {
    server.kill("SIGINT");
    await new Promise((resolve) => server.once("exit", resolve));
  }
}

const directory = await mkdtemp(join(tmpdir(), "labfy-core-browser-"));
const profile = await mkdtemp(join(tmpdir(), "labfy-core-firefox-"));
const generation = spawnSync("./tools/core_graph_demo", ["--output-dir", directory], {
  cwd: REPOSITORY,
  encoding: "utf8",
  timeout: 30000,
});
assert.equal(generation.status, 0, generation.stderr);
const server = spawn("python3", ["server.py", "--port", "0", "--core-snapshot", join(directory, "core-snapshot.json")], {
  cwd: PROTOTYPE,
  env: { ...process.env, PYTHONUNBUFFERED: "1" },
  stdio: ["ignore", "pipe", "pipe"],
});
let browser;
try {
  const url = await waitForServer(server);
  browser = await puppeteer.launch({
    browser: "firefox",
    executablePath: FIREFOX_PATH,
    headless: true,
    userDataDir: profile,
    args: ["--no-remote"],
  });
  const page = await browser.newPage();
  await page.setViewport({ width: 1440, height: 900 });
  const errors = [];
  page.on("pageerror", (error) => errors.push(error.message));
  page.on("console", (message) => {
    if (message.type() === "error") errors.push(message.text());
  });
  await page.goto(url, { waitUntil: "domcontentloaded" });
  await inspectCoreMode(page);
  assert.deepEqual(errors, []);
  await inspectExportError(browser);
  console.log("PASS navigateur Firefox : snapshot C, provenance, lecture seule et erreur d'export");
} finally {
  if (browser) await browser.close();
  server.kill("SIGINT");
  await new Promise((resolve) => server.once("exit", resolve));
  await rm(profile, { recursive: true, force: true });
  await rm(directory, { recursive: true, force: true });
}
