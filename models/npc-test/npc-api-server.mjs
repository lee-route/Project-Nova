/**
 * Phase A HTTP API — v1: parse + turn-in + accept-dialogue + LLM dialogue.
 *
 * Usage: node npc-api-server.mjs [--port=8787]
 * OpenAI: see llm-config.local.json or OPENAI_API_KEY + NOVA_LLM_USE_LIVE=true
 */
import http from "http";
import { createApiRuntime } from "./api-bootstrap.mjs";
import { dialogueFromTurnIn, handleDialogueRequest } from "./dialogue-api.mjs";
import { buildReportPresentation, finalizePresentation } from "./quest-presentation.mjs";

const rt = createApiRuntime();

export function apiError(code, message, details) {
  return {
    ok: false,
    error: { code: code, message: message, details: details || null },
  };
}

export function slimTurnInResponse(turnIn, dialogue, scenarioText) {
  if (!turnIn) return null;
  var hasDialogue = Boolean(dialogue && dialogue.ok && dialogue.npcSpeech);
  var reportPresentation =
    turnIn.completion && turnIn.completion.completed
      ? dialogue && dialogue.reportPresentation
        ? dialogue.reportPresentation
        : finalizePresentation(buildReportPresentation(turnIn, scenarioText))
      : null;
  return {
    ok: true,
    questId: turnIn.questId,
    giverId: turnIn.giverId,
    turnInProfileKey: turnIn.turnInProfileKey,
    sessionKey: turnIn.sessionKey,
    scenarioText: scenarioText != null ? String(scenarioText) : "",
    reportPresentation: reportPresentation,
    facts: turnIn.facts,
    propagation: turnIn.propagation,
    completion: turnIn.completion,
    outcomeBranch: turnIn.outcomeBranch
      ? {
          branchId: turnIn.outcomeBranch.branchId,
          metrics: turnIn.outcomeBranch.metrics,
        }
      : null,
    outcome: turnIn.outcome,
    reputationResult: turnIn.reputationResult,
    knowledgeAudit: turnIn.knowledgeAudit,
    gameState: turnIn.gameStateApply && turnIn.gameStateApply.state,
    v1OutcomeSource: turnIn.v1OutcomeSource,
    llmExcluded: !hasDialogue,
    dialogue: dialogue,
  };
}

export function shouldIncludeDialogue(body, turnIn) {
  if (body.includeDialogue === false) return false;
  return Boolean(turnIn && turnIn.completion && turnIn.completion.completed);
}

