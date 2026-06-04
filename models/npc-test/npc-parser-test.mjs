/**
 * Smoke test: npc-parser.js loads without app.js / DOM UI
 */
import path from "path";
import { fileURLToPath } from "url";
import vm from "vm";
import fs from "fs";

const __dirname = path.dirname(fileURLToPath(import.meta.url));

const sandbox = { window: {}, console };
sandbox.window = sandbox;
const ctx = vm.createContext(sandbox);
const read = (f) => fs.readFileSync(path.join(__dirname, f), "utf8");
vm.runInContext(read("dictionaries.js"), ctx, { filename: "dictionaries.js" });
vm.runInContext(read("npc-parser.js"), ctx, { filename: "npc-parser.js" });

const parser = sandbox.window.NpcParser;
let failed = 0;
function assert(cond, msg) {
  if (!cond) {
    console.error("FAIL:", msg);
    failed += 1;
  } else {
    console.log("ok:", msg);
  }
}

assert(parser && parser.parseScenarioText, "NpcParser exported");
const text = "안개 계곡에 밀수 화물이 버려져 있다. 마약초 열두 개가 들어 있다";
const parsed = parser.parseScenarioText(text);
const facts = parser.buildFactsFromParsed(parsed);
assert(facts.length >= 2, "multi-sentence facts");
assert(
  facts.some((f) => String(f.subject).indexOf("마약초") >= 0 && f.quantity === 12),
  "herb quantity 12"
);

console.log(failed ? "FAILED" : "npc-parser-test passed");
process.exit(failed ? 1 : 0);
