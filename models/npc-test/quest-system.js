const DEFAULTS = {
  FEAR_THRESHOLD: 0.7,
  HOSTILITY_THRESHOLD: 0.5,
  DAMPING_FACTOR: 0.65,
  AMPLIFICATION_FACTOR: 1.25,
  MIN_Q: 1,
  MAX_Q: 50,
  MIN_CERTAINTY: 0.05,
  MAX_CERTAINTY: 1,
  TRUST_BLOCK: 0.2,
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
  const neutralKeywords = [
    "바빠",
    "피곤",
    "조용",
    "한산",
    "평온",
    "지쳐",
    "긴장",
    "초조",
    "기침",
    "아프",
    "열",
    "몸살",
    "컨디션",
    "태연",
    "침착",
    "혼란",
    "징후 없음",
    "미발생",
  ];
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

function normalizeActionForSpeech(action) {
  let text = String(action || "")
    .replace(/[,.!?]+$/g, "")
    .trim();
  text = text
    .replace(/했(고|는데|지만)$/g, "했다")
    .replace(/되었(고|는데|지만)$/g, "되었다")
    .replace(/당했(고|는데|지만)$/g, "당했다");
  text = text.replace(/(하고|고|며|는데|지만)$/g, "").trim();
  if (!text) return "상황이 전해졌다";
  if (/(다|했다|되었다|당했다|중이다|보인다|상태다)$/.test(text)) return text;
  return `${text}했다`;
}

class InfoAtom {
  constructor({ info_id, truth_value, metadata }) {
    const facts = Array.isArray(truth_value.facts) ? truth_value.facts : [];
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
      facts,
    };
    this.metadata = {
      origin: metadata.origin,
      creation_tick: Number(metadata.creation_tick ?? 0),
      last_updated_tick: Number(metadata.last_updated_tick ?? metadata.creation_tick ?? 0),
      source: metadata.source || metadata.origin || "unknown",
      rumor: Boolean(metadata.rumor ?? false),
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
  constructor({ id, name, persona, stats, reputation }) {
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
    this.reputation = new Map();
    if (reputation && typeof reputation === "object") {
      Object.keys(reputation).forEach((key) => {
        this.setSubjectReputation(key, reputation[key]);
      });
    }
  }

  setTrustLevel(otherNpcId, trustLevel) {
    this.relations.set(otherNpcId, {
      trustLevel: clamp(Number(trustLevel ?? 0), 0, 1),
    });
  }

  getTrustLevel(otherNpcId) {
    return this.relations.get(otherNpcId)?.trustLevel ?? 0;
  }

  setSubjectReputation(subjectKey, score) {
    const key = String(subjectKey || "").trim();
    if (!key) return;
    this.reputation.set(key, clamp(Number(score ?? 0.5), 0, 1));
  }

  getSubjectReputation(subjectKey) {
    const key = String(subjectKey || "").trim();
    if (!key) return 0.5;
    if (this.reputation.has(key)) return this.reputation.get(key);
    for (const [repKey, value] of this.reputation.entries()) {
      if (key.includes(repKey) || repKey.includes(key)) return value;
    }
    return 0.5;
  }
}

function getSubjectReputation(npc, subject) {
  return npc.getSubjectReputation(String(subject || ""));
}

function distortInformation(rawInfo, npcStats) {
  const distorted = deepCopy(rawInfo);
  const tv = distorted.truth_value;
  const subject = tv.subject;
  const quantity = Number(tv.quantity ?? 1);
  const parseMode = String(tv.parse_mode || "structured");
  const actionType = String(tv.action_type || "unknown");
  const isConservativeMode = parseMode === "conservative_raw";
  const isStateLike = actionType === "state" || isNeutralStateAction(tv.action);
  const relationTrust = clamp(Number(npcStats.relationTrust ?? npcStats.trust ?? 0.5), 0, 1);
  const subjectRep = clamp(Number(npcStats.subjectReputation ?? 0.5), 0, 1);

  if (
    !isStateLike &&
    npcStats.fear > DEFAULTS.FEAR_THRESHOLD &&
    tv.is_countable &&
    !isConservativeMode
  ) {
    distorted.truth_value.subject = transformToThreatening(subject);
    const amplified = quantity * (1 + npcStats.fear * DEFAULTS.AMPLIFICATION_FACTOR);
    distorted.truth_value.quantity = clamp(
      Math.round(lerp(quantity, amplified, DEFAULTS.DAMPING_FACTOR)),
      DEFAULTS.MIN_Q,
      DEFAULTS.MAX_Q
    );
  }

  if (!isStateLike && npcStats.hostility > DEFAULTS.HOSTILITY_THRESHOLD) {
    const canHostilityRemap =
      (actionType === "threat" || actionType === "tactical_move") && !isConservativeMode;
    if (
      canHostilityRemap &&
      !isAlreadyHostileAction(tv.action) &&
      !isNeutralStateAction(tv.action)
    ) {
      distorted.truth_value.action = mapToNegativeAction(tv.action);
    }
  }

  const trustCertainty = calculateCertainty(npcStats.trust);
  let baseCertainty = clamp(Number(tv.certainty ?? trustCertainty), 0, 1);
  baseCertainty = clamp(
    Number((baseCertainty * 0.6 + trustCertainty * 0.4).toFixed(2)),
    DEFAULTS.MIN_CERTAINTY,
    DEFAULTS.MAX_CERTAINTY
  );

  if (relationTrust < 0.45) {
    baseCertainty = clamp(Number((baseCertainty * 0.85).toFixed(2)), DEFAULTS.MIN_CERTAINTY, DEFAULTS.MAX_CERTAINTY);
  }

  if (actionType === "threat" && subjectRep >= 0.75) {
    baseCertainty = clamp(Number((baseCertainty + 0.08).toFixed(2)), DEFAULTS.MIN_CERTAINTY, DEFAULTS.MAX_CERTAINTY);
  }

  if (distorted.metadata.rumor) {
    baseCertainty = clamp(Number((baseCertainty * 0.9).toFixed(2)), DEFAULTS.MIN_CERTAINTY, DEFAULTS.MAX_CERTAINTY);
    distorted.metadata.source = "불명확한 출처";
  }

  distorted.truth_value.certainty = baseCertainty;
  distorted.metadata.last_updated_tick += 1;
  distorted.metadata.source = npcStats.name || distorted.metadata.source;

  return distorted;
}

function reinterpretInfo(distortedInfo, receiverStats) {
  const interpreted = deepCopy(distortedInfo);
  const tv = interpreted.truth_value;
  const quantity = Number(tv.quantity ?? 1);
  const actionType = String(tv.action_type || "unknown");
  const isStateLike = actionType === "state" || isNeutralStateAction(tv.action);
  const credulityBoost = 1 + receiverStats.credulity * 0.55;
  const subjectRep = clamp(Number(receiverStats.subjectReputation ?? 0.5), 0, 1);

  if (tv.is_countable && !isStateLike) {
    interpreted.truth_value.quantity = clamp(
      Math.round(quantity * credulityBoost),
      DEFAULTS.MIN_Q,
      DEFAULTS.MAX_Q
    );
  }

  let certainty = Number(tv.certainty ?? 0.5);
  certainty = clamp(
    Number((certainty * (0.75 + receiverStats.credulity * 0.4)).toFixed(2)),
    DEFAULTS.MIN_CERTAINTY,
    DEFAULTS.MAX_CERTAINTY
  );

  if (actionType === "threat" && subjectRep >= 0.7) {
    certainty = clamp(Number((certainty + 0.05).toFixed(2)), DEFAULTS.MIN_CERTAINTY, DEFAULTS.MAX_CERTAINTY);
  }

  if (receiverStats.trust < 0.4) {
    certainty = clamp(Number((certainty * 0.92).toFixed(2)), DEFAULTS.MIN_CERTAINTY, DEFAULTS.MAX_CERTAINTY);
  }

  interpreted.truth_value.certainty = certainty;
  interpreted.metadata.last_updated_tick += 1;
  return interpreted;
}

function propagateInformation(sender, receiver, originalInfo) {
  return propagateFactBundle(sender, receiver, originalInfo, null);
}

function propagateFactBundle(sender, receiver, originalInfo, factsInput) {
  const trustLevel = sender.getTrustLevel(receiver.id);
  if (trustLevel < DEFAULTS.TRUST_BLOCK) {
    return {
      blocked: true,
      reason: `Trust_Level ${trustLevel.toFixed(2)} < ${DEFAULTS.TRUST_BLOCK.toFixed(2)}`,
      senderDistorted: null,
      receiverInterpreted: null,
      distortedFacts: [],
      interpretedFacts: [],
    };
  }

  const sourceFacts =
    factsInput && factsInput.length
      ? factsInput
      : originalInfo.truth_value.facts && originalInfo.truth_value.facts.length
        ? originalInfo.truth_value.facts
        : [
            {
              fact_id: "F01",
              subject: originalInfo.truth_value.subject,
              action: originalInfo.truth_value.action,
              target: originalInfo.truth_value.target,
              quantity: originalInfo.truth_value.quantity,
              certainty: originalInfo.truth_value.certainty,
              is_countable: originalInfo.truth_value.is_countable,
              action_type: originalInfo.truth_value.action_type,
              parse_confidence: originalInfo.truth_value.parse_confidence,
              parse_mode: originalInfo.truth_value.parse_mode,
              raw_text: originalInfo.truth_value.raw_text || "",
            },
          ];

  const distortedFacts = [];
  const interpretedFacts = [];

  for (let i = 0; i < sourceFacts.length; i += 1) {
    const fact = sourceFacts[i];
    const atom = new InfoAtom({
      info_id: fact.fact_id || `INF_${String(i + 1).padStart(3, "0")}`,
      truth_value: {
        subject: fact.subject,
        action: fact.action,
        target: fact.target,
        quantity: fact.quantity,
        certainty: fact.certainty,
        is_countable: fact.is_countable,
        action_type: fact.action_type,
        parse_confidence: fact.parse_confidence,
        parse_mode: fact.parse_mode,
        is_factual: true,
      },
      metadata: {
        origin: originalInfo.metadata.origin,
        source: originalInfo.metadata.source,
        creation_tick: originalInfo.metadata.creation_tick,
        last_updated_tick: originalInfo.metadata.last_updated_tick,
        rumor: Boolean(fact.rumor),
      },
    });

    const senderContext = {
      ...sender.stats,
      name: sender.name,
      relationTrust: trustLevel,
      subjectReputation: sender.getSubjectReputation(fact.subject),
    };
    const receiverContext = {
      ...receiver.stats,
      subjectReputation: receiver.getSubjectReputation(fact.subject),
    };

    const senderDistorted = distortInformation(atom, senderContext);
    const receiverInterpreted = reinterpretInfo(senderDistorted, receiverContext);
    receiver.knowledgeBase.update(receiverInterpreted);
    distortedFacts.push(senderDistorted);
    interpretedFacts.push(receiverInterpreted);
  }

  return {
    blocked: false,
    reason: null,
    senderDistorted: distortedFacts[0] || null,
    receiverInterpreted: interpretedFacts[0] || null,
    distortedFacts,
    interpretedFacts,
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

function pickKoreanParticle(word, withBatchim, withoutBatchim) {
  const text = String(word || "").trim();
  if (!text) return withoutBatchim;
  const lastChar = text.charAt(text.length - 1);
  const code = lastChar.charCodeAt(0);
  const HANGUL_BASE = 44032;
  const HANGUL_END = 55203;
  if (code < HANGUL_BASE || code > HANGUL_END) {
    return withoutBatchim;
  }
  const hasBatchim = ((code - HANGUL_BASE) % 28) !== 0;
  return hasBatchim ? withBatchim : withoutBatchim;
}

function buildContextGrounding(distortedBundle) {
  const facts = distortedBundle.facts || [];
  return {
    dataOnly: {
      info_id: distortedBundle.info_id,
      facts,
      source: distortedBundle.source,
    },
    facts,
  };
}

function factToSpeechLine(fact, persona) {
  const subjectText = String(fact.subject || "대상");
  const subjectParticle = pickKoreanParticle(subjectText, "이", "가");
  const actionText = normalizeActionForSpeech(fact.action);
  const qtyText = fact.is_countable ? `수량 약 ${fact.quantity}` : "수량 해당 없음";
  const targetText = fact.target ? `장소 ${fact.target}` : "장소 불명";
  const certaintyText = `확신도 ${fact.certainty}`;

  if (persona === "fearful_guard") {
    return `${subjectText}${subjectParticle} ${actionText}. (${targetText}, ${qtyText}, ${certaintyText})`;
  }
  if (persona === "hotblood_hunter") {
    return `${subjectText}${subjectParticle} ${actionText}! ${targetText}, ${qtyText}!`;
  }
  if (persona === "calm_scholar") {
    return `정리하면 ${subjectText}${subjectParticle} ${actionText}. ${targetText}, ${certaintyText}.`;
  }
  return `${subjectText}${subjectParticle} ${actionText}. ${targetText}, ${qtyText}, ${certaintyText}.`;
}

function mockLLMGenerate(systemPrompt, groundedContext, persona) {
  const facts = groundedContext.facts || [];
  const personaStyles = {
    fearful_guard: "목소리를 낮추며 주변을 살피고",
    cynical_merchant: "의심 섞인 한숨과 함께",
    calm_scholar: "차분하게 사실을 정리하며",
    hotblood_hunter: "격앙된 어조로 주먹을 쥐고",
  };
  const style = personaStyles[persona] || "신중한 어조로";
  const source = groundedContext.dataOnly?.source || "unknown";
  const speechLines = facts.map((f) => factToSpeechLine(f, persona));
  const speech =
    facts.length > 1
      ? `${style} 말한다: "${speechLines.join(" 그리고 ")}" (출처: ${source})`
      : `${style} 말한다: "${speechLines[0] || "정보가 전달되지 않았다"}" (출처: ${source})`;

  return [
    `[System Guardrail] ${systemPrompt}`,
    `[Grounded DATA]\n${JSON.stringify(groundedContext.dataOnly, null, 2)}`,
    `[NPC Speech]\n${speech}`,
  ].join("\n");
}

function generateNPCDialogue(distortedFactsBundle, npcPersona) {
  const systemPrompt = createSoftGuardrailPrompt(npcPersona);
  const context = buildContextGrounding(distortedFactsBundle);
  const finalSpeech = mockLLMGenerate(systemPrompt, context, npcPersona);
  return {
    systemPrompt,
    context,
    finalSpeech,
    engineData: context.dataOnly,
    npcSpeech: finalSpeech.split("[NPC Speech]\n")[1] || finalSpeech,
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
      facts: [],
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
    reputation: {
      촌장: 0.85,
      상인: 0.4,
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
    reputation: {
      촌장: 0.7,
      상인: 0.55,
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
  if (overrides.facts && overrides.facts.length) {
    info.truth_value.facts = overrides.facts;
    const primary = overrides.facts[0];
    Object.assign(info.truth_value, {
      subject: primary.subject,
      action: primary.action,
      target: primary.target,
      quantity: primary.quantity,
      certainty: primary.certainty,
      is_countable: primary.is_countable,
      action_type: primary.action_type,
      parse_confidence: primary.parse_confidence,
      parse_mode: primary.parse_mode,
    });
  }

  Object.assign(sender.stats, overrides.senderStats || {});
  Object.assign(receiver.stats, overrides.receiverStats || {});

  if (overrides.senderReputation && typeof overrides.senderReputation === "object") {
    Object.keys(overrides.senderReputation).forEach((key) => {
      sender.setSubjectReputation(key, overrides.senderReputation[key]);
    });
  }
  if (overrides.receiverReputation && typeof overrides.receiverReputation === "object") {
    Object.keys(overrides.receiverReputation).forEach((key) => {
      receiver.setSubjectReputation(key, overrides.receiverReputation[key]);
    });
  }

  if (typeof overrides.trustLevel === "number") {
    sender.setTrustLevel(receiver.id, overrides.trustLevel);
  }

  const propagation = propagateFactBundle(sender, receiver, info, overrides.facts || null);

  let dialogue = null;
  if (!propagation.blocked && propagation.interpretedFacts.length) {
    const interpretedFacts = propagation.interpretedFacts.map((item) => {
      const tv = item.truth_value;
      return {
        fact_id: item.info_id,
        subject: tv.subject,
        action: tv.action,
        target: tv.target,
        quantity: tv.quantity,
        certainty: tv.certainty,
        is_countable: tv.is_countable,
        action_type: tv.action_type,
      };
    });
    dialogue = generateNPCDialogue(
      {
        info_id: info.info_id,
        source: propagation.interpretedFacts[0].metadata.source,
        facts: interpretedFacts,
      },
      receiver.persona
    );
  }

  return {
    baseInfo: info,
    sender,
    receiver,
    propagation,
    dialogue,
    knowledgeBaseSnapshot: receiver.knowledgeBase.list(),
    facts: overrides.facts || info.truth_value.facts || [],
  };
}

window.QuestSystem = {
  InfoAtom,
  NPC,
  KnowledgeBase,
  distortInformation,
  reinterpretInfo,
  propagateInformation,
  propagateFactBundle,
  generateNPCDialogue,
  executeScenario,
  DEFAULTS,
};
