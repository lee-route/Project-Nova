@echo off
REM PATH/npm 없이 API 서버 기동 (Node LTS 기본 설치 경로)
setlocal
cd /d "%~dp0"

set "NODE=C:\Program Files\nodejs\node.exe"
if not exist "%NODE%" (
  echo Node.js not found: %NODE%
  echo Install LTS from https://nodejs.org then run this again.
  echo Or reopen terminal after install.
  exit /b 1
)

echo Using: %NODE%
echo API: http://127.0.0.1:8787
echo.
"%NODE%" npc-api-server.mjs --port=8787 --host=127.0.0.1
