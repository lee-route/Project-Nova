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
  vm.runInContext(read("app.js"), ctx, { filename: "app.js" });
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

  const before = Rep.snapshot("test");
  const r1 = Rep.applyNpcDelta("test", "mayor", 0.12, "test");
  assert(r1.change.after > r1.change.before, "mayor rep should increase");

  const trustLow = Rep.resolveTrustFromReputation("mayor", 0.7, "test");
  Rep.applyNpcDelta("test", "mayor", -0.4, "test");
  const trustAfterDrop = Rep.resolveTrustFromReputation("mayor", 0.7, "test");
  assert(trustAfterDrop < trustLow, "trust should drop when rep drops");

  Rep.clearState("quest-rep");
  const report = "북문에서 늑대 3마리가 탈출했다";
  const run = runtime.runQuestTurnIn({
    questId: "quest_report_wolf_escape",
    giverId: "guard",
    scenarioText: report,
    engine,
    parser,
    sessionKey: "quest-rep",
    reputationSessionKey: "quest-rep",
  });
  assert(run.completion.completed, "wolf quest should complete");
  assert(run.reputationResult && run.reputationResult.changes.length > 0, "rep changes on complete");
  const guardRep = run.reputationResult.state.npcReputation.guard;
  assert(guardRep > 0.5, "guard rep should rise after guard path quest");

  console.log("reputation-test: all passed");
  console.log("  guard rep after quest:", guardRep);
  console.log("  changes:", run.reputationResult.changes.map((c) => c.key + " " + c.delta).join(", "));
}

main();
