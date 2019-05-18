#include "Collider.hxx"
#include "Pluto/Systems/PhysicsSystem/PhysicsSystem.hxx"
Collider Collider::colliders[MAX_COLLIDERS];
std::map<std::string, uint32_t> Collider::lookupTable;
std::mutex Collider::creation_mutex;
bool Collider::Initialized = false;

Collider::Collider()
{
	initialized = false;
}

Collider::Collider(std::string name, uint32_t id)
{
	initialized = true;
	this->name = name;
	this->id = id;
}

std::string Collider::to_string()
{
	std::string output;
	output += "{\n";
	output += "\ttype: \"Collider\",\n";
	output += "\tname: \"" + name + "\"\n";
	output += "}";
	return output;
}

Collider *Collider::CreateBox(std::string name, float size_x, float size_y, float size_z) {
	std::lock_guard<std::mutex> lock(creation_mutex);
	auto collider = StaticFactory::Create(name, "Collider", lookupTable, colliders, MAX_COLLIDERS);
	try {
		auto ps = Systems::PhysicsSystem::Get();
		auto edit_mutex = ps->get_edit_mutex();
		auto edit_lock = std::lock_guard<std::mutex>(*edit_mutex.get());
		collider->colliderShape = std::make_shared<btBoxShape>(btVector3(btScalar(size_x), btScalar(size_y), btScalar(size_z)));
		return collider;
	} catch (...) {
		StaticFactory::DeleteIfExists(name, "Collider", lookupTable, colliders, MAX_COLLIDERS);
		throw;
	}
}

Collider *Collider::CreateSphere(std::string name, float radius) {
	std::lock_guard<std::mutex> lock(creation_mutex);
	auto collider = StaticFactory::Create(name, "Collider", lookupTable, colliders, MAX_COLLIDERS);
	try {
		auto ps = Systems::PhysicsSystem::Get();
		auto edit_mutex = ps->get_edit_mutex();
		auto edit_lock = std::lock_guard<std::mutex>(*edit_mutex.get());
		collider->colliderShape = std::make_shared<btSphereShape>(btScalar(radius));
		return collider;
	} catch (...) {
		StaticFactory::DeleteIfExists(name, "Collider", lookupTable, colliders, MAX_COLLIDERS);
		throw;
	}
}

Collider *Collider::CreateCapsule(std::string name, float radius, float height) {
	std::lock_guard<std::mutex> lock(creation_mutex);
	auto collider = StaticFactory::Create(name, "Collider", lookupTable, colliders, MAX_COLLIDERS);
	try {
		auto ps = Systems::PhysicsSystem::Get();
		auto edit_mutex = ps->get_edit_mutex();
		auto edit_lock = std::lock_guard<std::mutex>(*edit_mutex.get());
		collider->colliderShape = std::make_shared<btCapsuleShapeZ>(btScalar(radius), btScalar(height));
		return collider;
	} catch (...) {
		StaticFactory::DeleteIfExists(name, "Collider", lookupTable, colliders, MAX_COLLIDERS);
		throw;
	}
}

Collider *Collider::CreateCylinder(std::string name, float radius, float height) {
	std::lock_guard<std::mutex> lock(creation_mutex);
	auto collider = StaticFactory::Create(name, "Collider", lookupTable, colliders, MAX_COLLIDERS);
	try {
		auto ps = Systems::PhysicsSystem::Get();
		auto edit_mutex = ps->get_edit_mutex();
		auto edit_lock = std::lock_guard<std::mutex>(*edit_mutex.get());
		collider->colliderShape = std::make_shared<btCylinderShapeZ>(btVector3(radius, radius, height / 2.f));
		return collider;
	} catch (...) {
		StaticFactory::DeleteIfExists(name, "Collider", lookupTable, colliders, MAX_COLLIDERS);
		throw;
	}
}

Collider *Collider::CreateCone(std::string name, float radius, float height) {
	std::lock_guard<std::mutex> lock(creation_mutex);
	auto collider = StaticFactory::Create(name, "Collider", lookupTable, colliders, MAX_COLLIDERS);
	try {
		auto ps = Systems::PhysicsSystem::Get();
		auto edit_mutex = ps->get_edit_mutex();
		auto edit_lock = std::lock_guard<std::mutex>(*edit_mutex.get());
		collider->colliderShape = std::make_shared<btConeShapeZ>(btScalar(radius), btScalar(height));
		return collider;
	} catch (...) {
		StaticFactory::DeleteIfExists(name, "Collider", lookupTable, colliders, MAX_COLLIDERS);
		throw;
	}
}

Collider *Collider::Get(std::string name) {
	return StaticFactory::Get(name, "Collider", lookupTable, colliders, MAX_COLLIDERS);
}

Collider *Collider::Get(uint32_t id) {
	return StaticFactory::Get(id, "Collider", lookupTable, colliders, MAX_COLLIDERS);
}

void Collider::Delete(std::string name) {
	StaticFactory::Delete(name, "Collider", lookupTable, colliders, MAX_COLLIDERS);
}

void Collider::Delete(uint32_t id) {
	StaticFactory::Delete(id, "Collider", lookupTable, colliders, MAX_COLLIDERS);
}

Collider* Collider::GetFront() {
	return colliders;
}

uint32_t Collider::GetCount() {
	return MAX_COLLIDERS;
}

void Collider::Initialize()
{
	Initialized = true;
}

bool Collider::IsInitialized()
{
	return Initialized;
}

void Collider::CleanUp()
{
	if (!IsInitialized()) return;

	for (auto &collider : colliders) {
		if (collider.initialized) {
			collider.cleanup();
			Collider::Delete(collider.id);
		}
	}
}

btCollisionShape* Collider::get_collision_shape()
{
	return colliderShape.get();
}

void Collider::set_scale(float x, float y, float z)
{
	colliderShape->setLocalScaling(btVector3(x, y, z));
}

void Collider::set_scale(float r)
{
	colliderShape->setLocalScaling(btVector3(r, r, r));
}
	
void Collider::set_collision_margin(float m)
{
	colliderShape->setMargin(m);
}

void Collider::cleanup()
{

}