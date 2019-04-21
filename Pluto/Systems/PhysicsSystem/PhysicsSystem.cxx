#include "PhysicsSystem.hxx"

namespace Systems 
{
    PhysicsSystem* PhysicsSystem::Get() {
        static PhysicsSystem instance;
        return &instance;
    }

    bool PhysicsSystem::initialize() {
        using namespace Libraries;
        if (initialized) return false;
        
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

        dynamicsWorld->setGravity(btVector3(0, -10, 0));

        initialized = true;
        return true;
    }

    bool PhysicsSystem::start() {
        return false;
    }

    bool PhysicsSystem::stop() {
        return false;
    }

    PhysicsSystem::PhysicsSystem() {}
    PhysicsSystem::~PhysicsSystem() {}
}