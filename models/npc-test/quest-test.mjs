/**
 * Usage: node quest-test.mjs
 */
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";
import vm from "vm";

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const QUEST_ID = "quest_abandoned_cargo";

function loadRuntime() {
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

  const match = runtime.evaluateFactMatch(
    { subject: "마약초", quantity: 12, certainty: 0.8, action_type: "unknown" },
    runtime.getQuest(QUEST_ID).finalObjective.factMatch
  );
  assert(match.ok, "factMatch: " + match.reasons.join(", "));

  const reportHigh = "안개 계곡에 밀수 화물이 버려져 있다. 마약초 열두 개가 들어 있다";
  const guardRun = runtime.runQuestTurnIn({
    questId: QUEST_ID,
    giverId: "guard_timid",
    scenarioText: reportHigh,
    engine,
    parser,
    sessionKey: "qtest-guard",
    reputationSessionKey: "qtest-guard",
  });
  assert(guardRun.completion.completed, "guard path should complete");
  assert(
    guardRun.outcomeBranch.branchId === "authority_path",
    "guard dramatic expected authority, got " + guardRun.outcomeBranch.branchId
  );
  assert(guardRun.outcome.rewards.gold === 150, "authority gold");

  const reportLow = "안개 계곡에 밀수 화물이 버려져 있다. 마약초 다섯 개가 들어 있다";
  const scholarRun = runtime.runQuestTurnIn({
    questId: QUEST_ID,
    giverId: "scholar_alric",
    scenarioText: reportLow,
    engine,
    parser,
    sessionKey: "qtest-scholar",
    reputationSessionKey: "qtest-scholar",
  });
  assert(scholarRun.completion.completed, "scholar path should complete");
  assert(
    scholarRun.outcomeBranch.branchId === "black_market_path",
    "scholar faithful low qty expected black_market, got " + scholarRun.outcomeBranch.branchId
  );
  assert(scholarRun.outcome.rewards.gold === 300, "black_market gold");

  console.log("quest-test: all passed");
  console.log("  guard branch:", guardRun.outcomeBranch.branchId, "qty", guardRun.outcomeBranch.metrics.quantity);
  console.log("  scholar branch:", scholarRun.outcomeBranch.branchId, "qty", scholarRun.outcomeBranch.metrics.quantity);
}

main();
