#ifndef NCA_STARFIELD_HXX_
#define NCA_STARFIELD_HXX_

#include <vector>
#include <random>
#include <string>
#include "../gl_compat.hxx"
#include "../utils/math.hxx"
#include "../Camera/Camera.hxx"
#include "../Clock/Clock.hxx"
#include "../shader/ShaderProgram.hxx"

/* Starship-stage environment: deep-space sky sphere, star under the board,
   smooth corona swell, motion streaks, board rattle, screen flare. */
class Starfield {
public:
  Starfield();
  ~Starfield();

  void init(const std::string& ncaSharePath);
  void update(float dt);
  void drawBackground(int width, int height);
  void drawWorld(Camera* camera);
  /** Full-screen additive flare (call after 3D, before or after HUD). */
  void drawScreenFlare(int width, int height);

  Vector3f boardShake() const { return shake; }
  float boardHoverZ() const { return hoverZ; }
  /** 0..1 smooth corona intensity (for sun size / beams / flare). */
  float coronaIntensity() const { return corona; }
  float screenFlare() const { return flare; }

  bool ready() const { return ok; }

private:
  bool ok;
  GLuint texSpace;
  GLuint texStar;
  GLuint texHull;
  /** GLSL path for sky/star sampling (immune to fixed-function multitex dirt). */
  ShaderProgram* envProgram;

  struct Streak {
    Vector3f pos;
    Vector3f vel;
    float life;
    float maxLife;
  };
  std::vector<Streak> streaks;
  std::default_random_engine rng;

  // Smooth infinite corona cycle (not a snap/decay)
  float corona;          // 0..1 smooth envelope
  float flare;           // 0..1 screen brighten (slow attack/release)
  float flareTarget;     // tracks corona peaks smoothly
  float cyclePhase;      // 0..1 within flare cycle
  float cyclePeriod;     // seconds for full rest+swell+fade
  Vector3f shake;
  Vector3f shakeVel;     // spring-damper toward corona-driven target
  float hoverZ;
  float time;
  Clock clock;

  void spawnStreaks(int n);
  void beginEnvPass(Camera* camera);
  void endEnvPass();
  void bindEnvTexture(GLuint tex);
  void drawStarSphere(Camera* camera);
  void drawCoronaBeams(Camera* camera);
};

#endif
