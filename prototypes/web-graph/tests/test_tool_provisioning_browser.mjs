import assert from "node:assert/strict";
import { openToolingBrowser, waitToolingState } from "./browser_tooling_harness.mjs";

const fixture = await openToolingBrowser();
try {
  const { page } = fixture;
  await page.waitForSelector(".agent-toolbox-card button");
  assert.match(await page.$eval(".agent-toolbox-card", (node) => node.textContent),
    /WAITING_PROVISION_APPROVAL/);
  await page.click(".agent-toolbox-card button");
  await waitToolingState(page, ".agent-toolbox-card", "WAITING_INTEGRATION_APPROVAL",
    "#agent-toolbox-refresh");
  await page.click(".agent-toolbox-card button");
  await waitToolingState(page, ".agent-toolbox-card", "ACTIVE", "#agent-toolbox-refresh");
  await page.click("#search");
  await page.type("#search", "SPECIMEN_user");
  await page.click("#search-button");
  await page.waitForFunction(() => [...document.querySelectorAll("#actions button")]
    .some((button) => button.textContent.includes("Structurer ce pseudo avec jq")));
  await page.click('[role="tab"][data-panel="actions-panel"]');
  const action = await page.$("#actions [data-capability-id='data.jq.username']");
  assert(action);
  await action.click();
  await page.waitForFunction(() => document.querySelector(".agent-conversation")
    ?.textContent.includes("data.jq.username"));
  console.log("PASS Firefox provisioning SPECIMEN : A/B, menu dynamique, execute");
} finally {
  await fixture.close();
}
