(function () {
  var outputBox = document.querySelector("#out-test-results");
  var runButton = document.querySelector("#run-parser-tests");

  function includesText(haystack, needle) {
    if (!needle) return true;
    return String(haystack || "").indexOf(String(needle)) >= 0;
  }

  function checkPrimaryFact(primary, expectPrimary) {
    if (!expectPrimary) return { ok: true, reason: "" };
    if (expectPrimary.subjectContains && !includesText(primary.subject, expectPrimary.subjectContains)) {
      return { ok: false, reason: "subject mismatch: " + primary.subject };
    }
    if (expectPrimary.targetContains && !includesText(primary.target, expectPrimary.targetContains)) {
      return { ok: false, reason: "target mismatch: " + primary.target };
    }
    if (expectPrimary.actionType && primary.action_type !== expectPrimary.actionType) {
      return { ok: false, reason: "action_type mismatch: " + primary.action_type };
    }
    if (expectPrimary.quantity != null && Number(primary.quantity) !== Number(expectPrimary.quantity)) {
      return { ok: false, reason: "quantity mismatch: " + primary.quantity };
    }
    if (expectPrimary.certaintyMax != null && Number(primary.certainty) > Number(expectPrimary.certaintyMax)) {
      return { ok: false, reason: "certainty too high: " + primary.certainty };
    }
    return { ok: true, reason: "" };
  }

  function checkAnyFact(facts, anyFactRules) {
    if (!anyFactRules || !anyFactRules.length) return { ok: true, reason: "" };
    for (var i = 0; i < anyFactRules.length; i += 1) {
      var rule = anyFactRules[i];
      var matched = false;
      for (var j = 0; j < facts.length; j += 1) {
        var fact = facts[j];
        var subjectOk = !rule.subjectContains || includesText(fact.subject, rule.subjectContains);
        var typeOk = !rule.actionType || fact.action_type === rule.actionType;
        if (subjectOk && typeOk) {
          matched = true;
          break;
        }
      }
      if (!matched) {
        return { ok: false, reason: "missing fact rule: " + JSON.stringify(rule) };
      }
    }
    return { ok: true, reason: "" };
  }

  function evaluateCase(testCase, parser) {
    var parsed = parser.parseScenarioText(testCase.input);
    var facts = parsed.facts && parsed.facts.length ? parsed.facts : parser.buildFactsFromParsed(parsed);
    var primary = parser.selectPrimaryParse(parsed) || parsed;
    var expect = testCase.expect || {};
    var sentenceCount = parsed.sentenceParses ? parsed.sentenceParses.length : 1;

    if (expect.sentenceCount != null && sentenceCount !== expect.sentenceCount) {
      return { ok: false, reason: "sentenceCount " + sentenceCount + " != " + expect.sentenceCount };
    }
    if (expect.factsMin != null && facts.length < expect.factsMin) {
      return { ok: false, reason: "facts length " + facts.length + " < " + expect.factsMin };
    }

    var primaryCheck = checkPrimaryFact(primary, expect.primary);
    if (!primaryCheck.ok) return primaryCheck;

    var anyCheck = checkAnyFact(facts, expect.anyFact);
    if (!anyCheck.ok) return anyCheck;

    return { ok: true, reason: "" };
  }

  function renderReport(lines) {
    if (outputBox) outputBox.textContent = lines.join("\n");
  }

  function runEmbeddedSuite(suite) {
    var parser = window.NpcParser;
    if (!parser) {
      renderReport(["FAIL: window.NpcParser is not available"]);
      return;
    }

    var cases = suite.cases || [];
    var pass = 0;
    var fail = 0;
    var lines = ["=== Parser Regression ===", "cases: " + cases.length, ""];

    for (var i = 0; i < cases.length; i += 1) {
      var testCase = cases[i];
      var result = evaluateCase(testCase, parser);
      if (result.ok) {
        pass += 1;
        lines.push("[PASS] " + testCase.id + (testCase.suite ? " (" + testCase.suite + ")" : " (legacy)"));
      } else {
        fail += 1;
        lines.push("[FAIL] " + testCase.id + (testCase.suite ? " (" + testCase.suite + ")" : " (legacy)") + " -> " + result.reason);
        lines.push("       input: " + testCase.input);
      }
    }

    lines.push("");
    lines.push("summary: " + pass + " passed, " + fail + " failed");
    renderReport(lines);
  }

  function loadSuiteSync() {
    if (window.NPC_TEST_CASES) return window.NPC_TEST_CASES;
    try {
      var xhr = new XMLHttpRequest();
      xhr.open("GET", "./test-cases.json", false);
      xhr.send(null);
      if (xhr.status === 200 || xhr.status === 0) {
        return JSON.parse(xhr.responseText);
      }
    } catch (error) {}
    return null;
  }

  function loadSuiteAndRun() {
    var syncSuite = loadSuiteSync();
    if (syncSuite) {
      runEmbeddedSuite(syncSuite);
      return;
    }

    fetch("./test-cases.json")
      .then(function (response) {
        if (!response.ok) throw new Error("fetch failed");
        return response.json();
      })
      .then(function (suite) {
        runEmbeddedSuite(suite);
      })
      .catch(function () {
        renderReport(["FAIL: could not load test-cases.json"]);
      });
  }

  if (runButton) {
    runButton.addEventListener("click", loadSuiteAndRun);
  }
})();
