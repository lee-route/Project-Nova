/**
 * API handler tests (no live port required).
 */
import http from "http";
import { createApiRuntime } from "./api-bootstrap.mjs";
import { handleApiRequest } from "./npc-api-server.mjs";

const rt = createApiRuntime();

function mockReq(method, path, body) {
  const payload = body ? JSON.stringify(body) : "";
  return {
    method: method,
    url: path,
    headers: { "content-type": "application/json" },
    on: function (ev, fn) {
      if (ev === "data" && payload) fn(Buffer.from(payload));
      if (ev === "end") fn();
    },
  };
}

function mockRes() {
  const out = { status: 0, body: "" };
  return {
    writeHead: function (code) {
      out.status = code;
    },
    end: function (text) {
      out.body = text;
    },
    out: out,
  };
}

async function call(method, path, body) {
  const res = mockRes();
  await handleApiRequest(mockReq(method, path, body), res, rt);
  return { status: res.out.status, json: JSON.parse(res.out.body) };
}

let failed = 0;
function assert(cond, msg) {
  if (!cond) {
    console.error("FAIL:", msg);
    failed += 1;
  } else {
    console.log("ok:", msg);
  }
}

const health = await call("GET", "/health");
assert(health.json.ok, "health ok");
assert(Array.isArray(health.json.v1Endpoints), "health endpoints");
assert(typeof health.json.llmEnabled === "boolean", "health llmEnabled flag");

const meta = await call("GET", "/v1/quest/meta");
assert(meta.json.ok && meta.json.givers.length >= 3, "quest meta");

const parse = await call("POST", "/v1/parse", {
  scenarioText: "안개 계곡에 밀수 화물이 버려져 있다. 마약초 열두 개가 들어 있다",
});
assert(parse.json.ok && parse.json.facts.length >= 2, "parse facts");

const turnIn = await call("POST", "/v1/quest/turn-in", {
  questId: "quest_abandoned_cargo",
  giverId: "guard_timid",
  scenarioText: "안개 계곡에 밀수 화물이 버려져 있다. 마약초 열두 개가 들어 있다",
  sessionKey: "api-test-session",
  worldTruthFacts: [
    { subject: "마약초", quantity: 12, is_countable: true, action_type: "inventory", target: "안개 계곡" },
  ],
});
assert(turnIn.json.ok, "turn-in ok");
assert(turnIn.json.outcome && turnIn.json.outcome.rewards, "turn-in outcome");
assert(turnIn.json.propagation && !turnIn.json.propagation.blocked, "propagation ok");
assert(turnIn.json.knowledgeAudit && turnIn.json.knowledgeAudit.v1Policy === "diagnostic_only", "knowledge audit");
assert(turnIn.json.gameState && turnIn.json.gameState.gold >= 0, "game state applied");
assert(turnIn.json.llmExcluded === false, "dialogue included by default on completion");
assert(turnIn.json.dialogue && turnIn.json.dialogue.ok && turnIn.json.dialogue.npcSpeech, "turn-in dialogue npcSpeech");
assert(turnIn.json.dialogue.provider, "turn-in dialogue provider");

const turnInNoLlm = await call("POST", "/v1/quest/turn-in", {
  questId: "quest_abandoned_cargo",
  giverId: "guard_timid",
  scenarioText: "안개 계곡에 밀수 화물이 버려져 있다. 마약초 열두 개가 들어 있다",
  sessionKey: "api-test-no-llm",
  includeDialogue: false,
});
assert(turnInNoLlm.json.ok && turnInNoLlm.json.llmExcluded === true, "includeDialogue false skips llm");
assert(turnInNoLlm.json.dialogue == null, "no dialogue object when skipped");

const turnInFail = await call("POST", "/v1/quest/turn-in", {
  questId: "quest_abandoned_cargo",
  giverId: "guard_timid",
  scenarioText: "12개 확인함",
  sessionKey: "api-test-fail",
});
assert(turnInFail.json.ok && turnInFail.json.completion.completed === false, "failed turn-in");
assert(turnInFail.json.dialogue == null, "no dialogue on failed turn-in");

const bad = await call("POST", "/v1/quest/turn-in", { questId: "quest_abandoned_cargo" });
assert(bad.json.ok === false && bad.json.error.code, "error shape");

const snap = await call("GET", "/v1/session/snapshot?sessionKey=api-test-session");
assert(snap.json.ok && snap.json.snapshot.authoritativeSave === "unreal_engine", "session snapshot");

const dialogue = await call("POST", "/v1/dialogue", {
  scenarioText: "안개 계곡에 밀수 화물이 버려져 있다. 마약초 열두 개가 들어 있다",
  receiverProfileKey: "scholar_alric",
});
assert(dialogue.json.ok && dialogue.json.npcSpeech, "dialogue mock npcSpeech");
assert(dialogue.json.questJudgmentExcluded === true, "dialogue excludes quest judgment");

const llmStatus = await call("GET", "/v1/llm/status");
assert(llmStatus.json.ok && typeof llmStatus.json.llmEnabled === "boolean", "llm status");

console.log(failed ? "FAILED" : "api-server-test passed");
process.exit(failed ? 1 : 0);
