/**
 * Usage: node integration-test.mjs
 */
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";
import vm from "vm";

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const QUEST_ID = "quest_abandoned_cargo";

function load() {
  const sandbox = {
    window: {},
    document: { querySelector: () => null, querySelectorAll: () => [], getElementById: () => null },
    console,
    localStorage: { getItem: () => null, setItem: () => {}, removeItem: () => {} },
  };
  sandbox.window = sandbox;
  const ctx = vm.createContext(sandbox);
  const read = (f) => fs.readFileSync(path.join(__dirname, f), "utf8");
  for (const f of ["dictionaries.js", "quest-system.js", "reputation-system.js"]) {
    vm.runInContext(read(f), ctx, { filename: f });
  }
  const player = JSON.parse(read("player-profile.json"));
  sandbox.window.QuestSystem.registerProfiles({
    npcs: JSON.parse(read("npcs.json")).npcs,
    player: player.player,
  });
  vm.runInContext(read("app.js"), ctx, { filename: "app.js" });
  vm.runInContext(read("quest-runtime.js"), ctx, { filename: "quest-runtime.js" });
  sandbox.window.QuestRuntime.setQuestCatalog(JSON.parse(read("quests-draft.json")));
  return {
    parser: sandbox.window.NpcParser,
    engine: sandbox.window.QuestSystem,
    runtime: sandbox.window.QuestRuntime,
  };
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

function herbQuantity(facts) {
  const herb = (facts || []).find((f) => String(f.subject || "").includes("마약초"));
  return herb ? Number(herb.quantity) : null;
}

function scenarioOpts(testCase) {
  const opts = testCase.options || {};
  const usePlayer = opts.usePlayerAsSender !== false && !opts.senderProfileKey;
  return {
    quantityMode: opts.quantityMode || "dramatic",
    usePlayerAsSender: usePlayer,
    receiverProfileKey: opts.receiverProfileKey || "scholar_alric",
    senderProfileKey: opts.senderProfileKey,
    senderStats: {
      fear: opts.senderFear ?? 0.5,
      hostility: opts.senderHostility ?? 0.5,
      trust: 0.55,
    },
    receiverStats: { credulity: 0.7, trust: 0.65 },
    trustLevel: opts.trustLevel,
    allowPartialTrust: opts.allowPartialTrust,
  };
}

function evaluateCase(testCase, parser, engine, runtime) {
  const expect = testCase.expect || {};
  const opts = testCase.options || {};

  if (testCase.custom === "chain") {
    engine.clearSession("itest-chain");
    const facts = parser.buildFactsFromParsed(parser.parseScenarioText(testCase.input));
    const base = engine.getScenario({
      sessionKey: "itest-chain",
      ...scenarioOpts(testCase),
    });
    const chain = engine.propagateChain(
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
    const so = scenarioOpts(testCase);
    const facts1 = parser.buildFactsFromParsed(parser.parseScenarioText(testCase.input));
    const r1 = engine.executeScenario({
      facts: facts1,
      persistSession: true,
      sessionKey: "itest-session",
      quantityMode: "faithful",
      ...so,
    });
    const secondText = testCase.inputSecond || testCase.input;
    const facts2 = parser.buildFactsFromParsed(parser.parseScenarioText(secondText));
    const r2 = engine.executeScenario({
      facts: facts2,
      persistSession: true,
      sessionKey: "itest-session",
      quantityMode: "faithful",
      currentTick: 1005,
      ...so,
    });
    if (expect.kbGrowth && r2.knowledgeBaseSnapshot.length <= r1.knowledgeBaseSnapshot.length) {
      return { ok: false, reason: "KB did not grow/merge" };
    }
    return { ok: true, reason: "" };
  }

  if (testCase.custom === "kbConflict") {
    engine.clearSession("itest-kbconflict");
    const threatFacts = [
      {
        fact_id: "F01",
        subject: "밀수 화물",
        action: "버려져 있다",
        target: "안개 계곡",
        quantity: 12,
        certainty: 0.7,
        is_countable: true,
        action_type: "tactical_move",
        parse_mode: "structured",
        parse_confidence: 0.9,
      },
    ];
    const calmFacts = [
      {
        fact_id: "F02",
        subject: "안개 계곡",
        action: "태연한 상태다",
        target: "안개 계곡",
        quantity: 1,
        certainty: 0.6,
        is_countable: false,
        action_type: "state",
        parse_mode: "structured",
        parse_confidence: 0.9,
      },
    ];
    const r1 = engine.executeScenario({
      facts: threatFacts,
      persistSession: true,
      sessionKey: "itest-kbconflict",
      quantityMode: "faithful",
      currentTick: 1000,
      receiverProfileKey: "scholar_alric",
      senderStats: { fear: 0.9, hostility: 0.2, trust: 0.55 },
      receiverStats: { credulity: 0.7, trust: 0.65 },
      allowPartialTrust: false,
    });
    const threat1 = (r1.knowledgeBaseSnapshot || []).find(
      (x) =>
        String(x.truth_value.action_type || "").toLowerCase() === "tactical_move" &&
        String(x.truth_value.target || "").includes("안개 계곡")
    );
    const c1 = threat1 ? Number(threat1.truth_value.certainty || 0) : 0;

    const r2 = engine.executeScenario({
      facts: calmFacts,
      persistSession: true,
      sessionKey: "itest-kbconflict",
      quantityMode: "faithful",
      currentTick: 1001,
      receiverProfileKey: "scholar_alric",
      senderStats: { fear: 0.1, hostility: 0.2, trust: 0.55 },
      receiverStats: { credulity: 0.8, trust: 0.65 },
      allowPartialTrust: false,
    });
    const threat2 = (r2.knowledgeBaseSnapshot || []).find(
      (x) =>
        String(x.truth_value.action_type || "").toLowerCase() === "tactical_move" &&
        String(x.truth_value.target || "").includes("안개 계곡")
    );
    const c2 = threat2 ? Number(threat2.truth_value.certainty || 0) : 0;

    if (expect.threatCertaintyDecreased && !(c2 < c1)) {
      return { ok: false, reason: `threat certainty did not decrease: ${c1} -> ${c2}` };
    }
    return { ok: true, reason: "" };
  }

  if (testCase.custom === "questTurnIn") {
    const run = runtime.runQuestTurnIn({
      questId: QUEST_ID,
      giverId: opts.giverId || "guard_timid",
      scenarioText: testCase.input,
      engine,
      parser,
      sessionKey: "itest-quest-" + testCase.id,
      reputationSessionKey: "itest-quest-" + testCase.id,
    });
    if (!run.completion.completed) {
      return { ok: false, reason: "quest not completed: " + (run.completion.reasons || []).join(", ") };
    }
    const branch = run.outcomeBranch?.branchId;
    if (opts.expectBranch && branch !== opts.expectBranch) {
      return { ok: false, reason: `branch ${branch} != ${opts.expectBranch}` };
    }
    return { ok: true, reason: "" };
  }

  const parsed = parser.parseScenarioText(testCase.input);
  const facts = parsed.facts?.length ? parsed.facts : parser.buildFactsFromParsed(parsed);

  if (expect.quantity != null) {
    const q = herbQuantity(facts);
    if (q !== expect.quantity) return { ok: false, reason: `qty ${q} != ${expect.quantity}` };
  }
  if (expect.is_countable != null) {
    const herb = (facts || []).find((f) => String(f.subject || "").includes("마약초"));
    if (herb && Boolean(herb.is_countable) !== expect.is_countable) {
      return { ok: false, reason: "countable mismatch" };
    }
  }

  const result = engine.executeScenario({
    facts,
    groundTruth: testCase.groundTruth || null,
    ...scenarioOpts(testCase),
  });

  if (expect.auditDiffMin != null && (result.auditDiff || []).length < expect.auditDiffMin) {
    return { ok: false, reason: "auditDiff missing" };
  }
  if (expect.hasFields?.length && result.auditDiff?.[0]) {
    for (const field of expect.hasFields) {
      if (result.auditDiff[0][field] == null) return { ok: false, reason: "audit field " + field };
    }
  }

  const rules = collectRules(result);
  if (expect.rulesInclude) {
    for (const r of expect.rulesInclude) {
      if (!rules.includes(r)) return { ok: false, reason: "missing rule " + r };
    }
  }

  if (expect.groundTruthScoreMin != null) {
    const score = result.groundTruthReport?.score ?? 0;
    if (score < expect.groundTruthScoreMin) {
      return { ok: false, reason: "ground truth score " + score };
    }
  }

  return { ok: true, reason: "" };
}

function main() {
  const { parser, engine, runtime } = load();
  const suite = JSON.parse(fs.readFileSync(path.join(__dirname, "integration-test-cases.json"), "utf8"));
  let pass = 0;
  let fail = 0;
  for (const c of suite.cases) {
    const res = evaluateCase(c, parser, engine, runtime);
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
