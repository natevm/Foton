#include "RigidBody.hxx"

RigidBody::rigidbodies[MAX_RIGIDBODIES];
std::map<std::string, uint32_t> RigidBody::lookupTable;
std::mutex RigidBody::creation_mutex;
bool RigidBody::Initialized = false;

RigidBody::RigidBody()
{
    initialized = false;
}

RigidBody::RigidBody(std::string name, uint32_t id)
{
    initilized = true;
    this->name = name;
    this->id = id;
}

std::string RigidBody::to_string()
{
    std::string output;
    output += "{\n";
    output += "\ttype: \"RigidBody\",\n";
    output += "\tname: \"" + name + "\"\n";
    output += "}"
    return output;

}

RigidBody *Create(std::string name) {
    std::lock_guard<std::mutex> lock(creation_mutex);
	auto rigidbody = StaticFactory::Create(name, "RigidBody", lookupTable, rigidbodies, MAX_RIGIDBODIES);
	if (!rigidbody) return nullptr;
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

    for (auto &collider : rigidbodies) {
		if (collider.initialized) {
			collider.cleanup();
			RigidBody::Delete(collider.id);
		}
	}
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

void RigidBody::cleanup()
{
    
}