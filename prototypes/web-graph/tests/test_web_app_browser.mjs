import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import { mkdtemp, mkdir, readFile, rm, stat, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { spawn, spawnSync } from "node:child_process";
import puppeteer from "puppeteer-core";

const PROTOTYPE = new URL("..", import.meta.url);
const FIREFOX = process.env.FIREFOX_PATH ?? "/usr/bin/firefox";

function startServer(library, runtime, state, port = 0) {
  return spawn("python3", ["web_app.py", "serve", "--library", library,
    "--bridge", "../../tools/local-jobs", "--port", String(port)], {
    cwd: PROTOTYPE,
    env: { ...process.env, PYTHONUNBUFFERED: "1", XDG_RUNTIME_DIR: runtime,
      XDG_STATE_HOME: state },
    stdio: ["ignore", "pipe", "pipe"],
  });
}

function ready(server) {
  return new Promise((resolve, reject) => {
    let output = "";
    const timer = setTimeout(() => reject(new Error("démarrage Web expiré")), 10000);
    server.stdout.on("data", (chunk) => {
      output += chunk;
      const origin = output.match(/Labfy Web : (http:\/\/127\.0\.0\.1:\d+)\//);
      const code = output.match(/Code de session éphémère : ([^\s]+)/);
      if (origin && code) {
        clearTimeout(timer);
        resolve({ origin: origin[1], code: code[1] });
      }
    });
    server.once("exit", (code) => reject(new Error(`serveur arrêté (${code})`)));
  });
}

async function stopServer(server) {
  if (!server || server.exitCode !== null) return;
  const exited = new Promise((resolve) => server.once("exit", resolve));
  server.kill("SIGTERM");
  await exited;
}

async function login(page, session) {
  try {
    await page.evaluate((origin) => window.location.assign(origin), session.origin);
  } catch (error) {
    if (!String(error).includes("Execution context was destroyed")) throw error;
  }
  await page.waitForSelector("#login-form");
  await page.type("#bootstrap-code", session.code);
  await page.click("#login-form button");
  await page.waitForSelector("#library-home:not([hidden])");
  assert.equal(page.url().includes(session.code), false);
  assert.equal((await page.content()).includes(session.code), false);
}

async function createWorkspace(page, title) {
  await page.type("#workspace-title", title);
  await page.click("#workspace-create-form button");
  await page.waitForFunction((expectedTitle) => [...document.querySelectorAll(".workspace-card")]
    .some((card) => card.querySelector("h3")?.textContent === expectedTitle), {}, title);
  return page.$$eval(".workspace-card", (cards, expectedTitle) => {
    const card = cards.find((item) => item.querySelector("h3")?.textContent === expectedTitle);
    return card?.querySelector("[data-workspace-id]")?.dataset.workspaceId ?? null;
  }, title);
}

async function openWorkspace(page, workspaceId) {
  await page.click(`[data-workspace-id="${workspaceId}"]`);
  await page.waitForSelector("#workbench-shell:not([hidden])");
  try {
    await page.waitForFunction(() => {
      const connection = document.querySelector("#connection")?.textContent ?? "";
      return connection.includes("Ouverte") || connection.includes("Poste local synchronisé");
    }, { timeout: 15000 });
  } catch (error) {
    const connection = await page.$eval("#connection", (item) => item.textContent);
    throw new Error(`ouverture ${workspaceId} incomplète : ${connection}`, { cause: error });
  }
}

async function waitForDownload(path) {
  let size = -1;
  let stable = 0;
  for (let attempt = 0; attempt < 120 && stable < 3; attempt += 1) {
    await new Promise((resolve) => setTimeout(resolve, 50));
    try {
      const current = (await stat(path)).size;
      stable = current > 0 && current === size ? stable + 1 : 0;
      size = current;
    } catch {
      stable = 0;
    }
  }
  assert.ok(stable >= 3, `téléchargement Firefox terminé ${path}`);
  return size;
}

const library = await mkdtemp(join(tmpdir(), "labfy-web-library-"));
const profile = await mkdtemp(join(tmpdir(), "labfy-web-firefox-"));
const runtime = await mkdtemp(join(tmpdir(), "labfy-web-runtime-"));
const state = await mkdtemp(join(tmpdir(), "labfy-web-state-"));
const fixtures = await mkdtemp(join(tmpdir(), "labfy-web-SPECIMEN-"));
const downloads = join(fixtures, "downloads");
await mkdir(downloads, { mode: 0o700 });
const specimenEml = join(fixtures, "courrier rapport SPECIMEN.eml");
await writeFile(specimenEml,
  "From: Source SPECIMEN <source@example.test>\r\n" +
  "To: Pivot SPECIMEN <pivot@example.test>\r\n" +
  "Message-ID: <web-app-report@example.test>\r\n" +
  "Subject: Rapport navigateur A\r\n\r\n" +
  "Contenu synthétique pour le parcours A.\r\n");
let browser;
let server;
try {
  server = startServer(library, runtime, state);
  const session = await ready(server);
  const assignedPort = Number.parseInt(new URL(session.origin).port, 10);
  browser = await puppeteer.launch({ browser: "firefox", executablePath: FIREFOX,
    headless: true, userDataDir: profile, args: ["--no-remote"],
    extraPrefsFirefox: { "browser.download.folderList": 2,
      "browser.download.dir": downloads, "browser.download.useDownloadDir": true,
      "browser.helperApps.neverAsk.saveToDisk":
        "text/html,application/json,application/pdf,text/plain" } });
  const page = await browser.newPage();
  const errors = [];
  page.on("pageerror", (error) => errors.push(String(error)));
  await page.setViewport({ width: 700, height: 900 });
  await login(page, session);

  assert.equal(await page.$eval("#workspace-list", (item) => item.children.length), 0);
  assert.match(await page.$eval("#library-empty", (item) => item.textContent), /Aucune enquête/);

  const workspaceATitle = "Enquête A navigateur SPECIMEN";
  const workspaceAId = await createWorkspace(page, workspaceATitle);
  assert.ok(workspaceAId);
  assert.equal(await page.$eval("#workbench-shell", (item) => item.hidden), true);
  await new Promise((resolve) => setTimeout(resolve, 60));
  await openWorkspace(page, workspaceAId);
  assert.equal(await page.$eval("#investigation-title", (item) => item.textContent),
    workspaceATitle);
  assert.equal(await page.$eval("#mode-badge", (item) => item.textContent.includes("SPECIMEN")), false);

  await page.click("#open-import");
  const importInput = await page.$("#import-files");
  await importInput.uploadFile(specimenEml);
  await page.waitForFunction(() => {
    const items = [...document.querySelectorAll("#import-list li")];
    return items.length === 1 && items.every((item) => item.textContent.includes("préparé"));
  }, { timeout: 20000 });
  await page.click("#import-confirm");
  await page.waitForFunction(() => document.querySelectorAll(".node").length >= 1,
    { timeout: 20000 });

  await page.click('[data-work-panel="next-panel"]');
  await page.waitForFunction(() =>
    document.querySelectorAll("#planner-list input:not(:disabled)").length >= 1,
  { timeout: 15000 });
  await page.$$eval("#planner-list input:not(:disabled)", (inputs) => {
    inputs.forEach((input) => input.click());
  });
  await page.select("#planner-profile", "LOCAL_PRUDENT");
  await page.click("#planner-launch");
  await page.waitForFunction(() => {
    const jobs = [...document.querySelectorAll("#jobs-list li")];
    return jobs.length >= 1 && jobs.every((job) => job.dataset.jobState === "COMPLETED");
  }, { timeout: 45000 });

  await page.evaluate(() => {
    const candidate = [...document.querySelectorAll("#object-list button")]
      .find((button) => button.textContent.startsWith("email —"));
    if (!candidate) throw new Error("preuve e-mail SPECIMEN absente");
    candidate.click();
  });
  await page.click('[data-panel="actions-panel"]');
  await page.evaluate(() => {
    const button = [...document.querySelectorAll("#actions button")]
      .find((item) => item.textContent === "Ouvrir la preuve");
    if (!button) throw new Error("action Ouvrir la preuve absente");
    button.click();
  });
  await page.waitForSelector("#evidence-dialog[open] #observation-list li", { timeout: 15000 });
  await page.type("#observation-list li input[placeholder='Motif obligatoire']",
    "Revue rapport A SPECIMEN");
  await page.evaluate(() => {
    const button = [...document.querySelectorAll("#observation-list li:first-child button")]
      .find((item) => item.textContent === "Confirmer");
    if (!button) throw new Error("action Confirmer absente");
    button.click();
  });
  await page.waitForFunction(() =>
    document.querySelector("#evidence-status")?.textContent.includes("journalisée"),
  { timeout: 15000 });
  await page.click("#evidence-dialog .dialog-close button");

  await page.click('[data-report-action="toggle"]');
  await page.click('[data-work-panel="reports-panel"]');
  await page.click("#report-name", { clickCount: 3 });
  await page.type("#report-name", "Rapport enquête A SPECIMEN");
  await page.click("#report-preview");
  await page.waitForFunction(() =>
    document.querySelector("#report-preview-content")?.textContent.includes("Coupe"),
  { timeout: 15000 });
  await page.click("#report-generate");
  await page.waitForFunction(() => document.querySelectorAll("#report-status a").length === 5,
    { timeout: 30000 });

  const reportHref = await page.$eval('#report-status a[href$="report.json"]',
    (link) => link.getAttribute("href"));
  const reportId = reportHref.split("/")[4];
  const artifactNames = ["report.json", "report.html", "report.pdf", "manifest.json",
    "NOTICE.txt"];
  const receivedSizes = new Map();
  for (const name of artifactNames) {
    await page.$eval(`#report-status a[href$="${name}"]`, (link) => link.click());
    receivedSizes.set(name, await waitForDownload(join(downloads, name)));
  }
  const manifestBytes = await readFile(join(downloads, "manifest.json"));
  const manifest = JSON.parse(manifestBytes);
  assert.equal(manifest.contract, "labfy.report_manifest.v1");
  assert.equal(manifest.report_id, reportId);
  assert.deepEqual(new Set(manifest.files.map((entry) => entry.path)),
    new Set(artifactNames.filter((name) => name !== "manifest.json")));
  for (const entry of manifest.files) {
    const bytes = await readFile(join(downloads, entry.path));
    assert.equal(bytes.length, entry.size, `taille manifestée ${entry.path}`);
    assert.equal(receivedSizes.get(entry.path), entry.size, `taille reçue ${entry.path}`);
    assert.equal(createHash("sha256").update(bytes).digest("hex"), entry.sha256,
      `SHA-256 ${entry.path}`);
  }
  assert.equal(receivedSizes.get("manifest.json"), manifestBytes.length,
    "taille reçue manifest.json");
  const reportHtml = (await readFile(join(downloads, "report.html"))).toString("utf8");
  assert.doesNotMatch(reportHtml, /<(?:script|iframe|frame|object|embed|link)\b/i);
  assert.doesNotMatch(reportHtml, /\b(?:src|href|action)\s*=\s*["'](?:https?:|\/\/)/i);
  assert.doesNotMatch(reportHtml, /url\s*\(/i);
  const offlinePage = await browser.newPage();
  await offlinePage.goto(`file://${join(downloads, "report.html")}`,
    { waitUntil: "domcontentloaded" });
  assert.match(await offlinePage.$eval("body", (body) => body.textContent),
    /Rapport enquête A SPECIMEN/);
  assert.equal(await offlinePage.$$eval(
    "script,iframe,frame,object,embed,link,img,audio,video,source",
    (nodes) => nodes.length), 0);
  await offlinePage.close();
  const pdfText = spawnSync("pdftotext", [join(downloads, "report.pdf"), "-"],
    { encoding: "utf8" });
  assert.equal(pdfText.status, 0, pdfText.stderr);
  assert.match(pdfText.stdout.split(/\s+/).join(" "), /Rapport enquête A SPECIMEN/);

  await page.$eval("#report-name", (input) => {
    input.value = "Brouillon rapport A SPECIMEN";
    input.dispatchEvent(new Event("input", { bubbles: true }));
  });
  let draftKeys = await page.evaluate(() => Object.keys(sessionStorage)
    .filter((key) => key.startsWith("labfy-report-draft:")));
  assert.equal(draftKeys.length, 1);
  assert.ok(draftKeys[0].includes(workspaceAId));

  // INVARIANT: une actualisation devenue tardive après « Accueil » ne doit pas
  // restaurer le workbench ni altérer la bibliothèque rendue ensuite.
  await page.setRequestInterception(true);
  let releaseSnapshot;
  const delayedSnapshot = new Promise((resolve) => { releaseSnapshot = resolve; });
  let markSnapshotContinued;
  const snapshotContinued = new Promise((resolve) => { markSnapshotContinued = resolve; });
  let held = false;
  const holdSnapshotRequest = async (request) => {
    if (!held && new URL(request.url()).pathname === "/api/v1/snapshot") {
      held = true;
      await delayedSnapshot;
      await request.continue().catch(() => {});
      markSnapshotContinued();
      return;
    }
    await request.continue().catch(() => {});
  };
  page.on("request", holdSnapshotRequest);
  await page.waitForFunction(() => window.__LABFY_TEST__.getState().workspaceId !== null);
  for (let attempt = 0; attempt < 100 && !held; attempt += 1) {
    await new Promise((resolve) => setTimeout(resolve, 50));
  }
  assert.equal(held, true);
  await page.click("#home-button");
  await page.waitForSelector("#library-home:not([hidden])");
  releaseSnapshot();
  await snapshotContinued;
  await new Promise((resolve) => setTimeout(resolve, 150));
  assert.equal(await page.$eval("#workbench-shell", (item) => item.hidden), true);
  assert.match(await page.$eval(".workspace-card p", (item) => item.textContent), /Ouverte/);
  await page.setRequestInterception(false);
  page.off("request", holdSnapshotRequest);

  // Le port initial est attribué par le système. Sa réutilisation conserve le
  // même origin et permet de vérifier les brouillons session après redémarrage.
  await stopServer(server);
  server = startServer(library, runtime, state, assignedPort);
  const restartedForB = await ready(server);
  assert.equal(restartedForB.origin, session.origin);
  await login(page, restartedForB);
  assert.equal(await page.$eval("#workspace-list", (item) => item.children.length), 1);
  assert.equal(await page.$eval(".workspace-card h3", (item) => item.textContent), workspaceATitle);
  assert.match(await page.$eval(".workspace-card p", (item) => item.textContent), /Prête à ouvrir/);

  const workspaceBTitle = "Enquête B navigateur SPECIMEN";
  const workspaceBId = await createWorkspace(page, workspaceBTitle);
  assert.ok(workspaceBId);
  assert.notEqual(workspaceBId, workspaceAId);
  assert.equal(await page.$eval("#workspace-list", (item) => item.children.length), 2);
  await openWorkspace(page, workspaceBId);
  assert.equal(await page.$eval("#investigation-title", (item) => item.textContent),
    workspaceBTitle);
  assert.equal(await page.$eval("#report-name", (item) => item.value), "Rapport d’enquête");
  draftKeys = await page.evaluate(() => Object.keys(sessionStorage)
    .filter((key) => key.startsWith("labfy-report-draft:")));
  assert.equal(draftKeys.length, 1);
  assert.ok(draftKeys[0].includes(workspaceAId));
  assert.equal(draftKeys[0].includes(workspaceBId), false);

  await page.$eval("#report-name", (input) => {
    input.value = "Brouillon rapport B SPECIMEN";
    input.dispatchEvent(new Event("input", { bubbles: true }));
  });
  draftKeys = await page.evaluate(() => Object.keys(sessionStorage)
    .filter((key) => key.startsWith("labfy-report-draft:")));
  assert.equal(draftKeys.length, 2);
  assert.equal(draftKeys.some((key) => key.includes(workspaceAId)), true);
  assert.equal(draftKeys.some((key) => key.includes(workspaceBId)), true);

  await page.click("#home-button");
  await page.waitForSelector("#library-home:not([hidden])");
  await stopServer(server);
  server = startServer(library, runtime, state, assignedPort);
  const restartedForA = await ready(server);
  assert.equal(restartedForA.origin, session.origin);
  await login(page, restartedForA);
  assert.equal(await page.$eval("#workspace-list", (item) => item.children.length), 2);
  await openWorkspace(page, workspaceAId);
  assert.equal(await page.$eval("#investigation-title", (item) => item.textContent),
    workspaceATitle);
  assert.equal(await page.evaluate(() => window.__LABFY_TEST__.getState().workspaceId),
    workspaceAId);
  // La création de B incrémente la génération de bibliothèque : le brouillon
  // A de la génération précédente reste stocké mais ne peut contaminer A ou B.
  assert.equal(await page.$eval("#report-name", (item) => item.value),
    "Rapport d’enquête");
  assert.notEqual(await page.$eval("#report-name", (item) => item.value),
    "Brouillon rapport B SPECIMEN");
  draftKeys = await page.evaluate(() => Object.keys(sessionStorage)
    .filter((key) => key.startsWith("labfy-report-draft:")));
  assert.equal(draftKeys.length, 2);
  assert.equal(draftKeys.some((key) => key.includes(workspaceAId)), true);
  assert.equal(draftKeys.some((key) => key.includes(workspaceBId)), true);

  await page.evaluate(() => { document.documentElement.style.zoom = "200%"; });
  assert.equal(await page.evaluate(() =>
    document.documentElement.scrollWidth > document.documentElement.clientWidth), false);
  assert.deepEqual(errors, []);
  console.log("PASS navigateur Web : A importée/analysée/revue avec rapport vérifié, " +
    "bibliothèque A/B, brouillons isolés et deux redémarrages");
} finally {
  if (browser) await browser.close();
  await stopServer(server);
  await Promise.all([library, profile, runtime, state, fixtures].map((path) =>
    rm(path, { recursive: true, force: true })));
}
