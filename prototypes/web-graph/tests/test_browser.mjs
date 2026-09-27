import assert from "node:assert/strict";
import { mkdtemp, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { spawn } from "node:child_process";

import puppeteer from "puppeteer-core";

const FIREFOX_PATH = process.env.FIREFOX_PATH ?? "/usr/bin/firefox";
const WIDE_SCREENSHOT = "/tmp/labfy-web-graph-wide.png";
const NARROW_SCREENSHOT = "/tmp/labfy-web-graph-narrow-empty.png";
const ERROR_SCREENSHOT = "/tmp/labfy-web-graph-error.png";

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
      reject(new Error(`serveur arrêté prématurément (${code})`));
    });
  });
}

async function clickNode(page, nodeId) {
  await page.$eval(`[data-id="${nodeId}"]`, (element) => {
    element.dispatchEvent(new MouseEvent("click", { bubbles: true }));
  });
}

async function focusElement(page, selector) {
  await page.$eval(selector, (element) => element.focus());
}

async function state(page) {
  return page.evaluate(() => window.__LABFY_TEST__.getState());
}

async function dragNode(page, nodeId, deltaX, deltaY) {
  const element = await page.$(`[data-id="${nodeId}"]`);
  const box = await element.boundingBox();
  assert.ok(box, `boîte du nœud ${nodeId}`);
  const startX = box.x + box.width / 2;
  const startY = box.y + box.height / 2;
  await page.mouse.move(startX, startY);
  await page.mouse.down();
  await page.mouse.move(startX + deltaX / 3, startY + deltaY / 3, { steps: 3 });
  await page.mouse.move(startX + (2 * deltaX) / 3, startY + (2 * deltaY) / 3, { steps: 3 });
  await page.mouse.move(startX + deltaX, startY + deltaY, { steps: 3 });
  await page.mouse.up();
}

async function testGraphRendering(page) {
  await page.waitForSelector('.node[data-id="person-a"]');
  assert.equal(await page.$$eval(".node", (elements) => elements.length), 14);
  assert.equal(await page.$$eval(".edge-group", (elements) => elements.length), 12);

  const parallel = await page.evaluate(() => {
    const first = document.querySelector('[data-edge-id="e1"].edge');
    const second = document.querySelector('[data-edge-id="e11"].edge');
    return {
      first: first.getAttribute("d"),
      second: second.getAttribute("d"),
      firstMarker: first.getAttribute("marker-end"),
      labels: [...document.querySelectorAll(".edge-label")].map((element) => element.textContent),
    };
  });
  assert.notEqual(parallel.first, parallel.second);
  assert.equal(parallel.firstMarker, "url(#arrowhead)");
  assert.ok(parallel.labels.includes("business · e1"));
  assert.ok(parallel.labels.includes("support · e11"));

  await focusElement(page, '[data-edge-id="e11"].edge-group');
  await page.keyboard.press("Enter");
  assert.match(await page.$eval("#details", (element) => element.textContent), /support · e11/);
}

