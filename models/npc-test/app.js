(function () {
  var engine = window.QuestSystem;
  var lexicon = window.NpcLexicon || {};
  var executeScenario = engine && engine.executeScenario;
  var KNOWN_ACTIONS = lexicon.knownActions || [];
  var ACTION_PATTERNS = lexicon.actionPatterns || [];
  var NON_COUNTABLE_PATTERNS = lexicon.nonCountablePatterns || [];
  var CLASSIFY_KEYWORDS = lexicon.classifyKeywords || {};
  var ACTION_NORMALIZE_MAP = lexicon.actionNormalizeMap || {};
  var WORLD_PLACES = lexicon.worldPlaces || [];
  var WORLD_ROLES = lexicon.worldRoles || [];

  var form = document.querySelector("#control-form");
  var runButton = document.querySelector("#run-pipeline");
  var outputBase = document.querySelector("#out-base");
  var outputSender = document.querySelector("#out-sender");
  var outputReceiver = document.querySelector("#out-receiver");
  var outputKb = document.querySelector("#out-kb");
  var outputKnowledgeLayers = document.querySelector("#out-knowledge-layers");
  var outputDialogue = document.querySelector("#out-dialogue");
  var outputDialogueMain = document.querySelector("#out-dialogue-main");
  var outputParsePipeline = document.querySelector("#out-parse-pipeline");
  var outputFacts = document.querySelector("#out-facts");
  var outputEngineData = document.querySelector("#out-engine-data");
  var outputTestResults = document.querySelector("#out-test-results");
  var outputAudit = document.querySelector("#out-audit");
  var outputAuditDiff = document.querySelector("#out-audit-diff");
  var outputGroundTruth = document.querySelector("#out-ground-truth");
  var outputChain = document.querySelector("#out-chain");
  var outputDistortionTests = document.querySelector("#out-distortion-tests");
  var outputIntegrationTests = document.querySelector("#out-integration-tests");
  var debugLog = document.querySelector("#debug-log");
  var clickIndicator = document.querySelector("#click-indicator");
  var runTestsButton = document.querySelector("#run-parser-tests");

  function appendDebug(message) {
    var line = "[" + new Date().toLocaleTimeString() + "] " + message;
    if (debugLog) {
      debugLog.textContent = debugLog.textContent + "\n" + line;
    }
    if (Object.prototype.toString.call(window.__NPC_DEBUG) === "[object Array]") {
      window.__NPC_DEBUG.push(line);
    }
    if (typeof window.__appendInlineDebug === "function") {
      window.__appendInlineDebug("app.js: " + message);
    }
  }

  function parseNumber(value, fallback) {
    var parsed = Number(value);
    return isFinite(parsed) ? parsed : fallback;
  }

  function formatJSON(value) {
    return JSON.stringify(value, null, 2);
  }

  function getFieldValue(name) {
    if (!form) return "";
    var field = form.querySelector('[name="' + name + '"]');
    return field ? field.value : "";
  }

  function setFieldValue(name, value) {
    if (!form) return;
    var field = form.querySelector('[name="' + name + '"]');
    if (field && typeof value === "string") {
      field.value = value;
    }
  }

  function saveFormState() {
    var payload = {
      scenarioText: getFieldValue("scenarioText"),
      senderFear: getFieldValue("senderFear"),
      senderHostility: getFieldValue("senderHostility"),
      senderTrust: getFieldValue("senderTrust"),
      receiverCredulity: getFieldValue("receiverCredulity"),
      receiverTrust: getFieldValue("receiverTrust"),
      trustLevel: getFieldValue("trustLevel"),
      subjectReputation: getFieldValue("subjectReputation"),
      senderReputation: getFieldValue("senderReputation"),
      receiverReputation: getFieldValue("receiverReputation"),
      quantityMode: getFieldValue("quantityMode"),
      allowPartialTrust: getFieldValue("allowPartialTrust"),
      persistSession: getFieldValue("persistSession"),
      runChain: getFieldValue("runChain"),
      groundTruthJson: getFieldValue("groundTruthJson"),
    };
    try {
      localStorage.setItem("npc-test-form-state", JSON.stringify(payload));
    } catch (error) {}
  }

  function restoreFormState() {
    var raw = null;
    try {
      raw = localStorage.getItem("npc-test-form-state");
    } catch (error) {
      return;
    }
    if (!raw) return;
    try {
      var parsed = JSON.parse(raw);
      var keys = Object.keys(parsed);
      for (var i = 0; i < keys.length; i += 1) {
        var key = keys[i];
        setFieldValue(key, String(parsed[key]));
      }
    } catch (error) {
      try {
        localStorage.removeItem("npc-test-form-state");
      } catch (nestedError) {}
    }
  }

  function detectRumorText(text) {
    return /(소문|전해|듣|카더라|따르면|듣자하니)/.test(String(text || ""));
  }

  function buildFactsFromParsed(parsed) {
    var list = parsed.sentenceParses && parsed.sentenceParses.length ? parsed.sentenceParses : [parsed];
    var facts = [];
    for (var i = 0; i < list.length; i += 1) {
      var f = list[i];
      var sourceText = f.source_text || parsed.source_text || f.raw_text || "";
      facts.push({
        fact_id: "F" + String(i + 1).padStart(2, "0"),
        subject: sanitizeKoreanNoun(f.subject || "대상"),
        action: normalizeActionPhrase(f.action || ""),
        object: sanitizeKoreanNoun(f.object || ""),
        target: sanitizeKoreanNoun(f.target || "현장"),
        quantity: Number(f.quantity || 1),
        certainty: Number(f.certainty || 0.75),
        is_countable: Boolean(f.is_countable),
        action_type: f.action_type || "unknown",
        parse_confidence: Number(f.parse_confidence || 0.5),
        parse_mode: f.parse_mode || "structured",
        raw_text: f.raw_text || "",
        source_text: sourceText,
        rumor: detectRumorText(sourceText),
      });
    }
    return facts;
  }

  function buildParsePipelineTrace(parsed, primaryFact, facts) {
    var lines = [];
    lines.push("=== Parse Pipeline ===");
    lines.push("raw: " + (parsed.raw_text || ""));
    lines.push("mode: " + parsed.parse_mode + ", confidence: " + parsed.parse_confidence);
    lines.push("sentences: " + (parsed.sentenceParses ? parsed.sentenceParses.length : 1));
    for (var i = 0; i < (parsed.sentenceParses || []).length; i += 1) {
      var s = parsed.sentenceParses[i];
      lines.push(
        "  [" +
          (i + 1) +
          "] " +
          (s.raw_text || "") +
          " -> " +
          s.subject +
          " / " +
          s.action +
          " / " +
          s.target +
          (s.object ? " / object: " + s.object : "") +
          " (" +
          s.action_type +
          ")"
      );
    }
    var primaryLine =
      "primary: " + primaryFact.subject + " | " + primaryFact.action + " | " + primaryFact.target + (primaryFact.object ? " | object: " + primaryFact.object : "");
    if (facts && facts.length > 1) {
      var related = null;
      for (var r = 0; r < facts.length; r += 1) {
        var f = facts[r];
        if (
          f.subject !== primaryFact.subject &&
          (f.action_type === "tactical_move" || f.action_type === "threat") &&
          (f.is_countable || Number(f.quantity || 0) > 1)
        ) {
          related = f;
          break;
        }
      }
      if (related) {
        primaryLine +=
          " | linked: " +
          related.subject +
          " 수량 " +
          String(related.quantity || 1) +
          " (" +
          related.action +
          " @ " +
          related.target +
          ")";
      }
    }
    lines.push(primaryLine);
    lines.push("facts count: " + facts.length);
    return lines.join("\n");
  }

  function buildReadableDialogue(result) {
    if (result.propagation.blocked) {
      return "정보 전파 차단: " + result.propagation.reason;
    }
    var interpreted = result.propagation.interpretedFacts || [];
    if (!interpreted.length && result.propagation.receiverInterpreted) {
      interpreted = [result.propagation.receiverInterpreted];
    }
    var lines = [];
    for (var i = 0; i < interpreted.length; i += 1) {
      var item = interpreted[i];
      var d = item.truth_value;
      var source = item.metadata.source;
      var subjectText = sanitizeKoreanNoun(String(d.subject || "대상"));
      var actionText = normalizeActionPhrase(String(d.action || ""));
      var naturalAction = /(다|했다|중이다|보인다|상태다)$/.test(actionText) ? actionText : actionText + "했다";
      var targetText = sanitizeKoreanNoun(String(d.target || "현장"));
      var quantityText = d.is_countable ? String(d.quantity) : "해당 없음";
      lines.push(
        "[" +
          (item.info_id || "F" + (i + 1)) +
          "] " +
          subjectText +
          pickKoreanParticle(subjectText, "이", "가") +
          " " +
          naturalAction +
          ". 상황 맥락: " +
          targetText +
          ", 수량: " +
          quantityText +
          ", 확신도: " +
          d.certainty +
          " (" +
          source +
          ")"
      );
    }
    return lines.join("\n");
  }

  function pickKoreanParticle(word, withBatchim, withoutBatchim) {
    var text = String(word || "").trim();
    if (!text) return withoutBatchim;
    var lastChar = text.charAt(text.length - 1);
    var code = lastChar.charCodeAt(0);
    var HANGUL_BASE = 44032;
    var HANGUL_END = 55203;
    if (code < HANGUL_BASE || code > HANGUL_END) {
      return withoutBatchim;
    }
    var hasBatchim = ((code - HANGUL_BASE) % 28) !== 0;
    return hasBatchim ? withBatchim : withoutBatchim;
  }

  function sanitizeKoreanNoun(text) {
    var cleaned = String(text || "").trim();
    cleaned = cleaned.replace(/[,.!?]$/g, "");
    // Keep lexical endings like "마을", "겨울", "서울"; strip only topic/subject/genitive particles.
    return cleaned.replace(/(이|가|은|는|의)$/g, "");
  }

  function normalizeUserText(text) {
    var normalized = String(text || "").replace(/\u00A0/g, " ");
    normalized = normalized.replace(/\s+/g, " ").trim();
    return normalized;
  }

  function splitIntoSentences(fullText) {
    var t = normalizeUserText(fullText);
    if (!t) return [];
    // Split not only by punctuation, but also by common Korean connective clauses.
    var chunks = t
      .replace(/,\s*(그런데|하지만|그리고|그러나|한편)\s+/g, ". $1 ")
      .replace(/,\s+/g, ". ")
      .replace(/\s+(는데|지만|며)\s+/g, ". ")
      .split(/(?<=[.!?…])\s+/);
    var out = [];
    for (var i = 0; i < chunks.length; i += 1) {
      var s = normalizeUserText(chunks[i]);
      if (s) out.push(s);
    }
    if (out.length === 0) return [t];
    return out;
  }

  function actionTypePriority(type) {
    var order = { threat: 4, tactical_move: 3, routine: 2, state: 1, unknown: 0 };
    return order[type] != null ? order[type] : 0;
  }

  function selectPrimaryParse(parsed) {
    if (!parsed || !parsed.sentenceParses || parsed.sentenceParses.length === 0) return parsed;
    var list = parsed.sentenceParses;
    var best = list[0];
    var bestScore = -1;
    for (var i = 0; i < list.length; i += 1) {
      var item = list[i];
      var score = actionTypePriority(item.action_type) * 10 + (item.has_explicit_quantity ? 2 : 0) + (item.is_countable ? 1 : 0);
      if (score > bestScore) {
        bestScore = score;
        best = item;
      }
    }
    return best;
  }

  function mergeParsedSentences(list) {
    if (!list || list.length === 0) {
      return {
        subject: "밀수 화물",
        quantity: 1,
        action: "버려져 있다",
        target: "안개 계곡",
        certainty: 0.75,
        is_countable: false,
        action_type: "state",
        raw_text: "",
        has_explicit_quantity: false,
        parse_confidence: 0.5,
        parse_mode: "structured",
        sentenceParses: [],
      };
    }
    if (list.length === 1) {
      var only = list[0];
      only.sentenceParses = [JSON.parse(JSON.stringify(only))];
      only.facts = buildFactsFromParsed(only);
      return only;
    }

    var subjects = [];
    var actions = [];
    var targets = [];
    var quantities = [];
    var certainties = [];
    var confidences = [];
    var anyCountable = false;
    var anyConservative = false;
    var bestType = "unknown";
    var bestPrio = -1;
    var rawParts = [];
    var hasExplicitAny = false;

    for (var i = 0; i < list.length; i += 1) {
      var p = list[i];
      rawParts.push(p.raw_text || "");
      if (p.subject) subjects.push(String(p.subject));
      if (p.action) actions.push(String(p.action));
      if (p.target && p.target !== "알 수 없는 장소") targets.push(String(p.target));
      if (p.is_countable && isFinite(p.quantity)) quantities.push(Number(p.quantity));
      if (isFinite(p.certainty)) certainties.push(Number(p.certainty));
      if (isFinite(p.parse_confidence)) confidences.push(Number(p.parse_confidence));
      if (p.is_countable) anyCountable = true;
      if (p.parse_mode === "conservative_raw") anyConservative = true;
      if (p.has_explicit_quantity) hasExplicitAny = true;
      var pr = actionTypePriority(p.action_type);
      if (pr > bestPrio) {
        bestPrio = pr;
        bestType = p.action_type || "unknown";
      }
    }

    var subjectMerged = subjects[0] || "밀수 화물";
    var allSameSubject = true;
    for (var s = 1; s < subjects.length; s += 1) {
      if (subjects[s] !== subjects[0]) {
        allSameSubject = false;
        break;
      }
    }
    if (!allSameSubject && subjects.length > 1) {
      subjectMerged = subjectMerged + " 등";
    }

    var actionMerged = actions.filter(Boolean).join(" · ");
    var targetMerged = targets.length ? targets.join(" / ") : list[0].target || "알 수 없는 장소";

    var qtyMerged = 1;
    if (anyCountable && quantities.length) {
      qtyMerged = Math.max.apply(null, quantities);
    } else if (quantities.length) {
      qtyMerged = Math.max.apply(null, quantities);
    }

    var certMerged = certainties.length ? Math.min.apply(null, certainties) : 0.75;
    var confAvg =
      confidences.length > 0
        ? Number(
            (
              confidences.reduce(function (a, b) {
                return a + b;
              }, 0) / confidences.length
            ).toFixed(2)
          )
        : 0.6;

    var modeMerged = anyConservative || confAvg < 0.6 ? "conservative_raw" : "structured";
    var merged = {
      subject: subjectMerged,
      quantity: qtyMerged,
      action: actionMerged || list[0].action,
      target: targetMerged,
      certainty: certMerged,
      is_countable: anyCountable,
      action_type: bestType,
      raw_text: rawParts.join(" "),
      has_explicit_quantity: hasExplicitAny,
      parse_confidence: confAvg,
      parse_mode: modeMerged,
      sentenceParses: list.map(function (x) {
        return JSON.parse(JSON.stringify(x));
      }),
    };

    if (modeMerged === "conservative_raw") {
      merged.action = rawParts.join(" ");
      merged.is_countable = false;
      merged.quantity = 1;
      merged.target = "복합 입력";
    }

    merged.facts = buildFactsFromParsed(merged);
    return merged;
  }

  function detectUncertainty(rawText) {
    var text = String(rawText || "");
    var lowCertaintyPatterns = [
      /같다/,
      /같아/,
      /추정/,
      /아마/,
      /듯/,
      /처럼 보/,
      /소문/,
      /카더라/,
      /전해/,
      /뿐/,
      /인 것 같/,
    ];
    for (var i = 0; i < lowCertaintyPatterns.length; i += 1) {
      if (lowCertaintyPatterns[i].test(text)) return 0.4;
    }
    return 0.75;
  }

  function isCountableScenario(actionText, rawText) {
    var text = String(actionText || "") + " " + String(rawText || "");
    for (var i = 0; i < NON_COUNTABLE_PATTERNS.length; i += 1) {
      if (NON_COUNTABLE_PATTERNS[i].test(text)) return false;
    }
    return true;
  }

  function classifyActionType(actionText, rawText) {
    var actionOnly = String(actionText || "").toLowerCase();
    var rawOnly = String(rawText || "").toLowerCase();
    var actionRules = CLASSIFY_KEYWORDS.action || {};
    var rawHintRules = CLASSIFY_KEYWORDS.rawHint || {};
    if (actionRules.threat && actionRules.threat.test(actionOnly)) return "threat";
    if (actionRules.tactical_move && actionRules.tactical_move.test(actionOnly)) return "tactical_move";
    if (actionRules.routine && actionRules.routine.test(actionOnly)) return "routine";
    if (actionRules.state && actionRules.state.test(actionOnly)) return "state";
    if (rawHintRules.threat && rawHintRules.threat.test(rawOnly)) return "threat";
    if (rawHintRules.tactical_move && rawHintRules.tactical_move.test(rawOnly)) return "tactical_move";
    if (rawHintRules.routine && rawHintRules.routine.test(rawOnly)) return "routine";
    if (rawHintRules.state && rawHintRules.state.test(rawOnly)) return "state";
    return "unknown";
  }

  function calculateParseConfidence(parsed, rawText) {
    var score = 0.25;
    var raw = String(rawText || parsed.raw_text || "");
    if (parsed.subject && parsed.subject.length >= 2) score += 0.15;
    if (parsed.action && parsed.action.length >= 2) score += 0.15;
    if (parsed.action_type !== "unknown") score += 0.12;
    if (parsed.target && parsed.target !== "알 수 없는 장소" && parsed.target !== "현장") score += 0.1;
    if (parsed.has_explicit_quantity) score += 0.08;
    if (parsed.parse_mode === "structured") score += 0.1;
    for (var p = 0; p < ACTION_PATTERNS.length; p += 1) {
      if (ACTION_PATTERNS[p].regex.test(raw)) {
        score += 0.05;
        break;
      }
    }
    if (hasNegationCue(raw)) score += 0.05;
    return Math.max(0.05, Math.min(0.98, Number(score.toFixed(2))));
  }

  function normalizeActionToken(action) {
    var token = String(action || "").toLowerCase();
    return ACTION_NORMALIZE_MAP[token] || action || "";
  }

  function parseCountToken(token) {
    var text = String(token || "").trim().toLowerCase();
    if (!text) return null;
    if (/^\d+$/.test(text)) return parseInt(text, 10);
    var map = {
      한: 1,
      하나: 1,
      두: 2,
      둘: 2,
      세: 3,
      셋: 3,
      네: 4,
      넷: 4,
      다섯: 5,
      여섯: 6,
      일곱: 7,
      여덟: 8,
      아홉: 9,
      열: 10,
      열한: 11,
      열두: 12,
      열세: 13,
      열네: 14,
      열다섯: 15,
      열여섯: 16,
      열일곱: 17,
      열여덟: 18,
      열아홉: 19,
      스무: 20,
      스물: 20,
      서른: 30,
      마흔: 40,
      쉰: 50,
      예순: 60,
      일흔: 70,
      여든: 80,
      아흔: 90,
    };
    return map[text] != null ? map[text] : null;
  }

  function extractQuantityInfo(text) {
    var source = normalizeUserText(text);
    var m = source.match(
      /(\d+|한|하나|두|둘|세|셋|네|넷|다섯|여섯|일곱|여덟|아홉|열|열한|열두|열세|열네|열다섯|열여섯|열일곱|열여덟|열아홉|스무|스물|서른|마흔|쉰|예순|일흔|여든|아흔)\s*(마리|명|개|척|대|건|통)?/
    );
    if (!m) {
      return { has: false, value: 1, unit: "" };
    }
    var value = parseCountToken(m[1]);
    return {
      has: value != null,
      value: value != null ? value : 1,
      unit: m[2] || "",
    };
  }

  function isLikelyPlace(text) {
    var t = sanitizeKoreanNoun(text);
    if (!t) return false;
    for (var i = 0; i < WORLD_PLACES.length; i += 1) {
      if (t === WORLD_PLACES[i] || t.indexOf(WORLD_PLACES[i]) >= 0) return true;
    }
    return !/(이|가|은|는)$/.test(t);
  }

  function extractSubjectPredicate(sentenceBody) {
    var m = String(sentenceBody || "").match(/^(.+?)(?:이|가|은|는)\s+(.+)$/);
    if (!m) return null;
    return {
      subject: sanitizeKoreanNoun(m[1]),
      predicate: normalizeUserText(m[2]),
    };
  }

  function parseJsonField(raw) {
    var text = String(raw || "").trim();
    if (!text) return null;
    try {
      return JSON.parse(text);
    } catch (error) {
      return null;
    }
  }

  function parseReputationMap(raw) {
    var text = String(raw || "").trim();
    if (!text) return null;
    try {
      var parsed = JSON.parse(text);
      return parsed && typeof parsed === "object" ? parsed : null;
    } catch (error) {
      return null;
    }
  }

  function findWorldPlaceInText(rawText) {
    var text = String(rawText || "");
    for (var i = 0; i < WORLD_PLACES.length; i += 1) {
      var place = WORLD_PLACES[i];
      if (place && text.indexOf(place) >= 0) return sanitizeKoreanNoun(place);
    }
    return "";
  }

  function extractObjectTargetFromRaw(rawText) {
    var fromWorld = findWorldPlaceInText(rawText);
    if (fromWorld) return fromWorld;

    var lexicalEulWords = {
      마을: true,
      가을: true,
      겨울: true,
      저울: true,
      여울: true,
    };
    var parts = String(rawText || "").split(/\s+/).filter(Boolean);
    for (var i = parts.length - 1; i >= 0; i -= 1) {
      var token = parts[i].replace(/[,.!?]$/g, "");
      if (/(을|를)$/.test(token) && token.length >= 2) {
        var stripped = token.replace(/(을|를)$/g, "");
        if (token.endsWith("을") && lexicalEulWords[token]) {
          continue;
        }
        return sanitizeKoreanNoun(stripped);
      }
    }
    return "";
  }

  function hasPluralCue(subjectText, rawText) {
    var text = String(subjectText || "") + " " + String(rawText || "");
    return /(무리|떼|집단|부대|주민들|사람들|병사들|경비병들|학생들|군중|여러|다수|일행)/.test(text);
  }

  function hasNegationCue(text) {
    var t = String(text || "");
    return /(없다|없었|없음|아니다|않다|않았|못\s|안\s|불가능|실패|징후가?\s*없|계획이\s*없)/.test(t);
  }

  function applyNegationRule(parsed, text) {
    if (!parsed || !hasNegationCue(text)) return parsed;
    if (parsed.action_type === "threat" || parsed.action_type === "tactical_move") {
      parsed.action_type = "state";
      parsed.is_countable = false;
      parsed.quantity = 1;
      parsed.has_explicit_quantity = false;
      if (parsed.action && parsed.action.indexOf("없") < 0) {
        parsed.action = parsed.action + " (미발생)";
      }
      parsed.target = parsed.target || "현장";
      parsed.parse_confidence = Math.max(0.6, Number(parsed.parse_confidence || 0.6));
    }
    return parsed;
  }

  var OBSERVATION_SUBJECTS = {
    "공격 징후": true,
    "이상 징후": true,
  };

  function looksLikeClause(text) {
    var t = String(text || "");
    if (!t) return false;
    if (OBSERVATION_SUBJECTS[t]) return false;
    if (/(했|한다|했다|당했|당했다|중이다|상태다|보인다|빠졌|시도했|계획|징후|미발생)/.test(t)) return true;
    if (/\s+(고|며|는데|지만)\s+/.test(t)) return true;
    return false;
  }

  function normalizeParsedSlots(parsed, fallbackTarget) {
    if (!parsed) return parsed;
    parsed.subject = sanitizeKoreanNoun(parsed.subject || "대상");
    parsed.action = normalizeUserText(parsed.action || "");

    var target = sanitizeKoreanNoun(parsed.target || "");
    var invalidTarget =
      !target ||
      target === "알 수 없는 장소" ||
      target === parsed.subject ||
      target.length === 1 ||
      looksLikeClause(target) ||
      /^(아마|소문|오늘|어제|방금|지금)/.test(target) ||
      (parsed.action && target && parsed.action.indexOf(target) >= 0);

    if (invalidTarget) {
      target = sanitizeKoreanNoun(fallbackTarget || "현장");
    }
    parsed.target = target || "현장";

    if (parsed.subject && /(에서|에\s|으로\s)/.test(parsed.subject)) {
      var fixPlace = parsed.subject.match(/^(.+?)(?:에서|에|으로)\s+(.+)$/);
      if (fixPlace && fixPlace[1] && fixPlace[2]) {
        parsed.target = sanitizeKoreanNoun(fixPlace[1]);
        parsed.subject = sanitizeKoreanNoun(fixPlace[2]);
      }
    }
    if (parsed.subject && /(따르|소문|카더라)/.test(parsed.subject)) {
      parsed.parse_confidence = Math.min(Number(parsed.parse_confidence || 0.5), 0.55);
      parsed.parse_mode = "conservative_raw";
    }
    if (parsed.action_type === "unknown" && Number(parsed.parse_confidence || 0) > 0.6) {
      parsed.parse_confidence = 0.55;
      parsed.parse_mode = "conservative_raw";
    }
    if (Number(parsed.certainty || 1) <= 0.5) {
      parsed.parse_confidence = Math.min(Number(parsed.parse_confidence || 0.5), 0.55);
      parsed.parse_mode = "conservative_raw";
    }
    return parsed;
  }

  function normalizeActionPhrase(text) {
    var t = normalizeUserText(text || "");
    t = t.replace(/[,.!?]+$/g, "").trim();
    t = t
      .replace(/했(고|는데|지만)$/g, "했다")
      .replace(/되었(고|는데|지만)$/g, "되었다")
      .replace(/당했(고|는데|지만)$/g, "당했다")
      .replace(/고\s*있었(고|는데|지만)$/g, "고 있었다");
    return t.replace(/(하고|고|며|는데|지만)$/g, "").trim();
  }

  function extractActionFromText(text, tokens) {
    var actionToken = "";
    var actionCategory = "";
    var t = String(text || "");

    // Pick the best pattern with:
    // 1) higher category priority (state > routine, etc.)
    // 2) if tie, later appearance in the sentence.
    var categoryScore = {
      threat: 4,
      tactical_move: 3,
      state: 2,
      routine: 1,
      unknown: 0,
    };
    var bestIdx = -1;
    var bestScore = -1;
    for (var p = 0; p < ACTION_PATTERNS.length; p += 1) {
      var re = ACTION_PATTERNS[p].regex;
      var idx = -1;
      try {
        idx = t.search(re);
      } catch (e) {
        idx = -1;
      }
      if (idx >= 0) {
        var cat = ACTION_PATTERNS[p].category || "unknown";
        var score = categoryScore[cat] != null ? categoryScore[cat] : 0;
        if (score > bestScore || (score === bestScore && idx >= bestIdx)) {
        actionToken = ACTION_PATTERNS[p].value;
          actionCategory = cat;
          bestIdx = idx;
          bestScore = score;
        }
      }
    }
    if (!actionToken && tokens && tokens.length) {
      for (var j = 0; j < tokens.length; j += 1) {
        if (KNOWN_ACTIONS.indexOf(String(tokens[j]).toLowerCase()) >= 0) {
          actionToken = tokens[j];
          break;
        }
      }
    }
    return {
      token: actionToken,
      category: actionCategory,
    };
  }

  function parseScenarioText(scenarioText) {
    var full = normalizeUserText(scenarioText);
    if (!full) {
      return {
        subject: "밀수 화물",
        quantity: 1,
        action: "버려져 있다",
        target: "안개 계곡",
      };
    }

    var battleCompositeFacts = tryBattleDeathCompositeParse(full);
    if (battleCompositeFacts && battleCompositeFacts.length) {
      return mergeParsedSentences(battleCompositeFacts);
    }

    var sentences = splitIntoSentences(full);
    var perSentence = [];
    for (var si = 0; si < sentences.length; si += 1) {
      var one = parseSingleSentence(sentences[si]);
      if (one) {
        one.source_text = sentences[si];
        perSentence.push(one);
      }
    }
    if (perSentence.length === 0) {
      return mergeParsedSentences([]);
    }
    var merged = mergeParsedSentences(perSentence);
    merged.source_text = full;
    return merged;
  }

  function tryBattleDeathCompositeParse(fullText) {
    var raw = normalizeUserText(fullText);
    if (!raw) return null;
    var m = raw.match(
      /^(.+?)이\s+(.+?)에서\s+(.+?)에서\s+빠져나온\s+(.+?)\s+(\d+|한|하나|두|둘|세|셋|네|넷|다섯|여섯|일곱|여덟|아홉|열|열한|열두|열세|열네|열다섯|열여섯|열일곱|열여덟|열아홉|스무|스물|서른|마흔|쉰|예순|일흔|여든|아흔)\s*마리와\s+전투하다가\s+(.+)$/
    );
    if (!m) return null;

    var actor = sanitizeKoreanNoun(m[1]);
    var battlePlace = sanitizeKoreanNoun(m[2]);
    var escapePlace = sanitizeKoreanNoun(m[3]);
    var enemy = sanitizeKoreanNoun(m[4]);
    var qty = parseCountToken(m[5]);
    var deathClause = normalizeActionPhrase(m[6]);

    var fact1 = {
      subject: enemy || "도적",
      quantity: qty != null ? qty : 1,
      action: "탈출",
      target: escapePlace || "현장",
      certainty: detectUncertainty(raw),
      is_countable: true,
      action_type: "tactical_move",
      raw_text: escapePlace + "에서 빠져나온 " + enemy + " " + (qty != null ? String(qty) : "1") + "마리",
      has_explicit_quantity: true,
      parse_confidence: 0.95,
      parse_mode: "structured",
      source_text: fullText,
    };

    var deathAction = deathClause;
    if (!/(죽|사망|전사)/.test(deathAction)) {
      deathAction = "전투 중 사망했다";
    }
    var fact2 = {
      subject: actor || "대상",
      quantity: 1,
      action: normalizeActionPhrase(deathAction),
      target: battlePlace || "현장",
      certainty: detectUncertainty(raw),
      is_countable: false,
      action_type: "threat",
      raw_text: actor + "이 " + battlePlace + "에서 전투하다가 " + deathClause,
      has_explicit_quantity: false,
      parse_confidence: 0.93,
      parse_mode: "structured",
      source_text: fullText,
    };
    return [fact1, fact2];
  }

  function stripLeadingQualifiers(text) {
    var t = normalizeUserText(text);
    var rumorPrefix = t.match(/^(소문에 따르면|전해 들으니|카더라|듣자하니)\s+(.+)$/);
    if (rumorPrefix && rumorPrefix[2]) t = rumorPrefix[2].trim();
    var timePrefix = t.match(/^(오늘|어제|방금|지금|요즘|아마)(?:은|는)?\s+(.+)$/);
    if (timePrefix && timePrefix[2]) t = timePrefix[2].trim();
    return t;
  }

  function trySubjectPlacePredicateParse(raw, compact) {
    var m = compact.match(/^(.+?)(?:이|가|은|는)\s+(.+?)(?:에서|에|으로)\s+(.+)$/);
    if (!m) return null;
    var subjectText = sanitizeKoreanNoun(m[1]);
    var locationText = sanitizeKoreanNoun(m[2]);
    var predicateText = normalizeUserText(m[3]);
    if (!isLikelyPlace(locationText)) return null;
    var actionHit = extractActionFromText(predicateText + " " + raw, predicateText.split(/\s+/).filter(Boolean));
    var actionText = normalizeActionPhrase(actionHit.token || predicateText);

    // object(상대) 추출: 예) "... 반군 무리 ... 쫓고있다"
    var objectText = "";
    var enemyMatch = raw.match(/(반군\s*무리|반군|적병\s*무리|적병|도적\s*무리|도적|침입자\s*무리|침입자)/);
    var pursuitCue = /(쫓|추격|추적)/.test(raw);
    if (enemyMatch && pursuitCue) {
      objectText = sanitizeKoreanNoun(enemyMatch[1]);
    }

    var qtyInfo = extractQuantityInfo(compact);
    var parsed = {
      subject: subjectText,
      quantity: qtyInfo.has ? qtyInfo.value : 1,
      action: actionText,
      target: locationText,
      certainty: detectUncertainty(raw),
      is_countable: qtyInfo.has && isCountableScenario(actionText, raw),
      action_type: actionHit.category || classifyActionType(actionText, raw),
      raw_text: raw,
      object: objectText || "",
      has_explicit_quantity: qtyInfo.has,
      parse_confidence: 0.9,
      parse_mode: "structured",
    };
    parsed = applyNegationRule(parsed, raw);
    return normalizeParsedSlots(parsed, locationText);
  }

  function trySubjectLocationActionParse(raw, compact) {
    var m = compact.match(/^(.+?)(?:이|가|은|는)\s+(.+?)(?:에서|에|으로)\s+(.+)$/);
    if (!m) return null;
    var subjectText = sanitizeKoreanNoun(m[1]);
    var locationText = sanitizeKoreanNoun(m[2]);
    var predicateText = normalizeUserText(m[3]);
    var actionHit = extractActionFromText(predicateText + " " + raw, predicateText.split(/\s+/).filter(Boolean));
    var actionText = normalizeActionPhrase(actionHit.token || predicateText);
    var actionType = actionHit.category || classifyActionType(actionText, raw);
    var qtyInfo = extractQuantityInfo(compact);
    var parsed = {
      subject: subjectText,
      quantity: qtyInfo.has ? qtyInfo.value : 1,
      action: actionText,
      target: locationText || "현장",
      certainty: detectUncertainty(raw),
      is_countable: qtyInfo.has && isCountableScenario(actionText, raw),
      action_type: actionType,
      raw_text: raw,
      has_explicit_quantity: qtyInfo.has,
      parse_confidence: 0.88,
      parse_mode: "structured",
    };
    parsed = applyNegationRule(parsed, raw);
    return normalizeParsedSlots(parsed, locationText || "현장");
  }

  function tryPlaceSubjectPredicateParse(raw, compact) {
    var placeSubject = compact.match(/^(.+?)(?:에서|에|으로)\s+(.+?)(?:이|가|은|는)\s+(.+)$/);
    if (!placeSubject) return null;
    if (!isLikelyPlace(sanitizeKoreanNoun(placeSubject[1]))) return null;
    var locationText = sanitizeKoreanNoun(placeSubject[1]);
    var subjectText = sanitizeKoreanNoun(placeSubject[2]);
    var predicateText = normalizeUserText(placeSubject[3]);
    var actionHit = extractActionFromText(predicateText + " " + raw, predicateText.split(/\s+/).filter(Boolean));
    var actionText = normalizeActionPhrase(actionHit.token || predicateText);
    var locInPredicate = predicateText.match(/^(.+?)(?:에|에서|으로)\s+(.+)$/);
    if (locInPredicate && locInPredicate[1] && locInPredicate[2]) {
      locationText = sanitizeKoreanNoun(locInPredicate[1]);
      actionText = normalizeActionPhrase(locInPredicate[2]);
    }
    var actionType = actionHit.category || classifyActionType(actionText, raw);
    var qtyInfo = extractQuantityInfo(compact);
    var parsed = {
      subject: subjectText,
      quantity: qtyInfo.has ? qtyInfo.value : 1,
      action: actionText,
      target: locationText || "현장",
      certainty: detectUncertainty(raw),
      is_countable: qtyInfo.has && isCountableScenario(actionText, raw),
      action_type: actionType,
      raw_text: raw,
      has_explicit_quantity: qtyInfo.has,
      parse_confidence: 0.88,
      parse_mode: "structured",
    };
    parsed = applyNegationRule(parsed, raw);
    return normalizeParsedSlots(parsed, locationText || "현장");
  }

  function tryInlineCountSubjectParse(raw, compact) {
    var m = compact.match(
      /^(.+?)\s+(\d+|한|하나|두|둘|세|셋|네|넷|다섯|여섯|일곱|여덟|아홉|열|열한|열두|열세|열네|열다섯|열여섯|열일곱|열여덟|열아홉|스무|스물|서른|마흔|쉰|예순|일흔|여든|아흔)\s*(마리|명|개|척|대|건|통)(?:가|이|는|은)?\s+(.+)$/
    );
    if (!m) return null;
    var subjectText = sanitizeKoreanNoun(m[1]);
    var countVal = parseCountToken(m[2]);
    var unit = m[3] || "마리";
    var predicateText = normalizeUserText(m[4] || "");
    var actionHit = extractActionFromText(predicateText + " " + raw, predicateText.split(/\s+/).filter(Boolean));
    var actionText = normalizeActionPhrase(actionHit.token || predicateText);
    var locInPred = predicateText.match(/^(.+?)(?:에서|에|으로)\s+(.+)$/);
    var target = findWorldPlaceInText(raw) || "현장";
    if (locInPred) {
      target = sanitizeKoreanNoun(locInPred[1]);
      actionText = normalizeActionPhrase(locInPred[2]);
    }
    var parsed = {
      subject: subjectText,
      quantity: countVal != null ? countVal : 1,
      action: actionText,
      target: target,
      certainty: detectUncertainty(raw),
      is_countable: true,
      action_type: actionHit.category || classifyActionType(actionText, raw),
      raw_text: raw,
      has_explicit_quantity: true,
      parse_confidence: 0.92,
      parse_mode: "structured",
    };
    parsed = applyNegationRule(parsed, raw);
    return normalizeParsedSlots(parsed, target);
  }

  function tryPossessiveCountParse(raw, compact) {
    var m = compact.match(
      /^(.+?\s*(?:\d+|한|하나|두|둘|세|셋|네|넷|다섯|여섯|일곱|여덟|아홉|열|스무|스물|서른|마흔|쉰|예순|일흔|여든|아흔)\s*마리(?:의)?)\s+(.+?)(?:이|가|은|는)\s+(.+)$/
    );
    if (!m) return null;
    var countPart = m[1];
    var qtyInfo = extractQuantityInfo(countPart);
    var subjectText = sanitizeKoreanNoun(m[2]);
    var predicateText = normalizeUserText(m[3]);
    var actionHit = extractActionFromText(predicateText, predicateText.split(/\s+/).filter(Boolean));
    var actionText = normalizeActionPhrase(actionHit.token || predicateText);
    var place = findWorldPlaceInText(raw) || "현장";
    var locInPred = predicateText.match(/^(.+?)(?:에서|에|으로)\s+(.+)$/);
    if (locInPred) {
      place = sanitizeKoreanNoun(locInPred[1]);
      actionText = normalizeActionPhrase(locInPred[2]);
    }
    var parsed = {
      subject: subjectText,
      quantity: qtyInfo.has ? qtyInfo.value : 1,
      action: actionText,
      target: place,
      certainty: detectUncertainty(raw),
      is_countable: true,
      action_type: actionHit.category || classifyActionType(actionText, raw),
      raw_text: raw,
      has_explicit_quantity: true,
      parse_confidence: 0.9,
      parse_mode: "structured",
    };
    return normalizeParsedSlots(parsed, place);
  }

  function parseSingleSentence(sentenceText) {
    var raw = normalizeUserText(sentenceText);
    if (!raw) return null;
    raw = stripLeadingQualifiers(raw);

    var compact = raw.replace(/[,.!?]/g, " ");
    var inlineCountParsed = tryInlineCountSubjectParse(raw, compact);
    if (inlineCountParsed) return inlineCountParsed;

    var subjectPlaceParsed = trySubjectPlacePredicateParse(raw, compact);
    if (subjectPlaceParsed) return subjectPlaceParsed;

    var placeParsed = tryPlaceSubjectPredicateParse(raw, compact);
    if (placeParsed) return placeParsed;

    var subjectLocParsed = trySubjectLocationActionParse(raw, compact);
    if (subjectLocParsed) return subjectLocParsed;

    var possessiveParsed = tryPossessiveCountParse(raw, compact);
    if (possessiveParsed) return possessiveParsed;
    var tokens = compact.split(/\s+/).filter(Boolean);
    var structuredCountPattern = compact.match(
      /^(.+?)(?:에서|에|으로)\s+(.+?)\s+(\d+|한|하나|두|둘|세|셋|네|넷|다섯|여섯|일곱|여덟|아홉|열|열한|열두|열세|열네|열다섯|열여섯|열일곱|열여덟|열아홉|스무|스물|서른|마흔|쉰|예순|일흔|여든|아흔)\s*(마리|명|개|척|대|건|통)?(?:가|이|는|은)?\s+(.+)$/
    );
    if (structuredCountPattern) {
      var locationText = sanitizeKoreanNoun(structuredCountPattern[1]);
      var subjectText = sanitizeKoreanNoun(structuredCountPattern[2]);
      var countText = parseCountToken(structuredCountPattern[3]);
      var predicateText = normalizeUserText(structuredCountPattern[5] || "");
      var actionFromStructured = extractActionFromText(predicateText, predicateText.split(/\s+/).filter(Boolean));
      var normalizedStructuredAction = normalizeActionToken(actionFromStructured.token);
      var structuredAction = normalizedStructuredAction || predicateText;
      var structuredType = actionFromStructured.category || classifyActionType(structuredAction, raw);
      var parsedDirect = {
        subject: subjectText || "대상",
        quantity: countText != null ? countText : 1,
        action: structuredAction,
        target: locationText || "현장",
        certainty: detectUncertainty(raw),
        is_countable: isCountableScenario(structuredAction, raw),
        action_type: structuredType,
        raw_text: raw,
        has_explicit_quantity: true,
      };
      parsedDirect.parse_confidence = 0.95;
      parsedDirect.parse_mode = "structured";
      parsedDirect = applyNegationRule(parsedDirect, raw);
      return normalizeParsedSlots(parsedDirect, locationText || "현장");
    }
    var subjectCountPattern = compact.match(
      /^(.+?)\s+(\d+|한|하나|두|둘|세|셋|네|넷|다섯|여섯|일곱|여덟|아홉|열|열한|열두|열세|열네|열다섯|열여섯|열일곱|열여덟|열아홉|스무|스물|서른|마흔|쉰|예순|일흔|여든|아흔)\s*(마리|명|개|척|대|건|통)?(?:가|이|는|은)?\s+(.+)$/
    );
    if (subjectCountPattern) {
      var subjectCountAction = extractActionFromText(subjectCountPattern[4], subjectCountPattern[4].split(/\s+/).filter(Boolean));
      var normalizedSubjectCountAction = normalizeActionToken(subjectCountAction.token);
      var subjectCountText = parseCountToken(subjectCountPattern[2]);
      var subjectRaw = sanitizeKoreanNoun(subjectCountPattern[1]);
      var placeInSubject = subjectRaw.match(/^(.+?)(?:에서|에|으로)\s+(.+)$/);
      var subjectForCount = placeInSubject ? sanitizeKoreanNoun(placeInSubject[2]) : subjectRaw;
      var targetFromSubject = placeInSubject ? sanitizeKoreanNoun(placeInSubject[1]) : findWorldPlaceInText(raw);
      var subjectCountParsed = {
        subject: subjectForCount,
        quantity: subjectCountText != null ? subjectCountText : 1,
        action: normalizedSubjectCountAction || normalizeUserText(subjectCountPattern[4]),
        target: targetFromSubject || "현장",
        certainty: detectUncertainty(raw),
        is_countable: true,
        action_type: subjectCountAction.category || classifyActionType(subjectCountPattern[4], raw),
        raw_text: raw,
        has_explicit_quantity: true,
      };
      subjectCountParsed.parse_confidence = 0.88;
      subjectCountParsed.parse_mode = "structured";
      subjectCountParsed = applyNegationRule(subjectCountParsed, raw);
      return normalizeParsedSlots(subjectCountParsed, targetFromSubject || "현장");
    }
    var quantityInfo = extractQuantityInfo(compact);
    var hasExplicitQuantity = quantityInfo.has;
    var quantity = quantityInfo.has ? quantityInfo.value : 1;

    var sentenceBody = raw;
    var timeContext = "";

    var extractedAction = extractActionFromText(raw, tokens);
    var actionToken = extractedAction.token;
    var actionCategoryFromPattern = extractedAction.category;
    var action = normalizeActionToken(actionToken);

    var subject = tokens[0] || "밀수 화물";
    var subjectMatch = raw.match(/^([^\d\s]+)\s*\d+\s*마리/);
    if (subjectMatch && subjectMatch[1]) {
      subject = subjectMatch[1];
    }
    var subjectByParticle = extractSubjectPredicate(sentenceBody);
    var derivedFromPredicate = false;
    if (subjectByParticle && subjectByParticle.subject) {
      subject = subjectByParticle.subject;
    }
    subject = sanitizeKoreanNoun(subject);

    // object(상대) 추출: 예) "카엘이 ... 반군 무리 ... 쫓고있다"
    // subject는 행위자(카엘)로 두고, object는 상대(반군 무리)로 분리해 trace/UI에 노출한다.
    var objectText = "";
    var enemyMatch = raw.match(/(반군\s*무리|반군|적병\s*무리|적병|도적\s*무리|도적|침입자\s*무리|침입자)/);
    var pursuitCue = /(쫓|추격|추적)/.test(raw) || /(쫓|추격|추적)/.test(action);
    if (enemyMatch && pursuitCue) {
      objectText = sanitizeKoreanNoun(enemyMatch[1]);
    }

    var cleanedTarget = extractObjectTargetFromRaw(raw);
    if (!cleanedTarget) {
      var actionIndex = -1;
      for (var k = 0; k < tokens.length; k += 1) {
        if (String(tokens[k]).toLowerCase() === String(actionToken).toLowerCase()) {
          actionIndex = k;
          break;
        }
      }
      var targetTokens = actionIndex >= 0 ? tokens.slice(actionIndex + 1) : tokens.slice(1);
      cleanedTarget = targetTokens.join(" ").replace(/\b\d+\b/g, "").trim();
    }
    cleanedTarget = cleanedTarget.replace(/(했|한다|했다|하는|중이다)$/g, "").trim();
    cleanedTarget = sanitizeKoreanNoun(cleanedTarget);
    if (subjectByParticle && subjectByParticle.predicate) {
      var predicate = subjectByParticle.predicate;
      // If predicate includes an explicit location clause ("...에서 ..."), split into target/action.
      var locationPredicate = predicate.match(/^(.+?)(?:에서|에|으로)\s+(.+)$/);
      if (locationPredicate && locationPredicate[1] && locationPredicate[2]) {
        cleanedTarget = sanitizeKoreanNoun(locationPredicate[1]);
        action = normalizeActionPhrase(locationPredicate[2]);
        derivedFromPredicate = false;
      } else if (!/^(이동|공격|정찰|집결|탈출|독살|암살|살해|불을)/.test(predicate) && predicate.length > 0) {
        action = normalizeActionPhrase(predicate.replace(/[.]$/g, ""));
        derivedFromPredicate = true;
      }
    }
    if (!action) {
      action = sentenceBody.replace(/^(.+?)(?:이|가|은|는)\s+/, "").trim();
    }
    action = normalizeActionPhrase(action);
    // If connector normalization made threat phrases too short, restore canonical action from pattern.
    if (
      actionCategoryFromPattern === "threat" &&
      !/(다|했다|되었다|당했다|중이다|보인다|상태다)$/.test(action) &&
      actionToken
    ) {
      action = normalizeActionPhrase(normalizeActionToken(actionToken) || String(actionToken));
    }
    if (
      action.indexOf("독살") >= 0 ||
      action.indexOf("암살") >= 0 ||
      action.indexOf("살해") >= 0 ||
      action.indexOf("불을") >= 0
    ) {
      if (!cleanedTarget || cleanedTarget === "일상 관찰" || cleanedTarget === "현장") {
        var threatPlace = findWorldPlaceInText(raw);
        cleanedTarget = threatPlace || cleanedTarget || "궁정 내부";
      }
    }
    if (!cleanedTarget) {
      cleanedTarget = timeContext || "현장";
    }
    if (derivedFromPredicate) {
      cleanedTarget = timeContext || "일상 관찰";
    }

    var actionType = actionCategoryFromPattern || classifyActionType(action, raw);
    var countable =
      hasExplicitQuantity ||
      ((actionType === "threat" || actionType === "tactical_move") && hasPluralCue(subject, raw));
    if (hasExplicitQuantity && (quantityInfo.unit === "마리" || quantityInfo.unit === "명" || quantityInfo.unit === "개")) {
      countable = true;
    }

    var parsedResult = {
      subject: subject,
      quantity: quantity,
      action: action,
      object: objectText || "",
      target: cleanedTarget || "알 수 없는 장소",
      certainty: detectUncertainty(raw),
      is_countable: countable && (hasExplicitQuantity || isCountableScenario(action, raw)),
      action_type: actionType,
      raw_text: raw,
      has_explicit_quantity: hasExplicitQuantity,
    };
    parsedResult.parse_confidence = calculateParseConfidence(parsedResult, raw);
    parsedResult.parse_mode = parsedResult.parse_confidence < 0.6 ? "conservative_raw" : "structured";
    if (/(있는 것 같|것 같다|들어온 것 같)/.test(raw)) {
      parsedResult.parse_confidence = Math.min(parsedResult.parse_confidence, 0.55);
      parsedResult.parse_mode = "conservative_raw";
      parsedResult.certainty = detectUncertainty(raw);
    }
    if (parsedResult.parse_mode === "conservative_raw") {
      parsedResult.action = sentenceBody;
      parsedResult.is_countable = false;
      parsedResult.quantity = 1;
      parsedResult.target = findWorldPlaceInText(raw) || "원문 서술";
    }
    parsedResult = applyNegationRule(parsedResult, raw);
    return normalizeParsedSlots(parsedResult, findWorldPlaceInText(raw) || "현장");
  }

  function render() {
    try {
      appendDebug("render start");
      if (typeof executeScenario !== "function") {
        appendDebug("executeScenario is not a function");
        outputDialogueMain.value = "엔진 로드 실패: quest-system.js를 먼저 로드해야 합니다.";
        outputDialogue.textContent = "엔진 로드 실패";
        return;
      }

      var formData = new FormData(form);
      var trustLevel = parseNumber(formData.get("trustLevel"), 0.74);
      var senderStats = {
        fear: parseNumber(formData.get("senderFear"), 0.82),
        hostility: parseNumber(formData.get("senderHostility"), 0.62),
        trust: parseNumber(formData.get("senderTrust"), 0.55),
      };
      var receiverStats = {
        credulity: parseNumber(formData.get("receiverCredulity"), 0.86),
        trust: parseNumber(formData.get("receiverTrust"), 0.65),
      };
      var scenarioParsed = parseScenarioText(formData.get("scenarioText"));
      appendDebug("parse mode: " + scenarioParsed.parse_mode + ", confidence: " + scenarioParsed.parse_confidence);

      var primaryFact = selectPrimaryParse(scenarioParsed) || scenarioParsed;
      appendDebug("primary fact: " + (primaryFact.action_type || "unknown") + " / " + String(primaryFact.action || ""));

      var facts = scenarioParsed.facts && scenarioParsed.facts.length
        ? scenarioParsed.facts
        : buildFactsFromParsed(scenarioParsed);
      var reputationMap = parseReputationMap(formData.get("subjectReputation"));
      var senderReputation = parseReputationMap(formData.get("senderReputation")) || reputationMap;
      var receiverReputation = parseReputationMap(formData.get("receiverReputation")) || reputationMap;
      var quantityMode = String(formData.get("quantityMode") || "dramatic");
      var allowPartialTrust = String(formData.get("allowPartialTrust") || "on") === "on";

      if (outputParsePipeline) {
        outputParsePipeline.textContent = buildParsePipelineTrace(scenarioParsed, primaryFact, facts);
      }
      if (outputFacts) {
        outputFacts.textContent = formatJSON(facts);
      }

      var groundTruthRaw = parseJsonField(formData.get("groundTruthJson"));
      var groundTruth = Array.isArray(groundTruthRaw)
        ? groundTruthRaw
        : groundTruthRaw && groundTruthRaw.facts
          ? groundTruthRaw.facts
          : null;
      var persistSession = String(formData.get("persistSession") || "") === "on";
      var runChain = String(formData.get("runChain") || "") === "on";
      var usePlayerAsSender = String(formData.get("usePlayerAsSender") || "on") === "on";
      var receiverProfileKey = String(formData.get("receiverProfileKey") || "scholar_alric");
      var propagationPreset = String(formData.get("questPropagationPreset") || "");
      if (propagationPreset.indexOf(":") >= 0) {
        var presetParts = propagationPreset.split(":");
        quantityMode = presetParts[1] || quantityMode;
      }

      var sessionKey = "ui-session";
      var gameTick = Number(formData.get("gameTick"));
      if (window.GameClock && !isNaN(gameTick)) {
        window.GameClock.setTick(sessionKey, gameTick, { reason: "ui_form" });
      }
      var advanceTicks = Number(formData.get("advanceTicksAfterPropagate"));
      var seedWorld = String(formData.get("seedWorldTruthFromFacts") || "") === "on";
      var recordPkb = String(formData.get("recordPlayerKnowledge") || "on") === "on";

      var result = executeScenario({
        trustLevel: trustLevel,
        senderStats: senderStats,
        receiverStats: receiverStats,
        facts: facts,
        senderReputation: senderReputation,
        receiverReputation: receiverReputation,
        quantityMode: quantityMode,
        allowPartialTrust: allowPartialTrust,
        usePlayerAsSender: usePlayerAsSender,
        receiverProfileKey: receiverProfileKey,
        reputationSessionKey: sessionKey,
        persistSession: persistSession,
        sessionKey: sessionKey,
        groundTruth: groundTruth,
        seedWorldTruthFromFacts: seedWorld,
        recordPlayerKnowledge: recordPkb,
        advanceTicksAfterPropagate: !isNaN(advanceTicks) && advanceTicks > 0 ? advanceTicks : 0,
        infoTruthValue: {
          subject: primaryFact.subject,
          action: primaryFact.action,
          target: primaryFact.target,
          object: primaryFact.object || "",
          quantity: primaryFact.quantity,
          certainty: primaryFact.certainty,
          is_countable: primaryFact.is_countable,
          action_type: primaryFact.action_type,
          parse_confidence: primaryFact.parse_confidence,
          parse_mode: primaryFact.parse_mode,
        },
      });

      outputBase.textContent = formatJSON(result.baseInfo);
      outputSender.textContent = formatJSON(
        result.propagation.distortedFacts || result.propagation.senderDistorted
      );
      outputReceiver.textContent = formatJSON(
        result.propagation.interpretedFacts || result.propagation.receiverInterpreted
      );
      outputKb.textContent = formatJSON(result.knowledgeBaseSnapshot);
      if (outputKnowledgeLayers) {
        outputKnowledgeLayers.textContent = formatJSON({
          gameClock: result.gameClockSnapshot || null,
          playerKnowledge: result.playerKnowledgeRecord || null,
          knowledgeLayers: result.knowledgeLayersSnapshot || null,
          anchorValidation: result.anchorValidation || (result.dialogue && result.dialogue.anchorValidation),
        });
      }
      if (outputAudit) {
        outputAudit.textContent = formatJSON({
          auditTrail: result.auditTrail || [],
          bundleContext: result.bundleContext || {},
          propagationMeta: {
            partialTrust: result.propagation.partialTrust,
            quantityMode: result.propagation.quantityMode,
            reason: result.propagation.reason,
          },
        });
      }
      if (outputAuditDiff) {
        outputAuditDiff.textContent = formatJSON(result.auditDiff || []);
      }
      if (outputGroundTruth) {
        outputGroundTruth.textContent = result.groundTruthReport
          ? formatJSON(result.groundTruthReport)
          : "ground truth 미입력";
      }
      if (outputChain && runChain && engine.propagateChain) {
        var chainResult = engine.propagateChain(
          [
            { sender: result.sender, receiver: result.receiver },
            { sender: result.receiver, receiver: result.sender },
          ],
          facts,
          result.baseInfo,
          {
            quantityMode: quantityMode,
            allowPartialTrust: allowPartialTrust,
            currentTick: (window.GameClock ? window.GameClock.getTick(sessionKey) : 1000) + 10,
          }
        );
        outputChain.textContent = formatJSON({
          hops: chainResult.hopResults.map(function (h) {
            return {
              hop: h.hop,
              from: h.from,
              to: h.to,
              blocked: h.propagation.blocked,
              reason: h.propagation.reason,
            };
          }),
          finalFacts: chainResult.finalFacts,
          blocked: chainResult.blocked,
        });
      } else if (outputChain) {
        outputChain.textContent = "다단 전파 비활성 (체크박스 해제)";
      }
      outputDialogueMain.value = buildReadableDialogue(result);
      saveFormState();
      appendDebug("pipeline executed, facts=" + facts.length);

      if (result.propagation.blocked) {
        appendDebug("propagation blocked: " + result.propagation.reason);
        outputDialogue.textContent = "정보가 전달되지 않아 대화가 생성되지 않았습니다.";
        if (outputEngineData) outputEngineData.textContent = "";
        return;
      }

      function showDialogue(res) {
        if (outputEngineData && res.dialogue) {
          outputEngineData.textContent = formatJSON({
            engineData: res.dialogue.engineData || {},
            llmProvider: res.dialogue.llmProvider || "mock",
          });
        }
        outputDialogue.textContent = res.dialogue ? res.dialogue.finalSpeech : "";
        appendDebug("dialogue rendered (" + (res.dialogue && res.dialogue.llmProvider) + ")");
      }

      if (
        window.LlmAdapter &&
        window.LlmAdapter.isLive() &&
        result.dialogue &&
        result.dialogue.llmPending
      ) {
        outputDialogue.textContent = "Live LLM 생성 중…";
        window.LlmAdapter.enrichDialogueResult(result, result.receiver && result.receiver.persona)
          .then(showDialogue)
          .catch(function (err) {
            appendDebug("llm error: " + (err.message || err));
            outputDialogue.textContent =
              "LLM 오류 (mock 유지): " + (err.message || err) + "\n\n" + (result.dialogue.finalSpeech || "");
          });
        return;
      }

      showDialogue(result);
    } catch (error) {
      var message = (error && error.message) ? error.message : String(error);
      appendDebug("render error: " + message);
      outputDialogueMain.value = "렌더링 오류: " + message;
      outputDialogue.textContent = "렌더링 오류가 발생했습니다.";
    }
  }

  function initCollapsiblePanels() {
    var toggleButtons = document.querySelectorAll(".toggle-btn");
    for (var i = 0; i < toggleButtons.length; i += 1) {
      (function (button) {
        button.addEventListener("click", function () {
          var targetId = button.getAttribute("data-target");
          if (!targetId) return;
          var target = document.getElementById(targetId);
          if (!target) return;
          var isCollapsed = target.classList.toggle("is-collapsed");
          button.textContent = isCollapsed ? "펼치기" : "접기";
          appendDebug("toggle " + targetId + ": " + (isCollapsed ? "collapsed" : "expanded"));
        });
      })(toggleButtons[i]);
    }
  }

  if (!form || !runButton) {
    appendDebug("critical dom elements missing");
  } else {
    form.addEventListener("input", render);
    runButton.addEventListener("click", function () {
      if (clickIndicator) {
        clickIndicator.textContent = "클릭 감지(JS): " + new Date().toLocaleTimeString();
      }
      appendDebug("run button click event");
      render();
    });
  }

  window.NpcParser = {
    parseScenarioText: parseScenarioText,
    parseSingleSentence: parseSingleSentence,
    splitIntoSentences: splitIntoSentences,
    buildFactsFromParsed: buildFactsFromParsed,
    mergeParsedSentences: mergeParsedSentences,
    selectPrimaryParse: selectPrimaryParse,
    buildParsePipelineTrace: buildParsePipelineTrace,
    classifyActionType: classifyActionType,
  };

  if (form && runButton) {
    restoreFormState();
    initCollapsiblePanels();
    render();
  }
})();
