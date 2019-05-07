#pragma once

#include "Pluto/Tools/StaticFactory.hxx"

#include <iostream>
#include <map>
#include <vector>
#include <thread>
#include <mutex>

#include <glm/glm.hpp>

#ifndef MAX_CONSTRAINTS
#define MAX_CONSTRAINTS 512
#endif

class Constraint : public StaticFactory
{
    friend class StaticFactory;
    public:

    /* Returns a json string summarizing the constraint */
    std::string to_string();

    /* TODO */
    static Constraint *Create(std::string name);

    /* Retrieves a Constraint component by name */
    static Constraint *Get(std::string name);

    /* Retrieves a Constraint component by id */
    static Constraint *Get(uint32_t id);

    /* Returns a pointer to the list of Constraint components */
    static Constraint *GetFront();

    /* Returns the total number of reserved rigidbodies */
    static uint32_t GetCount();

    /* Deallocates a Constraint with the given name */
    static void Delete(std::string name);

    /* Deallocates a Constraint with the given id */
    static void Delete(uint32_t id);

    /* Initializes the Mesh factory. Loads default meshes. */
    static void Initialize();

    /* TODO */
    static bool IsInitialized();

    /* Releases resources */
	static void CleanUp();
    
    private:

    /* Creates an uninitialized Constraint. Useful for preallocation. */
    Constraint();

    /* Creates a Constraint with the given name and id. */
    Constraint(std::string name, uint32_t id);

    /* TODO */
    static std::mutex creation_mutex;
    
    /* TODO */
    static bool Initialized;

    /* The list of Constraint components, allocated statically */
	static Constraint constraints[MAX_CONSTRAINTS];

    /* A lookup table of name to constraint id */
	static std::map<std::string, uint32_t> lookupTable;

    /* Frees the current Constraint's resources*/
	void cleanup();
};