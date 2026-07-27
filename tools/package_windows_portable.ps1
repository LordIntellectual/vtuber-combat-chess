# Assemble a portable folder/zip for end users (double-click ready).
# Run AFTER install_and_build.ps1 on Windows.
#
# Output: dist\VTuberCombatChess-Windows\  and  dist\VTuberCombatChess-Windows.zip

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
if (-not (Test-Path (Join-Path $Root "install_and_build.ps1"))) {
  $Root = Split-Path -Parent $MyInvocation.MyCommand.Path
  $Root = Split-Path -Parent $Root
}
Set-Location $Root

$Bin = Join-Path $Root "local\bin"
$Exe = Join-Path $Bin "VTuberCombatChess.exe"
if (-not (Test-Path $Exe)) { throw "Build first: .\install_and_build.ps1" }

$OutDir = Join-Path $Root "dist\VTuberCombatChess-Windows"
if (Test-Path $OutDir) { Remove-Item -Recurse -Force $OutDir }
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

Copy-Item -Force $Exe $OutDir
foreach ($dll in @("libpng16.dll", "z.dll", "zlib1.dll")) {
  $p = Join-Path $Bin $dll
  if (Test-Path $p) { Copy-Item -Force $p $OutDir }
}
Get-ChildItem $Bin -Filter "libpng*.dll" -ErrorAction SilentlyContinue | Copy-Item -Force -Destination $OutDir
Get-ChildItem $Bin -Filter "zlib*.dll" -ErrorAction SilentlyContinue | Copy-Item -Force -Destination $OutDir

$sf = Join-Path $Bin "stockfish.exe"
if (-not (Test-Path $sf)) { throw "stockfish.exe missing in local\bin — re-run install_and_build.ps1" }
Copy-Item -Force $sf (Join-Path $OutDir "stockfish.exe")

if (-not (Test-Path (Join-Path $Bin "share"))) {
  throw "local\bin\share missing — re-run install_and_build.ps1"
}
Copy-Item -Recurse -Force (Join-Path $Bin "share") (Join-Path $OutDir "share")

@"
vTuber Combat Chess — Windows portable build
Author: Lord Intellectual
License: GNU GPL v3 (see LICENSE in the source repository)

HOW TO PLAY
1. Unzip this folder anywhere.
2. Double-click VTuberCombatChess.exe
3. AI uses the bundled stockfish.exe (same folder).

Do not separate the .exe from the share\ folder, DLLs, or stockfish.exe.

Stockfish is GPL-3: https://stockfishchess.org/
Source: https://github.com/LordIntellectual/vtuber-combat-chess (branch: windows)
"@ | Set-Content -Encoding ASCII (Join-Path $OutDir "README-WINDOWS.txt")

$Zip = Join-Path $Root "dist\VTuberCombatChess-Windows.zip"
if (Test-Path $Zip) { Remove-Item -Force $Zip }
Compress-Archive -Path $OutDir -DestinationPath $Zip -Force

Write-Host "[VCC] Portable folder: $OutDir"
Write-Host "[VCC] Zip: $Zip"
