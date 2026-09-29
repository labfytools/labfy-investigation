import { access } from "node:fs/promises";
import { resolve } from "node:path";
import { fileURLToPath } from "node:url";
import {
  PROCESS_STATUSES,
  create_interruption_controller,
  supervise_process,
} from "./owned_process_supervisor.mjs";

// CONTRACT: this ordered list is the complete browser-validation suite.
export const scenarios = Object.freeze([
  "test_browser.mjs",
  // CONTRACT: le point d'entrée utilisateur ouvre directement le workbench,
  // établit le cookie local et refuse toute origine de mutation étrangère.
  "test_direct_session_browser.mjs",
  "test_core_browser.mjs",
  "test_eml_browser.mjs",
  "test_local_toolkit_browser.mjs",
  "test_workspace_control_browser.mjs",
  "test_j8_browser.mjs",
  "test_j8_runtime_browser.mjs",
  "test_j9_browser.mjs",
  "test_workbench_browser.mjs",
  "test_workbench_populated_browser.mjs",
  // CONTRACT: cette vérification Firefox inspecte les tailles cibles, le
  // tiroir, les panneaux persistants et la surface Agent uniquement sur fixture.
  "test_visual_polish_browser.mjs",
  "test_local_workspace_import_browser.mjs",
  // Le scénario import/rapport précédent et ce cycle du vrai launcher forment
  // la preuve E2E : données SPECIMEN, rapport, bibliothèque, redémarrages et A/B.
  "test_web_app_browser.mjs",
  // CONTRACT: le service persistant n'inspecte la bibliothèque qu'après le
  // clic explicite de l'utilisateur, puis ouvre un workspace SPECIMEN choisi.
  "test_lazy_library_browser.mjs",
  // CONTRACT: ce parcours Firefox exerce le vrai formulaire et une expiration
  // de harness isolée ; il démontre l'arrêt du polling et la reconnexion sans
  // rejouer une mutation dont l'admission n'est pas connue.
  "test_session_lifetime_browser.mjs",
  // CONTRACT: le fake agent lit le backend et prépare seulement l'autorisation
  // recherche ; son activité est obtenue par polling borné, jamais par SSE.
  "test_agent_tool_protocol_browser.mjs",
  // CONTRACT: le runtime modèle contacte seulement un fake OpenAI loopback et
  // rend les événements du turn actif sans exposer les messages modèle.
  "test_local_model_agent_browser.mjs",
  // CONTRACT: mission et proposition restent contrôlées par les routes backend;
  // cette surface Firefox vérifie les budgets, décisions et refus SPECIMEN.
  "test_operational_agent_browser.mjs",
  // CONTRACT: deux vagues de recherche SPECIMEN approuvées sont visibles ; une
  // branche refusée demeure hors transport et ne peut donc produire de résultat.
  "test_research_browser.mjs",
  "test_tool_provisioning_browser.mjs",
  "test_qwen_self_integration_browser.mjs",
]);

const tests_directory = fileURLToPath(new URL(".", import.meta.url));

function print_result(scenario, result, started_ms, output) {
  output.log(
    `SCENARIO fin ${scenario} ${new Date().toISOString()} résultat=${result.status} ` +
    `code=${result.code ?? "null"} signal=${result.signal ?? "null"} ` +
    `durée_ms=${Date.now() - started_ms}`,
  );
  if (result.error) output.error(`SCENARIO erreur ${scenario}: ${result.error}`);
}

export function summarize_browser_results(results, expected_count) {
  const counts = Object.fromEntries(PROCESS_STATUSES.map((status) => [status, 0]));
  for (const result of results) counts[result.status] += 1;
  const failures = results.filter(({ status }) => status !== "SUCCESS");
  const executed = results.filter(({ status }) => status !== "MISSING").length;
  return {
    counts,
    failures,
    executed,
    exit_code: failures.length === 0 && executed === expected_count ? 0 : 1,
  };
}

export async function run_browser_suite({
  scenario_names = scenarios,
  directory = tests_directory,
  timeout_ms,
  term_grace_ms = 1000,
  kill_grace_ms = 1000,
  interruption = create_interruption_controller(),
  output = console,
} = {}) {
  const results = [];

  try {
    for (const scenario of scenario_names) {
      if (interruption.signal.aborted) break;

      const started_ms = Date.now();
      output.log(`SCENARIO début ${scenario} ${new Date().toISOString()}`);
      const scenario_path = resolve(directory, scenario);

      try {
        await access(scenario_path);
      } catch {
        const missing = { status: "MISSING", code: null, signal: null, error: null };
        print_result(scenario, missing, started_ms, output);
        results.push({ scenario, ...missing });
        continue;
      }

      const result = await supervise_process({
        command: process.execPath,
        args: [scenario_path],
        cwd: directory,
        timeout_ms,
        term_grace_ms,
        kill_grace_ms,
        abort_signal: interruption.signal,
      });
      print_result(scenario, result, started_ms, output);
      results.push({ scenario, ...result });

      // CONTRACT: an operator interruption cancels the current owned unit and
      // prevents every later scenario from being launched.
      if (result.status === "INTERRUPTED") break;
    }

    if (interruption.signal.aborted && !results.some(({ status }) => status === "INTERRUPTED")) {
      results.push({
        scenario: "<suite>", status: "INTERRUPTED", code: null,
        signal: interruption.interrupted_signal, error: null,
      });
    }
  } finally {
    interruption.dispose();
  }

  const summary = summarize_browser_results(results, scenario_names.length);
  output.log(
    `SUITE navigateur scénarios_exécutés=${summary.executed}/${scenario_names.length} ` +
    `échecs=${summary.failures.length} ` +
    `résultats=${PROCESS_STATUSES.map((status) => `${status}:${summary.counts[status]}`).join(",")}`,
  );
  return {
    results,
    counts: summary.counts,
    exit_code: summary.exit_code,
  };
}

async function main() {
  const timeout_ms = Number.parseInt(process.env.LABFY_BROWSER_SCENARIO_TIMEOUT_MS ?? "300000", 10);
  if (!Number.isSafeInteger(timeout_ms) || timeout_ms <= 0) {
    console.error("LABFY_BROWSER_SCENARIO_TIMEOUT_MS doit être un entier positif.");
    process.exitCode = 2;
    return;
  }
  const summary = await run_browser_suite({ timeout_ms });
  process.exitCode = summary.exit_code;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  await main();
}
