import assert from "node:assert/strict";
import { mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
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
    const timer = setTimeout(
      () => reject(new Error("démarrage expiré")),
      10000,
    );
    server.stdout.on("data", (chunk) => {
      output += chunk;
      const url = output.match(/http:\/\/127\.0\.0\.1:\d+\//);
      if (url) {
        clearTimeout(timer);
        resolve({ url: url[0] });
      }
    });
    server.once("exit", (code) =>
      reject(new Error(`serveur arrêté (${code})`)),
    );
  });
}


const workspace = await mkdtemp(join(tmpdir(), "labfy-workbench-populated-"));
const profile = await mkdtemp(
  join(tmpdir(), "labfy-workbench-populated-firefox-"),
);
let browser;
let server;

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

  const authenticationStarted = performance.now();
  await openAutomaticSession(page, session.url);
  await page.waitForSelector(".node");
  const firstRenderMs = Math.round(performance.now() - authenticationStarted);

  await page.click('[data-work-panel="next-panel"]');
  await page.waitForFunction(
    () =>
      document.querySelectorAll("#planner-list input:not(:disabled)").length ===
      6,
  );
  await page.$$eval("#planner-list input:not(:disabled)", (inputs) =>
    inputs.forEach((input) => input.click()),
  );
  await page.select("#planner-profile", "LOCAL_PRUDENT");
  const analysisStarted = performance.now();
  await page.click("#planner-launch");
  await page.waitForFunction(
    () =>
      document.querySelectorAll(".node").length > 6 &&
      document.querySelectorAll(".edge").length > 0,
    { timeout: 30000 },
  );
  await page.waitForFunction(
    () =>
      document.querySelectorAll(".node").length === 62 &&
      document.querySelectorAll(".edge").length === 100,
    { timeout: 30000 },
  );
  const resultAppliedMs = Math.round(performance.now() - analysisStarted);

  const graphMetrics = await page.evaluate(() => ({
    nodes: document.querySelectorAll(".node").length,
    edges: document.querySelectorAll(".edge").length,
    evidence: [
      ...document.querySelectorAll('.node[data-kind="evidence"]'),
    ].every((node) => node.querySelector("rect") !== null),
    extractions: document.querySelectorAll('.node[data-kind="extraction"] path')
      .length,
    observations: document.querySelectorAll(
      '.node[data-kind="observation"] rect',
    ).length,
    directed: document.querySelectorAll('.edge[marker-end="url(#arrowhead)"]')
      .length,
  }));
  assert.deepEqual([graphMetrics.nodes, graphMetrics.edges], [62, 100]);
  assert.equal(graphMetrics.evidence, true);
  assert.ok(graphMetrics.extractions > 0 && graphMetrics.observations > 0);
  assert.ok(graphMetrics.directed > 0);
  await page.click("#reset-layout");
  await page.screenshot({ path: "/tmp/labfy-workbench-populated-network.png" });

  await page.click("#view-infrastructure");
  const infrastructureCount = await page.$$eval(
    ".node",
    (nodes) => nodes.length,
  );
  assert.ok(infrastructureCount < 62);
  await page.click("#reset");
  assert.equal(await page.$$eval(".node", (nodes) => nodes.length), 62);
  assert.equal(
    await page.$eval("#reset", (button) => button.classList.contains("active")),
    true,
  );
  await page.screenshot({
    path: "/tmp/labfy-workbench-populated-navigation.png",
  });

  const snapshotPath = join(workspace, "core-snapshot.json");
  const originalBytes = await readFile(snapshotPath);
  const snapshot = JSON.parse(originalBytes);
  const observation = snapshot.nodes.find(
    (node) => node.object_kind === "observation",
  );
  assert.ok(observation);
  await page.click("#search");
  await page.keyboard.down("Control");
  await page.keyboard.press("A");
  await page.keyboard.up("Control");
  await page.type("#search", observation.label);
  await page.click("#search-button");
  await page.click('[data-panel="provenance-panel"]');
  await page.waitForSelector("#provenance-tab-details button");
  assert.ok(
    (await page.$$eval(
      "#provenance-tab-details button",
      (buttons) => buttons.length,
    )) > 0,
  );
  await page.screenshot({
    path: "/tmp/labfy-workbench-populated-provenance.png",
  });
  const originId = await page.$eval(
    "#provenance-tab-details button",
    (button) => button.dataset.originId,
  );
  await page.click("#provenance-tab-details button");
  assert.equal(
    await page.evaluate(() => window.__LABFY_TEST__.getState().selectedNodeId),
    originId,
  );
  await page.click("#back");
  assert.equal(
    await page.evaluate(() => window.__LABFY_TEST__.getState().selectedNodeId),
    observation.id,
  );

  const heightBefore = await page.$eval(
    "#graph-panel",
    (element) => element.getBoundingClientRect().height,
  );
  await page.click("#work-panel-toggle");

  await page.click("#graph");
  await page.keyboard.press("ArrowRight");
  await page.keyboard.down("Shift");
  await page.keyboard.press("F10");
  await page.keyboard.up("Shift");
  await page.waitForSelector("#node-context-menu:not([hidden])");
  await page.screenshot({ path: "/tmp/labfy-workbench-populated-menu.png" });
  await page.keyboard.press("Escape");
  assert.equal(
    await page.$eval("#node-context-menu", (menu) => menu.hidden),
    true,
  );
  const heightCollapsed = await page.$eval(
    "#graph-panel",
    (element) => element.getBoundingClientRect().height,
  );
  assert.ok(heightCollapsed > heightBefore + 100);
  await page.screenshot({ path: "/tmp/labfy-workbench-populated-drawer.png" });
  await page.click("#work-panel-toggle");

  const changed = structuredClone(snapshot);
  const stableNode = changed.nodes.find(
    (node) => node.object_kind === "evidence",
  );
  stableNode.label = "SPECIMEN UUID CONSTANT ACTUALISÉ";
  await writeFile(snapshotPath, JSON.stringify(changed));
  try {
    const updateStarted = performance.now();
    await page.waitForFunction(
      (id) =>
        [...document.querySelectorAll(".node")]
          .find((node) => node.dataset.id === id)
          ?.textContent.includes("UUID CONSTANT"),
      { timeout: 10000 },
      stableNode.id,
    );
    const updateAppliedMs = Math.round(performance.now() - updateStarted);
    assert.match(
      await page.$eval("#connection", (element) => element.textContent),
      /changements appliqués/,
    );
    assert.equal(await page.$$eval(".node", (nodes) => nodes.length), 62);

    await page.click("#search");
    await page.keyboard.down("Control");
    await page.keyboard.press("A");
    await page.keyboard.up("Control");
    await page.type("#search", stableNode.label);
    await page.click("#search-button");
    await page.click('[data-panel="actions-panel"]');
    await page.click('#actions [data-report-action="toggle"]');
    await page.click('[data-work-panel="reports-panel"]');
    await page.click("#report-preview");
    await page.waitForFunction(
      () => !document.querySelector("#report-generate").disabled,
    );
    await page.type("#report-name", " MODIFIÉ");
    assert.equal(
      await page.$eval("#report-generate", (button) => button.disabled),
      true,
    );
    assert.match(
      await page.$eval(
        "#report-preview-content",
        (element) => element.textContent,
      ),
      /recalculer/,
    );
    await page.click("#report-preview");
    await page.type("#report-comment", " modification pendant aperçu");
    await new Promise((resolve) => setTimeout(resolve, 500));
    assert.equal(
      await page.$eval("#report-generate", (button) => button.disabled),
      true,
    );
    await page.click("#report-preview");
    await page.waitForFunction(
      () => !document.querySelector("#report-generate").disabled,
    );
    await page.screenshot({
      path: "/tmp/labfy-workbench-populated-report.png",
    });

    const interactionStarted = performance.now();
    await page.click("#reset-layout");
    const interactionMs = Math.round(performance.now() - interactionStarted);
    console.log(
      `MESURES workbench alimenté : premier rendu ${firstRenderMs} ms ; ` +
        `résultats appliqués ${resultAppliedMs} ms ; actualisation UUID constante ` +
        `${updateAppliedMs} ms ; interaction ${interactionMs} ms ; ` +
        `${graphMetrics.nodes} nœuds/${graphMetrics.edges} arêtes`,
    );
  } finally {
    await writeFile(snapshotPath, originalBytes);
  }

  await page.setViewport({ width: 700, height: 900 });
  await page.screenshot({ path: "/tmp/labfy-workbench-populated-narrow.png" });
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
    "PASS workbench Firefox : graphe C alimenté, navigation et clôture W",
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
