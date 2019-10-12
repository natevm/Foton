#include "RigidBody.hxx"
#include "Pluto/Systems/PhysicsSystem/PhysicsSystem.hxx"

RigidBody RigidBody::rigidbodies[MAX_RIGIDBODIES];
std::map<std::string, uint32_t> RigidBody::lookupTable;
std::shared_ptr<std::mutex> RigidBody::creation_mutex;
bool RigidBody::Initialized = false;
bool RigidBody::Dirty = true;

RigidBody::RigidBody()
{
	initialized = false;
}

RigidBody::RigidBody(std::string name, uint32_t id)
{
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
	auto rigidbody = StaticFactory::Create(creation_mutex, name, "RigidBody", lookupTable, rigidbodies, MAX_RIGIDBODIES);
	return rigidbody;
}

RigidBody *RigidBody::Get(std::string name) {
	return StaticFactory::Get(creation_mutex, name, "RigidBody", lookupTable, rigidbodies, MAX_RIGIDBODIES);
}

RigidBody *RigidBody::Get(uint32_t id) {
	return StaticFactory::Get(creation_mutex, id, "RigidBody", lookupTable, rigidbodies, MAX_RIGIDBODIES);
}

void RigidBody::Delete(std::string name) {
	StaticFactory::Delete(creation_mutex, name, "RigidBody", lookupTable, rigidbodies, MAX_RIGIDBODIES);
}

void RigidBody::Delete(uint32_t id) {
	StaticFactory::Delete(creation_mutex, id, "RigidBody", lookupTable, rigidbodies, MAX_RIGIDBODIES);
}

RigidBody* RigidBody::GetFront() {
	return rigidbodies;
}

uint32_t RigidBody::GetCount() {
	return MAX_RIGIDBODIES;
}

void RigidBody::Initialize()
{
	creation_mutex = std::make_shared<std::mutex>();
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

void RigidBody::cleanup()
{
	
}