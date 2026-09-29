import { spawn } from "node:child_process";
import { mkdtemp, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import puppeteer from "puppeteer-core";
import { openAutomaticSession } from "./browser_automatic_session.mjs";

export async function openToolingBrowser() {
  const profile = await mkdtemp(join(tmpdir(), "labfy-tooling-firefox-SPECIMEN-"));
  const server = spawn("python3", ["browser_tooling_fixture.py"], {
    cwd: new URL(".", import.meta.url),
    env: { ...process.env, PYTHONUNBUFFERED: "1" },
    stdio: ["ignore", "pipe", "pipe"],
  });
  let browser;
  try {
    const origin = await new Promise((resolve, reject) => {
      let output = "";
      const timer = setTimeout(() => reject(new Error("fixture SPECIMEN expirée")), 15000);
      server.stdout.on("data", (part) => {
        output += part;
        const match = output.match(/http:\/\/127\.0\.0\.1:\d+\//);
        if (match) { clearTimeout(timer); resolve(match[0]); }
      });
      server.once("exit", (code) => {
        clearTimeout(timer);
        reject(new Error(`fixture SPECIMEN arrêtée (${code}): ${output}`));
      });
    });
    browser = await puppeteer.launch({
      browser: "firefox", executablePath: process.env.FIREFOX_PATH ?? "/usr/bin/firefox",
      headless: true, userDataDir: profile, args: ["--no-remote"],
    });
    const page = await browser.newPage();
    await page.setViewport({ width: 1360, height: 980 });
    await openAutomaticSession(page, origin, "tooling SPECIMEN");
    return { page, origin, close: async () => {
      await browser.close();
      server.kill("SIGINT");
      await new Promise((resolve) => server.once("exit", resolve));
      await rm(profile, { recursive: true, force: true });
    } };
  } catch (error) {
    if (browser) await browser.close();
    server.kill("SIGINT");
    await new Promise((resolve) => server.once("exit", resolve));
    await rm(profile, { recursive: true, force: true });
    throw error;
  }
}

export async function waitToolingState(page, cardSelector, state, refreshSelector) {
  for (let attempt = 0; attempt < 60; attempt++) {
    const content = await page.$eval(cardSelector, (node) => node.textContent);
    if (content.includes(state)) return;
    await page.click(refreshSelector);
    await new Promise((resolve) => setTimeout(resolve, 100));
  }
  throw new Error(`état SPECIMEN ${state} absent`);
}
