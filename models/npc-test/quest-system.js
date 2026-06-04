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
  TRUST_PARTIAL_MIN: 0.08,
  HIGH_REPUTATION: 0.75,
  LOW_REPUTATION: 0.3,
  KB_DECAY_PER_TICK: 0.03,
  HOP_CERTAINTY_DECAY: 0.92,
  BUNDLE_THREAT_QTY_CAP: 1.15,
};

/** Persona weights applied on top of base fear/hostility/credulity. */
const PERSONA_DISTORTION_PROFILES = {
  fearful_guard: { fear: 1.15, hostility: 1.05, credulity: 0.95, rumor: 1.1, target: 1.2 },
  cynical_merchant: { fear: 0.85, hostility: 0.9, credulity: 1.08, rumor: 1.35, target: 0.9 },
  calm_scholar: { fear: 0.75, hostility: 0.7, credulity: 0.92, rumor: 0.85, target: 0.85 },
  hotblood_hunter: { fear: 0.95, hostility: 1.25, credulity: 1.05, rumor: 0.95, target: 1.1 },
};

const SESSION_STORE = new Map();

const BUILTIN_PROFILE_REGISTRY = {
  player: {
    id: "player",
    displayName: "카엘",
    propagationProfile: { persona: "witness", fear: 0.1, hostility: 0.1 },
    defaultQuantityMode: "faithful",
  },
  npcs: {
    scout: {
      id: "NPC_SCOUT",
      displayName: "정찰병",
      persona: "hotblood_hunter",
      stats: { fear: 0.5, hostility: 0.4, trust: 0.6, credulity: 0.7 },
      reputation: { 늑대: 0.5, 촌장: 0.7, 상인: 0.5 },
      defaultTrustToPlayer: 0.7,
    },
    mayor: {
      id: "NPC_MAYOR",
      displayName: "촌장",
      persona: "calm_scholar",
      stats: { fear: 0.35, hostility: 0.2, trust: 0.7, credulity: 0.55 },
      reputation: { 촌장: 0.9, 늑대: 0.4, 상인: 0.65 },
      defaultTrustToPlayer: 0.72,
    },
    merchant: {
      id: "NPC_MERCHANT",
      displayName: "상인",
      persona: "cynical_merchant",
      stats: { fear: 0.45, hostility: 0.35, trust: 0.6, credulity: 0.88 },
      reputation: { 상인: 0.85, 늑대: 0.25, 촌장: 0.55 },
      defaultTrustToPlayer: 0.58,
    },
    guard: {
      id: "NPC_GUARD",
      displayName: "경비",
      persona: "fearful_guard",
      stats: { fear: 0.82, hostility: 0.55, trust: 0.55, credulity: 0.75 },
      reputation: { 늑대: 0.2, 촌장: 0.75, 상인: 0.45 },
      defaultTrustToPlayer: 0.65,
    },
  },
};

let PROFILE_REGISTRY = null;

function getProfileRegistry() {
  return PROFILE_REGISTRY || BUILTIN_PROFILE_REGISTRY;
}

function registerProfiles(registry) {
  if (!registry || typeof registry !== "object") return;
  const current = getProfileRegistry();
  PROFILE_REGISTRY = {
    npcs: registry.npcs || current.npcs,
    player: registry.player || current.player,
  };
}

function createNpcFromProfile(profileKey) {
  const prof = getProfileRegistry().npcs[profileKey];
  if (!prof) {
    throw new Error("Unknown NPC profile: " + profileKey);
  }
  const npc = new NPC({
    id: prof.id,
    name: prof.displayName,
    persona: prof.persona,
    stats: {
      fear: prof.stats.fear,
      hostility: prof.stats.hostility,
      trust: prof.stats.trust,
      credulity: prof.stats.credulity,
    },
    reputation: {},
  });
  Object.keys(prof.reputation || {}).forEach((key) => {
    npc.setSubjectReputation(key, prof.reputation[key]);
  });
  npc.profileKey = profileKey;
  return npc;
}

function createPlayerActor() {
  const p = getProfileRegistry().player;
  const spread = p.propagationProfile || {};
  const npc = new NPC({
    id: p.id,
    name: p.displayName,
    persona: spread.persona || "witness",
    stats: {
      fear: spread.fear ?? 0.1,
      hostility: spread.hostility ?? 0.1,
      trust: 0.5,
      credulity: 0.5,
    },
    reputation: {},
  });
  npc.isPlayer = true;
  return npc;
}

function resolveTrustToReceiver(sender, receiver, receiverProfileKey, overrides) {
  if (typeof overrides.trustLevel === "number") {
    return overrides.trustLevel;
  }
  if (sender.isPlayer) {
    const prof = getProfileRegistry().npcs[receiverProfileKey];
    const base = prof?.defaultTrustToPlayer ?? 0.7;
    if (typeof window !== "undefined" && window.ReputationSystem) {
      const sessionKey = overrides.reputationSessionKey || overrides.sessionKey || "default";
      return window.ReputationSystem.resolveTrustFromReputation(
        receiverProfileKey,
        base,
        overrides.reputationSessionKey || overrides.sessionKey || "default"
      );
    }
    return base;
  }
  return 0.74;
}

function clamp(value, min, max) {
  return Math.min(max, Math.max(min, value));
}

function lerp(start, end, t) {
  return start + (end - start) * t;
}

function deepCopy(value) {
  return JSON.parse(JSON.stringify(value));
}

function isHostileActionType(actionType) {
  return actionType === "threat" || actionType === "tactical_move";
}

