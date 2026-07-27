# Building vTuber Combat Chess on Windows 11

Branch: **`windows`**  
Repo: https://github.com/LordIntellectual/vtuber-combat-chess  

**Author:** Lord Intellectual  
**Status:** Pre-alpha. No warranty, no support.

> **Players:** you do **not** need this document. Download the portable zip from  
> [GitHub Releases](https://github.com/LordIntellectual/vtuber-combat-chess/releases),  
> unzip, and double-click `VTuberCombatChess.exe` (Stockfish AI included).

This guide is for **developers** building from source. It incorporates hard-won
fixes from the 2026-07-27 Windows build session (GLAD, MSVC tokens, Bullet
install, ASCII PowerShell, pure-C GLAD lib, Stockfish + DLL bundling).

## Prerequisites (once)

| Tool | Notes |
|------|--------|
| Windows 11 x64 | |
| **Visual Studio 2022** | Workload: **Desktop development with C++** (MSVC v143 + Windows SDK). Required for vcpkg and the game. |
| **CMake** 3.16+ | On PATH (`cmake --version`) |
| **Git for Windows** | Installer option: *Git from the command line and also from 3rd-party software* |
| **vcpkg** | `libpng` + `zlib` for x64-windows |
| **Stockfish** | `stockfish.exe` for AI (optional until you enable AI) |
| GPU drivers | OpenGL 3.3+ compatible |

### vcpkg

```powershell
cd $env:USERPROFILE
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg install libpng:x64-windows zlib:x64-windows
# Permanent:
setx VCPKG_ROOT "$env:USERPROFILE\vcpkg"
```

Open a **new** PowerShell after `setx`. Confirm:

```powershell
echo $env:VCPKG_ROOT
```

If vcpkg says it cannot find Visual Studio, open **Visual Studio Installer → Modify** and enable **Desktop development with C++**.

## Clone the windows branch

```powershell
cd $env:USERPROFILE\Videos
git clone -b windows https://github.com/LordIntellectual/vtuber-combat-chess.git
cd vtuber-combat-chess
```

## Build

```powershell
Set-ExecutionPolicy -Scope Process Bypass
# ensure vcpkg is visible in this session
$env:VCPKG_ROOT = "$env:USERPROFILE\vcpkg"   # adjust if needed
.\install_and_build.ps1
```

The script:

1. Downloads/builds **Bullet 2.87** and **copies** Release `.lib` + headers into `deps\local` (VS multi-config install is unreliable for Bullet 2.87).
2. Configures the game with `BULLET_ROOT` and CMake package-root policies.
3. Builds with **GLAD** (vendored OpenGL 3.3 compatibility loader).
4. Installs into `local\` and copies `share\` next to the `.exe`.
5. Copies libpng/zlib DLLs from vcpkg when available.

## Stockfish

Download Windows Stockfish, then either:

- Copy `stockfish.exe` into `local\bin\`, or  
- Put it on `PATH`, or  
- `setx VCC_STOCKFISH "C:\full\path\to\stockfish.exe"`

## Run

```powershell
.\run.ps1
```

## Portable layout

```
local\bin\
  VTuberCombatChess.exe
  stockfish.exe          (you add)
  libpng16.dll / zlib1.dll  (from vcpkg copy step)
  share\
    toonchess\
    nca\
```

## What changed after the first Windows report

| Issue | Fix on this branch |
|-------|--------------------|
| Unicode `...` in `.ps1` broke PS 5.1 | Script is **ASCII-only** |
| Bullet `cmake --install` empty | Explicit copy of libs + headers |
| `BULLET_ROOT` ignored | `CMP0074` / `CMP0144` NEW in CMakeLists + script flags |
| OpenGL symbols missing on MSVC | Vendored **GLAD** + `vccInitGL()` after context |
| `and` / `or` / `not` | `/FI msvc_compat.hxx` includes `<ciso646>` (**C++ only**) |
| `M_PI` undeclared | `_USE_MATH_DEFINES` + `M_PI` fallback in force-include |
| GLAD `gl.c` → STL1003 on MSVC | GLAD built as pure-C static lib `vcc_glad` (`LANGUAGE C` / `/TC`); `/FI` never applied to C |

## If configure still fails

Manual configure (single invocation, proven pattern):

```powershell
cmake -G "Visual Studio 17 2022" -A x64 -S upstream -B build `
  -DCMAKE_INSTALL_PREFIX="$PWD\local" `
  -DCMAKE_PREFIX_PATH="$PWD\deps\local;$env:VCPKG_ROOT\installed\x64-windows" `
  -DBULLET_ROOT="$PWD\deps\local" `
  -DCMAKE_POLICY_DEFAULT_CMP0074=NEW `
  -DCMAKE_POLICY_DEFAULT_CMP0144=NEW `
  -DCMAKE_BUILD_TYPE=Release

cmake --build build --config Release --parallel
cmake --install build --config Release
```

## Reporting failures

No official support. If reporting to the author for personal builds, include:

- Step (script / configure / compile / run)
- Full console text
- Whether `[GL] glad loaded` and `[Stockfish] Engine ready` appear
- GPU name for graphics issues

**Do not** paste local Windows account paths into public GitHub issues (Issues are disabled anyway). Prefer redacting `C:\Users\<name>\...`.

## License / policy

GPL-3. No warranty. Pull requests not accepted; forks OK.
