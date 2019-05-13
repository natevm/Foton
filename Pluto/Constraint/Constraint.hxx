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

class Entity;

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

	/* Returns the total number of reserved constraints */
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

	bool is_enabled();
	void enable(bool onoff);

	void set_a_linear_offset(glm::vec3 offset);
	void set_a_angular_offset(glm::quat offset);
	void set_b_linear_offset(glm::vec3 offset);
	void set_b_angular_offset(glm::quat offset);
	glm::vec3 get_a_linear_offset();
	glm::quat get_a_angular_offset();
	glm::vec3 get_b_linear_offset();
	glm::quat get_b_angular_offset();

	void set_lower_linear_limit(glm::vec3 lower_limit);
	void set_upper_linear_limit(glm::vec3 upper_limit);
	void set_lower_angular_limit(glm::vec3 lower_limit);
	void set_upper_angular_limit(glm::vec3 upper_limit);

	glm::vec3 get_lower_linear_limit();
	glm::vec3 get_upper_linear_limit();

	void set_lower_linear_limit(float x, float y, float z);
	void set_upper_linear_limit(float x, float y, float z);
	void set_lower_angular_limit(float x, float y, float z);
	void set_upper_angular_limit(float x, float y, float z);

	glm::vec3 get_lower_angular_limit();
	glm::vec3 get_upper_angular_limit();

	void enable_spring(uint32_t axis, bool on_off);
	void enable_bounce(uint32_t axis, bool on_off);
	bool is_spring_enabled(uint32_t axis);
	bool is_bounce_enabled(uint32_t axis);

	void enable_linear_spring(bool on_off);
	void enable_linear_bounce(bool on_off);
	void enable_angular_spring(bool on_off);
	void enable_angular_bounce(bool on_off);

	void set_linear_spring_stiffness(glm::vec3 stiffness);
	void set_angular_spring_stiffness(glm::vec3 stiffness);

	void set_linear_spring_stiffness(float x, float y, float z);
	void set_angular_spring_stiffness(float x, float y, float z);

	void set_spring_stiffness(uint32_t axis, float stiffness);

	glm::vec3 get_linear_spring_stiffness();

	glm::vec3 get_angular_spring_stiffness();

	void set_entity_a(Entity *entity);
	void set_entity_b(Entity *entity);

	Entity* get_entity_a();
	Entity* get_entity_b();
	
	void set_entities(Entity *entity_a, Entity *entity_b);

	void clear_entity_a();
	void clear_entity_b();
	void clear_entities();

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

	bool enabled;

	Entity *entity_a;
	Entity *entity_b;

	glm::vec3 a_linear_offset;
	glm::quat a_angular_offset;

	glm::vec3 b_linear_offset;
	glm::quat b_angular_offset;

	glm::vec3 lower_linear_limit;
	glm::vec3 upper_linear_limit;
	glm::vec3 lower_angular_limit;
	glm::vec3 upper_angular_limit;

	bool enableSpring[6];
	bool enableBounce[6];

	glm::vec3 linear_spring_stiffness;
	glm::vec3 angular_spring_stiffness;
};