function applyPersonaWeights(stats, persona, role) {
  const profile = PERSONA_DISTORTION_PROFILES[persona] || {
    fear: 1,
    hostility: 1,
    credulity: 1,
    rumor: 1,
    target: 1,
  };
  const out = { ...stats, persona: persona || stats.persona || "unknown", role: role || stats.role || "npc" };
  out.fear = clamp(Number(stats.fear || 0) * profile.fear, 0, 1);
  out.hostility = clamp(Number(stats.hostility || 0) * profile.hostility, 0, 1);
  out.credulity = clamp(Number(stats.credulity || 0) * profile.credulity, 0, 1);
  out.rumorWeight = profile.rumor;
  out.targetDistortWeight = profile.target;
  return out;
}

function transformTargetUnderDistress(target, fear, weight) {
  const t = String(target || "현장");
  const w = clamp(Number(weight ?? 1), 0.5, 1.5);
  if (fear <= DEFAULTS.FEAR_THRESHOLD) return t;
  const scaryTargets = {
    북문: "북문 일대(위험 구역)",
    남문: "남문 일대(위험 구역)",
    마을: "마을 전역(불안 정황)",
    숲: "숲 깊은 곳(위협 구역)",
    동물원: "동물원 전 구역(통제 불가)",
    궁정: "궁정 내부(긴급 상황)",
    시장: "시장 일대(혼란 가능)",
  };
  if (fear * w > 0.85 && scaryTargets[t]) return scaryTargets[t];
  if (fear * w > DEFAULTS.FEAR_THRESHOLD && t.length >= 2) {
    return `${t}(긴장 고조)`;
  }
  return t;
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
      applied_rules: Array.isArray(metadata.applied_rules) ? metadata.applied_rules.slice() : [],
      bundle_notes: Array.isArray(metadata.bundle_notes) ? metadata.bundle_notes.slice() : [],
    };
  }
}

function kbMergeKey(info) {
  const tv = info.truth_value || {};
  return [String(tv.subject || ""), String(tv.action_type || ""), String(tv.target || "")]
    .join("::")
    .toLowerCase();
}

function mergeKnowledgeEntries(existing, incoming) {
  const prev = deepCopy(existing);
  const next = deepCopy(incoming);
  const prevTv = prev.truth_value;
  const nextTv = next.truth_value;
  const prevRules = prev.metadata.applied_rules || [];
  const nextRules = next.metadata.applied_rules || [];

  if (nextTv.is_countable && prevTv.is_countable) {
    const prevQ = Number(prevTv.quantity || 1);
    const nextQ = Number(nextTv.quantity || 1);
    if (nextQ !== prevQ) {
      nextTv.quantity = Math.max(prevQ, nextQ);
      nextRules.push("kb_qty_conflict_max");
    }
  }

  const prevCert = Number(prevTv.certainty || 0.5);
  const nextCert = Number(nextTv.certainty || 0.5);
  nextTv.certainty = clamp(Number(Math.max(prevCert, nextCert).toFixed(2)), DEFAULTS.MIN_CERTAINTY, DEFAULTS.MAX_CERTAINTY);
  if (Math.abs(prevCert - nextCert) > 0.08) {
    nextRules.push("kb_certainty_conflict_max");
  }

  next.metadata.applied_rules = [...new Set([...prevRules, ...nextRules, "kb_merged"])];
  next.metadata.last_updated_tick = Math.max(
    Number(prev.metadata.last_updated_tick || 0),
    Number(next.metadata.last_updated_tick || 0)
  ) + 1;
  next.info_id = prev.info_id;
  return next;
}

class KnowledgeBase {
  constructor() {
    this.entries = new Map();
    this.mergeIndex = new Map();
  }

  applyDecay(entry, currentTick) {
    const tick = Number(currentTick ?? 0);
    const created = Number(entry.metadata.creation_tick ?? tick);
    const age = Math.max(0, tick - created);
    if (age <= 0) return entry;
    const decay = Math.pow(1 - DEFAULTS.KB_DECAY_PER_TICK, age);
    entry.truth_value.certainty = clamp(
      Number((Number(entry.truth_value.certainty || 0.5) * decay).toFixed(2)),
      DEFAULTS.MIN_CERTAINTY,
      DEFAULTS.MAX_CERTAINTY
    );
    pushRule(entry.metadata, "kb_time_decay");
    return entry;
  }

  update(info, currentTick) {
    const copy = deepCopy(info);
    copy.metadata.creation_tick = Number(copy.metadata.creation_tick ?? currentTick ?? 0);
    copy.metadata.last_updated_tick = Number(currentTick ?? copy.metadata.last_updated_tick ?? 0);
    const mergeKey = kbMergeKey(copy);
    const existingId = this.mergeIndex.get(mergeKey);
    if (existingId && this.entries.has(existingId)) {
      const merged = mergeKnowledgeEntries(this.entries.get(existingId), copy);
      this.entries.set(existingId, merged);
      return { action: "merge", info_id: existingId, merge_key: mergeKey };
    }

    // Anti-revision / contradiction dampening:
    // If new hostile info and old calm(state) coexist for same subject+place, reduce the conflicting one.
    const newTv = copy.truth_value || {};
    const newType = newTv.action_type || "";
    const newTargetKey = normalizePlaceKey(newTv.target || "");
    const newSubject = String(newTv.subject || "");
    for (const entry of this.entries.values()) {
      const tv = entry.truth_value || {};
      if (String(tv.subject || "") !== newSubject) continue;
      if (normalizePlaceKey(tv.target || "") !== newTargetKey) continue;
      const oldType = tv.action_type || "";

      const isConflict =
        (isHostileActionType(newType) && oldType === "state") ||
        (newType === "state" && isHostileActionType(oldType));
      if (!isConflict) continue;

      const prev = Number(tv.certainty ?? 0.5);
      const damp = newType === "state" ? 0.72 : 0.68; // new calm dampens old hostile
      tv.certainty = clamp(Number((prev * damp).toFixed(2)), DEFAULTS.MIN_CERTAINTY, DEFAULTS.MAX_CERTAINTY);
      pushRule(entry.metadata, newType === "state" ? "kb_conflict_threat_down" : "kb_conflict_calm_down");
    }

    this.entries.set(copy.info_id, copy);
    this.mergeIndex.set(mergeKey, copy.info_id);
    return { action: "insert", info_id: copy.info_id, merge_key: mergeKey };
  }

