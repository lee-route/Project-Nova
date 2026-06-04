/**
 * LLM adapter — default mock; switch to live API after billing.
 *
 * Browser:
 *   LlmAdapter.configure({ provider: "openai", apiKey: "sk-...", model: "gpt-4o-mini", useLive: true });
 *   await LlmAdapter.generateAsync({ systemPrompt, groundedContext, persona, bundleContext });
 *
 * Or copy llm-config.example.json → llm-config.local.json and load via llm-settings.js UI.
 */
(function (global) {
  var DEFAULT_CONFIG = {
    provider: "mock",
    model: "gpt-4o-mini",
    apiKey: "",
    baseUrl: "",
    useLive: false,
    timeoutMs: 45000,
  };

  var config = Object.assign({}, DEFAULT_CONFIG);

  function configure(partial) {
    if (!partial || typeof partial !== "object") return getConfig();
    if (partial.provider !== undefined) config.provider = String(partial.provider || "mock");
    if (partial.model !== undefined) config.model = String(partial.model || DEFAULT_CONFIG.model);
    if (partial.apiKey !== undefined) config.apiKey = String(partial.apiKey || "");
    if (partial.baseUrl !== undefined) config.baseUrl = String(partial.baseUrl || "");
    if (partial.useLive !== undefined) config.useLive = Boolean(partial.useLive);
    if (partial.timeoutMs !== undefined) config.timeoutMs = Number(partial.timeoutMs) || DEFAULT_CONFIG.timeoutMs;
    return getConfig();
  }

  function getConfig() {
    return {
      provider: config.provider,
      model: config.model,
      apiKey: config.apiKey ? "***" + config.apiKey.slice(-4) : "",
      baseUrl: config.baseUrl,
      useLive: config.useLive,
      timeoutMs: config.timeoutMs,
      hasApiKey: Boolean(config.apiKey),
    };
  }

  function getConfigForRuntime() {
    return Object.assign({}, config);
  }

  function isLive() {
    return config.useLive && config.provider !== "mock" && Boolean(config.apiKey);
  }

  function pickKoreanParticle(word, withBatchim, withoutBatchim) {
    var text = String(word || "").trim();
    if (!text) return withoutBatchim;
    var lastChar = text.charAt(text.length - 1);
    var code = lastChar.charCodeAt(0);
    if (code < 44032 || code > 55203) return withoutBatchim;
    return (code - 44032) % 28 !== 0 ? withBatchim : withoutBatchim;
  }

  function normalizeActionForSpeech(action) {
    var a = String(action || "").trim();
    if (!a) return "상황을 전달";
    if (/(다|했다|되었다|당했다|중이다|보인다|상태다)$/.test(a)) return a;
    return a + "했다";
  }

  function factToSpeechLine(fact, persona) {
    var subjectText = String(fact.subject || "대상");
    var subjectParticle = pickKoreanParticle(subjectText, "이", "가");
    var actionText = normalizeActionForSpeech(fact.action);
    var qtyText = fact.is_countable ? "수량 약 " + fact.quantity : "수량 해당 없음";
    var targetText = fact.target ? "장소 " + fact.target : "장소 불명";
    var certaintyText = "확신도 " + fact.certainty;
    if (persona === "fearful_guard") {
      return subjectText + subjectParticle + " " + actionText + ". (" + targetText + ", " + qtyText + ", " + certaintyText + ")";
    }
    if (persona === "hotblood_hunter") {
      return subjectText + subjectParticle + " " + actionText + "! " + targetText + ", " + qtyText + "!";
    }
    if (persona === "calm_scholar") {
      return "정리하면 " + subjectText + subjectParticle + " " + actionText + ". " + targetText + ", " + certaintyText + ".";
    }
    return subjectText + subjectParticle + " " + actionText + ". " + targetText + ", " + qtyText + ", " + certaintyText + ".";
  }

  function buildUserMessage(groundedContext, bundleContext) {
    var dataBlock = JSON.stringify(groundedContext.dataOnly || groundedContext, null, 2);
    var notes =
      bundleContext && bundleContext.hasContradiction
        ? "\n[주의] 번들 내 일부 사실이 서로 모순될 수 있음 — DATA에 있는 확신도만 반영."
        : "";
    return (
      "[Grounded DATA]\n" +
      dataBlock +
      notes +
      "\n\n위 JSON의 사실만 사용해 NPC 대사를 2~4문장 한국어로 작성하라. " +
      "수치·장소·주어·행동을 바꾸거나 추가 사건을 지어내지 마라. 말투만 페르소나에 맞출 것."
    );
  }

  function resolveAnchor() {
    return global.LlmFactAnchor || null;
  }

  function attachAnchorValidation(out, groundedContext, opts) {
    var anchor = resolveAnchor();
    if (!anchor || typeof anchor.validateSpeech !== "function") {
      out.anchorValidation = { ok: true, skipped: true };
      return out;
    }
    var validation = anchor.validateSpeech(out.npcSpeech, groundedContext, opts);
    out.anchorValidation = validation;
    return out;
  }

  function wrapDialogueOutput(systemPrompt, groundedContext, npcSpeech, provider, anchorOpts) {
    var dataJson = JSON.stringify(groundedContext.dataOnly || {}, null, 2);
    var finalSpeech =
      "[System Guardrail] " +
      systemPrompt +
      "\n[Grounded DATA]\n" +
      dataJson +
      "\n[NPC Speech]\n" +
      npcSpeech;
    var out = {
      finalSpeech: finalSpeech,
      npcSpeech: npcSpeech,
      provider: provider || "mock",
    };
    return attachAnchorValidation(out, groundedContext, anchorOpts);
  }

  function resolveMockFallback() {
    return global.LlmMockFallback || null;
  }

  /**
   * Anchor 실패 후 연출용 mock — facts는 그대로, 말투만 styleId에 따라 변경
   */
  function mockGenerateFallback(params, liveValidation) {
    var systemPrompt = params.systemPrompt;
    var groundedContext = params.groundedContext;
    var persona = params.persona;
    var bundleContext = params.bundleContext || {};
    var facts = groundedContext.facts || [];
    var fb = resolveMockFallback();
    var violations = (liveValidation && liveValidation.violations) || [];
    var styleId = fb ? fb.pickFallbackStyle(persona, violations) : String(persona || "neutral") + "_recover";
    var speech = fb
      ? fb.buildAnchoredFallbackSpeech(facts, persona, styleId, bundleContext)
      : null;

    if (!speech) {
      return mockGenerate(params);
    }

    var out = wrapDialogueOutput(systemPrompt, groundedContext, speech, "mock_fallback");
    out.fallbackStyle = styleId;
    out.anchorValidation = {
      ok: out.anchorValidation && out.anchorValidation.ok,
      usedFallback: true,
      fallbackStyle: styleId,
      live: liveValidation || null,
    };
    return out;
  }

  function mockGenerate(params) {
    if (params && params.forceFallbackStyle) {
      var fb = resolveMockFallback();
      if (fb) {
        var facts = (params.groundedContext && params.groundedContext.facts) || [];
        var speech = fb.buildAnchoredFallbackSpeech(
          facts,
          params.persona,
          params.forceFallbackStyle,
          params.bundleContext || {}
        );
        var forced = wrapDialogueOutput(params.systemPrompt, params.groundedContext, speech, "mock");
        forced.fallbackStyle = params.forceFallbackStyle;
        return forced;
      }
    }

    var systemPrompt = params.systemPrompt;
    var groundedContext = params.groundedContext;
    var persona = params.persona;
    var bundleContext = params.bundleContext || {};
    var facts = groundedContext.facts || [];
    var personaStyles = {
      fearful_guard: "목소리를 낮추며 주변을 살피고",
      cynical_merchant: "의심 섞인 한숨과 함께",
      calm_scholar: "차분하게 사실을 정리하며",
      hotblood_hunter: "격앙된 어조로 주먹을 쥐고",
      witness: "담담한 목소리로",
    };
    var style = personaStyles[persona] || "신중한 어조로";
    var source = (groundedContext.dataOnly && groundedContext.dataOnly.source) || "unknown";
    var speechLines = facts.map(function (f) {
      return factToSpeechLine(f, persona);
    });
    var tension = bundleContext.hasContradiction ? " (다만 일부 정보는 서로 맞지 않아 보인다고 덧붙이며)" : "";
    var speech =
      facts.length > 1
        ? style + ' 말한다: "' + speechLines.join(" 그리고 ") + '"' + tension + " (출처: " + source + ")"
        : style + ' 말한다: "' + (speechLines[0] || "정보가 전달되지 않았다") + '"' + tension + " (출처: " + source + ")";
    return wrapDialogueOutput(systemPrompt, groundedContext, speech, "mock");
  }

  function fetchWithTimeout(url, options, timeoutMs) {
    return new Promise(function (resolve, reject) {
      var timer = setTimeout(function () {
        reject(new Error("LLM request timeout (" + timeoutMs + "ms)"));
      }, timeoutMs);
      global
        .fetch(url, options)
        .then(function (res) {
          clearTimeout(timer);
          resolve(res);
        })
        .catch(function (err) {
          clearTimeout(timer);
          reject(err);
        });
    });
  }

  async function callOpenAICompatible(systemPrompt, userMessage, runtime) {
    var base = runtime.baseUrl || "https://api.openai.com/v1/chat/completions";
    var res = await fetchWithTimeout(
      base,
      {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
          Authorization: "Bearer " + runtime.apiKey,
        },
        body: JSON.stringify({
          model: runtime.model,
          messages: [
            { role: "system", content: systemPrompt },
            { role: "user", content: userMessage },
          ],
          temperature: 0.65,
        }),
      },
      runtime.timeoutMs
    );
    if (!res.ok) {
      var errBody = await res.text();
      throw new Error("OpenAI-compatible API " + res.status + ": " + errBody.slice(0, 300));
    }
    var json = await res.json();
    var text = json.choices && json.choices[0] && json.choices[0].message && json.choices[0].message.content;
    if (!text) throw new Error("OpenAI-compatible API: empty choices");
    return String(text).trim();
  }

  async function callAnthropic(systemPrompt, userMessage, runtime) {
    var base = runtime.baseUrl || "https://api.anthropic.com/v1/messages";
    var res = await fetchWithTimeout(
      base,
      {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
          "x-api-key": runtime.apiKey,
          "anthropic-version": "2023-06-01",
        },
        body: JSON.stringify({
          model: runtime.model || "claude-3-5-haiku-20241022",
          max_tokens: 512,
          system: systemPrompt,
          messages: [{ role: "user", content: userMessage }],
        }),
      },
      runtime.timeoutMs
    );
    if (!res.ok) {
      var errBody = await res.text();
      throw new Error("Anthropic API " + res.status + ": " + errBody.slice(0, 300));
    }
    var json = await res.json();
    var block = json.content && json.content[0];
    if (!block || block.type !== "text") throw new Error("Anthropic API: unexpected content");
    return String(block.text).trim();
  }

  async function generateAsync(params) {
    var runtime = getConfigForRuntime();
    if (!isLive()) {
      return mockGenerate(params);
    }
    if (typeof global.fetch !== "function") {
      throw new Error("fetch is not available (use browser or Node 18+)");
    }
    var userMessage = buildUserMessage(params.groundedContext, params.bundleContext);
    var rawText;
    if (runtime.provider === "anthropic") {
      rawText = await callAnthropic(params.systemPrompt, userMessage, runtime);
    } else if (runtime.provider === "openai" || runtime.provider === "openai_compatible") {
      rawText = await callOpenAICompatible(params.systemPrompt, userMessage, runtime);
    } else {
      throw new Error("Unknown provider: " + runtime.provider);
    }
    var wrapped = wrapDialogueOutput(
      params.systemPrompt,
      params.groundedContext,
      rawText,
      runtime.provider,
      { minScore: params.minAnchorScore }
    );
    if (
      wrapped.anchorValidation &&
      !wrapped.anchorValidation.ok &&
      params.strictAnchor !== false
    ) {
      return mockGenerateFallback(params, wrapped.anchorValidation);
    }
    return wrapped;
  }

  function generateSync(params) {
    if (isLive()) {
      throw new Error("Live LLM is async-only. Call generateAsync() or disable useLive.");
    }
    return mockGenerate(params);
  }

  /**
   * After executeScenario — replace dialogue speech with live LLM when configured.
   */
  async function enrichDialogueResult(engineResult, persona) {
    if (!engineResult || !engineResult.dialogue || engineResult.propagation.blocked) {
      return engineResult;
    }
    if (!isLive()) return engineResult;
    var d = engineResult.dialogue;
    var out = await generateAsync({
      systemPrompt: d.systemPrompt,
      groundedContext: d.context,
      persona: persona || (engineResult.receiver && engineResult.receiver.persona),
      bundleContext: (d.context && d.context.bundleContext) || engineResult.bundleContext,
    });
    d.finalSpeech = out.finalSpeech;
    d.npcSpeech = out.npcSpeech;
    d.llmProvider = out.provider;
    d.fallbackStyle = out.fallbackStyle || null;
    d.anchorValidation = out.anchorValidation;
    engineResult.anchorValidation = out.anchorValidation;
    return engineResult;
  }

  global.LlmAdapter = {
    configure: configure,
    getConfig: getConfig,
    getConfigForRuntime: getConfigForRuntime,
    isLive: isLive,
    generateSync: generateSync,
    generateAsync: generateAsync,
    mockGenerate: mockGenerate,
    mockGenerateFallback: mockGenerateFallback,
    pickFallbackStyle: function (persona, violations) {
      var fb = resolveMockFallback();
      return fb ? fb.pickFallbackStyle(persona, violations) : "neutral_recover";
    },
    enrichDialogueResult: enrichDialogueResult,
    validateSpeech: function (speech, ctx, opts) {
      var anchor = resolveAnchor();
      return anchor ? anchor.validateSpeech(speech, ctx, opts) : { ok: true, skipped: true };
    },
    PROVIDERS: ["mock", "openai", "openai_compatible", "anthropic"],
  };
})(typeof window !== "undefined" ? window : globalThis);
