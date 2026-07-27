#ifndef CAMERA_HXX_
#define CAMERA_HXX_

#include <vector>
#include <cmath>

#include <GLFW/glfw3.h>

#include "../utils/math.hxx"

class Camera {
private:
  /* Camera field of view */
  GLint fovy = 50;

  /* Position of the camera in the space coordinates */
  Vector3f position;

  /* Look-at / orbit centre (board origin by default; piece while action cam) */
  Vector3f center = {0.0, 0.0, 0.0};

  /* Camera up vector */
  Vector3f up = {0.0, 0.0, 1.0};

  /* Rotation radius of the camera (orbit distance / zoom) */
  GLfloat radius = 40.0;
  GLfloat minRadius = 8.0;
  GLfloat maxRadius = 160.0;

  /* Rotation angle around Z axis (yaw) — free 360°, wraps for stability */
  GLfloat phi = 0.0;

  /* Elevation angle (pitch) — limited so the view stays above the board */
  GLfloat theta = 0.0;
  GLfloat minTheta = 0.0f;
  GLfloat maxTheta = 1.45f; // ~83° — almost top-down, no flip under board

  /* Base height above orbit centre (was fixed board elevation) */
  GLfloat heightBase = 20.0f;

  /* Rotation speed in radians per pixel (same for yaw and pitch) */
  GLfloat rotationSpeed = 0.003f;

  /* Zoom: multiplicative factor per scroll notch */
  GLfloat zoomFactor = 0.92f;

  /* Inertia after releasing RMB (applied only when not dragging) */
  GLfloat dPhi = 0.0;
  GLfloat dTheta = 0.0;
  GLfloat damping = 0.88f;
  bool dragging = false;

  void checkConstraints();

public:
  /* Constructor */
  explicit Camera(GLfloat screenRatio);

  void computeViewMatrix();

  /* Update the camera perspective matrix */
  void updatePerspective(GLfloat screenRatio);

  /* Begin/end RMB orbit drag (stops inertia re-apply during drag) */
  void setDragging(bool active);
  bool isDragging() const { return dragging; }

  /* Orbit camera from this frame's mouse delta only (pixels). Call once per
     unique cursor event, then clear the delta in the caller. */
  void move(GLfloat dX, GLfloat dY, GLfloat screenRatio);

  /* Zoom in/out from mouse-wheel scroll (positive y = zoom in) */
  void zoom(GLfloat scrollY);

  /* Coast with damping when not dragging; no-op while dragging */
  void update();

  /* Orbit parameters (action camera / save-restore) */
  GLfloat getPhi() const { return phi; }
  GLfloat getTheta() const { return theta; }
  GLfloat getRadius() const { return radius; }
  GLfloat getHeightBase() const { return heightBase; }
  Vector3f getCenter() const { return center; }
  Vector3f getPosition() const { return position; }

  void setPhi(GLfloat v) { phi = v; checkConstraints(); }
  void setTheta(GLfloat v) { theta = v; checkConstraints(); }
  void setRadius(GLfloat v) { radius = v; checkConstraints(); }
  void setHeightBase(GLfloat v) { heightBase = v; }
  void setCenter(Vector3f c) { center = c; }

  void setOrbit(GLfloat phi_, GLfloat theta_, GLfloat radius_,
                Vector3f center_, GLfloat heightBase_ = 20.f);

  void clearInertia() { dPhi = 0.f; dTheta = 0.f; }

  /* Camera view matrix */
  std::vector<GLfloat> viewMatrix;

  /* Camera projection matrix */
  std::vector<GLfloat> projectionMatrix;

  /* Destructor */
  ~Camera(){};
};

#endif
