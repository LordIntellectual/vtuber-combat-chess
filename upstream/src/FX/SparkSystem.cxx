#define GL_GLEXT_PROTOTYPES
#include <GLFW/glfw3.h>

#include "SparkSystem.hxx"
#include "../get_share_path.hxx"
#include "../shader/Shader.hxx"
#include <cmath>
#include <iostream>
#include <algorithm>
#include <vector>

// Camera-facing unit quad corners (XZ), triangle strip — matches smoke
const GLfloat SparkSystem::kQuadVerts[12] = {
  -0.5f, 0.0f, -0.5f,
   0.5f, 0.0f, -0.5f,
  -0.5f, 0.0f,  0.5f,
   0.5f, 0.0f,  0.5f,
};

SparkSystem::SparkSystem()
  : quadVbo(0), centerSizeVbo(0), colorAlphaVbo(0),
    program(nullptr), intensity(1.f), ready(false) {
  rng.seed(1337);
}

SparkSystem::~SparkSystem() {
  if (quadVbo) glDeleteBuffers(1, &quadVbo);
  if (centerSizeVbo) glDeleteBuffers(1, &centerSizeVbo);
  if (colorAlphaVbo) glDeleteBuffers(1, &colorAlphaVbo);
  if (program) {
    delete program;
    program = nullptr;
  }
}

void SparkSystem::init() {
  std::string base = get_share_path();
  std::string nca = base;
  auto pos = nca.rfind("toonchess");
  if (pos != std::string::npos) nca.replace(pos, 9, "nca");
  else nca = base + "../nca/";

  try {
    std::string vsPath = nca + "shaders/sparkVS.glsl";
    std::string fsPath = nca + "shaders/sparkFS.glsl";
    Shader* v = new Shader(vsPath, GL_VERTEX_SHADER);
    Shader* f = new Shader(fsPath, GL_FRAGMENT_SHADER);
    std::vector<Shader*> shaders = {v, f};
    program = new ShaderProgram(shaders);
    // Bind locations before link so draw can use 0/1/2 like smoke
    // (ShaderProgram::compile links immediately — set after shaders compile
    // but we need pre-link binds. compile() does compile+link; so we replicate
    // a short path: compile shaders, create program, bind, link.)
    // ShaderProgram already links in compile(). Attribute layout layout(location)
    // isn't in GLSL 130. Rely on declaration order (0,1,2) like smoke, and
    // verify with glGetAttribLocation after compile.
    program->compile();
  } catch (const std::exception& e) {
    std::cerr << "[Spark] shader load failed: " << e.what()
              << " — sparks disabled\n";
    ready = false;
    return;
  }

  GLint loc0 = glGetAttribLocation(program->id, "vertexPosition");
  GLint loc1 = glGetAttribLocation(program->id, "centerSize");
  GLint loc2 = glGetAttribLocation(program->id, "colorAlpha");
  std::cout << "[Spark] attrib locs corner=" << loc0
            << " centerSize=" << loc1
            << " colorAlpha=" << loc2 << "\n";
  if (loc0 < 0 || loc1 < 0 || loc2 < 0) {
    std::cerr << "[Spark] missing attribute locations — sparks disabled\n";
    ready = false;
    return;
  }

  glGenBuffers(1, &quadVbo);
  glBindBuffer(GL_ARRAY_BUFFER, quadVbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVerts), kQuadVerts, GL_STATIC_DRAW);

  glGenBuffers(1, &centerSizeVbo);
  glBindBuffer(GL_ARRAY_BUFFER, centerSizeVbo);
  glBufferData(GL_ARRAY_BUFFER, MAX_SPARKS * 4 * sizeof(GLfloat),
               NULL, GL_STREAM_DRAW);

  glGenBuffers(1, &colorAlphaVbo);
  glBindBuffer(GL_ARRAY_BUFFER, colorAlphaVbo);
  glBufferData(GL_ARRAY_BUFFER, MAX_SPARKS * 4 * sizeof(GLfloat),
               NULL, GL_STREAM_DRAW);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  ready = true;
  std::cout << "[Spark] Ready (billboard sparks + neon trails)\n";
}

