
#pragma once

#include "Pluto/Tools/System.hxx"
#include "Pluto/Libraries/GLFW/GLFW.hxx"

#include <queue>
#include <map>

#include "btBulletDynamicsCommon.h"

class Camera;
class Texture;
class Transform;

struct PhysicsObject;

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
            void add_entity_to_simulation(uint32_t entity_id);
            void remove_entity_from_simulation(uint32_t entity_id);
            void update_entity(uint32_t entity_id);

            bool should_constraint_exist(uint32_t constraint_id);
            bool does_constraint_exist(uint32_t constraint_id);
            void add_constraint_to_simulation(uint32_t constraint_id);
            void remove_constraint_from_simulation(uint32_t constraint_id);
            void update_constraint(uint32_t constraint_id);

            btDefaultCollisionConfiguration* collisionConfiguration;
            btCollisionDispatcher* dispatcher;
            btBroadphaseInterface* overlappingPairCache;
            btSequentialImpulseConstraintSolver* solver;
            btDiscreteDynamicsWorld* dynamicsWorld;

            std::map<uint32_t, PhysicsObject> physicsObjectMap;
            std::map<uint32_t, btGeneric6DofSpring2Constraint> constraintMap;
            
            PhysicsSystem();
            ~PhysicsSystem();
    };   
}
