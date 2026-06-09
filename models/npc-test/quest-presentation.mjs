/**
 * Turn-in 연출: 플레이어 보고 직후 알릭이 해석 수량을 지레짐작 → 이후 결과.
 * UE는 reportBeats 순서대로 재생 (마지막에만 수량 넣지 말 것).
 */

const GIVER_LABELS = {
  guard_timid: "소심한 경비",
  merchant_greedy: "탐욕한 상인",
  scholar_alric: "학자 알릭",
};

const GIVER_SPEAKER_TAG = {
  guard_timid: "소심한 경비",
  merchant_greedy: "탐욕한 상인",
  scholar_alric: "학자 알릭",
};

const RULE_LABELS = {
  fear_qty_amp: "공포·수량 과장",
  credulity_qty_amp: "의뢰 경로 신뢰 증폭",
  faithful_qty_preserve: "수량 충실 유지",
  rumor_source_unclear: "출처 불명 소문",
};

function primaryCountableQuantity(facts, subjectPrefer) {
  if (!facts || !facts.length) return { quantity: 0, subject: "", found: false };
  var prefer = String(subjectPrefer || "마약");
  var best = { quantity: 0, subject: "", found: false };
  for (var i = 0; i < facts.length; i += 1) {
    var f = facts[i];
    var sub = String(f.subject || "");
    var qty = Number(f.quantity);
    if (!Number.isFinite(qty)) qty = 0;
    var countable = f.is_countable !== false && qty > 0;
    if (!countable) continue;
    if (sub.indexOf(prefer) >= 0 || prefer === "") {
      if (qty >= best.quantity) best = { quantity: qty, subject: sub || prefer, found: true };
    } else if (!best.found && qty > best.quantity) {
      best = { quantity: qty, subject: sub, found: true };
    }
  }
  return best;
}

function quantityFromInterpreted(interpretedFacts) {
  return primaryCountableQuantity(
    (interpretedFacts || []).map(function (item) {
      var tv = item.truth_value || item;
      return { subject: tv.subject, quantity: tv.quantity, is_countable: tv.is_countable !== false };
    }),
    "마약"
  );
}

function quantityFromPlayerFacts(facts) {
  return primaryCountableQuantity(facts, "마약");
}

function collectAppliedRules(interpretedFacts) {
  var set = {};
  (interpretedFacts || []).forEach(function (item) {
    var rules = (item.metadata && item.metadata.applied_rules) || item.applied_rules || [];
    rules.forEach(function (r) {
      if (r) set[r] = true;
    });
  });
  return Object.keys(set);
}

function giverBriefingLine(processSteps) {
  var steps = processSteps || [];
  for (var i = 0; i < steps.length; i += 1) {
    if (steps[i].type === "dialogue" && steps[i].npcLine) return String(steps[i].npcLine);
  }
  return "";
}

function ruleLabelsKo(rules) {
  return (rules || []).map(function (r) {
    return RULE_LABELS[r] || r;
  });
}

function buildDistortionSummary(giverLabel, quantityMode, playerQty, interpretedQty, rules) {
  var distorted = playerQty !== interpretedQty;
  if (!distorted) {
    return giverLabel + " (" + quantityMode + ") → " + interpretedQty + "개 유지";
  }
  var dir = interpretedQty > playerQty ? "증폭" : "축소";
  var ruleText = ruleLabelsKo(rules).join(", ");
  return (
    giverLabel +
    " (" +
    quantityMode +
    ") → " +
    interpretedQty +
    "개로 " +
    dir +
    " (보고 " +
    playerQty +
    " → 해석 " +
    interpretedQty +
    ")" +
    (ruleText ? " · " + ruleText : "")
  );
}

function buildPlayerLine(scenarioText) {
  var t = String(scenarioText || "").trim();
  return t ? "[플레이어] " + t : "";
}

