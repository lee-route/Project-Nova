/**
 * Player ↔ NPC reputation + village reputation.
 * NPC subject reputation (주제별) remains on NPC class in quest-system.js.
 */
(function (global) {
  var CONFIG = null;
  var SESSION_STORE = new Map();

  function clamp(v, min, max) {
    return Math.min(max, Math.max(min, v));
  }

  function loadConfig() {
    if (CONFIG) return CONFIG;
    try {
      var xhr = new XMLHttpRequest();
      xhr.open("GET", "./reputation-config.json", false);
      xhr.send(null);
      if (xhr.status === 200 || xhr.status === 0) {
        CONFIG = JSON.parse(xhr.responseText);
        return CONFIG;
      }
    } catch (e) {}
    CONFIG = {
      version: 1,
      village: { min: 0, max: 100, default: 50 },
      npcKeys: {
        mayor: { label: "촌장", default: 0.5 },
        merchant: { label: "상인", default: 0.5 },
        guard: { label: "경비", default: 0.5 },
        scout: { label: "정찰병", default: 0.55 },
      },
      tiers: {
        hostile: { max: 0.24, label: "적대" },
        wary: { max: 0.39, label: "경계" },
        neutral: { max: 0.59, label: "보통" },
        friendly: { max: 0.79, label: "우호" },
        trusted: { max: 1, label: "신뢰" },
      },
      trustFromReputation: { trustScale: 0.35, minTrust: 0.08, maxTrust: 0.98 },
    };
    return CONFIG;
  }

  function setConfig(cfg) {
    CONFIG = cfg;
  }

  function defaultNpcRep() {
    var cfg = loadConfig();
    var out = {};
    Object.keys(cfg.npcKeys || {}).forEach(function (key) {
      out[key] = Number(cfg.npcKeys[key].default ?? 0.5);
    });
    return out;
  }

  function createState(initial) {
    var cfg = loadConfig();
    var base = {
      version: 1,
      villageReputation: Number(cfg.village?.default ?? 50),
      npcReputation: defaultNpcRep(),
      history: [],
    };
    if (initial && typeof initial === "object") {
      if (typeof initial.villageReputation === "number") {
        base.villageReputation = clamp(
          initial.villageReputation,
          cfg.village?.min ?? 0,
          cfg.village?.max ?? 100
        );
      }
      if (initial.npcReputation && typeof initial.npcReputation === "object") {
        Object.keys(initial.npcReputation).forEach(function (key) {
          base.npcReputation[key] = clamp(Number(initial.npcReputation[key]), 0, 1);
        });
      }
    }
    return base;
  }

  var TIER_ORDER = ["hostile", "wary", "neutral", "friendly", "trusted"];

  function getTier(score) {
    var cfg = loadConfig();
    var tiers = cfg.tiers || {};
    var keys = Object.keys(tiers);
    for (var i = 0; i < keys.length; i += 1) {
      var t = tiers[keys[i]];
      if (score <= Number(t.max)) {
        return { id: keys[i], label: t.label, score: score };
      }
    }
    return { id: "neutral", label: "보통", score: score };
  }

  function tierRank(tierId) {
    var idx = TIER_ORDER.indexOf(String(tierId || ""));
    return idx < 0 ? 2 : idx;
  }

  function meetsMinTier(currentTierId, minTierId) {
    return tierRank(currentTierId) >= tierRank(minTierId);
  }

  function resolveTrustFromReputation(profileKey, baseTrust, sessionKey) {
    var cfg = loadConfig();
    var tf = cfg.trustFromReputation || {};
    var state = getState(sessionKey || "default");
    var rep = state.npcReputation[profileKey];
    if (rep === undefined) rep = cfg.npcKeys?.[profileKey]?.default ?? 0.5;
    var base = typeof baseTrust === "number" ? baseTrust : 0.7;
    var trust =
      base + (Number(rep) - 0.5) * Number(tf.trustScale ?? 0.35);
    return clamp(trust, Number(tf.minTrust ?? 0.08), Number(tf.maxTrust ?? 0.98));
  }

  function getState(sessionKey) {
    var key = String(sessionKey || "default");
    if (!SESSION_STORE.has(key)) {
      var fromPlayer = null;
      if (global.QuestSystem && global.QuestSystem.getProfileRegistry) {
        var p = global.QuestSystem.getProfileRegistry().player;
        if (p && p.initialNpcReputation) fromPlayer = { npcReputation: p.initialNpcReputation };
        if (p && typeof p.initialVillageReputation === "number") {
          fromPlayer = fromPlayer || {};
          fromPlayer.villageReputation = p.initialVillageReputation;
        }
      }
      SESSION_STORE.set(key, createState(fromPlayer));
    }
    return SESSION_STORE.get(key);
  }

  function clearState(sessionKey) {
    SESSION_STORE.delete(String(sessionKey || "default"));
  }

  function applyNpcDelta(state, profileKey, delta, reason) {
    if (!profileKey || delta === 0) return null;
    var before = state.npcReputation[profileKey];
    if (before === undefined) before = loadConfig().npcKeys?.[profileKey]?.default ?? 0.5;
    var after = clamp(Number(before) + Number(delta), 0, 1);
    state.npcReputation[profileKey] = after;
    var entry = {
      type: "npc",
      key: profileKey,
      delta: Number(delta),
      before: Number(before.toFixed(3)),
      after: Number(after.toFixed(3)),
      tier: getTier(after),
      reason: reason || "",
    };
    state.history.push(entry);
    return entry;
  }

  function applyVillageDelta(state, delta, reason) {
    var cfg = loadConfig();
    var before = state.villageReputation;
    var after = clamp(
      Number(before) + Number(delta),
      cfg.village?.min ?? 0,
      cfg.village?.max ?? 100
    );
    state.villageReputation = after;
    var entry = {
      type: "village",
      delta: Number(delta),
      before: before,
      after: after,
      reason: reason || "",
    };
    state.history.push(entry);
    return entry;
  }

  /**
   * @param {object} effects — quests-draft experience.softEffects or outcome
   *   playerRepDelta: { mayor: 0.12, ... }
   *   villageRepDelta: number (optional)
   *   reputation: number (alias for village, from quest_result)
   */
  function applyQuestEffects(sessionKey, effects, meta) {
    var state = getState(sessionKey);
    var changes = [];
    var reason = (meta && meta.questId) || "quest";

    var npcDelta = effects?.playerRepDelta || effects?.npcRepDelta;
    if (npcDelta && typeof npcDelta === "object") {
      Object.keys(npcDelta).forEach(function (key) {
        var ch = applyNpcDelta(state, key, npcDelta[key], reason + ":" + (meta?.giverId || ""));
        if (ch) changes.push(ch);
      });
    }

    var vDelta = effects?.villageRepDelta;
    if (vDelta === undefined && typeof effects?.reputation === "number") {
      vDelta = effects.reputation;
    }
    if (typeof vDelta === "number" && vDelta !== 0) {
      changes.push(applyVillageDelta(state, vDelta, reason));
    }

    return { state: snapshot(state), changes: changes };
  }

  function snapshot(state) {
    return {
      villageReputation: state.villageReputation,
      npcReputation: Object.assign({}, state.npcReputation),
      tiers: Object.keys(state.npcReputation).reduce(function (acc, key) {
        acc[key] = getTier(state.npcReputation[key]);
        return acc;
      }, {}),
      historyLength: state.history.length,
      recentChanges: state.history.slice(-8),
    };
  }

  function exportState(sessionKey) {
    return JSON.parse(JSON.stringify(getState(sessionKey)));
  }

  function importState(sessionKey, data) {
    SESSION_STORE.set(String(sessionKey || "default"), createState(data));
    return getState(sessionKey);
  }

  global.ReputationSystem = {
    loadConfig: loadConfig,
    setConfig: setConfig,
    createState: createState,
    getState: getState,
    clearState: clearState,
    getTier: getTier,
    tierRank: tierRank,
    meetsMinTier: meetsMinTier,
    TIER_ORDER: TIER_ORDER,
    resolveTrustFromReputation: resolveTrustFromReputation,
    applyNpcDelta: function (sessionKey, profileKey, delta, reason) {
      var state = getState(sessionKey);
      var ch = applyNpcDelta(state, profileKey, delta, reason);
      return { change: ch, state: snapshot(state) };
    },
    applyQuestEffects: applyQuestEffects,
    snapshot: function (sessionKey) {
      return snapshot(getState(sessionKey));
    },
    exportState: exportState,
    importState: importState,
  };
})(typeof window !== "undefined" ? window : globalThis);
