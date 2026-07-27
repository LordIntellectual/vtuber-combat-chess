# vTuber Combat Chess

**Pre-alpha** 3D combat chess for streams — cel-shaded pieces, capture destruction, themed stages, and streamer-friendly controls.

| | |
|---|---|
| **Author** | Lord Intellectual |
| **Contact** | [x.com/LordIntellectX](https://x.com/LordIntellectX) |
| **Platform (this branch)** | Linux (tested on Ubuntu / Zorin-class systems) |
| **License** | [GNU GPL v3](LICENSE) |
| **Status** | Pre-alpha — menus, assets, and sounds are not final |

## Important: no warranty, no support, no pull requests

This software is provided **as is**, with **no warranty** of any kind.  
There is **no official support**. You may download, compile, and run it at your own risk.

**Pull requests are not accepted.** You are free to **fork** the repository and maintain your own version under the GPL. See [CONTRIBUTING.md](CONTRIBUTING.md).

## Features (pre-alpha)

- Play vs **Stockfish** (or human vs human with AI off)
- Capture **destruction physics** (Bullet fragments), sparks / neon trails
- Action camera on captures; checkmate / forfeit victory screens
- Themes: Cyber Neon Lounge, Bioluminescent Jungle, Starship Over a Star
- Piece sets including classic, starship, space, and vTuber-style sets
- Streamer HUD, settings (sound / video / gameplay)

## Requirements (Linux)

- Display with OpenGL
- CMake 3.5+, C++11 compiler (`build-essential`)
- Development packages: OpenGL / X11 (`xorg-dev`, `freeglut3-dev`), `libpng-dev`, `zlib1g-dev`, `pkg-config`
- **stockfish** on `PATH` (often `/usr/games/stockfish`)
- Network on first build (downloads **Bullet** and **GLFW**)

On Debian / Ubuntu / Zorin:

```bash
sudo apt-get install -y build-essential cmake curl stockfish \
  xorg-dev freeglut3-dev libpng-dev zlib1g-dev pkg-config
```

## Build & run

```bash
git clone https://github.com/LordIntellectual/vtuber-combat-chess.git
cd vtuber-combat-chess
./install_and_build.sh
./run.sh
```

First build compiles Bullet into `deps/local/` (can take several minutes), then builds the game into `local/bin/VTuberCombatChess`.

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

## Layout

```
vtuber-combat-chess/
├── README.md
├── LICENSE                 # GPL-3
├── NOTICE
├── CONTRIBUTING.md
├── install_and_build.sh    # Bullet + game
├── run.sh
├── share/nca/              # audio, piece sets, shaders, textures
├── tools/                  # asset helpers
├── docs/                   # design notes / gallery
└── upstream/               # C++ source (ToonChess-derived)
```

## Credits

- **Author / direction:** Lord Intellectual  
- **Base engine:** [ToonChess](https://github.com/martinRenou/ToonChess) by Martin Renou (GPL-3)  
- **AI opponent:** [Stockfish](https://stockfishchess.org/) (install separately)  
- **Physics:** Bullet Physics 2.87  
- **Implementation assistance:** AI coding agents (including Grok / xAI and others), under Lord Intellectual’s direction  

## Windows

A Windows 11 port is planned on a separate branch of this repository. This **main** branch targets **Linux only**.

## License

GNU General Public License version 3 — see [LICENSE](LICENSE) and [NOTICE](NOTICE).
