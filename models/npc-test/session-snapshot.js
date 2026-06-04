/**
 * 서버 RAM 세션 스냅샷 — UE가 authoritative save. dev/디버그용 export·import.
 */
(function (global) {
  function exportSession(sessionKey) {
    var key = String(sessionKey || "default");
    var snap = {
      version: 1,
      sessionKey: key,
      exportedAt: Date.now(),
      authoritativeSave: "unreal_engine",
      note: "Server process memory only; restart clears unless UE posts import.",
      gameState: null,
      reputation: null,
      gameClock: null,
      knowledgeLayers: null,
    };
    if (global.QuestGameState && global.QuestGameState.exportState) {
      snap.gameState = global.QuestGameState.exportState(key);
    }
    if (global.ReputationSystem && global.ReputationSystem.exportState) {
      snap.reputation = global.ReputationSystem.exportState(key);
    }
    if (global.GameClock && global.GameClock.snapshot) {
      snap.gameClock = global.GameClock.snapshot(key);
    }
    if (global.PlayerKnowledge && global.PlayerKnowledge.snapshotAll) {
      snap.knowledgeLayers = global.PlayerKnowledge.snapshotAll(key);
    }
    return snap;
  }

  function importSession(sessionKey, data) {
    var key = String(sessionKey || "default");
    var d = data || {};
    if (global.QuestGameState && global.QuestGameState.importState && d.gameState) {
      global.QuestGameState.importState(key, d.gameState);
    }
    if (global.ReputationSystem && global.ReputationSystem.importState && d.reputation) {
      global.ReputationSystem.importState(key, d.reputation);
    }
    if (global.GameClock && global.GameClock.setTick && d.gameClock && d.gameClock.tick != null) {
      global.GameClock.setTick(key, d.gameClock.tick, {
        day: d.gameClock.day,
        hour: d.gameClock.hour,
        reason: "session_import",
      });
    }
    return exportSession(key);
  }

  function clearSession(sessionKey) {
    var key = String(sessionKey || "default");
    if (global.QuestGameState && global.QuestGameState.clearState) {
      global.QuestGameState.clearState(key);
    }
    if (global.ReputationSystem && global.ReputationSystem.clearState) {
      global.ReputationSystem.clearState(key);
    }
    if (global.GameClock && global.GameClock.clearClock) {
      global.GameClock.clearClock(key);
    }
    if (global.PlayerKnowledge && global.PlayerKnowledge.clearAll) {
      global.PlayerKnowledge.clearAll(key);
    }
    if (global.QuestRuntime && global.QuestRuntime.clearQuestFlow) {
      global.QuestRuntime.clearQuestFlow(key);
    }
  }

  global.SessionSnapshot = {
    exportSession: exportSession,
    importSession: importSession,
    clearSession: clearSession,
  };
})(typeof window !== "undefined" ? window : globalThis);
