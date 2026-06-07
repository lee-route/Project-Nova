/**
 * Turn-in 보고 vs WorldTruth / PlayerKnowledge — 게임 outcome 변경 없음(진단·2차 기만용).
 */
(function (global) {
  function resolveLayers() {
    return {
      WorldTruth: global.WorldTruth || null,
      PlayerKnowledge: global.PlayerKnowledge || null,
    };
  }

  function subjectSim(a, b) {
    var x = String(a || "");
    var y = String(b || "");
    if (!x || !y) return 0;
    return x.includes(y) || y.includes(x) ? 1 : 0;
  }

  function compareReportToWorld(reportFacts, worldFacts) {
    if (!worldFacts || !worldFacts.length) {
      return { hasWorldTruth: false, contradictions: [], aligned: [] };
    }
    var contradictions = [];
    var aligned = [];
    (reportFacts || []).forEach(function (rf) {
      var best = 0;
      var bestWt = null;
      for (var i = 0; i < worldFacts.length; i += 1) {
        var wt = worldFacts[i].snapshot || worldFacts[i];
        var qtyDelta = Math.abs(Number(rf.quantity) - Number(wt.quantity));
        var qtyScore =
          !rf.is_countable && !wt.is_countable
            ? 0.8
            : qtyDelta === 0
              ? 1
              : qtyDelta <= 2
                ? 0.85
                : qtyDelta <= 5
                  ? 0.4
                  : 0;
        var sc =
          0.45 * subjectSim(rf.subject, wt.subject) +
          0.25 * (rf.action_type === wt.action_type ? 1 : 0.4) +
          0.3 * qtyScore;
        if (sc > best) {
          best = sc;
          bestWt = wt;
        }
      }
      var row = {
        report: { subject: rf.subject, quantity: rf.quantity, target: rf.target },
        world: bestWt,
        score: Number(best.toFixed(2)),
        aligned: best >= 0.72,
      };
      if (best >= 0.72) aligned.push(row);
      else contradictions.push(row);
    });
    return {
      hasWorldTruth: true,
      contradictions: contradictions,
      aligned: aligned,
      severity:
        contradictions.length === 0
          ? "none"
          : contradictions.length >= (reportFacts || []).length
            ? "high"
            : "partial",
    };
  }

  function auditUnsupportedClaims(reportFacts, playerView) {
    var unsupported = [];
    if (!playerView || !playerView.entries || !playerView.entries.length) {
      return { checked: false, unsupported: [], note: "empty_player_knowledge" };
    }
    var knownSubjects = [];
    playerView.flatFacts.forEach(function (pf) {
      if (pf.fact && pf.fact.subject) knownSubjects.push(String(pf.fact.subject));
    });
    (reportFacts || []).forEach(function (rf) {
      var sub = String(rf.subject || "");
      var ok = knownSubjects.some(function (k) {
        return subjectSim(sub, k) >= 1;
      });
      if (!ok && sub.length >= 2) {
        unsupported.push({ subject: sub, reason: "not_in_player_knowledge_before_report" });
      }
    });
    return {
      checked: true,
      unsupported: unsupported,
      severity: unsupported.length ? "medium" : "none",
    };
  }

  /**
   * @param {string} sessionKey
   * @param {object[]} reportFacts parser facts from this turn-in
   * @param {object} options { worldTruthFacts?, investigationSeeded? }
   */
  function auditTurnInReport(sessionKey, reportFacts, options) {
    var opts = options || {};
    var layers = resolveLayers();
    var out = {
      affectsOutcome: false,
      v1Policy: "diagnostic_only",
      worldCompare: null,
      playerCompare: null,
      unsupportedClaims: null,
    };

    if (layers.WorldTruth) {
      if (opts.worldTruthFacts && opts.worldTruthFacts.length) {
        layers.WorldTruth.setWorldFacts(sessionKey, opts.worldTruthFacts, opts.tick || 0, "investigation");
      }
      var worldRows = layers.WorldTruth.getWorldFacts(sessionKey);
      out.worldCompare = compareReportToWorld(reportFacts, worldRows);
    }

    if (layers.PlayerKnowledge) {
      out.playerCompare = layers.PlayerKnowledge.comparePlayerToWorld(sessionKey);
      var view = layers.PlayerKnowledge.getPlayerView(sessionKey);
      out.unsupportedClaims = auditUnsupportedClaims(reportFacts, view);
    }

    return out;
  }

  global.DeceptionAudit = {
    auditTurnInReport: auditTurnInReport,
    compareReportToWorld: compareReportToWorld,
  };
})(typeof window !== "undefined" ? window : globalThis);
