#!/usr/bin/env node
import assert from "node:assert/strict";
import puppeteer from "puppeteer-core";
import { openAutomaticSession } from "./tests/browser_automatic_session.mjs";

const origin = process.argv[2];
if (!/^http:\/\/127\.0\.0\.1:\d+$/.test(origin ?? ""))
  throw new Error("Preview SPECIMEN loopback requis");

const browser = await puppeteer.launch({
  browser: "firefox", executablePath: "/usr/bin/firefox", headless: true,
  protocol: "webDriverBiDi",
});
try {
  const page = await browser.newPage();
  await openAutomaticSession(page, origin, "preview SPECIMEN");
  await page.waitForSelector("#search-button");
  await page.type("#search", "SPECIMEN_user");
  await page.click("#search-button");
  await page.waitForFunction(() => [...document.querySelectorAll("#actions button")]
    .some((button) => button.textContent.includes("Explorer ce pseudo")));
  const labels = await page.$$eval("#actions button", (buttons) =>
    buttons.map((button) => button.textContent));
  assert(labels.some((label) => label.includes("Explorer ce pseudo")));
  console.log("FIREFOX_SPECIMEN_PREVIEW=PASS");
} finally {
  await browser.close();
}
