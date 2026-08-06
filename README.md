# vTuber Combat Chess

![vTuber Combat Chess title](docs/promo/title.png)

![Gameplay](docs/promo/gameplay.gif)

![Screenshot 1](docs/promo/screenshot_01.png)

![Screenshot 2](docs/promo/screenshot_02.png)

![Screenshot 3](docs/promo/screenshot_03.png)

![Screenshot 4](docs/promo/screenshot_04.png)

---

**Pre-alpha** 3D combat chess for streams — cel-shaded pieces, capture destruction, themed stages, and streamer-friendly controls.

| | |
|---|---|
| **Author** | Lord Intellectual |
| **Contact** | [x.com/LordIntellectX](https://x.com/LordIntellectX) |
| **This branch** | **`windows`** — Windows 11 port |
| **Linux** | Branch [`main`](https://github.com/LordIntellectual/vtuber-combat-chess/tree/main) |
| **License** | [GNU GPL v3](LICENSE) |
| **Status** | Pre-alpha — no warranty, no support |

## Play on Windows (double-click — recommended)

**No Visual Studio. No build. Stockfish AI included.**

1. Open **[Releases](https://github.com/LordIntellectual/vtuber-combat-chess/releases)**  
2. Download **`VTuberCombatChess-Windows.zip`**  
3. Unzip the folder **somewhere permanent** (keep all files together)  
4. Double-click **`VTuberCombatChess.exe`**  
   (or `Play.bat`)

The zip already contains:

- `VTuberCombatChess.exe`
- `stockfish.exe` (AI)
- `libpng16.dll` / `z.dll`
- `share\` (assets / audio / piece sets)

**Do not** separate the `.exe` from `share\`, `stockfish.exe`, or the DLLs.

## Important: no warranty, no support, no pull requests

This software is provided **as is**, with **no warranty** of any kind.  
There is **no official support**. You may download, compile, and run it at your own risk.

**Pull requests are not accepted.** You may **fork** under the GPL. See [CONTRIBUTING.md](CONTRIBUTING.md).

## Build from source (Windows, optional)

Full instructions: **[docs/BUILD_WINDOWS.md](docs/BUILD_WINDOWS.md)**

```powershell
git clone -b windows https://github.com/LordIntellectual/vtuber-combat-chess.git
cd vtuber-combat-chess
$env:VCPKG_ROOT = "$env:USERPROFILE\vcpkg"
.\install_and_build.ps1
.\run.ps1
```

`install_and_build.ps1` downloads Stockfish and copies runtime DLLs next to the game for a portable `local\bin` layout.

## Linux

Use the **`main`** branch and `./install_and_build.sh` / `./run.sh`.
## Features (pre-alpha)

- Play vs **Stockfish** (or human vs human with AI off)
- **Network multiplayer (experimental):** host/join over TCP — see [docs/MULTIPLAYER_DESIGN.md](docs/MULTIPLAYER_DESIGN.md)
- Capture **destruction physics** (Bullet fragments), sparks / neon trails
- Action camera on captures; checkmate / forfeit victory screens
- Themes: Cyber Neon Lounge, Bioluminescent Jungle, Starship Over a Star
- Piece sets including classic, starship, space, and vTuber-style sets
- Streamer HUD, settings (sound / video / gameplay)

### Main Menu & multiplayer (experimental)

After the pre-alpha splash you get a **Main Menu**:

| Option | Effect |
|--------|--------|
| **Single Player** | Offline game (Stockfish AI / local hotseat with **A**) |
| **Multiplayer** | Host a game or join via `HOST:PORT` |
| **Settings** | Sound / Video / Gameplay (same menu as in-game **S**) |

**Esc** while playing returns to the Main Menu (does not quit). Quit from the menu **Quit** button (or Esc on the root menu).

Multiplayer is host-authoritative TCP (protocol **VCC1**), default port **7777**. Host = White, guest = Black; AI is forced off. Optional CLI still works: `--host [PORT]`, `--join HOST:PORT`. Full design: [docs/MULTIPLAYER_DESIGN.md](docs/MULTIPLAYER_DESIGN.md).

## Controls (summary)

| Key | Action |
|-----|--------|
| **1 / 2 / 3** | Theme |
| **T** | Cycle theme |
| **A** | Toggle AI (Stockfish) |
| **M** | Toggle music |
| **F** | FX intensity |
| **R** | Reset board |
| **S** | Settings |
| **P** | Cycle piece set |
| **H** | Toggle HUD |
| **Esc** | Quit |
| **RMB drag** | Orbit camera |
| **LMB** | Select / move |

## Credits

- **Author / direction:** Lord Intellectual  
- **Base engine:** [ToonChess](https://github.com/martinRenou/ToonChess) by Martin Renou (GPL-3)  
- **AI opponent:** [Stockfish](https://stockfishchess.org/) (install separately)  
- **Physics:** Bullet Physics 2.87  
- **Implementation assistance:** AI coding agents (including Grok / xAI and others), under Lord Intellectual’s direction  

## License

GNU General Public License version 3 — see [LICENSE](LICENSE) and [NOTICE](NOTICE).
