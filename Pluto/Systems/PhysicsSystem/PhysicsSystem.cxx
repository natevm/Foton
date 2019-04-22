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
        initialized = true;
        return true;
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

            dynamicsWorld->setGravity(btVector3(0, -10, 0));

            // Create a few basic rigid bodies
            
            // the ground is a cube of side 100 at position y = -56
            // the sphere will hit it at y = -6, with center at -5
            {
                btCollisionShape* groundShape = new btBoxShape(btVector3(btScalar(50.), btScalar(50.), btScalar(50.)));

                collisionShapes.push_back(groundShape);

                btTransform groundTransform;
                groundTransform.setIdentity();
                groundTransform.setOrigin(btVector3(0, -56, 0));

                btScalar mass(0.);

                //rigidbody is dynamic if and only if mass is non zero, otherwise static
                bool isDynamic = (mass != 0.f);

                btVector3 localInertia(0, 0, 0);
                if (isDynamic)
                    groundShape->calculateLocalInertia(mass, localInertia);
                
                //using motion state is optional, it provides interpolation capabilities, and only synchronizes 'active' objects
                btDefaultMotionState* myMotionState = new btDefaultMotionState(groundTransform);
                btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, myMotionState, groundShape, localInertia);
                btRigidBody* body = new btRigidBody(rbInfo);

                // add the body to the dynamics world
                dynamicsWorld->addRigidBody(body);
            }

            {
                // Create a dynamic rigidbody

                //btCollisionShape* colShape = new btBoxShape(btVector3(1,1,1));
                btCollisionShape* colShape = new btSphereShape(btScalar(1.));
                collisionShapes.push_back(colShape);

                // Create dynamic objects
                btTransform startTransform;
                startTransform.setIdentity();

                btScalar mass(1.0f);

                // rigid body is dynamic if and only if mass is non zero, otherwise static
                bool isDynamic = (mass != 0.f);

                btVector3 localInertia(0,0,0);
                if (isDynamic)
                    colShape->calculateLocalInertia(mass, localInertia);

                startTransform.setOrigin(btVector3(2, 10, 0));

                //using motionstate is recommended, it provides interpolation capabilities, and only synchronizes 'active' objects
                btDefaultMotionState* myMotionState = new btDefaultMotionState(startTransform);
                btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, myMotionState, colShape, localInertia);
                btRigidBody* body = new btRigidBody(rbInfo);

                dynamicsWorld->addRigidBody(body);
            }

            while (true)
            {
                if (futureObj.wait_for(.1ms) == future_status::ready) break;

                // TODO: maybe regulate the framerate here?
                dynamicsWorld->stepSimulation(1.0f/60.f, 10);

                for (uint32_t j = 0; j < dynamicsWorld->getNumCollisionObjects(); ++j)
                {
                    btCollisionObject* obj  = dynamicsWorld->getCollisionObjectArray()[j];
                    btRigidBody* body = btRigidBody::upcast(obj);
                    btTransform trans;
                    if (body && body->getMotionState())
                    {
                        body->getMotionState()->getWorldTransform(trans);
                    } else 
                    {
                        trans = obj->getWorldTransform();
                    }
                    std::cout<<"world pos object " << j << " = " << 
                        float(trans.getOrigin().getX()) << " " <<
                        float(trans.getOrigin().getY()) << " " <<
                        float(trans.getOrigin().getZ()) << std::endl;
                }
            }

            // cleanup in the reverse order of creation/initialization

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

            // delete collision shapes
            for (uint32_t j = 0; j < collisionShapes.size(); j++)
            {
                btCollisionShape* shape = collisionShapes[j];
                collisionShapes[j] = 0;
                delete shape;
            }

            // delete dynamics world
            delete dynamicsWorld;

            // delete solver
            delete solver;

            // delete broadphase
            delete overlappingPairCache;

            // delete dispatcher
            delete dispatcher;
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