function certaintyFromInterpreted(interpretedFacts) {
  var max = 0;
  (interpretedFacts || []).forEach(function (item) {
    var tv = item.truth_value || item;
    var c = Number(tv.certainty);
    if (Number.isFinite(c) && c > max) max = c;
  });
  return max;
}

function targetFromInterpreted(interpretedFacts) {
  for (var i = 0; i < (interpretedFacts || []).length; i += 1) {
    var tv = interpretedFacts[i].truth_value || interpretedFacts[i];
    if (tv.target) return String(tv.target);
  }
  return "현장";
}

/**
 * 알릭이 보고 직후 지레짐작하는 대사 — 해석 수량을 먼저·확신 있게 말함.
 * 플레이어가 말한 숫자와의 대조는 UI(reportBeats)로, 여기선 알릭 머릿속 숫자만.
 */
/** 소문·현장 기준 수량 (조사 전 의뢰인이 듣는 루머). */
export function rumorBaselineQuantity(quest) {
  if (!quest) return 12;
  if (quest.rumorBaselineQuantity != null) return Number(quest.rumorBaselineQuantity);
  if (quest.expectedParseExample && quest.expectedParseExample.quantity != null) {
    return Number(quest.expectedParseExample.quantity);
  }
  return 12;
}

/** dramatic 의뢰인 브리핑 증폭 — turn-in 12→15와 맞춤. */
export function giverPresumedQuantity(baseline, quantityMode) {
  var base = Number(baseline) || 12;
  if (quantityMode === "faithful") return base;
  var amp = Math.round(base * 1.25);
  return amp > base ? amp : base + 3;
}

export function buildGiverPresumedSpeech(giverId, presumedQty, baselineQty, quantityMode) {
  var tag = GIVER_SPEAKER_TAG[giverId] || GIVER_LABELS[giverId] || "의뢰인";
  if (giverId === "guard_timid") {
    return (
      "밀수꾼들이 도망쳤대. 마약초만 해도 " +
      presumedQty +
      "개는 될 거야! (소문 " +
      baselineQty +
      "개쯤인데, 난 더 많을 것 같아.)"
    );
  }
  if (giverId === "merchant_greedy") {
    return (
      "금빛 상자를 버렸다더군. " +
      presumedQty +
      "개는 넘게 남았을 거야. (들은 양 " +
      baselineQty +
      "개보다 훨씬 많겠지.)"
    );
  }
  if (quantityMode === "faithful") {
    return (
      "소문상 " +
      baselineQty +
      "개 전후라 하나, 직접 가서 정확히 세어 오게. 내가 미리 단정하진 않겠네."
    );
  }
  return "현장에 " + presumedQty + "개쯤 있을 거라 보는군.";
}

/** 의뢰 수락(①) — 각 NPC가 지레짐작하는 수량. */
export function buildGiverAcceptPresentation(quest, giver) {
  var giverId = giver.giverId || "";
  var exp = giver.experience || {};
  var quantityMode = (exp.propagationOptions && exp.propagationOptions.quantityMode) || "faithful";
  var baseline = rumorBaselineQuantity(quest);
  var presumed = giverPresumedQuantity(baseline, quantityMode);
  var briefing = giverBriefingLine(giver.processSteps);
  var speech = buildGiverPresumedSpeech(giverId, presumed, baseline, quantityMode);
  var tag = GIVER_SPEAKER_TAG[giverId] || giver.label || giverId;
  var presumedLine = "[" + tag + "] " + speech;

  return {
    giverId: giverId,
    giverLabel: GIVER_LABELS[giverId] || giver.label || giverId,
    quantityMode: quantityMode,
    rumorBaselineQuantity: baseline,
    presumedQuantity: presumed,
    presumedSpeech: speech,
    presumedLine: presumedLine,
    briefingFlavorLine: briefing,
    distortedAtAccept: presumed !== baseline,
    acceptBeats: [
      {
        order: 1,
        phase: "giver_presume",
        who: giverId,
        quantity: presumed,
        line: presumedLine,
        note: "의뢰 수락 직후 — NPC가 지레짐작하는 수량",
      },
      {
        order: 2,
        phase: "briefing_flavor",
        who: giverId,
        line: briefing ? "[" + tag + "] " + briefing : "",
        optional: true,
      },
    ],
  };
}

