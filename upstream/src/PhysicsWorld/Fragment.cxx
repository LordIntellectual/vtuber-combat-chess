#include <math.h>
#include <algorithm>

#include <BulletCollision/CollisionShapes/btShapeHull.h>
#include <BulletCollision/CollisionShapes/btConvexHullShape.h>

#include "../utils/math.hxx"

#include "Fragment.hxx"

Fragment::Fragment(
    Mesh* mesh, Vector2i position, GLfloat rotation, GLfloat lifetime,
    GLfloat visualScale)
    : mesh{mesh}, lifetime{lifetime}, visualScale{visualScale} {
  // Create a simplified version of the original mesh for optimization purpose
  btConvexHullShape* originalConvexHullShape = new btConvexHullShape();
  for (unsigned int i = 0; i < mesh->vertices.size() / 3; i++) {
    originalConvexHullShape->addPoint(btVector3(
      mesh->vertices.at(3 * i),
      mesh->vertices.at(3 * i + 1),
      mesh->vertices.at(3 * i + 2)
    ));
  }
  hull = new btShapeHull(originalConvexHullShape);
  btScalar margin = originalConvexHullShape->getMargin();
  hull->buildHull(margin);
  convexHullShape = new btConvexHullShape(
    (btScalar*)hull->getVertexPointer(), hull->numVertices());
  delete originalConvexHullShape;

  // Match intact piece display scale so chunks fill the same volume
  const btScalar s = (btScalar)visualScale;
  convexHullShape->setLocalScaling(btVector3(s, s, s));
  convexHullShape->setMargin(0.04f);

  // Compute inertia of the shape
  mass = std::max(0.15f, mesh->mass);
  origin = mesh->origin;
  inertia = btVector3(0, 0, 0);
  convexHullShape->calculateLocalInertia(mass, inertia);

  // Create the motion state — board cell + scaled local origin
  btTransform initialPosition = btTransform(
    btQuaternion(btVector3(0, 0, 1), rotation * M_PI / 180.),
    btVector3(
      position.x * 4 - 14,
      position.y * 4 - 14,
      0
    )
  );
  initialPosition *= btTransform(
    btQuaternion::getIdentity(),
    btVector3(
      origin.x * visualScale,
      origin.y * visualScale,
      origin.z * visualScale
    )
  );
  motionState = new btDefaultMotionState(initialPosition);

  // Rigid body: bouncy, low damping so explosion carries
  btRigidBody::btRigidBodyConstructionInfo fallRigidBodyCI(
    mass,
    motionState,
    convexHullShape,
    inertia
  );
  fallRigidBodyCI.m_friction = 0.55f;
  fallRigidBodyCI.m_restitution = 0.72f;
  fallRigidBodyCI.m_linearDamping = 0.08f;
  fallRigidBodyCI.m_angularDamping = 0.12f;
  rigidBody = new btRigidBody(fallRigidBodyCI);
  rigidBody->setActivationState(DISABLE_DEACTIVATION);
  rigidBody->setCcdMotionThreshold(0.5f);
  rigidBody->setCcdSweptSphereRadius(0.25f * visualScale);
}

btVector3 Fragment::worldCenter() const {
  btTransform t;
  rigidBody->getMotionState()->getWorldTransform(t);
  // Also pull live body transform if stepped
  t = rigidBody->getWorldTransform();
  return t.getOrigin();
}

std::vector<GLfloat> Fragment::getMoveMatrix() {
  btTransform transform;
  rigidBody->getMotionState()->getWorldTransform(transform);
  // Prefer live transform after simulation step
  transform = rigidBody->getWorldTransform();
  btScalar _matrix[16];
  transform.getOpenGLMatrix(_matrix);
  std::vector<GLfloat> matrix(
    _matrix,
    _matrix + sizeof _matrix / sizeof _matrix[0]
  );

  // Scale mesh in local space to match collision scaling
  if (visualScale != 1.0f) {
    std::vector<GLfloat> S = getIdentityMatrix();
    S = scale(&S, visualScale);
    matrix = matrixProduct(&matrix, &S);
  }
  return matrix;
}

Fragment::~Fragment() {
  delete convexHullShape;
  delete hull;
  delete motionState;
  delete rigidBody;
}
