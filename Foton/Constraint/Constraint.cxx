#include "Constraint.hxx"
#include "Foton/Collider/Collider.hxx"
#include "Foton/Entity/Entity.hxx"

Constraint Constraint::constraints[MAX_CONSTRAINTS];
std::map<std::string, uint32_t> Constraint::lookupTable;
std::shared_ptr<std::mutex> Constraint::creation_mutex;
bool Constraint::Initialized = false;
bool Constraint::Dirty = true;

Constraint::Constraint()
{
	initialized = false;
}

Constraint::Constraint(std::string name, uint32_t id)
{
	initialized = true;
	this->name = name;
	this->id = id;
	this->lower_linear_limit = glm::vec3(0.0, 0.0, 0.0);
	this->upper_linear_limit = glm::vec3(0.0, 0.0, 0.0);
	this->lower_angular_limit = glm::vec3(0.0, 0.0, 0.0);
	this->upper_angular_limit = glm::vec3(0.0, 0.0, 0.0);
	this->enabled = true;

	for (uint32_t i = 0; i < 6; ++i) {
		enableSpring[i] = false;
		enableBounce[i] = false;
	}

	this->linear_spring_stiffness = glm::vec3(0.0, 0.0, 0.0);
	this->angular_spring_stiffness = glm::vec3(0.0, 0.0, 0.0);

	this->entity_a = nullptr;
	this->entity_b = nullptr;

	this->a_linear_offset = glm::vec3(0.0f, 0.0f, 0.0f);
	this->a_angular_offset = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	this->b_linear_offset = glm::vec3(0.0f, 0.0f, 0.0f);
	this->b_angular_offset = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
}

std::string Constraint::to_string()
{
	std::string output;
	output += "{\n";
	output += "\ttype: \"Constraint\",\n";
	output += "\tname: \"" + name + "\"\n";
	output += "}";
	return output;
}

Constraint *Constraint::Create(std::string name) {
	auto constraint = StaticFactory::Create(creation_mutex, name, "Constraint", lookupTable, constraints, MAX_CONSTRAINTS);
	return constraint;
}

Constraint *Constraint::Get(std::string name) {
	return StaticFactory::Get(creation_mutex, name, "Constraint", lookupTable, constraints, MAX_CONSTRAINTS);
}

Constraint *Constraint::Get(uint32_t id) {
	return StaticFactory::Get(creation_mutex, id, "Constraint", lookupTable, constraints, MAX_CONSTRAINTS);
}

void Constraint::Delete(std::string name) {
	StaticFactory::Delete(creation_mutex, name, "Constraint", lookupTable, constraints, MAX_CONSTRAINTS);
}

void Constraint::Delete(uint32_t id) {
	StaticFactory::Delete(creation_mutex, id, "Constraint", lookupTable, constraints, MAX_CONSTRAINTS);
}

Constraint* Constraint::GetFront() {
	return constraints;
}

uint32_t Constraint::GetCount() {
	return MAX_CONSTRAINTS;
}

void Constraint::Initialize()
{
	creation_mutex = std::make_shared<std::mutex>();
	Initialized = true;
}

bool Constraint::IsInitialized()
{
	return Initialized;
}

void Constraint::CleanUp()
{
	if (!IsInitialized()) return;

	for (auto &collider : constraints) {
		if (collider.initialized) {
			collider.cleanup();
			Constraint::Delete(collider.id);
		}
	}
}

void Constraint::cleanup()
{
	
}

void Constraint::enable(bool onoff)
{
	this->enabled = onoff;
}

bool Constraint::is_enabled()
{
	return this->enabled;
}

void Constraint::set_lower_linear_limit(glm::vec3 lower_limit)
{
	lower_linear_limit = lower_limit;
}

void Constraint::set_upper_linear_limit(glm::vec3 upper_limit)
{
	upper_linear_limit = upper_limit;
}

void Constraint::set_lower_angular_limit(glm::vec3 lower_limit)
{
	lower_angular_limit = lower_limit;
}

void Constraint::set_upper_angular_limit(glm::vec3 upper_limit)
{
	upper_angular_limit = upper_limit;
}

void Constraint::set_lower_linear_limit(float x, float y, float z)
{
	lower_linear_limit = glm::vec3(x, y, z);
}

void Constraint::set_upper_linear_limit(float x, float y, float z)
{
	upper_linear_limit = glm::vec3(x, y, z);
}

glm::vec3 Constraint::get_lower_linear_limit()
{
	return lower_linear_limit;
}

glm::vec3 Constraint::get_upper_linear_limit()
{
	return upper_linear_limit;
}

glm::vec3 Constraint::get_lower_angular_limit()
{
	return lower_angular_limit;
}

glm::vec3 Constraint::get_upper_angular_limit()
{
	return upper_angular_limit;
}

void Constraint::set_lower_angular_limit(float x, float y, float z)
{
	lower_angular_limit = glm::vec3(x, y, z);
}

