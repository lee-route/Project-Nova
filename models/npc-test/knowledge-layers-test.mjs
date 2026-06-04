/**
 * WorldTruth + PlayerKnowledge regression
 */
import path from "path";
import { fileURLToPath } from "url";
import { loadTestEngine } from "./test-engine-bootstrap.mjs";

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const { engine, WorldTruth, PlayerKnowledge, GameClock } = loadTestEngine(__dirname);

let failed = 0;
function assert(cond, msg) {
  if (!cond) {
    console.error("FAIL:", msg);
    failed += 1;
  } else {
    console.log("ok:", msg);
  }
}

GameClock.clearClock("pk-test");
PlayerKnowledge.clearAll("pk-test");
WorldTruth.clearWorld("pk-test");

WorldTruth.setWorldFacts(
  "pk-test",
  [{ subject: "마약초", quantity: 12, is_countable: true, action_type: "inventory" }],
  1000
);

const r = engine.executeScenario({
  sessionKey: "pk-test",
  usePlayerAsSender: true,
  receiverProfileKey: "scholar_alric",
  facts: [
    {
      fact_id: "F01",
      subject: "마약초",
      action: "들어 있다",
      target: "안개 계곡",
      quantity: 12,
      certainty: 0.95,
      is_countable: true,
      action_type: "inventory",
    },
  ],
  quantityMode: "faithful",
  recordPlayerKnowledge: true,
});

assert(r.playerKnowledgeRecord && r.playerKnowledgeRecord.recorded, "PKB records player turn-in");
const view = PlayerKnowledge.getPlayerView("pk-test");
assert(view.entries.length >= 1, "PKB has entries");
const cmp = PlayerKnowledge.comparePlayerToWorld("pk-test");
assert(cmp.hasWorldTruth === true, "compare uses WorldTruth");
assert(cmp.score != null, "compare score computed");

console.log(failed ? "FAILED" : "knowledge-layers-test passed");
process.exit(failed ? 1 : 0);