  listWithDecay(currentTick) {
    const tick = Number(currentTick ?? 0);
    const out = [];
    for (const entry of this.entries.values()) {
      out.push(this.applyDecay(deepCopy(entry), tick));
    }
    return out;
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

function isFaithfulQuantityMode(npcStats) {
  return String(npcStats.quantityMode || "dramatic").toLowerCase() === "faithful";
}

function pushRule(metadata, ruleId) {
  if (!metadata.applied_rules) metadata.applied_rules = [];
  if (metadata.applied_rules.indexOf(ruleId) < 0) metadata.applied_rules.push(ruleId);
}

function snapshotTruth(tv) {
  return {
    subject: tv.subject,
    action: tv.action,
    target: tv.target,
    object: tv.object || "",
    quantity: tv.quantity,
    certainty: tv.certainty,
    is_countable: tv.is_countable,
    action_type: tv.action_type,
  };
}

function toGroundedFact(tv, meta = {}) {
  return {
    subject: tv.subject,
    action: tv.action,
    target: tv.target,
    object: tv.object || "",
    quantity: tv.quantity,
    certainty: tv.certainty,
    is_countable: tv.is_countable,
    action_type: tv.action_type,
    applied_rules: meta.applied_rules || [],
    bundle_notes: meta.bundle_notes || [],
    rumor: Boolean(meta.rumor),
  };
}

function factInputToAtom(fact, originalInfo, index) {
  return new InfoAtom({
    info_id: fact.fact_id || `INF_${String(index + 1).padStart(3, "0")}`,
    truth_value: {
      subject: fact.subject,
      action: fact.action,
      target: fact.target,
      object: fact.object || "",
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
      source_chain: Array.isArray(fact.source_chain) ? fact.source_chain.slice() : [originalInfo.metadata.origin],
    },
  });
}

function interpretedToFactInputs(interpretedFacts) {
  return interpretedFacts.map((item, index) => {
    const tv = item.truth_value;
    return {
      fact_id: item.info_id || `F${String(index + 1).padStart(2, "0")}`,
      subject: tv.subject,
      action: tv.action,
      target: tv.target,
      object: tv.object || "",
      quantity: tv.quantity,
      certainty: tv.certainty,
      is_countable: tv.is_countable,
      action_type: tv.action_type,
      parse_confidence: tv.parse_confidence,
      parse_mode: tv.parse_mode,
      rumor: item.metadata.rumor,
      source_chain: item.metadata.source_chain || [],
    };
  });
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
  const faithfulQty = isFaithfulQuantityMode(npcStats);
  const repDampen = subjectRep >= DEFAULTS.HIGH_REPUTATION ? 0.55 : 1;
  const repAmplify = subjectRep <= DEFAULTS.LOW_REPUTATION ? 1.2 : 1;

  if (Number(npcStats.trustDegradeMultiplier || 1) < 1) {
    pushRule(distorted.metadata, "partial_trust_degrade");
  }

  if (
    !isStateLike &&
    npcStats.fear > DEFAULTS.FEAR_THRESHOLD &&
    tv.is_countable &&
    !isConservativeMode &&
    !faithfulQty
  ) {
    const fearScale = repDampen;
    if (fearScale < 1) pushRule(distorted.metadata, "reputation_dampen_fear");
    if (subjectRep < DEFAULTS.HIGH_REPUTATION) {
      distorted.truth_value.subject = transformToThreatening(subject);
      pushRule(distorted.metadata, "fear_subject_threaten");
    }
    const amplified = quantity * (1 + npcStats.fear * DEFAULTS.AMPLIFICATION_FACTOR * repAmplify * fearScale);
    distorted.truth_value.quantity = clamp(
      Math.round(lerp(quantity, amplified, DEFAULTS.DAMPING_FACTOR)),
      DEFAULTS.MIN_Q,
      DEFAULTS.MAX_Q
    );
    pushRule(distorted.metadata, "fear_qty_amp");
  } else if (faithfulQty && tv.is_countable) {
    distorted.truth_value.quantity = quantity;
    pushRule(distorted.metadata, "faithful_qty_preserve");
  }

  if (!isStateLike && npcStats.hostility > DEFAULTS.HOSTILITY_THRESHOLD) {
    const hostilityScale = repDampen;
    const canHostilityRemap =
      (actionType === "threat" || actionType === "tactical_move") && !isConservativeMode;
    if (
      canHostilityRemap &&
      !isAlreadyHostileAction(tv.action) &&
      !isNeutralStateAction(tv.action) &&
      subjectRep < DEFAULTS.HIGH_REPUTATION
    ) {
      distorted.truth_value.action = mapToNegativeAction(tv.action);
      pushRule(distorted.metadata, "hostility_action_remap");
    } else if (subjectRep >= DEFAULTS.HIGH_REPUTATION) {
      pushRule(distorted.metadata, "reputation_skip_hostility_remap");
    }
    if (hostilityScale < 1 && canHostilityRemap) {
      pushRule(distorted.metadata, "reputation_dampen_hostility");
    }
    if (repAmplify > 1 && canHostilityRemap && subjectRep <= DEFAULTS.LOW_REPUTATION) {
      pushRule(distorted.metadata, "reputation_amplify_hostility");
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
    pushRule(distorted.metadata, "low_relation_trust");
  }

  if (actionType === "threat" && subjectRep >= DEFAULTS.HIGH_REPUTATION) {
    baseCertainty = clamp(Number((baseCertainty + 0.08).toFixed(2)), DEFAULTS.MIN_CERTAINTY, DEFAULTS.MAX_CERTAINTY);
    pushRule(distorted.metadata, "reputation_boost_threat_certainty");
  }

  if (subjectRep <= DEFAULTS.LOW_REPUTATION && !isStateLike) {
    baseCertainty = clamp(Number((baseCertainty * 0.93).toFixed(2)), DEFAULTS.MIN_CERTAINTY, DEFAULTS.MAX_CERTAINTY);
    pushRule(distorted.metadata, "reputation_lower_certainty");
  }

  if (distorted.metadata.rumor) {
    const rumorScale = clamp(Number(npcStats.rumorWeight ?? 1), 0.7, 1.5);
    baseCertainty = clamp(
      Number((baseCertainty * (0.82 / rumorScale)).toFixed(2)),
      DEFAULTS.MIN_CERTAINTY,
      DEFAULTS.MAX_CERTAINTY
    );
    distorted.metadata.source = "불명확한 출처";
    pushRule(distorted.metadata, "rumor_source_unclear");
    if (rumorScale > 1.1) pushRule(distorted.metadata, "persona_rumor_skeptic");
  }

  if (
    !isStateLike &&
    npcStats.fear > DEFAULTS.FEAR_THRESHOLD &&
    !isConservativeMode &&
    tv.target
  ) {
    const targetW = clamp(Number(npcStats.targetDistortWeight ?? 1), 0.5, 1.5);
    distorted.truth_value.target = transformTargetUnderDistress(tv.target, npcStats.fear, targetW);
    if (distorted.truth_value.target !== tv.target) {
      pushRule(distorted.metadata, "fear_target_distort");
    }
  }

  if (Number(npcStats.trustDegradeMultiplier || 1) < 1) {
    baseCertainty = clamp(
      Number((baseCertainty * Number(npcStats.trustDegradeMultiplier)).toFixed(2)),
      DEFAULTS.MIN_CERTAINTY,
      DEFAULTS.MAX_CERTAINTY
    );
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
  const faithfulQty = isFaithfulQuantityMode(receiverStats);

  if (tv.is_countable && !isStateLike && !faithfulQty) {
    interpreted.truth_value.quantity = clamp(
      Math.round(quantity * credulityBoost),
      DEFAULTS.MIN_Q,
      DEFAULTS.MAX_Q
    );
    pushRule(interpreted.metadata, "credulity_qty_amp");
  } else if (faithfulQty && tv.is_countable) {
    interpreted.truth_value.quantity = quantity;
    pushRule(interpreted.metadata, "faithful_qty_preserve");
  }

  let certainty = Number(tv.certainty ?? 0.5);
  certainty = clamp(
    Number((certainty * (0.75 + receiverStats.credulity * 0.4)).toFixed(2)),
    DEFAULTS.MIN_CERTAINTY,
    DEFAULTS.MAX_CERTAINTY
  );
  pushRule(interpreted.metadata, "credulity_certainty_blend");

  if (actionType === "threat" && subjectRep >= DEFAULTS.HIGH_REPUTATION) {
    certainty = clamp(Number((certainty + 0.05).toFixed(2)), DEFAULTS.MIN_CERTAINTY, DEFAULTS.MAX_CERTAINTY);
    pushRule(interpreted.metadata, "reputation_boost_threat_certainty");
  }

  if (subjectRep <= DEFAULTS.LOW_REPUTATION && !isStateLike) {
    certainty = clamp(Number((certainty * 1.06).toFixed(2)), DEFAULTS.MIN_CERTAINTY, DEFAULTS.MAX_CERTAINTY);
    pushRule(interpreted.metadata, "reputation_boost_distortion_belief");
  }

  if (receiverStats.trust < 0.4) {
    certainty = clamp(Number((certainty * 0.92).toFixed(2)), DEFAULTS.MIN_CERTAINTY, DEFAULTS.MAX_CERTAINTY);
    pushRule(interpreted.metadata, "receiver_low_trust");
  }

  if (Number(receiverStats.trustDegradeMultiplier || 1) < 1) {
    certainty = clamp(
      Number((certainty * Number(receiverStats.trustDegradeMultiplier)).toFixed(2)),
      DEFAULTS.MIN_CERTAINTY,
      DEFAULTS.MAX_CERTAINTY
    );
    pushRule(interpreted.metadata, "partial_trust_degrade");
  }

  // State reinterpretation under distress:
  // Calm-looking states may be recolored into anxious/tense interpretations for suspicious & credulous receivers.
  if (isStateLike) {
    const actionText = String(tv.action || "");
    const isNeutralCalm =
      /(태연|침착|평온|조용|한산)/.test(actionText) && !/(징후|이상|없)/.test(actionText);
    if (
      isNeutralCalm &&
      receiverStats.hostility > DEFAULTS.HOSTILITY_THRESHOLD &&
      receiverStats.credulity > 0.7
    ) {
      const remapped =
        actionText.indexOf("태연") >= 0
          ? "초조해 보인다"
          : actionText.indexOf("침착") >= 0 || actionText.indexOf("평온") >= 0
            ? "긴장한 상태다"
            : "불안한 상태다";
      if (remapped && remapped !== tv.action) {
        tv.action = remapped;
        pushRule(interpreted.metadata, "state_reinterpret_unsettled");
      }
    }
  }

  interpreted.truth_value.certainty = certainty;
  interpreted.metadata.last_updated_tick += 1;
  return interpreted;
}

function normalizePlaceKey(target) {
  return String(target || "")
    .replace(/\(.*?\)/g, "")
    .trim()
    .toLowerCase();
}

function applyBundleCoherence(interpretedFacts) {
  if (!interpretedFacts || interpretedFacts.length < 2) {
    return { interpretedFacts, bundleContext: { hasContradiction: false, notes: [] } };
  }

  const urgentFacts = interpretedFacts.filter((item) => {
    const type = item.truth_value.action_type;
    return type === "threat" || type === "tactical_move";
  });
  const calmStates = interpretedFacts.filter((item) => {
    const tv = item.truth_value;
    return tv.action_type === "state" && isNeutralStateAction(tv.action);
  });

  const notes = [];
  let hasContradiction = false;

  const placeTokens = [
    "북문",
    "남문",
    "마을",
    "숲",
    "동물원",
    "궁정",
    "시장",
    "주막",
    "대장간",
    "약초원",
    "동굴",
    "항구",
    "성문",
    "탑",
    "지하실",
    "농장",
    "연못",
    "교량",
  ];
  function guessPlaceFromText(text) {
    const t = String(text || "");
    for (let i = 0; i < placeTokens.length; i += 1) {
      if (t.indexOf(placeTokens[i]) >= 0) return normalizePlaceKey(placeTokens[i]);
    }
    return "";
  }

  function preferPlaceFromTarget(targetKey, subjectKey) {
    var t = String(targetKey || "");
    // generic fallback targets should defer to subject-based place extraction.
    if (
      !t ||
      t === "현장" ||
      t === "일상 관찰" ||
      t === "원문 서술" ||
      t === "알 수 없는 장소"
    ) {
      return subjectKey || "";
    }
    return t;
  }

  const urgentPlaceKeys = urgentFacts
    .map((f) => {
      var targetKey = normalizePlaceKey(f.truth_value.target);
      var subjectPlace = guessPlaceFromText(f.truth_value.subject);
      return preferPlaceFromTarget(targetKey, subjectPlace);
    })
    .filter(Boolean);
  function placeMatches(urgentKey, calmKey) {
    if (!urgentKey || !calmKey) return false;
    if (urgentKey === calmKey) return true;
    // substring match: e.g. urgentKey="동물원 전 구역" calmKey="동물원"
    return urgentKey.indexOf(calmKey) >= 0 || calmKey.indexOf(urgentKey) >= 0;
  }

  const samePlaceCalm = calmStates.filter((c) => {
    var targetKey = normalizePlaceKey(c.truth_value.target);
    var subjectPlace = guessPlaceFromText(c.truth_value.subject);
    var calmKey = preferPlaceFromTarget(targetKey, subjectPlace);
    return urgentPlaceKeys.some((u) => placeMatches(u, calmKey));
  });

  if (urgentFacts.length > 0 && calmStates.length > 0) {
    hasContradiction = true;
    notes.push("bundle_threat_vs_calm_state");

    for (let i = 0; i < calmStates.length; i += 1) {
      const tv = calmStates[i].truth_value;
      const samePlace = samePlaceCalm.indexOf(calmStates[i]) >= 0;
      tv.certainty = clamp(
        Number((tv.certainty * (samePlace ? 0.55 : 0.72)).toFixed(2)),
        DEFAULTS.MIN_CERTAINTY,
        DEFAULTS.MAX_CERTAINTY
      );
      pushRule(calmStates[i].metadata, samePlace ? "bundle_same_place_calm_denial" : "bundle_contradiction_calm_down");
      calmStates[i].metadata.bundle_notes = calmStates[i].metadata.bundle_notes || [];
      calmStates[i].metadata.bundle_notes.push(
        samePlace
          ? "같은 장소에서 위협과 평온 서술이 충돌하여 평온 쪽 확신도를 크게 하향"
          : "위협 정보와 정서가 충돌하여 확신도 하향"
      );
      if (samePlace) {
        tv.action = "상황이 혼란스러워 판단을 유보한다";
        pushRule(calmStates[i].metadata, "bundle_calm_action_suppressed");
      }
    }

    for (let j = 0; j < urgentFacts.length; j += 1) {
      const item = urgentFacts[j];
      const tv = item.truth_value;
      pushRule(item.metadata, "bundle_threat_priority");
      if (samePlaceCalm.length > 0 && tv.is_countable) {
        const cap = Math.max(1, Math.round(Number(tv.quantity || 1) * DEFAULTS.BUNDLE_THREAT_QTY_CAP));
        if (cap < tv.quantity) {
          tv.quantity = cap;
          pushRule(item.metadata, "bundle_threat_qty_capped");
          notes.push("bundle_threat_qty_capped");
        }
      }
    }
  }

  return { interpretedFacts, bundleContext: { hasContradiction, notes, samePlaceConflict: samePlaceCalm.length > 0 } };
}

function propagateInformation(sender, receiver, originalInfo) {
  return propagateFactBundle(sender, receiver, originalInfo, null);
}

function propagateFactBundle(sender, receiver, originalInfo, factsInput, options) {
  const opts = options || {};
  const trustLevel = sender.getTrustLevel(receiver.id);
  let trustDegradeMultiplier = 1;
  let partialTrust = false;

  if (trustLevel < DEFAULTS.TRUST_BLOCK) {
    if (opts.allowPartialTrust && trustLevel >= DEFAULTS.TRUST_PARTIAL_MIN) {
      partialTrust = true;
      trustDegradeMultiplier = clamp(trustLevel / DEFAULTS.TRUST_BLOCK, 0.35, 0.95);
    } else {
      return {
        blocked: true,
        reason: `Trust_Level ${trustLevel.toFixed(2)} < ${DEFAULTS.TRUST_BLOCK.toFixed(2)}`,
        senderDistorted: null,
        receiverInterpreted: null,
        distortedFacts: [],
        interpretedFacts: [],
        bundleContext: { hasContradiction: false, notes: [] },
        auditTrail: [],
      };
    }
  }

  const quantityMode = opts.quantityMode === "faithful" ? "faithful" : "dramatic";

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
  const auditDiff = [];
  const currentTick = Number(opts.currentTick ?? originalInfo.metadata.creation_tick ?? 0);

  const senderWeighted = applyPersonaWeights(sender.stats, sender.persona, "sender");
  const receiverWeighted = applyPersonaWeights(receiver.stats, receiver.persona, "receiver");

  for (let i = 0; i < sourceFacts.length; i += 1) {
    const fact = sourceFacts[i];
    const atom = factInputToAtom(fact, originalInfo, i);
    const inputSnapshot = snapshotTruth(atom.truth_value);

    const senderContext = {
      ...senderWeighted,
      name: sender.name,
      persona: sender.persona,
      relationTrust: trustLevel,
      subjectReputation: sender.getSubjectReputation(fact.subject),
      quantityMode,
      trustDegradeMultiplier,
    };
    const receiverContext = {
      ...receiverWeighted,
      persona: receiver.persona,
      subjectReputation: receiver.getSubjectReputation(fact.subject),
      quantityMode,
      trustDegradeMultiplier,
    };

    const senderDistorted = distortInformation(atom, senderContext);
    const receiverInterpreted = reinterpretInfo(senderDistorted, receiverContext);
    receiverInterpreted.metadata.source_chain = [
      ...(atom.metadata.source_chain || []),
      sender.name,
      receiver.name,
    ];
    distortedFacts.push(senderDistorted);
    interpretedFacts.push(receiverInterpreted);

    auditDiff.push({
      info_id: receiverInterpreted.info_id,
      input: inputSnapshot,
      distorted: snapshotTruth(senderDistorted.truth_value),
      interpreted: snapshotTruth(receiverInterpreted.truth_value),
      applied_rules: [],
      bundle_notes: [],
    });
  }

  const bundleResult = applyBundleCoherence(interpretedFacts);
  const finalInterpreted = bundleResult.interpretedFacts;
  for (let sync = 0; sync < finalInterpreted.length; sync += 1) {
    receiver.knowledgeBase.update(finalInterpreted[sync], currentTick + sync);
  }
  const auditTrail = [];
  for (let a = 0; a < finalInterpreted.length; a += 1) {
    const item = finalInterpreted[a];
    const dist = distortedFacts[a];
    const mergedRules = [
      ...new Set([...(dist?.metadata?.applied_rules || []), ...(item.metadata.applied_rules || [])]),
    ];
    item.metadata.applied_rules = mergedRules;
    if (auditDiff[a]) {
      auditDiff[a].applied_rules = mergedRules;
      auditDiff[a].bundle_notes = item.metadata.bundle_notes || [];
      auditDiff[a].interpreted = snapshotTruth(item.truth_value);
    }
    auditTrail.push({
      info_id: item.info_id,
      applied_rules: mergedRules,
      bundle_notes: item.metadata.bundle_notes || [],
    });
  }

  return {
    blocked: false,
    reason: partialTrust
      ? `partial_trust ${trustLevel.toFixed(2)} (degrade x${trustDegradeMultiplier.toFixed(2)})`
      : null,
    partialTrust,
    quantityMode,
    senderDistorted: distortedFacts[0] || null,
    receiverInterpreted: finalInterpreted[0] || null,
    distortedFacts,
    interpretedFacts: finalInterpreted,
    bundleContext: bundleResult.bundleContext,
    auditTrail,
    auditDiff,
  };
}

function propagateChain(hops, facts, originalInfo, options) {
  const opts = options || {};
  if (!hops || hops.length === 0) {
    return { hopResults: [], finalFacts: facts || [], blocked: false, auditDiff: [] };
  }

  let currentFacts = (facts || []).map((f) => ({ ...f }));
  const hopResults = [];
  let allAuditDiff = [];
  let blocked = false;

  for (let h = 0; h < hops.length; h += 1) {
    const hop = hops[h];
    const hopTick = Number(opts.currentTick ?? 0) + h;
    const info =
      originalInfo ||
      new InfoAtom({
        info_id: "INF_CHAIN",
        truth_value: { subject: "info", action: "spread", target: "현장", quantity: 1, certainty: 0.8, facts: [] },
        metadata: { origin: hop.sender.name, source: hop.sender.name, creation_tick: hopTick },
      });
    info.metadata.creation_tick = hopTick;
    info.metadata.last_updated_tick = hopTick;

    const result = propagateFactBundle(hop.sender, hop.receiver, info, currentFacts, {
      ...opts,
      currentTick: hopTick,
    });

    hopResults.push({
      hop: h + 1,
      from: hop.sender.name,
      to: hop.receiver.name,
      propagation: result,
    });

    if (result.auditDiff) allAuditDiff = allAuditDiff.concat(result.auditDiff);

    if (result.blocked) {
      blocked = true;
      break;
    }

    currentFacts = interpretedToFactInputs(result.interpretedFacts).map((f) => {
      f.certainty = clamp(
        Number((Number(f.certainty || 0.5) * DEFAULTS.HOP_CERTAINTY_DECAY).toFixed(2)),
        DEFAULTS.MIN_CERTAINTY,
        DEFAULTS.MAX_CERTAINTY
      );
      f.source_chain = [...(f.source_chain || []), hop.receiver.name];
      return f;
    });
  }

  return {
    hopResults,
    finalFacts: currentFacts,
    blocked,
    auditDiff: allAuditDiff,
    bundleContext: hopResults.length ? hopResults[hopResults.length - 1].propagation.bundleContext : {},
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
  const rawFacts = distortedBundle.facts || [];
  const facts = rawFacts.map((f) => {
    if (f.applied_rules !== undefined && f.subject !== undefined) {
      return f;
    }
    return toGroundedFact(f, {
      applied_rules: f.applied_rules,
      bundle_notes: f.bundle_notes,
      rumor: f.rumor,
    });
  });
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
  const tension =
    groundedContext.bundleContext && groundedContext.bundleContext.hasContradiction
      ? " (다만 일부 정보는 서로 맞지 않아 보인다고 덧붙이며)"
      : "";
  const speech =
    facts.length > 1
      ? `${style} 말한다: "${speechLines.join(" 그리고 ")}"${tension} (출처: ${source})`
      : `${style} 말한다: "${speechLines[0] || "정보가 전달되지 않았다"}"${tension} (출처: ${source})`;

  return [
    `[System Guardrail] ${systemPrompt}`,
    `[Grounded DATA]\n${JSON.stringify(groundedContext.dataOnly, null, 2)}`,
    `[NPC Speech]\n${speech}`,
  ].join("\n");
}

function resolveLlmAdapter() {
  if (typeof window !== "undefined" && window.LlmAdapter) return window.LlmAdapter;
  if (typeof globalThis !== "undefined" && globalThis.LlmAdapter) return globalThis.LlmAdapter;
  return null;
}

function generateNPCDialogue(distortedFactsBundle, npcPersona, bundleContext) {
  const systemPrompt = createSoftGuardrailPrompt(npcPersona);
  const context = buildContextGrounding(distortedFactsBundle);
  const bundleCtx = bundleContext || { hasContradiction: false, notes: [] };
  context.bundleContext = bundleCtx;

  const adapter = resolveLlmAdapter();
  const llmParams = { systemPrompt, groundedContext: context, persona: npcPersona, bundleContext: bundleCtx };

  if (adapter && adapter.isLive && adapter.isLive()) {
    const placeholder = adapter.mockGenerate(llmParams);
    return {
      systemPrompt,
      context,
      finalSpeech: placeholder.finalSpeech,
      engineData: context.dataOnly,
      npcSpeech: placeholder.npcSpeech,
      llmProvider: "pending",
      llmPending: true,
    };
  }

  if (adapter && typeof adapter.generateSync === "function") {
    const speechOut = adapter.generateSync(llmParams);
    return {
      systemPrompt,
      context,
      finalSpeech: speechOut.finalSpeech,
      engineData: context.dataOnly,
      npcSpeech: speechOut.npcSpeech,
      llmProvider: speechOut.provider || "mock",
      llmPending: false,
    };
  }

  const finalSpeech = mockLLMGenerate(systemPrompt, context, npcPersona);
  return {
    systemPrompt,
    context,
    finalSpeech,
    engineData: context.dataOnly,
    npcSpeech: finalSpeech.split("[NPC Speech]\n")[1] || finalSpeech,
    llmProvider: "mock",
    llmPending: false,
  };
}

function compareWithGroundTruth(interpretedFacts, groundTruthFacts, options) {
  if (!groundTruthFacts || !groundTruthFacts.length) return { matches: [], score: null };
  const opts = options || {};
  const quantityMode = String(opts.quantityMode || "dramatic").toLowerCase();

  const matches = [];
  let sum = 0;

  function subjectSim(actualSubject, gtSubject) {
    const a = String(actualSubject || "");
    const g = String(gtSubject || "");
    if (!a || !g) return 0;
    return a.includes(g) || g.includes(a) ? 1 : 0;
  }

  function typeSim(actualType, gtType) {
    if (!gtType) return 0.9;
    return actualType === gtType ? 1 : 0;
  }

  function quantitySim(actualQty, gt) {
    if (!gt.is_countable) return 0.8;
    const a = Number(actualQty ?? 1);
    const g = Number(gt.quantity ?? 1);
    const delta = Math.abs(a - g);
    if (delta === 0) return 1;
    if (quantityMode === "faithful") return 0;
    // dramatic mode: allow some quantity drift
    const allowed = Math.max(1, Math.round(g * 0.5));
    const raw = 1 - delta / (allowed + 1);
    return clamp(raw, 0, 0.95);
  }

  for (let i = 0; i < interpretedFacts.length; i += 1) {
    const item = interpretedFacts[i];
    const tv = item.truth_value || {};

    // Best-match among all ground truth candidates.
    let bestScore = -1;
    let bestGt = null;
    for (let j = 0; j < groundTruthFacts.length; j += 1) {
      const gt = groundTruthFacts[j];
      const s = 0.5 * subjectSim(tv.subject, gt.subject);
      const t = 0.3 * typeSim(tv.action_type, gt.action_type);
      const q = 0.2 * quantitySim(tv.quantity, gt);
      const sc = s + t + q;
      if (sc > bestScore) {
        bestScore = sc;
        bestGt = gt;
      }
    }

    const aligned = bestScore >= 0.72;
    sum += bestScore < 0 ? 0 : bestScore;

    matches.push({
      info_id: item.info_id,
      aligned,
      score: Number(bestScore.toFixed(2)),
      expected: bestGt ? snapshotTruth(bestGt) : null,
      actual: snapshotTruth(tv),
    });
  }

  const avg = interpretedFacts.length ? sum / interpretedFacts.length : 0;
  return { matches, score: Number(avg.toFixed(2)), hit: null, total: interpretedFacts.length };
}

function getScenario(overrides = {}) {
  const key = String(overrides.sessionKey || "default");
  if (overrides.persistSession) {
    if (!SESSION_STORE.has(key)) {
      SESSION_STORE.set(key, createBaseScenario(overrides));
    }
    return SESSION_STORE.get(key);
  }
  return createBaseScenario(overrides);
}

function createBaseScenario(overrides = {}) {
  const senderKey = overrides.senderProfileKey || "guard";
  const receiverKey = overrides.receiverProfileKey || "merchant";
  const sender = overrides.usePlayerAsSender
    ? createPlayerActor()
    : createNpcFromProfile(senderKey);
  const receiver = createNpcFromProfile(receiverKey);
  const trust = resolveTrustToReceiver(sender, receiver, receiverKey, overrides);
  sender.setTrustLevel(receiver.id, trust);

  const playerName = getProfileRegistry().player.displayName;
  const info = new InfoAtom({
    info_id: "INF_001",
    truth_value: {
      subject: "밀수 화물",
      action: "버려져 있다",
      target: "안개 계곡",
      object: "",
      quantity: 3,
      is_factual: true,
      certainty: 1.0,
      facts: [],
    },
    metadata: {
      origin: overrides.usePlayerAsSender ? playerName : sender.name,
      source: overrides.usePlayerAsSender ? playerName : sender.name,
      creation_tick: 1250,
    },
  });

  return { info, sender, receiver };
}

function executeScenario(overrides = {}) {
  let { info, sender, receiver } = getScenario(overrides);

  if (overrides.usePlayerAsSender) {
    const receiverKey = overrides.receiverProfileKey || "merchant";
    sender = createPlayerActor();
    receiver = createNpcFromProfile(receiverKey);
    const trust = resolveTrustToReceiver(sender, receiver, receiverKey, overrides);
    sender.setTrustLevel(receiver.id, trust);
    const playerName = getProfileRegistry().player.displayName;
    info.metadata.origin = playerName;
    info.metadata.source = playerName;
  } else if (overrides.receiverProfileKey || overrides.senderProfileKey) {
    const receiverKey = overrides.receiverProfileKey || receiver.profileKey || "merchant";
    const senderKey = overrides.senderProfileKey || sender.profileKey || "guard";
    sender = createNpcFromProfile(senderKey);
    receiver = createNpcFromProfile(receiverKey);
    const trust = resolveTrustToReceiver(sender, receiver, receiverKey, overrides);
    sender.setTrustLevel(receiver.id, trust);
  }

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
      object: primary.object || "",
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

  const currentTick = Number(overrides.currentTick ?? info.metadata.creation_tick ?? 0);
  info.metadata.creation_tick = currentTick;
  info.metadata.last_updated_tick = currentTick;

  const propagation = propagateFactBundle(sender, receiver, info, overrides.facts || null, {
    quantityMode: overrides.quantityMode === "faithful" ? "faithful" : "dramatic",
    allowPartialTrust: overrides.allowPartialTrust !== false,
    currentTick,
  });

  const groundTruthReport = overrides.groundTruth
    ? compareWithGroundTruth(propagation.interpretedFacts || [], overrides.groundTruth, {
        quantityMode: overrides.quantityMode === "faithful" ? "faithful" : "dramatic",
      })
    : null;

  let dialogue = null;
  if (!propagation.blocked && propagation.interpretedFacts.length) {
    const interpretedFacts = propagation.interpretedFacts.map((item) =>
      toGroundedFact(item.truth_value, {
        applied_rules: item.metadata.applied_rules || [],
        bundle_notes: item.metadata.bundle_notes || [],
        rumor: item.metadata.rumor,
      })
    );
    interpretedFacts.forEach((f, i) => {
      f.fact_id = propagation.interpretedFacts[i].info_id;
    });
    dialogue = generateNPCDialogue(
      {
        info_id: info.info_id,
        source: propagation.interpretedFacts[0].metadata.source,
        facts: interpretedFacts,
      },
      receiver.persona,
      propagation.bundleContext
    );
  }

  let playerReputationSnapshot = null;
  if (typeof window !== "undefined" && window.ReputationSystem && overrides.usePlayerAsSender) {
    playerReputationSnapshot = window.ReputationSystem.snapshot(
      overrides.reputationSessionKey || overrides.sessionKey || "default"
    );
  }

  return {
    baseInfo: info,
    sender,
    receiver,
    propagation,
    dialogue,
    playerReputationSnapshot,
    knowledgeBaseSnapshot: overrides.persistSession
      ? receiver.knowledgeBase.listWithDecay(currentTick)
      : receiver.knowledgeBase.list(),
    facts: overrides.facts || info.truth_value.facts || [],
    auditTrail: propagation.auditTrail || [],
    auditDiff: propagation.auditDiff || [],
    bundleContext: propagation.bundleContext || { hasContradiction: false, notes: [] },
    groundTruthReport,
    sessionKey: overrides.sessionKey || "default",
    persistSession: Boolean(overrides.persistSession),
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
  propagateChain,
  applyBundleCoherence,
  compareWithGroundTruth,
  generateNPCDialogue,
  executeScenario,
  getScenario,
  getProfileRegistry,
  registerProfiles,
  createNpcFromProfile,
  createPlayerActor,
  toGroundedFact,
  clearSession: (key) => SESSION_STORE.delete(String(key || "default")),
  DEFAULTS,
  PERSONA_DISTORTION_PROFILES,
};
