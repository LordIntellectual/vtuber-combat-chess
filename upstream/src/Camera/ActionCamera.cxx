#include "ActionCamera.hxx"
#include "../ChessGame/ChessGame.hxx"
#include <random>
#include "../PhysicsWorld/PhysicsWorld.hxx"
#include "../constants.hxx"
#include <cmath>
#include <algorithm>
#include <iostream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

ActionCamera::ActionCamera()
  : enabled_(true), state_(Idle),
    blend_(0.f), engageDur_(0.28f), returnDur_(0.55f),
    holdTimer_(0.f), maxTimer_(0.f),
    sawCaptureEvent_(false), pieceLanded_(false),
    followTarget_(0, 0, 0), endWorld_(0, 0, 0) {}

void ActionCamera::setEnabled(bool e) {
  enabled_ = e;
  if (!e) state_ = Idle;
}

Vector3f ActionCamera::boardToWorld(float bx, float by, float bz) {
  return {bx * 4.f - 14.f, by * 4.f - 14.f, bz};
}

float ActionCamera::lerp(float a, float b, float t) {
  return a + (b - a) * t;
}

float ActionCamera::lerpAngle(float a, float b, float t) {
  float d = b - a;
  while (d > (float)M_PI) d -= 2.f * (float)M_PI;
  while (d < -(float)M_PI) d += 2.f * (float)M_PI;
  return a + d * t;
}

ActionCamera::Pose ActionCamera::lerpPose(const Pose& a, const Pose& b, float t) {
  t = std::max(0.f, std::min(1.f, t));
  // Smoothstep
  float s = t * t * (3.f - 2.f * t);
  Pose o;
  o.phi = lerpAngle(a.phi, b.phi, s);
  o.theta = lerp(a.theta, b.theta, s);
  o.radius = lerp(a.radius, b.radius, s);
  o.heightBase = lerp(a.heightBase, b.heightBase, s);
  o.center.x = lerp(a.center.x, b.center.x, s);
  o.center.y = lerp(a.center.y, b.center.y, s);
  o.center.z = lerp(a.center.z, b.center.z, s);
  return o;
}

ActionCamera::Pose ActionCamera::readPose(const Camera* cam) {
  Pose p;
  p.phi = cam->getPhi();
  p.theta = cam->getTheta();
  p.radius = cam->getRadius();
  p.heightBase = cam->getHeightBase();
  p.center = cam->getCenter();
  return p;
}

void ActionCamera::applyPose(Camera* cam, const Pose& p) {
  cam->setOrbit(p.phi, p.theta, p.radius, p.center, p.heightBase);
}

ActionCamera::Pose ActionCamera::makeActionPose(const ChessGame* game,
                                                Vector3f target) {
  Pose p;
  p.center = target;
  p.radius = 14.f;
  p.theta = 0.32f;
  p.heightBase = 3.5f;

  float dx = (float)(game->movingPieceEndPosition.x - game->movingPieceStartPosition.x);
  float dy = (float)(game->movingPieceEndPosition.y - game->movingPieceStartPosition.y);
  // Place camera behind the piece looking along travel (see Camera::computeViewMatrix)
  if (std::fabs(dx) < 1e-4f && std::fabs(dy) < 1e-4f) {
    p.phi = 0.f;
  } else {
    p.phi = std::atan2(-dx, dy);
  }
  return p;
}

void ActionCamera::beginCapture(Camera* cam, const ChessGame* game) {
  if (!enabled_ || !cam || !game) return;
  if (state_ != Idle) return;
  if (game->movingPiece == EMPTY) return;

  saved_ = readPose(cam);
  // Always restore to board-centred overview with the pre-capture angles/zoom
  saved_.center = {0.f, 0.f, 0.f};
  if (saved_.heightBase < 10.f) saved_.heightBase = 20.f;

  followTarget_ = boardToWorld(
    game->movingPiecePosition.x, game->movingPiecePosition.y, 1.4f);
  endWorld_ = boardToWorld(
    (float)game->movingPieceEndPosition.x,
    (float)game->movingPieceEndPosition.y, 1.4f);

  engageFrom_ = readPose(cam);
  engageTo_ = makeActionPose(game, followTarget_);

  blend_ = 0.f;
  holdTimer_ = 0.f;
  maxTimer_ = 0.f;
  sawCaptureEvent_ = false;
  pieceLanded_ = false;
  state_ = Engaging;
  cam->clearInertia();

  std::cout << "[ActionCam] Capture cam engaged\n";
}

void ActionCamera::onPieceTaken() {
  if (state_ == Idle) return;
  sawCaptureEvent_ = true;
}

void ActionCamera::cancel(Camera* cam) {
  if (state_ == Idle) return;
  if (cam) applyPose(cam, saved_);
  state_ = Idle;
  sawCaptureEvent_ = false;
  pieceLanded_ = false;
}

void ActionCamera::finishReturn(Camera* cam) {
  applyPose(cam, saved_);
  state_ = Idle;
  sawCaptureEvent_ = false;
  pieceLanded_ = false;
  std::cout << "[ActionCam] Restored overview\n";
}

void ActionCamera::update(Camera* cam, const ChessGame* game,
                          const PhysicsWorld* physics, float dt) {
  if (!cam) return;
  if (!enabled_) {
    if (state_ != Idle) cancel(cam);
    return;
  }
  if (state_ == Idle) return;

  if (dt <= 0.f || dt > 0.1f) dt = 0.016f;
  maxTimer_ += dt;
  // Safety: never stick forever
  if (maxTimer_ > 6.f) {
    finishReturn(cam);
    return;
  }

  const bool moving = game && (
    game->getState() == USER_MOVING ||
    game->getState() == AI_MOVING ||
    game->getState() == BLACK_MOVING);

  if (moving && game->movingPiece != EMPTY) {
    followTarget_ = boardToWorld(
      game->movingPiecePosition.x, game->movingPiecePosition.y, 1.4f);
    endWorld_ = boardToWorld(
      (float)game->movingPieceEndPosition.x,
      (float)game->movingPieceEndPosition.y, 1.4f);
    pieceLanded_ = false;
  } else if (!moving) {
    pieceLanded_ = true;
    followTarget_ = endWorld_;
  }

  (void)physics; // destruction VFX no longer gate the return

  if (state_ == Engaging) {
    blend_ += dt / engageDur_;
    engageTo_.center = followTarget_;
    float t = std::min(1.f, blend_);
    applyPose(cam, lerpPose(engageFrom_, engageTo_, t));
    if (t >= 1.f) {
      state_ = Following;
      applyPose(cam, engageTo_);
    }
    return;
  }

  if (state_ == Following) {
    // Centre on the capturer; leave phi/theta/radius alone so RMB/zoom work
    Vector3f c = followTarget_;
    c.z = 1.2f;
    cam->setCenter(c);
    cam->computeViewMatrix();

    // After the move lands, linger ~0.5s on the taking piece, then pull back.
    // (Do not wait for destruction debris — that felt too long.)
    if (!moving) {
      holdTimer_ += dt;
      if (holdTimer_ >= 0.5f) {
        engageFrom_ = readPose(cam);
        blend_ = 0.f;
        state_ = Returning;
        cam->clearInertia();
      }
    } else {
      holdTimer_ = 0.f;
    }
    return;
  }

  if (state_ == Returning) {
    blend_ += dt / returnDur_;
    float t = std::min(1.f, blend_);
    applyPose(cam, lerpPose(engageFrom_, saved_, t));
    if (t >= 1.f) finishReturn(cam);
  }
}
