#include "RigidBody.hxx"
#include "Pluto/Systems/PhysicsSystem/PhysicsSystem.hxx"

RigidBody RigidBody::rigidbodies[MAX_RIGIDBODIES];
std::map<std::string, uint32_t> RigidBody::lookupTable;
std::mutex RigidBody::creation_mutex;
bool RigidBody::Initialized = false;

RigidBody::RigidBody()
{
	initialized = false;
}

RigidBody::RigidBody(std::string name, uint32_t id)
{
	auto ps = Systems::PhysicsSystem::Get();
	auto edit_mutex = ps->get_edit_mutex();
    auto edit_lock = std::lock_guard<std::mutex>(*edit_mutex.get());

	initialized = true;
	this->name = name;
	this->id = id;
	this->mode = KINEMATIC;

	this->mass = 0.0;
	this->friction = .5;
	this->rolling_friction = .0;
	this->spinning_friction = .0;
}

std::string RigidBody::to_string()
{
	std::string output;
	output += "{\n";
	output += "\ttype: \"RigidBody\",\n";
	output += "\tname: \"" + name + "\"\n";
	output += "}";
	return output;

}

RigidBody *RigidBody::Create(std::string name) {
	std::lock_guard<std::mutex> lock(creation_mutex);
	auto rigidbody = StaticFactory::Create(name, "RigidBody", lookupTable, rigidbodies, MAX_RIGIDBODIES);
	return rigidbody;
}

RigidBody *RigidBody::Get(std::string name) {
	return StaticFactory::Get(name, "RigidBody", lookupTable, rigidbodies, MAX_RIGIDBODIES);
}

RigidBody *RigidBody::Get(uint32_t id) {
	return StaticFactory::Get(id, "RigidBody", lookupTable, rigidbodies, MAX_RIGIDBODIES);
}

void RigidBody::Delete(std::string name) {
	StaticFactory::Delete(name, "RigidBody", lookupTable, rigidbodies, MAX_RIGIDBODIES);
}

void RigidBody::Delete(uint32_t id) {
	StaticFactory::Delete(id, "RigidBody", lookupTable, rigidbodies, MAX_RIGIDBODIES);
}

RigidBody* RigidBody::GetFront() {
	return rigidbodies;
}

uint32_t RigidBody::GetCount() {
	return MAX_RIGIDBODIES;
}

void RigidBody::Initialize()
{
	Initialized = true;
}

bool RigidBody::IsInitialized()
{
	return Initialized;
}

void RigidBody::CleanUp()
{
	if (!IsInitialized()) return;

	// for (auto &collider : rigidbodies) {
	// 	if (collider.initialized) {
	// 		collider.cleanup();
	// 		RigidBody::Delete(collider.id);
	// 	}
	// }
}

void RigidBody::make_kinematic()
{
	mode = KINEMATIC;
}

void RigidBody::make_dynamic()
{
	mode = DYNAMIC;
}

void RigidBody::make_static()
{
	mode = STATIC;
}

bool RigidBody::is_kinematic()
{
	return mode == KINEMATIC;
}

bool RigidBody::is_dynamic()
{
	return mode == DYNAMIC;
}

bool RigidBody::is_static()
{
	return mode == STATIC;
}

// void RigidBody::set_collider(Collider* collider)
// {
// 	if (!collider) throw std::runtime_error("Error: collider was nullptr");
// 	if (!collider->is_initialized()) throw std::runtime_error("Error: collider not initialized");

// 	this->collider = collider;

// 	update_local_inertia();
// }


void RigidBody::set_mass(float mass)
{
	if (mass < 0.0) throw std::runtime_error("Error: mass must be greater than or equal to 0");

	auto ps = Systems::PhysicsSystem::Get();
	auto edit_mutex = ps->get_edit_mutex();
    auto edit_lock = std::lock_guard<std::mutex>(*edit_mutex.get());

	this->mass = (btScalar) mass;
	// update_local_inertia();
}

void RigidBody::set_friction(float friction)
{
	if (friction < 0.0) throw std::runtime_error("Error: friction must be greater than or equal to 0");
	this->friction = friction;
}

float RigidBody::get_friction()
{
	return this->friction;
}

void RigidBody::set_rolling_friction(float friction)
{
	if (friction < 0.0) throw std::runtime_error("Error: friction must be greater than or equal to 0");
	this->rolling_friction = friction;
}

float RigidBody::get_rolling_friction()
{
	return this->rolling_friction;
}

void RigidBody::set_spinning_friction(float friction)
{
	if (friction < 0.0) throw std::runtime_error("Error: friction must be greater than or equal to 0");
	this->spinning_friction = friction;
}

float RigidBody::get_spinning_friction()
{
	return this->spinning_friction;
}

float RigidBody::get_mass()
{
	return (float) mass;
}

// Now that rigid body components arent one to one with colliders, this will need to 
// be handled inside the physics system

// void RigidBody::update_local_inertia()
// {
// 	if (!collider || mass <= 0.0) {
// 		localInertia = glm::vec3(0.f, 0.f, 0.f);
// 	}
// 	else {
// 		btCollisionShape *shape = collider->get_collision_shape();
// 		btVector3 temp;
// 		shape->calculateLocalInertia(mass, temp);
// 		localInertia = glm::vec3(
// 			(float) temp.getX(),
// 			(float) temp.getY(),
// 			(float) temp.getZ()
// 		);
// 	}
// }

// glm::vec3 RigidBody::get_local_inertia()
// {
// 	return localInertia;
// }


void RigidBody::cleanup()
{
	
}