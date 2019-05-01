
#pragma once

#include "Pluto/Tools/System.hxx"
#include "Pluto/Libraries/GLFW/GLFW.hxx"

#include <queue>
#include <map>

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
            bool does_entity_have_physics(uint32_t entity_id);
            bool should_entity_have_physics(uint32_t entity_id);
            void add_physics_to_entity(uint32_t entity_id);
            void remove_physics_from_entity(uint32_t entity_id);
            void update_entity(uint32_t entity_id);

            btDefaultCollisionConfiguration* collisionConfiguration;
            btCollisionDispatcher* dispatcher;
            btBroadphaseInterface* overlappingPairCache;
            btSequentialImpulseConstraintSolver* solver;
            btDiscreteDynamicsWorld* dynamicsWorld;

            std::map<uint32_t, btRigidBody*> rigidBodyMap;
            
            PhysicsSystem();
            ~PhysicsSystem();
    };   
}
