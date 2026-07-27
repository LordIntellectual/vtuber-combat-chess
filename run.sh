#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
BIN="$ROOT/local/bin/VTuberCombatChess"
if [[ ! -x "$BIN" ]]; then
  echo "Build first: ./install_and_build.sh" >&2
  exit 1
fi
export PATH="/usr/games:${PATH:-}"
export LD_LIBRARY_PATH="$ROOT/deps/local/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$BIN" "$@"
