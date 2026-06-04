/**
 * Usage: node process-steps-test.mjs
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
  const { parser, engine, runtime } = load();
  const report = "안개 계곡에 밀수 화물이 버려져 있다. 마약초 열두 개가 들어 있다";

  for (const giverId of ["guard_timid", "merchant_greedy", "scholar_alric"]) {
    const preview = runtime.runProcessSteps({
      questId: QUEST_ID,
      giverId,
      scenarioText: report,
      engine,
      parser,
      sessionKey: "pstep-" + giverId,
    });
    assert(preview.steps.length === 3, giverId + " should have 3 steps");
    assert(preview.steps[0].type === "dialogue" && preview.steps[0].npcLine, giverId + " briefing");
    assert(preview.steps[2].triggersEngine, giverId + " fact_input triggers engine");
    assert(preview.acceptDialogue.line, giverId + " accept line");
    assert(
      preview.turnInResult && preview.turnInResult.completion.completed,
      giverId + " turn-in via process steps"
    );
  }

  const guardAccept = runtime.getAcceptDialogue(QUEST_ID, "guard_timid", { sessionKey: "pstep-accept" });
  const merchantAccept = runtime.getAcceptDialogue(QUEST_ID, "merchant_greedy", { sessionKey: "pstep-accept" });
  assert(guardAccept.line !== merchantAccept.line, "per-giver neutral accept lines differ");
  assert(guardAccept.source === "giver.acceptDialogue", "guard uses giver.acceptDialogue");

  console.log("process-steps-test: passed");
}

main();
