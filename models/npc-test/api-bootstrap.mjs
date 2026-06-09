/**
 * Node API server / api-server-test runtime loader (no DOM, no sync XHR).
 */
import fs from "fs";
import path from "path";
import vm from "vm";
import { fileURLToPath } from "url";
import { loadLlmServerConfig, llmConfigSummary } from "./llm-server-config.mjs";

const __dirname = path.dirname(fileURLToPath(import.meta.url));

export function createApiRuntime(rootDir) {
  const root = rootDir || __dirname;
  const sandbox = {
    window: {},
    document: { querySelector: () => null, querySelectorAll: () => [], getElementById: () => null },
    console,
    localStorage: { getItem: () => null, setItem: () => {}, removeItem: () => {} },
    fetch: globalThis.fetch,
    setTimeout: globalThis.setTimeout.bind(globalThis),
    clearTimeout: globalThis.clearTimeout.bind(globalThis),
  };
  sandbox.window = sandbox;
  const ctx = vm.createContext(sandbox);
  const read = (f) => fs.readFileSync(path.join(root, f), "utf8");
  const scripts = [
    "game-clock.js",
    "knowledge-layers.js",
    "deception-audit.js",
    "session-snapshot.js",
    "reputation-system.js",
    "quest-game-state.js",
    "dictionaries.js",
    "npc-parser.js",
    "quest-system.js",
    "quest-presentation.js",
    "quest-runtime.js",
    "llm-fact-anchor.js",
    "llm-mock-fallback.js",
    "llm-adapter.js",
  ];
  for (const f of scripts) {
    vm.runInContext(read(f), ctx, { filename: f });
  }

  const llmCfg = loadLlmServerConfig(root);
  sandbox.window.LlmAdapter.configure(llmCfg);

  const npcs = JSON.parse(read("npcs.json"));
  const player = JSON.parse(read("player-profile.json"));
  const repCfg = JSON.parse(read("reputation-config.json"));
  const quests = JSON.parse(read("quests-draft.json"));

  sandbox.window.QuestSystem.registerProfiles({ npcs: npcs.npcs, player: player.player });
  sandbox.window.ReputationSystem.setConfig(repCfg);
  sandbox.window.QuestRuntime.setQuestCatalog(quests);

  return {
    root,
    QuestSystem: sandbox.window.QuestSystem,
    NpcParser: sandbox.window.NpcParser,
    QuestRuntime: sandbox.window.QuestRuntime,
    QuestGameState: sandbox.window.QuestGameState,
    ReputationSystem: sandbox.window.ReputationSystem,
    GameClock: sandbox.window.GameClock,
    WorldTruth: sandbox.window.WorldTruth,
    PlayerKnowledge: sandbox.window.PlayerKnowledge,
    DeceptionAudit: sandbox.window.DeceptionAudit,
    SessionSnapshot: sandbox.window.SessionSnapshot,
    LlmAdapter: sandbox.window.LlmAdapter,
    llmConfig: llmConfigSummary(llmCfg),
  };
}
