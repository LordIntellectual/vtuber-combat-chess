#include "ScreenShake.hxx"
#include <cmath>
#include <algorithm>

ScreenShake::ScreenShake()
  : remaining(0.f), duration(2.f), strength(1.f),
    current(0, 0, 0), vel(0, 0, 0) {
  rng.seed(90210);
}

void ScreenShake::trigger(float seconds, float str) {
  duration = std::max(0.05f, seconds);
  remaining = duration;
  strength = std::max(0.f, str);
  // Kick velocity for an immediate jolt
  std::uniform_real_distribution<float> U(-1.f, 1.f);
  vel.x += U(rng) * 18.f * strength;
  vel.y += U(rng) * 18.f * strength;
  vel.z += U(rng) * 8.f * strength;
}

void ScreenShake::update(float dt) {
  if (dt <= 0.f || dt > 0.1f) dt = 0.016f;
  if (remaining <= 0.f) {
    current.x *= 0.8f;
    current.y *= 0.8f;
    current.z *= 0.8f;
    if (std::fabs(current.x) < 1e-4f) current.x = 0.f;
    if (std::fabs(current.y) < 1e-4f) current.y = 0.f;
    if (std::fabs(current.z) < 1e-4f) current.z = 0.f;
    vel = {0, 0, 0};
    return;
  }

  remaining -= dt;
  float t = std::max(0.f, remaining / duration); // 1 → 0
  // Extreme at start, still strong for first half, then falls off
  float envelope = t * t * (0.35f + 0.65f * t);
  float mag = strength * 2.8f * envelope;

  std::uniform_real_distribution<float> U(-1.f, 1.f);
  // Target noise + spring toward it (violent rattle)
  Vector3f target = {
    U(rng) * mag,
    U(rng) * mag,
    U(rng) * mag * 0.55f
  };
  // Spring-damper
  vel.x += (target.x - current.x) * 90.f * dt;
  vel.y += (target.y - current.y) * 90.f * dt;
  vel.z += (target.z - current.z) * 90.f * dt;
  vel.x *= (1.f - 8.f * dt);
  vel.y *= (1.f - 8.f * dt);
  vel.z *= (1.f - 8.f * dt);
  current.x += vel.x * dt;
  current.y += vel.y * dt;
  current.z += vel.z * dt;

  // Clamp so we don't throw the board off-screen
  float maxOff = 2.2f * strength;
  current.x = std::max(-maxOff, std::min(maxOff, current.x));
  current.y = std::max(-maxOff, std::min(maxOff, current.y));
  current.z = std::max(-maxOff * 0.6f, std::min(maxOff * 0.6f, current.z));
}
