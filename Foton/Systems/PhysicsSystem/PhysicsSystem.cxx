#include "PhysicsSystem.hxx"

#include "Foton/Entity/Entity.hxx"
#include "Foton/RigidBody/RigidBody.hxx"
#include "Foton/Collider/Collider.hxx"
#include "Foton/Constraint/Constraint.hxx"
#include "Foton/Transform/Transform.hxx"

#include "Foton/Libraries/GLFW/GLFW.hxx"

#include "BulletCollision/NarrowPhaseCollision/btRaycastCallback.h"

struct PhysicsObject {
    btRigidBody* rigid_body;
    btCollisionShape* shape;
    btScalar mass;
    btVector3 scale;
};

struct ConstraintObject {
    btGeneric6DofSpring2Constraint* constraint;
};

namespace Systems 
{
    PhysicsSystem* PhysicsSystem::Get() {
        static PhysicsSystem instance;
        return &instance;
    }

    bool PhysicsSystem::initialize() {
        using namespace Libraries;
        if (initialized) return false;

        contact_mutex = std::make_shared<std::mutex>();
        edit_mutex = std::make_shared<std::mutex>();

        initialized = true;
        return true;
    }

    std::shared_ptr<std::mutex> PhysicsSystem::get_contact_mutex()
    {
        if (initialized == false)
            throw std::runtime_error( std::string("Error: Uninitialized, can't get contact mutex."));
        
        return contact_mutex;
    }

    std::shared_ptr<std::mutex> PhysicsSystem::get_edit_mutex()
    {
        if (initialized == false)
            throw std::runtime_error( std::string("Error: Uninitialized, can't get edit mutex."));
        
        return edit_mutex;
    }

    bool PhysicsSystem::does_entity_have_physics(uint32_t entity_id)
    {
        auto it = physicsObjectMap.find(entity_id);
        return (it != physicsObjectMap.end());
    }

    bool PhysicsSystem::should_entity_have_physics(uint32_t entity_id)
    {
        Entity* entities = Entity::GetFront();
        uint32_t entity_count = Entity::GetCount();

        if (entity_id >= entity_count) throw std::runtime_error("Error: Invalid entity id");
        
        /* The entity must be valid, and must have a transform, a rigid body, and a collider component */
        auto &entity = entities[entity_id];
        if (!entity.is_initialized()) return false;

        auto transform = entity.get_transform();
        auto rigid_body = entity.get_rigid_body();
        auto collider = entity.get_collider();

        if (!collider) return false;
        if (!rigid_body) return false;
        if (!transform) return false;

        return true;
    }

    void PhysicsSystem::add_entity_to_simulation(uint32_t entity_id)
    {
        Entity* entities = Entity::GetFront();
        auto &entity = entities[entity_id];

        auto transform = entity.get_transform();
        auto rigid_body = entity.get_rigid_body();
        auto collider = entity.get_collider();

        if (!collider) throw std::runtime_error("Error, collider was null during add entity to physics simulation");
        if (!rigid_body) throw std::runtime_error("Error, rigid body was null during add entity to physics simulation");
        if (!transform) throw std::runtime_error("Error, transform was null during add entity to physics simulation");

        btCollisionShape *shape = collider->get_collision_shape();

        glm::vec3 origin = transform->get_world_translation();
        glm::quat rotation = transform->get_world_rotation();

        btTransform bullet_transform;
        bullet_transform.setOrigin(btVector3(origin[0], origin[1], origin[2]));
        bullet_transform.setRotation(btQuaternion(rotation.x, rotation.y, rotation.z, rotation.w));

        btScalar bullet_mass = btScalar(rigid_body->get_mass());

        btVector3 local_inertia;
        shape->calculateLocalInertia(bullet_mass, local_inertia);

        //using motion state is optional, it provides interpolation capabilities, and only synchronizes 'active' objects
        btDefaultMotionState* motionState = new btDefaultMotionState(bullet_transform);
        btRigidBody::btRigidBodyConstructionInfo rbInfo(bullet_mass, motionState, shape, local_inertia);
        btRigidBody* body = new btRigidBody(rbInfo);

        PhysicsObject obj;
        obj.rigid_body = body;
        obj.mass = bullet_mass;
        obj.shape = shape;
        obj.scale = shape->getLocalScaling();
        physicsObjectMap[entity_id] = obj;
        rigidBodyToEntity[body] = entity_id;

        // add the body to the dynamics world
        dynamicsWorld->addRigidBody(body);
    }

