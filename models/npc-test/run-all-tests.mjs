/**
 * Run all Node regression suites (excluding batch-parse stochastic QA).
 * Usage: node run-all-tests.mjs
 */
import { spawnSync } from "child_process";
import path from "path";
import { fileURLToPath } from "url";

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const suites = [
  "npc-parser-test.mjs",
  "deception-audit-test.mjs",
  "api-server-test.mjs",
  "quest-presentation-test.mjs",
  "distortion-test.mjs",
  "integration-test.mjs",
  "quest-test.mjs",
  "reputation-test.mjs",
  "accept-dialogue-test.mjs",
  "process-steps-test.mjs",
  "profiles-sync-test.mjs",
  "quest-flow-test.mjs",
  "knowledge-layers-test.mjs",
  "source-chain-test.mjs",
  "llm-anchoring-test.mjs",
  "batch-parse-quick-test.mjs",
];

let failed = 0;
for (const file of suites) {
  const full = path.join(__dirname, file);
  console.log("\n>>> " + file);
  const r = spawnSync(process.execPath, [full], { stdio: "inherit", cwd: __dirname });
  if (r.status !== 0) failed += 1;
}

console.log(failed ? "\nFAILED " + failed + " suite(s)" : "\nAll suites passed.");
process.exit(failed ? 1 : 0);
