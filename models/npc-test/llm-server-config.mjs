/**
 * Server-side LLM config for npc-api-server.mjs
 * Priority: env vars > llm-config.local.json > defaults (mock)
 */
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";

const __dirname = path.dirname(fileURLToPath(import.meta.url));

function parseUseLive(value) {
  if (value === true || value === 1) return true;
  if (typeof value === "string") {
    const v = value.trim().toLowerCase();
    return v === "1" || v === "true" || v === "yes" || v === "on";
  }
  return false;
}

export function loadLlmServerConfig(rootDir) {
  const root = rootDir || __dirname;
  let fileCfg = {};
  const localPath = path.join(root, "llm-config.local.json");
  if (fs.existsSync(localPath)) {
    try {
      fileCfg = JSON.parse(fs.readFileSync(localPath, "utf8"));
    } catch (e) {
      console.warn("[llm-server-config] llm-config.local.json parse failed:", e.message);
    }
  }

  const env = process.env;
  const provider = env.NOVA_LLM_PROVIDER || fileCfg.provider || "mock";
  const apiKey = env.OPENAI_API_KEY || env.NOVA_LLM_API_KEY || fileCfg.apiKey || "";
  const model = env.NOVA_LLM_MODEL || fileCfg.model || "gpt-4o-mini";
  const baseUrl = env.OPENAI_BASE_URL || env.NOVA_LLM_BASE_URL || fileCfg.baseUrl || "";
  const useLive = parseUseLive(env.NOVA_LLM_USE_LIVE ?? fileCfg.useLive);
  const timeoutMs = Number(env.NOVA_LLM_TIMEOUT_MS || fileCfg.timeoutMs || 45000);

  return { provider, apiKey, model, baseUrl, useLive, timeoutMs };
}

export function llmConfigSummary(cfg) {
  return {
    provider: cfg.provider,
    model: cfg.model,
    useLive: cfg.useLive,
    hasApiKey: Boolean(cfg.apiKey),
    baseUrl: cfg.baseUrl || "(default)",
  };
}