export async function handleApiRequest(req, res, runtime) {
  const R = runtime || rt;
  const url = new URL(req.url || "/", "http://localhost");
  const path = url.pathname.replace(/\/+$/, "") || "/";

  let body = {};
  if (req.method === "POST" || req.method === "PUT") {
    try {
      body = await readJsonBody(req);
    } catch (e) {
      sendJson(res, 400, apiError("invalid_json", String(e.message || e)));
      return;
    }
  }

  if (req.method === "GET" && path === "/health") {
    sendJson(res, 200, {
      ok: true,
      service: "project-nova-npc-api",
      version: 2,
      primaryQuestId: "quest_abandoned_cargo",
      v1Endpoints: [
        "/v1/quest/meta",
        "/v1/parse",
        "/v1/quest/turn-in",
        "/v1/quest/accept-dialogue",
        "/v1/dialogue",
        "/v1/llm/status",
        "/v1/session/snapshot",
      ],
      llmEnabled: Boolean(R.LlmAdapter && R.LlmAdapter.isLive && R.LlmAdapter.isLive()),
      llmConfig: R.llmConfig || null,
      authoritativeSave: "unreal_engine",
    });
    return;
  }

  if (req.method === "GET" && path === "/v1/quest/meta") {
    const questId = url.searchParams.get("questId") || "quest_abandoned_cargo";
    const catalog = R.QuestRuntime.getQuestCatalog();
    const quest = catalog && catalog.quests ? catalog.quests.find(function (q) { return q.id === questId; }) : null;
    const givers = R.QuestRuntime.listQuestGiverOptions(questId);
    sendJson(res, 200, {
      ok: true,
      primaryQuestId: "quest_abandoned_cargo",
      questId: questId,
      title: quest ? quest.title : null,
      playerInputHintFinal: quest ? quest.playerInputHintFinal : null,
      givers: givers,
      turnInNote:
        "giverId = 의뢰인(questGivers[].giverId). 보고 수신 NPC는 turnInProfileKey(보통 scholar_alric).",
      exampleReports: {
        highQuantity: "안개 계곡에 밀수 화물이 버려져 있다. 마약초 열두 개가 들어 있다",
        lowQuantity: "안개 계곡에 밀수 화물이 버려져 있다. 마약초 다섯 개가 들어 있다",
      },
    });
    return;
  }

  if (req.method === "POST" && path === "/v1/parse") {
    const text = body.scenarioText != null ? String(body.scenarioText) : "";
    const parsed = R.NpcParser.parseScenarioText(text);
    const facts = R.NpcParser.buildFactsFromParsed(parsed);
    const primary = R.NpcParser.selectPrimaryParse(parsed) || parsed;
    sendJson(res, 200, { ok: true, parsed: parsed, facts: facts, primary: primary });
    return;
  }

  if (req.method === "POST" && path === "/v1/quest/turn-in") {
    const questId = body.questId || "quest_abandoned_cargo";
    const giverId = body.giverId;
    const scenarioText = body.scenarioText;
    if (!giverId) {
      sendJson(res, 400, apiError("missing_giver_id", "giverId required"));
      return;
    }
    if (!scenarioText && !body.facts) {
      sendJson(res, 400, apiError("missing_report", "scenarioText or facts[] required"));
      return;
    }
    try {
      const turnIn = R.QuestRuntime.runQuestTurnIn({
        engine: R.QuestSystem,
        parser: R.NpcParser,
        questId: questId,
        giverId: giverId,
        scenarioText: scenarioText,
        facts: body.facts,
        sessionKey: body.sessionKey || "api-default",
        reputationSessionKey: body.reputationSessionKey || body.sessionKey,
        quantityMode: body.quantityMode,
        allowPartialTrust: body.allowPartialTrust,
        persistSession: Boolean(body.persistSession),
        worldTruthFacts: body.worldTruthFacts || body.investigationFacts,
        applyGameState: body.applyGameState !== false,
        includeKnowledgeAudit: body.includeKnowledgeAudit !== false,
      });
      var dialogue = null;
      if (shouldIncludeDialogue(body, turnIn)) {
        dialogue = await dialogueFromTurnIn(R, turnIn, scenarioText);
      }
      const payload = slimTurnInResponse(turnIn, dialogue, scenarioText);
      sendJson(res, 200, payload);
    } catch (e) {
      sendJson(res, 400, apiError("turn_in_failed", String(e.message || e)));
    }
    return;
  }

  if (req.method === "GET" && path === "/v1/quest/accept-dialogue") {
    const questId = url.searchParams.get("questId") || "quest_abandoned_cargo";
    const giverId = url.searchParams.get("giverId");
    const sessionKey = url.searchParams.get("sessionKey") || "api-default";
    if (!giverId) {
      sendJson(res, 400, apiError("missing_giver_id", "giverId query required"));
      return;
    }
    try {
      const accept = R.QuestRuntime.getAcceptDialogue(questId, giverId, { sessionKey: sessionKey });
      sendJson(res, 200, { ok: true, accept: accept });
    } catch (e) {
      sendJson(res, 400, apiError("accept_dialogue_failed", String(e.message || e)));
    }
    return;
  }

  if (req.method === "GET" && path === "/v1/session/snapshot") {
    const sessionKey = url.searchParams.get("sessionKey") || "api-default";
    const snap = R.SessionSnapshot.exportSession(sessionKey);
    sendJson(res, 200, { ok: true, snapshot: snap });
    return;
  }

  if (req.method === "POST" && path === "/v1/session/import") {
    const sessionKey = body.sessionKey || "api-default";
    const snap = R.SessionSnapshot.importSession(sessionKey, body.snapshot || body);
    sendJson(res, 200, { ok: true, snapshot: snap });
    return;
  }

  if (req.method === "GET" && path === "/v1/llm/status") {
    const cfg = R.LlmAdapter ? R.LlmAdapter.getConfig() : null;
    sendJson(res, 200, {
      ok: true,
      llmEnabled: Boolean(R.LlmAdapter && R.LlmAdapter.isLive && R.LlmAdapter.isLive()),
      config: cfg,
      questJudgmentExcluded: true,
    });
    return;
  }

  if (req.method === "POST" && path === "/v1/dialogue") {
    try {
      const payload = await handleDialogueRequest(R, body);
      if (!payload.ok) {
        sendJson(res, 400, apiError(payload.error.code, payload.error.message));
        return;
      }
      sendJson(res, 200, payload);
    } catch (e) {
      sendJson(res, 500, apiError("dialogue_failed", String(e.message || e)));
    }
    return;
  }

  if (path.startsWith("/v1/llm") && path !== "/v1/llm/status") {
    sendJson(res, 404, apiError("not_found", "Unknown LLM path: " + path));
    return;
  }

  sendJson(res, 404, apiError("not_found", "Unknown path: " + path));
}

