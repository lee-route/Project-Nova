/**
 * Node batch parser QA: generates sentences, runs NpcParser, flags anomalies.
 * Usage: node batch-parse-test.mjs [--count=2000] [--seed=42]
 */
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";
import vm from "vm";

const __dirname = path.dirname(fileURLToPath(import.meta.url));

function loadParser() {
  const sandbox = {
    window: {},
    document: {
      querySelector: () => null,
      querySelectorAll: () => [],
      getElementById: () => null,
    },
    console,
    localStorage: {
      getItem: () => null,
      setItem: () => {},
      removeItem: () => {},
    },
  };
  sandbox.window = sandbox;
  const ctx = vm.createContext(sandbox);
  const dictSrc = fs.readFileSync(path.join(__dirname, "dictionaries.js"), "utf8");
  const appSrc = fs.readFileSync(path.join(__dirname, "app.js"), "utf8");
  vm.runInContext(dictSrc, ctx, { filename: "dictionaries.js" });
  vm.runInContext(appSrc, ctx, { filename: "app.js" });
  if (!sandbox.window.NpcParser) {
    throw new Error("NpcParser not exported after app.js load");
  }
  const parser = sandbox.window.NpcParser;
  if (!parser) throw new Error("NpcParser not exported");
  return parser;
}

function mulberry32(seed) {
  let t = seed >>> 0;
  return function () {
    t += 0x6d2b79f5;
    let r = Math.imul(t ^ (t >>> 15), 1 | t);
    r ^= r + Math.imul(r ^ (r >>> 7), 61 | r);
    return ((r ^ (r >>> 14)) >>> 0) / 4294967296;
  };
}

function pick(rng, arr) {
  return arr[Math.floor(rng() * arr.length)];
}

function pickN(rng, arr, n) {
  const out = [];
  for (let i = 0; i < n; i += 1) out.push(pick(rng, arr));
  return out;
}

function generateSentences(count, seed) {
  const rng = mulberry32(seed);
  const places = [
    "안개 계곡", "마을 정문", "시장", "학자 서재", "동물원", "북문", "남문", "마을", "궁정",
    "주막", "대장간", "약초원", "숲", "동굴", "항구", "성문", "탑", "지하실", "농장", "연못", "교량",
  ];
  const subjects = [
    "밀수 화물", "마약초", "도적", "밀수업자", "늑대", "정찰병", "경비", "촌장", "원장", "상인", "약사", "대장장이",
    "사냥꾼", "학생", "귀족", "병사", "죄수", "박쥐", "늑대 무리", "도적 무리",
    "상점 주인", "조력자", "사절", "적병", "백성", "주민들",
  ];
  const counts = ["1", "2", "3", "5", "7", "한", "두", "세", "다섯", "열", "스무", "서른"];
  const units = ["마리", "명", "개", "척", ""];
  const actions = [
    "이동했다", "탈출했다", "도주했다", "공격했다", "순찰했다", "정찰했다",
    "집결했다", "매복했다", "대기 중이다", "밥을 먹었다", "장사를 했다",
    "회의를 했다", "협상을 했다", "보고했다", "발견했다", "독살당했다",
    "살해되었다", "태연하다", "피곤해 보인다", "바빠 보인다", "기침을 한다",
    "공격 징후가 없었다", "이상이 없다", "불을 질렀다", "납치당했다",
  ];
  const connectors = ["", "는데", "지만", "고", "며"];
  const prefixes = ["", "오늘은 ", "어제 ", "방금 ", "아마 ", "소문에 따르면 "];
  const templates = [
    (p, s, c, u, a, prefix) => `${prefix}${p}에서 ${s} ${c}${u}가 ${a}`,
    (p, s, c, u, a, prefix) => `${prefix}${s} ${c}${u}가 ${p}으로 ${a}`,
    (p, s, c, u, a, prefix) => `${prefix}${s}가 ${p}에서 ${a}`,
    (p, s, c, u, a, prefix) => `${prefix}${p} ${s}는 ${a}`,
    (p, s, c, u, a, prefix) => `${prefix}${s}는 ${a}`,
    (p, s, c, u, a, prefix) => `${prefix}${p}에서 ${s}가 ${a}`,
    (p, s, c, u, a, prefix) => `${prefix}${s} ${c}${u}가 ${a}`,
    (p, s, c, u, a, prefix) => `${prefix}${s}가 ${p}에서 ${a}`,
  ];

  const sentences = new Set();
  const existing = path.join(__dirname, "test-cases.json");
  if (fs.existsSync(existing)) {
    const suite = JSON.parse(fs.readFileSync(existing, "utf8"));
    for (const c of suite.cases || []) sentences.add(c.input);
  }

  let guard = 0;
  while (sentences.size < count && guard < count * 20) {
    guard += 1;
    const p = pick(rng, places);
    const s = pick(rng, subjects);
    const c = pick(rng, counts);
    const u = pick(rng, units);
    const a = pick(rng, actions);
    const prefix = pick(rng, prefixes);
    const tpl = pick(rng, templates);
    let line = tpl(p, s, c, u, a, prefix);
    if (rng() < 0.35) {
      const p2 = pick(rng, places);
      const s2 = pick(rng, subjects);
      const a2 = pick(rng, actions);
      const conn = pick(rng, connectors);
      line = `${line.replace(/[.!?]$/, "")}${conn}, ${p2} ${s2}는 ${a2}.`;
    }
    sentences.add(line);
  }

  return [...sentences].slice(0, count);
}

