#ifndef NCA_ACTION_CAMERA_HXX_
#define NCA_ACTION_CAMERA_HXX_

#include "Camera.hxx"
#include "../utils/math.hxx"

class ChessGame;
class PhysicsWorld;

/**
 * Capture-only cinematic camera: snaps close behind the capturer facing the
 * direction of travel, follows through the move, holds ~0.5s after landing,
 * then returns to the saved overview. Orbit / zoom remain usable while
 * following (centred on the taking piece).
 */
class ActionCamera {
public:
  enum State {
    Idle = 0,
    Engaging,
    Following,
    Returning
  };

  ActionCamera();

  bool enabled() const { return enabled_; }
  void setEnabled(bool e);
  bool isActive() const { return state_ != Idle; }
  State state() const { return state_; }

  /** Call when a capture move begins (moving piece already set on game). */
  void beginCapture(Camera* cam, const ChessGame* game);

  /**
   * Per-frame update. Call after game/physics step, after optional user
   * orbit/zoom so follow centre is applied last.
   */
  void update(Camera* cam, const ChessGame* game, const PhysicsWorld* physics,
              float dt);

  /** Notify that a PieceTakenEvent fired (destruction in progress). */
  void onPieceTaken();

  /** Force cancel and restore saved camera immediately. */
  void cancel(Camera* cam);

private:
  struct Pose {
    float phi = 0.f;
    float theta = 0.f;
    float radius = 40.f;
    float heightBase = 20.f;
    Vector3f center = {0, 0, 0};
  };

  bool enabled_;
  State state_;

  Pose saved_;
  Pose engageFrom_;
  Pose engageTo_;

  float blend_;       // 0..1 for engage/return
  float engageDur_;
  float returnDur_;
  float holdTimer_;   // after land before return if no fragments
  float maxTimer_;    // safety timeout

  bool sawCaptureEvent_;
  bool pieceLanded_;
  Vector3f followTarget_;
  Vector3f endWorld_; // destination world pos (stable after move fields clear)

  static Vector3f boardToWorld(float bx, float by, float bz = 1.2f);
  static float lerp(float a, float b, float t);
  static float lerpAngle(float a, float b, float t);
  static Pose lerpPose(const Pose& a, const Pose& b, float t);
  static Pose readPose(const Camera* cam);
  static void applyPose(Camera* cam, const Pose& p);
  static Pose makeActionPose(const ChessGame* game, Vector3f target);
  void finishReturn(Camera* cam);
};

#endif
