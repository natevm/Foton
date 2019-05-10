#include "PhysicsSystem.hxx"

#include "Pluto/Entity/Entity.hxx"
#include "Pluto/RigidBody/RigidBody.hxx"
#include "Pluto/Collider/Collider.hxx"
#include "Pluto/Constraint/Constraint.hxx"
#include "Pluto/Transform/Transform.hxx"

#include "Pluto/Libraries/GLFW/GLFW.hxx"

namespace Systems 
{
    PhysicsSystem* PhysicsSystem::Get() {
        static PhysicsSystem instance;
        return &instance;
    }

    bool PhysicsSystem::initialize() {
        using namespace Libraries;
        if (initialized) return false;
        initialized = true;
        return true;
    }

    bool PhysicsSystem::does_entity_have_physics(uint32_t entity_id)
    {
        auto it = rigidBodyMap.find(entity_id);
        return (it != rigidBodyMap.end());
    }

    bool PhysicsSystem::should_entity_have_physics(uint32_t entity_id)
    {
        Entity* entities = Entity::GetFront();
        uint32_t entity_count = Entity::GetCount();

        if (entity_id >= entity_count) throw std::runtime_error("Error: Invalid entity id");
        
        /* The entity must be valid, and must have a transform and rigid body component */
        auto &entity = entities[entity_id];
        if (!entity.is_initialized()) return false;

        auto transform_id = entity.get_transform();
        auto rigid_body_id = entity.get_rigid_body();
        
        RigidBody* rigidBodies = RigidBody::GetFront();
        Transform* transforms = Transform::GetFront();

        uint32_t rigid_body_count = RigidBody::GetCount();
        uint32_t transform_count = Transform::GetCount();

        if ((rigid_body_id < 0) || ((uint32_t)rigid_body_id >= rigid_body_count)) return false;
        if ((transform_id < 0) || ((uint32_t)transform_id >= transform_count)) return false;

        auto &transform = transforms[transform_id];
        auto &rigid_body = rigidBodies[rigid_body_id];

        if (!transform.is_initialized()) return false;
        if (!rigid_body.is_initialized()) return false;

        /* The rigid body component must have an associated collider */

        auto collider = rigid_body.get_collider();
        if (collider == nullptr) return false;
        if (!collider->is_initialized()) return false;

        return true;
    }

    void PhysicsSystem::add_entity_to_simulation(uint32_t entity_id)
    {
        Entity* entities = Entity::GetFront();
        RigidBody* rigidBodies = RigidBody::GetFront();
        Transform* transforms = Transform::GetFront();

        auto &entity = entities[entity_id];
        auto &transform = transforms[entity.get_transform()];
        auto &rigidBody = rigidBodies[entity.get_rigid_body()];

        btCollisionShape *shape = rigidBody.get_collider()->get_collision_shape();

        glm::vec3 origin = transform.get_world_translation();
        glm::quat rotation = transform.get_world_rotation();

        btTransform bullet_transform;
        bullet_transform.setOrigin(btVector3(origin[0], origin[1], origin[2]));
        bullet_transform.setRotation(btQuaternion(rotation.x, rotation.y, rotation.z, rotation.w));

        btScalar bullet_mass = btScalar(rigidBody.get_mass());

        glm::vec3 local_inertia = rigidBody.get_local_inertia();
        btVector3 bullet_local_inertia = btVector3(local_inertia.x, local_inertia.y, local_inertia.z);

        //using motion state is optional, it provides interpolation capabilities, and only synchronizes 'active' objects
        btDefaultMotionState* motionState = new btDefaultMotionState(bullet_transform);
        btRigidBody::btRigidBodyConstructionInfo rbInfo(bullet_mass, motionState, shape, bullet_local_inertia);
        btRigidBody* body = new btRigidBody(rbInfo);

        // body->setDamping(0.0, 0.0);
        body->setFriction(rigidBody.get_friction());
        body->setRollingFriction(rigidBody.get_rolling_friction());
        body->setSpinningFriction(rigidBody.get_spinning_friction());

        rigidBodyMap[entity_id] = body;

        /* TODO: do this each frame */
        if (rigidBody.is_kinematic()) {
            body->setCollisionFlags( btCollisionObject::CF_KINEMATIC_OBJECT);
            body->setActivationState( DISABLE_DEACTIVATION );
        }

        else if (rigidBody.is_static()) {
            body->setCollisionFlags( btCollisionObject::CF_STATIC_OBJECT);
        }

        // add the body to the dynamics world
        dynamicsWorld->addRigidBody(body);
    }

