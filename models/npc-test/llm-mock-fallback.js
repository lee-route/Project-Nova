/**
 * LlmFactAnchor 실패 시 사용 — grounded facts 고정, 말투·프레이밍만 persona/violation별 분기
 */
(function (global) {
  function pickKoreanParticle(word, withBatchim, withoutBatchim) {
    var text = String(word || "").trim();
    if (!text) return withoutBatchim;
    var lastChar = text.charAt(text.length - 1);
    var code = lastChar.charCodeAt(0);
    if (code < 44032 || code > 55203) return withoutBatchim;
    return (code - 44032) % 28 !== 0 ? withBatchim : withoutBatchim;
  }

  function primaryViolation(violations) {
    if (!violations || !violations.length) return "generic";
    var order = ["quantity_mismatch", "subject_missing"];
    for (var i = 0; i < order.length; i += 1) {
      for (var j = 0; j < violations.length; j += 1) {
        if (violations[j].type === order[i]) return order[i];
      }
    }
    return violations[0].type || "generic";
  }

  /**
   * @returns {string} style id for logging / UE 연출 훅
   */
  function pickFallbackStyle(persona, violations) {
    var p = String(persona || "neutral");
    var v = primaryViolation(violations);
    if (v === "quantity_mismatch") return p + "_qty_safe";
    if (v === "subject_missing") return p + "_subject_hedge";
    return p + "_recover";
  }

  function lineForFact(fact, mode) {
    var sub = String(fact.subject || "대상");
    var pa = pickKoreanParticle(sub, "이", "가");
    var tgt = fact.target ? String(fact.target) : "장소 불명";
    var qty = fact.is_countable ? "수량 약 " + fact.quantity : "";
    var cert = fact.certainty != null ? "확신도 " + fact.certainty : "";

    if (mode === "qty_only") {
      return sub + pa + (qty ? " " + qty : "") + (tgt !== "장소 불명" ? ", " + tgt : "");
    }
    if (mode === "hedge") {
      return "그 현장(" + tgt + ")에서 " + sub + pa + " 관련 정황" + (qty ? ", " + qty : "") + (cert ? ", " + cert : "");
    }
    return sub + pa + " " + String(fact.action || "관련됨") + ". " + tgt + (qty ? ", " + qty : "") + (cert ? ", " + cert : "");
  }

  var WRAPPERS = {
    fearful_guard_qty_safe: {
      open: "…잠깐, 숫자부터 맞춰 말할게. ",
      close: " (주변을 살피며 목소리를 낮춘다)",
    },
    fearful_guard_subject_hedge: {
      open: "정확한 이름은… 기억이 흐릿한데, 들은 내용만은 이래. ",
      close: " (말끝을 흐린다)",
    },
    fearful_guard_recover: {
      open: "미안, 다시 말할게. 들은 건 이거야. ",
      close: " (조심스럽게)",
    },
    calm_scholar_qty_safe: {
      open: "기록상 확인된 수치만 정리하면, ",
      close: ".",
    },
    calm_scholar_subject_hedge: {
      open: "명칭은 단정할 수 없으나, 사실 관계는 다음과 같다. ",
      close: ".",
    },
    calm_scholar_recover: {
      open: "요약하면, ",
      close: ".",
    },
    cynical_merchant_qty_safe: {
      open: "뭐, 숫자만 믿을 만한 건 이거지. ",
      close: " (한숨)",
    },
    cynical_merchant_subject_hedge: {
      open: "이름은 대충 넘어가고, 요지만 봐. ",
      close: "",
    },
    cynical_merchant_recover: {
      open: "다시 말해줄게, 귀찮지만. ",
      close: "",
    },
    hotblood_hunter_qty_safe: {
      open: "숫자! 이거다! ",
      close: "!",
    },
    hotblood_hunter_subject_hedge: {
      open: "누군지는 몰라도 현장은 확실해! ",
      close: "!",
    },
    hotblood_hunter_recover: {
      open: "아니, 이렇게다! ",
      close: "!",
    },
    witness_qty_safe: {
      open: "본 것 중 확실한 수치는, ",
      close: ".",
    },
    witness_subject_hedge: {
      open: "딱히 이름은 모르겠고, ",
      close: ".",
    },
    witness_recover: {
      open: "다시 하면, ",
      close: ".",
    },
    neutral_qty_safe: { open: "확인된 내용만 말하면, ", close: "." },
    neutral_subject_hedge: { open: "들은 바로는, ", close: "." },
    neutral_recover: { open: "정리하면, ", close: "." },
  };

  function resolveWrapper(styleId, persona) {
    if (WRAPPERS[styleId]) return WRAPPERS[styleId];
    var p = String(persona || "neutral");
    var suffix = styleId.indexOf("_qty_safe") >= 0 ? "_qty_safe" : styleId.indexOf("_subject_hedge") >= 0 ? "_subject_hedge" : "_recover";
    return WRAPPERS[p + suffix] || WRAPPERS.neutral_recover;
  }

  function factLineMode(styleId) {
    if (styleId.indexOf("_qty_safe") >= 0) return "qty_only";
    if (styleId.indexOf("_subject_hedge") >= 0) return "hedge";
    return "full";
  }

  /**
   * @param {object[]} facts grounded facts (unchanged values)
   */
  function buildAnchoredFallbackSpeech(facts, persona, styleId, bundleContext) {
    var list = facts || [];
    var wrap = resolveWrapper(styleId, persona);
    var mode = factLineMode(styleId);
    var body = list.map(function (f) {
      return lineForFact(f, mode);
    });
    var tension =
      bundleContext && bundleContext.hasContradiction
        ? " 다만 일부 정보는 서로 맞지 않아 보인다."
        : "";
    if (!body.length) {
      return wrap.open + "전달된 정보가 없다" + wrap.close;
    }
    var core = body.length > 1 ? body.join(" 또, ") : body[0];
    return wrap.open + core + wrap.close + tension;
  }

  global.LlmMockFallback = {
    pickFallbackStyle: pickFallbackStyle,
    primaryViolation: primaryViolation,
    buildAnchoredFallbackSpeech: buildAnchoredFallbackSpeech,
    FALLBACK_STYLE_SUFFIXES: ["_qty_safe", "_subject_hedge", "_recover"],
  };
})(typeof window !== "undefined" ? window : globalThis);
