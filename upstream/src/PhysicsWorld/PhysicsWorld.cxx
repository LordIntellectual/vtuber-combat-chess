#include <vector>
#include <map>
#include <algorithm>
#include <random>

#include <GLFW/glfw3.h>

#include <btBulletDynamicsCommon.h>

#include "../mesh/Mesh.hxx"
#include "../mesh/meshes.hxx"
#include "../Event/Event.hxx"
#include "../Event/EventStack.hxx"
#include "../constants.hxx"

#include "PhysicsWorld.hxx"

PhysicsWorld::PhysicsWorld(
    std::map<int, std::vector<Mesh*>>* fragmentMeshes, ChessGame* game)
    : fragmentMeshes{fragmentMeshes}{
  // Create dynamics world
  broadphase = new btDbvtBroadphase();
  collisionConfiguration = new btDefaultCollisionConfiguration();
  dispatcher = new btCollisionDispatcher(collisionConfiguration);
  solver = new btSequentialImpulseConstraintSolver;
  dynamicsWorld = new btDiscreteDynamicsWorld(
    dispatcher,
    broadphase,
    solver,
    collisionConfiguration
  );

  // Set gravity
  dynamicsWorld->setGravity(btVector3(0, 0, -9.81));

  // Create the ground as a box (static — fragments bounce, board never moves):
  groundShape = new btBoxShape(btVector3(16, 16, 0.2));
  groundMotionState = new btDefaultMotionState(
    btTransform(btQuaternion(0, 0, 0, 1), btVector3(0, 0, -0.2)));
  btRigidBody::btRigidBodyConstructionInfo groundRigidBodyCI(
    0, groundMotionState, groundShape, btVector3(0, 0, 0));
  groundRigidBodyCI.m_friction = 0.7f;
  groundRigidBodyCI.m_restitution = 0.45f;
  groundRigidBody = new btRigidBody(groundRigidBodyCI);
  groundRigidBody->setRestitution(0.45f);
  dynamicsWorld->addRigidBody(groundRigidBody);

  // Create another ground as a 2D plane:
  limitGroundShape = new btStaticPlaneShape(btVector3(0, 0, 1), 0);
  limitGroundMotionState = new btDefaultMotionState(
    btTransform(btQuaternion(0, 0, 0, 1), btVector3(0, 0, -40)));
  btRigidBody::btRigidBodyConstructionInfo limitGroundRigidBodyCI(
    0, limitGroundMotionState, limitGroundShape, btVector3(0, 0, 0));
  limitGroundRigidBody = new btRigidBody(limitGroundRigidBodyCI);
  dynamicsWorld->addRigidBody(limitGroundRigidBody);

  // Create a cylinder rigid body for each piece on the board
  pieceShape = new btCylinderShapeZ(btVector3(1.6, 1.6, 7.5));
  for(int x = 0; x < 8; x++){
    for(int y = 0; y < 8; y++){
      if(game->boardAt(x, y) != EMPTY){
        btDefaultMotionState* motionState = new btDefaultMotionState(btTransform(
          btQuaternion(0, 0, 0, 1),
          btVector3(x * 4 - 14, y * 4 - 14, 3.75)
        ));

        pieceMotionStates.push_back(motionState);

        // Mass 0 = static collider. Fragments bounce off; intact pieces never move.
        btRigidBody::btRigidBodyConstructionInfo pieceRigidBodyCI(
          0, motionState, pieceShape, btVector3(0, 0, 0));
        pieceRigidBodyCI.m_restitution = 0.35f;
        pieceRigidBodyCI.m_friction = 0.6f;
        pieceRigidBodies[x][y] = new btRigidBody(pieceRigidBodyCI);
        pieceRigidBodies[x][y]->setRestitution(0.35f);
        pieceRigidBodies[x][y]->setCollisionFlags(
          pieceRigidBodies[x][y]->getCollisionFlags() |
          btCollisionObject::CF_STATIC_OBJECT);
        dynamicsWorld->addRigidBody(pieceRigidBodies[x][y]);
      } else {
        pieceRigidBodies[x][y] = NULL;
      }
    }
  }

  // Start the innerClock
  innerClock = new Clock();
};

void PhysicsWorld::updatePiecePosition(
    Vector2i startPosition, Vector2f currentPosition){
  if(startPosition.x < 0 || startPosition.x > 7 ||
     startPosition.y < 0 || startPosition.y > 7) return;
  btRigidBody* movingRigidBody = pieceRigidBodies[
    startPosition.x][startPosition.y];
  if(!movingRigidBody) return;

  // Move rigid body in the dynamics world
  btTransform transform(
    btQuaternion(0, 0, 0, 1),
    btVector3(
      currentPosition.x * 4 - 14,
      currentPosition.y * 4 - 14,
      3.75
    )
  );
  movingRigidBody->setWorldTransform(transform);
};