    void PhysicsSystem::remove_entity_from_simulation(uint32_t entity_id)
    {
        auto body = rigidBodyMap[entity_id];
        rigidBodyMap.erase(entity_id);

        // remove the rigid body from the dynamic world and delete it.
        // btCollisionObject* obj = dynamicsWorld->getCollisionObjectArray()[i];
        // btRigidBody* body = btRigidBody::upcast(obj);

        if (body && body->getMotionState())
        {
            delete body->getMotionState();
        }
        dynamicsWorld->removeCollisionObject((btCollisionObject*)body);
        delete body;
    }

    void PhysicsSystem::update_entity(uint32_t entity_id)
    {
        Entity* entities = Entity::GetFront();
        RigidBody* rigidBodies = RigidBody::GetFront();
        Transform* transforms = Transform::GetFront();

        auto &entity = entities[entity_id];
        auto &transform = transforms[entity.get_transform()];
        auto &rigidBody = rigidBodies[entity.get_rigid_body()];

        auto &body = rigidBodyMap[entity_id];

        // body->setDamping(0.0, 0.0);
        body->setFriction(rigidBody.get_friction());
        body->setRollingFriction(rigidBody.get_rolling_friction());

        /* Not sure if these can be changed per frame... */
        if (rigidBody.is_kinematic()) {
            body->setCollisionFlags( body->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
            body->setCollisionFlags( body->getCollisionFlags() & ~btCollisionObject::CF_STATIC_OBJECT);
            body->forceActivationState( DISABLE_DEACTIVATION );
        }
        else if (rigidBody.is_static()) {
            body->forceActivationState( ACTIVE_TAG );
            body->setCollisionFlags( body->getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);
            body->setCollisionFlags( body->getCollisionFlags() & ~btCollisionObject::CF_KINEMATIC_OBJECT);
        } else {
            body->forceActivationState( ACTIVE_TAG );
            body->setCollisionFlags( body->getCollisionFlags() & ~btCollisionObject::CF_KINEMATIC_OBJECT);
            body->setCollisionFlags( body->getCollisionFlags() & ~btCollisionObject::CF_STATIC_OBJECT);
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

        if (rigidBody.is_kinematic() )
        {
            btTransform worldTrans;

            glm::vec3 position = transform.get_world_translation();
            glm::quat rotation = transform.get_world_rotation();

            worldTrans.setOrigin(btVector3(position.x, position.y, position.z));
            worldTrans.setRotation(btQuaternion(rotation.x, rotation.y, rotation.z, rotation.w));
            if (body && body->getMotionState())
            {
                body->getMotionState()->setWorldTransform(worldTrans);
            }
            else {
                body->setWorldTransform(worldTrans);
            }
        }
        else {
            btTransform trans;
            if (body && body->getMotionState())
            {
                body->getMotionState()->getWorldTransform(trans);
            } else {
                trans = body->getWorldTransform();
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

            transform.set_position(position);
            transform.set_rotation(rotation);
        }
    }

    bool PhysicsSystem::should_constraint_exist(uint32_t constraint_id)
    {
        // A constraint should exist when:
        // 1. the constraint id is valid
        // 2. the constraint component has two associated rigid bodies
        // 3. the associated rigid bodies are registered in the simulation
        return false;
    }

    bool PhysicsSystem::does_constraint_exist(uint32_t constraint_id)
    {
        return false;
    }

    void PhysicsSystem::add_constraint_to_simulation(uint32_t constraint_id)
    {

    }

    void PhysicsSystem::remove_constraint_from_simulation(uint32_t constraint_id)
    {

    }

    void PhysicsSystem::update_constraint(uint32_t constraint_id)
    {

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

            dynamicsWorld->setGravity(btVector3(0, 0, -9.8));

            // // Create a few basic rigid bodies
            
            // // the ground is a cube of side 100 at position y = -56
            // // the sphere will hit it at y = -6, with center at -5
            // {
            //     btCollisionShape* groundShape = new btBoxShape(btVector3(btScalar(50.), btScalar(50.), btScalar(50.)));

            //     collisionShapes.push_back(groundShape);

            //     btTransform groundTransform;
            //     groundTransform.setIdentity();
            //     groundTransform.setOrigin(btVector3(0, -56, 0));

            //     btScalar mass(0.);

            //     //rigidbody is dynamic if and only if mass is non zero, otherwise static
            //     bool isDynamic = (mass != 0.f);

            //     btVector3 localInertia(0, 0, 0);
            //     if (isDynamic)
            //         groundShape->calculateLocalInertia(mass, localInertia);
                
            //     //using motion state is optional, it provides interpolation capabilities, and only synchronizes 'active' objects
            //     btDefaultMotionState* myMotionState = new btDefaultMotionState(groundTransform);
            //     btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, myMotionState, groundShape, localInertia);
            //     btRigidBody* body = new btRigidBody(rbInfo);

            //     // add the body to the dynamics world
            //     dynamicsWorld->addRigidBody(body);
            // }

            // {
            //     // Create a dynamic rigidbody

            //     //btCollisionShape* colShape = new btBoxShape(btVector3(1,1,1));
            //     btCollisionShape* colShape = new btSphereShape(btScalar(1.));
            //     collisionShapes.push_back(colShape);

            //     // Create dynamic objects
            //     btTransform startTransform;
            //     startTransform.setIdentity();

            //     btScalar mass(1.0f);

            //     // rigid body is dynamic if and only if mass is non zero, otherwise static
            //     bool isDynamic = (mass != 0.f);

            //     btVector3 localInertia(0,0,0);
            //     if (isDynamic)
            //         colShape->calculateLocalInertia(mass, localInertia);

            //     startTransform.setOrigin(btVector3(2, 10, 0));

            //     //using motionstate is recommended, it provides interpolation capabilities, and only synchronizes 'active' objects
            //     btDefaultMotionState* myMotionState = new btDefaultMotionState(startTransform);
            //     btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, myMotionState, colShape, localInertia);
            //     btRigidBody* body = new btRigidBody(rbInfo);

            //     dynamicsWorld->addRigidBody(body);
            // }
            double lastTime, currentTime;
            lastTime = glfwGetTime();

            while (true)
            {
                if (futureObj.wait_for(.1ms) == future_status::ready) break;

                currentTime = glfwGetTime();
                
                dynamicsWorld->stepSimulation((currentTime - lastTime), 100, 1.0 / 120.f);
                lastTime = currentTime;

                /* Add or remove rigid bodies from the dynamic world */

                Entity* entities = Entity::GetFront();
                Constraint* constraints = Constraint::GetFront();
                
                // Handle entities (rigid bodies and collider components)
                for (uint32_t entity_id = 0; entity_id < Entity::GetCount(); ++entity_id )
                {
                    if (!entities[entity_id].is_initialized()) continue;

                    bool does_have_physics = does_entity_have_physics(entity_id);
                    bool should_have_physics = should_entity_have_physics(entity_id);

                    if (should_have_physics && !does_have_physics)
                    {
                        add_entity_to_simulation(entity_id);
                    }
                    
                    if (does_have_physics) {
                        update_entity(entity_id);
                    }

                    if (does_have_physics && !should_have_physics) {
                        remove_entity_from_simulation(entity_id);
                    }
                }

                // Handle constraints (rigid bodies and constraint components)
                for (uint32_t constraint_id = 0; constraint_id < Constraint::GetCount(); ++constraint_id)
                {
                    if (!constraints[constraint_id].is_initialized()) continue;

                    bool does_exist = does_constraint_exist(constraint_id);
                    bool should_exist = should_constraint_exist(constraint_id);

                    if (should_exist && !does_exist)
                    {
                        add_constraint_to_simulation(constraint_id);
                    }

                    if (does_exist)
                    {
                        update_constraint(constraint_id);
                    }

                    if (does_exist && !should_exist)
                    {
                        remove_constraint_from_simulation(constraint_id);
                    }
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
            rigidBodyMap.clear();

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