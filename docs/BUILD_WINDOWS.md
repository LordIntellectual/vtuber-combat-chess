# Building vTuber Combat Chess on Windows 11

This document is for the **`windows` branch** of  
https://github.com/LordIntellectual/vtuber-combat-chess

**Author:** Lord Intellectual  
**Status:** Pre-alpha Windows port — expect rough edges. No warranty, no support.

## What you need

| Tool | Notes |
|------|--------|
| **Windows 11** x64 | Target platform for this branch |
| **CMake** 3.16+ | https://cmake.org/download/ (add to PATH) |
| **Visual Studio 2022** | “Desktop development with C++” workload, **or** Build Tools only |
| **Git** | For GLFW fetch during configure |
| **libpng + zlib** | Easiest via **vcpkg** (see below) |
| **Stockfish** | `stockfish.exe` on PATH, next to the game, or set `VCC_STOCKFISH` |
| **GPU drivers** | OpenGL-capable GPU |

Optional: **Ninja** build tool if not using the Visual Studio generator.

## One-time: vcpkg for libpng

In PowerShell (example):

```powershell
cd $env:USERPROFILE
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg install libpng:x64-windows zlib:x64-windows
$env:VCPKG_ROOT = "$env:USERPROFILE\vcpkg"
```

Keep `VCPKG_ROOT` set for the build session (or add permanently in System Environment Variables).

## Clone this branch

```powershell
git clone -b windows https://github.com/LordIntellectual/vtuber-combat-chess.git
cd vtuber-combat-chess
```

## Build

```powershell
# Allow script for this session only if needed
Set-ExecutionPolicy -Scope Process Bypass

# Ensure vcpkg is visible
$env:VCPKG_ROOT = "$env:USERPROFILE\vcpkg"   # adjust if different

.\install_and_build.ps1
```

This will:

1. Download and build **Bullet 2.87** into `deps\local`
2. Configure/build **VTuberCombatChess** with CMake
3. Install into `local\` and copy `share\` next to the `.exe` for portable asset discovery

## Stockfish (AI opponent)

Download a Windows Stockfish binary (e.g. from https://stockfishchess.org/download/ ) and either:

- Copy `stockfish.exe` into `local\bin\`, or  
- Put it on your system `PATH`, or  
- Set environment variable:  
  `setx VCC_STOCKFISH "C:\path\to\stockfish.exe"`

Without Stockfish the game still launches; AI will fail if left ON (toggle with **A**).

## Run

```powershell
.\run.ps1
```

Or double-click `local\bin\VTuberCombatChess.exe` (ensure `local\bin\share\` exists and Stockfish is available).

## Portable folder layout (after install)

```
local\bin\
  VTuberCombatChess.exe
  stockfish.exe          (you add this)
  share\
    toonchess\           # base meshes / shaders
    nca\                 # audio, piece sets, FX
```

The game also checks env `VCC_SHARE` if you relocate assets.

## Reporting issues

There is **no official support**. If you report problems to the author for personal builds, include:

- Windows version  
- GPU / driver  
- Full console output  
- Whether Stockfish started (`[Stockfish] Engine ready`)  
- Steps to reproduce  

Pull requests are not accepted on the public repo; forks are fine under GPL-3.

## Relation to Linux / Nightfire

- GitHub **`main`** = Linux-first  
- GitHub **`windows`** = this Windows port work  
- Local Nightfire tree is separate; this public repo is standalone  

## Known Windows caveats (honest)

- First-time CMake + Bullet build can take a long time  
- libpng must be found via `CMAKE_PREFIX_PATH` / vcpkg  
- Antivirus may flag unsigned `.exe` or Stockfish downloads  
- OpenGL compatibility context is required (same as Linux HUD)  
- Path with non-ASCII characters may confuse older tools — prefer ASCII paths  
