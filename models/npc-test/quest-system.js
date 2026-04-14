const DEFAULTS = {
  FEAR_THRESHOLD: 0.7,
  HOSTILITY_THRESHOLD: 0.5,
  DAMPING_FACTOR: 0.65,
  AMPLIFICATION_FACTOR: 1.25,
  MIN_Q: 1,
  MAX_Q: 50,
  MIN_CERTAINTY: 0.05,
  MAX_CERTAINTY: 1,
};

function clamp(value, min, max) {
  return Math.min(max, Math.max(min, value));
}

function lerp(start, end, t) {
  return start + (end - start) * t;
}

function deepCopy(value) {
  return JSON.parse(JSON.stringify(value));
}

function transformToThreatening(subject) {
  const threatMap = {
    wolf: "광폭한 늑대 무리",
    wolves: "광폭한 늑대 무리",
    bandit: "무장한 약탈자 무리",
    stranger: "수상한 침입자",
    scout: "적의 전초 정찰병",
    늑대: "광폭한 늑대 무리",
    도적: "무장한 약탈자 무리",
    정찰병: "적의 전초 정찰병",
  };

  const lowered = String(subject || "").toLowerCase();
  return threatMap[lowered] || String(subject || "");
}

function isAlreadyHostileAction(action) {
  const text = String(action || "").toLowerCase();
  const hostileKeywords = ["독살", "암살", "살해", "피습", "공격", "습격", "납치"];
  return hostileKeywords.some((keyword) => text.includes(keyword));
}

function isNeutralStateAction(action) {
  const text = String(action || "").toLowerCase();
  const neutralKeywords = ["바빠", "피곤", "조용", "한산", "평온", "지쳐", "긴장", "초조", "기침", "아프", "열", "몸살", "컨디션"];
  return neutralKeywords.some((keyword) => text.includes(keyword));
}

function mapToNegativeAction(action) {
  const map = {
    move: "습격을 준비하고 있다",
    travel: "마을 침투를 시도하고 있다",
    gather: "약탈을 위해 집결하고 있다",
    wait: "매복하며 기회를 노리고 있다",
    talk: "악의적인 소문을 퍼뜨리고 있다",
    observe: "공격 경로를 정찰하고 있다",
    escape: "혼란을 노리고 도주 중이다",
    이동: "습격을 준비하고 있다",
    탈출: "혼란을 노리고 도주 중이다",
    공격: "선제공격을 준비하고 있다",
    정찰: "공격 경로를 정찰하고 있다",
  };

  const lowered = String(action || "").toLowerCase();
  return map[lowered] || `적대적 의도로 행동하고 있다 (${action})`;
}

function calculateCertainty(trust) {
  const normalizedTrust = clamp(Number(trust) || 0, 0, 1);
  const certainty = lerp(0.35, 1.0, normalizedTrust);
  return clamp(Number(certainty.toFixed(2)), DEFAULTS.MIN_CERTAINTY, DEFAULTS.MAX_CERTAINTY);
}

class InfoAtom {
  constructor({ info_id, truth_value, metadata }) {
    this.info_id = info_id;
    this.truth_value = {
      subject: truth_value.subject,
      action: truth_value.action,
      target: truth_value.target || null,
      quantity: Number(truth_value.quantity ?? 1),
      certainty: Number(truth_value.certainty ?? 1),
      is_countable: Boolean(truth_value.is_countable ?? true),
      action_type: truth_value.action_type || "unknown",
      parse_confidence: Number(truth_value.parse_confidence ?? 1),
      parse_mode: truth_value.parse_mode || "structured",
      is_factual: Boolean(truth_value.is_factual),
    };
    this.metadata = {
      origin: metadata.origin,
      creation_tick: Number(metadata.creation_tick ?? 0),
      last_updated_tick: Number(metadata.last_updated_tick ?? metadata.creation_tick ?? 0),
      source: metadata.source || metadata.origin || "unknown",
    };
  }
}

class KnowledgeBase {
  constructor() {
    this.entries = new Map();
  }

  update(info) {
    this.entries.set(info.info_id, deepCopy(info));
  }

  get(infoId) {
    return this.entries.get(infoId);
  }

  list() {
    return [...this.entries.values()];
  }
}

