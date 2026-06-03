/**
 * Quest draft loader, factMatch evaluation, turn-in flow.
 * Depends on window.QuestSystem and (browser) parser helpers on window.NpcParser.
 */
(function (global) {
  var catalog = null;

  function loadJsonSync(url) {
    try {
      var xhr = new XMLHttpRequest();
      xhr.open("GET", url, false);
      xhr.send(null);
      if (xhr.status === 200 || xhr.status === 0) return JSON.parse(xhr.responseText);
    } catch (e) {}
    return null;
  }

  function loadQuestCatalog(url) {
    var path = url || "./quests-draft.json";
    var data = loadJsonSync(path);
    if (data && data.quests) {
      catalog = data;
    }
    return catalog;
  }

  function setQuestCatalog(data) {
    catalog = data;
    return catalog;
  }

  function getQuestCatalog() {
    return catalog;
  }

  function getQuest(questId) {
    if (!catalog || !catalog.quests) return null;
    for (var i = 0; i < catalog.quests.length; i += 1) {
      if (catalog.quests[i].id === questId) return catalog.quests[i];
    }
    return null;
  }

  function getQuestGiver(quest, giverId) {
    var list = quest.questGivers || [];
    for (var i = 0; i < list.length; i += 1) {
      if (list[i].giverId === giverId) return list[i];
    }
    return null;
  }

  function turnInProfileKey(giver) {
    if (giver.turnInTarget && giver.turnInTarget.npcProfileRef) {
      return giver.turnInTarget.npcProfileRef;
    }
    return giver.npcProfileRef;
  }

  function factSnapshotFromInterpreted(item) {
    var tv = item.truth_value || item;
    var meta = item.metadata || {};
    return {
      subject: String(tv.subject || ""),
      action: String(tv.action || ""),
      target: String(tv.target || ""),
      object: String(tv.object || ""),
      quantity: Number(tv.quantity ?? 1),
      certainty: Number(tv.certainty ?? 0),
      action_type: String(tv.action_type || ""),
      applied_rules: meta.applied_rules || item.applied_rules || [],
    };
  }

  function evaluateFactMatch(fact, spec) {
    if (!spec) return { ok: false, reasons: ["no spec"] };
    var reasons = [];
    var ok = true;

    if (spec.subjectContains) {
      if (fact.subject.indexOf(spec.subjectContains) < 0) {
        ok = false;
        reasons.push("subject missing " + spec.subjectContains);
      }
    }
    if (spec.targetContains) {
      if (fact.target.indexOf(spec.targetContains) < 0) {
        ok = false;
        reasons.push("target missing " + spec.targetContains);
      }
    }
    if (spec.objectContains) {
      if (fact.object.indexOf(spec.objectContains) < 0) {
        ok = false;
        reasons.push("object missing " + spec.objectContains);
      }
    }
    if (spec.actionType) {
      if (fact.action_type !== spec.actionType) {
        ok = false;
        reasons.push("action_type " + fact.action_type + " != " + spec.actionType);
      }
    }
    if (typeof spec.minQuantity === "number" && fact.quantity < spec.minQuantity) {
      ok = false;
      reasons.push("quantity " + fact.quantity + " < " + spec.minQuantity);
    }
    if (typeof spec.minCertainty === "number" && fact.certainty < spec.minCertainty) {
      ok = false;
      reasons.push("certainty " + fact.certainty + " < " + spec.minCertainty);
    }

    return { ok: ok, reasons: reasons };
  }

  function evaluateQuestCompletion(interpretedFacts, quest) {
    var spec = quest.finalObjective && quest.finalObjective.factMatch;
    if (!spec) {
      return { completed: false, matches: [], reason: "no factMatch on quest" };
    }
    var matches = [];
    var anyOk = false;
    for (var i = 0; i < interpretedFacts.length; i += 1) {
      var snap = factSnapshotFromInterpreted(interpretedFacts[i]);
      var ev = evaluateFactMatch(snap, spec);
      matches.push({ factIndex: i, snapshot: snap, evaluation: ev });
      if (ev.ok) anyOk = true;
    }
    return {
      completed: anyOk,
      matches: matches,
      reason: anyOk ? "" : matches.length ? matches[0].evaluation.reasons.join("; ") : "no facts",
    };
  }

  function runQuestTurnIn(options) {
    var engine = options.engine || global.QuestSystem;
    var parser = options.parser;
    if (!engine || !engine.executeScenario) {
      throw new Error("QuestSystem engine required");
    }

    var quest = options.quest || getQuest(options.questId);
    if (!quest) throw new Error("Quest not found: " + options.questId);

    var giver = options.giver || getQuestGiver(quest, options.giverId);
    if (!giver) throw new Error("Quest giver not found: " + options.giverId);

    var facts = options.facts;
    if (!facts || !facts.length) {
      if (!parser || !parser.parseScenarioText || !parser.buildFactsFromParsed) {
        throw new Error("facts or parser required");
      }
      var parsed = parser.parseScenarioText(options.scenarioText || "");
      facts = parser.buildFactsFromParsed(parsed);
    }

    var recvKey = turnInProfileKey(giver);
    var exp = giver.experience || {};
    var propOpts = exp.propagationOptions || {};
    var quantityMode = options.quantityMode || propOpts.quantityMode || "faithful";
    var allowPartialTrust =
      options.allowPartialTrust !== undefined
        ? options.allowPartialTrust
        : propOpts.allowPartialTrust !== false;

    var result = engine.executeScenario({
      facts: facts,
      usePlayerAsSender: true,
      receiverProfileKey: recvKey,
      quantityMode: quantityMode,
      allowPartialTrust: allowPartialTrust,
      persistSession: Boolean(options.persistSession),
      sessionKey: options.sessionKey || "quest-" + quest.id + "-" + giver.giverId,
      currentTick: options.currentTick || Date.now() % 100000,
      trustLevel: options.trustLevel,
    });

    var interpreted = result.propagation.interpretedFacts || [];
    var completion = evaluateQuestCompletion(interpreted, quest);

    var reputationResult = null;
    if (completion.completed && global.ReputationSystem) {
      var sessionKey = options.reputationSessionKey || options.sessionKey || "default";
      var soft = exp.softEffects || {};
      var effects = {
        playerRepDelta: soft.playerRepDelta,
        villageRepDelta: soft.villageRepDelta,
        reputation: quest.outcome && quest.outcome.reputation,
      };
      reputationResult = global.ReputationSystem.applyQuestEffects(sessionKey, effects, {
        questId: quest.id,
        giverId: giver.giverId,
      });
    }

    return {
      questId: quest.id,
      giverId: giver.giverId,
      turnInProfileKey: recvKey,
      engineResult: result,
      completion: completion,
      outcome: completion.completed ? quest.outcome : null,
      experience: exp,
      processSteps: giver.processSteps || [],
      reputationResult: reputationResult,
    };
  }

  function listQuestGiverOptions(questId) {
    var quest = getQuest(questId);
    if (!quest) return [];
    return (quest.questGivers || []).map(function (g) {
      return {
        giverId: g.giverId,
        label: g.label,
        npcProfileRef: g.npcProfileRef,
        turnInProfileKey: turnInProfileKey(g),
        uiHint: g.uiHint || "",
      };
    });
  }

  /**
   * Pick quest accept line from player rep tier toward this giver.
   * giver.acceptDialogueByTier[tierId] → default → acceptDialogue
   */
  function resolveAcceptDialogue(giver, options) {
    var opts = options || {};
    var profileKey = giver.npcProfileRef || giver.giverId;
    var sessionKey = opts.reputationSessionKey || opts.sessionKey || "default";
    var Rep = global.ReputationSystem;
    var repScore = 0.5;
    var tier = { id: "neutral", label: "보통" };
    if (Rep) {
      var state = Rep.getState(sessionKey);
      repScore = state.npcReputation[profileKey];
      if (repScore === undefined) {
        repScore = Rep.loadConfig().npcKeys?.[profileKey]?.default ?? 0.5;
      }
      tier = Rep.getTier(repScore);
    }

    var byTier = giver.acceptDialogueByTier;
    var line = "";
    var source = "acceptDialogue";
    if (byTier && typeof byTier === "object") {
      if (byTier[tier.id]) {
        line = byTier[tier.id];
        source = "acceptDialogueByTier:" + tier.id;
      } else if (byTier.default) {
        line = byTier.default;
        source = "acceptDialogueByTier:default";
      }
    }
    if (!line) {
      line = giver.acceptDialogue || "";
      source = "acceptDialogue";
    }

    var minTier = giver.minTierToAccept || null;
    var canAccept = true;
    var blockReason = "";
    if (minTier && Rep) {
      canAccept = Rep.meetsMinTier(tier.id, minTier);
      if (!canAccept) {
        blockReason =
          "평판 " + tier.label + "(" + tier.id + ") < 필요 " + minTier;
        if (byTier && byTier.refused) {
          line = byTier.refused;
          source = "acceptDialogueByTier:refused";
        } else if (giver.refusedDialogue) {
          line = giver.refusedDialogue;
          source = "refusedDialogue";
        }
      }
    }

    return {
      line: line,
      source: source,
      tier: tier,
      repScore: Number(Number(repScore).toFixed(3)),
      npcProfileRef: profileKey,
      canAccept: canAccept,
      blockReason: blockReason,
      minTierToAccept: minTier,
    };
  }

  function getAcceptDialogue(questId, giverId, options) {
    var quest = getQuest(questId);
    if (!quest) throw new Error("Quest not found: " + questId);
    var giver = getQuestGiver(quest, giverId);
    if (!giver) throw new Error("Quest giver not found: " + giverId);
    return resolveAcceptDialogue(giver, options);
  }

  global.QuestRuntime = {
    loadQuestCatalog: loadQuestCatalog,
    setQuestCatalog: setQuestCatalog,
    getQuestCatalog: getQuestCatalog,
    getQuest: getQuest,
    getQuestGiver: getQuestGiver,
    evaluateFactMatch: evaluateFactMatch,
    evaluateQuestCompletion: evaluateQuestCompletion,
    runQuestTurnIn: runQuestTurnIn,
    listQuestGiverOptions: listQuestGiverOptions,
    resolveAcceptDialogue: resolveAcceptDialogue,
    getAcceptDialogue: getAcceptDialogue,
    turnInProfileKey: turnInProfileKey,
  };
})(typeof window !== "undefined" ? window : globalThis);
