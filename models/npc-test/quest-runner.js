(function () {
  var questSelect = document.querySelector("#quest-select");
  var giverSelect = document.querySelector("#quest-giver-select");
  var questReportText = document.querySelector("#quest-report-text");
  var runQuestBtn = document.querySelector("#run-quest-turn-in");
  var compareQuestBtn = document.querySelector("#run-quest-compare");
  var outQuest = document.querySelector("#out-quest");
  var outQuestCompare = document.querySelector("#out-quest-compare");

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
      questReportText.value = "북문에서 늑대 3마리가 탈출했다";
    }
    refreshGiverOptions();
  }

  function runSingleGiver(giverId, scenarioText) {
    return window.QuestRuntime.runQuestTurnIn({
      questId: questSelect.value,
      giverId: giverId,
      scenarioText: scenarioText,
      engine: window.QuestSystem,
      parser: parserApi,
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
        blocked: engineResult.propagation.blocked,
        dialogue: engineResult.dialogue,
        interpretedFacts: (run.engineResult.propagation.interpretedFacts || []).map(function (item) {
          var tv = item.truth_value;
          return {
            subject: tv.subject,
            action: tv.action,
            target: tv.target,
            object: tv.object,
            quantity: tv.quantity,
            certainty: tv.certainty,
            action_type: tv.action_type,
            applied_rules: item.metadata.applied_rules,
          };
        }),
        experienceFlavor: run.experience.expectedDistortionFlavor,
      });
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
          action_type: snap.action_type,
          blocked: run.engineResult.propagation.blocked,
          sameOutcome: run.completion.completed ? run.outcome : null,
        });
      } catch (e) {
        rows.push({ giver: opt.giverId, error: e.message });
      }
    });
    outQuestCompare.textContent = formatJSON({
      questId: questId,
      report: report,
      paths: rows,
      note: "completed 경로는 동일 outcome (gold/xp/worldFlags)",
    });
  }

  if (questSelect) {
    questSelect.addEventListener("change", refreshGiverOptions);
  }
  if (runQuestBtn) {
    runQuestBtn.addEventListener("click", onRunQuest);
  }
  if (compareQuestBtn) {
    compareQuestBtn.addEventListener("click", onCompareQuest);
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", initQuestUi);
  } else {
    initQuestUi();
  }
})();
