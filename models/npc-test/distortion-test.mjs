/**
 * Node runner for distortion regression tests.
 * Usage: node distortion-test.mjs
 */
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";
import vm from "vm";

const __dirname = path.dirname(fileURLToPath(import.meta.url));

function loadEngine() {
  const sandbox = {
    window: {},
    document: { querySelector: () => null, querySelectorAll: () => [], getElementById: () => null },
    console,
    localStorage: { getItem: () => null, setItem: () => {}, removeItem: () => {} },
  };
  sandbox.window = sandbox;
  const ctx = vm.createContext(sandbox);
  const read = (f) => fs.readFileSync(path.join(__dirname, f), "utf8");
  for (const f of ["dictionaries.js", "npc-parser.js", "quest-system.js"]) {
    vm.runInContext(read(f), ctx, { filename: f });
  }
  const player = JSON.parse(read("player-profile.json"));
  sandbox.window.QuestSystem.registerProfiles({
    npcs: JSON.parse(read("npcs.json")).npcs,
    player: player.player,
  });
  return { parser: sandbox.window.NpcParser, engine: sandbox.window.QuestSystem };
}

function includesAll(rules, expected) {
  if (!expected?.length) return true;
  return expected.every((r) => rules.includes(r));
}

function excludesAll(rules, excluded) {
  if (!excluded?.length) return true;
  return excluded.every((r) => !rules.includes(r));
}

function collectRules(result) {
  const rules = [];
  for (const item of result.auditTrail || []) {
    for (const r of item.applied_rules || []) {
      if (!rules.includes(r)) rules.push(r);
    }
  }
  return rules;
}

function maxQuantity(result) {
  return Math.max(
    0,
    ...(result.propagation.interpretedFacts || []).map((x) => Number(x.truth_value.quantity || 0))
  );
}

function evaluateCase(testCase, parser, engine) {
  const expect = testCase.expect || {};
  if (testCase.custom === "kb_merge") {
    const kb = new engine.KnowledgeBase();
    const atom = {
      info_id: "F01",
      truth_value: {
        subject: "마약초",
        action: "들어 있다",
        target: "안개 계곡",
        quantity: 12,
        certainty: 0.7,
        is_countable: true,
        action_type: "unknown",
      },
      metadata: { applied_rules: [], bundle_notes: [], rumor: false },
    };
    kb.update(atom);
    const atom2 = JSON.parse(JSON.stringify(atom));
    atom2.truth_value.quantity = 15;
    atom2.truth_value.certainty = 0.9;
    const mergeRes = kb.update(atom2);
    const list = kb.list();
    if (mergeRes.action !== "merge" || list.length !== 1 || list[0].truth_value.quantity < 15) {
      return { ok: false, reason: "kb merge failed" };
    }
    return { ok: true, reason: "" };
  }

  const opts = testCase.options || {};
  let facts = null;
  if (testCase.input) {
    const parsed = parser.parseScenarioText(testCase.input);
    facts = parsed.facts?.length ? parsed.facts : parser.buildFactsFromParsed(parsed);
  }

  const usePlayer = opts.usePlayerAsSender !== false && !opts.senderProfileKey;
  const result = engine.executeScenario({
    facts,
    trustLevel: opts.trustLevel ?? 0.74,
    allowPartialTrust: opts.allowPartialTrust !== false,
    quantityMode: opts.quantityMode || "dramatic",
    usePlayerAsSender: usePlayer,
    receiverProfileKey: opts.receiverProfileKey || "scholar_alric",
    senderProfileKey: opts.senderProfileKey,
    senderStats: { fear: opts.senderFear ?? 0.5, hostility: opts.senderHostility ?? 0.5, trust: 0.55 },
    receiverStats: { credulity: opts.receiverCredulity ?? 0.7, trust: 0.65 },
    senderReputation: opts.senderReputation || { 밀수: 0.5, 화물: 0.5, 마약초: 0.5 },
    receiverReputation: opts.receiverReputation || { 밀수: 0.55, 화물: 0.6, 마약초: 0.7 },
  });

  if (Boolean(expect.blocked) !== Boolean(result.propagation.blocked)) {
    return { ok: false, reason: "blocked mismatch" };
  }
  if (expect.blocked) return { ok: true, reason: "" };

  if (expect.factRumor && facts?.[0] && !facts[0].rumor) {
    return { ok: false, reason: "fact.rumor not set" };
  }

  const rules = collectRules(result);
  if (!includesAll(rules, expect.rulesInclude)) {
    return { ok: false, reason: `rules missing ${JSON.stringify(expect.rulesInclude)} got ${JSON.stringify(rules)}` };
  }
  if (!excludesAll(rules, expect.rulesExclude)) {
    return { ok: false, reason: `forbidden rules ${JSON.stringify(expect.rulesExclude)}` };
  }
  if (expect.minQuantity != null && maxQuantity(result) < expect.minQuantity) {
    return { ok: false, reason: `qty low ${maxQuantity(result)}` };
  }
  if (expect.maxQuantity != null && maxQuantity(result) > expect.maxQuantity) {
    return { ok: false, reason: `qty high ${maxQuantity(result)}` };
  }
  if (expect.partialTrust && !result.propagation.partialTrust) {
    return { ok: false, reason: "no partialTrust" };
  }
  if (expect.bundleContradiction && !result.bundleContext?.hasContradiction) {
    return { ok: false, reason: "no bundle contradiction" };
  }
  if (expect.factsMin != null && (result.propagation.interpretedFacts || []).length < expect.factsMin) {
    return { ok: false, reason: "factsMin" };
  }
  if (expect.auditDiffMin != null && (result.auditDiff || []).length < expect.auditDiffMin) {
    return { ok: false, reason: "auditDiffMin" };
  }
  return { ok: true, reason: "" };
}

function main() {
  const { parser, engine } = loadEngine();
  const suite = JSON.parse(fs.readFileSync(path.join(__dirname, "distortion-test-cases.json"), "utf8"));
  let pass = 0;
  let fail = 0;
  for (const c of suite.cases) {
    const res = evaluateCase(c, parser, engine);
    if (res.ok) {
      pass += 1;
      console.log("[PASS]", c.id);
    } else {
      fail += 1;
      console.log("[FAIL]", c.id, "->", res.reason);
    }
  }
  console.log(`\nsummary: ${pass} passed, ${fail} failed`);
  process.exit(fail ? 1 : 0);
}

main();
