/**
 * In-browser / Node game state for quest outcomes (gold, worldFlags, active instance).
 * UE 연동 전 로컬 세이브·데모용. 언리얼 브릿지 없음.
 */
(function (global) {
  var SESSION_STORE = new Map();

  function defaultState() {
    return {
      gold: 0,
      xp: 0,
      villageReputation: 50,
      worldFlags: {},
      activeQuest: null,
      questHistory: [],
    };
  }

  function getState(sessionKey) {
    var key = String(sessionKey || "default");
    if (!SESSION_STORE.has(key)) {
      var base = defaultState();
      if (global.QuestSystem && global.QuestSystem.getProfileRegistry) {
        var p = global.QuestSystem.getProfileRegistry().player;
        if (p && typeof p.initialVillageReputation === "number") {
          base.villageReputation = p.initialVillageReputation;
        }
      }
      SESSION_STORE.set(key, base);
    }
    return SESSION_STORE.get(key);
  }

  function clearState(sessionKey) {
    SESSION_STORE.delete(String(sessionKey || "default"));
  }

  function applyTurnInOutcome(sessionKey, turnInResult) {
    var state = getState(sessionKey);
    if (!turnInResult || !turnInResult.completion || !turnInResult.completion.completed) {
      return { applied: false, state: state, reason: "quest not completed" };
    }
    var effects = turnInResult.outcome || {};
    var rewards = effects.rewards || {};
    if (typeof rewards.gold === "number") state.gold += rewards.gold;
    if (typeof rewards.xp === "number") state.xp += rewards.xp;
    if (effects.worldFlags && typeof effects.worldFlags === "object") {
      Object.keys(effects.worldFlags).forEach(function (k) {
        state.worldFlags[k] = effects.worldFlags[k];
      });
    }
    if (turnInResult.reputationResult && typeof turnInResult.reputationResult.villageReputation === "number") {
      state.villageReputation = turnInResult.reputationResult.villageReputation;
    }
    var entry = {
      questId: turnInResult.questId,
      giverId: turnInResult.giverId,
      branchId: turnInResult.outcomeBranch && turnInResult.outcomeBranch.branchId,
      gold: rewards.gold,
      worldFlags: effects.worldFlags || {},
      at: Date.now(),
    };
    state.questHistory.push(entry);
    if (state.activeQuest) {
      state.activeQuest.state = "completed";
      state.activeQuest.branchId = entry.branchId;
    }
    return { applied: true, state: state, entry: entry };
  }

  function setActiveQuest(sessionKey, instance, meta) {
    var state = getState(sessionKey);
    state.activeQuest = {
      instance: instance,
      questId: meta.questId,
      giverId: meta.giverId,
      introDialogue: meta.introDialogue || "",
      acceptLine: meta.acceptLine || "",
      state: instance.state || "active",
    };
    return state.activeQuest;
  }

  function markTurnInFailed(sessionKey, reason) {
    var state = getState(sessionKey);
    if (state.activeQuest) {
      state.activeQuest.state = "turn_in_failed";
      state.activeQuest.failReason = reason || "보고 조건 미충족";
    }
    return state.activeQuest;
  }

  function snapshot(sessionKey) {
    var s = getState(sessionKey);
    return JSON.parse(JSON.stringify(s));
  }

  function exportState(sessionKey) {
    return snapshot(sessionKey);
  }

  function importState(sessionKey, data) {
    var key = String(sessionKey || "default");
    var next = defaultState();
    if (data && typeof data === "object") {
      Object.assign(next, data);
    }
    SESSION_STORE.set(key, next);
    return next;
  }

  global.QuestGameState = {
    getState: getState,
    clearState: clearState,
    applyTurnInOutcome: applyTurnInOutcome,
    setActiveQuest: setActiveQuest,
    markTurnInFailed: markTurnInFailed,
    snapshot: snapshot,
    exportState: exportState,
    importState: importState,
  };
})(typeof window !== "undefined" ? window : globalThis);
