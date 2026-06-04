/**
 * 밀수 화물 퀘스트 — 3 의뢰인 × high/low report 리포트
 * Usage: node cargo-slice-report.mjs
 */
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";
import vm from "vm";

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const QUEST_ID = "quest_abandoned_cargo";
const REPORT_HIGH = "안개 계곡에 밀수 화물이 버려져 있다. 마약초 열두 개가 들어 있다";
const REPORT_LOW = "안개 계곡에 밀수 화물이 버려져 있다. 마약초 다섯 개가 들어 있다";
const GIVERS = ["guard_timid", "merchant_greedy", "scholar_alric"];

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

function runPath(runtime, engine, parser, giverId, text) {
  const run = runtime.runQuestTurnIn({
    questId: QUEST_ID,
    giverId,
    scenarioText: text,
    engine,
    parser,
    sessionKey: "cargo-slice-" + giverId + "-" + text.length,
    reputationSessionKey: "cargo-slice-" + giverId + "-" + text.length,
  });
  const metrics = run.outcomeBranch && run.outcomeBranch.metrics;
  return {
    giverId,
    report: text,
    completed: run.completion.completed,
    branchId: run.outcomeBranch && run.outcomeBranch.branchId,
    gold: run.outcome && run.outcome.rewards && run.outcome.rewards.gold,
    worldFlags: run.outcome && run.outcome.worldFlags,
    quantity: metrics && metrics.quantity,
    certainty: metrics && metrics.certainty,
  };
}

function main() {
  const { parser, engine, runtime } = loadRuntime();
  const quest = runtime.getQuest(QUEST_ID);
  const pathsHigh = GIVERS.map((id) => runPath(runtime, engine, parser, id, REPORT_HIGH));
  const pathsLow = GIVERS.map((id) => runPath(runtime, engine, parser, id, REPORT_LOW));

  const report = {
    generatedAt: new Date().toISOString(),
    questId: QUEST_ID,
    title: quest.title,
    reportHigh: REPORT_HIGH,
    reportLow: REPORT_LOW,
    pathsHigh,
    pathsLow,
    note: "authority: qty>=10 & certainty>=0.6 | black_market: qty<=9 | scholar faithful keeps lower qty",
  };

  const outPath = path.join(__dirname, "cargo-slice-report.json");
  fs.writeFileSync(outPath, JSON.stringify(report, null, 2), "utf8");
  console.log("Wrote " + outPath);
  console.log(JSON.stringify({ pathsHigh, pathsLow }, null, 2));
}

main();
