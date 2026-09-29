import assert from "node:assert/strict";
import http from "node:http";
import { mkdtemp, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { spawn, spawnSync } from "node:child_process";
import puppeteer from "puppeteer-core";
import { openAutomaticSession } from "./browser_automatic_session.mjs";

const repository = new URL("../../..", import.meta.url);
const prototype = new URL("..", import.meta.url);
const firefox = process.env.FIREFOX_PATH ?? "/usr/bin/firefox";

function initialize(workspace) {
  for (const args of [["init-j7-specimen", "--workspace", workspace], ["export", "--workspace", workspace]]) {
    const result = spawnSync("./tools/local-jobs", args, { cwd: repository, encoding: "utf8", timeout: 30000 });
    assert.equal(result.status, 0, result.stderr);
  }
}
function start(workspace, authority) {
  return spawn("python3", ["workspace_server.py", "--workspace", workspace, "--bridge", "../../tools/local-jobs", "--port", "0"], { cwd: prototype, env: { ...process.env, PYTHONUNBUFFERED: "1", LABFY_RESEARCH_FIXTURE_AUTHORITY: authority }, stdio: ["ignore", "pipe", "pipe"] });
}
function ready(server) {
  return new Promise((resolve, reject) => {
    let output=""; const timer=setTimeout(() => reject(new Error("démarrage agent expiré")), 8000);
    server.stdout.on("data", (chunk) => { output+=chunk; const match=output.match(/http:\/\/127\.0\.0\.1:\d+\//); if (match) { clearTimeout(timer); resolve(match[0]); } });
    server.once("exit", (code) => reject(new Error(`serveur arrêté (${code})`)));
  });
}
async function stop(server) { server.kill("SIGINT"); await new Promise((resolve) => server.once("exit", resolve)); }

const workspace=await mkdtemp(join(tmpdir(), "labfy-agent-protocol-SPECIMEN-"));
const profile=await mkdtemp(join(tmpdir(), "labfy-agent-firefox-SPECIMEN-"));
const contacts=[]; const provider=http.createServer((request,response) => { contacts.push(request.url); response.writeHead(500,{"Content-Length":"0"}); response.end(); });
await new Promise((resolve) => provider.listen(0,"127.0.0.1",resolve));
let browser, server;
try {
  initialize(workspace); server=start(workspace,`127.0.0.1:${provider.address().port}`);
  const origin=await ready(server); browser=await puppeteer.launch({browser:"firefox",executablePath:firefox,headless:true,userDataDir:profile,args:["--no-remote"]});
  const page=await browser.newPage(); await page.setViewport({width:1280,height:900}); await openAutomaticSession(page,origin);
  await page.waitForFunction(() => document.querySelector("#agent-status")?.textContent.includes("Agent de démonstration déterministe"));
  const hostile='<img src=x onerror="window.__agentInjected=true"> recherche SPECIMEN';
  await page.type("#agent-prompt",hostile);
  await page.click("#agent-send");
  await page.waitForSelector(".agent-card.error");
  assert.equal(await page.evaluate(() => window.__agentInjected),undefined);
  assert.equal(await page.$eval("#agent-prompt", (input) => input.value), hostile);
  assert.equal(await page.$eval(".agent-conversation", (node) => node.querySelector("img, script")), null);
  assert.match(await page.$eval(".agent-card.error", (node) => node.textContent), /Sélection de recherche invalide/);
  await page.$eval("#agent-prompt", (input) => { input.value = ""; });
  const objective = await page.evaluate(async () => {
    const response = await fetch("/api/v1/snapshot", { cache: "no-store" });
    const value = await response.json();
    return value.nodes.find((node) => typeof node.label === "string" && node.label)?.label;
  });
  assert.equal(typeof objective, "string");
  await page.type("#agent-prompt",objective); await page.click("#agent-send");
  await page.waitForFunction(() => document.querySelector(".agent-conversation")?.textContent.includes("PLAN"));
  await page.waitForFunction(() => document.querySelector(".agent-conversation")?.textContent.includes("investigation.search · COMPLETED"));
  await page.waitForFunction(() => document.querySelector(".agent-conversation")?.textContent.includes("AUTHORIZATION_REQUIRED"));
  assert.equal(await page.evaluate(() => window.__agentInjected),undefined);
  assert.match(await page.$eval(".activity-stream",(node)=>node.textContent),/agent\.tool\.completed/);
  assert.notEqual(await page.evaluate(() => window.__LABFY_TEST__.getState().selectedNodeId),null);
  assert.deepEqual(contacts,[]);
  // CONTRACT: this browser path proves the Agent Tool boundary pauses before
  // human research approval. The full grant/campaign UI is independently
  // exercised by test_research_browser.mjs with its two-wave fixture.
  assert.deepEqual(contacts,[]);
  console.log("PASS protocole agent Firefox : objectif, plan, lectures et pause d’autorisation sans contact");
} finally {
  if (browser) await browser.close(); if (server?.exitCode===null) await stop(server);
  await new Promise((resolve)=>provider.close(resolve)); await rm(workspace,{recursive:true,force:true}); await rm(profile,{recursive:true,force:true});
}
