/**
 * WorldTruth (객관·시뮬레이터) + PlayerKnowledge (플레이어가 접한 interpreted 정보)
 */
(function (global) {
  var WORLD_STORE = new Map();
  var PLAYER_STORE = new Map();

  function factSnapshotFromAtom(item) {
    var tv = item.truth_value || item;
    var meta = item.metadata || {};
    return {
      subject: String(tv.subject || ""),
      action: String(tv.action || ""),
      target: String(tv.target || ""),
      object: String(tv.object || ""),
      quantity: Number(tv.quantity ?? 1),
      certainty: Number(tv.certainty ?? 0),
      is_countable: Boolean(tv.is_countable),
      action_type: String(tv.action_type || ""),
      source_chain: (meta.source_chain || []).slice(),
      hop_depth: Number(meta.hop_depth ?? 0),
      rumor: Boolean(meta.rumor),
      applied_rules: (meta.applied_rules || []).slice(),
    };
  }

  // --- WorldTruth ---
  function getWorld(sessionKey) {
    var key = String(sessionKey || "default");
    if (!WORLD_STORE.has(key)) {
      WORLD_STORE.set(key, { facts: [], events: [], updatedTick: 0 });
    }
    return WORLD_STORE.get(key);
  }

  function clearWorld(sessionKey) {
    WORLD_STORE.delete(String(sessionKey || "default"));
  }

  function setWorldFacts(sessionKey, facts, tick, source) {
    var w = getWorld(sessionKey);
    var t = Number(tick ?? 0);
    var list = Array.isArray(facts) ? facts : [];
    w.facts = list.map(function (f, i) {
      return {
        id: "WT_" + (i + 1),
        snapshot: typeof f.subject === "string" ? f : factSnapshotFromAtom({ truth_value: f, metadata: {} }),
        recordedTick: t,
        source: source || "gm_seed",
      };
    });
    w.updatedTick = t;
    return JSON.parse(JSON.stringify(w));
  }

  function seedFromParserFacts(sessionKey, parserFacts, tick) {
    return setWorldFacts(sessionKey, parserFacts, tick, "parser_seed");
  }

  function getWorldFacts(sessionKey) {
    return JSON.parse(JSON.stringify(getWorld(sessionKey).facts));
  }

  // --- PlayerKnowledge ---
  function getPlayer(sessionKey) {
    var key = String(sessionKey || "default");
    if (!PLAYER_STORE.has(key)) {
      PLAYER_STORE.set(key, { entries: [], updatedTick: 0 });
    }
    return PLAYER_STORE.get(key);
  }

  function clearPlayer(sessionKey) {
    PLAYER_STORE.delete(String(sessionKey || "default"));
  }

  /**
   * @param acquisition "direct" | "heard" | "overheard"
   */
  function recordEntry(sessionKey, payload) {
    var p = getPlayer(sessionKey);
    var tick = Number(payload.tick ?? 0);
    var interpreted = payload.interpretedFacts || [];
    var entry = {
      id: "PK_" + String(p.entries.length + 1).padStart(4, "0"),
      tick: tick,
      acquisition: payload.acquisition || "heard",
      fromNpc: payload.fromNpc || null,
      toNpc: payload.toNpc || null,
      source_chain: (payload.source_chain || []).slice(),
      hop_depth: Number(payload.hop_depth ?? 0),
      facts: interpreted.map(function (item) {
        return factSnapshotFromAtom(item);
      }),
      questId: payload.questId || null,
      note: payload.note || "",
    };
    p.entries.push(entry);
    p.updatedTick = tick;
    return entry;
  }

  function recordFromPropagation(sessionKey, engineResult, options) {
    var opts = options || {};
    if (!engineResult || !engineResult.propagation || engineResult.propagation.blocked) {
      return { recorded: false, reason: "blocked" };
    }
    var interpreted = engineResult.propagation.interpretedFacts || [];
    if (!interpreted.length) return { recorded: false, reason: "no facts" };

    var acquisition = "heard";
    if (opts.usePlayerAsSender) acquisition = "direct";

    var chain = [];
    if (interpreted[0] && interpreted[0].metadata && interpreted[0].metadata.source_chain) {
      chain = interpreted[0].metadata.source_chain.slice();
    }
    var hop = interpreted[0] && interpreted[0].metadata ? Number(interpreted[0].metadata.hop_depth || 0) : 0;

    var entry = recordEntry(sessionKey, {
      tick: opts.tick != null ? opts.tick : (global.GameClock && global.GameClock.getTick(sessionKey)) || 0,
      acquisition: acquisition,
      fromNpc: engineResult.sender && engineResult.sender.name,
      toNpc: engineResult.receiver && engineResult.receiver.name,
      source_chain: chain,
      hop_depth: hop,
      interpretedFacts: interpreted,
      questId: opts.questId,
      note: opts.note || "",
    });

    return { recorded: true, entry: entry };
  }

  function getPlayerEntries(sessionKey) {
    return JSON.parse(JSON.stringify(getPlayer(sessionKey).entries));
  }

  function getPlayerView(sessionKey) {
    var entries = getPlayerEntries(sessionKey);
    var merged = [];
    entries.forEach(function (e) {
      e.facts.forEach(function (f) {
        merged.push({
          entryId: e.id,
          acquisition: e.acquisition,
          tick: e.tick,
          hop_depth: f.hop_depth,
          source_chain: f.source_chain,
          fact: f,
        });
      });
    });
    return { entries: entries, flatFacts: merged };
  }

  /**
   * 플레이어가 알고 있는 것 vs WorldTruth (있을 때만)
   */
  function comparePlayerToWorld(sessionKey) {
    var view = getPlayerView(sessionKey);
    var world = getWorldFacts(sessionKey);
    if (!world.length) {
      return { hasWorldTruth: false, matches: [], score: null };
    }

    function subjectSim(a, b) {
      return a.includes(b) || b.includes(a) ? 1 : 0;
    }

    var matches = [];
    var sum = 0;
    view.flatFacts.forEach(function (pf) {
      var f = pf.fact;
      var best = 0;
      var bestWt = null;
      for (var i = 0; i < world.length; i += 1) {
        var wt = world[i].snapshot;
        var sc =
          0.5 * subjectSim(f.subject, wt.subject) +
          0.3 * (f.action_type === wt.action_type ? 1 : 0.5) +
          0.2 * (Math.abs(f.quantity - wt.quantity) <= 2 ? 1 : 0);
        if (sc > best) {
          best = sc;
          bestWt = wt;
        }
      }
      sum += best;
      matches.push({
        playerFact: f,
        acquisition: pf.acquisition,
        bestWorld: bestWt,
        score: Number(best.toFixed(2)),
        aligned: best >= 0.72,
      });
    });

    var avg = view.flatFacts.length ? sum / view.flatFacts.length : 0;
    return {
      hasWorldTruth: true,
      matches: matches,
      score: Number(avg.toFixed(2)),
      playerEntryCount: view.entries.length,
      worldFactCount: world.length,
    };
  }

  function snapshotAll(sessionKey) {
    return {
      world: JSON.parse(JSON.stringify(getWorld(sessionKey))),
      player: JSON.parse(JSON.stringify(getPlayer(sessionKey))),
      compare: comparePlayerToWorld(sessionKey),
    };
  }

  function clearAll(sessionKey) {
    clearWorld(sessionKey);
    clearPlayer(sessionKey);
  }

  global.WorldTruth = {
    getWorld: getWorld,
    clearWorld: clearWorld,
    setWorldFacts: setWorldFacts,
    seedFromParserFacts: seedFromParserFacts,
    getWorldFacts: getWorldFacts,
  };

  global.PlayerKnowledge = {
    getPlayer: getPlayer,
    clearPlayer: clearPlayer,
    recordEntry: recordEntry,
    recordFromPropagation: recordFromPropagation,
    getPlayerEntries: getPlayerEntries,
    getPlayerView: getPlayerView,
    comparePlayerToWorld: comparePlayerToWorld,
    snapshotAll: snapshotAll,
    clearAll: clearAll,
  };
})(typeof window !== "undefined" ? window : globalThis);
