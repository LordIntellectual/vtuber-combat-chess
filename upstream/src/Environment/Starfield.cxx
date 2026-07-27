#include "../gl_compat.hxx"
#include "Starfield.hxx"
#include "../utils/utils.hxx"
#include "../utils/GlState.hxx"
#include "../shader/Shader.hxx"
#include <cmath>
#include <iostream>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Smoothstep 0..1
static float smootherstep(float t) {
  t = std::max(0.f, std::min(1.f, t));
  return t * t * t * (t * (t * 6.f - 15.f) + 10.f);
}

Starfield::Starfield()
  : ok(false), texSpace(0), texStar(0), texHull(0), envProgram(nullptr),
    corona(0.f), flare(0.f), flareTarget(0.f),
    cyclePhase(0.f), cyclePeriod(10.f),
    shake{0,0,0}, shakeVel{0,0,0}, hoverZ(0.f), time(0.f) {
  rng.seed(42);
}

Starfield::~Starfield() {
  if (envProgram) {
    delete envProgram;
    envProgram = nullptr;
  }
}

void Starfield::init(const std::string& ncaSharePath) {
  std::string base = ncaSharePath;
  if (!base.empty() && base.back() != '/') base.push_back('/');
  try {
    texSpace = loadPNGTexture(base + "textures/space_scape.png");
  } catch (const std::exception& e) {
    std::cerr << "[Starfield] space: " << e.what() << "\n";
    texSpace = 0;
  }
  try {
    texStar = loadPNGTexture(base + "textures/star_surface.png");
  } catch (const std::exception& e) {
    std::cerr << "[Starfield] star: " << e.what() << "\n";
    texStar = 0;
  }
  try {
    texHull = loadPNGTexture(base + "textures/hull_deck.png");
  } catch (const std::exception& e) {
    std::cerr << "[Starfield] hull: " << e.what() << "\n";
    texHull = 0;
  }
  if (!texSpace && !texStar) {
    ok = false;
    std::cerr << "[Starfield] Failed to load textures from " << base << "\n";
    return;
  }

  // Textured env shader — samples only unit 0; survives smoke multitex dirt.
  try {
    std::string vsPath = base + "shaders/envTexVS.glsl";
    std::string fsPath = base + "shaders/envTexFS.glsl";
    Shader* v = new Shader(vsPath, GL_VERTEX_SHADER);
    Shader* f = new Shader(fsPath, GL_FRAGMENT_SHADER);
    std::vector<Shader*> sh = {v, f};
    envProgram = new ShaderProgram(sh);
    envProgram->compile();
    std::cout << "[Starfield] envTex shader ready\n";
  } catch (const std::exception& e) {
    std::cerr << "[Starfield] envTex shader failed (fallback fixed-fn): "
              << e.what() << "\n";
    envProgram = nullptr;
  }

  ok = true;
  spawnStreaks(100);
  // Start mid-cycle so first swell isn't immediate
  cyclePhase = 0.15f;
  cyclePeriod = 11.f;
  std::cout << "[Starfield] Space environment ready (sky sphere + smooth corona)\n";
}

void Starfield::spawnStreaks(int n) {
  std::uniform_real_distribution<float> U01(0.f, 1.f);
  for (int i = 0; i < n; i++) {
    Streak s;
    s.pos = {
      (U01(rng)*2.f - 1.f) * 40.f,
      (U01(rng)*2.f - 1.f) * 40.f,
      4.f + U01(rng) * 28.f
    };
    float spd = 10.f + U01(rng) * 28.f;
    s.vel = {(U01(rng)*2.f - 1.f) * 3.f, -spd, (U01(rng)*2.f - 1.f) * 2.f};
    s.maxLife = 2.f + U01(rng) * 4.f;
    s.life = U01(rng) * s.maxLife;
    streaks.push_back(s);
  }
}

