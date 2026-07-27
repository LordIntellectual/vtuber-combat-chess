# Build Bullet + vTuber Combat Chess on Windows 11 (PowerShell).
# Requires: CMake, a C++ toolchain (Visual Studio 2022 Build Tools recommended),
#           and Git (for GLFW download). Network needed on first run.
#
# Usage (from repo root, PowerShell):
#   Set-ExecutionPolicy -Scope Process Bypass
#   .\install_and_build.ps1
#
# Optional: install system packages via vcpkg if CMAKE_PREFIX_PATH is set.
# This script builds Bullet from source under deps\ like the Linux installer.

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

function Log($msg) { Write-Host "[VCC] $msg" }

$Deps = Join-Path $Root "deps"
$Upstream = Join-Path $Root "upstream"
$Build = Join-Path $Root "build"
$Prefix = Join-Path $Root "local"
$BulletVer = "2.87"
$BulletTgz = Join-Path $Deps "downloads\bullet3-$BulletVer.tar.gz"
$BulletSrc = Join-Path $Deps "src\bullet3-$BulletVer"

Log "Root: $Root"

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
  throw "cmake not found. Install CMake and add it to PATH."
}

# Prefer Visual Studio generator when available
$Generator = $null
$Arch = "x64"
if (Get-Command vswhere -ErrorAction SilentlyContinue) {
  $vs = & vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
  if ($vs) { $Generator = "Visual Studio 17 2022" }
}
# Fallback: let CMake pick (Ninja / MinGW / VS)
$UseVs = $false
if ($Generator) { $UseVs = $true }

New-Item -ItemType Directory -Force -Path (Join-Path $Deps "downloads") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $Deps "src") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $Deps "local") | Out-Null

if (-not (Test-Path $BulletTgz)) {
  Log "Downloading Bullet $BulletVer…"
  $url = "https://github.com/bulletphysics/bullet3/archive/$BulletVer.tar.gz"
  Invoke-WebRequest -Uri $url -OutFile $BulletTgz
}

if (-not (Test-Path $BulletSrc)) {
  Log "Extracting Bullet…"
  # tar is available on Windows 10/11
  tar -xzf $BulletTgz -C (Join-Path $Deps "src")
}

$BulletLib = Join-Path $Deps "local\lib\BulletDynamics.lib"
$BulletLibAlt = Join-Path $Deps "local\lib\libBulletDynamics.a"
if (-not (Test-Path $BulletLib) -and -not (Test-Path $BulletLibAlt)) {
  Log "Building Bullet into deps\local …"
  $BulletBuild = Join-Path $Deps "build\bullet"
  if (Test-Path $BulletBuild) { Remove-Item -Recurse -Force $BulletBuild }
  New-Item -ItemType Directory -Force -Path $BulletBuild | Out-Null

  $cmakeBullet = @(
    "-S", $BulletSrc,
    "-B", $BulletBuild,
    "-DCMAKE_INSTALL_PREFIX=$Deps\local",
    "-DCMAKE_BUILD_TYPE=Release",
    "-DBUILD_EXTRAS=OFF",
    "-DBUILD_BULLET2_DEMOS=OFF",
    "-DBUILD_CPU_DEMOS=OFF",
    "-DBUILD_OPENGL3_DEMOS=OFF",
    "-DBUILD_UNIT_TESTS=OFF",
    "-DBUILD_SHARED_LIBS=OFF",
    "-DUSE_MSVC_RUNTIME_LIBRARY_DLL=ON",
    "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
  )
  if ($UseVs) {
    $cmakeBullet = @("-G", $Generator, "-A", $Arch) + $cmakeBullet
  }
  & cmake @cmakeBullet
  & cmake --build $BulletBuild --config Release --parallel
  & cmake --install $BulletBuild --config Release
} else {
  Log "Bullet already installed under deps\local"
}

# libpng: prefer vcpkg if VCPKG_ROOT is set, else try to find system / chocolatey
$ExtraPrefix = @("$Deps\local")
if ($env:VCPKG_ROOT) {
  $triplet = if ($env:VCPKG_DEFAULT_TRIPLET) { $env:VCPKG_DEFAULT_TRIPLET } else { "x64-windows" }
  $vp = Join-Path $env:VCPKG_ROOT "installed\$triplet"
  if (Test-Path $vp) {
    Log "Using vcpkg prefix: $vp"
    $ExtraPrefix += $vp
  } else {
    Log "VCPKG_ROOT set but $vp missing — install: vcpkg install libpng zlib --triplet $triplet"
  }
}
$PrefixPath = ($ExtraPrefix -join ";")

if (Test-Path $Build) { Remove-Item -Recurse -Force $Build }
New-Item -ItemType Directory -Force -Path $Build, $Prefix | Out-Null

Log "Configuring vTuber Combat Chess…"
$cmakeGame = @(
  "-S", $Upstream,
  "-B", $Build,
  "-DCMAKE_INSTALL_PREFIX=$Prefix",
  "-DCMAKE_PREFIX_PATH=$PrefixPath",
  "-DCMAKE_BUILD_TYPE=Release",
  "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
)
if ($UseVs) {
  $cmakeGame = @("-G", $Generator, "-A", $Arch) + $cmakeGame
}
& cmake @cmakeGame

Log "Building…"
& cmake --build $Build --config Release --parallel
Log "Installing…"
& cmake --install $Build --config Release

# Ensure nca assets present
$NcaDst = Join-Path $Prefix "share\nca"
New-Item -ItemType Directory -Force -Path $NcaDst | Out-Null
Copy-Item -Recurse -Force (Join-Path $Root "share\nca\*") $NcaDst

# Portable layout next to exe for runtime path discovery
$BinDir = Join-Path $Prefix "bin"
$ShareNextToBin = Join-Path $BinDir "share"
New-Item -ItemType Directory -Force -Path $ShareNextToBin | Out-Null
if (Test-Path (Join-Path $Prefix "share\toonchess")) {
  Copy-Item -Recurse -Force (Join-Path $Prefix "share\toonchess") (Join-Path $ShareNextToBin "toonchess")
}
if (Test-Path (Join-Path $Prefix "share\nca")) {
  Copy-Item -Recurse -Force (Join-Path $Prefix "share\nca") (Join-Path $ShareNextToBin "nca")
}

# Optional: copy stockfish.exe if present in tools or PATH
$StockfishCandidates = @(
  (Join-Path $Root "tools\stockfish.exe"),
  (Join-Path $Root "stockfish.exe"),
  (Join-Path $BinDir "stockfish.exe")
)
foreach ($s in $StockfishCandidates) {
  if (Test-Path $s) {
    Copy-Item -Force $s (Join-Path $BinDir "stockfish.exe")
    Log "Bundled stockfish.exe next to game"
    break
  }
}

$Exe = Join-Path $BinDir "VTuberCombatChess.exe"
if (-not (Test-Path $Exe)) {
  # Multi-config generators may place exe under Release\
  $Alt = Join-Path $Build "Release\VTuberCombatChess.exe"
  if (Test-Path $Alt) {
    Copy-Item -Force $Alt $Exe
  }
}

Log "Done."
Log "Executable: $Exe"
Log "Run: .\run.ps1"
Log "Stockfish: place stockfish.exe in local\bin or on PATH (or set VCC_STOCKFISH)"
