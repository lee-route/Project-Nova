# npm 없이 API 서버 기동 (Windows)
# Usage: .\run-api.ps1
#        .\run-api.ps1 -Port 8787 -Host 0.0.0.0

param(
  [int]$Port = 8787,
  [string]$ListenHost = "127.0.0.1"
)

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

function Find-NodeExe {
  # 설치 직후 예전 터미널은 PATH 미갱신 → Program Files 우선
  $candidates = @(
    "$env:ProgramFiles\nodejs\node.exe",
    "$env:LOCALAPPDATA\Programs\node\node.exe"
  )
  foreach ($p in $candidates) {
    if (Test-Path $p) { return $p }
  }

  $cmd = Get-Command node -ErrorAction SilentlyContinue
  if ($cmd) { return $cmd.Source }
  $cursorRoot = Join-Path $env:LOCALAPPDATA "Programs\cursor\resources\app\resources\helpers\node.exe"
  $candidates += $cursorRoot

  return $null
}

function Refresh-PathEnv {
  $env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" +
    [System.Environment]::GetEnvironmentVariable("Path", "User")
}

Refresh-PathEnv
$node = Find-NodeExe
if (-not $node) {
  Write-Host ""
  Write-Host "node.exe를 찾을 수 없습니다." -ForegroundColor Red
  Write-Host "  1) https://nodejs.org 에서 LTS 설치 후 터미널을 다시 열기"
  Write-Host "  2) 설치 후: node npc-api-server.mjs"
  Write-Host ""
  exit 1
}

Write-Host "Using: $node"
Write-Host "API: http://${ListenHost}:${Port}"
Write-Host ""

& $node npc-api-server.mjs "--port=$Port" "--host=$ListenHost"