async function testDragFocusAndSse(page) {
  await clickNode(page, "artifact");
  await dragNode(page, "person-b", 105, 70);
  let current = await state(page);
  assert.equal(current.selectedNodeId, "artifact", "un drag ne devient pas un clic");
  assert.ok(current.pinned["person-b"]);
  const pinnedAfterDrag = current.pinned["person-b"];

  await dragNode(page, "username", 45, -35);
  const movedParallelPaths = await page.$$eval(
    '[data-edge-id="e1"].edge, [data-edge-id="e11"].edge',
    (elements) => elements.map((element) => element.getAttribute("d")),
  );
  assert.equal(new Set(movedParallelPaths).size, 2, "les liens restent séparés après drag");

  await page.evaluate(() => {
    const node = document.querySelector('[data-id="person-b"]');
    node.addEventListener(
      "pointerdown",
      (event) => {
        window.__cancelPointerId = event.pointerId;
      },
      { once: true },
    );
  });
  const node = await page.$('[data-id="person-b"]');
  const box = await node.boundingBox();
  await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
  await page.mouse.down();
  await page.mouse.move(box.x + box.width / 2 + 20, box.y + box.height / 2 + 10);
  await page.evaluate(() => {
    const target = document.querySelector('[data-id="person-b"]');
    target.dispatchEvent(
      new PointerEvent("pointercancel", {
        bubbles: true,
        pointerId: window.__cancelPointerId,
      }),
    );
  });
  await page.mouse.move(box.x + box.width / 2 + 80, box.y + box.height / 2 + 60);
  await page.mouse.up();
  const pinnedAfterCancel = (await state(page)).pinned["person-b"];
  assert.ok(pinnedAfterCancel.x - pinnedAfterDrag.x < 40, "pointercancel arrête le drag");

  const graphBox = await (await page.$("#graph")).boundingBox();
  await page.mouse.move(graphBox.x + 15, graphBox.y + 15);
  await page.mouse.down();
  await page.mouse.move(graphBox.x + 65, graphBox.y + 55, { steps: 4 });
  await page.mouse.up();
  await page.mouse.wheel({ deltaY: -180 });
  const viewportBeforeFocus = await state(page);
  assert.notDeepEqual(viewportBeforeFocus.offset, { x: 0, y: 0 });
  assert.ok(viewportBeforeFocus.scale > 1);

  await focusElement(page, '[data-id="person-a"]');
  await page.keyboard.press("Enter");
  await page.$eval('[data-capability-id="focus-neighborhood"]', (element) => element.click());
  current = await state(page);
  assert.equal(current.selectedNodeId, "person-a");
  assert.equal(current.focusNodeId, "person-a");
  assert.deepEqual(current.visibleNodeIds.sort(), ["person-a", "transaction", "username"]);

  await page.waitForFunction(
    () => document.getElementById("connection").textContent.startsWith("Scénario terminé"),
    { timeout: 8000 },
  );
  current = await state(page);
  assert.equal(current.revision, 3);
  assert.ok(current.reconnectCount >= 2, "les deux reprises EventSource ont été observées");
  assert.equal(current.selectedNodeId, "person-a");
  assert.equal(current.focusNodeId, "person-a");
  assert.deepEqual(current.pinned["person-b"], pinnedAfterCancel);
  assert.equal(
    await page.$eval('[data-id="person-a"]', (element) => element === document.activeElement),
    true,
    "le focus DOM est restauré après les événements",
  );
  assert.match(
    await page.$eval("#details h3", (element) => element.textContent),
    /revue/,
  );

  await page.screenshot({ path: WIDE_SCREENSHOT, fullPage: true });
  await page.$eval("#back", (element) => element.click());
  current = await state(page);
  assert.equal(current.focusNodeId, null);
  assert.equal(current.selectedNodeId, "person-a");
  assert.deepEqual(current.offset, viewportBeforeFocus.offset);
  assert.equal(current.scale, viewportBeforeFocus.scale);
  assert.deepEqual(current.pinned["person-b"], pinnedAfterCancel);
}

async function testCapabilitiesAndProvenance(page) {
  await clickNode(page, "person-a");
  assert.deepEqual(
    await page.$$eval("#actions [data-capability-id]", (elements) =>
      elements.map((element) => element.dataset.capabilityId),
    ),
    ["focus-neighborhood"],
  );

  await page.select("#state-filter", "under_review");
  await clickNode(page, "person-a");
  await page.$eval('[data-capability-id="focus-neighborhood"]', (element) => element.click());
  await page.select("#state-filter", "");
  await page.$eval("#back", (element) => element.click());
  assert.equal(await page.$eval("#state-filter", (element) => element.value), "under_review");
  assert.deepEqual(
    (await state(page)).visibleNodeIds.sort(),
    ["hypothesis-a", "hypothesis-b", "person-a"],
  );
  await page.$eval("#reset", (element) => element.click());

  await clickNode(page, "domain");
  assert.deepEqual(
    await page.$$eval("#actions [data-capability-id]", (elements) =>
      elements.map((element) => [element.dataset.capabilityId, element.disabled]),
    ),
    [["focus-neighborhood", false], ["rdap", true]],
  );

  await clickNode(page, "artifact");
  assert.deepEqual(
    await page.$$eval("#actions [data-capability-id]", (elements) =>
      elements.map((element) => element.dataset.capabilityId),
    ),
    ["focus-neighborhood", "show-provenance"],
  );
  assert.deepEqual(
    await page.$$eval("#provenance-details [data-origin-id]", (elements) =>
      elements.map((element) => element.dataset.originId).sort(),
    ),
    ["source", "source-b"],
  );
}

