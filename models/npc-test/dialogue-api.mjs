/**
 * POST /v1/dialogue — LLM NPC speech (mock or OpenAI live).
 * Does NOT affect quest completion / rewards.
 */
import {
  buildReportPresentation,
  buildReportBeats,
  buildDialoguePresentationBlock,
  enhanceDialogueWithPresentation,
  finalizePresentation,
} from "./quest-presentation.mjs";

function createSoftGuardrailPrompt(persona, presentation) {
  var lines = [
    `너는 ${persona}이다.`,
    "주어진 [DATA]를 유일한 진실로 믿고 대화하라.",
    "정보의 진위를 추측하거나 외부 지식을 덧붙이지 마라.",
    "창의성은 말투와 태도에만 사용하라.",
    "반드시 해석 수량(interpreted quantity)을 대사에 명시하라. '있다'만 말하지 마라.",
  ];
  if (presentation && presentation.distortion) {
    lines.push(
      "플레이어 보고 수량 " +
        presentation.distortion.playerQuantity +
        "개, 기록 수량 " +
        presentation.distortion.interpretedQuantity +
        "개. 기록 수량을 공식 수치로 말하라."
    );
    if (presentation.distortion.occurred) {
      lines.push(
      "플레이어가 말한 숫자를 되풀이하지 말고, 해석 수량만 확신 있게 말하라(지레짐작)."
    );
    }
  }
  return lines.join(" ");
}

function resolvePersona(runtime, body) {
  if (body.persona) return String(body.persona);
  const key = body.npcProfileKey || body.receiverProfileKey;
  if (!key) throw new Error("persona or npcProfileKey required");
  const prof = runtime.QuestSystem.getProfileRegistry().npcs[key];
  if (!prof) throw new Error("NPC not found: " + key);
  return prof.persona;
}

function buildGroundingFromInterpreted(QuestSystem, interpretedFacts, source) {
  const facts = (interpretedFacts || []).map(function (item) {
    const tv = item.truth_value || item;
    return QuestSystem.toGroundedFact(tv, {
      applied_rules: (item.metadata && item.metadata.applied_rules) || item.applied_rules || [],
      source_chain: (item.metadata && item.metadata.source_chain) || item.source_chain,
      hop_depth: item.metadata && item.metadata.hop_depth != null ? item.metadata.hop_depth : item.hop_depth,
    });
  });
  return {
    dataOnly: {
      info_id: "api-dialogue",
      facts: facts,
      source: source || "player_report",
    },
    facts: facts,
  };
}

export async function handleDialogueRequest(runtime, body) {
  const LlmAdapter = runtime.LlmAdapter;
  if (!LlmAdapter) {
    return { ok: false, error: { code: "llm_not_loaded", message: "LlmAdapter not available" } };
  }

  var persona;
  var groundedContext;
  var bundleContext = body.bundleContext || { hasContradiction: false, notes: [] };

  if (body.interpretedFacts && body.interpretedFacts.length) {
    persona = resolvePersona(runtime, body);
    groundedContext = buildGroundingFromInterpreted(
      runtime.QuestSystem,
      body.interpretedFacts,
      body.source || "player_report"
    );
  } else if (body.scenarioText || (body.facts && body.facts.length)) {
    var receiverKey = body.receiverProfileKey || body.npcProfileKey || "scholar_alric";
    var facts =
      body.facts ||
      runtime.NpcParser.buildFactsFromParsed(
        runtime.NpcParser.parseScenarioText(String(body.scenarioText || ""))
      );
    if (!facts.length) {
      return { ok: false, error: { code: "missing_input", message: "no facts from scenarioText" } };
    }
    var result = runtime.QuestSystem.executeScenario({
      facts: facts,
      usePlayerAsSender: true,
      receiverProfileKey: receiverKey,
      recordPlayerKnowledge: false,
      persistSession: false,
      sessionKey: body.sessionKey || "dialogue-api",
      quantityMode: body.quantityMode,
      allowPartialTrust: body.allowPartialTrust,
    });
    if (result.propagation && result.propagation.blocked) {
      return {
        ok: false,
        error: {
          code: "propagation_blocked",
          message: result.propagation.reason || "propagation blocked",
        },
      };
    }
    persona = result.receiver && result.receiver.persona;
    if (!result.dialogue || !result.dialogue.context) {
      return { ok: false, error: { code: "dialogue_unavailable", message: "no dialogue context" } };
    }
    groundedContext = result.dialogue.context;
    bundleContext =
      (result.dialogue.context && result.dialogue.context.bundleContext) ||
      result.bundleContext ||
      bundleContext;
  } else {
    return {
      ok: false,
      error: {
        code: "missing_input",
        message: "interpretedFacts[] or scenarioText/facts[] required",
      },
    };
  }

  var presentation = body.presentation || null;
  var systemPrompt = createSoftGuardrailPrompt(persona, presentation);
  var out = await LlmAdapter.generateAsync({
    systemPrompt: systemPrompt,
    groundedContext: groundedContext,
    persona: persona,
    bundleContext: bundleContext,
    presentationContext: presentation,
    strictAnchor: body.strictAnchor !== false,
  });

  var npcSpeech = out.npcSpeech;
  if (presentation) {
    npcSpeech = enhanceDialogueWithPresentation(npcSpeech, presentation);
  }

  var result = {
    ok: true,
    npcSpeech: npcSpeech,
    provider: out.provider,
    persona: persona,
    anchorValidation: out.anchorValidation,
    fallbackStyle: out.fallbackStyle || null,
    questJudgmentExcluded: true,
  };
  if (presentation) {
    result.presentation = buildDialoguePresentationBlock(presentation, npcSpeech, out.provider);
  }
  return result;
}

/** Turn-in 완료 응답에 붙일 LLM 대사 (판정·보상과 분리). */
export async function dialogueFromTurnIn(runtime, turnIn, scenarioText) {
  if (!turnIn || !turnIn.completion || !turnIn.completion.completed) {
    return null;
  }
  if (turnIn.propagation && turnIn.propagation.blocked) {
    return null;
  }
  var interpreted = (turnIn.propagation && turnIn.propagation.interpretedFacts) || [];
  if (!interpreted.length) {
    return null;
  }
  var npcProfileKey = turnIn.turnInProfileKey || "scholar_alric";
  var presentation = finalizePresentation(buildReportPresentation(turnIn, scenarioText));
  var result = await handleDialogueRequest(runtime, {
    interpretedFacts: interpreted,
    npcProfileKey: npcProfileKey,
    presentation: presentation,
  });
  if (!result.ok) {
    return {
      ok: false,
      npcProfileKey: npcProfileKey,
      error: result.error,
      reportPresentation: presentation,
      questJudgmentExcluded: true,
    };
  }
  presentation.reportBeats = buildReportBeats(presentation, result.npcSpeech);
  return {
    ok: true,
    npcProfileKey: npcProfileKey,
    npcSpeech: result.npcSpeech,
    provider: result.provider,
    persona: result.persona,
    anchorValidation: result.anchorValidation,
    fallbackStyle: result.fallbackStyle || null,
    reportPresentation: presentation,
    presentation: result.presentation || buildDialoguePresentationBlock(presentation, result.npcSpeech, result.provider),
    questJudgmentExcluded: true,
  };
}
