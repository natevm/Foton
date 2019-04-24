#include "Collider.hxx"

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

Collider *Collider::CreateBox(std::string name) {
    std::lock_guard<std::mutex> lock(creation_mutex);
	auto collider = StaticFactory::Create(name, "Collider", lookupTable, colliders, MAX_COLLIDERS);
	if (!collider) return nullptr;

    collider->colliderShape = std::make_shared<btBoxShape>(btVector3(btScalar(1.), btScalar(1.), btScalar(1.)));

	return collider;
}

Collider *Collider::CreateSphere(std::string name) {
    std::lock_guard<std::mutex> lock(creation_mutex);
	auto collider = StaticFactory::Create(name, "Collider", lookupTable, colliders, MAX_COLLIDERS);
	if (!collider) return nullptr;

    collider->colliderShape = std::make_shared<btSphereShape>(btScalar(1.));

	return collider;
}

Collider *Collider::CreateCapsule(std::string name) {
    std::lock_guard<std::mutex> lock(creation_mutex);
	auto collider = StaticFactory::Create(name, "Collider", lookupTable, colliders, MAX_COLLIDERS);
	if (!collider) return nullptr;
	return collider;
}

Collider *Collider::CreateCylinder(std::string name) {
    std::lock_guard<std::mutex> lock(creation_mutex);
	auto collider = StaticFactory::Create(name, "Collider", lookupTable, colliders, MAX_COLLIDERS);
	if (!collider) return nullptr;
	return collider;
}

Collider *Collider::CreateCone(std::string name) {
    std::lock_guard<std::mutex> lock(creation_mutex);
	auto collider = StaticFactory::Create(name, "Collider", lookupTable, colliders, MAX_COLLIDERS);
	if (!collider) return nullptr;
	return collider;
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