class NPC {
  constructor({ id, name, persona, stats }) {
    this.id = id;
    this.name = name;
    this.persona = persona;
    this.stats = {
      fear: clamp(Number(stats.fear ?? 0), 0, 1),
      hostility: clamp(Number(stats.hostility ?? 0), 0, 1),
      trust: clamp(Number(stats.trust ?? 0.5), 0, 1),
      credulity: clamp(Number(stats.credulity ?? 0.5), 0, 1),
    };
    this.knowledgeBase = new KnowledgeBase();
    this.relations = new Map();
  }

  setTrustLevel(otherNpcId, trustLevel) {
    this.relations.set(otherNpcId, {
      trustLevel: clamp(Number(trustLevel ?? 0), 0, 1),
    });
  }

  getTrustLevel(otherNpcId) {
    return this.relations.get(otherNpcId)?.trustLevel ?? 0;
  }
}

function distortInformation(rawInfo, npcStats) {
  const distorted = deepCopy(rawInfo);
  const subject = distorted.truth_value.subject;
  const quantity = Number(distorted.truth_value.quantity ?? 1);
  const parseMode = String(distorted.truth_value.parse_mode || "structured");
  const isConservativeMode = parseMode === "conservative_raw";

  if (npcStats.fear > DEFAULTS.FEAR_THRESHOLD && distorted.truth_value.is_countable && !isConservativeMode) {
    distorted.truth_value.subject = transformToThreatening(subject);
    const amplified = quantity * (1 + npcStats.fear * DEFAULTS.AMPLIFICATION_FACTOR);
    distorted.truth_value.quantity = clamp(
      Math.round(lerp(quantity, amplified, DEFAULTS.DAMPING_FACTOR)),
      DEFAULTS.MIN_Q,
      DEFAULTS.MAX_Q
    );
  }

  if (npcStats.hostility > DEFAULTS.HOSTILITY_THRESHOLD) {
    const actionType = String(distorted.truth_value.action_type || "unknown");
    const canHostilityRemap = (actionType === "threat" || actionType === "tactical_move") && !isConservativeMode;
    if (
      canHostilityRemap &&
      !isAlreadyHostileAction(distorted.truth_value.action) &&
      !isNeutralStateAction(distorted.truth_value.action)
    ) {
      distorted.truth_value.action = mapToNegativeAction(distorted.truth_value.action);
    }
  }

  const trustCertainty = calculateCertainty(npcStats.trust);
  const baseCertainty = clamp(Number(distorted.truth_value.certainty ?? trustCertainty), 0, 1);
  distorted.truth_value.certainty = clamp(
    Number((baseCertainty * 0.6 + trustCertainty * 0.4).toFixed(2)),
    DEFAULTS.MIN_CERTAINTY,
    DEFAULTS.MAX_CERTAINTY
  );
  distorted.metadata.last_updated_tick += 1;
  distorted.metadata.source = npcStats.name || distorted.metadata.source;

  return distorted;
}

function reinterpretInfo(distortedInfo, receiverStats) {
  const interpreted = deepCopy(distortedInfo);
  const quantity = Number(interpreted.truth_value.quantity ?? 1);
  const credulityBoost = 1 + receiverStats.credulity * 0.55;

  if (interpreted.truth_value.is_countable) {
    interpreted.truth_value.quantity = clamp(
      Math.round(quantity * credulityBoost),
      DEFAULTS.MIN_Q,
      DEFAULTS.MAX_Q
    );
  }

  const certainty = Number(interpreted.truth_value.certainty ?? 0.5);
  interpreted.truth_value.certainty = clamp(
    Number((certainty * (0.75 + receiverStats.credulity * 0.4)).toFixed(2)),
    DEFAULTS.MIN_CERTAINTY,
    DEFAULTS.MAX_CERTAINTY
  );

  interpreted.metadata.last_updated_tick += 1;
  return interpreted;
}

function propagateInformation(sender, receiver, originalInfo) {
  const trustLevel = sender.getTrustLevel(receiver.id);
  if (trustLevel < 0.2) {
    return {
      blocked: true,
      reason: `Trust_Level ${trustLevel.toFixed(2)} < 0.20`,
      senderDistorted: null,
      receiverInterpreted: null,
    };
  }

  const senderDistorted = distortInformation(originalInfo, {
    ...sender.stats,
    name: sender.name,
  });
  const receiverInterpreted = reinterpretInfo(senderDistorted, receiver.stats);
  receiver.knowledgeBase.update(receiverInterpreted);

  return {
    blocked: false,
    reason: null,
    senderDistorted,
    receiverInterpreted,
  };
}