void SparkSystem::burst(Vector3f origin, Vector3f color, int count, float power) {
  if (!ready) return;
  count = (int)(count * intensity);
  if (count < 1) return;
  std::uniform_real_distribution<float> U(-1.f, 1.f);
  std::uniform_real_distribution<float> U01(0.f, 1.f);
  for (int i = 0; i < count && (int)sparks.size() < MAX_SPARKS; i++) {
    Spark s;
    s.pos = origin;
    Vector3f dir = {U(rng), U(rng), U01(rng) * 0.8f + 0.2f};
    float len = std::sqrt(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z) + 1e-5f;
    dir.x /= len; dir.y /= len; dir.z /= len;
    float spd = (2.f + U01(rng) * 8.f) * power;
    s.vel = {dir.x * spd, dir.y * spd, dir.z * spd};
    s.color = color;
    s.maxLife = 0.4f + U01(rng) * 0.8f;
    s.life = s.maxLife;
    s.size = 0.18f + U01(rng) * 0.35f;
    s.neon = false;
    sparks.push_back(s);
  }
}

void SparkSystem::emitNeonCloud(Vector3f origin, Vector3f color, int count) {
  if (!ready) return;
  count = (int)std::ceil(count * intensity);
  if (count < 1) count = 1;
  std::uniform_real_distribution<float> U(-1.f, 1.f);
  std::uniform_real_distribution<float> U01(0.f, 1.f);
  for (int i = 0; i < count && (int)sparks.size() < MAX_SPARKS; i++) {
    Spark s;
    s.pos = {
      origin.x + U(rng) * 0.7f,
      origin.y + U(rng) * 0.7f,
      origin.z + U01(rng) * 1.1f + 0.15f
    };
    s.vel = {
      U(rng) * 0.85f,
      U(rng) * 0.85f,
      0.35f + U01(rng) * 1.0f
    };
    s.color = {
      std::min(1.f, color.x * (0.85f + U01(rng) * 0.35f)),
      std::min(1.f, color.y * (0.85f + U01(rng) * 0.35f)),
      std::min(1.f, color.z * (0.85f + U01(rng) * 0.35f))
    };
    s.maxLife = 2.0f + U01(rng) * 1.2f;
    s.life = s.maxLife;
    // Board cells are ~4 units; 0.5–1.2 reads as small glowing orbs
    s.size = 0.50f + U01(rng) * 0.70f;
    s.neon = true;
    sparks.push_back(s);
  }
}

void SparkSystem::update() {
  float dt = clock.getElapsedTime();
  clock.restart();
  if (dt <= 0.f || dt > 0.1f) dt = 0.016f;
  for (size_t i = 0; i < sparks.size();) {
    Spark& s = sparks[i];
    s.life -= dt;
    if (s.neon) {
      s.vel.z -= 1.2f * dt;
      s.vel.x *= (1.f - 0.6f * dt);
      s.vel.y *= (1.f - 0.6f * dt);
    } else {
      s.vel.z -= 12.f * dt;
    }
    s.pos.x += s.vel.x * dt;
    s.pos.y += s.vel.y * dt;
    s.pos.z += s.vel.z * dt;
    if (s.life <= 0.f) {
      sparks[i] = sparks.back();
      sparks.pop_back();
    } else {
      i++;
    }
  }
}

void SparkSystem::gatherNeonLights(Vector3f* positions, Vector3f* colors,
                                   float* intensities, int maxCount,
                                   int* outCount) const {
  *outCount = 0;
  if (maxCount <= 0) return;

  struct Cand { int idx; float score; };
  std::vector<Cand> cands;
  cands.reserve(sparks.size());
  for (size_t i = 0; i < sparks.size(); i++) {
    if (!sparks[i].neon) continue;
    float a = sparks[i].life / sparks[i].maxLife;
    float br = (sparks[i].color.x + sparks[i].color.y + sparks[i].color.z) / 3.f;
    cands.push_back({(int)i, a * (0.4f + br)});
  }
  if (cands.empty()) return;
  std::partial_sort(
    cands.begin(),
    cands.begin() + std::min((int)cands.size(), maxCount),
    cands.end(),
    [](const Cand& a, const Cand& b) { return a.score > b.score; });

  int n = std::min((int)cands.size(), maxCount);
  for (int i = 0; i < n; i++) {
    const Spark& s = sparks[(size_t)cands[(size_t)i].idx];
    float a = s.life / s.maxLife;
    positions[i] = s.pos;
    colors[i] = s.color;
    intensities[i] = a * 2.8f * (s.size / 0.9f);
  }
  *outCount = n;
}

