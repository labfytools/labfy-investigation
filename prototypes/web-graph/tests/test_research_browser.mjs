import assert from "node:assert/strict";
import http from "node:http";
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
    let text = "";
    const timer = setTimeout(() => reject(new Error("démarrage recherche expiré")), 8000);
    server.stdout.on("data", (chunk) => {
      text += chunk;
      const url = text.match(/http:\/\/127\.0\.0\.1:(\d+)\//);
      if (url) {
        clearTimeout(timer);
        resolve({ url: url[0] });
      }
    });
    server.once("exit", (code) => reject(new Error(`serveur arrêté (${code})`)));
  });
}

function start(workspace, authority) {
  return spawn("python3", ["workspace_server.py", "--workspace", workspace,
    "--bridge", "../../tools/local-jobs", "--port", "0"], {
    cwd: PROTOTYPE,
    env: { ...process.env, PYTHONUNBUFFERED: "1",
      LABFY_RESEARCH_FIXTURE_AUTHORITY: authority },
    stdio: ["ignore", "pipe", "pipe"],
  });
}

async function stop(server) {
  server.kill("SIGINT");
  await new Promise((resolve) => server.once("exit", resolve));
}


function init(workspace) {
  const result = spawnSync("./tools/local-jobs",
    ["init-j7-specimen", "--workspace", workspace],
    { cwd: REPOSITORY, encoding: "utf8", timeout: 30000 });
  assert.equal(result.status, 0, result.stderr);
  assert.equal(spawnSync("./tools/local-jobs",
    ["export", "--workspace", workspace],
    { cwd: REPOSITORY, timeout: 15000 }).status, 0);
}

const contacts = [];
const provider = http.createServer((request, response) => {
  contacts.push(request.url);
  const body = JSON.stringify({ contract: "labfy.fixture.provider.v1",
    path: request.url });
  response.writeHead(200, { "Content-Type": "application/json",
    "Content-Length": Buffer.byteLength(body) });
  response.end(body);
});
await new Promise((resolve) => provider.listen(0, "127.0.0.1", resolve));
const authority = `127.0.0.1:${provider.address().port}`;
const workspaceA = await mkdtemp(join(tmpdir(), "labfy-research-A-"));
const workspaceB = await mkdtemp(join(tmpdir(), "labfy-research-B-"));
const profile = await mkdtemp(join(tmpdir(), "labfy-research-firefox-"));
let browser, server;
try {
  init(workspaceA); init(workspaceB);
  server = start(workspaceA, authority); const sessionA = await ready(server);
  browser = await puppeteer.launch({ browser: "firefox", executablePath: FIREFOX,
    headless: true, userDataDir: profile, args: ["--no-remote"] });
  const page = await browser.newPage();
  await page.setViewport({ width: 1280, height: 900 });
  await openAutomaticSession(page, sessionA.url);
  await page.waitForSelector("#object-list button");
  await page.click("#object-list button");
  await page.click('[data-work-panel="research-panel"]');
  await page.type("#research-question", "Que confirme cette source SPECIMEN ?");
  await page.click("#research-prepare");
  await page.waitForSelector(".research-card input[value=authorize]");
  const cards = await page.$$(".research-card");
  assert.equal(cards.length, 3);
  await (await cards[0].$("input[value=authorize]")).click();
  await (await cards[2].$("input[value=refuse]")).click();
  await page.click("#research-launch");
  await page.waitForFunction(() => document.querySelector("#research-plan")
    ?.textContent.includes("Résultat : NEW"));
  assert.deepEqual(contacts, ["/specimen/wave-1"]);
  assert.match(await page.$eval("#research-plan", (node) => node.textContent),
    /Nouvelle action non contactée/);
  const secondWave = (await page.$$(".research-card"))[1];
  await (await secondWave.$("input[value=authorize]")).click();
  await page.click("#research-launch");
  await page.waitForFunction(() => document.querySelector("#research-plan")
    ?.textContent.includes("Résultat : CONTRADICTION"));
  assert.deepEqual(contacts, ["/specimen/wave-1", "/specimen/wave-2"]);
  assert.doesNotMatch(contacts.join(" "), /refused/);

  await stop(server); server = start(workspaceB, authority);
  const sessionB = await ready(server); const pageB = await browser.newPage();
  await openAutomaticSession(pageB, sessionB.url);
  await pageB.click('[data-work-panel="research-panel"]');
  assert.equal(await pageB.$$eval(".research-card", (nodes) => nodes.length), 0);
  assert.match(await pageB.$eval("#research-note", (node) => node.textContent),
    /Sélectionnez/);
  console.log("PASS recherche Firefox : deux vagues A, refus sans contact, B isolé");
} finally {
  if (browser) await browser.close();
  if (server?.exitCode === null) await stop(server);
  await new Promise((resolve) => provider.close(resolve));
  await rm(workspaceA, { recursive: true, force: true });
  await rm(workspaceB, { recursive: true, force: true });
  await rm(profile, { recursive: true, force: true });
}
