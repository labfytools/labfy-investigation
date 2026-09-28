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
  for (const args of [
    ["init-j7-specimen", "--workspace", workspace],
    ["export", "--workspace", workspace],
  ]) {
    const result = spawnSync("./tools/local-jobs", args, {
      cwd: REPOSITORY,
      encoding: "utf8",
      timeout: 30000,
    });
    assert.equal(result.status, 0, result.stderr);
  }
}

function ready(server) {
  return new Promise((resolve, reject) => {
    let output = "";
    const timer = setTimeout(() => reject(new Error("démarrage runtime expiré")), 10000);
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

function modelAction(kind, extra) {
  return JSON.stringify({
    contract: "labfy.agent_model_action.v1",
    kind,
    ...extra,
  });
}

function startFakeModel(requests) {
  return http.createServer((request, response) => {
    const chunks = [];
    request.on("data", (chunk) => chunks.push(chunk));
    request.on("end", () => {
      assert.equal(request.method, "POST");
      assert.equal(request.url, "/v1/chat/completions");
      const value = JSON.parse(Buffer.concat(chunks).toString("utf8"));
      requests.push(value);
      const transcript = JSON.stringify(value.messages);
      const searchCompleted = transcript.includes("labfy.agent_model_tool_result.v1");
      let content;
      if (transcript.includes("JSON-INVALIDE-SPECIMEN")) {
        content = "{invalide";
      } else if (searchCompleted) {
        content = modelAction("final", { text: "Bilan SPECIMEN local terminé." });
      } else {
        content = modelAction("tool_call", {
          tool_id: "investigation.search",
          arguments: { query: "SPECIMEN" },
        });
      }
      const body = Buffer.from(JSON.stringify({
        choices: [{ message: { role: "assistant", content } }],
      }));
      const send = () => {
        response.writeHead(200, {
          "Content-Type": "application/json",
          "Content-Length": String(body.length),
        });
        response.end(body);
      };
      if (transcript.includes("ANNULER-SPECIMEN")) setTimeout(send, 2500);
      else send();
    });
  });
}

const workspace = await mkdtemp(join(tmpdir(), "labfy-local-agent-SPECIMEN-"));
const profile = await mkdtemp(join(tmpdir(), "labfy-local-agent-firefox-SPECIMEN-"));
const requests = [];
const fakeModel = startFakeModel(requests);
await new Promise((resolve) => fakeModel.listen(0, "127.0.0.1", resolve));
const endpoint = `http://127.0.0.1:${fakeModel.address().port}`;
let browser;
let server;

try {
  initialize(workspace);
  server = spawn("python3", [
    "workspace_server.py",
    "--workspace", workspace,
    "--bridge", "../../tools/local-jobs",
    "--port", "0",
    "--agent-mode", "local-model",
    "--agent-endpoint", endpoint,
    "--agent-model", "qwen-SPECIMEN",
  ], {
    cwd: PROTOTYPE,
    env: { ...process.env, PYTHONUNBUFFERED: "1" },
    stdio: ["ignore", "pipe", "pipe"],
  });
  const origin = await ready(server);
  browser = await puppeteer.launch({
    browser: "firefox",
    executablePath: FIREFOX,
    headless: true,
    userDataDir: profile,
    args: ["--no-remote"],
  });
  const page = await browser.newPage();
  await page.setViewport({ width: 1280, height: 900 });
  await openAutomaticSession(page, origin);
  await page.waitForFunction(() =>
    document.querySelector("#agent-status")?.textContent === "Agent local · qwen-SPECIMEN");

  const hostile = '<img src=x onerror="window.__localAgentInjected=true"> SPECIMEN';
  await page.type("#agent-prompt", hostile);
  await page.click("#agent-send");
  await page.waitForSelector(".agent-card.result", { timeout: 15000 });
  assert.equal(await page.evaluate(() => window.__localAgentInjected), undefined);
  assert.equal(await page.$eval("#agent-prompt", (node) => node.value), hostile);
  assert.match(
    await page.$eval(".agent-conversation", (node) => node.textContent),
    /TOOL_RESULT[\s\S]*RESULT/,
  );
  const modelCards = await page.$$eval(".agent-card.model", (nodes) =>
    nodes.map((node) => node.textContent));
  assert.ok(modelCards.length > 0);
  assert.ok(modelCards.every((text) => !text.includes(hostile) && /octets/.test(text)));
  assert.ok(requests.length >= 2);

  await page.$eval("#agent-prompt", (node) => { node.value = "ANNULER-SPECIMEN"; });
  await page.click("#agent-send");
  await page.waitForFunction(() => !document.querySelector("#agent-cancel").disabled);
  await page.click("#agent-cancel");
  await page.waitForFunction(() =>
    document.querySelector(".agent-conversation")?.textContent.includes("WARNING"),
  { timeout: 10000 });

  await page.$eval("#agent-prompt", (node) => { node.value = "JSON-INVALIDE-SPECIMEN"; });
  await page.click("#agent-send");
  await page.waitForFunction(() =>
    document.querySelector(".agent-conversation")?.textContent.includes("ERROR"),
  { timeout: 10000 });
  assert.match(
    await page.$eval(".agent-conversation", (node) => node.textContent),
    /Réponse modèle non JSON/,
  );
  console.log(
    "PASS runtime agent local Firefox : loopback, outil, résultat, annulation et JSON invalide",
  );
} finally {
  if (browser) await browser.close();
  if (server?.exitCode === null) await stop(server);
  await new Promise((resolve) => fakeModel.close(resolve));
  await rm(profile, { recursive: true, force: true });
  await rm(workspace, { recursive: true, force: true });
}
