#ifndef FRAGMENT_HXX_
#define FRAGMENT_HXX_

#include <btBulletDynamicsCommon.h>
#include <BulletCollision/CollisionShapes/btShapeHull.h>

#include "../utils/math.hxx"

#include "../mesh/Mesh.hxx"


// cppcheck-suppress noCopyConstructor
class Fragment {
  public:
    /* Constructor
       visualScale matches intact piece render scale (board uses 2× pieces). */
    explicit Fragment(
      Mesh* mesh, Vector2i position, GLfloat rotation, GLfloat lifetime,
      GLfloat visualScale = 2.0f);

    /* Shape */
    btShapeHull* hull;
    btConvexHullShape* convexHullShape;

    /* Motion State */
    btDefaultMotionState* motionState;

    /* Rigid body */
    btRigidBody* rigidBody;

    /* Mesh */
    Mesh* mesh;

    /* Lifetime before the fragment disappear, in seconds */
    GLfloat lifetime;

    btVector3 inertia;
    float mass;

    /* Origin of the fragment (local, unscaled mesh space) */
    Vector3f origin;

    /* Visual / collision scale vs raw mesh */
    GLfloat visualScale;

    /* Returns the movement matrix of the Fragment
      \return movement matrix as a vector of GLfloat
    */
    std::vector<GLfloat> getMoveMatrix();

    /* World-space centre of mass (for explosion radial force). */
    btVector3 worldCenter() const;

    /* Destructor */
    ~Fragment();
};

#endif
