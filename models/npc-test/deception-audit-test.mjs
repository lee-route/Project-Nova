import path from "path";
import { fileURLToPath } from "url";
import { loadTestEngine } from "./test-engine-bootstrap.mjs";

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const { WorldTruth, DeceptionAudit } = loadTestEngine(__dirname);

let failed = 0;
function assert(c, m) {
  if (!c) {
    console.error("FAIL:", m);
    failed += 1;
  } else console.log("ok:", m);
}

WorldTruth.setWorldFacts(
  "da-test",
  [{ subject: "마약초", quantity: 12, is_countable: true, action_type: "inventory", target: "안개 계곡" }],
  1
);

const audit = DeceptionAudit.auditTurnInReport(
  "da-test",
  [{ subject: "전혀 다른 물건", quantity: 1, is_countable: true, action_type: "inventory", target: "북문" }],
  {}
);
assert(audit.affectsOutcome === false, "audit does not affect outcome");
assert(audit.worldCompare && audit.worldCompare.hasWorldTruth === true, "world truth active");
assert(
  audit.worldCompare.contradictions && audit.worldCompare.contradictions.length >= 1,
  "detects report vs world mismatch"
);

console.log(failed ? "FAILED" : "deception-audit-test passed");
process.exit(failed ? 1 : 0);