void PhysicsWorld::movePiece(
    Vector2i startPosition, Vector2i endPosition){
  if(startPosition.x < 0 || startPosition.x > 7 ||
     startPosition.y < 0 || startPosition.y > 7 ||
     endPosition.x < 0 || endPosition.x > 7 ||
     endPosition.y < 0 || endPosition.y > 7) return;

  // Move the piece to its end position
  updatePiecePosition(
    startPosition, {float(endPosition.x), float(endPosition.y)});

  // And update the rigid bodies grid
  btRigidBody* movingRigidBody = pieceRigidBodies[
    startPosition.x][startPosition.y];
  if(!movingRigidBody) return;
  pieceRigidBodies[startPosition.x][startPosition.y] = NULL;
  // If something already sits on end (shouldn't for legal moves), drop it
  if(pieceRigidBodies[endPosition.x][endPosition.y] &&
     pieceRigidBodies[endPosition.x][endPosition.y] != movingRigidBody){
    dynamicsWorld->removeRigidBody(pieceRigidBodies[endPosition.x][endPosition.y]);
    delete pieceRigidBodies[endPosition.x][endPosition.y];
  }
  pieceRigidBodies[endPosition.x][endPosition.y] = movingRigidBody;
};

void PhysicsWorld::clearFragments(){
  for (unsigned int i = 0; i < fragmentPool.size(); i++) {
    Fragment* fragment = fragmentPool.at(i).second;
    if (!fragment) continue;
    dynamicsWorld->removeRigidBody(fragment->rigidBody);
    delete fragment;
  }
  fragmentPool.clear();
}

void PhysicsWorld::resyncBoard(ChessGame* game){
  clearFragments();

  // Remove existing static piece colliders
  for (int x = 0; x < 8; x++) {
    for (int y = 0; y < 8; y++) {
      if (pieceRigidBodies[x][y]) {
        dynamicsWorld->removeRigidBody(pieceRigidBodies[x][y]);
        delete pieceRigidBodies[x][y];
        pieceRigidBodies[x][y] = NULL;
      }
    }
  }
  for (unsigned int i = 0; i < pieceMotionStates.size(); i++)
    delete pieceMotionStates.at(i);
  pieceMotionStates.clear();

  // Recreate from current board (mass 0 static)
  for (int x = 0; x < 8; x++) {
    for (int y = 0; y < 8; y++) {
      if (game->boardAt(x, y) == EMPTY) {
        pieceRigidBodies[x][y] = NULL;
        continue;
      }
      btDefaultMotionState* motionState = new btDefaultMotionState(btTransform(
        btQuaternion(0, 0, 0, 1),
        btVector3(x * 4 - 14, y * 4 - 14, 3.75)
      ));
      pieceMotionStates.push_back(motionState);
      btRigidBody::btRigidBodyConstructionInfo pieceRigidBodyCI(
        0, motionState, pieceShape, btVector3(0, 0, 0));
      pieceRigidBodyCI.m_restitution = 0.35f;
      pieceRigidBodyCI.m_friction = 0.6f;
      pieceRigidBodies[x][y] = new btRigidBody(pieceRigidBodyCI);
      pieceRigidBodies[x][y]->setRestitution(0.35f);
      pieceRigidBodies[x][y]->setCollisionFlags(
        pieceRigidBodies[x][y]->getCollisionFlags() |
        btCollisionObject::CF_STATIC_OBJECT);
      dynamicsWorld->addRigidBody(pieceRigidBodies[x][y]);
    }
  }
}

