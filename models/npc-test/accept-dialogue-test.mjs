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
  const player = JSON.parse(read("player-profile.json"));
  sandbox.window.QuestSystem.registerProfiles({
    npcs: JSON.parse(read("npcs.json")).npcs,
    player: player.player,
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
  Rep.clearState("adt");

  const low = runtime.getAcceptDialogue("quest_report_wolf_escape", "mayor", {
    sessionKey: "adt",
  });
  Rep.applyNpcDelta("adt", "mayor", 0.35, "test");
  const high = runtime.getAcceptDialogue("quest_report_wolf_escape", "mayor", {
    sessionKey: "adt",
  });

  assert(low.line !== high.line, "mayor accept line should change with rep");
  assert(high.tier.id === "friendly" || high.tier.id === "trusted", "high rep tier");
  assert(high.source.indexOf("acceptDialogueByTier") >= 0, "should use tier table");

  Rep.clearState("adt2");
  Rep.applyNpcDelta("adt2", "mayor", -0.35, "test");
  const refused = runtime.getAcceptDialogue("quest_report_wolf_escape", "mayor", {
    sessionKey: "adt2",
  });
  assert(refused.canAccept === false, "hostile rep should block accept");
  assert(refused.line.indexOf("신뢰") >= 0 || refused.source.indexOf("refused") >= 0, "refused line");

  console.log("accept-dialogue-test: passed");
  console.log("  low:", low.line.slice(0, 40) + "…");
  console.log("  high:", high.line.slice(0, 40) + "…");
}

main();
