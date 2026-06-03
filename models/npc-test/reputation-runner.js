(function () {
  var panel = document.getElementById("reputation-panel");
  var outRep = document.getElementById("out-reputation");
  var resetBtn = document.getElementById("rep-reset");
  var persistCheck = document.getElementById("rep-persist-session");

  function formatJSON(v) {
    return JSON.stringify(v, null, 2);
  }

  function sessionKey() {
    return persistCheck && persistCheck.checked ? "ui-reputation" : "default";
  }

  function renderReputation() {
    if (!window.ReputationSystem || !outRep) return;
    var snap = window.ReputationSystem.snapshot(sessionKey());
    outRep.textContent = formatJSON(snap);
    if (panel) {
      var lines = [];
      Object.keys(snap.npcReputation || {}).forEach(function (key) {
        var tier = snap.tiers[key];
        lines.push(key + ": " + snap.npcReputation[key] + " (" + (tier && tier.label) + ")");
      });
      lines.push("village: " + snap.villageReputation);
      var summary = document.getElementById("rep-summary");
      if (summary) summary.textContent = lines.join(" · ");
    }
  }

  window.refreshReputationPanel = function () {
    renderReputation();
    if (typeof window.refreshAcceptDialoguePreview === "function") {
      window.refreshAcceptDialoguePreview();
    }
  };

  if (resetBtn) {
    resetBtn.addEventListener("click", function () {
      if (window.ReputationSystem) {
        window.ReputationSystem.clearState(sessionKey());
        window.ReputationSystem.getState(sessionKey());
      }
      renderReputation();
    });
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", renderReputation);
  } else {
    renderReputation();
  }
})();
