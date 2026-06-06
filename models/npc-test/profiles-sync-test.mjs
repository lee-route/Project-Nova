/**
 * npcs.json ↔ reputation-config.json ↔ player-profile.json 키·기본값 일치 검증
 * Usage: node profiles-sync-test.mjs
 */
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";

const __dirname = path.dirname(fileURLToPath(import.meta.url));

function readJson(name) {
  return JSON.parse(fs.readFileSync(path.join(__dirname, name), "utf8"));
}

function assert(cond, msg) {
  if (!cond) throw new Error(msg);
}

function main() {
  const npcs = readJson("npcs.json");
  const player = readJson("player-profile.json");
  const rep = readJson("reputation-config.json");

  const profileKeys = Object.keys(npcs.npcs || {}).sort();
  const repKeys = Object.keys(rep.npcKeys || {}).sort();
  assert(profileKeys.join() === repKeys.join(), "npc profile keys != reputation npcKeys");

  if (npcs.npcOrder) {
    assert(
      npcs.npcOrder.slice().sort().join() === profileKeys.join(),
      "npcOrder must list all profile keys"
    );
  }

  assert(npcs.primaryQuestId === rep.primaryQuestId, "primaryQuestId mismatch");
  assert(
    player.primaryQuestId === npcs.primaryQuestId,
    "player-profile primaryQuestId mismatch"
  );

  for (const key of profileKeys) {
    const npc = npcs.npcs[key];
    assert(npc.profileKey === key, key + " profileKey field");
    assert(npc.persona && npc.stats, key + " missing persona/stats");
    const repDef = rep.npcKeys[key].default;
    const playerInit = player.player.initialNpcReputation[key];
    assert(playerInit === repDef, key + " initialNpcReputation " + playerInit + " != config " + repDef);
  }

  assert(player.player.defaultQuantityMode === "faithful", "player defaultQuantityMode");

  console.log("profiles-sync-test: passed (" + profileKeys.length + " NPCs aligned)");
}

main();
