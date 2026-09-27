import assert from "node:assert/strict";
import { mkdtemp, readFile, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { spawn, spawnSync } from "node:child_process";

import puppeteer from "puppeteer-core";

const FIREFOX_PATH = process.env.FIREFOX_PATH ?? "/usr/bin/firefox";
const REPOSITORY = new URL("../../..", import.meta.url);
const PROTOTYPE = new URL("..", import.meta.url);
const PRIMARY = "extraction:61000000-0000-4000-8000-000000000031";
const DERIVATIVE = "evidence:62000000-0000-4000-8000-000000000031";

function generate(directory, variant = "default") {
  const result = spawnSync(
    "./tools/eml_graph_demo",
    ["--output-dir", directory, "--variant", variant],
    { cwd: REPOSITORY, encoding: "utf8", timeout: 45000 },
  );
  assert.equal(result.status, 0, result.stderr || result.error?.message);
}

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
      reject(new Error(`serveur EML arrêté prématurément (${code})`));
    });
  });
}

async function clickNode(page, id) {
  await page.$eval(`[data-id="${id}"]`, (element) => {
    element.dispatchEvent(new MouseEvent("click", { bubbles: true }));
  });
}

async function inspectPersistedAnalysis(page, manifest, snapshot) {
  const source = `evidence:${manifest.source_evidence_id}`;
  await page.waitForSelector(`[data-id="${PRIMARY}"]`);
  const state = await page.evaluate(() => window.__LABFY_TEST__.getState());
  assert.equal(state.contract, "labfy.web_graph.snapshot.v3");
  assert.equal(state.coreMode, true);
  assert.equal(snapshot.schema_version, 20);
  assert.equal(snapshot.nodes.filter((node) => node.object_kind === "extraction").length, 2);
  assert.equal(snapshot.nodes.filter((node) => node.object_kind === "observation").length, manifest.total_observations);
  assert.equal(snapshot.nodes.filter((node) => node.state === "unpromoted").length, manifest.total_observations);
  assert.ok(snapshot.nodes.every((node) =>
    node.object_kind !== "observation" ||
    (node.details.review_state === "proposed" && node.details.provenance_complete === true)
  ));
  assert.ok(snapshot.nodes.some((node) =>
    node.details?.value_raw === "Elodie@Atelier.test" &&
    node.details?.value_normalized === "elodie@atelier.test"
  ));
  assert.ok(snapshot.edges.some((edge) =>
    edge.source === source && edge.target === PRIMARY && edge.semantic === "analysis_input"
  ));
  assert.ok(snapshot.edges.some((edge) =>
    edge.source === PRIMARY && edge.target === DERIVATIVE && edge.semantic === "analysis_derivative"
  ));
  assert.equal(await page.evaluate(() => window.__LABFY_EML_XSS__), undefined);
  assert.match(await page.$eval(`[data-id="${source}"]`, (item) => item.textContent), /<img onerror=/);

  for (const id of [source, PRIMARY, DERIVATIVE]) await clickNode(page, id);
  assert.match(await page.$eval("#details", (item) => item.textContent), /derived/);
  await clickNode(page, PRIMARY);
  assert.match(await page.$eval("#details", (item) => item.textContent), /labfy\.eml_analyzer/);
  assert.match(await page.$eval("#details", (item) => item.textContent), /persisted/);

  const observation = snapshot.nodes.find((node) =>
    node.object_kind === "observation" && node.details.value_raw === "Elodie@Atelier.test"
  );
  await clickNode(page, observation.id);
  const details = await page.$eval("#details", (item) => item.textContent);
  assert.match(details, /Elodie@Atelier\.test/);
  assert.match(details, /elodie@atelier\.test/);
  assert.match(details, /proposed/);
  const origins = await page.$$eval("#provenance-details [data-origin-id]", (items) =>
    items.map((item) => item.dataset.originId),
  );
  assert.ok(origins.includes(source), "la provenance remonte jusqu'à la preuve originale");

  await page.$eval('[data-capability-id="focus-neighborhood"]', (item) => item.click());
  assert.equal((await page.evaluate(() => window.__LABFY_TEST__.getState())).focusNodeId, observation.id);
  await page.$eval("#back", (item) => item.click());
  await page.focus("#search");
  await page.keyboard.type("Elodie@Atelier.test");
  await page.$eval("#search-button", (item) => item.click());
  assert.equal((await page.evaluate(() => window.__LABFY_TEST__.getState())).selectedNodeId, observation.id);

  /* Firefox BiDi refuse une action physique hors viewport même si le graphe
   * vient de recalculer sa hauteur. Agrandir avant le drag rend la barrière
   * géométrique déterministe, sans modifier le comportement testé. */
  await page.setViewport({ width: 1440, height: 1200 });
  await page.$eval(`[data-id="${PRIMARY}"]`, (item) =>
    item.scrollIntoView({ block: "center", inline: "center" })
  );
  const element = await page.$(`[data-id="${PRIMARY}"]`);
  const box = await element.boundingBox();
  await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
  await page.mouse.down();
  await page.mouse.move(box.x + box.width / 2 + 60, box.y + box.height / 2 + 35, { steps: 4 });
  await page.mouse.up();
  assert.ok((await page.evaluate(() => window.__LABFY_TEST__.getState())).pinned[PRIMARY]);

  await page.setViewport({ width: 700, height: 900 });
  assert.equal(
    (await page.$eval("main", (item) => getComputedStyle(item).gridTemplateColumns)).split(" ").length,
    1,
  );
  await page.screenshot({ path: "/tmp/labfy-eml-graph-narrow.png", fullPage: true });
  await page.setViewport({ width: 1440, height: 900 });
  await page.screenshot({ path: "/tmp/labfy-eml-graph-wide.png", fullPage: true });
}

