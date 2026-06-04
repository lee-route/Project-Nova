/**
 * Usage: node quest-flow-test.mjs
 */
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";
import vm from "vm";

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const QUEST_ID = "quest_abandoned_cargo";

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
  sandbox.window.QuestSystem.registerProfiles({
    npcs: JSON.parse(read("npcs.json")).npcs,
    player: JSON.parse(read("player-profile.json")).player,
  });
  vm.runInContext(read("quest-game-state.js"), ctx, { filename: "quest-game-state.js" });
  vm.runInContext(read("app.js"), ctx, { filename: "app.js" });
  vm.runInContext(read("quest-runtime.js"), ctx, { filename: "quest-runtime.js" });
  sandbox.window.QuestRuntime.setQuestCatalog(JSON.parse(read("quests-draft.json")));
  return {
    parser: sandbox.window.NpcParser,
    engine: sandbox.window.QuestSystem,
    runtime: sandbox.window.QuestRuntime,
    game: sandbox.window.QuestGameState,
  };
}

function assert(cond, msg) {
  if (!cond) throw new Error(msg);
}

function main() {
  const { parser, engine, runtime, game } = load();
  const report = "안개 계곡에 밀수 화물이 버려져 있다. 마약초 열두 개가 들어 있다";
  const sk = "qflow-test";

  game.clearState(sk);
  runtime.clearQuestFlow(sk);

  const accept = runtime.acceptQuest(QUEST_ID, "guard_timid", { sessionKey: sk });
  assert(accept.ok, "accept");
  assert(accept.briefing.briefingLine.indexOf("마약초") >= 0, "guard briefing");

  let stepsDone = 0;
  while (stepsDone < 10) {
    const adv = runtime.advanceProcessStep({
      sessionKey: sk,
      scenarioText: report,
      engine,
      parser,
      reputationSessionKey: sk,
    });
    if (adv.awaitingReport) break;
    assert(adv.ok, "advance: " + (adv.reason || ""));
    stepsDone += 1;
    if (adv.done) {
      assert(adv.instance.state === "completed", "completed state");
      assert(adv.turnInResult.outcomeBranch.branchId === "authority_path", "branch");
      break;
    }
  }

  const snap = game.snapshot(sk);
  assert(snap.gold >= 150, "gold applied");
  assert(snap.worldFlags.is_cargo_secured === true, "world flag");
  assert(snap.questHistory.length >= 1, "history");

  game.clearState(sk);
  runtime.clearQuestFlow(sk);
  const flow = runtime.runQuestFlow({
    questId: QUEST_ID,
    giverId: "scholar_alric",
    scenarioText: "안개 계곡에 밀수 화물이 버려져 있다. 마약초 다섯 개가 들어 있다",
    engine,
    parser,
    sessionKey: sk + "-full",
    reputationSessionKey: sk + "-full",
  });
  assert(flow.ok, "full flow scholar low");
  assert(flow.turnInResult.outcomeBranch.branchId === "black_market_path", "black market full flow");

  console.log("quest-flow-test: passed");
}

main();
