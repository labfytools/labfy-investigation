import assert from "node:assert/strict";
import http from "node:http";
import { spawn, spawnSync } from "node:child_process";
import { mkdtemp, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import puppeteer from "puppeteer-core";
import { openAutomaticSession } from "./browser_automatic_session.mjs";

const REPOSITORY = new URL("../../..", import.meta.url);
const PROTOTYPE = new URL("..", import.meta.url);
const FIREFOX = process.env.FIREFOX_PATH ?? "/usr/bin/firefox";

function initialize(workspace) {
  for (const arguments_ of [
    ["init-j7-specimen", "--workspace", workspace],
    ["export", "--workspace", workspace],
  ]) {
    const result = spawnSync("./tools/local-jobs", arguments_, {
      cwd: REPOSITORY, encoding: "utf8", timeout: 30000,
    });
    assert.equal(result.status, 0, result.stderr);
  }
}

function ready(server) {
  return new Promise((resolve, reject) => {
    let output = "";
    const timer = setTimeout(() => reject(new Error("démarrage opérationnel expiré")), 10000);
    server.stdout.on("data", (chunk) => {
      output += chunk;
      const match = output.match(/http:\/\/127\.0\.0\.1:\d+\//);
      if (match) {
        clearTimeout(timer);
        resolve(match[0]);
      }
    });
    server.once("exit", (code) => reject(new Error(`serveur arrêté (${code})`)));
  });
}

async function stop(server) {
  server.kill("SIGINT");
  await new Promise((resolve) => server.once("exit", resolve));
}

function fakeModel() {
  return http.createServer((request, response) => {
    const chunks = [];
    request.on("data", (chunk) => chunks.push(chunk));
    request.on("end", () => {
      assert.equal(request.url, "/v1/chat/completions");
      JSON.parse(Buffer.concat(chunks).toString("utf8"));
      const content = JSON.stringify({
        contract: "labfy.agent_model_action.v1",
        kind: "final",
        text: "Bilan opérationnel SPECIMEN.",
      });
      const body = Buffer.from(JSON.stringify({
        choices: [{ message: { role: "assistant", content } }],
      }));
      response.writeHead(200, {
        "Content-Type": "application/json",
        "Content-Length": String(body.length),
      });
      response.end(body);
    });
  });
}

async function api(page, path, body) {
  return page.evaluate(async ({ path, body }) => {
    const session = await fetch("/api/v1/session", { cache: "no-store" }).then(
      (response) => response.json(),
    );
    const response = await fetch(path, {
      method: "POST",
      headers: { "Content-Type": "application/json", "X-Labfy-CSRF": session.csrf },
      body: JSON.stringify(body),
    });
    return { status: response.status, body: await response.json(), session };
  }, { path, body });
}

const workspace = await mkdtemp(join(tmpdir(), "labfy-operational-agent-SPECIMEN-"));
const profile = await mkdtemp(join(tmpdir(), "labfy-operational-firefox-SPECIMEN-"));
const model = fakeModel();
await new Promise((resolve) => model.listen(0, "127.0.0.1", resolve));
let browser;
let server;

try {
  initialize(workspace);
  const endpoint = `http://127.0.0.1:${model.address().port}`;
  server = spawn("python3", [
    "workspace_server.py", "--workspace", workspace,
    "--bridge", "../../tools/local-jobs", "--port", "0",
    "--agent-mode", "local-model", "--agent-endpoint", endpoint,
    "--agent-model", "qwen-SPECIMEN",
  ], {
    cwd: PROTOTYPE,
    env: { ...process.env, PYTHONUNBUFFERED: "1" },
    stdio: ["ignore", "pipe", "pipe"],
  });
  const origin = await ready(server);
  browser = await puppeteer.launch({
    browser: "firefox", executablePath: FIREFOX, headless: true,
    userDataDir: profile, args: ["--no-remote"],
  });
  const page = await browser.newPage();
  await page.setViewport({ width: 1360, height: 980 });
  const session = await openAutomaticSession(page, origin, "agent opérationnel");
  await page.waitForFunction(() =>
    document.querySelector("#agent-system-qwen")?.textContent === "qwen-SPECIMEN");
  assert.equal(await page.$eval("#agent-system-sandbox", (node) => node.textContent),
    "indisponible");
  assert.equal(await page.$eval("#agent-system-privacy", (node) => node.textContent),
    "indisponible");

  const objectIds = await page.$$eval("#object-list button", (nodes) =>
    nodes.slice(0, 2).map((node) => node.dataset.objectId));
  assert.equal(objectIds.length, 2);
  const base = {
    goal: "Refus SPECIMEN",
    scoped_refs: [{ object_id: objectIds[0] }],
    pivots: [{ object_id: objectIds[1] }],
    allowed_risk_classes: ["LOCAL_READ_ONLY"],
    network_profile: "OFFLINE",
    max_contacts: 0,
    max_duration_seconds: 60,
    max_tool_calls: 2,
  };
  const envelope = (specification, workspaceId = session.investigation_id) => ({
    workspace_id: workspaceId, specification, human_confirmed: true,
    idempotency_key: crypto.randomUUID(),
  });
  const networkDenied = await api(page, "/api/v1/agent-mission/start", envelope({
    ...base, allowed_risk_classes: ["PASSIVE_PUBLIC"],
  }));
  assert.equal(networkDenied.status, 409);
  const activeDenied = await api(page, "/api/v1/agent-mission/start", envelope({
    ...base, allowed_risk_classes: ["PUBLIC_ACTIVE"],
  }));
  assert.equal(activeDenied.status, 409);
  const foreignPivot = await api(page, "/api/v1/agent-mission/start", envelope({
    ...base,
    pivots: [{ object_id: "entity:00000000-0000-4000-8000-000000000000" }],
  }));
  assert.equal(foreignPivot.status, 409);
  const foreignWorkspace = await api(
    page, "/api/v1/agent-mission/start", envelope(base, "workspace-SPECIMEN-foreign"),
  );
  assert.equal(foreignWorkspace.status, 403);

  await page.click("#object-list button");
  await page.type("#agent-mission-goal-input", "Mission opérationnelle SPECIMEN");
  await page.click("#agent-mission-start");
  await page.waitForFunction(() =>
    document.querySelector("#agent-mission-state")?.textContent.includes("Mission active"));
  assert.match(await page.$eval("#agent-mission-profile", (node) => node.textContent),
    /OFFLINE/);
  assert.match(await page.$eval("#agent-system-privacy", (node) => node.textContent),
    /non requis/);

  const mission = await page.evaluate(() => fetch("/api/v1/agent-mission/current")
    .then((response) => response.json()));
  const hostile = '<img src=x onerror="window.__proposalInjected=true"> proposition SPECIMEN';
  const created = await api(page, "/api/v1/agent-proposals", {
    workspace_id: session.investigation_id,
    mission_id: mission.mission.mission_id,
    proposal: {
      title: "Proposition SPECIMEN", reason: hostile,
      object_refs: [{ object_id: objectIds[0] }],
      suggested_capability: "investigation.search",
      risk_class: "LOCAL_READ_ONLY", expected_value: "Vérification humaine",
    },
    idempotency_key: crypto.randomUUID(),
  });
  assert.equal(created.status, 201);
  await page.click("#agent-proposals-refresh");
  await page.waitForFunction((text) =>
    document.querySelector("#agent-proposal-list")?.textContent.includes(text), {}, hostile);
  assert.equal(await page.evaluate(() => window.__proposalInjected), undefined);
  await page.click(".agent-proposal-card button:nth-of-type(2)");
  await page.waitForFunction(() =>
    document.querySelector(".agent-proposal-card")?.textContent.includes("aucun grant"));
  const proposals = await page.evaluate(() => fetch("/api/v1/agent-proposals")
    .then((response) => response.json()));
  assert.equal(proposals.proposals[0].decision.policy_grant_created, false);
  assert.match(await page.$eval(".activity-stream", (node) => node.textContent),
    /agent\.proposal/);

  await page.type("#agent-prompt", "Bilan Qwen SPECIMEN");
  await page.click("#agent-send");
  await page.waitForFunction(() =>
    document.querySelector(".agent-conversation")?.textContent.includes(
      "Bilan opérationnel SPECIMEN"), { timeout: 15000 });
  console.log("PASS agent opérationnel Firefox : mission, propositions, Qwen et refus bornés");
} finally {
  if (browser) await browser.close();
  if (server?.exitCode === null) await stop(server);
  await new Promise((resolve) => model.close(resolve));
  await rm(profile, { recursive: true, force: true });
  await rm(workspace, { recursive: true, force: true });
}
