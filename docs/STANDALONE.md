# Standalone repository

This tree is the public **vTuber Combat Chess** project, spun out for GitHub.

- Product name: vTuber Combat Chess
- Author: Lord Intellectual
- Source history: developed from a ToonChess (GPL) base; some internal paths and comments may still say “Nightfire Chess Arena” or `nca` from the working title.
- `main` branch: Linux (native)
- `windows` branch: Windows 11 port (build via `install_and_build.ps1` — see `docs/BUILD_WINDOWS.md`)

See the top-level README for build instructions and policy (no warranty, no support, no PRs).

### How branches relate to your PC

- **GitHub repo** `LordIntellectual/vtuber-combat-chess` is standalone (not the Nightfire monorepo).
- Checkout **`main`** for Linux work, **`windows`** for the Windows port.
- Nightfire’s local tree under `Nightfire_Games/...` is independent; changes for the public game should be committed here and pushed to GitHub.
