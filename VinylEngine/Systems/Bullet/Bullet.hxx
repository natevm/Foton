#pragma once

#include <iostream>

#include <thread>
#include <iostream>
#include <assert.h>
#include <chrono>
#include <future>
#include <vector>
#include <unordered_map>

#include <btBulletDynamicsCommon.h>

namespace Systems {
    using namespace std;
    class Bullet
    {
    public:
        static Bullet& Get();
        
    private:
        Bullet() {  
            cout<<"Physics Engine Started"<<std::endl;
	        future<void> futureObj = exitSignal.get_future();
            eventThread = thread(&Loop, move(futureObj));
        }
        ~Bullet() {
            cout<<"Physics Engine Stopped"<<std::endl;
            exitSignal.set_value();
            eventThread.join();
        }
    
        Bullet(const Bullet&) = delete;
        Bullet& operator=(const Bullet&) = delete;
        Bullet(Bullet&&) = delete;
        Bullet& operator=(Bullet&&) = delete;
        
        promise<void> exitSignal;
        thread eventThread;

        static void Loop(future<void> futureObj) {
            btBroadphaseInterface *broadphase = new btDbvtBroadphase();
            btDefaultCollisionConfiguration* collisionConfiguration = new btDefaultCollisionConfiguration();
            btCollisionDispatcher* dispatcher = new btCollisionDispatcher(collisionConfiguration);
            btSequentialImpulseConstraintSolver* solver = new btSequentialImpulseConstraintSolver;
            
            btDiscreteDynamicsWorld* dynamicsWorld = new btDiscreteDynamicsWorld(dispatcher, broadphase, solver, collisionConfiguration);
            
            dynamicsWorld->setGravity(btVector3(0, -10, 0));

            btCollisionShape* groundShape = new btStaticPlaneShape(btVector3(0, 1, 0), 1);
            btCollisionShape* fallShape = new btSphereShape(1);

            btDefaultMotionState* groundMotionState =
                new btDefaultMotionState(btTransform(btQuaternion(0, 0, 0, 1), btVector3(0, -1, 0)));
            btRigidBody::btRigidBodyConstructionInfo
                groundRigidBodyCI(0, groundMotionState, groundShape, btVector3(0, 0, 0));
            btRigidBody* groundRigidBody = new btRigidBody(groundRigidBodyCI);

            dynamicsWorld->addRigidBody(groundRigidBody);

            btDefaultMotionState* fallMotionState =
                new btDefaultMotionState(btTransform(btQuaternion(0, 0, 0, 1), btVector3(0, 50, 0)));
            btScalar mass = 1;
            btVector3 fallInertia(0, 0, 0);
            fallShape->calculateLocalInertia(mass, fallInertia);

            btRigidBody::btRigidBodyConstructionInfo fallRigidBodyCI(mass, fallMotionState, fallShape, fallInertia);
            btRigidBody* fallRigidBody = new btRigidBody(fallRigidBodyCI);
            dynamicsWorld->addRigidBody(fallRigidBody);

            while (futureObj.wait_for(chrono::milliseconds(1)) == future_status::timeout)
            {
                dynamicsWorld->stepSimulation(1 / 60.f, 10);

                btTransform trans;
                fallRigidBody->getMotionState()->getWorldTransform(trans);
            }

            delete dynamicsWorld;
            delete solver;
            delete dispatcher;
            delete collisionConfiguration;
            delete broadphase;
        }
    };
    
    Bullet& Bullet::Get()
    {
        static Bullet instance;
        return instance;
    }
}
