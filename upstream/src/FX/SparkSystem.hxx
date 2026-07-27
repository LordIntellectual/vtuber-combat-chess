#ifndef NCA_SPARK_SYSTEM_HXX_
#define NCA_SPARK_SYSTEM_HXX_

#include <vector>
#include <random>
#include "../gl_compat.hxx"
#include "../utils/math.hxx"
#include "../Camera/Camera.hxx"
#include "../Clock/Clock.hxx"
#include "../shader/ShaderProgram.hxx"

struct Spark {
  Vector3f pos;
  Vector3f vel;
  Vector3f color;
  float life;
  float maxLife;
  float size;   // world-space billboard diameter
  bool neon;    // soft glowing trail (space theme) vs hard spark
};

class SparkSystem {
public:
  static const int MAX_POINT_LIGHTS = 8;

  explicit SparkSystem();
  ~SparkSystem();
  void init();
  /** Short hard sparks (captures, high FX). */
  void burst(Vector3f origin, Vector3f color, int count, float power);
  /** Soft neon cloud matching smoke lifetime (~2–3s); for space move trails. */
  void emitNeonCloud(Vector3f origin, Vector3f color, int count);
  void update();
  void draw(Camera* camera);
  void setIntensity(float s) { intensity = s; }

  /** Gather up to maxCount neon particles as dynamic point lights (world space). */
  void gatherNeonLights(Vector3f* positions, Vector3f* colors, float* intensities,
                        int maxCount, int* outCount) const;

private:
  static const int MAX_SPARKS = 4000;

  std::vector<Spark> sparks;
  GLuint quadVbo;       // shared unit quad corners
  GLuint centerSizeVbo; // per-particle center + size
  GLuint colorAlphaVbo; // per-particle color + alpha
  ShaderProgram* program;
  Clock clock;
  std::default_random_engine rng;
  float intensity;
  bool ready;

  // Unit quad (XZ plane) — same layout as SmokeGenerator
  static const GLfloat kQuadVerts[12];
};

#endif
