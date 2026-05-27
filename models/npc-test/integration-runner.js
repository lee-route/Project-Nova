(function () {
  var outputBox = document.querySelector("#out-integration-tests");
  var runButton = document.querySelector("#run-integration-tests");

  function loadSuiteSync() {
    try {
      var xhr = new XMLHttpRequest();
      xhr.open("GET", "./integration-test-cases.json", false);
      xhr.send(null);
      if (xhr.status === 200 || xhr.status === 0) return JSON.parse(xhr.responseText);
    } catch (error) {}
    return null;
  }

  function collectRules(result) {
    var rules = [];
    (result.auditTrail || []).forEach(function (item) {
      (item.applied_rules || []).forEach(function (r) {
        if (rules.indexOf(r) < 0) rules.push(r);
      });
    });
    return rules;
  }

  function evaluateCase(testCase, parser, engine) {
    var expect = testCase.expect || {};
    var opts = (testCase.expect && testCase.expect.options) || testCase.options || {};

    if (testCase.custom === "chain") {
      engine.clearSession("itest-chain");
      var facts = parser.buildFactsFromParsed(parser.parseScenarioText(testCase.input));
      var base = engine.getScenario({ sessionKey: "itest-chain" });
      var chain = engine.propagateChain(
        [
          { sender: base.sender, receiver: base.receiver },
          { sender: base.receiver, receiver: base.sender },
        ],
        facts,
        base.info,
        { quantityMode: "faithful", currentTick: 1000 }
      );
      if (expect.hopMin && chain.hopResults.length < expect.hopMin) {
        return { ok: false, reason: "hop count " + chain.hopResults.length };
      }
      if (expect.finalFactsMin && chain.finalFacts.length < expect.finalFactsMin) {
        return { ok: false, reason: "final facts " + chain.finalFacts.length };
      }
      return { ok: true, reason: "" };
    }

    if (testCase.custom === "session") {
      engine.clearSession("itest-session");
      var f1 = parser.buildFactsFromParsed(parser.parseScenarioText(testCase.input));
      var r1 = engine.executeScenario({ facts: f1, persistSession: true, sessionKey: "itest-session", quantityMode: "faithful" });
      var r2 = engine.executeScenario({
        facts: f1,
        persistSession: true,
        sessionKey: "itest-session",
        quantityMode: "faithful",
        currentTick: 1005,
      });
      if (expect.kbGrowth && r2.knowledgeBaseSnapshot.length <= r1.knowledgeBaseSnapshot.length) {
        return { ok: false, reason: "KB did not grow/merge" };
      }
      return { ok: true, reason: "" };
    }

    if (testCase.custom === "kbConflict") {
      engine.clearSession("itest-kbconflict");
      var threatFacts = [
        {
          fact_id: "F01",
          subject: "늑대",
          action: "탈출",
          target: "북문",
          quantity: 3,
          certainty: 0.7,
          is_countable: true,
          action_type: "tactical_move",
          parse_mode: "structured",
          parse_confidence: 0.9,
        },
      ];
      var calmFacts = [
        {
          fact_id: "F02",
          subject: "늑대",
          action: "태연한 상태다",
          target: "북문",
          quantity: 1,
          certainty: 0.6,
          is_countable: false,
          action_type: "state",
          parse_mode: "structured",
          parse_confidence: 0.9,
        },
      ];

      var r1 = engine.executeScenario({
        facts: threatFacts,
        persistSession: true,
        sessionKey: "itest-kbconflict",
        quantityMode: "faithful",
        currentTick: 1000,
        senderStats: { fear: 0.9, hostility: 0.2, trust: 0.55 },
        receiverStats: { credulity: 0.7, trust: 0.65 },
        allowPartialTrust: false,
      });
      var threat1 = (r1.knowledgeBaseSnapshot || []).find(function (x) {
        return (
          String(x.truth_value.action_type || "").toLowerCase() === "tactical_move" &&
          String(x.truth_value.target || "").indexOf("북문") >= 0
        );
      });
      var c1 = threat1 ? Number(threat1.truth_value.certainty || 0) : 0;

      var r2 = engine.executeScenario({
        facts: calmFacts,
        persistSession: true,
        sessionKey: "itest-kbconflict",
        quantityMode: "faithful",
        currentTick: 1001,
        senderStats: { fear: 0.1, hostility: 0.2, trust: 0.55 },
        receiverStats: { credulity: 0.8, trust: 0.65 },
        allowPartialTrust: false,
      });
      var threat2 = (r2.knowledgeBaseSnapshot || []).find(function (x) {
        return (
          String(x.truth_value.action_type || "").toLowerCase() === "tactical_move" &&
          String(x.truth_value.target || "").indexOf("북문") >= 0
        );
      });
      var c2 = threat2 ? Number(threat2.truth_value.certainty || 0) : 0;

      if (expect.threatCertaintyDecreased && !(c2 < c1)) {
        return { ok: false, reason: "threat certainty did not decrease: " + c1 + " -> " + c2 };
      }
      return { ok: true, reason: "" };
    }

    var parsed = parser.parseScenarioText(testCase.input);
    var facts = parsed.facts && parsed.facts.length ? parsed.facts : parser.buildFactsFromParsed(parsed);

    if (expect.quantity != null && facts[0] && Number(facts[0].quantity) !== expect.quantity) {
      return { ok: false, reason: "qty " + facts[0].quantity + " != " + expect.quantity };
    }
    if (expect.is_countable != null && facts[0] && Boolean(facts[0].is_countable) !== expect.is_countable) {
      return { ok: false, reason: "countable mismatch" };
    }

    var result = engine.executeScenario({
      facts: facts,
      quantityMode: opts.quantityMode || "dramatic",
      senderStats: { fear: opts.senderFear != null ? opts.senderFear : 0.5, hostility: 0.5, trust: 0.55 },
      receiverStats: { credulity: 0.7, trust: 0.65 },
      groundTruth: testCase.groundTruth || null,
    });

    if (expect.auditDiffMin != null && (result.auditDiff || []).length < expect.auditDiffMin) {
      return { ok: false, reason: "auditDiff missing" };
    }
    if (expect.hasFields && result.auditDiff && result.auditDiff[0]) {
      for (var i = 0; i < expect.hasFields.length; i += 1) {
        if (result.auditDiff[0][expect.hasFields[i]] == null) {
          return { ok: false, reason: "audit field " + expect.hasFields[i] };
        }
      }
    }

    var rules = collectRules(result);
    if (expect.rulesInclude) {
      for (var r = 0; r < expect.rulesInclude.length; r += 1) {
        if (rules.indexOf(expect.rulesInclude[r]) < 0) {
          return { ok: false, reason: "missing rule " + expect.rulesInclude[r] + " in " + JSON.stringify(rules) };
        }
      }
    }

    if (expect.groundTruthScoreMin != null) {
      var score = result.groundTruthReport ? result.groundTruthReport.score : 0;
      if (score < expect.groundTruthScoreMin) {
        return { ok: false, reason: "ground truth score " + score };
      }
    }

    return { ok: true, reason: "" };
  }

  function runSuite(suite) {
    var parser = window.NpcParser;
    var engine = window.QuestSystem;
    if (!parser || !engine) {
      if (outputBox) outputBox.textContent = "FAIL: parser/engine missing";
      return;
    }
    var lines = ["=== Integration E2E ===", ""];
    var pass = 0;
    var fail = 0;
    for (var i = 0; i < (suite.cases || []).length; i += 1) {
      var c = suite.cases[i];
      var res = evaluateCase(c, parser, engine);
      if (res.ok) {
        pass += 1;
        lines.push("[PASS] " + c.id);
      } else {
        fail += 1;
        lines.push("[FAIL] " + c.id + " -> " + res.reason);
      }
    }
    lines.push("");
    lines.push("summary: " + pass + " passed, " + fail + " failed");
    if (outputBox) outputBox.textContent = lines.join("\n");
  }

  if (runButton) {
    runButton.addEventListener("click", function () {
      var suite = loadSuiteSync();
      if (suite) runSuite(suite);
      else if (outputBox) outputBox.textContent = "FAIL: integration-test-cases.json load failed";
    });
  }
})();
