(function () {
  function loadJsonSync(url) {
    try {
      var xhr = new XMLHttpRequest();
      xhr.open("GET", url, false);
      xhr.send(null);
      if (xhr.status === 200 || xhr.status === 0) return JSON.parse(xhr.responseText);
    } catch (e) {}
    return null;
  }

  var npcData = loadJsonSync("./npcs.json");
  var playerData = loadJsonSync("./player-profile.json");
  var repConfig = loadJsonSync("./reputation-config.json");

  if (window.QuestSystem && window.QuestSystem.registerProfiles) {
    window.QuestSystem.registerProfiles({
      npcs: npcData && npcData.npcs,
      player: playerData && playerData.player,
    });
  }

  if (window.ReputationSystem && repConfig) {
    window.ReputationSystem.setConfig(repConfig);
  }

  if (typeof window.__appendInlineDebug === "function") {
    var npcCount = npcData && npcData.npcs ? Object.keys(npcData.npcs).length : 0;
    window.__appendInlineDebug(
      "data-loader: npcs=" +
        (npcData ? npcCount + " profiles" : "fallback") +
        ", player=" +
        (playerData ? "ok" : "fallback") +
        ", repConfig=" +
        (repConfig ? "ok" : "fallback")
    );
  }
})();