void Constraint::set_upper_angular_limit(float x, float y, float z)
{
	upper_angular_limit = glm::vec3(x, y, z);
}

void Constraint::enable_spring(uint32_t axis, bool on_off)
{
	if (axis > 5) throw std::runtime_error("Error: axis must be in range [0, 6)");
	enableSpring[axis] = on_off;
}

bool Constraint::is_spring_enabled(uint32_t axis)
{
	if (axis > 5) throw std::runtime_error("Error: axis must be in range [0, 6)");
	return enableSpring[axis];
}

void Constraint::enable_bounce(uint32_t axis, bool on_off)
{
	if (axis > 5) throw std::runtime_error("Error: axis must be in range [0, 6)");
	enableBounce[axis] = on_off;
}

bool Constraint::is_bounce_enabled(uint32_t axis)
{
	if (axis > 5) throw std::runtime_error("Error: axis must be in range [0, 6)");
	return enableBounce[axis];
}

glm::vec3 Constraint::get_linear_spring_stiffness()
{
	return linear_spring_stiffness;
}

glm::vec3 Constraint::get_angular_spring_stiffness()
{
	return angular_spring_stiffness;
}

void Constraint::enable_linear_spring(bool on_off)
{
	enableSpring[0] = enableSpring[1] = enableSpring[2] = on_off;
}

void Constraint::enable_linear_bounce(bool on_off)
{
	enableBounce[0] = enableBounce[1] = enableBounce[2] = on_off;
}

void Constraint::enable_angular_spring(bool on_off)
{
	enableSpring[3] = enableSpring[4] = enableSpring[5] = on_off;
}

void Constraint::enable_angular_bounce(bool on_off)
{
	enableBounce[3] = enableBounce[4] = enableBounce[5] = on_off;
}

void Constraint::set_linear_spring_stiffness(glm::vec3 stiffness)
{
	linear_spring_stiffness = stiffness;
}

void Constraint::set_angular_spring_stiffness(glm::vec3 stiffness)
{
	angular_spring_stiffness = stiffness;
}

void Constraint::set_linear_spring_stiffness(float x, float y, float z)
{
	linear_spring_stiffness = glm::vec3(x, y, z);
}

void Constraint::set_angular_spring_stiffness(float x, float y, float z)
{
	angular_spring_stiffness = glm::vec3(x, y, z);
}

void Constraint::set_spring_stiffness(uint32_t axis, float stiffness)
{
	if (axis > 5) throw std::runtime_error("Error: axis must be in range [0, 6)");

	if (axis < 3) {
		linear_spring_stiffness[axis] = stiffness;
	} else {
		angular_spring_stiffness[axis - 3] = stiffness;
	}
}

void Constraint::set_a_linear_offset(glm::vec3 offset)
{
	a_linear_offset = offset;
}

void Constraint::set_a_angular_offset(glm::quat offset)
{
	a_angular_offset = offset;
}

void Constraint::set_b_linear_offset(glm::vec3 offset)
{
	b_linear_offset = offset;
}

void Constraint::set_b_angular_offset(glm::quat offset)
{
	b_angular_offset = offset;
}

glm::vec3 Constraint::get_a_linear_offset()
{
	return a_linear_offset;
}

glm::quat Constraint::get_a_angular_offset()
{
	return a_angular_offset;
}

glm::vec3 Constraint::get_b_linear_offset()
{
	return b_linear_offset;
}

glm::quat Constraint::get_b_angular_offset()
{
	return b_angular_offset;
}

void Constraint::set_entity_a(Entity *entity) 
{
	if (!entity) throw std::runtime_error("Error: entity was nullptr");
	if (!entity->is_initialized()) throw std::runtime_error("Error: entity not initialized");
	this->entity_a = entity;
}

void Constraint::set_entity_b(Entity *entity) 
{
	if (!entity) throw std::runtime_error("Error: entity was nullptr");
	if (!entity->is_initialized()) throw std::runtime_error("Error: entity not initialized");
	this->entity_b = entity;
}

void Constraint::set_entities(Entity *entity_a, Entity *entity_b) 
{
	if (!entity_a) throw std::runtime_error("Error: entity_a was nullptr");
	if (!entity_a->is_initialized()) throw std::runtime_error("Error: entity_a not initialized");
	
	if (!entity_b) throw std::runtime_error("Error: entity_b was nullptr");
	if (!entity_b->is_initialized()) throw std::runtime_error("Error: entity_b not initialized");
	
	this->entity_a = entity_a;
	this->entity_b = entity_b;
}

Entity* Constraint::get_entity_a()
{
	return this->entity_a;
}

Entity* Constraint::get_entity_b()
{
	return this->entity_b;
}

void Constraint::clear_entity_a()
{
	this->entity_a = nullptr;
}

void Constraint::clear_entity_b()
{
	this->entity_b = nullptr;
}

void Constraint::clear_entities()
{
	this->entity_a = nullptr;
	this->entity_b = nullptr;
}
