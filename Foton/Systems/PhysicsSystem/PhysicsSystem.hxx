
#pragma once

#include "Foton/Tools/System.hxx"
#include "Foton/Libraries/GLFW/GLFW.hxx"

#include <queue>
#include <map>
#include <unordered_map>

#include "btBulletDynamicsCommon.h"

class Camera;
class Texture;
class Transform;

struct PhysicsObject;
struct ConstraintObject;

namespace Systems 
{
    class PhysicsSystem : public System {
        public:
            static PhysicsSystem* Get();
            bool initialize();
            bool start();
            bool stop();
            
            int32_t raycast(glm::vec3 position, glm::vec3 direction, float distance);
            // int32_t test_contact(uint32_t entity_id);
            std::vector<uint32_t> get_contacting_entities(uint32_t entity_id);

            std::shared_ptr<std::mutex> get_contact_mutex();
            std::shared_ptr<std::mutex> get_edit_mutex();

        private:
            std::shared_ptr<std::mutex> contact_mutex;
            std::shared_ptr<std::mutex> edit_mutex;

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

            std::map<btRigidBody*, uint32_t> rigidBodyToEntity;
            std::map<uint32_t, PhysicsObject> physicsObjectMap;
            std::map<uint32_t, ConstraintObject> constraintMap;

            std::unordered_map<uint32_t, std::vector<uint32_t>> contactMap;
            
            PhysicsSystem();
            ~PhysicsSystem();
    };   
}
