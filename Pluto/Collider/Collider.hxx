#pragma once

#include "Pluto/Tools/StaticFactory.hxx"

#ifndef MAX_COLLIDERS
#define MAX_COLLIDERS 512
#endif


class Collider : public StaticFactory
{
    public:

    //TODO
    static Collider *CreateBox(std::string name);
    
    //TODO
    static Collider *CreateSphere(std::string name);
    
    //TODO
    static Collider *CreateCapsule(std::string name);
    
    //TODO
    static Collider *CreateCylinder(std::string name);
    
    //TODO
    static Collider *CreateCone(std::string name);
    
    /* Returns a json string summarizing the collider */
    std::string to_string();

    /* Retrieves a collider component by name */
    static Collider *Get(std::string name);

    /* Retrieves a collider component by id */
    static Collider *Get(uint32_t id);

    /* Returns a pointer to the list of collider components */
    static Collider *GetFront();

    /* Returns the total number of reserved colliders */
    static uint32_t GetCount();

    /* Deallocates a collider with the given name */
    static void Delete(std::string name);

    /* Deallocates a collider with the given id */
    static void Delete(uint32_t id);

    /* Initializes the Mesh factory. Loads default meshes. */
    static void Initialize();

    /* TODO */
    static bool IsInitialized();

    /* Releases resources */
	static void CleanUp();

    /* Creates an uninitialized collider. Useful for preallocation. */
    Collider();

    /* Creates a collider with the given name and id. */
    Collider(std::string name, uint32_t id);

    private:
    /* TODO */
    static std::mutex creation_mutex;
    
    /* TODO */
    static bool Initialized;

    /* The list of collider components, allocated statically */
	static Collider colliders[MAX_COLLIDERS];

    /* A lookup table of name to collider id */
	static std::map<std::string, uint32_t> lookupTable;

    /* Frees the current collider's resources*/
	void cleanup();
}