void PhysicsWorld::collapsePiece(int piece, Vector2i position, float forceScale){
  // Remove standing collider for the victim (static mass-0 cylinder)
  if (pieceRigidBodies[position.x][position.y]) {
    dynamicsWorld->removeRigidBody(pieceRigidBodies[position.x][position.y]);
    delete pieceRigidBodies[position.x][position.y];
    pieceRigidBodies[position.x][position.y] = NULL;
  }

  int absPiece = abs(piece);
  if (!fragmentMeshes || fragmentMeshes->find(absPiece) == fragmentMeshes->end() ||
      fragmentMeshes->at(absPiece).empty()) {
    return;
  }

  GLfloat rotation = piece > 0 ? -90.f : 90.f;
  // Persist several seconds on the board
  std::uniform_real_distribution<float> lifeDist(5.5f, 8.0f);
  std::uniform_real_distribution<float> U(-1.f, 1.f);
  std::uniform_real_distribution<float> U01(0.f, 1.f);

  const float kScale = 2.0f; // match main PIECE_SCALE
  std::vector<Fragment*> spawned;
  spawned.reserve(fragmentMeshes->at(absPiece).size());

  for (unsigned int i = 0; i < fragmentMeshes->at(absPiece).size(); i++) {
    GLfloat lifetime = lifeDist(generator);
    Fragment* fragment = new Fragment(
      fragmentMeshes->at(absPiece).at(i), position, rotation, lifetime, kScale);
    fragmentPool.push_back(std::make_pair(piece, fragment));
    dynamicsWorld->addRigidBody(fragment->rigidBody);
    spawned.push_back(fragment);
  }

  if (spawned.empty()) return;

  // Explosion centroid: single point at the piece COM (board cell + height)
  btVector3 centroid(0, 0, 0);
  for (Fragment* f : spawned) centroid += f->worldCenter();
  centroid /= (btScalar)spawned.size();
  // Prefer a clean board-space centroid so force is always "from the heart"
  btVector3 blastOrigin(
    position.x * 4.f - 14.f,
    position.y * 4.f - 14.f,
    2.2f
  );
  // Blend mesh COM with board centre so awkward origins still look good
  blastOrigin = blastOrigin * 0.45f + centroid * 0.55f;

  // Outward impulse from blastOrigin — dramatic, chunky, not vaporous
  for (Fragment* f : spawned) {
    btVector3 pos = f->worldCenter();
    btVector3 dir = pos - blastOrigin;
    if (dir.length2() < 1e-5f) {
      dir = btVector3(U(generator), U(generator), U01(generator) * 0.8f + 0.2f);
    }
    // Bias upward so chunks leap off the board then bounce
    dir.setZ(dir.getZ() + 0.55f * dir.length());
    if (dir.length2() < 1e-6f) dir = btVector3(0, 0, 1);
    dir.normalize();

    // Base blast (forceScale from Gameplay settings; 1 = default)
    float fs = forceScale;
    if (fs < 0.f) fs = 0.f;
    if (fs > 2.f) fs = 2.f; // slider allows up to 200% of default

    float power = (11.f + U01(generator) * 19.f) * fs;
    float mass = std::max(0.15f, f->mass);
    btVector3 impulse = dir * power * mass;
    // Extra vertical pop
    impulse += btVector3(0, 0, (4.f + U01(generator) * 7.f) * mass * fs);

    f->rigidBody->activate(true);
    f->rigidBody->applyCentralImpulse(impulse);
    // Spin scales with force
    btVector3 torque(
      U(generator) * 6.f * mass * fs,
      U(generator) * 6.f * mass * fs,
      U(generator) * 4.f * mass * fs
    );
    f->rigidBody->applyTorqueImpulse(torque);
  }
};

void PhysicsWorld::simulate(){
  float timeSinceLastCall = innerClock->getElapsedTime();

  // Take into account fragments lifetime
  for(unsigned int i = 0; i < fragmentPool.size(); i++){
    Fragment* fragment = fragmentPool.at(i).second;
    fragment->lifetime -= timeSinceLastCall;

    if(fragment->lifetime <= 0.0){
      // Generate smoke particles where the fragment disapeared
      btTransform trans;
      fragment->rigidBody->getMotionState()->getWorldTransform(trans);

      // Trigger fragment disappears event
      Event event;
      event.type = Event::FragmentDisappearsEvent;
      event.fragment.position = {
        trans.getOrigin().getX(),
        trans.getOrigin().getY(),
        trans.getOrigin().getZ()
      };
      event.fragment.volume = fragment->mass;
      event.fragment.piece = fragmentPool.at(i).first;
      EventStack::pushEvent(event);

      dynamicsWorld->removeRigidBody(fragment->rigidBody);
      delete fragment;
      fragmentPool.at(i).second = NULL;
    }
  }

  // Remove fragments which lifetime is over from the fragment pool
  fragmentPool.erase(
    std::remove_if(
      fragmentPool.begin(), fragmentPool.end(),
      [](std::pair<int, Fragment*> x){
        return x.second == NULL;
      }
    ),
    fragmentPool.end()
  );

  // Simulate the dynamics world
  dynamicsWorld->stepSimulation(timeSinceLastCall, 7);
  innerClock->restart();
};

PhysicsWorld::~PhysicsWorld(){
  // Delete rigid bodies
  for(unsigned int i = 0; i < fragmentPool.size(); i++)
    delete fragmentPool.at(i).second;
  fragmentPool.clear();

  // Delete ground
  delete groundShape;
  delete groundMotionState;
  delete groundRigidBody;

  // Delete cylinders
  delete pieceShape;
  for(unsigned int i = 0; i < pieceMotionStates.size(); i++)
    delete pieceMotionStates.at(i);
  for(int x = 0; x < 8; x++)
    for(int y = 0; y < 8; y++)
      if(pieceRigidBodies[x][y]) delete pieceRigidBodies[x][y];

  // Delete dynamics world
  delete dynamicsWorld;
  delete solver;
  delete dispatcher;
  delete collisionConfiguration;
  delete broadphase;

  // Delete clock
  delete innerClock;
}
