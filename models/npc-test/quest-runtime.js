/**
 * Quest draft loader, factMatch evaluation, turn-in flow.
 * Depends on window.QuestSystem and (browser) parser helpers on window.NpcParser.
 */
(function (global) {
  var catalog = null;
  var QUEST_FLOW_STORE = new Map();

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
    var objective = quest.finalObjective || {};
    var specs = [];
    if (objective.factMatch) specs.push(objective.factMatch);
    if (objective.factMatchAlt) specs.push(objective.factMatchAlt);
    if (!specs.length) {
      return { completed: false, matches: [], reason: "no factMatch on quest" };
    }
    var matches = [];
    var anyOk = false;
    for (var i = 0; i < interpretedFacts.length; i += 1) {
      var snap = factSnapshotFromInterpreted(interpretedFacts[i]);
      for (var s = 0; s < specs.length; s += 1) {
        var ev = evaluateFactMatch(snap, specs[s]);
        matches.push({ factIndex: i, specIndex: s, snapshot: snap, evaluation: ev });
        if (ev.ok) anyOk = true;
      }
    }
    return {
      completed: anyOk,
      matches: matches,
      reason: anyOk ? "" : matches.length ? matches[0].evaluation.reasons.join("; ") : "no facts",
    };
  }

  function aggregateInterpretedMetrics(interpretedFacts) {
    var quantity = 0;
    var certainty = 0;
    for (var i = 0; i < interpretedFacts.length; i += 1) {
      var snap = factSnapshotFromInterpreted(interpretedFacts[i]);
      quantity = Math.max(quantity, snap.quantity);
      certainty = Math.max(certainty, snap.certainty);
    }
    return { quantity: quantity, certainty: certainty };
  }

  function matchesOutcomeCondition(metrics, condition) {
    if (!condition) return false;
    var c = condition;
    if (typeof c.interpreted_quantity_min === "number" && metrics.quantity < c.interpreted_quantity_min) {
      return false;
    }
    if (typeof c.interpreted_quantity_max === "number" && metrics.quantity > c.interpreted_quantity_max) {
      return false;
    }
    if (typeof c.interpreted_certainty_min === "number" && metrics.certainty < c.interpreted_certainty_min) {
      return false;
    }
    if (typeof c.interpreted_certainty_max === "number" && metrics.certainty > c.interpreted_certainty_max) {
      return false;
    }
    return true;
  }

  function pickOutcomeBranch(interpretedFacts, quest) {
    var branches = quest.outcomes;
    if (!branches || !branches.length) {
      return quest.outcome
        ? { branchId: "legacy_outcome", effects: quest.outcome, metrics: aggregateInterpretedMetrics(interpretedFacts) }
        : null;
    }
    var metrics = aggregateInterpretedMetrics(interpretedFacts);
    for (var i = 0; i < branches.length; i += 1) {
      var b = branches[i];
      if (matchesOutcomeCondition(metrics, b.condition)) {
        return {
          branchId: b.id || "outcome_" + i,
          effects: b.effects || {},
          metrics: metrics,
          condition: b.condition,
        };
      }
    }
    var fallback = quest.outcomeFallback;
    return {
      branchId: "fallback",
      effects: fallback || { questState: "completed", rewards: { gold: 0 } },
      metrics: metrics,
      condition: null,
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

    var sessionKey = options.reputationSessionKey || options.sessionKey || "quest-" + quest.id + "-" + giver.giverId;
    var recordPkb = options.recordPlayerKnowledge !== false;

    var result = engine.executeScenario({
      facts: facts,
      usePlayerAsSender: true,
      receiverProfileKey: recvKey,
      quantityMode: quantityMode,
      allowPartialTrust: allowPartialTrust,
      persistSession: Boolean(options.persistSession),
      sessionKey: sessionKey,
      currentTick:
        options.currentTick != null
          ? options.currentTick
          : global.GameClock
            ? global.GameClock.resolveTick({ sessionKey: sessionKey })
            : 1000,
      recordPlayerKnowledge: false,
      seedWorldTruthFromFacts: Boolean(options.seedWorldTruthFromFacts),
      advanceTicksAfterPropagate: options.advanceTicksAfterPropagate || 1,
      trustLevel: options.trustLevel,
      questId: quest.id,
    });

    var interpreted = result.propagation.interpretedFacts || [];
    var completion = evaluateQuestCompletion(interpreted, quest);
    var outcomeBranch = completion.completed ? pickOutcomeBranch(interpreted, quest) : null;

    var reputationResult = null;
    if (completion.completed && global.ReputationSystem && outcomeBranch) {
      var branchEffects = outcomeBranch.effects || {};
      reputationResult = global.ReputationSystem.applyQuestEffects(sessionKey, branchEffects, {
        questId: quest.id,
        giverId: giver.giverId,
        npcProfileRef: giver.npcProfileRef,
      });
    }

    var knowledgeAudit = null;
    if (global.DeceptionAudit && options.includeKnowledgeAudit !== false) {
      knowledgeAudit = global.DeceptionAudit.auditTurnInReport(sessionKey, facts, {
        worldTruthFacts: options.worldTruthFacts || options.investigationFacts,
        tick: result.gameClockSnapshot && result.gameClockSnapshot.tick,
      });
    }

    if (recordPkb && global.PlayerKnowledge && !result.propagation.blocked) {
      global.PlayerKnowledge.recordFromPropagation(
        sessionKey,
        { propagation: result.propagation, sender: result.sender, receiver: result.receiver },
        { tick: result.gameClockSnapshot && result.gameClockSnapshot.tick, usePlayerAsSender: true, questId: quest.id }
      );
    }

    var gameStateApply = null;
    if (global.QuestGameState && options.applyGameState !== false && completion.completed) {
      gameStateApply = global.QuestGameState.applyTurnInOutcome(sessionKey, {
        questId: quest.id,
        giverId: giver.giverId,
        completion: completion,
        outcome: outcomeBranch ? outcomeBranch.effects : null,
        outcomeBranch: outcomeBranch,
        reputationResult: reputationResult,
      });
    }

    return {
      questId: quest.id,
      giverId: giver.giverId,
      turnInProfileKey: recvKey,
      facts: facts,
      engineResult: result,
      propagation: {
        blocked: result.propagation.blocked,
        reason: result.propagation.reason,
        interpretedFacts: result.propagation.interpretedFacts || [],
        partialTrust: result.propagation.partialTrust,
        quantityMode: result.propagation.quantityMode,
      },
      completion: completion,
      outcome: outcomeBranch ? outcomeBranch.effects : null,
      outcomeBranch: outcomeBranch,
      experience: exp,
      processSteps: giver.processSteps || [],
      reputationResult: reputationResult,
      knowledgeAudit: knowledgeAudit,
      gameStateApply: gameStateApply,
      sessionKey: sessionKey,
      v1OutcomeSource: "interpretedFacts_only",
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
  function resolveAcceptDialogue(giver, quest, options) {
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

    var sharedTier = quest && quest.sharedAcceptDialogueByTier;
    var giverTier = giver.acceptDialogueByTier;
    var line = "";
    var source = "acceptDialogue";
    if (giverTier && typeof giverTier === "object" && giverTier[tier.id]) {
      line = giverTier[tier.id];
      source = "giver.acceptDialogueByTier:" + tier.id;
    } else if (tier.id === "neutral" && giver.acceptDialogue) {
      line = giver.acceptDialogue;
      source = "giver.acceptDialogue";
    } else if (sharedTier && typeof sharedTier === "object" && sharedTier[tier.id]) {
      line = sharedTier[tier.id];
      source = "sharedAcceptDialogueByTier:" + tier.id;
    } else if (sharedTier && sharedTier.default) {
      line = sharedTier.default;
      source = "sharedAcceptDialogueByTier:default";
    } else if (giver.acceptDialogue) {
      line = giver.acceptDialogue;
      source = "giver.acceptDialogue";
    }

    var minTier = giver.minTierToAccept || (quest && quest.minTierToAccept) || null;
    var canAccept = true;
    var blockReason = "";
    if (minTier && Rep) {
      canAccept = Rep.meetsMinTier(tier.id, minTier);
      if (!canAccept) {
        blockReason =
          "평판 " + tier.label + "(" + tier.id + ") < 필요 " + minTier;
        if (sharedTier && sharedTier.refused) {
          line = sharedTier.refused;
          source = "sharedAcceptDialogueByTier:refused";
        } else if (giverTier && giverTier.refused) {
          line = giverTier.refused;
          source = "giver.acceptDialogueByTier:refused";
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
    return resolveAcceptDialogue(giver, quest, options);
  }

  function createQuestInstance(questId, giverId) {
    var quest = getQuest(questId);
    if (!quest) throw new Error("Quest not found: " + questId);
    var giver = getQuestGiver(quest, giverId);
    if (!giver) throw new Error("Quest giver not found: " + giverId);
    var template = (catalog && catalog.questInstanceTemplate) || {};
    return {
      questId: questId,
      state: "accepted",
      chosenQuestGiverId: giverId,
      currentStepId: null,
      completedStepIds: [],
      turnInNpcId: template.turnInNpcId || turnInProfileKey(giver),
    };
  }

  function getGiverBriefing(giver, quest) {
    var steps = (giver.processSteps || []).slice().sort(function (a, b) {
      return (a.order || 0) - (b.order || 0);
    });
    var firstDialogue = steps.find(function (s) {
      return s.type === "dialogue" && s.npcLine;
    });
    return {
      introDialogue: (quest && quest.introDialogue) || "",
      briefingLine: firstDialogue ? firstDialogue.npcLine : "",
      briefingLocation: firstDialogue ? firstDialogue.location : "",
      stepsTotal: steps.length,
    };
  }

  function acceptQuest(questId, giverId, options) {
    var opts = options || {};
    var sessionKey = opts.sessionKey || "default";
    var quest = getQuest(questId);
    var giver = getQuestGiver(quest, giverId);
    var accept = resolveAcceptDialogue(giver, quest, opts);
    if (!accept.canAccept) {
      return { ok: false, accept: accept, reason: accept.blockReason || "cannot accept" };
    }
    var instance = createQuestInstance(questId, giverId);
    instance.state = "active";
    instance.stepIndex = 0;
    var briefing = getGiverBriefing(giver, quest);
    QUEST_FLOW_STORE.set(sessionKey, {
      questId: questId,
      giverId: giverId,
      instance: instance,
      steps: (giver.processSteps || []).slice().sort(function (a, b) {
        return (a.order || 0) - (b.order || 0);
      }),
      briefing: briefing,
      log: [],
    });
    if (global.QuestGameState) {
      global.QuestGameState.setActiveQuest(sessionKey, instance, {
        questId: questId,
        giverId: giverId,
        introDialogue: briefing.introDialogue,
        acceptLine: accept.line,
      });
    }
    return {
      ok: true,
      accept: accept,
      instance: instance,
      briefing: briefing,
      introDialogue: briefing.introDialogue,
    };
  }

  function getQuestFlow(sessionKey) {
    return QUEST_FLOW_STORE.get(String(sessionKey || "default")) || null;
  }

  function advanceProcessStep(options) {
    var opts = options || {};
    var sessionKey = opts.sessionKey || "default";
    var flow = QUEST_FLOW_STORE.get(sessionKey);
    if (!flow) {
      return { ok: false, reason: "no active quest; call acceptQuest first" };
    }
    var idx = flow.instance.stepIndex || 0;
    if (idx >= flow.steps.length) {
      return { ok: false, reason: "all steps done", instance: flow.instance };
    }
    var step = flow.steps[idx];
    var row = {
      stepId: step.stepId,
      order: step.order,
      type: step.type,
      location: step.location || "",
      title: step.title || "",
      status: "completed",
      npcLine: step.npcLine || null,
      playerAction: step.playerAction || null,
    };
    var turnInResult = null;

    if (step.type === "dialogue") {
      row.summary = step.npcLine || step.title;
    } else if (step.type === "travel") {
      row.summary = "이동 완료: " + (step.location || "");
    } else if (step.type === "fact_input") {
      if (!opts.scenarioText || !opts.engine || !opts.parser) {
        row.status = "awaiting_report";
        row.summary = "플레이어 보고 문장 필요";
        flow.log.push(row);
        return { ok: true, awaitingReport: true, step: row, instance: flow.instance };
      }
      turnInResult = runQuestTurnIn({
        questId: flow.questId,
        giverId: flow.giverId,
        scenarioText: opts.scenarioText,
        engine: opts.engine,
        parser: opts.parser,
        sessionKey: opts.engineSessionKey || sessionKey,
        reputationSessionKey: opts.reputationSessionKey || sessionKey,
        persistSession: opts.persistSession,
      });
      row.turnIn = {
        completed: turnInResult.completion.completed,
        branchId: turnInResult.outcomeBranch && turnInResult.outcomeBranch.branchId,
        gold: turnInResult.outcome && turnInResult.outcome.rewards && turnInResult.outcome.rewards.gold,
      };
      row.status = turnInResult.completion.completed ? "completed" : "failed";
      row.summary = turnInResult.completion.completed ? "보고 완료" : "보고 실패";
      flow.turnInResult = turnInResult;
      if (turnInResult.completion.completed) {
        flow.instance.state = "completed";
        if (global.QuestGameState) {
          global.QuestGameState.applyTurnInOutcome(sessionKey, turnInResult);
        }
      }
    } else {
      row.summary = step.title || step.type;
    }

    flow.instance.completedStepIds.push(step.stepId);
    flow.instance.currentStepId = step.stepId;
    flow.instance.stepIndex = idx + 1;
    flow.log.push(row);

    var giver = getQuestGiver(getQuest(flow.questId), flow.giverId);
    var done = flow.instance.stepIndex >= flow.steps.length;
    return {
      ok: true,
      step: row,
      turnInResult: turnInResult,
      instance: flow.instance,
      done: done,
      completionDialogue:
        done && giver && giver.experience ? giver.experience.completionDialogue : "",
      gameState: global.QuestGameState ? global.QuestGameState.snapshot(sessionKey) : null,
    };
  }

  /**
   * Walks processSteps in order (dialogue/travel/fact_input).
   * fact_input + scenarioText runs runQuestTurnIn when engine/parser provided.
   */
  function runProcessSteps(options) {
    var quest = getQuest(options.questId);
    if (!quest) throw new Error("Quest not found: " + options.questId);
    var giver = getQuestGiver(quest, options.giverId);
    if (!giver) throw new Error("Quest giver not found: " + options.giverId);

    var instance = options.instance || createQuestInstance(options.questId, options.giverId);
    var steps = (giver.processSteps || []).slice().sort(function (a, b) {
      return (a.order || 0) - (b.order || 0);
    });
    var executed = [];
    var turnInResult = null;

    for (var i = 0; i < steps.length; i += 1) {
      var step = steps[i];
      var row = {
        stepId: step.stepId,
        order: step.order,
        type: step.type,
        location: step.location || "",
        title: step.title || "",
        status: "completed",
        npcLine: step.npcLine || null,
        playerAction: step.playerAction || null,
        triggersEngine: Boolean(step.triggersEngine),
      };

      if (step.type === "dialogue" && step.npcLine) {
        row.summary = step.npcLine;
      } else if (step.type === "travel") {
        row.summary = "이동: " + (step.location || "?");
      } else if (step.type === "fact_input") {
        row.summary = "보고 입력 대기 → 엔진 연동";
        if (options.scenarioText && options.engine && options.parser) {
          turnInResult = runQuestTurnIn({
            questId: options.questId,
            giverId: options.giverId,
            scenarioText: options.scenarioText,
            engine: options.engine,
            parser: options.parser,
            sessionKey: options.sessionKey,
            reputationSessionKey: options.reputationSessionKey,
            persistSession: options.persistSession,
          });
          row.turnIn = {
            completed: turnInResult.completion.completed,
            branchId: turnInResult.outcomeBranch && turnInResult.outcomeBranch.branchId,
            gold: turnInResult.outcome && turnInResult.outcome.rewards && turnInResult.outcome.rewards.gold,
          };
          row.status = turnInResult.completion.completed ? "completed" : "failed";
        }
      } else if (step.type === "interact") {
        row.summary = "상호작용: " + (step.title || step.stepId);
      }

      executed.push(row);
      instance.completedStepIds.push(step.stepId);
      instance.currentStepId = step.stepId;
    }

    instance.state = turnInResult
      ? turnInResult.completion.completed
        ? "completed"
        : "turn_in_failed"
      : "steps_preview_done";

    return {
      questId: quest.id,
      giverId: giver.giverId,
      introDialogue: quest.introDialogue || "",
      acceptDialogue: resolveAcceptDialogue(giver, quest, options),
      completionDialogue: (giver.experience && giver.experience.completionDialogue) || "",
      steps: executed,
      instance: instance,
      turnInResult: turnInResult,
    };
  }

  /**
   * Full local quest play: accept → all steps → turn-in → game state apply.
   */
  function runQuestFlow(options) {
    var opts = options || {};
    var sessionKey = opts.sessionKey || "default";
    var acceptRes = acceptQuest(opts.questId, opts.giverId, opts);
    if (!acceptRes.ok) {
      return { ok: false, accept: acceptRes.accept, reason: acceptRes.reason };
    }
    var stepResults = [];
    var lastAdvance = null;
    while (true) {
      lastAdvance = advanceProcessStep({
        sessionKey: sessionKey,
        scenarioText: opts.scenarioText,
        engine: opts.engine,
        parser: opts.parser,
        reputationSessionKey: opts.reputationSessionKey,
        engineSessionKey: opts.engineSessionKey,
        persistSession: opts.persistSession,
      });
      if (!lastAdvance.ok) break;
      if (lastAdvance.awaitingReport) {
        return {
          ok: false,
          reason: "report required at fact_input",
          accept: acceptRes,
          steps: stepResults,
        };
      }
      stepResults.push(lastAdvance.step);
      if (lastAdvance.done) break;
    }
    var giver = getQuestGiver(getQuest(opts.questId), opts.giverId);
    var flow = QUEST_FLOW_STORE.get(sessionKey);
    var turnInResult = (flow && flow.turnInResult) || (lastAdvance && lastAdvance.turnInResult);
    var completed =
      turnInResult && turnInResult.completion && turnInResult.completion.completed;
    return {
      ok: Boolean(completed),
      accept: acceptRes.accept,
      briefing: acceptRes.briefing,
      introDialogue: acceptRes.introDialogue,
      steps: stepResults,
      instance: flow && flow.instance,
      turnInResult: turnInResult,
      completionDialogue:
        (giver && giver.experience && giver.experience.completionDialogue) || "",
      gameState: global.QuestGameState ? global.QuestGameState.snapshot(sessionKey) : null,
    };
  }

  function clearQuestFlow(sessionKey) {
    QUEST_FLOW_STORE.delete(String(sessionKey || "default"));
  }

  global.QuestRuntime = {
    loadQuestCatalog: loadQuestCatalog,
    setQuestCatalog: setQuestCatalog,
    getQuestCatalog: getQuestCatalog,
    getQuest: getQuest,
    getQuestGiver: getQuestGiver,
    evaluateFactMatch: evaluateFactMatch,
    evaluateQuestCompletion: evaluateQuestCompletion,
    pickOutcomeBranch: pickOutcomeBranch,
    aggregateInterpretedMetrics: aggregateInterpretedMetrics,
    runQuestTurnIn: runQuestTurnIn,
    listQuestGiverOptions: listQuestGiverOptions,
    resolveAcceptDialogue: resolveAcceptDialogue,
    getAcceptDialogue: getAcceptDialogue,
    turnInProfileKey: turnInProfileKey,
    createQuestInstance: createQuestInstance,
    runProcessSteps: runProcessSteps,
    getGiverBriefing: getGiverBriefing,
    acceptQuest: acceptQuest,
    getQuestFlow: getQuestFlow,
    advanceProcessStep: advanceProcessStep,
    runQuestFlow: runQuestFlow,
    clearQuestFlow: clearQuestFlow,
  };
})(typeof window !== "undefined" ? window : globalThis);
