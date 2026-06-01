(function () {
  var STORAGE_KEY = "npc-test-llm-config";

  function loadJsonSync(url) {
    try {
      var xhr = new XMLHttpRequest();
      xhr.open("GET", url, false);
      xhr.send(null);
      if (xhr.status === 200 || xhr.status === 0) return JSON.parse(xhr.responseText);
    } catch (e) {}
    return null;
  }

  function loadFromStorage() {
    try {
      var raw = localStorage.getItem(STORAGE_KEY);
      if (raw) return JSON.parse(raw);
    } catch (e) {}
    return null;
  }

  function saveToStorage(cfg) {
    try {
      localStorage.setItem(STORAGE_KEY, JSON.stringify(cfg));
    } catch (e) {}
  }

  function bootstrapLlmConfig() {
    if (!window.LlmAdapter) return;
    var fromFile = loadJsonSync("./llm-config.local.json");
    var fromStore = loadFromStorage();
    var merged = Object.assign({}, fromFile || {}, fromStore || {});
    if (Object.keys(merged).length) {
      window.LlmAdapter.configure(merged);
    }
  }

  function bindLlmSettingsUi() {
    var form = document.getElementById("llm-settings-form");
    if (!form || !window.LlmAdapter) return;

    var provider = form.querySelector('[name="llmProvider"]');
    var model = form.querySelector('[name="llmModel"]');
    var apiKey = form.querySelector('[name="llmApiKey"]');
    var baseUrl = form.querySelector('[name="llmBaseUrl"]');
    var useLive = form.querySelector('[name="llmUseLive"]');
    var status = document.getElementById("llm-status");

    function refreshStatus() {
      var c = window.LlmAdapter.getConfig();
      if (status) {
        status.textContent =
          "provider=" +
          c.provider +
          ", live=" +
          (window.LlmAdapter.isLive() ? "ON" : "OFF (mock)") +
          ", key=" +
          (c.hasApiKey ? "set" : "missing");
      }
    }

    var stored = loadFromStorage();
    if (stored) {
      if (provider && stored.provider) provider.value = stored.provider;
      if (model && stored.model) model.value = stored.model;
      if (baseUrl && stored.baseUrl) baseUrl.value = stored.baseUrl;
      if (useLive) useLive.checked = Boolean(stored.useLive);
    }
    refreshStatus();

    form.addEventListener("submit", function (e) {
      e.preventDefault();
      var cfg = {
        provider: provider ? provider.value : "mock",
        model: model ? model.value : "gpt-4o-mini",
        apiKey: apiKey ? apiKey.value : "",
        baseUrl: baseUrl ? baseUrl.value : "",
        useLive: useLive ? useLive.checked : false,
      };
      if (!cfg.apiKey && loadFromStorage() && loadFromStorage().apiKey) {
        cfg.apiKey = loadFromStorage().apiKey;
      }
      window.LlmAdapter.configure(cfg);
      saveToStorage({
        provider: cfg.provider,
        model: cfg.model,
        apiKey: cfg.apiKey,
        baseUrl: cfg.baseUrl,
        useLive: cfg.useLive,
      });
      if (apiKey) apiKey.value = "";
      refreshStatus();
      if (typeof window.__appendInlineDebug === "function") {
        window.__appendInlineDebug("llm-settings saved, live=" + window.LlmAdapter.isLive());
      }
    });
  }

  bootstrapLlmConfig();
  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", bindLlmSettingsUi);
  } else {
    bindLlmSettingsUi();
  }

  window.NpcLlmSettings = { bootstrapLlmConfig: bootstrapLlmConfig, STORAGE_KEY: STORAGE_KEY };
})();
