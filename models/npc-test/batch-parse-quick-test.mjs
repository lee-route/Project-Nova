/**
 * CI-friendly batch parse (small sample). Full QA: node batch-parse-test.mjs --count=2000
 */
import { spawnSync } from "child_process";
import path from "path";
import { fileURLToPath } from "url";

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const r = spawnSync(process.execPath, ["batch-parse-test.mjs", "--count=80", "--seed=42"], {
  cwd: __dirname,
  stdio: "inherit",
});
process.exit(r.status !== 0 ? 1 : 0);