const OBSERVATION_SUBJECTS = new Set(["공격 징후", "이상 징후"]);
const SHORT_SUBJECT_OK = new Set(["적"]);

function looksLikeClause(text) {
  const t = String(text || "");
  if (!t) return false;
  if (OBSERVATION_SUBJECTS.has(t)) return false;
  if (/(했|한다|했다|당했|당했다|중이다|상태다|보인다|빠졌|시도했|계획|징후|미발생)/.test(t)) return true;
  if (/\s+(고|며|는데|지만)\s+/.test(t)) return true;
  return false;
}

function analyzeParse(input, parsed, facts) {
  const issues = [];
  const primary = parsed;

  if (!parsed) {
    issues.push({ code: "NULL_PARSE", detail: "parse returned null" });
    return issues;
  }

  if (!parsed.subject || (parsed.subject.length < 2 && !SHORT_SUBJECT_OK.has(parsed.subject))) {
    issues.push({ code: "SHORT_SUBJECT", detail: parsed.subject });
  }
  if (parsed.subject && parsed.subject.length === 1 && !SHORT_SUBJECT_OK.has(parsed.subject)) {
    issues.push({ code: "ONE_CHAR_SUBJECT", detail: parsed.subject });
  }
  if (looksLikeClause(parsed.subject)) {
    issues.push({ code: "CLAUSE_AS_SUBJECT", detail: parsed.subject });
  }
  if (looksLikeClause(parsed.target)) {
    issues.push({ code: "CLAUSE_AS_TARGET", detail: parsed.target });
  }
  if (parsed.target === parsed.subject) {
    issues.push({ code: "TARGET_EQ_SUBJECT", detail: parsed.target });
  }
  if (parsed.action && parsed.action.length > 80) {
    issues.push({ code: "ACTION_TOO_LONG", detail: parsed.action.slice(0, 40) + "..." });
  }
  if (parsed.parse_mode === "structured" && parsed.action_type === "unknown" && parsed.parse_confidence > 0.7) {
    issues.push({ code: "HIGH_CONF_UNKNOWN_TYPE", detail: String(parsed.parse_confidence) });
  }
  if (parsed.is_countable && (!parsed.quantity || parsed.quantity < 1 || parsed.quantity > 100)) {
    issues.push({ code: "BAD_QUANTITY", detail: String(parsed.quantity) });
  }
  if (/^(마|을|를|이|가|은|는)$/.test(parsed.subject || "")) {
    issues.push({ code: "PARTICLE_SUBJECT", detail: parsed.subject });
  }
  if (/(따르|소문|카더라|아마|오늘|어제|방금|지금)/.test(parsed.subject || "")) {
    issues.push({ code: "PREFIX_LEAK_SUBJECT", detail: parsed.subject });
  }
  if (/(따르|아마|소문)/.test(parsed.target || "")) {
    issues.push({ code: "PREFIX_LEAK_TARGET", detail: parsed.target });
  }
  if (parsed.subject && /에서/.test(parsed.subject) && !/(공격 징후|이상 징후)/.test(parsed.subject)) {
    issues.push({ code: "PLACE_IN_SUBJECT", detail: parsed.subject });
  }
  if (parsed.action && /(불을 질렀|질렀다)/.test(parsed.action) && parsed.target === "일상 관찰") {
    issues.push({ code: "THREAT_WRONG_TARGET", detail: parsed.target });
  }
  if (/살해당|독살당|암살당/.test(parsed.action || "") && parsed.action.length < 4) {
    issues.push({ code: "TRUNCATED_THREAT_ACTION", detail: parsed.action });
  }

  for (let i = 0; i < (facts || []).length; i += 1) {
    const f = facts[i];
    if (!f.subject || (f.subject.length < 2 && !SHORT_SUBJECT_OK.has(f.subject))) {
      issues.push({ code: "FACT_SHORT_SUBJECT", detail: `F${i + 1}:${f.subject}` });
    }
    if (looksLikeClause(f.subject)) {
      issues.push({ code: "FACT_CLAUSE_SUBJECT", detail: `F${i + 1}:${f.subject}` });
    }
    if (looksLikeClause(f.target)) {
      issues.push({ code: "FACT_CLAUSE_TARGET", detail: `F${i + 1}:${f.target}` });
    }
  }

  const sentCount = parsed.sentenceParses ? parsed.sentenceParses.length : 1;
  if (sentCount > 1 && (!facts || facts.length < sentCount)) {
    issues.push({ code: "FACTS_LT_SENTENCES", detail: `${facts?.length}/${sentCount}` });
  }

  return issues;
}

