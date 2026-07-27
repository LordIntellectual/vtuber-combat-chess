#include "../utils/math.hxx"

#include "Camera.hxx"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Camera::Camera(GLfloat screenRatio) {
  position.x = 0.0;
  position.y = - radius;
  position.z = 20.0;

  viewMatrix = getLookAtMatrix(position, center, up);
  projectionMatrix = getPerspectiveProjMatrix(
    fovy, screenRatio, 1, 1000
  );
}

void Camera::checkConstraints(){
  // Full 360° yaw: wrap to [-PI, PI] so angles stay finite (infinite spins OK)
  const GLfloat twoPi = static_cast<GLfloat>(2.0 * M_PI);
  while (phi > static_cast<GLfloat>(M_PI)) phi -= twoPi;
  while (phi < static_cast<GLfloat>(-M_PI)) phi += twoPi;

  // Pitch: stay above the board (no flip underneath). Kill residual velocity
  // into the stop so the view doesn't "chatter" against the clamp.
  if (theta > maxTheta) {
    theta = maxTheta;
    if (dTheta > 0.f) dTheta = 0.f;
  } else if (theta < minTheta) {
    theta = minTheta;
    if (dTheta < 0.f) dTheta = 0.f;
  }

  if (radius < minRadius) radius = minRadius;
  else if (radius > maxRadius) radius = maxRadius;
}

void Camera::computeViewMatrix(){
  // Orbit on a sphere around `center` (board origin or action-cam target)
  position.x = center.x + radius * std::sin(phi);
  position.y = center.y - radius * std::cos(phi) * std::cos(theta);
  position.z = center.z + heightBase + radius * std::sin(theta);

  viewMatrix = getLookAtMatrix(position, center, up);
}

void Camera::setOrbit(GLfloat phi_, GLfloat theta_, GLfloat radius_,
                      Vector3f center_, GLfloat heightBase_) {
  phi = phi_;
  theta = theta_;
  radius = radius_;
  center = center_;
  heightBase = heightBase_;
  checkConstraints();
  computeViewMatrix();
}

void Camera::updatePerspective(GLfloat screenRatio){
  // Recompute perspective matrix
  projectionMatrix = getPerspectiveProjMatrix(
    fovy, screenRatio, 1, 1000
  );
}

void Camera::setDragging(bool active) {
  dragging = active;
  if (active) {
    // Fresh drag: drop any leftover coast so the first pixels aren't a jump
    dPhi = 0.f;
    dTheta = 0.f;
  }
}

void Camera::move(GLfloat dX, GLfloat dY, GLfloat /*screenRatio*/){
  // Ignore zero deltas (caller should also filter, but be safe)
  if (dX == 0.f && dY == 0.f) return;

  // Same radians-per-pixel for yaw and pitch. (Old code multiplied only dX by
  // aspect ratio, so axes felt different; combined with re-applying stale dY
  // every frame, tilt looked jerky while orbit felt OK.)
  const GLfloat aPhi = -rotationSpeed * dX;
  // Mouse Y grows downward; positive dY → increase elevation (more top-down)
  const GLfloat aTheta = rotationSpeed * dY;

  phi += aPhi;
  theta += aTheta;

  // Seed inertia for a short coast after RMB release (not applied while dragging)
  dPhi = aPhi;
  dTheta = aTheta;

  checkConstraints();
  computeViewMatrix();
}

void Camera::zoom(GLfloat scrollY){
  if (scrollY == 0.f) return;
  // Scroll up (positive) → zoom in (smaller orbit radius)
  if (scrollY > 0.f) {
    radius *= zoomFactor;
  } else {
    radius /= zoomFactor;
  }
  checkConstraints();
  computeViewMatrix();
}

void Camera::update(){
  // While RMB is held, orbit is driven only by move() with fresh deltas.
  // Applying dPhi/dTheta again here was doubling every step and — worse —
  // when the mouse stopped, the last dY was re-injected every frame → jerky tilt.
  if (dragging) return;

  dPhi *= damping;
  dTheta *= damping;

  // Snap tiny residuals to zero so coast ends cleanly
  if (std::fabs(dPhi) < 1e-6f) dPhi = 0.f;
  if (std::fabs(dTheta) < 1e-6f) dTheta = 0.f;
  if (dPhi == 0.f && dTheta == 0.f) return;

  phi += dPhi;
  theta += dTheta;

  checkConstraints();
  computeViewMatrix();
}
