import assert from "node:assert/strict";
import { mkdtemp, mkdir, readFile, rename, rm, stat } from "node:fs/promises";
import { createHash } from "node:crypto";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { spawn, spawnSync } from "node:child_process";
import puppeteer from "puppeteer-core";
const REPOSITORY = new URL("../../..", import.meta.url),
  PROTOTYPE = new URL("..", import.meta.url),
  FIREFOX = process.env.FIREFOX_PATH ?? "/usr/bin/firefox";
function ready(server) {
  return new Promise((resolve, reject) => {
    let text = "";
    const timer = setTimeout(
      () => reject(new Error("démarrage J9 expiré")),
      8000,
    );
    server.stdout.on("data", (chunk) => {
      text += chunk;
      const u = text.match(/http:\/\/127\.0\.0\.1:(\d+)\//),
        c = text.match(/Code de session éphémère : ([^\s]+)/);
      if (u && c) {
        clearTimeout(timer);
        resolve({ url: u[0], code: c[1] });
      }
    });
    server.once("exit", (code) =>
      reject(new Error(`serveur J9 arrêté (${code})`)),
    );
  });
}
function start(workspace) {
  return spawn(
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
}
async function stop(server) {
  server.kill("SIGINT");
  await new Promise((resolve) => server.once("exit", resolve));
}
async function login(page, session) {
  await page.goto(session.url, { waitUntil: "domcontentloaded" });
  await page.type("#bootstrap-code", session.code);
  await Promise.all([
    page.waitForNavigation({ waitUntil: "domcontentloaded" }),
    page.click("#login-form button"),
  ]);
}
const workspace = await mkdtemp(join(tmpdir(), "labfy-j9-browser-")),
  profile = await mkdtemp(join(tmpdir(), "labfy-j9-firefox-"));
let browser, server;
try {
  const received = join(workspace, "received-report");
  await mkdir(received);
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
  server = start(workspace);
  const session = await ready(server);
  browser = await puppeteer.launch({
    browser: "firefox",
    executablePath: FIREFOX,
    headless: true,
    userDataDir: profile,
    args: ["--no-remote"],
    extraPrefsFirefox: {
      "browser.download.folderList": 2,
      "browser.download.dir": received,
      "browser.download.useDownloadDir": true,
      "browser.helperApps.neverAsk.saveToDisk":
        "text/html,application/json,application/pdf,text/plain",
    },
  });
  const page = await browser.newPage();
  const errors = [];
  page.on("pageerror", (e) => errors.push(String(e)));
  await page.setViewport({ width: 1440, height: 900 });
  await login(page, session);
  await page.waitForSelector("#view-evidence");
  await page.click("#view-evidence");
  await page.click("#view-timeline");
  assert.equal(await page.$eval("#timeline-panel", (x) => x.hidden), false);
  await page.click("#view-infrastructure");
  await page.screenshot({ path: "/tmp/labfy-j9-wide.png", fullPage: true });
  await page.setViewport({ width: 700, height: 900 });
  await page.screenshot({ path: "/tmp/labfy-j9-narrow.png", fullPage: true });
  await page.click("#view-evidence");
  await page.waitForSelector("#object-list button");
  await page.click("#object-list button");
  await page.click('[data-panel="actions-panel"]');
  await page.waitForSelector('[data-report-action="toggle"]');
  await page.click('[data-report-action="toggle"]');
  assert.match(
    await page.$eval("#report-count", (x) => x.textContent),
    /1 objet/,
  );
  await page.click('[data-work-panel="reports-panel"]');
  await page.click("#report-selection button");
  assert.match(
    await page.$eval("#report-count", (x) => x.textContent),
    /0 objet/,
  );
  await page.click('[data-panel="details-panel"]');
  await page.click("#object-list button");
  await page.click('[data-panel="actions-panel"]');
  await page.click('[data-report-action="toggle"]');
  await page.click('[data-work-panel="reports-panel"]');
  await page.$eval(
    "#report-name",
    (x) => (x.value = "Rapport Firefox Unicode œ € → Łukasz"),
  );
  await page.$eval(
    "#report-comment",
    (x) => (x.value = "Commentaire humain éèàçù ’ — ()\\<>"),
  );
  await page.click("#report-preview");
  await page.waitForFunction(() =>
    document
      .querySelector("#report-preview-content")
      ?.textContent.includes("Coupe"),
  );
  await page.click("#report-generate");
  await page.waitForFunction(
    () => document.querySelectorAll("#report-status a").length === 5,
    { timeout: 15000 },
  );
  const href = await page.$eval('#report-status a[href$="report.html"]', (x) =>
    x.getAttribute("href"),
  );
  const reportId = href.split("/")[4];
  const names = [
    "report.html",
    "report.json",
    "report.pdf",
    "manifest.json",
    "NOTICE.txt",
  ];
  const receivedSizes = new Map();
  for (const name of names) {
    await page.$eval(`#report-status a[href$="${name}"]`, (link) =>
      link.click(),
    );
    let size = 0,
      stable = 0;
    for (let attempt = 0; attempt < 100 && stable < 2; attempt++) {
      await new Promise((resolve) => setTimeout(resolve, 50));
      try {
        const current = (await stat(join(received, name))).size;
        stable = current > 0 && current === size ? stable + 1 : 0;
        size = current;
      } catch {
        stable = 0;
      }
    }
    assert.ok(stable >= 2, `téléchargement Firefox terminé ${name}`);
    receivedSizes.set(name, size);
  }
  const bundle = join(received, reportId);
  await mkdir(bundle);
  for (const name of names)
    await rename(join(received, name), join(bundle, name));
  const manifest = JSON.parse(
    await readFile(join(bundle, "manifest.json"), "utf8"),
  );
  for (const entry of manifest.files) {
    const bytes = await readFile(join(bundle, entry.path));
    assert.equal(bytes.length, entry.size);
    assert.equal(receivedSizes.get(entry.path), entry.size);
    assert.equal(
      createHash("sha256").update(bytes).digest("hex"),
      entry.sha256,
    );
  }
  const offline = join(bundle, "report.html"),
    offlinePage = await browser.newPage();
  await offlinePage.goto(`file://${offline}`, {
    waitUntil: "domcontentloaded",
  });
  assert.match(
    await offlinePage.$eval("body", (x) => x.textContent),
    /Rapport Firefox Unicode/,
  );
  assert.equal(await offlinePage.$$("script").then((x) => x.length), 0);
  await offlinePage.close();
  assert.equal(
    spawnSync("python3", ["report_bundle.py", "verify", bundle], {
      cwd: PROTOTYPE,
    }).status,
    0,
  );
  const pdfText = spawnSync("pdftotext", [join(bundle, "report.pdf"), "-"], {
    encoding: "utf8",
  }).stdout;
  assert.match(pdfText.split(/\s+/).join(" "), /Unicode œ € → Łukasz/);
  assert.equal(
    spawnSync("pdftoppm", [
      "-png",
      join(bundle, "report.pdf"),
      join(workspace, "pdf-page"),
    ]).status,
    0,
  );
  assert.deepEqual(errors, []);
  console.log(
    "PASS navigateur Firefox : J9, cinq téléchargements HTTP vérifiés et dossier reçu hors ligne",
  );
} finally {
  if (browser) await browser.close();
  if (server) await stop(server);
  await rm(profile, { recursive: true, force: true });
  await rm(workspace, { recursive: true, force: true });
}
