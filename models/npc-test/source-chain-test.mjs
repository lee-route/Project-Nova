/**
 * source_chain / hop_depth distortion rules
 */
import path from "path";
import { fileURLToPath } from "url";
import { loadTestEngine } from "./test-engine-bootstrap.mjs";

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const { engine } = loadTestEngine(__dirname);

function collectRules(result) {
  const rules = [];
  for (const item of result.auditTrail || []) {
    for (const r of item.applied_rules || []) {
      if (!rules.includes(r)) rules.push(r);
    }
  }
  for (const item of result.propagation.interpretedFacts || []) {
    for (const r of item.metadata.applied_rules || []) {
      if (!rules.includes(r)) rules.push(r);
    }
  }
  return rules;
}

let failed = 0;
function assert(cond, msg) {
  if (!cond) {
    console.error("FAIL:", msg);
    failed += 1;
  } else {
    console.log("ok:", msg);
  }
}

const guard = engine.createNpcFromProfile("guard_timid");
const scholar = engine.createNpcFromProfile("scholar_alric");
guard.setTrustLevel(scholar.id, 0.8);
scholar.setTrustLevel(guard.id, 0.75);

const facts = [
  {
    fact_id: "F01",
    subject: "밀수 화물",
    action: "버려져 있다",
    target: "안개 계곡",
    quantity: 3,
    certainty: 0.9,
    is_countable: true,
    action_type: "tactical_move",
    source_chain: ["목격자", "상인", guard.name],
  },
];

const info = {
  info_id: "INF_CHAIN",
  truth_value: { subject: "info", action: "spread", target: "현장", quantity: 1, certainty: 0.8 },
  metadata: { origin: guard.name, source: guard.name, creation_tick: 2000 },
};

const chain = engine.propagateChain(
  [{ sender: guard, receiver: scholar }],
  facts,
  info,
  { currentTick: 2000, quantityMode: "dramatic" }
);

const hopRules = [];
chain.hopResults.forEach((h) => {
  const interpreted = h.propagation.interpretedFacts || [];
  interpreted.forEach((item) => {
    (item.metadata.applied_rules || []).forEach((r) => {
      if (!hopRules.includes(r)) hopRules.push(r);
    });
    assert((item.metadata.source_chain || []).length >= 2, "source_chain appended on hop");
    assert(item.metadata.hop_depth >= 1, "hop_depth set");
  });
});

assert(
  hopRules.includes("source_chain_secondhand") || hopRules.includes("source_chain_multi_hop"),
  "chain hop triggers source_chain rules"
);

const grounded = engine.toGroundedFact(
  { subject: "밀수", action: "a", target: "t", quantity: 1, certainty: 0.5, is_countable: true, action_type: "x" },
  { source_chain: ["A", "B", "C"], hop_depth: 2 }
);
assert(grounded.source_chain.length === 3 && grounded.hop_depth === 2, "toGroundedFact exports chain");

console.log(failed ? "FAILED" : "source-chain-test passed");
process.exit(failed ? 1 : 0);
