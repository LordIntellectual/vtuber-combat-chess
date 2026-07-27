#ifndef NCA_SCREEN_SHAKE_HXX_
#define NCA_SCREEN_SHAKE_HXX_

#include "../utils/math.hxx"
#include <random>

/** Violent capture / explosion camera-board shake. */
class ScreenShake {
public:
  ScreenShake();

  /** Start (or re-trigger) a shake lasting `seconds` at `strength` (1 = extreme). */
  void trigger(float seconds = 2.0f, float strength = 1.0f);

  void update(float dt);

  /** World-space offset to add to board / look target. */
  Vector3f offset() const { return current; }

  bool active() const { return remaining > 0.f; }

private:
  float remaining;
  float duration;
  float strength;
  Vector3f current;
  Vector3f vel;
  std::default_random_engine rng;
};

#endif
