/**
 * Node 테스트용 엔진 샌드박스 로드
 */
import fs from "fs";
import path from "path";
import vm from "vm";

export function loadTestEngine(dirname, extraFiles = []) {
  const sandbox = {
    window: {},
    document: { querySelector: () => null, querySelectorAll: () => [], getElementById: () => null },
    console,
    localStorage: { getItem: () => null, setItem: () => {}, removeItem: () => {} },
  };
  sandbox.window = sandbox;
  const ctx = vm.createContext(sandbox);
  const read = (f) => fs.readFileSync(path.join(dirname, f), "utf8");
  const core = [
    "game-clock.js",
    "knowledge-layers.js",
    "llm-fact-anchor.js",
    "llm-adapter.js",
    "dictionaries.js",
    "quest-system.js",
    ...extraFiles,
  ];
  for (const f of core) {
    vm.runInContext(read(f), ctx, { filename: f });
  }
  const player = JSON.parse(read("player-profile.json"));
  sandbox.window.QuestSystem.registerProfiles({
    npcs: JSON.parse(read("npcs.json")).npcs,
    player: player.player,
  });
  return {
    engine: sandbox.window.QuestSystem,
    GameClock: sandbox.window.GameClock,
    WorldTruth: sandbox.window.WorldTruth,
    PlayerKnowledge: sandbox.window.PlayerKnowledge,
    LlmFactAnchor: sandbox.window.LlmFactAnchor,
    LlmAdapter: sandbox.window.LlmAdapter,
    parser: sandbox.window.NpcParser,
  };
}
