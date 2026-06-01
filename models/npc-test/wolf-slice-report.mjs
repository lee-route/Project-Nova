/**
 * 늑대 퀘스트 수직 슬라이스 — 3 의뢰인 경로 비교 리포트 생성
 * Usage: node wolf-slice-report.mjs
 * Output: wolf-slice-report.json
 */
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";
import vm from "vm";

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const QUEST_ID = "quest_report_wolf_escape";
const REPORT_TEXT = "북문에서 늑대 3마리가 탈출했다";
const GIVERS = ["mayor", "merchant", "guard"];

function loadRuntime() {
  const sandbox = {
    window: {},
    document: { querySelector: () => null, querySelectorAll: () => [], getElementById: () => null },
    console,
    localStorage: { getItem: () => null, setItem: () => {}, removeItem: () => {} },
  };
  sandbox.window = sandbox;
  const ctx = vm.createContext(sandbox);
  const read = (f) => fs.readFileSync(path.join(__dirname, f), "utf8");
  for (const f of ["dictionaries.js", "quest-system.js"]) {
    vm.runInContext(read(f), ctx, { filename: f });
  }
  const npcs = JSON.parse(read("npcs.json"));
  const player = JSON.parse(read("player-profile.json"));
  sandbox.window.QuestSystem.registerProfiles({
    npcs: npcs.npcs,
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

function snapInterpreted(item) {
  const tv = item.truth_value;
  const meta = item.metadata || {};
  return {
    subject: tv.subject,
    action: tv.action,
    target: tv.target,
    object: tv.object || "",
    quantity: tv.quantity,
    certainty: Number(Number(tv.certainty).toFixed(3)),
    action_type: tv.action_type,
    applied_rules: meta.applied_rules || [],
  };
}

function main() {
  const { parser, engine, runtime } = loadRuntime();
  const quest = runtime.getQuest(QUEST_ID);
  const parsed = parser.parseScenarioText(REPORT_TEXT);
  const parserFacts = parser.buildFactsFromParsed(parsed);

  const paths = [];
  for (const giverId of GIVERS) {
    const giver = runtime.getQuestGiver(quest, giverId);
    const run = runtime.runQuestTurnIn({
      questId: QUEST_ID,
      giverId,
      scenarioText: REPORT_TEXT,
      engine,
      parser,
    });
    const interpreted = run.engineResult.propagation.interpretedFacts || [];
    paths.push({
      giverId,
      label: giver.label,
      turnInProfileKey: run.turnInProfileKey,
      quantityMode: giver.experience?.propagationOptions?.quantityMode,
      expectedFlavor: giver.experience?.expectedDistortionFlavor,
      rumorSpread: giver.experience?.rumorSpread,
      completed: run.completion.completed,
      outcome: run.outcome,
      blocked: run.engineResult.propagation.blocked,
      partialTrust: run.engineResult.propagation.partialTrust,
      interpretedFacts: interpreted.map(snapInterpreted),
      completionReason: run.completion.reason,
      processStepCount: (giver.processSteps || []).length,
    });
  }

  const outcomesMatch =
    paths.length > 0 &&
    paths.every(
      (p) =>
        p.completed &&
        p.outcome &&
        JSON.stringify(p.outcome.worldFlags) === JSON.stringify(paths[0].outcome.worldFlags) &&
        p.outcome.rewards?.gold === paths[0].outcome?.rewards?.gold
    );

  const report = {
    generatedAt: new Date().toISOString(),
    title: quest.title,
    questId: QUEST_ID,
    playerReportText: REPORT_TEXT,
    parserOutput: {
      parse_mode: parsed.parse_mode,
      parse_confidence: parsed.parse_confidence,
      facts: parserFacts,
    },
    designClaim:
      "동일 플레이어 보고 → 의뢰인별 전파·해석 차이 → factMatch 통과 시 동일 outcome",
    paths,
    summary: {
      allCompleted: paths.every((p) => p.completed),
      outcomesMatch,
      sharedOutcome: paths[0]?.outcome || null,
      quantityByGiver: Object.fromEntries(paths.map((p) => [p.giverId, p.interpretedFacts[0]?.quantity])),
      certaintyByGiver: Object.fromEntries(paths.map((p) => [p.giverId, p.interpretedFacts[0]?.certainty])),
    },
    capstoneChecklist: {
      captureScreenshots: [
        "index.html Quest Playtest — Compare All Givers",
        "파서 Facts 패널 (player input)",
        "interpretedFacts / Engine Data (NPC가 본 정보)",
      ],
      pasteThisJson: "wolf-slice-report.json",
    },
  };

  const outPath = path.join(__dirname, "wolf-slice-report.json");
  fs.writeFileSync(outPath, JSON.stringify(report, null, 2), "utf8");
  console.log("Wrote " + outPath);
  console.log("Summary:", JSON.stringify(report.summary, null, 2));
  for (const p of paths) {
    console.log(
      `  ${p.giverId}: qty=${p.interpretedFacts[0]?.quantity} certainty=${p.interpretedFacts[0]?.certainty} rules=${(p.interpretedFacts[0]?.applied_rules || []).join(",")}`
    );
  }
}

main();
