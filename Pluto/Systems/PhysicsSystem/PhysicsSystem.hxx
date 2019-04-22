
#pragma once

#include "Pluto/Tools/System.hxx"
#include "Pluto/Libraries/GLFW/GLFW.hxx"

#include <queue>

#include "btBulletDynamicsCommon.h"

class Camera;
class Texture;
class Transform;

namespace Systems 
{
    class PhysicsSystem : public System {
        public:
            static PhysicsSystem* Get();
            bool initialize();
            bool start();
            bool stop();
        private:
            btDefaultCollisionConfiguration* collisionConfiguration;
            btCollisionDispatcher* dispatcher;
            btBroadphaseInterface* overlappingPairCache;
            btSequentialImpulseConstraintSolver* solver;
            btDiscreteDynamicsWorld* dynamicsWorld;

            btAlignedObjectArray<btCollisionShape*> collisionShapes;
            
            PhysicsSystem();
            ~PhysicsSystem();
    };   
}