export function buildAlricPresumedSpeech(presentation) {
  var sub = presentation.interpretedRecord.subject || "마약초";
  var iq = presentation.interpretedRecord.quantity;
  var tgt = presentation.interpretedRecord.target || "현장";
  var giverId = presentation.giverId;
  var distorted = presentation.distortion.occurred;

  if (!distorted) {
    return iq + "개… 숫자는 분명하군. " + sub + ", " + tgt + " 맞지.";
  }
  if (giverId === "guard_timid") {
    return "…" + iq + "개군. 경비가 겁먹어 과장했을 수도 있지만, 이만큼은 잡아 두겠네.";
  }
  if (giverId === "merchant_greedy") {
    return iq + "개 쯤 되겠어. 상인 말이 과장이긴 해도, 이 정도 규모로 기록하겠네.";
  }
  return iq + "개로 보는 게 맞겠군. 의뢰 경로를 감안하면 이 수치로 잡겠네.";
}

/** QA·디버그용. 게임 연출 2순위(알릭 직후 작은 UI) — 알릭 대사에 섞지 않음. */
export function buildDistortionCompareLine(presentation) {
  if (!presentation.distortion.occurred) return "";
  return (
    "(당신 보고 " +
    presentation.distortion.playerQuantity +
    "개 ↔ 알릭 기록 " +
    presentation.distortion.interpretedQuantity +
    "개)"
  );
}

/** @deprecated — presumed + aftermath 조합. UE는 reportBeats 사용 권장. */
export function buildAlricDiegeticSpeech(presentation) {
  return buildAlricPresumedSpeech(presentation);
}

export function buildReportBeats(presentation, npcSpeech) {
  var beats = [
    {
      order: 1,
      phase: "player_report",
      who: "player",
      line: presentation.playerReport.line,
      quantity: presentation.playerReport.quantity,
    },
    {
      order: 2,
      phase: "alric_presume",
      who: "alric",
      line: '[학자 알릭] "' + presentation.alricPresumedSpeech + '"',
      quantity: presentation.interpretedRecord.quantity,
      presumed: true,
      note: "보고 직후 알릭이 지레짐작하는 수량 — 여기서 interpreted quantity 표시",
    },
  ];

  if (presentation.distortion.occurred && presentation.distortionCompareLine) {
    beats.push({
      order: 3,
      phase: "distortion_hint",
      who: "ui",
      line: presentation.distortionCompareLine,
      meta: true,
      optional: true,
    });
  }

  if (npcSpeech) {
    beats.push({
      order: 4,
      phase: "alric_dialogue",
      who: "alric",
      line: npcSpeech,
      quantity: presentation.interpretedRecord.quantity,
    });
  }

  if (presentation.giverCompletionLine) {
    beats.push({
      order: 5,
      phase: "giver_echo",
      who: "giver",
      line: "[의뢰 회수] " + presentation.giverCompletionLine,
    });
  }

  if (presentation.outcomeNarration) {
    beats.push({
      order: 6,
      phase: "outcome",
      who: "narrator",
      line: presentation.outcomeNarration,
    });
  }

  return beats;
}

