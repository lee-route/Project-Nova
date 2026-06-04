(function () {
  var questSelect = document.querySelector("#quest-select");
  var giverSelect = document.querySelector("#quest-giver-select");
  var questReportText = document.querySelector("#quest-report-text");
  var runQuestBtn = document.querySelector("#run-quest-turn-in");
  var compareQuestBtn = document.querySelector("#run-quest-compare");
  var processStepsBtn = document.querySelector("#run-quest-process-steps");
  var acceptQuestBtn = document.querySelector("#run-quest-accept");
  var nextStepBtn = document.querySelector("#run-quest-next-step");
  var fullFlowBtn = document.querySelector("#run-quest-full-flow");
  var resetQuestBtn = document.querySelector("#run-quest-reset");
  var outQuest = document.querySelector("#out-quest");
  var outQuestCompare = document.querySelector("#out-quest-compare");
  var outAcceptDialogue = document.querySelector("#out-accept-dialogue");
  var outQuestSteps = document.querySelector("#out-quest-steps");
  var outQuestFlow = document.querySelector("#out-quest-flow");
  var outGameState = document.querySelector("#out-game-state");
  var refreshAcceptBtn = document.querySelector("#refresh-accept-dialogue");

  var parserApi = {
    parseScenarioText: function (text) {
      if (window.NpcParser && window.NpcParser.parseScenarioText) {
        return window.NpcParser.parseScenarioText(text);
      }
      return null;
    },
    buildFactsFromParsed: function (parsed) {
      if (window.NpcParser && window.NpcParser.buildFactsFromParsed) {
        return window.NpcParser.buildFactsFromParsed(parsed);
      }
      return [];
    },
  };

  function formatJSON(v) {
    return JSON.stringify(v, null, 2);
  }

  function ensureCatalog() {
    if (!window.QuestRuntime) return null;
    if (!window.QuestRuntime.getQuestCatalog()) {
      window.QuestRuntime.loadQuestCatalog("./quests-draft.json");
    }
    return window.QuestRuntime.getQuestCatalog();
  }

  function repSessionKey() {
    return document.getElementById("rep-persist-session") &&
      document.getElementById("rep-persist-session").checked
      ? "ui-reputation"
      : "default";
  }

  function questSessionKey() {
    return "ui-quest-" + repSessionKey();
  }

  function refreshGameStatePanel() {
    if (!outGameState || !window.QuestGameState) return;
    outGameState.textContent = formatJSON(window.QuestGameState.snapshot(questSessionKey()));
  }

  function refreshAcceptDialoguePreview() {
    if (!outAcceptDialogue || !window.QuestRuntime || !giverSelect || !questSelect) return;
    try {
      var resolved = window.QuestRuntime.getAcceptDialogue(
        questSelect.value,
        giverSelect.value,
        { sessionKey: repSessionKey(), reputationSessionKey: repSessionKey() }
      );
      outAcceptDialogue.textContent = formatJSON(resolved);
    } catch (err) {
      outAcceptDialogue.textContent = "Error: " + (err.message || err);
    }
  }

  window.refreshAcceptDialoguePreview = refreshAcceptDialoguePreview;

  function refreshGiverOptions() {
    if (!giverSelect || !questSelect || !window.QuestRuntime) return;
    var questId = questSelect.value;
    var options = window.QuestRuntime.listQuestGiverOptions(questId);
    giverSelect.innerHTML = "";
    options.forEach(function (opt) {
      var el = document.createElement("option");
      el.value = opt.giverId;
      el.textContent = opt.label + " → " + opt.turnInProfileKey;
      giverSelect.appendChild(el);
    });
    refreshAcceptDialoguePreview();
  }

  function initQuestUi() {
    if (!questSelect || !window.QuestRuntime) return;
    ensureCatalog();
    var cat = window.QuestRuntime.getQuestCatalog();
    if (!cat || !cat.quests) return;
    questSelect.innerHTML = "";
    cat.quests.forEach(function (q) {
      var el = document.createElement("option");
      el.value = q.id;
      el.textContent = q.title + " (" + q.id + ")";
      questSelect.appendChild(el);
    });
    if (questReportText && !questReportText.value) {
      questReportText.value =
        "안개 계곡에 밀수 화물이 버려져 있다. 마약초 열두 개가 들어 있다";
    }
    refreshGiverOptions();
    refreshGameStatePanel();
  }

  function runSingleGiver(giverId, scenarioText) {
    return window.QuestRuntime.runQuestTurnIn({
      questId: questSelect.value,
      giverId: giverId,
      scenarioText: scenarioText,
      engine: window.QuestSystem,
      parser: parserApi,
      sessionKey: questSessionKey() + "-" + giverId,
      reputationSessionKey: repSessionKey(),
    });
  }

  function onRunQuest() {
    if (!outQuest) return;
    try {
      var report = questReportText ? questReportText.value : "";
      var run = runSingleGiver(giverSelect.value, report);
      function printQuestResult(engineResult) {
        outQuest.textContent = formatJSON({
          quest: run.questId,
          giver: run.giverId,
          turnIn: run.turnInProfileKey,
          completed: run.completion.completed,
          completion: run.completion,
          outcome: run.outcome,
          outcomeBranch: run.outcomeBranch,
          blocked: engineResult.propagation.blocked,
          dialogue: engineResult.dialogue,
          interpretedFacts: (run.engineResult.propagation.interpretedFacts || []).map(function (item) {
            var tv = item.truth_value;
            return {
              subject: tv.subject,
              action: tv.action,
              target: tv.target,
              quantity: tv.quantity,
              certainty: tv.certainty,
              applied_rules: item.metadata.applied_rules,
            };
          }),
          experienceFlavor: run.experience.expectedDistortionFlavor,
          reputationResult: run.reputationResult,
          gameStateApplied: window.QuestGameState
            ? window.QuestGameState.applyTurnInOutcome(questSessionKey(), run)
            : null,
        });
        refreshGameStatePanel();
      }
      if (typeof window.refreshReputationPanel === "function") {
        window.refreshReputationPanel();
      }
      if (
        window.LlmAdapter &&
        window.LlmAdapter.isLive() &&
        run.engineResult.dialogue &&
        run.engineResult.dialogue.llmPending
      ) {
        outQuest.textContent = "Live LLM 생성 중…";
        window.LlmAdapter.enrichDialogueResult(
          run.engineResult,
          run.engineResult.receiver && run.engineResult.receiver.persona
        )
          .then(printQuestResult)
          .catch(function (err) {
            outQuest.textContent = "LLM error: " + err.message;
            printQuestResult(run.engineResult);
          });
        return;
      }
      if (window.QuestGameState && run.completion.completed) {
        window.QuestGameState.applyTurnInOutcome(questSessionKey(), run);
      }
      printQuestResult(run.engineResult);
    } catch (err) {
      outQuest.textContent = "Error: " + (err.message || String(err));
    }
  }

  function onCompareQuest() {
    if (!outQuestCompare || !window.QuestRuntime) return;
    var report = questReportText ? questReportText.value : "";
    var questId = questSelect.value;
    var options = window.QuestRuntime.listQuestGiverOptions(questId);
    var rows = [];
    options.forEach(function (opt) {
      try {
        var run = runSingleGiver(opt.giverId, report);
        var first = run.engineResult.propagation.interpretedFacts;
        var snap = first && first[0] ? first[0].truth_value : {};
        rows.push({
          giver: opt.giverId,
          turnIn: opt.turnInProfileKey,
          completed: run.completion.completed,
          quantity: snap.quantity,
          certainty: Number(snap.certainty && snap.certainty.toFixed ? snap.certainty.toFixed(2) : snap.certainty),
          blocked: run.engineResult.propagation.blocked,
          branchId: run.outcomeBranch && run.outcomeBranch.branchId,
          gold: run.outcome && run.outcome.rewards && run.outcome.rewards.gold,
          worldFlags: run.outcome && run.outcome.worldFlags,
        });
      } catch (e) {
        rows.push({ giver: opt.giverId, error: e.message });
      }
    });
    outQuestCompare.textContent = formatJSON({
      questId: questId,
      report: report,
      paths: rows,
      note: "의뢰인별 quantityMode·왜곡 → interpreted metrics·outcomes[] 분기가 달라질 수 있음",
    });
  }

  function onRunProcessSteps() {
    if (!outQuestSteps || !window.QuestRuntime) return;
    try {
      var report = questReportText ? questReportText.value : "";
      var run = window.QuestRuntime.runProcessSteps({
        questId: questSelect.value,
        giverId: giverSelect.value,
        scenarioText: report,
        engine: window.QuestSystem,
        parser: parserApi,
        sessionKey: questSessionKey(),
        reputationSessionKey: repSessionKey(),
      });
      if (window.QuestGameState && run.turnInResult && run.turnInResult.completion.completed) {
        window.QuestGameState.applyTurnInOutcome(questSessionKey(), run.turnInResult);
      }
      outQuestSteps.textContent = formatJSON(run);
      refreshGameStatePanel();
    } catch (err) {
      outQuestSteps.textContent = "Error: " + (err.message || String(err));
    }
  }

  function onAcceptQuest() {
    if (!outQuestFlow || !window.QuestRuntime) return;
    try {
      var res = window.QuestRuntime.acceptQuest(questSelect.value, giverSelect.value, {
        sessionKey: questSessionKey(),
        reputationSessionKey: repSessionKey(),
      });
      outQuestFlow.textContent = formatJSON(res);
      refreshGameStatePanel();
    } catch (err) {
      outQuestFlow.textContent = "Error: " + (err.message || String(err));
    }
  }

  function onNextStep() {
    if (!outQuestFlow || !window.QuestRuntime) return;
    try {
      var report = questReportText ? questReportText.value : "";
      var res = window.QuestRuntime.advanceProcessStep({
        sessionKey: questSessionKey(),
        scenarioText: report,
        engine: window.QuestSystem,
        parser: parserApi,
        reputationSessionKey: repSessionKey(),
      });
      var flow = window.QuestRuntime.getQuestFlow(questSessionKey());
      outQuestFlow.textContent = formatJSON({ advance: res, log: flow && flow.log });
      refreshGameStatePanel();
      if (typeof window.refreshReputationPanel === "function") {
        window.refreshReputationPanel();
      }
    } catch (err) {
      outQuestFlow.textContent = "Error: " + (err.message || String(err));
    }
  }

  function onFullFlow() {
    if (!outQuestFlow || !window.QuestRuntime) return;
    try {
      var report = questReportText ? questReportText.value : "";
      var res = window.QuestRuntime.runQuestFlow({
        questId: questSelect.value,
        giverId: giverSelect.value,
        scenarioText: report,
        engine: window.QuestSystem,
        parser: parserApi,
        sessionKey: questSessionKey(),
        reputationSessionKey: repSessionKey(),
      });
      outQuestFlow.textContent = formatJSON(res);
      if (outQuestSteps && res.steps) {
        outQuestSteps.textContent = formatJSON(res.steps);
      }
      refreshGameStatePanel();
      if (typeof window.refreshReputationPanel === "function") {
        window.refreshReputationPanel();
      }
    } catch (err) {
      outQuestFlow.textContent = "Error: " + (err.message || String(err));
    }
  }

  function onResetQuest() {
    if (window.QuestRuntime) {
      window.QuestRuntime.clearQuestFlow(questSessionKey());
    }
    if (window.QuestGameState) {
      window.QuestGameState.clearState(questSessionKey());
      window.QuestGameState.getState(questSessionKey());
    }
    if (outQuestFlow) outQuestFlow.textContent = "퀘스트 세션 초기화됨";
    refreshGameStatePanel();
  }

  if (questSelect) {
    questSelect.addEventListener("change", refreshGiverOptions);
  }
  if (giverSelect) {
    giverSelect.addEventListener("change", refreshAcceptDialoguePreview);
  }
  if (refreshAcceptBtn) {
    refreshAcceptBtn.addEventListener("click", refreshAcceptDialoguePreview);
  }
  if (runQuestBtn) {
    runQuestBtn.addEventListener("click", onRunQuest);
  }
  if (compareQuestBtn) {
    compareQuestBtn.addEventListener("click", onCompareQuest);
  }
  if (processStepsBtn) {
    processStepsBtn.addEventListener("click", onRunProcessSteps);
  }
  if (acceptQuestBtn) {
    acceptQuestBtn.addEventListener("click", onAcceptQuest);
  }
  if (nextStepBtn) {
    nextStepBtn.addEventListener("click", onNextStep);
  }
  if (fullFlowBtn) {
    fullFlowBtn.addEventListener("click", onFullFlow);
  }
  if (resetQuestBtn) {
    resetQuestBtn.addEventListener("click", onResetQuest);
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", initQuestUi);
  } else {
    initQuestUi();
  }
})();
