(function () {
  var engine = window.QuestSystem;
  var parser = window.NpcParser || {};
  var executeScenario = engine && engine.executeScenario;
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
      var subjectText = parser.sanitizeKoreanNoun(String(d.subject || "대상"));
      var actionText = parser.normalizeActionPhrase(String(d.action || ""));
      var naturalAction = /(다|했다|중이다|보인다|상태다)$/.test(actionText) ? actionText : actionText + "했다";
      var targetText = parser.sanitizeKoreanNoun(String(d.target || "현장"));
      var quantityText = d.is_countable ? String(d.quantity) : "해당 없음";
      lines.push(
        "[" +
          (item.info_id || "F" + (i + 1)) +
          "] " +
          subjectText +
          parser.pickKoreanParticle(subjectText, "이", "가") +
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

  function render() {
    try {
      appendDebug("render start");
      if (typeof executeScenario !== "function") {
        appendDebug("executeScenario is not a function");
        outputDialogueMain.value = "엔진 로드 실패: quest-system.js를 먼저 로드해야 합니다.";
        outputDialogue.textContent = "엔진 로드 실패";
        return;
      }
      if (!parser.parseScenarioText) {
        appendDebug("NpcParser not loaded");
        outputDialogueMain.value = "파서 로드 실패: npc-parser.js를 먼저 로드해야 합니다.";
        outputDialogue.textContent = "파서 로드 실패";
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
      var scenarioParsed = parser.parseScenarioText(formData.get("scenarioText"));
      appendDebug("parse mode: " + scenarioParsed.parse_mode + ", confidence: " + scenarioParsed.parse_confidence);

      var primaryFact = parser.selectPrimaryParse(scenarioParsed) || scenarioParsed;
      appendDebug("primary fact: " + (primaryFact.action_type || "unknown") + " / " + String(primaryFact.action || ""));

      var facts = scenarioParsed.facts && scenarioParsed.facts.length
        ? scenarioParsed.facts
        : parser.buildFactsFromParsed(scenarioParsed);
      var reputationMap = parseReputationMap(formData.get("subjectReputation"));
      var senderReputation = parseReputationMap(formData.get("senderReputation")) || reputationMap;
      var receiverReputation = parseReputationMap(formData.get("receiverReputation")) || reputationMap;
      var quantityMode = String(formData.get("quantityMode") || "dramatic");
      var allowPartialTrust = String(formData.get("allowPartialTrust") || "on") === "on";

      if (outputParsePipeline) {
        outputParsePipeline.textContent = parser.buildParsePipelineTrace(scenarioParsed, primaryFact, facts);
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
            fallbackStyle: res.dialogue.fallbackStyle || null,
            anchorValidation: res.anchorValidation || res.dialogue.anchorValidation || null,
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

  if (form && runButton) {
    restoreFormState();
    initCollapsiblePanels();
    render();
  }
})();