void Starfield::update(float dt) {
  if (!ok) return;
  if (dt <= 0.f || dt > 0.1f) dt = 0.016f;
  time += dt;

  // --- Continuous smooth corona cycle ---
  // Timeline within cyclePeriod:
  //  0.00–0.25 : quiet rest (corona low)
  //  0.25–0.50 : swell build ~2.5–3s of 11s period
  //  0.50–0.52 : peak (few frames)
  //  0.52–0.77 : recede ~2.5–3s
  //  0.77–1.00 : quiet rest
  cyclePhase += dt / cyclePeriod;
  if (cyclePhase >= 1.f) {
    cyclePhase -= 1.f;
    std::uniform_real_distribution<float> P(9.5f, 13.f);
    cyclePeriod = P(rng);
    // Extra streaks when a new swell begins (smooth, not a size snap)
    spawnStreaks(30);
    std::cout << "[Starfield] Corona swell cycle\n";
  }

  float p = cyclePhase;
  float swell = 0.f;
  if (p < 0.25f) {
    swell = 0.08f; // idle simmer
  } else if (p < 0.50f) {
    // build 2.5s-ish
    float t = (p - 0.25f) / 0.25f;
    swell = 0.08f + 0.92f * smootherstep(t);
  } else if (p < 0.53f) {
    swell = 1.f; // brief peak
  } else if (p < 0.78f) {
    float t = (p - 0.53f) / 0.25f;
    swell = 1.f - 0.92f * smootherstep(t);
  } else {
    swell = 0.08f;
  }
  // Continuous micro-pulse so size never looks “stuck”
  float micro = 0.04f * std::sin(time * 1.3f) + 0.02f * std::sin(time * 2.7f);
  corona = std::max(0.f, std::min(1.f, swell + micro));

  // Screen flare: follows corona with slower attack/release (epilepsy-safe)
  // Build/recede already 2–3s via swell; flare lags slightly and caps intensity.
  flareTarget = corona * 0.42f; // never near full white
  // asymmetric smoothing: attack ~2.2s, release ~2.5s toward target
  float attack = 1.f - std::exp(-dt / 2.2f);
  float release = 1.f - std::exp(-dt / 2.5f);
  if (flareTarget > flare)
    flare += (flareTarget - flare) * attack;
  else
    flare += (flareTarget - flare) * release;

  // Board hover (flight)
  hoverZ = 0.1f * std::sin(time * 1.7f) + 0.05f * std::sin(time * 3.3f);

  // Smooth board rattle: spring toward corona-driven noise, critically damped-ish
  std::uniform_real_distribution<float> U(-1.f, 1.f);
  Vector3f target = {
    U(rng) * (0.04f + corona * 0.35f),
    U(rng) * (0.04f + corona * 0.35f),
    U(rng) * (0.02f + corona * 0.12f)
  };
  // spring-damper
  const float k = 18.f + corona * 40.f;
  const float d = 8.f;
  shakeVel.x += (target.x - shake.x) * k * dt - shakeVel.x * d * dt;
  shakeVel.y += (target.y - shake.y) * k * dt - shakeVel.y * d * dt;
  shakeVel.z += (target.z - shake.z) * k * dt - shakeVel.z * d * dt;
  shake.x += shakeVel.x * dt;
  shake.y += shakeVel.y * dt;
  shake.z += shakeVel.z * dt;
  // clamp wild spikes
  shake.x = std::max(-0.6f, std::min(0.6f, shake.x));
  shake.y = std::max(-0.6f, std::min(0.6f, shake.y));
  shake.z = std::max(-0.25f, std::min(0.25f, shake.z));

  std::uniform_real_distribution<float> U01(0.f, 1.f);
  for (auto& s : streaks) {
    s.life -= dt;
    s.pos.x += s.vel.x * dt;
    s.pos.y += s.vel.y * dt;
    s.pos.z += s.vel.z * dt;
    if (s.life <= 0.f || s.pos.y < -55.f) {
      s.pos = {
        (U01(rng)*2.f - 1.f) * 40.f,
        35.f + U01(rng) * 15.f,
        5.f + U01(rng) * 25.f
      };
      float spd = 12.f + U01(rng) * 32.f;
      s.vel = {(U01(rng)*2.f - 1.f) * 3.f, -spd, (U01(rng)*2.f - 1.f) * 2.f};
      s.maxLife = 2.f + U01(rng) * 3.f;
      s.life = s.maxLife;
    }
  }
}

void Starfield::beginEnvPass(Camera* camera) {
  ncaResetPipelineState();
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glLoadMatrixf(camera->projectionMatrix.data());
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadMatrixf(camera->viewMatrix.data());
  glDisable(GL_CULL_FACE);
  glDepthMask(GL_FALSE);
  glDisable(GL_DEPTH_TEST);
}

void Starfield::endEnvPass() {
  if (envProgram) glUseProgram(0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, 0);
  glDisable(GL_TEXTURE_2D);
  glDepthMask(GL_TRUE);
  glEnable(GL_DEPTH_TEST);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_BLEND);
  glEnable(GL_CULL_FACE);
  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
}

void Starfield::bindEnvTexture(GLuint tex) {
  // Always force unit 0 — never trust leftover active texture from smoke.
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  if (envProgram) {
    glUseProgram(envProgram->id);
    envProgram->bindTexture(0, GL_TEXTURE0, "uTex", tex);
  } else {
    glUseProgram(0);
    glEnable(GL_TEXTURE_2D);
  }
}

static void emitSphere(float cx, float cy, float cz, float radius,
                       int slices, int stacks,
                       bool heatColor, float corona, float timeSec) {
  for (int i = 0; i < stacks; i++) {
    float v0 = (float)i / stacks;
    float v1 = (float)(i + 1) / stacks;
    float phi0 = v0 * (float)M_PI;
    float phi1 = v1 * (float)M_PI;
    glBegin(GL_QUAD_STRIP);
    for (int j = 0; j <= slices; j++) {
      float u = (float)j / slices;
      float th = u * 2.f * (float)M_PI;
      for (int k = 0; k < 2; k++) {
        float phi = (k == 0) ? phi0 : phi1;
        float vv = (k == 0) ? v0 : v1;
        float x = cx + radius * std::sin(phi) * std::cos(th);
        float y = cy + radius * std::sin(phi) * std::sin(th);
        float z = cz + radius * std::cos(phi);
        if (heatColor) {
          float heat = 0.8f + 0.2f * corona + 0.08f * std::sin(timeSec * 2.2f + phi * 3.f);
          glColor4f(1.f, 0.5f + 0.2f * heat, 0.08f + 0.12f * corona,
                    0.92f + 0.08f * corona);
        } else {
          glColor4f(1.f, 1.f, 1.f, 1.f);
        }
        glTexCoord2f(u, vv);
        glVertex3f(x, y, z);
      }
    }
    glEnd();
  }
}

