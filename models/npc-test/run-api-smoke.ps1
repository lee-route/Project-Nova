# API 서버가 떠 있는 상태에서 HTTP 연동 확인 (npm 불필요)
param([int]$Port = 8787, [string]$ListenHost = "127.0.0.1")

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

$node = (Get-Command node -ErrorAction SilentlyContinue).Source
if (-not $node) {
  $fallback = Join-Path $env:LOCALAPPDATA "Programs\cursor\resources\app\resources\helpers\node.exe"
  if (Test-Path $fallback) { $node = $fallback }
}
if (-not $node) {
  Write-Host "node.exe 없음. Node.js LTS 설치 필요." -ForegroundColor Red
  exit 1
}

$env:NPC_API_PORT = "$Port"
$env:NPC_API_HOST = $ListenHost
& $node api-smoke.mjs
