# Stockfish (not vendored in git)

Stockfish Windows binaries are large (~100MB) and exceed GitHub’s git file size
limit, so they are **not** stored in this repository.

## How end users get AI

Download the **Windows portable release zip** from GitHub Releases — it already
includes `stockfish.exe` next to the game.

## How builders get AI

`install_and_build.ps1` downloads the official Stockfish Windows build
(GPL-3) and places `stockfish.exe` in `local\bin\`.

License: GNU GPL v3 — https://stockfishchess.org/  
Source: https://github.com/official-stockfish/Stockfish  
