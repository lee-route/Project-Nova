/**
 * LLM 출력 대사가 [Grounded DATA] 수치·주어와 어긋나지 않는지 검증
 */
(function (global) {
  var KOREAN_NUM = {
    한: 1,
    두: 2,
    세: 3,
    네: 4,
    다섯: 5,
    여섯: 6,
    일곱: 7,
    여덟: 8,
    아홉: 9,
    열: 10,
    열한: 11,
    열두: 12,
    열세: 13,
    열네: 14,
    열다섯: 15,
    스물: 20,
    서른: 30,
    백: 100,
    천: 1000,
  };

  function extractNumbers(text) {
    var t = String(text || "");
    var found = [];
    var re = /(\d{1,4})\s*(개|명|마리|상자|포|다발)?/g;
    var m;
    while ((m = re.exec(t)) !== null) {
      found.push(Number(m[1]));
    }
    var words = Object.keys(KOREAN_NUM).sort(function (a, b) {
      return b.length - a.length;
    });
    words.forEach(function (word) {
      if (t.indexOf(word) >= 0) found.push(KOREAN_NUM[word]);
    });
    var mQty = t.match(/수량\s*약\s*(\d+)/);
    if (mQty) found.push(Number(mQty[1]));
    return found;
  }

  function normalizeFacts(groundedContext) {
    if (!groundedContext) return [];
    if (Array.isArray(groundedContext)) return groundedContext;
    if (groundedContext.facts) return groundedContext.facts;
    if (groundedContext.dataOnly && groundedContext.dataOnly.facts) {
      return groundedContext.dataOnly.facts;
    }
    return [];
  }

  function subjectInSpeech(subject, speech) {
    var s = String(subject || "").trim();
    if (!s || s.length < 2) return true;
    return String(speech || "").indexOf(s) >= 0;
  }

  /**
   * @returns {{ ok: boolean, score: number, violations: Array }}
   */
  function validateSpeech(npcSpeech, groundedContext, options) {
    var opts = options || {};
    var facts = normalizeFacts(groundedContext);
    var speech = String(npcSpeech || "");
    var violations = [];
    var checks = 0;
    var passed = 0;

    facts.forEach(function (fact) {
      if (fact.is_countable) {
        checks += 1;
        var qty = Number(fact.quantity ?? 1);
        var nums = extractNumbers(speech);
        if (!nums.length) {
          passed += 1;
          return;
        }
        var okQty = nums.some(function (n) {
          return Math.abs(n - qty) <= Math.max(2, Math.round(qty * 0.35));
        });
        if (!okQty) {
          violations.push({
            type: "quantity_mismatch",
            expected: qty,
            found: nums,
            subject: fact.subject,
          });
        } else {
          passed += 1;
        }
      }

      if (fact.subject && String(fact.subject).length >= 2) {
        checks += 1;
        if (subjectInSpeech(fact.subject, speech)) {
          passed += 1;
        } else if (opts.requireSubjectMention) {
          violations.push({
            type: "subject_missing",
            expected: fact.subject,
          });
        } else {
          passed += 1;
        }
      }
    });

    var score = checks ? passed / checks : 1;
    var ok = violations.length === 0 && score >= (opts.minScore != null ? opts.minScore : 0.85);
    return {
      ok: ok,
      score: Number(score.toFixed(2)),
      violations: violations,
      checks: checks,
    };
  }

  global.LlmFactAnchor = {
    validateSpeech: validateSpeech,
    extractNumbers: extractNumbers,
  };
})(typeof window !== "undefined" ? window : globalThis);
