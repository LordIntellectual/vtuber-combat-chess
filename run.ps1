# Launch vTuber Combat Chess (Windows)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Bin = Join-Path $Root "local\bin"
$Exe = Join-Path $Bin "VTuberCombatChess.exe"
if (-not (Test-Path $Exe)) {
  Write-Error "Build first: .\install_and_build.ps1"
}
# Bullet / libpng DLLs if shared builds were used
$env:PATH = "$Bin;$Root\deps\local\bin;$Root\deps\local\lib;" + $env:PATH
if (-not $env:VCC_STOCKFISH) {
  $sf = Join-Path $Bin "stockfish.exe"
  if (Test-Path $sf) { $env:VCC_STOCKFISH = $sf }
}
Set-Location $Bin
& $Exe @args