void SparkSystem::draw(Camera* camera) {
  if (!ready || !program || sparks.empty()) return;

  const int n = (int)sparks.size();
  std::vector<GLfloat> centerSize((size_t)n * 4);
  std::vector<GLfloat> colorAlpha((size_t)n * 4);

  for (int i = 0; i < n; i++) {
    const Spark& s = sparks[(size_t)i];
    float lifeT = s.life / s.maxLife;
    float fade = s.neon ? std::sqrt(std::max(0.f, lifeT)) : lifeT;
    float sizeMul = 1.f;
    if (s.neon) {
      float age = 1.f - lifeT;
      if (age < 0.12f) sizeMul = age / 0.12f;
      if (lifeT < 0.35f) sizeMul *= lifeT / 0.35f;
      sizeMul = std::max(0.15f, sizeMul);
    } else {
      sizeMul = 0.4f + 0.6f * lifeT;
    }
    float alpha = s.neon ? (0.70f + 0.30f * fade) : fade;

    centerSize[(size_t)i * 4 + 0] = s.pos.x;
    centerSize[(size_t)i * 4 + 1] = s.pos.y;
    centerSize[(size_t)i * 4 + 2] = s.pos.z;
    centerSize[(size_t)i * 4 + 3] = s.size * sizeMul;

    colorAlpha[(size_t)i * 4 + 0] = s.color.x;
    colorAlpha[(size_t)i * 4 + 1] = s.color.y;
    colorAlpha[(size_t)i * 4 + 2] = s.color.z;
    colorAlpha[(size_t)i * 4 + 3] = alpha;
  }

  GLint locCorner = glGetAttribLocation(program->id, "vertexPosition");
  GLint locCS = glGetAttribLocation(program->id, "centerSize");
  GLint locCA = glGetAttribLocation(program->id, "colorAlpha");
  if (locCorner < 0 || locCS < 0 || locCA < 0) return;

  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  glDepthMask(GL_FALSE);
  glEnable(GL_DEPTH_TEST);

  glUseProgram(program->id);
  program->setViewMatrix(&camera->viewMatrix);
  program->setProjectionMatrix(&camera->projectionMatrix);

  // Upload per-particle data (same pattern as SmokeGenerator)
  glBindBuffer(GL_ARRAY_BUFFER, centerSizeVbo);
  glBufferData(GL_ARRAY_BUFFER, n * 4 * sizeof(GLfloat), NULL, GL_STREAM_DRAW);
  glBufferSubData(GL_ARRAY_BUFFER, 0, n * 4 * sizeof(GLfloat), centerSize.data());

  glBindBuffer(GL_ARRAY_BUFFER, colorAlphaVbo);
  glBufferData(GL_ARRAY_BUFFER, n * 4 * sizeof(GLfloat), NULL, GL_STREAM_DRAW);
  glBufferSubData(GL_ARRAY_BUFFER, 0, n * 4 * sizeof(GLfloat), colorAlpha.data());

  glEnableVertexAttribArray(locCorner);
  glBindBuffer(GL_ARRAY_BUFFER, quadVbo);
  glVertexAttribPointer(locCorner, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

  glEnableVertexAttribArray(locCS);
  glBindBuffer(GL_ARRAY_BUFFER, centerSizeVbo);
  glVertexAttribPointer(locCS, 4, GL_FLOAT, GL_FALSE, 0, (void*)0);

  glEnableVertexAttribArray(locCA);
  glBindBuffer(GL_ARRAY_BUFFER, colorAlphaVbo);
  glVertexAttribPointer(locCA, 4, GL_FLOAT, GL_FALSE, 0, (void*)0);

  glVertexAttribDivisor(locCorner, 0);
  glVertexAttribDivisor(locCS, 1);
  glVertexAttribDivisor(locCA, 1);

  glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, n);

  glDisableVertexAttribArray(locCorner);
  glDisableVertexAttribArray(locCS);
  glDisableVertexAttribArray(locCA);
  glVertexAttribDivisor(locCorner, 0);
  glVertexAttribDivisor(locCS, 0);
  glVertexAttribDivisor(locCA, 0);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glUseProgram(0);

  glDepthMask(GL_TRUE);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_BLEND);
  glEnable(GL_CULL_FACE);
}
