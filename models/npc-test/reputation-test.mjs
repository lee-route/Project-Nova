/**
 * Usage: node reputation-test.mjs
 */
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";
import vm from "vm";

const __dirname = path.dirname(fileURLToPath(import.meta.url));

function load() {
  const sandbox = {
    window: {},
    document: { querySelector: () => null },
    console,
    localStorage: { getItem: () => null, setItem: () => {}, removeItem: () => {} },
  };
  sandbox.window = sandbox;
  const ctx = vm.createContext(sandbox);
  const read = (f) => fs.readFileSync(path.join(__dirname, f), "utf8");
  for (const f of ["dictionaries.js", "quest-system.js", "reputation-system.js"]) {
    vm.runInContext(read(f), ctx, { filename: f });
  }
  const player = JSON.parse(read("player-profile.json"));
  sandbox.window.QuestSystem.registerProfiles({
    npcs: JSON.parse(read("npcs.json")).npcs,
    player: player.player,
  });
  vm.runInContext(read("npc-parser.js"), ctx, { filename: "npc-parser.js" });
  vm.runInContext(read("quest-runtime.js"), ctx, { filename: "quest-runtime.js" });
  sandbox.window.QuestRuntime.setQuestCatalog(JSON.parse(read("quests-draft.json")));
  return {
    Rep: sandbox.window.ReputationSystem,
    runtime: sandbox.window.QuestRuntime,
    engine: sandbox.window.QuestSystem,
    parser: sandbox.window.NpcParser,
  };
}

function assert(cond, msg) {
  if (!cond) throw new Error(msg);
}

function main() {
  const { Rep, runtime, engine, parser } = load();
  Rep.clearState("test");

  const r1 = Rep.applyNpcDelta("test", "scholar_alric", 0.12, "test");
  assert(r1.change.after > r1.change.before, "scholar rep increase");

  const report = "안개 계곡에 밀수 화물이 버려져 있다. 마약초 열두 개가 들어 있다";
  const run = runtime.runQuestTurnIn({
    questId: "quest_abandoned_cargo",
    giverId: "merchant_greedy",
    scenarioText: report,
    engine,
    parser,
    sessionKey: "quest-rep",
    reputationSessionKey: "quest-rep",
  });
  assert(run.completion.completed, "cargo quest complete");
  assert(run.reputationResult.changes.length > 0, "rep changes applied");
  assert(run.reputationResult.state.villageReputation > 50, "village +5");

  console.log("reputation-test: all passed");
}

main();
