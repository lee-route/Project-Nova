# 현재 PowerShell 세션만 PATH 갱신 (터미널 재시작 대신)
$env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" +
  [System.Environment]::GetEnvironmentVariable("Path", "User")
Write-Host "PATH refreshed."
Write-Host "node: $(Get-Command node -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source)"
Write-Host "npm:  $(Get-Command npm -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source)"
if (Get-Command node -ErrorAction SilentlyContinue) { node -v }
if (Get-Command npm -ErrorAction SilentlyContinue) { npm -v }
