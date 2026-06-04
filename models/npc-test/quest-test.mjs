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
  vm.runInContext(read("npc-parser.js"), ctx, { filename: "npc-parser.js" });
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

  const altMatch = runtime.evaluateFactMatch(
    { subject: "밀수 화물", target: "안개 계곡", quantity: 1, certainty: 0.5, action_type: "state" },
    runtime.getQuest(QUEST_ID).finalObjective.factMatchAlt
  );
  assert(altMatch.ok, "factMatchAlt");

  const reportHigh = "안개 계곡에 밀수 화물이 버려져 있다. 마약초 열두 개가 들어 있다";
  const reportLow = "안개 계곡에 밀수 화물이 버려져 있다. 마약초 다섯 개가 들어 있다";
  const reportAltOnly = "안개 계곡에 밀수 화물이 버려져 있다";

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
  assert(guardRun.outcomeBranch.branchId === "authority_path", "guard authority");
  assert(guardRun.outcome.rewards.gold === 150, "authority gold");

  const merchantRun = runtime.runQuestTurnIn({
    questId: QUEST_ID,
    giverId: "merchant_greedy",
    scenarioText: reportHigh,
    engine,
    parser,
    sessionKey: "qtest-merchant",
    reputationSessionKey: "qtest-merchant",
  });
  assert(merchantRun.completion.completed, "merchant should complete");
  assert(merchantRun.outcomeBranch.branchId === "authority_path", "merchant high report authority");

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
  assert(scholarRun.outcomeBranch.branchId === "black_market_path", "scholar black_market");
  assert(scholarRun.outcome.rewards.gold === 300, "black_market gold");

  const altRun = runtime.runQuestTurnIn({
    questId: QUEST_ID,
    giverId: "scholar_alric",
    scenarioText: reportAltOnly,
    engine,
    parser,
    sessionKey: "qtest-alt",
    reputationSessionKey: "qtest-alt",
  });
  assert(altRun.completion.completed, "alt-only report should complete via factMatchAlt");

  const blockedRun = runtime.runQuestTurnIn({
    questId: QUEST_ID,
    giverId: "guard_timid",
    scenarioText: "오늘 날씨가 좋다",
    engine,
    parser,
    sessionKey: "qtest-fail",
    reputationSessionKey: "qtest-fail",
  });
  assert(!blockedRun.completion.completed, "irrelevant report should not complete");

  const branch = runtime.pickOutcomeBranch(
    [{ truth_value: { quantity: 8, certainty: 0.35 } }],
    runtime.getQuest(QUEST_ID)
  );
  assert(branch.branchId === "fallback", "mid metrics should hit outcomeFallback, got " + branch.branchId);

  console.log("quest-test: all passed");
  console.log("  guard:", guardRun.outcomeBranch.branchId, "qty", guardRun.outcomeBranch.metrics.quantity);
  console.log("  merchant:", merchantRun.outcomeBranch.branchId, "qty", merchantRun.outcomeBranch.metrics.quantity);
  console.log("  scholar low:", scholarRun.outcomeBranch.branchId, "qty", scholarRun.outcomeBranch.metrics.quantity);
}

main();
