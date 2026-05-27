import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";
import vm from "vm";

const __dirname = path.dirname(fileURLToPath(import.meta.url));

function load() {
  const sandbox = {
    window: {},
    document: { querySelector: () => null, querySelectorAll: () => [], getElementById: () => null },
    console,
    localStorage: { getItem: () => null, setItem: () => {}, removeItem: () => {} },
  };
  sandbox.window = sandbox;
  const ctx = vm.createContext(sandbox);
  for (const f of ["dictionaries.js", "quest-system.js", "app.js"]) {
    vm.runInContext(fs.readFileSync(path.join(__dirname, f), "utf8"), ctx, { filename: f });
  }
  return { parser: sandbox.window.NpcParser, engine: sandbox.window.QuestSystem };
}

function main() {
  const { parser, engine } = load();
  const suite = JSON.parse(fs.readFileSync(path.join(__dirname, "integration-test-cases.json"), "utf8"));
  let pass = 0;
  let fail = 0;
  for (const c of suite.cases) {
    let ok = true;
    let reason = "";
    try {
      if (c.custom === "chain") {
        engine.clearSession("itest-chain");
        const facts = parser.buildFactsFromParsed(parser.parseScenarioText(c.input));
        const base = engine.getScenario({ sessionKey: "itest-chain" });
        const chain = engine.propagateChain(
          [
            { sender: base.sender, receiver: base.receiver },
            { sender: base.receiver, receiver: base.sender },
          ],
          facts,
          base.info,
          { quantityMode: "faithful", currentTick: 1000 }
        );
        if (chain.hopResults.length < c.expect.hopMin) ok = false;
      } else if (c.id === "parser-qty-inline") {
        const facts = parser.buildFactsFromParsed(parser.parseScenarioText(c.input));
        if (facts[0].quantity !== 3 || !facts[0].is_countable) {
          ok = false;
          reason = `qty=${facts[0].quantity} countable=${facts[0].is_countable}`;
        }
      } else if (c.custom === "kbConflict") {
        engine.clearSession("itest-kbconflict");

        const threatFacts = [
          {
            fact_id: "F01",
            subject: "늑대",
            action: "탈출",
            target: "북문",
            quantity: 3,
            certainty: 0.7,
            is_countable: true,
            action_type: "tactical_move",
            parse_mode: "structured",
            parse_confidence: 0.9,
          },
        ];
        const calmFacts = [
          {
            fact_id: "F02",
            subject: "늑대",
            action: "태연한 상태다",
            target: "북문",
            quantity: 1,
            certainty: 0.6,
            is_countable: false,
            action_type: "state",
            parse_mode: "structured",
            parse_confidence: 0.9,
          },
        ];

        const r1 = engine.executeScenario({
          facts: threatFacts,
          persistSession: true,
          sessionKey: "itest-kbconflict",
          quantityMode: "faithful",
          currentTick: 1000,
          senderStats: { fear: 0.9, hostility: 0.2, trust: 0.55 },
          receiverStats: { credulity: 0.7, trust: 0.65 },
          allowPartialTrust: false,
        });
        const threat1 = (r1.knowledgeBaseSnapshot || []).find(
          (x) =>
            String(x.truth_value.action_type || "").toLowerCase() === "tactical_move" &&
            String(x.truth_value.target || "").indexOf("북문") >= 0
        );
        const c1 = threat1 ? Number(threat1.truth_value.certainty || 0) : 0;

        const r2 = engine.executeScenario({
          facts: calmFacts,
          persistSession: true,
          sessionKey: "itest-kbconflict",
          quantityMode: "faithful",
          currentTick: 1001,
          senderStats: { fear: 0.1, hostility: 0.2, trust: 0.55 },
          receiverStats: { credulity: 0.8, trust: 0.65 },
          allowPartialTrust: false,
        });
        const threat2 = (r2.knowledgeBaseSnapshot || []).find(
          (x) =>
            String(x.truth_value.action_type || "").toLowerCase() === "tactical_move" &&
            String(x.truth_value.target || "").indexOf("북문") >= 0
        );
        const c2 = threat2 ? Number(threat2.truth_value.certainty || 0) : 0;

        if (c.expect && c.expect.threatCertaintyDecreased && !(c2 < c1)) {
          ok = false;
          reason = `threat certainty did not decrease: ${c1} -> ${c2}`;
        }
      } else {
        const facts = parser.buildFactsFromParsed(parser.parseScenarioText(c.input));
        const r = engine.executeScenario({ facts, quantityMode: "faithful", senderStats: { fear: 0.9 } });
        if (c.expect.auditDiffMin && (r.auditDiff || []).length < c.expect.auditDiffMin) ok = false;
      }
    } catch (e) {
      ok = false;
      reason = e.message;
    }
    if (ok) {
      pass += 1;
      console.log("[PASS]", c.id);
    } else {
      fail += 1;
      console.log("[FAIL]", c.id, reason);
    }
  }
  console.log(`\nsummary: ${pass} passed, ${fail} failed`);
  process.exit(fail ? 1 : 0);
}

main();
