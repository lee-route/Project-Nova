/**
 * Usage: node accept-dialogue-test.mjs
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
  sandbox.window.QuestSystem.registerProfiles({
    npcs: JSON.parse(read("npcs.json")).npcs,
    player: JSON.parse(read("player-profile.json")).player,
  });
  vm.runInContext(read("quest-runtime.js"), ctx, { filename: "quest-runtime.js" });
  sandbox.window.QuestRuntime.setQuestCatalog(JSON.parse(read("quests-draft.json")));
  return { Rep: sandbox.window.ReputationSystem, runtime: sandbox.window.QuestRuntime };
}

function assert(cond, msg) {
  if (!cond) throw new Error(msg);
}

function main() {
  const { Rep, runtime } = load();
  const Q = "quest_abandoned_cargo";

  Rep.clearState("adt");
  const lines = {};
  for (const giverId of ["guard_timid", "merchant_greedy", "scholar_alric"]) {
    const a = runtime.getAcceptDialogue(Q, giverId, { sessionKey: "adt" });
    assert(a.canAccept, giverId + " neutral accept");
    assert(a.source === "giver.acceptDialogue", giverId + " uses per-giver line");
    lines[giverId] = a.line;
  }
  assert(lines.guard_timid !== lines.merchant_greedy, "guard vs merchant accept differ");
  assert(lines.scholar_alric.indexOf("사실") >= 0 || lines.scholar_alric.indexOf("정확") >= 0, "scholar tone");

  Rep.applyNpcDelta("adt", "scholar_alric", 0.35, "test");
  const trusted = runtime.getAcceptDialogue(Q, "scholar_alric", { sessionKey: "adt" });
  assert(trusted.line !== lines.scholar_alric, "trusted uses shared tier line");
  assert(trusted.source.indexOf("sharedAcceptDialogueByTier") >= 0, "high tier from shared");

  Rep.clearState("adt2");
  Rep.applyNpcDelta("adt2", "scholar_alric", -0.35, "test");
  const refused = runtime.getAcceptDialogue(Q, "scholar_alric", { sessionKey: "adt2" });
  assert(refused.canAccept === false, "hostile should not accept");
  assert(refused.line.indexOf("사기꾼") >= 0 || refused.source.indexOf("refused") >= 0, "refused line");

  console.log("accept-dialogue-test: passed");
}

main();
