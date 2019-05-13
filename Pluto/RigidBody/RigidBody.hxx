#pragma once

#include "Pluto/Tools/StaticFactory.hxx"

#include <iostream>
#include <map>
#include <vector>
#include <thread>
#include <mutex>

#include <glm/glm.hpp>

#include "btBulletDynamicsCommon.h"

#ifndef MAX_RIGIDBODIES
#define MAX_RIGIDBODIES 512
#endif

enum RigidBodyMode : uint32_t { 
    STATIC,
    DYNAMIC,
    KINEMATIC
};

class RigidBody : public StaticFactory
{
    friend class StaticFactory;
    public:

    /* Returns a json string summarizing the rigidbody */
    std::string to_string();

    /* TODO */
    static RigidBody *Create(std::string name);

    /* Retrieves a rigidbody component by name */
    static RigidBody *Get(std::string name);

    /* Retrieves a rigidbody component by id */
    static RigidBody *Get(uint32_t id);

    /* Returns a pointer to the list of rigidbody components */
    static RigidBody *GetFront();

    /* Returns the total number of reserved rigidbodies */
    static uint32_t GetCount();

    /* Deallocates a rigidbody with the given name */
    static void Delete(std::string name);

    /* Deallocates a rigidbody with the given id */
    static void Delete(uint32_t id);

    /* Initializes the Mesh factory. Loads default meshes. */
    static void Initialize();

    /* TODO */
    static bool IsInitialized();

    /* Releases resources */
	static void CleanUp();
    
    /* TODO */
    void make_kinematic();
    
    /* TODO */
    void make_dynamic();
    
    /* TODO */
    void make_static();
    
    /* TODO */
    bool is_kinematic();
    
    /* TODO */
    bool is_dynamic();
    
    /* TODO */
    bool is_static();

    void set_mass(float mass);

    float get_mass();

    void set_friction(float friction);

    float get_friction();
    
    void set_rolling_friction(float friction);

    float get_rolling_friction();

    void set_spinning_friction(float friction);

    float get_spinning_friction();

    // glm::vec3 get_local_inertia();

    private:

    /* Creates an uninitialized rigidbody. Useful for preallocation. */
    RigidBody();

    /* Creates a rigidbody with the given name and id. */
    RigidBody(std::string name, uint32_t id);

    RigidBodyMode mode;

    float mass; 

    float friction;

    float rolling_friction;

    float spinning_friction;

    glm::vec3 localInertia;

    /* TODO */
    static std::mutex creation_mutex;
    
    /* TODO */
    static bool Initialized;

    /* The list of rigidbody components, allocated statically */
	static RigidBody rigidbodies[MAX_RIGIDBODIES];

    /* A lookup table of name to rigidbody id */
	static std::map<std::string, uint32_t> lookupTable;

    /* Frees the current rigidbody's resources*/
	void cleanup();

    /* TODO */
    // void update_local_inertia();
};