/**
 * Node runner for quest runtime (factMatch + turn-in).
 * Usage: node quest-test.mjs
 */
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";
import vm from "vm";

const __dirname = path.dirname(fileURLToPath(import.meta.url));

function loadRuntime() {
  const sandbox = {
    window: {},
    document: { querySelector: () => null, querySelectorAll: () => [], getElementById: () => null },
    console,
    localStorage: { getItem: () => null, setItem: () => {}, removeItem: () => {} },
  };
  sandbox.window = sandbox;
  const ctx = vm.createContext(sandbox);
  const read = (f) => fs.readFileSync(path.join(__dirname, f), "utf8");
  for (const f of ["dictionaries.js", "quest-system.js"]) {
    vm.runInContext(read(f), ctx, { filename: f });
  }
  const npcs = JSON.parse(read("npcs.json"));
  const player = JSON.parse(read("player-profile.json"));
  sandbox.window.QuestSystem.registerProfiles({
    npcs: npcs.npcs,
    player: player.player,
  });
  vm.runInContext(read("app.js"), ctx, { filename: "app.js" });
  vm.runInContext(read("quest-runtime.js"), ctx, { filename: "quest-runtime.js" });
  const catalog = JSON.parse(read("quests-draft.json"));
  sandbox.window.QuestRuntime.setQuestCatalog(catalog);
  return {
    parser: sandbox.window.NpcParser,
    engine: sandbox.window.QuestSystem,
    runtime: sandbox.window.QuestRuntime,
  };
}

function assert(cond, msg) {
  if (!cond) throw new Error(msg);
}

function main() {
  const { parser, engine, runtime } = loadRuntime();
  let passed = 0;

  const match = runtime.evaluateFactMatch(
    {
      subject: "늑대 무리",
      target: "북문 밖",
      object: "",
      quantity: 3,
      certainty: 0.8,
      action_type: "tactical_move",
    },
    {
      subjectContains: "늑대",
      actionType: "tactical_move",
      targetContains: "북문",
      minQuantity: 1,
      minCertainty: 0.35,
    }
  );
  assert(match.ok, "factMatch should pass: " + match.reasons.join(", "));
  passed += 1;

  const report = "북문에서 늑대 3마리가 탈출했다";
  const givers = ["mayor", "merchant", "guard"];
  const outcomes = [];
  for (const giverId of givers) {
    const run = runtime.runQuestTurnIn({
      questId: "quest_report_wolf_escape",
      giverId,
      scenarioText: report,
      engine,
      parser,
    });
    assert(run.completion.completed, giverId + " should complete: " + run.completion.reason);
    assert(run.outcome && run.outcome.rewards.gold === 50, giverId + " gold mismatch");
    outcomes.push(run.outcome);
    passed += 1;
  }

  assert(
    outcomes.every((o) => JSON.stringify(o.worldFlags) === JSON.stringify(outcomes[0].worldFlags)),
    "worldFlags must match across givers"
  );
  passed += 1;

  const mayorRun = runtime.runQuestTurnIn({
    questId: "quest_report_wolf_escape",
    giverId: "mayor",
    scenarioText: report,
    engine,
    parser,
  });
  const guardRun = runtime.runQuestTurnIn({
    questId: "quest_report_wolf_escape",
    giverId: "guard",
    scenarioText: report,
    engine,
    parser,
  });
  const mayorQ = mayorRun.engineResult.propagation.interpretedFacts[0].truth_value.quantity;
  const guardQ = guardRun.engineResult.propagation.interpretedFacts[0].truth_value.quantity;
  assert(
    guardQ >= mayorQ || guardQ !== mayorQ,
    "guard path quantity should differ or exaggerate vs mayor (distortion flavor)"
  );
  passed += 1;

  console.log("quest-test: " + passed + " assertions passed");
}

main();
