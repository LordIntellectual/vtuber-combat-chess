# Build Bullet + vTuber Combat Chess on Windows 11 (PowerShell 5.1+).
# ASCII-only script (no Unicode ellipsis) for PowerShell 5.1 encoding safety.
#
# Prerequisites:
#   - Visual Studio 2022 with "Desktop development with C++"
#   - CMake on PATH
#   - Git on PATH
#   - vcpkg with: vcpkg install libpng:x64-windows zlib:x64-windows
#   - $env:VCPKG_ROOT set to your vcpkg root
#
# Usage (from repo root):
#   Set-ExecutionPolicy -Scope Process Bypass
#   .\install_and_build.ps1

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

function Log([string]$msg) { Write-Host "[VCC] $msg" }

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
if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
  throw "git not found. Install Git for Windows (enable PATH option)."
}

$Generator = "Visual Studio 17 2022"
$Arch = "x64"

New-Item -ItemType Directory -Force -Path (Join-Path $Deps "downloads") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $Deps "src") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $Deps "local\lib") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $Deps "local\include") | Out-Null

if (-not (Test-Path $BulletTgz)) {
  Log "Downloading Bullet $BulletVer..."
  $url = "https://github.com/bulletphysics/bullet3/archive/$BulletVer.tar.gz"
  Invoke-WebRequest -Uri $url -OutFile $BulletTgz
}

if (-not (Test-Path $BulletSrc)) {
  Log "Extracting Bullet..."
  tar -xzf $BulletTgz -C (Join-Path $Deps "src")
}

function Install-BulletArtifacts {
  # Bullet 2.87 + multi-config VS often leaves cmake --install incomplete.
  # Copy Release .lib files and headers into deps\local explicitly.
  $libSrc = Join-Path $Deps "build\bullet\lib\Release"
  $libDst = Join-Path $Deps "local\lib"
  $incDst = Join-Path $Deps "local\include\bullet"
  if (Test-Path $libSrc) {
    Log "Copying Bullet Release libraries to deps\local\lib..."
    New-Item -ItemType Directory -Force -Path $libDst | Out-Null
    Copy-Item -Force (Join-Path $libSrc "*.lib") $libDst
  }
  # Also try non-multi-config layout
  $libSrc2 = Join-Path $Deps "build\bullet\lib"
  if (Test-Path $libSrc2) {
    Get-ChildItem -Path $libSrc2 -Filter "*.lib" -Recurse -ErrorAction SilentlyContinue |
      ForEach-Object { Copy-Item -Force $_.FullName $libDst }
  }
  if (Test-Path (Join-Path $BulletSrc "src")) {
    Log "Copying Bullet headers to deps\local\include\bullet..."
    New-Item -ItemType Directory -Force -Path $incDst | Out-Null
    Copy-Item -Recurse -Force (Join-Path $BulletSrc "src\*") $incDst
  }
}