function main() {
  const args = process.argv.slice(2);
  let count = 2000;
  let seed = 42;
  for (const arg of args) {
    if (arg.startsWith("--count=")) count = Number(arg.split("=")[1]) || 2000;
    if (arg.startsWith("--seed=")) seed = Number(arg.split("=")[1]) || 42;
  }

  console.log(`Loading parser...`);
  const parser = loadParser();
  console.log(`Generating ${count} sentences (seed=${seed})...`);
  const sentences = generateSentences(count, seed);

  const issueCounts = new Map();
  const samples = new Map();
  let crashCount = 0;
  let issueSentenceCount = 0;

  for (let i = 0; i < sentences.length; i += 1) {
    const input = sentences[i];
    try {
      const parsed = parser.parseScenarioText(input);
      const facts = parsed.facts?.length ? parsed.facts : parser.buildFactsFromParsed(parsed);
      const issues = analyzeParse(input, parsed, facts);
      if (issues.length) {
        issueSentenceCount += 1;
        for (const issue of issues) {
          issueCounts.set(issue.code, (issueCounts.get(issue.code) || 0) + 1);
          if (!samples.has(issue.code)) samples.set(issue.code, []);
          const arr = samples.get(issue.code);
          if (arr.length < 8) {
            arr.push({ input, detail: issue.detail, parsed: {
              subject: parsed.subject,
              action: (parsed.action || "").slice(0, 60),
              target: parsed.target,
              action_type: parsed.action_type,
              facts: facts.length,
            }});
          }
        }
      }
    } catch (err) {
      crashCount += 1;
      const code = "CRASH";
      issueCounts.set(code, (issueCounts.get(code) || 0) + 1);
      if (!samples.has(code)) samples.set(code, []);
      const arr = samples.get(code);
      if (arr.length < 5) arr.push({ input, detail: err.message });
    }
  }

  const report = {
    total: sentences.length,
    issueSentenceCount,
    crashCount,
    issueCounts: Object.fromEntries([...issueCounts.entries()].sort((a, b) => b[1] - a[1])),
    samples: Object.fromEntries(samples),
  };

  const outPath = path.join(__dirname, "batch-report.json");
  fs.writeFileSync(outPath, JSON.stringify(report, null, 2), "utf8");

  console.log(`\n=== Batch Parse Report ===`);
  console.log(`Total: ${report.total}`);
  console.log(`Sentences with issues: ${issueSentenceCount} (${((issueSentenceCount / report.total) * 100).toFixed(1)}%)`);
  console.log(`Crashes: ${crashCount}`);
  console.log(`\nIssue breakdown:`);
  for (const [code, n] of [...issueCounts.entries()].sort((a, b) => b[1] - a[1])) {
    console.log(`  ${code}: ${n}`);
  }
  console.log(`\nFull report: ${outPath}`);
}

main();
