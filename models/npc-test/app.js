(function () {
  var engine = window.QuestSystem;
  var lexicon = window.NpcLexicon || {};
  var executeScenario = engine && engine.executeScenario;
  var KNOWN_ACTIONS = lexicon.knownActions || [];
  var ACTION_PATTERNS = lexicon.actionPatterns || [];
  var NON_COUNTABLE_PATTERNS = lexicon.nonCountablePatterns || [];
  var CLASSIFY_KEYWORDS = lexicon.classifyKeywords || {};
  var ACTION_NORMALIZE_MAP = lexicon.actionNormalizeMap || {};

  var form = document.querySelector("#control-form");
  var runButton = document.querySelector("#run-pipeline");
  var outputBase = document.querySelector("#out-base");
  var outputSender = document.querySelector("#out-sender");
  var outputReceiver = document.querySelector("#out-receiver");
  var outputKb = document.querySelector("#out-kb");
  var outputDialogue = document.querySelector("#out-dialogue");
  var outputDialogueMain = document.querySelector("#out-dialogue-main");
  var debugLog = document.querySelector("#debug-log");
  var clickIndicator = document.querySelector("#click-indicator");

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

  function buildReadableDialogue(result) {
    if (result.propagation.blocked) {
      return "정보 전파 차단: " + result.propagation.reason;
    }
    var d = result.propagation.receiverInterpreted.truth_value;
    var source = result.propagation.receiverInterpreted.metadata.source;
    var actionText = String(d.action || "");
    var naturalAction = /다$|다\)|있다$/.test(actionText) ? actionText : actionText + "했다";
    var subjectText = sanitizeKoreanNoun(String(d.subject || "대상"));
    var targetText = sanitizeKoreanNoun(String(d.target || "알 수 없는 장소"));
    var quantityText = d.is_countable ? String(d.quantity) : "해당 없음";
    return (
      subjectText +
      pickKoreanParticle(subjectText, "이", "가") +
      " " +
      naturalAction +
      ". " +
      "상황 맥락: " +
      targetText +
      ", 수량: " +
      quantityText +
      ", 확신도: " +
      d.certainty +
      " (" +
      source +
      pickKoreanParticle(String(source || ""), "의", "의") +
      " 정보)"
    );
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
    return cleaned.replace(/(이|가|은|는|을|를|의)$/g, "");
  }

  function normalizeUserText(text) {
    var normalized = String(text || "").replace(/\u00A0/g, " ");
    normalized = normalized.replace(/\s+/g, " ").trim();
    return normalized;
  }

  function splitIntoSentences(fullText) {
    var t = normalizeUserText(fullText);
    if (!t) return [];
    var chunks = t.split(/(?<=[.!?…])\s+/);
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

  function mergeParsedSentences(list) {
    if (!list || list.length === 0) {
      return {
        subject: "wolf",
        quantity: 3,
        action: "move",
        target: "north gate",
        certainty: 0.75,
        is_countable: true,
        action_type: "unknown",
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

    var subjectMerged = subjects[0] || "늑대";
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

    return merged;
  }

  function detectUncertainty(rawText) {
    var text = String(rawText || "");
    var lowCertaintyPatterns = [/같다/, /같아/, /추정/, /아마/, /듯/, /처럼 보/];
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

  function calculateParseConfidence(parsed) {
    var score = 0.35;
    if (parsed.subject && parsed.subject.length >= 2) score += 0.2;
    if (parsed.action && parsed.action.length >= 2) score += 0.2;
    if (parsed.action_type !== "unknown") score += 0.15;
    if (parsed.target && parsed.target !== "알 수 없는 장소") score += 0.1;
    if (parsed.has_explicit_quantity) score += 0.1;
    return Math.max(0.05, Math.min(0.98, Number(score.toFixed(2))));
  }

  function normalizeActionToken(action) {
    var token = String(action || "").toLowerCase();
    return ACTION_NORMALIZE_MAP[token] || action || "";
  }

  function parseScenarioText(scenarioText) {
    var full = normalizeUserText(scenarioText);
    if (!full) {
      return {
        subject: "wolf",
        quantity: 3,
        action: "move",
        target: "north gate",
      };
    }

    var sentences = splitIntoSentences(full);
    var perSentence = [];
    for (var si = 0; si < sentences.length; si += 1) {
      var one = parseSingleSentence(sentences[si]);
      if (one) perSentence.push(one);
    }
    if (perSentence.length === 0) {
      return mergeParsedSentences([]);
    }
    return mergeParsedSentences(perSentence);
  }

  function parseSingleSentence(sentenceText) {
    var raw = normalizeUserText(sentenceText);
    if (!raw) return null;

    var compact = raw.replace(/[,.!?]/g, " ");
    var tokens = compact.split(/\s+/).filter(Boolean);
    var escapePattern = compact.match(/^(.+?)에서\s+(.+?)\s+(\d+)\s*마리가\s+(탈출(?:했|한|하는|중)?)/);
    if (escapePattern) {
      var parsedDirect = {
        subject: sanitizeKoreanNoun(escapePattern[2]),
        quantity: parseNumber(escapePattern[3], 1),
        action: "탈출했다",
        target: sanitizeKoreanNoun(escapePattern[1]),
        certainty: detectUncertainty(raw),
        is_countable: true,
        action_type: "tactical_move",
        raw_text: raw,
        has_explicit_quantity: true,
      };
      parsedDirect.parse_confidence = 0.95;
      parsedDirect.parse_mode = "structured";
      return parsedDirect;
    }
    var quantityToken = null;
    for (var i = 0; i < tokens.length; i += 1) {
      if (/^\d+$/.test(tokens[i])) {
        quantityToken = tokens[i];
        break;
      }
    }
    var hasExplicitQuantity = !!quantityToken;
    var quantity = hasExplicitQuantity ? parseNumber(quantityToken, 3) : 1;

    var sentenceBody = raw;
    var timeContext = "";
    var topicPrefix = raw.match(/^(오늘|어제|방금|지금|요즘)(은|는)?\s+(.+)/);
    if (topicPrefix && topicPrefix[3]) {
      timeContext = topicPrefix[1];
      sentenceBody = topicPrefix[3].trim();
    }

    var actionToken = "";
    var actionCategoryFromPattern = "";
    for (var p = 0; p < ACTION_PATTERNS.length; p += 1) {
      if (ACTION_PATTERNS[p].regex.test(raw)) {
        actionToken = ACTION_PATTERNS[p].value;
        actionCategoryFromPattern = ACTION_PATTERNS[p].category || "";
        break;
      }
    }
    for (var j = 0; j < tokens.length; j += 1) {
      if (KNOWN_ACTIONS.indexOf(String(tokens[j]).toLowerCase()) >= 0) {
        actionToken = tokens[j];
        break;
      }
    }
    var action = normalizeActionToken(actionToken);

    var subject = tokens[0] || "늑대";
    var subjectMatch = raw.match(/^([^\d\s]+)\s*\d+\s*마리/);
    if (subjectMatch && subjectMatch[1]) {
      subject = subjectMatch[1];
    }
    var subjectByParticle = sentenceBody.match(/^(.+?)(?:이|가|은|는)\s+/);
    var derivedFromPredicate = false;
    if (subjectByParticle && subjectByParticle[1]) {
      subject = subjectByParticle[1].trim();
    }
    subject = sanitizeKoreanNoun(subject);

    var cleanedTarget = "";
    var targetMatch = raw.match(/(?:이|가|은|는)\s*([^\n]+?)(?:을|를)\s*(?:탈출|이동|공격|정찰|집결)/);
    if (targetMatch && targetMatch[1]) {
      cleanedTarget = targetMatch[1].trim();
    } else {
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
    if (subjectByParticle && subjectByParticle[2]) {
      var predicate = sentenceBody.replace(/^(.+?)(?:이|가|은|는)\s+/, "").trim();
      if (!/^(이동|공격|정찰|집결|탈출|독살|암살|살해)/.test(predicate) && predicate.length > 0) {
        action = predicate.replace(/[.]$/g, "");
        derivedFromPredicate = true;
      }
    }
    if (!action) {
      action = sentenceBody.replace(/^(.+?)(?:이|가|은|는)\s+/, "").trim();
    }
    if (action.indexOf("독살") >= 0 || action.indexOf("암살") >= 0 || action.indexOf("살해") >= 0) {
      cleanedTarget = cleanedTarget || "궁정 내부";
    }
    if (!cleanedTarget) {
      cleanedTarget = timeContext || "현장";
    }
    if (derivedFromPredicate) {
      cleanedTarget = timeContext || "일상 관찰";
    }

    var actionType = actionCategoryFromPattern || classifyActionType(action, raw);
    var countable = hasExplicitQuantity || actionType === "threat" || actionType === "tactical_move";

    var parsedResult = {
      subject: subject,
      quantity: quantity,
      action: action,
      target: cleanedTarget || "알 수 없는 장소",
      certainty: detectUncertainty(raw),
      is_countable: countable && isCountableScenario(action, raw),
      action_type: actionType,
      raw_text: raw,
      has_explicit_quantity: hasExplicitQuantity,
    };
    parsedResult.parse_confidence = calculateParseConfidence(parsedResult);
    parsedResult.parse_mode = parsedResult.parse_confidence < 0.6 ? "conservative_raw" : "structured";
    if (parsedResult.parse_mode === "conservative_raw") {
      parsedResult.action = sentenceBody;
      parsedResult.is_countable = false;
      parsedResult.quantity = 1;
      parsedResult.target = timeContext || "원문 서술";
    }
    return parsedResult;
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

      var result = executeScenario({
        trustLevel: trustLevel,
        senderStats: senderStats,
        receiverStats: receiverStats,
        infoTruthValue: {
          subject: scenarioParsed.subject,
          action: scenarioParsed.action,
          target: scenarioParsed.target,
          quantity: scenarioParsed.quantity,
          certainty: scenarioParsed.certainty,
          is_countable: scenarioParsed.is_countable,
          action_type: scenarioParsed.action_type,
          parse_confidence: scenarioParsed.parse_confidence,
          parse_mode: scenarioParsed.parse_mode,
        },
      });

      outputBase.textContent = formatJSON(result.baseInfo);
      outputSender.textContent = formatJSON(result.propagation.senderDistorted);
      outputReceiver.textContent = formatJSON(result.propagation.receiverInterpreted);
      outputKb.textContent = formatJSON(result.knowledgeBaseSnapshot);
      outputDialogueMain.value = buildReadableDialogue(result);
      saveFormState();
      appendDebug("pipeline executed");

      if (result.propagation.blocked) {
        appendDebug("propagation blocked: " + result.propagation.reason);
        outputDialogue.textContent = "정보가 전달되지 않아 대화가 생성되지 않았습니다.";
        return;
      }

      outputDialogue.textContent = result.dialogue.finalSpeech;
      appendDebug("dialogue rendered");
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

  restoreFormState();
  initCollapsiblePanels();
  render();
})();