void Starfield::drawBackground(int width, int height) {
  (void)width; (void)height;
}

void Starfield::drawStarSphere(Camera* camera) {
  if (!ok || !texStar) return;
  beginEnvPass(camera);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  bindEnvTexture(texStar);

  // Smooth radius: base + continuous swell (no snap)
  const float cx = shake.x * 0.25f;
  const float cy = shake.y * 0.25f;
  const float cz = -28.f;
  const float R = 20.f + corona * 8.f; // 20..28 smooth
  emitSphere(cx, cy, cz, R, 40, 20, true, corona, time);

  endEnvPass();
}

void Starfield::drawCoronaBeams(Camera* camera) {
  if (!ok) return;
  // Untextured additive geometry — must not leave a sampler program bound.
  ncaResetPipelineState();
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glLoadMatrixf(camera->projectionMatrix.data());
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadMatrixf(camera->viewMatrix.data());

  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  glDepthMask(GL_FALSE);
  glUseProgram(0);

  const float intensity = 0.12f + corona * 0.75f;
  const int beams = 12;
  for (int i = 0; i < beams; i++) {
    float a = time * 0.25f + i * (2.f * (float)M_PI / beams);
    float wobble = 0.12f * std::sin(time * 1.4f + i);
    float bx = std::cos(a + wobble) * 2.f + shake.x;
    float by = std::sin(a + wobble) * 2.f + shake.y;
    glBegin(GL_TRIANGLES);
    glColor4f(1.f, 0.55f, 0.08f, 0.f);
    glVertex3f(bx * 8.f, by * 8.f, -20.f);
    glColor4f(1.f, 0.85f, 0.35f, intensity);
    glVertex3f(bx * 0.5f, by * 0.5f, 2.f + hoverZ);
    glColor4f(1.f, 0.4f, 0.05f, 0.f);
    glVertex3f(bx * 8.f + std::cos(a + 0.2f) * 3.f,
               by * 8.f + std::sin(a + 0.2f) * 3.f, -18.f);
    glEnd();
  }

  glBegin(GL_LINES);
  for (const auto& s : streaks) {
    float a = std::max(0.f, s.life / s.maxLife);
    glColor4f(0.7f, 0.85f, 1.f, a * 0.55f);
    glVertex3f(s.pos.x, s.pos.y, s.pos.z);
    glColor4f(1.f, 0.9f, 0.6f, 0.f);
    glVertex3f(s.pos.x - s.vel.x * 0.08f,
               s.pos.y - s.vel.y * 0.08f,
               s.pos.z - s.vel.z * 0.08f);
  }
  glEnd();

  glDepthMask(GL_TRUE);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
}

void Starfield::drawScreenFlare(int width, int height) {
  if (!ok || flare < 0.01f) return;
  (void)width; (void)height;
  ncaResetPipelineState();
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  // Additive warm wash — capped so it never flashbangs
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  float a = flare; // already 0..~0.42
  // Center-weighted: stronger bottom (star) softer top
  glBegin(GL_QUADS);
  glColor4f(1.f, 0.75f, 0.35f, a * 0.35f);
  glVertex2f(-1.f, 1.f); glVertex2f(1.f, 1.f);
  glColor4f(1.f, 0.55f, 0.15f, a * 0.85f);
  glVertex2f(1.f, -1.f); glVertex2f(-1.f, -1.f);
  glEnd();

  // Soft vignette brightening pulse at peak
  if (flare > 0.25f) {
    float peak = (flare - 0.25f) / 0.17f;
    peak = std::max(0.f, std::min(1.f, peak));
    glColor4f(1.f, 0.9f, 0.6f, peak * 0.12f);
    glBegin(GL_QUADS);
    glVertex2f(-1.f, -1.f); glVertex2f(1.f, -1.f);
    glVertex2f(1.f, 1.f); glVertex2f(-1.f, 1.f);
    glEnd();
  }

  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);
}

void Starfield::drawWorld(Camera* camera) {
  if (!ok) return;

  // Sky sphere: radius 240, slow spin
  float skyYaw = time * (3.5f / 3.f);
  if (texSpace) {
    beginEnvPass(camera);
    bindEnvTexture(texSpace);
    glPushMatrix();
    glRotatef(skyYaw, 0.f, 0.f, 1.f);
    emitSphere(0.f, 0.f, 0.f, 240.f, 48, 32, false, 0.f, time);
    glPopMatrix();
    endEnvPass();
  }

  drawStarSphere(camera);
  drawCoronaBeams(camera);
}
