(function () {
  var outputBox = document.querySelector("#out-distortion-tests");
  var runButton = document.querySelector("#run-distortion-tests");

  function includesAll(rules, expected) {
    if (!expected || !expected.length) return true;
    for (var i = 0; i < expected.length; i += 1) {
      if (rules.indexOf(expected[i]) < 0) return false;
    }
    return true;
  }

  function excludesAll(rules, excluded) {
    if (!excluded || !excluded.length) return true;
    for (var i = 0; i < excluded.length; i += 1) {
      if (rules.indexOf(excluded[i]) >= 0) return false;
    }
    return true;
  }

  function collectRules(result) {
    var rules = [];
    var trail = result.auditTrail || [];
    for (var i = 0; i < trail.length; i += 1) {
      var r = trail[i].applied_rules || [];
      for (var j = 0; j < r.length; j += 1) {
        if (rules.indexOf(r[j]) < 0) rules.push(r[j]);
      }
    }
    return rules;
  }

  function maxQuantity(result) {
    var max = 0;
    var list = result.propagation.interpretedFacts || [];
    for (var i = 0; i < list.length; i += 1) {
      max = Math.max(max, Number(list[i].truth_value.quantity || 0));
    }
    return max;
  }

  function evaluateCase(testCase, parser, engine) {
    var expect = testCase.expect || {};
    if (testCase.custom === "kb_merge") {
      var kb = new engine.KnowledgeBase();
      var atom = {
        info_id: "F01",
        truth_value: {
          subject: "늑대",
          action: "탈출",
          target: "북문",
          quantity: 3,
          certainty: 0.7,
          is_countable: true,
          action_type: "tactical_move",
        },
        metadata: { applied_rules: [], bundle_notes: [], rumor: false },
      };
      kb.update(atom);
      var atom2 = JSON.parse(JSON.stringify(atom));
      atom2.truth_value.quantity = 7;
      atom2.truth_value.certainty = 0.9;
      var mergeRes = kb.update(atom2);
      var list = kb.list();
      if (!expect.kbMerge) return { ok: true, reason: "" };
      if (mergeRes.action !== "merge") return { ok: false, reason: "expected merge got " + mergeRes.action };
      if (list.length !== 1) return { ok: false, reason: "KB size " + list.length };
      if (Number(list[0].truth_value.quantity) < 7) return { ok: false, reason: "qty merge failed" };
      return { ok: true, reason: "" };
    }

    var opts = testCase.options || {};
    var facts = null;
    if (testCase.input) {
      var parsed = parser.parseScenarioText(testCase.input);
      facts = parsed.facts && parsed.facts.length ? parsed.facts : parser.buildFactsFromParsed(parsed);
    }

    var result = engine.executeScenario({
      facts: facts,
      trustLevel: opts.trustLevel != null ? opts.trustLevel : 0.74,
      allowPartialTrust: opts.allowPartialTrust !== false,
      quantityMode: opts.quantityMode || "dramatic",
      senderStats: {
        fear: opts.senderFear != null ? opts.senderFear : 0.5,
        hostility: opts.senderHostility != null ? opts.senderHostility : 0.5,
        trust: 0.55,
      },
      receiverStats: {
        credulity: opts.receiverCredulity != null ? opts.receiverCredulity : 0.7,
        trust: 0.65,
      },
      senderReputation: opts.senderReputation || { 촌장: 0.85, 늑대: 0.25 },
      receiverReputation: opts.receiverReputation || { 촌장: 0.85, 늑대: 0.35 },
    });

    if (Boolean(expect.blocked) !== Boolean(result.propagation.blocked)) {
      return { ok: false, reason: "blocked mismatch" };
    }
    if (expect.blocked) return { ok: true, reason: "" };

    if (expect.factRumor && facts && facts[0] && !facts[0].rumor) {
      return { ok: false, reason: "fact.rumor not set" };
    }

    var rules = collectRules(result);
    if (!includesAll(rules, expect.rulesInclude)) {
      return { ok: false, reason: "missing rules: " + JSON.stringify(expect.rulesInclude) + " got " + JSON.stringify(rules) };
    }
    if (!excludesAll(rules, expect.rulesExclude)) {
      return { ok: false, reason: "forbidden rules present: " + JSON.stringify(expect.rulesExclude) };
    }

    if (expect.minQuantity != null && maxQuantity(result) < expect.minQuantity) {
      return { ok: false, reason: "qty " + maxQuantity(result) + " < " + expect.minQuantity };
    }
    if (expect.maxQuantity != null && maxQuantity(result) > expect.maxQuantity) {
      return { ok: false, reason: "qty " + maxQuantity(result) + " > " + expect.maxQuantity };
    }
    if (expect.partialTrust && !result.propagation.partialTrust) {
      return { ok: false, reason: "expected partialTrust" };
    }
    if (expect.bundleContradiction && !(result.bundleContext && result.bundleContext.hasContradiction)) {
      return { ok: false, reason: "expected bundle contradiction" };
    }
    if (expect.factsMin != null) {
      var n = (result.propagation.interpretedFacts || []).length;
      if (n < expect.factsMin) return { ok: false, reason: "facts " + n + " < " + expect.factsMin };
    }

    if (expect.auditDiffMin != null) {
      var diffLen = (result.auditDiff || []).length;
      if (diffLen < expect.auditDiffMin) return { ok: false, reason: "auditDiff " + diffLen };
    }

    return { ok: true, reason: "" };
  }

  function runSuite(suite) {
    var parser = window.NpcParser;
    var engine = window.QuestSystem;
    if (!parser || !engine) {
      renderReport(["FAIL: NpcParser or QuestSystem missing"]);
      return;
    }

    var cases = suite.cases || [];
    var pass = 0;
    var fail = 0;
    var lines = ["=== Distortion Regression ===", "cases: " + cases.length, ""];

    for (var i = 0; i < cases.length; i += 1) {
      var testCase = cases[i];
      var result = evaluateCase(testCase, parser, engine);
      if (result.ok) {
        pass += 1;
        lines.push("[PASS] " + testCase.id);
      } else {
        fail += 1;
        lines.push("[FAIL] " + testCase.id + " -> " + result.reason);
      }
    }

    lines.push("");
    lines.push("summary: " + pass + " passed, " + fail + " failed");
    renderReport(lines);
  }

  function renderReport(lines) {
    if (outputBox) outputBox.textContent = lines.join("\n");
  }

  function loadSuiteSync() {
    try {
      var xhr = new XMLHttpRequest();
      xhr.open("GET", "./distortion-test-cases.json", false);
      xhr.send(null);
      if (xhr.status === 200 || xhr.status === 0) return JSON.parse(xhr.responseText);
    } catch (error) {}
    return null;
  }

  function loadAndRun() {
    var suite = loadSuiteSync();
    if (suite) {
      runSuite(suite);
      return;
    }
    fetch("./distortion-test-cases.json")
      .then(function (r) {
        return r.json();
      })
      .then(runSuite)
      .catch(function () {
        renderReport(["FAIL: could not load distortion-test-cases.json"]);
      });
  }

  if (runButton) runButton.addEventListener("click", loadAndRun);
})();
