import assert from "node:assert/strict";

// CONTRACT: les scénarios Firefox utilisent le parcours produit complet. Ce
// helper ne connaît ni code de bootstrap ni cookie synthétique : le serveur
// établit le cookie HttpOnly lors de GET / et l'application lit le CSRF.
export async function openAutomaticSession(page, origin, label = "session") {
  await page.goto(origin, { waitUntil: "domcontentloaded" });
  return assertAutomaticSessionReady(page, new URL(origin).origin, label);
}

// CONTRACT: Firefox BiDi peut ne jamais résoudre page.goto() lorsqu'un serveur
// possédé redémarre sur la même origine. Cette primitive conserve la navigation
// réelle (GET / et ses redirections) sans faire de cette promesse BiDi la preuve
// de disponibilité. Le même onglet est indispensable aux brouillons
// sessionStorage du scénario A/B.
export async function resumeAutomaticSessionAfterOwnedRestart(page, origin, label = "restart") {
  const expectedOrigin = new URL(origin).origin;
  assert.equal(page.url(), "about:blank", `${label}: page détachée attendue`);
  // WHY: le document racine peut être mis en cache par Firefox après l'arrêt
  // de l'instance possédée. Désactiver ce cache pour cette reprise impose le
  // GET / réel qui crée le nouveau cookie HttpOnly via la redirection 303.
  await page.setCacheEnabled(false);
  await replaceLocation(page, origin);
  await waitForResumedDocument(page, expectedOrigin);
  try {
    return await assertAutomaticSessionReady(page, expectedOrigin, label);
  } catch (error) {
    if (!new Set(["LABFY_WORKBENCH_UNAVAILABLE", "LABFY_SESSION_UNAVAILABLE"])
      .has(error?.code)) throw error;
    // INVARIANT: cette seule relance ne s'applique qu'après une navigation
    // racine valide et une session API 200 ; elle ne peut pas masquer un 401,
    // un CSRF absent, une mauvaise origine ou une page login/bootstrap.
    if (error.code === "LABFY_WORKBENCH_UNAVAILABLE") await assertSessionApi(page, label);
    await replaceLocation(page, origin);
    await waitForResumedDocument(page, expectedOrigin);
    return assertAutomaticSessionReady(page, expectedOrigin, label);
  }
}

async function replaceLocation(page, origin) {
  await page.evaluate((nextOrigin) => {
    // INVARIANT: le timer laisse l'évaluation se terminer avant le changement
    // de document, qui détruirait autrement son ExecutionContext.
    window.setTimeout(() => window.location.replace(nextOrigin), 0);
  }, origin);
}

async function waitForResumedDocument(page, expectedOrigin) {
  await page.waitForFunction((expectedOrigin) =>
    location.origin === expectedOrigin &&
      (document.readyState === "interactive" || document.readyState === "complete"),
  {}, expectedOrigin);
}

async function assertAutomaticSessionReady(page, origin, label) {
  assert.equal(await page.evaluate(() => location.origin), origin,
    `${label}: origine de reprise incorrecte`);
  try {
    await page.waitForFunction(() =>
      document.querySelector("#library-home:not([hidden]), #workbench-shell:not([hidden])") !== null,
    { timeout: 15000 });
  } catch (error) {
    const diagnostic = await page.evaluate(() => ({
      href: location.href,
      readyState: document.readyState,
      connection: document.querySelector("#library-connection")?.textContent ?? "",
      state: window.__LABFY_TEST__?.getState?.() ?? null,
      body: document.body?.textContent?.slice(0, 500) ?? "",
    })).catch((diagnosticError) => ({ evaluationError: String(diagnosticError) }));
    const unavailable = new Error(
      `${label}: workbench indisponible après reprise ${JSON.stringify(diagnostic)}`,
      { cause: error });
    unavailable.code = "LABFY_WORKBENCH_UNAVAILABLE";
    throw unavailable;
  }
  assert.equal(await page.$("#bootstrap-code"), null);
  assert.equal(await page.$("#login-form"), null);
  assert.doesNotMatch(await page.$eval("body", (body) => body.textContent),
    /Code de session éphémère/);
  const session = await assertSessionApi(page, label);
  return session;
}

async function assertSessionApi(page, label) {
  const session = await page.evaluate(async () => {
    const response = await fetch("/api/v1/session", { cache: "no-store" });
    return { status: response.status, body: await response.json() };
  });
  if (session.status !== 200) {
    const unavailable = new Error(`${label}: session API indisponible (${session.status})`);
    unavailable.code = "LABFY_SESSION_UNAVAILABLE";
    throw unavailable;
  }
  assert.equal(typeof session.body.csrf, "string", `${label}: CSRF absent`);
  assert.ok(session.body.csrf.length > 0, `${label}: CSRF vide`);
  return session.body;
}
