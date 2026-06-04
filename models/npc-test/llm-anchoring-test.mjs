/**
 * LLM fact anchoring validation
 */
import path from "path";
import { fileURLToPath } from "url";
import { loadTestEngine } from "./test-engine-bootstrap.mjs";

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const { LlmFactAnchor, LlmAdapter } = loadTestEngine(__dirname);

let failed = 0;
function assert(cond, msg) {
  if (!cond) {
    console.error("FAIL:", msg);
    failed += 1;
  } else {
    console.log("ok:", msg);
  }
}

const facts = [
  {
    subject: "마약초",
    action: "들어 있다",
    target: "안개 계곡",
    quantity: 12,
    certainty: 0.9,
    is_countable: true,
    action_type: "inventory",
  },
];
const ctx = { facts, dataOnly: { facts } };

const good = LlmFactAnchor.validateSpeech("마약초 열두 개가 안개 계곡에 들어 있다.", ctx);
assert(good.ok === true, "aligned speech passes");

const bad = LlmFactAnchor.validateSpeech("마약초 백 개가 산 너머에 있다.", ctx);
assert(bad.ok === false && bad.violations.length > 0, "wrong quantity fails");

const mock = LlmAdapter.mockGenerate({
  systemPrompt: "test",
  groundedContext: ctx,
  persona: "calm_scholar",
});
assert(mock.anchorValidation && mock.anchorValidation.ok !== false, "mock speech validates");

console.log(failed ? "FAILED" : "llm-anchoring-test passed");
process.exit(failed ? 1 : 0);