function readJsonBody(req) {
  return new Promise(function (resolve, reject) {
    var buf = "";
    req.on("data", function (chunk) {
      buf += chunk;
      if (buf.length > 2e6) reject(new Error("body too large"));
    });
    req.on("end", function () {
      if (!buf) return resolve({});
      try {
        resolve(JSON.parse(buf));
      } catch (e) {
        reject(e);
      }
    });
  });
}

function sendJson(res, status, obj) {
  var text = JSON.stringify(obj);
  res.writeHead(status, {
    "Content-Type": "application/json; charset=utf-8",
    "Access-Control-Allow-Origin": "*",
  });
  res.end(text);
}

function logRequest(req, path, status) {
  var line = "[" + new Date().toISOString() + "] " + req.method + " " + path + " → " + status;
  console.log(line);
}

function main() {
  var port = 8787;
  var host = "127.0.0.1";
  process.argv.slice(2).forEach(function (arg) {
    if (arg.startsWith("--port=")) port = Number(arg.split("=")[1]) || 8787;
    if (arg.startsWith("--host=")) host = arg.split("=")[1] || "127.0.0.1";
  });
  var server = http.createServer(function (req, res) {
    var pathOnly = (req.url || "/").split("?")[0];
    var origEnd = res.end;
    res.end = function (body) {
      logRequest(req, pathOnly, res.statusCode || 200);
      return origEnd.call(res, body);
    };
    if (req.method === "OPTIONS") {
      res.writeHead(204, {
        "Access-Control-Allow-Origin": "*",
        "Access-Control-Allow-Methods": "GET,POST,OPTIONS",
        "Access-Control-Allow-Headers": "Content-Type",
      });
      res.end();
      return;
    }
    handleApiRequest(req, res, rt).catch(function (e) {
      sendJson(res, 500, apiError("internal", String(e.message || e)));
    });
  });
  server.listen(port, host, function () {
    console.log("npc-api listening on http://" + host + ":" + port);
    console.log("  GET  /health");
    console.log("  GET  /v1/quest/meta");
    console.log("  POST /v1/parse");
    console.log("  POST /v1/quest/turn-in");
    console.log("  GET  /v1/quest/accept-dialogue?giverId=guard_timid&sessionKey=...");
    console.log("  POST /v1/dialogue");
    console.log("  GET  /v1/llm/status");
    if (rt.llmConfig) {
      console.log("  LLM:", rt.llmConfig.provider, rt.llmConfig.useLive ? "(live)" : "(mock)");
    }
    console.log("");
    console.log("Quick test: npm run api:smoke");
  });
}

if (process.argv[1] && process.argv[1].endsWith("npc-api-server.mjs")) {
  main();
}