async function inspectError(browser) {
  const server = spawn("python3", ["server.py", "--port", "0", "--core-error", "Analyse EML volontairement indisponible"], {
    cwd: PROTOTYPE,
    env: { ...process.env, PYTHONUNBUFFERED: "1" },
    stdio: ["ignore", "pipe", "pipe"],
  });
  try {
    const page = await browser.newPage();
    await page.goto(await waitForServer(server), { waitUntil: "domcontentloaded" });
    assert.match(await page.$eval("#connection", (item) => item.textContent), /Erreur d'export cœur/);
    assert.equal(await page.$$eval(".node", (items) => items.length), 0);
    await page.screenshot({ path: "/tmp/labfy-eml-graph-error.png", fullPage: true });
    await page.close();
  } finally {
    server.kill("SIGINT");
    await new Promise((resolve) => server.once("exit", resolve));
  }
}

const directory = await mkdtemp(join(tmpdir(), "labfy-eml-browser-"));
const alternateDirectory = await mkdtemp(join(tmpdir(), "labfy-eml-alternate-"));
const profile = await mkdtemp(join(tmpdir(), "labfy-eml-firefox-"));
let browser;
let server;
try {
  generate(directory);
  generate(alternateDirectory, "alternate");
  const manifest = JSON.parse(await readFile(join(directory, "generation-manifest.json"), "utf8"));
  const snapshot = JSON.parse(await readFile(join(directory, "core-snapshot.json"), "utf8"));
  const alternate = JSON.parse(await readFile(join(alternateDirectory, "core-snapshot.json"), "utf8"));
  assert.ok(alternate.nodes.some((node) => node.details?.value_normalized === "maelle@variant.test"));
  assert.ok(!alternate.nodes.some((node) => node.details?.value_normalized === "elodie@atelier.test"));
  server = spawn("python3", ["server.py", "--port", "0", "--core-snapshot", join(directory, "core-snapshot.json")], {
    cwd: PROTOTYPE,
    env: { ...process.env, PYTHONUNBUFFERED: "1" },
    stdio: ["ignore", "pipe", "pipe"],
  });
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
  page.on("console", (message) => { if (message.type() === "error") errors.push(message.text()); });
  await page.goto(url, { waitUntil: "domcontentloaded" });
  await inspectPersistedAnalysis(page, manifest, snapshot);
  assert.deepEqual(errors, []);
  await inspectError(browser);
  console.log("PASS navigateur Firefox : analyse EML C persistée, provenance et variante");
} finally {
  if (browser) await browser.close();
  if (server) {
    server.kill("SIGINT");
    await new Promise((resolve) => server.once("exit", resolve));
  }
  await rm(profile, { recursive: true, force: true });
  await rm(alternateDirectory, { recursive: true, force: true });
  await rm(directory, { recursive: true, force: true });
}
