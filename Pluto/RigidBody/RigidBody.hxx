#pragma once

#include "Pluto/Tools/StaticFactory.hxx"

#ifndef MAX_RIGIDBODIES
#define MAX_RIGIDBODIES 512
#endif

class RigidBody : public StaticFactory
{
    public:

    /* Returns a json string summarizing the rigidbody */
    std::string to_string();

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

    /* Creates an uninitialized rigidbody. Useful for preallocation. */
    RigidBody();

    /* Creates a rigidbody with the given name and id. */
    RigidBody(std::string name, uint32_t id);

    private:
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
}