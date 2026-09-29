import assert from "node:assert/strict";
import { openToolingBrowser, waitToolingState } from "./browser_tooling_harness.mjs";

const fixture = await openToolingBrowser();
try {
  const { page } = fixture;
  await page.waitForSelector(".agent-code-change-card button");
  assert.match(await page.$eval(".agent-code-change-card", (node) => node.textContent),
    /WAITING_DEV_APPROVAL.*worktree/);
  await page.click(".agent-code-change-card button");
  await waitToolingState(page, ".agent-code-change-card", "WAITING_APPLY_APPROVAL",
    "#agent-code-changes-refresh");
  await page.click(".agent-code-change-card button");
  await page.waitForSelector(".agent-code-change-card pre");
  assert.equal(await page.evaluate(() => window.__injected), undefined);
  assert.match(await page.$eval(".agent-code-change-card pre", (node) => node.textContent),
    /Explorer ce pseudo/);
  assert.equal(await page.$eval(".agent-code-change-card a", (node) =>
    node.textContent), "Prévisualisation SPECIMEN");
  const buttons = await page.$$(".agent-code-change-card button");
  await buttons[1].click();
  await waitToolingState(page, ".agent-code-change-card", "APPLIED_LOCAL",
    "#agent-code-changes-refresh");
  console.log("PASS Firefox self integration SPECIMEN : C1, diff texte, preview, C2");
} finally {
  await fixture.close();
}
