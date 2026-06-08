(function () {
  var questSelect = document.querySelector("#quest-select");
  var giverSelect = document.querySelector("#quest-giver-select");
  var questReportText = document.querySelector("#quest-report-text");
  var compareQuestBtn = document.querySelector("#run-quest-compare");
  var fullFlowBtn = document.querySelector("#run-quest-full-flow");
  var resetQuestBtn = document.querySelector("#run-quest-reset");
  var outQuest = document.querySelector("#out-quest");
  var outQuestCompare = document.querySelector("#out-quest-compare");
  var outAcceptLine = document.querySelector("#out-accept-line");
  var outGameState = document.querySelector("#out-game-state");
  var loadStatus = document.querySelector("#load-status");
  var questPhase = document.querySelector("#quest-phase");
  var completionBanner = document.querySelector("#completion-banner");
  var outCompletionLine = document.querySelector("#out-completion-line");
  var failureBanner = document.querySelector("#failure-banner");
  var outFailureLine = document.querySelector("#out-failure-line");
  var stepsBlock = document.querySelector("#steps-block");
  var outQuestSteps = document.querySelector("#out-quest-steps");
  var resultSummary = document.querySelector("#result-summary");
  var summaryBranch = document.querySelector("#summary-branch");
  var summaryGold = document.querySelector("#summary-gold");
  var summaryDestination = document.querySelector("#summary-destination");
  var summaryQuantity = document.querySelector("#summary-quantity");

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

  function setStatus(message, isError) {
    if (!loadStatus) return;
    loadStatus.textContent = message;
    loadStatus.classList.toggle("status-error", Boolean(isError));
  }

  function setPhase(text, state) {
    if (!questPhase) return;
    questPhase.textContent = text;
    questPhase.className = "quest-phase";
    if (state) questPhase.classList.add("phase-" + state);
  }

  function ensureCatalog() {
    if (!window.QuestRuntime) return null;
    if (!window.QuestRuntime.getQuestCatalog()) {
      window.QuestRuntime.loadQuestCatalog("./quests-draft.json");
    }
    return window.QuestRuntime.getQuestCatalog();
  }

  function repSessionKey() {
    return "default";
  }

  function questSessionKey() {
    return "ui-quest-" + repSessionKey();
  }

  function flowOptions(report) {
    return {
      questId: questSelect.value,
      giverId: giverSelect.value,
      scenarioText: report,
      engine: window.QuestSystem,
      parser: parserApi,
      sessionKey: questSessionKey(),
      reputationSessionKey: repSessionKey(),
    };
  }

  function refreshGameStatePanel() {
    if (!outGameState || !window.QuestGameState) return;
    outGameState.textContent = formatJSON(window.QuestGameState.snapshot(questSessionKey()));
  }

  function interpretedMetrics(run) {
    if (run.outcomeBranch && run.outcomeBranch.metrics) {
      return run.outcomeBranch.metrics;
    }
    var facts =
      (run.engineResult &&
        run.engineResult.propagation &&
        run.engineResult.propagation.interpretedFacts) ||
      [];
    if (window.QuestRuntime && window.QuestRuntime.aggregateInterpretedMetrics) {
      return window.QuestRuntime.aggregateInterpretedMetrics(facts);
    }
    return { quantity: 0, certainty: 0 };
  }

  function showResultSummary(show) {
    if (!resultSummary) return;
    resultSummary.hidden = !show;
  }

  function showCompletion(show, line) {
    if (completionBanner) completionBanner.hidden = !show;
    if (outCompletionLine) outCompletionLine.textContent = line || "";
    if (show && failureBanner) failureBanner.hidden = true;
  }

  function showFailure(show, line) {
    if (failureBanner) failureBanner.hidden = !show;
    if (outFailureLine) outFailureLine.textContent = line || "";
    if (show && completionBanner) completionBanner.hidden = true;
  }

  function showSteps(show) {
    if (stepsBlock) stepsBlock.hidden = !show;
  }

  function refreshResultSummary(run) {
    if (!resultSummary || !run) return;
    var branch = run.outcomeBranch && run.outcomeBranch.branchId;
    var gold = run.outcome && run.outcome.rewards && run.outcome.rewards.gold;
    var destination =
      run.outcome && run.outcome.worldFlags && run.outcome.worldFlags.cargo_destination;
    var metrics = interpretedMetrics(run);

    if (summaryBranch) summaryBranch.textContent = branch || "—";
    if (summaryGold) summaryGold.textContent = gold != null ? String(gold) : "—";
    if (summaryDestination) summaryDestination.textContent = destination || "—";
    if (summaryQuantity) {
      summaryQuantity.textContent =
        metrics && metrics.quantity != null ? String(metrics.quantity) : "—";
    }
    showResultSummary(true);
  }

  function clearResultSummary() {
    showResultSummary(false);
    if (summaryBranch) summaryBranch.textContent = "—";
    if (summaryGold) summaryGold.textContent = "—";
    if (summaryDestination) summaryDestination.textContent = "—";
    if (summaryQuantity) summaryQuantity.textContent = "—";
  }

  function renderSteps(steps) {
    if (!outQuestSteps) return;
    outQuestSteps.innerHTML = "";
    if (!steps || !steps.length) {
      showSteps(false);
      return;
    }
    steps.forEach(function (step) {
      var li = document.createElement("li");
      var label = step.title || step.type || "단계";
      var detail = step.summary || step.npcLine || "";
      li.textContent = label + (detail ? " — " + detail : "");
      if (step.status === "failed") li.classList.add("step-failed");
      outQuestSteps.appendChild(li);
    });
    showSteps(true);
  }

  function printQuestResult(run, flowMeta) {
    if (!outQuest) return;
    outQuest.textContent = formatJSON({
      completed: run.completion.completed,
      questState: flowMeta && flowMeta.instance && flowMeta.instance.state,
      quest: run.questId,
      giver: run.giverId,
      outcomeBranch: run.outcomeBranch,
      outcome: run.outcome,
      completionDialogue: flowMeta && flowMeta.completionDialogue,
      interpretedFacts: (run.engineResult.propagation.interpretedFacts || []).map(function (item) {
        var tv = item.truth_value;
        return {
          subject: tv.subject,
          quantity: tv.quantity,
          certainty: tv.certainty,
        };
      }),
    });
    refreshResultSummary(run);
    refreshGameStatePanel();
  }

  function refreshAcceptDialoguePreview() {
    if (!outAcceptLine || !window.QuestRuntime || !giverSelect || !questSelect) return;
    if (!questSelect.value || !giverSelect.value) {
      outAcceptLine.textContent = "의뢰인을 선택하세요.";
      return;
    }
    try {
      var resolved = window.QuestRuntime.getAcceptDialogue(
        questSelect.value,
        giverSelect.value,
        { sessionKey: repSessionKey(), reputationSessionKey: repSessionKey() }
      );
      outAcceptLine.textContent = resolved && resolved.line ? resolved.line : "—";
    } catch (err) {
      outAcceptLine.textContent = "오류: " + (err.message || err);
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
      el.textContent = opt.label;
      giverSelect.appendChild(el);
    });
    refreshAcceptDialoguePreview();
  }

  function clearCompletionUi() {
    clearResultSummary();
    showCompletion(false);
    showFailure(false);
    showSteps(false);
    if (outQuestSteps) outQuestSteps.innerHTML = "";
    setPhase("대기 중", "idle");
  }

  function initQuestUi() {
    if (location.protocol === "file:") {
      setStatus(
        "file:// 로는 JSON을 불러올 수 없습니다. 터미널에서 python -m http.server 5500 실행 후 http://127.0.0.1:5500/index.html 로 여세요.",
        true
      );
      return;
    }
    if (!window.QuestRuntime || !window.QuestSystem || !window.NpcParser) {
      setStatus("엔진 스크립트 로드 실패. 페이지를 새로고침(Ctrl+F5)하세요.", true);
      return;
    }
    var cat = ensureCatalog();
    if (!cat || !cat.quests || !cat.quests.length) {
      setStatus("quests-draft.json 로드 실패. 로컬 서버로 실행 중인지 확인하세요.", true);
      return;
    }
    if (!questSelect) return;

    questSelect.innerHTML = "";
    cat.quests.forEach(function (q) {
      var el = document.createElement("option");
      el.value = q.id;
      el.textContent = q.title;
      questSelect.appendChild(el);
    });
    refreshGiverOptions();
    refreshGameStatePanel();
    clearCompletionUi();
    setStatus("준비 완료. 「퀘스트 실행」 = 수락 → 조사 → 보고 → 완료");
  }

  function turnInCompleted(turnIn) {
    return Boolean(turnIn && turnIn.completion && turnIn.completion.completed);
  }

  function turnInFailReason(flowResult, turnIn) {
    if (turnIn && turnIn.completion && turnIn.completion.reason) {
      return turnIn.completion.reason;
    }
    if (flowResult && flowResult.reason) {
      return flowResult.reason;
    }
    return "보고 조건 미충족 (마약·화물·장소 등 핵심 정보 필요)";
  }

  function onRunQuest() {
    if (!outQuest || !window.QuestRuntime) return;
    try {
      window.QuestRuntime.clearQuestFlow(questSessionKey());
      setStatus("퀘스트 진행 중… (수락 → 단계 → 보고)");
      setPhase("진행 중", "active");
      showCompletion(false);
      showFailure(false);

      var report = questReportText ? String(questReportText.value || "").trim() : "";
      if (!report) {
        outQuest.textContent = formatJSON({
          ok: false,
          reason: "조사 보고를 입력하세요.",
        });
        clearResultSummary();
        setPhase("실패", "failed");
        showFailure(true, "보고 문장이 비어 있습니다. 현장에서 확인한 내용을 입력하세요.");
        setStatus("보고 문장을 입력한 뒤 다시 실행하세요.", true);
        return;
      }

      var flowResult = window.QuestRuntime.runQuestFlow(flowOptions(report));

      renderSteps(flowResult.steps || []);

      var turnIn = flowResult.turnInResult;
      if (!turnInCompleted(turnIn)) {
        var failReason = turnInFailReason(flowResult, turnIn);
        outQuest.textContent = formatJSON({
          ok: false,
          completed: false,
          reason: failReason,
          steps: flowResult.steps,
          completion: turnIn && turnIn.completion,
        });
        clearResultSummary();
        setPhase("실패", "failed");
        showFailure(true, failReason);
        setStatus("퀘스트 실패: " + failReason, true);
        refreshGameStatePanel();
        return;
      }

      printQuestResult(turnIn, flowResult);
      showCompletion(true, flowResult.completionDialogue || "수고했네.");

      var gold = turnIn.outcome && turnIn.outcome.rewards && turnIn.outcome.rewards.gold;
      var branch = turnIn.outcomeBranch && turnIn.outcomeBranch.branchId;
      setPhase("완료", "done");
      setStatus(
        "퀘스트 완료 — " +
          (branch || "분기 없음") +
          ", 골드 +" +
          (gold != null ? gold : 0)
      );
    } catch (err) {
      outQuest.textContent = "오류: " + (err.message || String(err));
      clearResultSummary();
      showCompletion(false);
      showFailure(false);
      setPhase("오류", "failed");
      setStatus("실행 오류: " + (err.message || String(err)), true);
    }
  }

  function onCompareQuest() {
    if (!outQuestCompare || !window.QuestRuntime) return;
    try {
      setStatus("의뢰인 비교 중…");
      var report = questReportText ? questReportText.value : "";
      var questId = questSelect.value;
      var options = window.QuestRuntime.listQuestGiverOptions(questId);
      var rows = [];
      options.forEach(function (opt) {
        try {
          var run = window.QuestRuntime.runQuestTurnIn({
            questId: questId,
            giverId: opt.giverId,
            scenarioText: report,
            engine: window.QuestSystem,
            parser: parserApi,
            sessionKey: questSessionKey() + "-compare-" + opt.giverId,
            reputationSessionKey: repSessionKey(),
            applyGameState: false,
          });
          var metrics = interpretedMetrics(run);
          rows.push({
            giver: opt.giverId,
            quantity: metrics.quantity,
            branch: run.outcomeBranch && run.outcomeBranch.branchId,
            gold: run.outcome && run.outcome.rewards && run.outcome.rewards.gold,
            destination:
              run.outcome && run.outcome.worldFlags && run.outcome.worldFlags.cargo_destination,
            completed: run.completion.completed,
          });
        } catch (e) {
          rows.push({ giver: opt.giverId, error: e.message });
        }
      });
      outQuestCompare.textContent = formatJSON(rows);
      setStatus("의뢰인 비교 완료 (완료 여부·분기·골드)");
    } catch (err) {
      setStatus("비교 오류: " + (err.message || String(err)), true);
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
    clearCompletionUi();
    if (outQuest) outQuest.textContent = "퀘스트 실행 전";
    if (outQuestCompare) outQuestCompare.textContent = "「의뢰인 비교」 버튼을 누르세요.";
    refreshGameStatePanel();
    setStatus("초기화됨. 「퀘스트 실행」을 누르세요.");
  }

  if (questSelect) {
    questSelect.addEventListener("change", refreshGiverOptions);
  }
  if (giverSelect) {
    giverSelect.addEventListener("change", refreshAcceptDialoguePreview);
  }
  if (compareQuestBtn) {
    compareQuestBtn.addEventListener("click", onCompareQuest);
  }
  if (fullFlowBtn) {
    fullFlowBtn.addEventListener("click", onRunQuest);
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