    void PhysicsSystem::remove_entity_from_simulation(uint32_t entity_id)
    {
        auto obj = physicsObjectMap[entity_id];
        physicsObjectMap.erase(entity_id);
        rigidBodyToEntity.erase(obj.rigid_body);

        // remove the rigid body from the dynamic world and delete it.
        // btCollisionObject* obj = dynamicsWorld->getCollisionObjectArray()[i];
        // btRigidBody* body = btRigidBody::upcast(obj);

        if (obj.rigid_body && obj.rigid_body->getMotionState())
        {
            delete obj.rigid_body->getMotionState();
        }
        dynamicsWorld->removeCollisionObject((btCollisionObject*)obj.rigid_body);
        delete obj.rigid_body;
    }

    void PhysicsSystem::update_entity(uint32_t entity_id)
    {
        Entity* entities = Entity::GetFront();
        RigidBody* rigidBodies = RigidBody::GetFront();
        Transform* transforms = Transform::GetFront();
        Collider* colliders = Collider::GetFront();

        auto &entity = entities[entity_id];

        auto transform = entity.get_transform();
        auto rigid_body = entity.get_rigid_body();
        auto collider = entity.get_collider();

        if (!collider) throw std::runtime_error("Error, collider was null during update in physics simulation");
        if (!rigid_body) throw std::runtime_error("Error, rigid body was null during update in physics simulation");
        if (!transform) throw std::runtime_error("Error, transform was null during update in physics simulation");

        auto &obj = physicsObjectMap[entity_id];

        if (rigid_body->is_kinematic() )
        {
            btTransform worldTrans;

            glm::vec3 position = transform->get_world_translation();
            glm::quat rotation = transform->get_world_rotation();

            worldTrans.setOrigin(btVector3(position.x, position.y, position.z));
            worldTrans.setRotation(btQuaternion(rotation.x, rotation.y, rotation.z, rotation.w));
            if (obj.rigid_body && obj.rigid_body->getMotionState())
            {
                obj.rigid_body->getMotionState()->setWorldTransform(worldTrans);
            }
            else {
                obj.rigid_body->setWorldTransform(worldTrans);
            }
        }
        else {
            btTransform trans;
            if (obj.rigid_body && obj.rigid_body->getMotionState())
            {
                obj.rigid_body->getMotionState()->getWorldTransform(trans);
            } else {
                trans = obj.rigid_body->getWorldTransform();
            }

            glm::vec3 position = glm::vec3(
                (float) trans.getOrigin().getX(), 
                (float) trans.getOrigin().getY(), 
                (float) trans.getOrigin().getZ());

            btQuaternion bullet_rotation = trans.getRotation();

            glm::quat rotation = glm::quat();
            rotation.x = bullet_rotation.x();
            rotation.y = bullet_rotation.y();
            rotation.z = bullet_rotation.z();
            rotation.w = bullet_rotation.w();

            transform->set_position(position);
            transform->set_rotation(rotation);
        }

        // Did the mass change?
        if (obj.mass != rigid_body->get_mass()) 
        {
            btScalar bullet_mass = btScalar(rigid_body->get_mass());
            btVector3 local_inertia;
            collider->get_collision_shape()->calculateLocalInertia(bullet_mass, local_inertia);
            obj.rigid_body->setMassProps(bullet_mass, local_inertia);

            obj.mass = rigid_body->get_mass();
        }

        // Did the shape or scale change?
        else if ((obj.shape != collider->get_collision_shape()) 
            || (obj.scale != collider->get_collision_shape()->getLocalScaling())) 
        {
            btScalar bullet_mass = btScalar(rigid_body->get_mass());
            btVector3 local_inertia;
            collider->get_collision_shape()->calculateLocalInertia(bullet_mass, local_inertia);
            obj.rigid_body->setCollisionShape(collider->get_collision_shape());
            obj.rigid_body->setMassProps(bullet_mass, local_inertia);
            obj.shape = collider->get_collision_shape();
            obj.scale = collider->get_collision_shape()->getLocalScaling();
        }

        // need to also check and see if any of the following changed... 
        // Right now, reference counters are incrementing like crazy

        // body->setDamping(0.0, 0.0);
        obj.rigid_body->setFriction(rigid_body->get_friction());
        obj.rigid_body->setRollingFriction(rigid_body->get_rolling_friction());

        /* Not sure if these can be changed per frame... */
        if (rigid_body->is_kinematic()) {
            obj.rigid_body->setCollisionFlags( obj.rigid_body->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
            obj.rigid_body->setCollisionFlags( obj.rigid_body->getCollisionFlags() & ~btCollisionObject::CF_STATIC_OBJECT);
            obj.rigid_body->forceActivationState( DISABLE_DEACTIVATION );
        }
        else if (rigid_body->is_static()) {
            obj.rigid_body->forceActivationState( ACTIVE_TAG );
            obj.rigid_body->setCollisionFlags( obj.rigid_body->getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);
            obj.rigid_body->setCollisionFlags( obj.rigid_body->getCollisionFlags() & ~btCollisionObject::CF_KINEMATIC_OBJECT);
        } else {
            obj.rigid_body->forceActivationState( ACTIVE_TAG );
            obj.rigid_body->setCollisionFlags( obj.rigid_body->getCollisionFlags() & ~btCollisionObject::CF_KINEMATIC_OBJECT);
            obj.rigid_body->setCollisionFlags( obj.rigid_body->getCollisionFlags() & ~btCollisionObject::CF_STATIC_OBJECT);
        }
        
        /* Lots of fields we could be setting */
        //body->setActivationState();
        //body->setAngularFactor();
        // body->setAngularVelocity();
        // body->setAnisotropicFriction();
        // body->setBroadphaseHandle();
        // body->setCollisionShape();
        // body->setHitFraction()
        // body->set()
    }

    bool PhysicsSystem::should_constraint_exist(uint32_t constraint_id)
    {
        // A constraint should exist when:
        // 1. the constraint id is valid
        // 2. the constraint component has two associated entities
        // 3. the associated entities are registered in the simulation

        if ((constraint_id < 0 ) || (constraint_id >= MAX_CONSTRAINTS))
            return false;
        
        auto constraint = Constraint::Get(constraint_id);
        if ((!constraint) || (!constraint->is_initialized())) 
            return false;

        auto entity_a = constraint->get_entity_a();
        auto entity_b = constraint->get_entity_b();

        if ((!entity_a) || (!entity_b)) 
            return false;
        
        if ((!entity_a->is_initialized()) || (!entity_b->is_initialized()))
            return false;

        if (!does_entity_have_physics(entity_a->get_id())) return false;
        if (!does_entity_have_physics(entity_b->get_id())) return false;
        
        return true;
    }

    bool PhysicsSystem::does_constraint_exist(uint32_t constraint_id)
    {
        auto it = constraintMap.find(constraint_id);
        return (it != constraintMap.end());
    }

    void PhysicsSystem::add_constraint_to_simulation(uint32_t constraint_id)
    {
        auto constraint = Constraint::Get(constraint_id);
        if (!constraint) 
            throw std::runtime_error("Error, constraint was null in add constraint to physics simulation");

        auto entity_a = constraint->get_entity_a();
        auto entity_b = constraint->get_entity_b();

        if ((!entity_a) || (!entity_b))
            throw std::runtime_error("Error, constrained entity was null in add constraint to physics simulation");

        if ((!entity_a->is_initialized()) || (!entity_b->is_initialized()))
            throw std::runtime_error("Error, constrained entity was uninitialized in add constraint to physics simulation");

        if ((!does_entity_have_physics(entity_a->get_id())) 
            || (!does_entity_have_physics(entity_b->get_id()))) 
            throw std::runtime_error("Error, constrained entity not currently registered in the physics simulation");

        auto A = physicsObjectMap[entity_a->get_id()];
        auto B = physicsObjectMap[entity_b->get_id()];

        // auto frame_a = constraint->get_a_frame();
        // auto frame_b = constraint->get_b_frame();

        btTransform btFrameInA;
        btTransform btFrameInB;
        btFrameInA.setIdentity();
        btFrameInB.setIdentity();

        ConstraintObject obj;
        obj.constraint = new btGeneric6DofSpring2Constraint(*A.rigid_body, *B.rigid_body, btFrameInA, btFrameInB);

        constraintMap[constraint_id] = obj;
        dynamicsWorld->addConstraint((btTypedConstraint*)obj.constraint);

    }

    void PhysicsSystem::remove_constraint_from_simulation(uint32_t constraint_id)
    {
        auto obj = constraintMap[constraint_id];
        constraintMap.erase(constraint_id);

        // remove the rigid body from the dynamic world and delete it.
        // btCollisionObject* obj = dynamicsWorld->getCollisionObjectArray()[i];
        // btRigidBody* body = btRigidBody::upcast(obj);

        dynamicsWorld->removeConstraint((btTypedConstraint*)obj.constraint);
        delete obj.constraint;
    }

    void PhysicsSystem::update_constraint(uint32_t constraint_id)
    {
        auto obj = constraintMap[constraint_id];
        auto constraint = Constraint::Get(constraint_id);
        
        if (!constraint) throw std::runtime_error("Error, constraint was null in update constraint simulation");
        
        btTransform btFrameInA;
        btTransform btFrameInB;
        btFrameInA.setIdentity();
        btFrameInB.setIdentity();
        auto alof = constraint->get_a_linear_offset();
        auto blof = constraint->get_b_linear_offset();

        auto aaof = constraint->get_a_angular_offset();
        auto baof = constraint->get_b_angular_offset();

        btFrameInA.setOrigin(btVector3(alof.x, alof.y, alof.z));
        btFrameInB.setOrigin(btVector3(blof.x, blof.y, blof.z));

        btFrameInA.setRotation(btQuaternion(aaof.x, aaof.y, aaof.z, aaof.w));
        btFrameInB.setRotation(btQuaternion(baof.x, baof.y, baof.z, baof.w));
        
        obj.constraint->setFrames(btFrameInA, btFrameInB);
        
        auto lll = constraint->get_lower_linear_limit();
        auto ull = constraint->get_upper_linear_limit();

        auto lal = constraint->get_lower_angular_limit();
        auto ual = constraint->get_upper_angular_limit();
        
        obj.constraint->setLinearLowerLimit(btVector3(lll.x, lll.y, lll.z));
        obj.constraint->setLinearUpperLimit(btVector3(ull.x, ull.y, ull.z));
        obj.constraint->setAngularLowerLimit(btVector3(lal.x, lal.y, lal.z));
        obj.constraint->setAngularUpperLimit(btVector3(ual.x, ual.y, ual.z));

        for (uint32_t i = 0; i < 6; ++i)
        {
            obj.constraint->enableSpring(i, constraint->is_spring_enabled(i));
            obj.constraint->setBounce(i, (constraint->is_bounce_enabled(i) ? 1.0f : 0.0f));
            glm::vec3 stiffness;
            if (i < 3) stiffness = constraint->get_linear_spring_stiffness();
            else stiffness = constraint->get_angular_spring_stiffness();
            obj.constraint->setStiffness(i, stiffness[i % 3]);
        }

        obj.constraint->setEnabled(constraint->is_enabled());
    }

    int32_t PhysicsSystem::raycast(glm::vec3 position, glm::vec3 direction, float distance)
    {
        if (dynamicsWorld == 0) return -1;
        direction = glm::normalize(direction);
        glm::vec3 to = distance * direction + position;

        btVector3 rayFromWorld = btVector3(position.x, position.y, position.z);
        btVector3 rayToWorld = btVector3(to.x, to.y, to.z);
        btCollisionWorld::ClosestRayResultCallback rayCallback(rayFromWorld, rayToWorld);

        rayCallback.m_flags |= btTriangleRaycastCallback::kF_UseGjkConvexCastRaytest;
        dynamicsWorld->rayTest(rayFromWorld, rayToWorld, rayCallback);

        if(rayCallback.hasHit())
        {
            btVector3 pickPos = rayCallback.m_hitPointWorld;
            btVector3 pickNorm = rayCallback.m_hitNormalWorld;
            btRigidBody* body = (btRigidBody*) btRigidBody::upcast(rayCallback.m_collisionObject);
            auto result = rigidBodyToEntity.find(body);
            if (result != rigidBodyToEntity.end()) {
                return result->second;
            }
        }

        return -1;
    }

    // int32_t PhysicsSystem::test_contact(uint32_t entity_id)
    // {
    //     if (!does_entity_have_physics(entity_id)) return -1;
    //     auto obj = physicsObjectMap[entity_id];

    //     btCollisionWorld::ContactResultCallback callback;
    //     dynamicsWorld->contactTest((btCollisionObject*)obj.rigid_body, callback);

    //     callback.
    // }

    std::vector<uint32_t> PhysicsSystem::get_contacting_entities(uint32_t entity_id)
    {
        auto mutex = contact_mutex.get();
        std::lock_guard<std::mutex> lock(*mutex);
        std::vector<uint32_t> result;
        /* If entity isn't in contact map, add it. */
        if (contactMap.find(entity_id) == contactMap.end()) {
            return result;
        }
        result = contactMap[entity_id];
        return result;
    }

    bool PhysicsSystem::start() {
        /* Dont start unless initialized. Dont start twice. */
        if (!initialized) return false;
        if (running) return false;

        auto loop = [this](future<void> futureObj) {
            // collision configuration contains default setup for memory, collision setup. 
            // advanced users can create their own configuration.
            collisionConfiguration = new btDefaultCollisionConfiguration();

            // use the default collision dispatcher. For parallel processing you can use a 
            // different dispatcher (See ExtrasBulletMultithreaded)
            dispatcher = new btCollisionDispatcher(collisionConfiguration);

            // btDbvhBroadphase is a good general purpose broadphase. You can also try out btAxis3Sweep.
            overlappingPairCache = new btDbvtBroadphase();

            // The default constraint solver. For parallel processing you can use a different solver (See Extras/BulletMultiThreaded)
            solver = new btSequentialImpulseConstraintSolver();

            dynamicsWorld = new btDiscreteDynamicsWorld(dispatcher, overlappingPairCache, solver, collisionConfiguration);

            dynamicsWorld->setGravity(btVector3(0.f, 0.f, -9.8f));

            double lastTime, currentTime;
            lastTime = glfwGetTime();

            while (true)
            {
                if (futureObj.wait_for(.1ms) == future_status::ready) break;

                currentTime = glfwGetTime();
                
                dynamicsWorld->stepSimulation((btScalar)(currentTime - lastTime), 100, 1.0f / 120.f);
                lastTime = currentTime;

                /* Add or remove rigid bodies from the dynamic world */
                Entity* entities = Entity::GetFront();
                Constraint* constraints = Constraint::GetFront();
                
                {
                    auto mutex = edit_mutex.get();
                    std::lock_guard<std::mutex> lock(*mutex);

                    // Maintain registered entities (rigid bodies and collider components)
                    for (uint32_t entity_id = 0; entity_id < Entity::GetCount(); ++entity_id )
                    {
                        if (!entities[entity_id].is_initialized()) continue;

                        bool does_have_physics = does_entity_have_physics(entity_id);
                        bool should_have_physics = should_entity_have_physics(entity_id);

                        /* Note, update is before add to sim, so that all new entities step through 
                        the simulation first. I get strange bugs with bullet otherwise... */
                        if (does_have_physics) {
                            update_entity(entity_id);
                        }

                        if (should_have_physics && !does_have_physics)
                        {
                            add_entity_to_simulation(entity_id);
                        }
                        
                        if (does_have_physics && !should_have_physics) {
                            remove_entity_from_simulation(entity_id);
                        }

                    }

                    // Maintain registered constraints (rigid bodies and constraint components)
                    for (uint32_t constraint_id = 0; constraint_id < Constraint::GetCount(); ++constraint_id)
                    {
                        if (!constraints[constraint_id].is_initialized()) continue;

                        bool does_exist = does_constraint_exist(constraint_id);
                        bool should_exist = should_constraint_exist(constraint_id);

                        if (does_exist)
                        {
                            update_constraint(constraint_id);
                        }

                        if (should_exist && !does_exist)
                        {
                            add_constraint_to_simulation(constraint_id);
                        }

                        if (does_exist && !should_exist)
                        {
                            remove_constraint_from_simulation(constraint_id);
                        }
                    }

                }


                // Mark collisions 
                std::unordered_map<uint32_t, std::vector<uint32_t>> newContactMap;

                uint32_t numManifolds = dispatcher->getNumManifolds();
                for (uint32_t i = 0; i < numManifolds; ++i)
                {
                    btPersistentManifold* contactManifold = dispatcher->getManifoldByIndexInternal(i);
                    if (contactManifold->getNumContacts() > 0)
                    {
                        btCollisionObject* obA = const_cast<btCollisionObject*>(contactManifold->getBody0());
                        btCollisionObject* obB = const_cast<btCollisionObject*>(contactManifold->getBody1());
                        btRigidBody* bodyA = btRigidBody::upcast(obA);
                        btRigidBody* bodyB = btRigidBody::upcast(obB);
                        uint32_t entityA = rigidBodyToEntity[bodyA];
                        uint32_t entityB = rigidBodyToEntity[bodyB];

                        /* Register contacting entities */
                        newContactMap[entityB].push_back(entityA);
                        newContactMap[entityA].push_back(entityB);
                    }
                }

                {
                    auto mutex = contact_mutex.get();
                    std::lock_guard<std::mutex> lock(*mutex);
                    contactMap = newContactMap;
                }
            }

            // cleanup in the reverse order of creation/initialization

            // remove the constraints from the dynamics world and delete them
            for (int32_t i = dynamicsWorld->getNumConstraints() - 1; i >= 0; i--)
            {
                btTypedConstraint* constraint = dynamicsWorld->getConstraint(i);
                dynamicsWorld->removeConstraint(constraint);
                delete constraint;
            }
            constraintMap.clear();

            // remove the rigid bodies from the dynamics world and delete them
            for (int32_t i = dynamicsWorld->getNumCollisionObjects() - 1; i >= 0; i--)
            {
                btCollisionObject* obj = dynamicsWorld->getCollisionObjectArray()[i];
                btRigidBody* body = btRigidBody::upcast(obj);
                if (body && body->getMotionState())
                {
                    delete body->getMotionState();
                }
                dynamicsWorld->removeCollisionObject(obj);
                delete obj;
            }
            physicsObjectMap.clear();

            // // delete collision shapes
            // for (uint32_t j = 0; j < collisionShapes.size(); j++)
            // {
            //     btCollisionShape* shape = collisionShapes[j];
            //     collisionShapes[j] = 0;
            //     delete shape;
            // }

            // delete dynamics world
            delete dynamicsWorld;

            // delete solver
            delete solver;

            // delete broadphase
            delete overlappingPairCache;

            // delete dispatcher
            delete dispatcher;

            // collisionShapes.clear();
        };

        exitSignal = promise<void>();
        future<void> futureObj = exitSignal.get_future();
        eventThread = thread(loop, move(futureObj));

        running = true;
        return true;
    }

    bool PhysicsSystem::stop() {
        if (!initialized) return false;
        if (!running) return false;

        exitSignal.set_value();
        eventThread.join();

        running = false;
        return true;
    }

    PhysicsSystem::PhysicsSystem() {}
    PhysicsSystem::~PhysicsSystem() {
    }
}