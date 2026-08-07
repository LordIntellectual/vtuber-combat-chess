#ifndef NCA_THEME_HXX_
#define NCA_THEME_HXX_

#include <string>
#include "../utils/math.hxx"

enum ThemeId {
  THEME_NEON = 0,
  THEME_JUNGLE = 1,
  THEME_STARSHIP = 2,
  THEME_COUNT = 3
};

enum FxIntensity {
  FX_LOW = 0,
  FX_MED = 1,
  FX_HIGH = 2
};

/* Per-level move trail style (smoke sprites vs neon energy cloud). */
enum MoveParticleStyle {
  MOVE_PARTICLE_SMOKE = 0,
  MOVE_PARTICLE_NEON = 1
};

struct Theme {
  ThemeId id;
  const char* name;
  const char* tagline;

  // Clear / sky gradient
  Vector3f clearTop;
  Vector3f clearBottom;

  // Board
  Vector4f boardDark;
  Vector4f boardLight;
  Vector4f boardHighlight;

  // Pieces
  Vector4f pieceUser;
  Vector4f pieceAI;

  // Particles
  Vector3f smokeUser;
  Vector3f smokeAI;
  Vector3f sparkColor;
  Vector3f moveTrail;
  MoveParticleStyle moveParticleStyle;
  Vector3f neonTrailUser; // white/player move glow (starship: red)
  Vector3f neonTrailAI;   // black/AI move glow (starship: purple)

  // Light
  Vector3f lightDir;
  float emissiveBoost; // 0..1 extra brightness on pieces
  float borderScale;   // outline thickness feel (shader-side proxy via color)

  // Audio
  const char* musicFile; // relative under share/nca/audio/
};

class ThemeManager {
public:
  ThemeManager();
  void setTheme(ThemeId id);
  void cycle();
  const Theme& current() const { return themes[static_cast<int>(active)]; }
  ThemeId activeId() const { return active; }

  void setFx(FxIntensity fx) { fxLevel = fx; }
  void cycleFx();
  FxIntensity fx() const { return fxLevel; }
  float fxScale() const; // particle multiplier

  static const char* fxName(FxIntensity f);

private:
  Theme themes[THEME_COUNT];
  ThemeId active;
  FxIntensity fxLevel;
};

#endif