$BulletLib = Join-Path $Deps "local\lib\BulletDynamics.lib"
if (-not (Test-Path $BulletLib)) {
  Log "Building Bullet into deps\local..."
  $BulletBuild = Join-Path $Deps "build\bullet"
  if (Test-Path $BulletBuild) { Remove-Item -Recurse -Force $BulletBuild }
  New-Item -ItemType Directory -Force -Path $BulletBuild | Out-Null

  & cmake -G $Generator -A $Arch `
    -S $BulletSrc `
    -B $BulletBuild `
    "-DCMAKE_INSTALL_PREFIX=$Deps\local" `
    "-DCMAKE_BUILD_TYPE=Release" `
    "-DBUILD_EXTRAS=OFF" `
    "-DBUILD_BULLET2_DEMOS=OFF" `
    "-DBUILD_CPU_DEMOS=OFF" `
    "-DBUILD_OPENGL3_DEMOS=OFF" `
    "-DBUILD_UNIT_TESTS=OFF" `
    "-DBUILD_SHARED_LIBS=OFF" `
    "-DUSE_MSVC_RUNTIME_LIBRARY_DLL=ON" `
    "-DINSTALL_LIBS=ON" `
    "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"

  & cmake --build $BulletBuild --config Release --parallel
  # Try official install, then always force-copy (hard-won Windows fix)
  try { & cmake --install $BulletBuild --config Release } catch { Log "cmake --install Bullet reported errors (continuing with manual copy)" }
  Install-BulletArtifacts
} else {
  Log "Bullet already present under deps\local\lib"
  # Refresh headers if missing
  if (-not (Test-Path (Join-Path $Deps "local\include\bullet\btBulletDynamicsCommon.h"))) {
    Install-BulletArtifacts
  }
}

if (-not (Test-Path $BulletLib)) {
  throw "BulletDynamics.lib still missing after Bullet build. See docs\BUILD_WINDOWS.md"
}

# vcpkg libpng/zlib
$ExtraPrefix = @("$Deps\local")
if ($env:VCPKG_ROOT) {
  $triplet = if ($env:VCPKG_DEFAULT_TRIPLET) { $env:VCPKG_DEFAULT_TRIPLET } else { "x64-windows" }
  $vp = Join-Path $env:VCPKG_ROOT "installed\$triplet"
  if (Test-Path $vp) {
    Log "Using vcpkg prefix: $vp"
    $ExtraPrefix += $vp
  } else {
    Log "WARNING: VCPKG_ROOT set but $vp missing. Run: vcpkg install libpng:x64-windows zlib:x64-windows"
  }
} else {
  Log "WARNING: VCPKG_ROOT not set. libpng may not be found."
}
$PrefixPath = ($ExtraPrefix -join ";")

if (Test-Path $Build) { Remove-Item -Recurse -Force $Build }
New-Item -ItemType Directory -Force -Path $Build, $Prefix | Out-Null

Log "Configuring vTuber Combat Chess..."
# Single-line-friendly flags (PowerShell 5.1 splits multi-line args poorly)
$cmakeArgs = @(
  "-G", $Generator,
  "-A", $Arch,
  "-S", $Upstream,
  "-B", $Build,
  "-DCMAKE_INSTALL_PREFIX=$Prefix",
  "-DCMAKE_PREFIX_PATH=$PrefixPath",
  "-DBULLET_ROOT=$Deps\local",
  "-DCMAKE_POLICY_DEFAULT_CMP0074=NEW",
  "-DCMAKE_POLICY_DEFAULT_CMP0144=NEW",
  "-DCMAKE_BUILD_TYPE=Release"
)
& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }

Log "Building..."
& cmake --build $Build --config Release --parallel
if ($LASTEXITCODE -ne 0) { throw "Build failed" }

Log "Installing..."
& cmake --install $Build --config Release

$NcaDst = Join-Path $Prefix "share\nca"
New-Item -ItemType Directory -Force -Path $NcaDst | Out-Null
Copy-Item -Recurse -Force (Join-Path $Root "share\nca\*") $NcaDst

$BinDir = Join-Path $Prefix "bin"
$ShareNextToBin = Join-Path $BinDir "share"
New-Item -ItemType Directory -Force -Path $ShareNextToBin | Out-Null
if (Test-Path (Join-Path $Prefix "share\toonchess")) {
  Copy-Item -Recurse -Force (Join-Path $Prefix "share\toonchess") (Join-Path $ShareNextToBin "toonchess")
}
if (Test-Path (Join-Path $Prefix "share\nca")) {
  Copy-Item -Recurse -Force (Join-Path $Prefix "share\nca") (Join-Path $ShareNextToBin "nca")
}

# Multi-config: copy exe from build\Release if install missed it
$Exe = Join-Path $BinDir "VTuberCombatChess.exe"
$Alt = Join-Path $Build "Release\VTuberCombatChess.exe"
if (-not (Test-Path $Exe) -and (Test-Path $Alt)) {
  New-Item -ItemType Directory -Force -Path $BinDir | Out-Null
  Copy-Item -Force $Alt $Exe
}

# Runtime DLLs next to the exe (required for double-click without vcpkg on PATH)
function Copy-RuntimeDlls([string]$DestDir) {
  $copied = @()
  $searchDirs = @()
  if ($env:VCPKG_ROOT) {
    $triplet = if ($env:VCPKG_DEFAULT_TRIPLET) { $env:VCPKG_DEFAULT_TRIPLET } else { "x64-windows" }
    $searchDirs += (Join-Path $env:VCPKG_ROOT "installed\$triplet\bin")
    $searchDirs += (Join-Path $env:VCPKG_ROOT "installed\$triplet\debug\bin")
  }
  $searchDirs += (Join-Path $Deps "local\bin")
  $searchDirs += (Join-Path $Deps "local\lib")

  $want = @("libpng16.dll", "libpng16d.dll", "zlib1.dll", "z.dll", "libzlib.dll")
  foreach ($dir in $searchDirs) {
    if (-not (Test-Path $dir)) { continue }
    foreach ($name in $want) {
      $src = Join-Path $dir $name
      if (Test-Path $src) {
        Copy-Item -Force $src $DestDir
        $copied += $name
      }
    }
    # Also grab any libpng*.dll / zlib*.dll patterns
    Get-ChildItem -Path $dir -Filter "libpng*.dll" -ErrorAction SilentlyContinue |
      ForEach-Object { Copy-Item -Force $_.FullName $DestDir; $copied += $_.Name }
    Get-ChildItem -Path $dir -Filter "zlib*.dll" -ErrorAction SilentlyContinue |
      ForEach-Object { Copy-Item -Force $_.FullName $DestDir; $copied += $_.Name }
  }
  if ($copied.Count -gt 0) {
    Log ("Copied runtime DLLs: " + (($copied | Select-Object -Unique) -join ", "))
  } else {
    Log "WARNING: No libpng/zlib DLLs found. Copy them next to the .exe before double-clicking."
  }
}

Copy-RuntimeDlls $BinDir

# Stockfish next to the exe (required for AI without PATH setup)
function Install-Stockfish([string]$DestDir) {
  $destExe = Join-Path $DestDir "stockfish.exe"
  if (Test-Path $destExe) {
    Log "stockfish.exe already present next to game"
    return
  }
  foreach ($s in @(
    (Join-Path $Root "tools\stockfish\stockfish.exe"),
    (Join-Path $Root "tools\stockfish.exe"),
    (Join-Path $Root "third_party\stockfish\stockfish.exe"),
    (Join-Path $Root "stockfish.exe")
  )) {
    if (Test-Path $s) {
      Copy-Item -Force $s $destExe
      Log "Bundled stockfish.exe from $s"
      return
    }
  }

  # Download official Stockfish Windows build (GPL-3) if missing
  $url = "https://github.com/official-stockfish/Stockfish/releases/download/sf_18/stockfish-windows-x86-64-avx2.zip"
  $zip = Join-Path $Deps "downloads\stockfish-windows-x86-64-avx2.zip"
  $extract = Join-Path $Deps "downloads\stockfish_extract"
  New-Item -ItemType Directory -Force -Path (Split-Path $zip) | Out-Null
  try {
    if (-not (Test-Path $zip)) {
      Log "Downloading Stockfish (Windows x86-64 AVX2)..."
      Invoke-WebRequest -Uri $url -OutFile $zip
    }
    if (Test-Path $extract) { Remove-Item -Recurse -Force $extract }
    Expand-Archive -Path $zip -DestinationPath $extract -Force
    $found = Get-ChildItem -Path $extract -Recurse -Filter "stockfish*.exe" |
      Where-Object { $_.Name -match "windows" -or $_.Name -eq "stockfish.exe" } |
      Select-Object -First 1
    if (-not $found) {
      $found = Get-ChildItem -Path $extract -Recurse -Filter "*.exe" | Select-Object -First 1
    }
    if ($found) {
      Copy-Item -Force $found.FullName $destExe
      Log "Installed stockfish.exe from official release ($($found.Name))"
    } else {
      Log "WARNING: Stockfish zip extracted but no .exe found"
    }
  } catch {
    Log "WARNING: Could not download Stockfish: $_"
    Log "Place stockfish.exe next to VTuberCombatChess.exe for AI support."
  }
}

Install-Stockfish $BinDir

if (-not (Test-Path $Exe)) {
  throw "VTuberCombatChess.exe not found after build. Check build log above."
}

# Verify double-click layout
$sfOk = Test-Path (Join-Path $BinDir "stockfish.exe")
$shareOk = (Test-Path (Join-Path $BinDir "share\toonchess")) -and (Test-Path (Join-Path $BinDir "share\nca"))
Log "Done."
Log "Executable: $Exe"
Log "Portable layout ready for double-click: share=$shareOk stockfish=$sfOk"
Log "Run: .\run.ps1   OR double-click: $Exe"