function createSoftGuardrailPrompt(persona) {
  return [
    `너는 ${persona}이다.`,
    "주어진 [DATA]를 유일한 진실로 믿고 대화하라.",
    "정보의 진위를 추측하거나 외부 지식을 덧붙이지 마라.",
    "창의성은 말투와 태도에만 사용하라.",
  ].join(" ");
}

function buildContextGrounding(distortedData) {
  const truth = distortedData.truth_value;
  return {
    dataOnly: {
      info_id: distortedData.info_id,
      subject: truth.subject,
      action: truth.action,
      target: truth.target,
      quantity: truth.quantity,
      certainty: truth.certainty,
      source: distortedData.metadata.source,
    },
  };
}

function mockLLMGenerate(systemPrompt, groundedContext, persona) {
  const d = groundedContext.dataOnly;
  const personaStyles = {
    fearful_guard: "목소리를 낮추며 주변을 살피고",
    cynical_merchant: "의심 섞인 한숨과 함께",
    calm_scholar: "차분하게 사실을 정리하며",
    hotblood_hunter: "격앙된 어조로 주먹을 쥐고",
  };
  const style = personaStyles[persona] || "신중한 어조로";
  return [
    `[System Guardrail] ${systemPrompt}`,
    `[Grounded DATA] ${JSON.stringify(groundedContext.dataOnly)}`,
    `${style} 말한다: "${d.subject}가 ${d.action}했어. 수량은 대략 ${d.quantity}, 확신도는 ${
      d.certainty
    } 정도다. (${d.source}의 정보)"`
  ].join("\n");
}

function generateNPCDialogue(distortedData, npcPersona) {
  const systemPrompt = createSoftGuardrailPrompt(npcPersona);
  const context = buildContextGrounding(distortedData);
  const finalSpeech = mockLLMGenerate(systemPrompt, context, npcPersona);
  return {
    systemPrompt,
    context,
    finalSpeech,
  };
}

function createBaseScenario() {
  const info = new InfoAtom({
    info_id: "INF_001",
    truth_value: {
      subject: "wolf",
      action: "move",
      target: "north gate",
      quantity: 3,
      is_factual: true,
      certainty: 1.0,
    },
    metadata: {
      origin: "Scout_A",
      source: "Scout_A",
      creation_tick: 1250,
    },
  });

  const sender = new NPC({
    id: "NPC_A",
    name: "Rogan",
    persona: "fearful_guard",
    stats: {
      fear: 0.82,
      hostility: 0.62,
      trust: 0.55,
      credulity: 0.2,
    },
  });

  const receiver = new NPC({
    id: "NPC_B",
    name: "Mira",
    persona: "cynical_merchant",
    stats: {
      fear: 0.33,
      hostility: 0.28,
      trust: 0.65,
      credulity: 0.86,
    },
  });

  sender.setTrustLevel(receiver.id, 0.74);
  receiver.setTrustLevel(sender.id, 0.61);

  return { info, sender, receiver };
}

function executeScenario(overrides = {}) {
  const { info, sender, receiver } = createBaseScenario();
  if (overrides.infoTruthValue && typeof overrides.infoTruthValue === "object") {
    Object.assign(info.truth_value, overrides.infoTruthValue);
  }
  if (overrides.infoMetadata && typeof overrides.infoMetadata === "object") {
    Object.assign(info.metadata, overrides.infoMetadata);
  }
  Object.assign(sender.stats, overrides.senderStats || {});
  Object.assign(receiver.stats, overrides.receiverStats || {});
  if (typeof overrides.trustLevel === "number") {
    sender.setTrustLevel(receiver.id, overrides.trustLevel);
  }

  const propagation = propagateInformation(sender, receiver, info);
  const dialogue = propagation.blocked
    ? null
    : generateNPCDialogue(propagation.receiverInterpreted, receiver.persona);

  return {
    baseInfo: info,
    sender,
    receiver,
    propagation,
    dialogue,
    knowledgeBaseSnapshot: receiver.knowledgeBase.list(),
  };
}

window.QuestSystem = {
  InfoAtom,
  NPC,
  KnowledgeBase,
  distortInformation,
  reinterpretInfo,
  propagateInformation,
  generateNPCDialogue,
  executeScenario,
  DEFAULTS,
};
