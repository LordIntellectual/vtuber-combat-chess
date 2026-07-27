#!/usr/bin/env bash
# Build Bullet 2.87 (local) + vTuber Combat Chess. Self-contained — no sibling projects.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
DEPS="$ROOT/deps"
UPSTREAM="$ROOT/upstream"
BUILD="$ROOT/build"
PREFIX="$ROOT/local"
BULLET_VER=2.87
BULLET_TGZ="$DEPS/downloads/bullet3-${BULLET_VER}.tar.gz"
BULLET_SRC="$DEPS/src/bullet3-${BULLET_VER}"

log() { printf '[VCC] %s\n' "$*"; }

log "Root: $ROOT"

if command -v apt-get >/dev/null 2>&1; then
  log "Ensuring system packages (may prompt for sudo)…"
  sudo apt-get install -y \
    build-essential cmake curl \
    stockfish \
    xorg-dev freeglut3-dev \
    libpng-dev zlib1g-dev \
    pkg-config \
    >/dev/null || true
fi

export PATH="/usr/games:${PATH:-/usr/bin:/bin}"
if ! command -v stockfish >/dev/null 2>&1; then
  log "Warning: stockfish not found on PATH — AI opponent will fail until installed"
else
  log "stockfish: $(command -v stockfish)"
fi

if [[ ! -f "$UPSTREAM/CMakeLists.txt" ]]; then
  echo "Missing $UPSTREAM/CMakeLists.txt" >&2
  exit 1
fi

# --- Bullet Physics (local install; not system-wide) ---
mkdir -p "$DEPS/downloads" "$DEPS/src" "$DEPS/build/bullet" "$DEPS/local"
if [[ ! -f "$BULLET_TGZ" ]]; then
  log "Downloading Bullet ${BULLET_VER}…"
  curl -L -o "$BULLET_TGZ" \
    "https://github.com/bulletphysics/bullet3/archive/${BULLET_VER}.tar.gz"
fi
if [[ ! -d "$BULLET_SRC" ]]; then
  log "Extracting Bullet…"
  tar -xzf "$BULLET_TGZ" -C "$DEPS/src"
fi
if [[ ! -f "$DEPS/local/lib/libBulletDynamics.so" && ! -f "$DEPS/local/lib/libBulletDynamics.a" ]]; then
  log "Building Bullet into $DEPS/local …"
  rm -rf "$DEPS/build/bullet"
  mkdir -p "$DEPS/build/bullet"
  cmake -S "$BULLET_SRC" -B "$DEPS/build/bullet" \
    -DCMAKE_INSTALL_PREFIX="$DEPS/local" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DBUILD_EXTRAS=OFF \
    -DBUILD_BULLET2_DEMOS=OFF \
    -DBUILD_CPU_DEMOS=OFF \
    -DBUILD_OPENGL3_DEMOS=OFF \
    -DBUILD_UNIT_TESTS=OFF \
    -DBUILD_SHARED_LIBS=ON
  cmake --build "$DEPS/build/bullet" -j"$(nproc 2>/dev/null || echo 2)"
  cmake --install "$DEPS/build/bullet"
else
  log "Bullet already installed under deps/local"
fi

export CMAKE_PREFIX_PATH="$DEPS/local${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"
export CMAKE_POLICY_VERSION_MINIMUM=3.5
export LD_LIBRARY_PATH="$DEPS/local/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

# Game assets live at repo share/nca (sibling of upstream/)
mkdir -p "$ROOT/share/nca"

log "Configuring vTuber Combat Chess…"
rm -rf "$BUILD"
mkdir -p "$BUILD" "$PREFIX"
cmake -S "$UPSTREAM" -B "$BUILD" \
  -DCMAKE_INSTALL_PREFIX="$PREFIX" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$DEPS/local" \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5

log "Building…"
cmake --build "$BUILD" -j"$(nproc 2>/dev/null || echo 2)"
log "Installing…"
cmake --install "$BUILD"

mkdir -p "$PREFIX/share/nca"
cp -a "$ROOT/share/nca/." "$PREFIX/share/nca/" 2>/dev/null || true

ln -sfn "$PREFIX/bin/VTuberCombatChess" "$ROOT/VTuberCombatChess"
log "Done → $PREFIX/bin/VTuberCombatChess"
log "Run: $ROOT/run.sh"
