(function () {
  var engine = window.QuestSystem;
  var executeScenario = engine && engine.executeScenario;

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
    var nonCountablePatterns = [/기침/, /아프/, /피곤/, /바빠/, /컨디션/, /소문/, /긴장/, /초조/, /같다/, /보인다/];
    for (var i = 0; i < nonCountablePatterns.length; i += 1) {
      if (nonCountablePatterns[i].test(text)) return false;
    }
    return true;
  }

  function classifyActionType(actionText, rawText) {
    var text = (String(actionText || "") + " " + String(rawText || "")).toLowerCase();
    if (/(독살|암살|살해|공격|습격|납치|피습)/.test(text)) return "threat";
    if (/(정찰|침투|매복|집결|추적)/.test(text)) return "tactical_move";
    if (/(먹|마시|자|취침|휴식|대화|일하|근무|장사|요리|청소|정리|산책|준비|수리)/.test(text)) return "routine";
    if (/(바쁘|피곤|아프|기침|긴장|초조|불안)/.test(text)) return "state";
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
    var map = {
      이동: "이동",
      move: "이동",
      attack: "공격",
      공격: "공격",
      gather: "집결",
      집결: "집결",
      talk: "대화",
      대화: "대화",
      observe: "정찰",
      정찰: "정찰",
      travel: "이동",
      탈출: "탈출",
      escape: "탈출",
    };
    var token = String(action || "").toLowerCase();
    return map[token] || action || "";
  }

  function parseScenarioText(scenarioText) {
    var raw = normalizeUserText(scenarioText);
    if (!raw) {
      return {
        subject: "wolf",
        quantity: 3,
        action: "move",
        target: "north gate",
      };
    }

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

    var knownActions = [
      "move",
      "attack",
      "gather",
      "talk",
      "observe",
      "travel",
      "escape",
      "이동",
      "공격",
      "집결",
      "대화",
      "정찰",
      "탈출",
      "독살",
      "암살",
      "살해",
      "기침",
      "먹",
      "먹었다",
      "마셨",
      "잤",
      "일했",
      "장사",
      "요리",
      "청소",
      "준비",
      "수리",
      "정리",
    ];
    var actionToken = "";
    var actionPatterns = [
      { regex: /탈출(했|한|하는|중)/, value: "탈출" },
      { regex: /이동(했|한|하는|중)/, value: "이동" },
      { regex: /공격(했|한|하는|중)/, value: "공격" },
      { regex: /정찰(했|한|하는|중)/, value: "정찰" },
      { regex: /집결(했|한|하는|중)/, value: "집결" },
      { regex: /독살/, value: "독살당했다" },
      { regex: /암살/, value: "암살당했다" },
      { regex: /살해/, value: "살해되었다" },
      { regex: /바빠보인|바쁘다/, value: "바빠 보인다" },
      { regex: /피곤해보인|피곤하다/, value: "피곤해 보인다" },
      { regex: /기침/, value: "기침을 많이 했다" },
      { regex: /밥을?\s*먹/, value: "밥을 먹었다" },
      { regex: /식사(를)?\s*했/, value: "식사했다" },
      { regex: /음식(을)?\s*먹/, value: "음식을 먹었다" },
      { regex: /마셨|마시/, value: "무언가를 마셨다" },
      { regex: /잠을?\s*잤|취침/, value: "잠을 잤다" },
      { regex: /일했|근무/, value: "일을 했다" },
      { regex: /장사/, value: "장사를 했다" },
      { regex: /요리/, value: "요리를 했다" },
    ];
    for (var p = 0; p < actionPatterns.length; p += 1) {
      if (actionPatterns[p].regex.test(raw)) {
        actionToken = actionPatterns[p].value;
        break;
      }
    }
    for (var j = 0; j < tokens.length; j += 1) {
      if (knownActions.indexOf(String(tokens[j]).toLowerCase()) >= 0) {
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

    var actionType = classifyActionType(action, raw);
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
