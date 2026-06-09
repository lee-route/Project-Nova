/**
 * Usage: node quest-presentation-test.mjs
 */
import { createApiRuntime } from "./api-bootstrap.mjs";
import {
  buildReportPresentation,
  buildGiverAcceptPresentation,
  buildAlricPresumedSpeech,
  finalizePresentation,
} from "./quest-presentation.mjs";

const rt = createApiRuntime();
let failed = 0;

function assert(cond, msg) {
  if (!cond) {
    console.error("FAIL:", msg);
    failed += 1;
  } else {
    console.log("ok:", msg);
  }
}

const quest = rt.QuestRuntime.getQuest("quest_abandoned_cargo");
const guardGiver = quest.questGivers.find(function (g) {
  return g.giverId === "guard_timid";
});
const guardAccept = buildGiverAcceptPresentation(quest, guardGiver);
assert(guardAccept.presumedQuantity === 15, "guard accept presumes 15");
assert(guardAccept.rumorBaselineQuantity === 12, "rumor baseline 12");
assert(guardAccept.presumedSpeech.indexOf("15") >= 0, "guard accept speech has 15");
assert(guardAccept.acceptBeats[0].phase === "giver_presume", "accept beat presume");

const scholarGiver = quest.questGivers.find(function (g) {
  return g.giverId === "scholar_alric";
});
const scholarAccept = buildGiverAcceptPresentation(quest, scholarGiver);
assert(scholarAccept.presumedQuantity === 12, "scholar accept faithful 12");
assert(scholarAccept.distortedAtAccept === false, "scholar no accept distortion");

const guardTurnIn = rt.QuestRuntime.runQuestTurnIn({
  engine: rt.QuestSystem,
  parser: rt.NpcParser,
  questId: "quest_abandoned_cargo",
  giverId: "guard_timid",
  scenarioText: "안개 계곡에 밀수 화물이 버려져 있다. 마약초 열두 개가 들어 있다",
  sessionKey: "pres-guard",
  applyGameState: false,
});

const guardPres = finalizePresentation(
  buildReportPresentation(guardTurnIn, "안개 계곡에 밀수 화물이 버려져 있다. 마약초 열두 개가 들어 있다")
);
assert(guardPres.playerReport.quantity === 12, "guard player qty 12");
assert(guardPres.interpretedRecord.quantity === 15, "guard interpreted qty 15");
assert(guardPres.distortion.occurred === true, "guard distortion occurred");
assert(guardPres.distortion.summary.indexOf("12") >= 0 && guardPres.distortion.summary.indexOf("15") >= 0, "guard summary");
assert(guardPres.playerReport.line.indexOf("[플레이어]") === 0, "player line prefix");
assert(guardPres.interpretedRecord.presumedLine.indexOf("15") >= 0, "presumed line has 15");

const presumed = buildAlricPresumedSpeech(guardPres);
assert(presumed.indexOf("15") >= 0, "alric presumed leads with interpreted 15");
assert(presumed.indexOf("12") < 0, "alric presumed does not repeat player 12 (지레짐작)");
assert(guardPres.reportBeats[1].phase === "alric_presume", "beat 2 is alric presume");
assert(guardPres.reportBeats[1].quantity === 15, "presume beat quantity");

const scholarTurnIn = rt.QuestRuntime.runQuestTurnIn({
  engine: rt.QuestSystem,
  parser: rt.NpcParser,
  questId: "quest_abandoned_cargo",
  giverId: "scholar_alric",
  scenarioText: "안개 계곡에 밀수 화물이 버려져 있다. 마약초 다섯 개가 들어 있다",
  sessionKey: "pres-scholar",
  applyGameState: false,
});

const scholarPres = finalizePresentation(
  buildReportPresentation(scholarTurnIn, "안개 계곡에 밀수 화물이 버려져 있다. 마약초 다섯 개가 들어 있다")
);
assert(scholarPres.playerReport.quantity === 5, "scholar player qty 5");
assert(scholarPres.interpretedRecord.quantity === 5, "scholar interpreted qty 5");
assert(scholarPres.distortion.occurred === false, "scholar no distortion");
assert(scholarTurnIn.outcomeBranch && scholarTurnIn.outcomeBranch.branchId === "black_market_path", "scholar 5 -> black market");

console.log(failed ? "FAILED" : "quest-presentation-test passed");
process.exit(failed ? 1 : 0);
