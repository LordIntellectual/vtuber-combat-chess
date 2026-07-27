#include "Theme.hxx"

ThemeManager::ThemeManager() : active(THEME_STARSHIP), fxLevel(FX_HIGH) {
  // Cyber Neon Lounge
  themes[THEME_NEON] = {
    THEME_NEON,
    "Cyber Neon Lounge",
    "Magenta midnight · cyber grid",
    {0.02f, 0.00f, 0.08f}, {0.05f, 0.00f, 0.12f},
    {0.08f, 0.02f, 0.18f, 1.f}, {0.02f, 0.12f, 0.22f, 1.f}, {1.0f, 0.2f, 0.85f, 1.f},
    {0.2f, 0.95f, 1.0f, 1.f}, {1.0f, 0.15f, 0.75f, 1.f},
    {0.4f, 0.9f, 1.0f}, {1.0f, 0.2f, 0.8f}, {1.0f, 0.4f, 1.0f}, {0.3f, 1.0f, 1.0f},
    MOVE_PARTICLE_SMOKE,
    {0.3f, 0.9f, 1.0f}, {1.0f, 0.2f, 0.75f},
    {-0.4f, -0.8f, -0.5f}, 0.55f, 1.2f,
    "music_neon.wav"
  };
  // Bioluminescent Alien Jungle
  themes[THEME_JUNGLE] = {
    THEME_JUNGLE,
    "Bioluminescent Jungle",
    "Teal glow · living undergrowth",
    {0.00f, 0.04f, 0.03f}, {0.01f, 0.10f, 0.06f},
    {0.04f, 0.14f, 0.10f, 1.f}, {0.10f, 0.28f, 0.18f, 1.f}, {0.4f, 1.0f, 0.55f, 1.f},
    {0.85f, 1.0f, 0.55f, 1.f}, {0.15f, 0.55f, 0.45f, 1.f},
    {0.3f, 0.9f, 0.5f}, {0.2f, 0.5f, 0.4f}, {0.6f, 1.0f, 0.3f}, {0.4f, 0.95f, 0.6f},
    MOVE_PARTICLE_SMOKE,
    {0.4f, 1.0f, 0.6f}, {0.9f, 0.4f, 0.2f},
    {-0.3f, -0.7f, -0.6f}, 0.4f, 1.0f,
    "music_jungle.wav"
  };
  // Starship over a star — neon energy trails (blue white / red black)
  themes[THEME_STARSHIP] = {
    THEME_STARSHIP,
    "Starship Over a Star",
    "Corona fire · hull metal",
    {0.15f, 0.04f, 0.01f}, {0.55f, 0.18f, 0.02f},
    // Board: lit metal panels (readable on stream, not pure black)
    {0.32f, 0.30f, 0.28f, 1.f}, {0.62f, 0.58f, 0.52f, 1.f}, {1.0f, 0.55f, 0.15f, 1.f},
    // Pieces: bright hull white / heat-scorched red
    {0.95f, 0.93f, 0.88f, 1.f}, {0.75f, 0.28f, 0.12f, 1.f},
    {1.0f, 0.7f, 0.3f}, {0.9f, 0.3f, 0.1f}, {1.0f, 0.5f, 0.1f}, {1.0f, 0.85f, 0.4f},
    MOVE_PARTICLE_NEON,
    {0.25f, 0.75f, 1.0f}, // white side: electric blue
    {1.0f, 0.15f, 0.12f}, // black side: neon red
    // Light from below-star-ish but with enough lateral component for cel bands
    {-0.35f, -0.55f, -0.75f}, 0.55f, 1.1f,
    // Full track: Nova Chase Orbit (share/nca/audio/music/)
    "music/nova_chase_orbit.mp3"
  };
}

void ThemeManager::setTheme(ThemeId id) {
  if (id < 0 || id >= THEME_COUNT) return;
  active = id;
}

void ThemeManager::cycle() {
  active = static_cast<ThemeId>((static_cast<int>(active) + 1) % THEME_COUNT);
}

void ThemeManager::cycleFx() {
  fxLevel = static_cast<FxIntensity>((static_cast<int>(fxLevel) + 1) % 3);
}

float ThemeManager::fxScale() const {
  switch (fxLevel) {
    case FX_LOW: return 0.45f;
    case FX_HIGH: return 1.75f;
    default: return 1.0f;
  }
}

const char* ThemeManager::fxName(FxIntensity f) {
  switch (f) {
    case FX_LOW: return "Low";
    case FX_HIGH: return "High";
    default: return "Med";
  }
}
