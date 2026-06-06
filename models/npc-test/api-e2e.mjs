/**
 * Spawn API server, run live HTTP smoke, exit.
 * Usage: npm run api:e2e
 */
import { spawn } from "child_process";
import path from "path";
import { fileURLToPath } from "url";

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const port = 18787 + Math.floor(Math.random() * 1000);

const child = spawn(process.execPath, ["npc-api-server.mjs", "--port=" + port], {
  cwd: __dirname,
  stdio: ["ignore", "pipe", "pipe"],
  env: Object.assign({}, process.env, { NPC_API_PORT: String(port) }),
});

let ready = false;
child.stdout.on("data", function (buf) {
  const t = buf.toString();
  process.stdout.write(t);
  if (t.indexOf("listening") >= 0) ready = true;
});

await new Promise(function (resolve, reject) {
  const t = setTimeout(function () {
    reject(new Error("server start timeout"));
  }, 15000);
  const iv = setInterval(function () {
    if (ready) {
      clearTimeout(t);
      clearInterval(iv);
      setTimeout(resolve, 300);
    }
  }, 100);
});

const smoke = spawn(process.execPath, ["api-smoke.mjs"], {
  cwd: __dirname,
  stdio: "inherit",
  env: Object.assign({}, process.env, { NPC_API_PORT: String(port) }),
});

smoke.on("close", function (code) {
  child.kill();
  process.exit(code || 0);
});
