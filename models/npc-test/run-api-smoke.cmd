@echo off
setlocal
cd /d "%~dp0"

set "NODE=C:\Program Files\nodejs\node.exe"
if not exist "%NODE%" (
  echo Node.js not found: %NODE%
  exit /b 1
)

set NPC_API_PORT=8787
set NPC_API_HOST=127.0.0.1
"%NODE%" api-smoke.mjs