async function testProjectionKeyboardAndResets(page) {
  await page.$eval("#reset", (element) => element.click());
  assert.equal((await state(page)).visibleNodeIds.length, 14);
  await page.$eval("#collapse", (element) => element.click());
  assert.equal((await state(page)).visibleNodeIds.length, 9);
  await page.$eval("#collapse", (element) => element.click());
  assert.equal((await state(page)).visibleNodeIds.length, 14);

  await page.select("#type-filter", "OBSERVATION");
  await page.$eval("#collapse", (element) => element.click());
  assert.equal((await state(page)).visibleNodeIds.length, 0);
  await focusElement(page, "#graph");
  await page.keyboard.press("ArrowDown");
  await page.keyboard.press("Enter");
  assert.equal((await state(page)).selectedNodeId, null);
  assert.match(await page.$eval("#details", (element) => element.textContent), /Sélectionnez/);
  assert.match(await page.$eval("#actions", (element) => element.textContent), /Aucune action/);

  await page.setViewport({ width: 700, height: 900 });
  const narrowLayout = await page.evaluate(() => ({
    columns: getComputedStyle(document.querySelector("main")).gridTemplateColumns,
    asideBorderTop: getComputedStyle(document.querySelector("aside")).borderTopStyle,
  }));
  assert.equal(narrowLayout.columns.split(" ").length, 1);
  assert.equal(narrowLayout.asideBorderTop, "solid");
  await page.screenshot({ path: NARROW_SCREENSHOT, fullPage: true });

  await page.setViewport({ width: 1440, height: 900 });
  await page.$eval("#reset", (element) => element.click());
  await dragNode(page, "person-b", 60, 25);
  const pinned = (await state(page)).pinned["person-b"];
  await page.$eval("#reset", (element) => element.click());
  assert.deepEqual((await state(page)).pinned["person-b"], pinned);
  await page.$eval("#reset-layout", (element) => element.click());
  assert.equal((await state(page)).pinned["person-b"], undefined);

  await clickNode(page, "domain");
  await page.focus("#search");
  await page.keyboard.press("ArrowDown");
  assert.equal((await state(page)).selectedNodeId, "domain");
  const accepted = await page.evaluate(() => window.__LABFY_TEST__.selectNode("unknown"));
  assert.equal(accepted, false);
  assert.equal((await state(page)).selectedNodeId, "domain");
}

async function testEventFailures(page) {
  assert.equal(
    await page.evaluate(() => window.__LABFY_TEST__.processEventData("{invalid")),
    "invalid-json",
  );
  assert.match(await page.$eval("#connection", (element) => element.textContent), /JSON illisible/);
  await page.screenshot({ path: ERROR_SCREENSHOT, fullPage: true });

  const invalidStatus = await page.evaluate(() =>
    window.__LABFY_TEST__.processEventData(
      JSON.stringify({ contract: "unexpected", id: "4", revision: 4 }),
    ),
  );
  assert.equal(invalidStatus, "invalid");
  assert.match(await page.$eval("#connection", (element) => element.textContent), /contrat rejeté/);

  const duplicateStatus = await page.evaluate(() =>
    window.__LABFY_TEST__.processEventData(
      JSON.stringify({
        contract: "labfy.web_graph.event.v1",
        id: "2",
        base_revision: 2,
        revision: 3,
        kind: "node_patch",
        node_id: "person-a",
        changes: { label: "ne doit pas être appliqué" },
        message: "doublon",
      }),
    ),
  );
  assert.equal(duplicateStatus, "duplicate");

  const gapStatus = await page.evaluate(() =>
    window.__LABFY_TEST__.processEventData(
      JSON.stringify({
        contract: "labfy.web_graph.event.v1",
        id: "5",
        base_revision: 4,
        revision: 5,
        kind: "node_patch",
        node_id: "person-a",
        changes: { label: "trou" },
        message: "trou",
      }),
    ),
  );
  assert.equal(gapStatus, "gap");
  assert.equal((await state(page)).revision, 3);
  assert.match(await page.$eval("#connection", (element) => element.textContent), /Rattrapé/);
}

const server = spawn("python3", ["server.py", "--port", "0"], {
  cwd: new URL("..", import.meta.url),
  env: { ...process.env, PYTHONUNBUFFERED: "1" },
  stdio: ["ignore", "pipe", "pipe"],
});
const profile = await mkdtemp(join(tmpdir(), "labfy-firefox-profile-"));
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
  const pageErrors = [];
  page.on("pageerror", (error) => pageErrors.push(error.message));
  page.on("console", (message) => {
    if (message.type() === "error") pageErrors.push(message.text());
  });

  await page.goto(url, { waitUntil: "domcontentloaded" });
  await testGraphRendering(page);
  await testDragFocusAndSse(page);
  await testCapabilitiesAndProvenance(page);
  await testProjectionKeyboardAndResets(page);
  await testEventFailures(page);
  assert.deepEqual(pageErrors, [], `erreurs JavaScript : ${pageErrors.join(" | ")}`);
  console.log("PASS navigateur Firefox : interactions, SSE, contexte et rendus");
  console.log(`Captures : ${WIDE_SCREENSHOT}, ${NARROW_SCREENSHOT}, ${ERROR_SCREENSHOT}`);
} finally {
  if (browser) await browser.close();
  server.kill("SIGINT");
  await new Promise((resolve) => server.once("exit", resolve));
  await rm(profile, { recursive: true, force: true });
}
