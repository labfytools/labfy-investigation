import assert from "node:assert/strict";
import { mkdtemp, readFile, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { spawn, spawnSync } from "node:child_process";
import puppeteer from "puppeteer-core";

const REPOSITORY = new URL("../../..", import.meta.url);
const PROTOTYPE = new URL("..", import.meta.url);
const FIREFOX_PATH = process.env.FIREFOX_PATH ?? "/usr/bin/firefox";
const EXTRACTION = "extraction:71000000-0000-4000-8000-000000000042";

function waitForServer(server) {
  return new Promise((resolve, reject) => {
    let output = "";
    const timeout = setTimeout(() => reject(new Error("démarrage serveur expiré")), 5000);
    server.stdout.on("data", (chunk) => {
      output += chunk.toString();
      const match = output.match(/http:\/\/127\.0\.0\.1:(\d+)\//);
      if (match) { clearTimeout(timeout); resolve(match[0]); }
    });
    server.once("exit", (code) => reject(new Error(`serveur J4 arrêté (${code})`)));
  });
}

const directory = await mkdtemp(join(tmpdir(), "labfy-j4-browser-"));
const profile = await mkdtemp(join(tmpdir(), "labfy-j4-firefox-"));
let browser;
let server;
try {
  const generated = spawnSync("./tools/local_toolkit_demo", ["--output-dir", directory], {
    cwd: REPOSITORY, encoding: "utf8", timeout: 60000,
  });
  assert.equal(generated.status, 0, generated.stderr || generated.error?.message);
  const snapshot = JSON.parse(await readFile(join(directory, "core-snapshot.json"), "utf8"));
  const manifest = JSON.parse(await readFile(join(directory, "generation-manifest.json"), "utf8"));
  assert.equal(manifest.exiftool_version, "13.55");
  assert.ok(snapshot.nodes.some((node) => node.id === EXTRACTION &&
    node.details.tool_id === "exiftool" && node.details.tool_version === manifest.exiftool_version));
  const image = snapshot.nodes.find((node) => node.type === "photo" && node.state !== "derived");
  assert.ok(image);
  const application = snapshot.capabilities.find((item) =>
    item.node_id === image.id && item.capability_id === "labfy.capability.exif_metadata.v1");
  assert.ok(application);
  assert.equal(application.available, false);
  assert.match(application.reason, /Exécution par le lanceur local/);
  const derivative = snapshot.nodes.find((node) => node.id ===
    "evidence:71000000-0000-4000-8000-000000000043");
  assert.match(derivative.raw, /Alice SPECIMEN/);
  assert.ok(snapshot.edges.some((edge) => edge.source === image.id &&
    edge.target === EXTRACTION && edge.semantic === "analysis_input"));
  assert.ok(snapshot.edges.some((edge) => edge.source === EXTRACTION &&
    edge.target === derivative.id && edge.semantic === "analysis_derivative"));
  server = spawn("python3", ["server.py", "--port", "0", "--core-snapshot",
    join(directory, "core-snapshot.json")], {
    cwd: PROTOTYPE, env: {...process.env, PYTHONUNBUFFERED: "1"},
    stdio: ["ignore", "pipe", "pipe"],
  });
  browser = await puppeteer.launch({browser: "firefox", executablePath: FIREFOX_PATH,
    headless: true, userDataDir: profile, args: ["--no-remote"]});
  const page = await browser.newPage();
  await page.setViewport({width: 1440, height: 900});
  await page.goto(await waitForServer(server), {waitUntil: "domcontentloaded"});
  await page.$eval(`[data-id="${image.id}"]`, (item) =>
    item.dispatchEvent(new MouseEvent("click", {bubbles: true})));
  const actions = await page.$eval("#actions", (item) => item.textContent);
  assert.match(actions, /Examiner les métadonnées/);
  assert.match(actions, /Exécution par le lanceur local/);
  assert.equal(await page.$eval('[data-capability-id="labfy.capability.exif_metadata.v1"]',
    (item) => item.disabled), true);
  await page.$eval(`[data-id="${EXTRACTION}"]`, (item) =>
    item.dispatchEvent(new MouseEvent("click", {bubbles: true})));
  assert.match(await page.$eval("#details", (item) => item.textContent), /ExifTool/);
  await page.$eval(`[data-id="${derivative.id}"]`, (item) =>
    item.dispatchEvent(new MouseEvent("click", {bubbles: true})));
  assert.match(await page.$eval("#details", (item) => item.textContent), /Alice SPECIMEN/);
  assert.equal(await page.evaluate(() => window.__LABFY_J4_XSS__), undefined);
  await page.screenshot({path: "/tmp/labfy-j4-toolkit-wide.png", fullPage: true});
  await page.setViewport({width: 700, height: 900});
  await page.screenshot({path: "/tmp/labfy-j4-toolkit-narrow.png", fullPage: true});
  console.log("PASS navigateur Firefox : Toolkit J4 EML + ExifTool réel");
} finally {
  if (browser) await browser.close();
  if (server) { server.kill("SIGINT"); await new Promise((resolve) => server.once("exit", resolve)); }
  await rm(profile, {recursive: true, force: true});
  await rm(directory, {recursive: true, force: true});
}
