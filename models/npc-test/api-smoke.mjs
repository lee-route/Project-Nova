/**
 * Live HTTP smoke test — 서버가 떠 있어야 함.
 * Usage:
 *   Terminal A: npm run api
 *   Terminal B: npm run api:smoke
 * Or one-shot: npm run api:e2e
 */
import http from "http";

const port = Number(process.env.NPC_API_PORT || 8787);
const host = process.env.NPC_API_HOST || "127.0.0.1";
const base = "http://" + host + ":" + port;

function request(method, path, body) {
  return new Promise(function (resolve, reject) {
    const payload = body ? JSON.stringify(body) : "";
    const req = http.request(
      base + path,
      { method: method, headers: { "Content-Type": "application/json", "Content-Length": Buffer.byteLength(payload) } },
      function (res) {
        let data = "";
        res.on("data", function (c) { data += c; });
        res.on("end", function () {
          try {
            resolve({ status: res.statusCode, json: JSON.parse(data) });
          } catch (e) {
            reject(new Error("invalid json: " + data.slice(0, 200)));
          }
        });
      }
    );
    req.on("error", reject);
    if (payload) req.write(payload);
    req.end();
  });
}

let failed = 0;
function assert(cond, msg) {
  if (!cond) {
    console.error("FAIL:", msg);
    failed += 1;
  } else {
    console.log("ok:", msg);
  }
}

console.log("Smoke against " + base);

const health = await request("GET", "/health");
assert(health.json.ok, "health");

const meta = await request("GET", "/v1/quest/meta");
assert(meta.json.ok && meta.json.givers && meta.json.givers.length >= 3, "quest meta givers");

const turnIn = await request("POST", "/v1/quest/turn-in", {
  questId: "quest_abandoned_cargo",
  giverId: "guard_timid",
  scenarioText: "안개 계곡에 밀수 화물이 버려져 있다. 마약초 열두 개가 들어 있다",
  sessionKey: "smoke-session",
});
assert(turnIn.json.ok, "turn-in");
assert(turnIn.json.outcomeBranch && turnIn.json.outcomeBranch.branchId, "branch: " + turnIn.json.outcomeBranch.branchId);
assert(turnIn.json.outcome && turnIn.json.outcome.rewards, "gold=" + (turnIn.json.outcome.rewards && turnIn.json.outcome.rewards.gold));

console.log(failed ? "\nFAILED" : "\napi-smoke passed");
process.exit(failed ? 1 : 0);