export function buildReportPresentation(turnIn, scenarioText) {
  var giverId = turnIn.giverId || "";
  var giverLabel = GIVER_LABELS[giverId] || giverId;
  var exp = turnIn.experience || {};
  var propOpts = exp.propagationOptions || {};
  var quantityMode = (turnIn.propagation && turnIn.propagation.quantityMode) || propOpts.quantityMode || "faithful";
  var interpreted = (turnIn.propagation && turnIn.propagation.interpretedFacts) || [];
  var playerSnap = quantityFromPlayerFacts(turnIn.facts || []);
  var interpretedSnap = quantityFromInterpreted(interpreted);
  var rules = collectAppliedRules(interpreted);
  var playerQty = playerSnap.quantity;
  var interpretedQty = interpretedSnap.quantity;
  var distorted = playerQty !== interpretedQty;
  var briefing = giverBriefingLine(turnIn.processSteps);
  var outcome = turnIn.outcome || {};
  var branchId = turnIn.outcomeBranch && turnIn.outcomeBranch.branchId;

  var base = {
    scenarioText: String(scenarioText || "").trim(),
    giverId: giverId,
    giverLabel: giverLabel,
    quantityMode: quantityMode,
    giverBriefingLine: briefing,
    giverCompletionLine: exp.completionDialogue || "",
    playerReport: {
      quantity: playerQty,
      subject: playerSnap.subject,
      line: buildPlayerLine(scenarioText),
    },
    interpretedRecord: {
      quantity: interpretedQty,
      subject: interpretedSnap.subject || "마약초",
      certainty: certaintyFromInterpreted(interpreted),
      target: targetFromInterpreted(interpreted),
      displayWhen: "alric_presume",
    },
    distortion: {
      occurred: distorted,
      playerQuantity: playerQty,
      interpretedQuantity: interpretedQty,
      delta: interpretedQty - playerQty,
      appliedRules: rules,
      appliedRuleLabels: ruleLabelsKo(rules),
      summary: buildDistortionSummary(giverLabel, quantityMode, playerQty, interpretedQty, rules),
    },
    outcomeNarration: outcome.narration ? "[" + outcome.narration + "]" : "",
    branchId: branchId || null,
  };

  base.alricPresumedSpeech = buildAlricPresumedSpeech(base);
  base.alricDiegeticSpeech = base.alricPresumedSpeech;
  base.distortionCompareLine = buildDistortionCompareLine(base);
  base.interpretedRecord.presumedLine = '[학자 알릭] "' + base.alricPresumedSpeech + '"';
  base.reportBeats = buildReportBeats(base, null);
  return base;
}

export function finalizePresentation(presentation) {
  return Object.assign({}, presentation);
}

export function enhanceDialogueWithPresentation(baseSpeech, presentation) {
  var presumed = buildAlricPresumedSpeech(presentation);
  var style = "잠시 생각한 뒤";
  if (baseSpeech && baseSpeech.indexOf("말한다") >= 0) {
    if (baseSpeech.indexOf(String(presentation.interpretedRecord.quantity)) >= 0) return baseSpeech;
    return baseSpeech.replace(/말한다:\s*"[^"]*"/, '말한다: "' + presumed + '"');
  }
  return style + ' 말한다: "' + presumed + '"';
}

export function buildDialoguePresentationBlock(presentation, npcSpeech, provider) {
  return {
    primaryBeat: "alric_presume",
    playerLine: presentation.playerReport.line,
    alricPresumedLine: presentation.interpretedRecord.presumedLine,
    alricPresumedSpeech: presentation.alricPresumedSpeech,
    presumedQuantity: presentation.interpretedRecord.quantity,
    distortionCompareLine: presentation.distortionCompareLine,
    distortionSummary: presentation.distortion.summary,
    giverCompletionLine: presentation.giverCompletionLine,
    outcomeNarration: presentation.outcomeNarration,
    reportBeats: buildReportBeats(presentation, npcSpeech),
    npcSpeech: npcSpeech,
    provider: provider,
    quantities: {
      player: presentation.distortion.playerQuantity,
      interpreted: presentation.distortion.interpretedQuantity,
      distorted: presentation.distortion.occurred,
      delta: presentation.distortion.delta,
      appliedRules: presentation.distortion.appliedRules,
    },
  };
}
