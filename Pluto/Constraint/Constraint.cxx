#include "Constraint.hxx"
#include "Pluto/Collider/Collider.hxx"
#include "Pluto/RigidBody/RigidBody.hxx"

Constraint Constraint::constraints[MAX_CONSTRAINTS];
std::map<std::string, uint32_t> Constraint::lookupTable;
std::mutex Constraint::creation_mutex;
bool Constraint::Initialized = false;

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

	for (uint32_t i = 0; i < 6; ++i) {
		enableSpring[i] = false;
		enableBounce[i] = false;
	}

	this->linear_spring_stiffness = glm::vec3(0.0, 0.0, 0.0);
	this->angular_spring_stiffness = glm::vec3(0.0, 0.0, 0.0);

	this->rigid_body_a = nullptr;
	this->rigid_body_b = nullptr;
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
	std::lock_guard<std::mutex> lock(creation_mutex);
	auto constraint = StaticFactory::Create(name, "Constraint", lookupTable, constraints, MAX_CONSTRAINTS);
	return constraint;
}

Constraint *Constraint::Get(std::string name) {
	return StaticFactory::Get(name, "Constraint", lookupTable, constraints, MAX_CONSTRAINTS);
}

Constraint *Constraint::Get(uint32_t id) {
	return StaticFactory::Get(id, "Constraint", lookupTable, constraints, MAX_CONSTRAINTS);
}

void Constraint::Delete(std::string name) {
	StaticFactory::Delete(name, "Constraint", lookupTable, constraints, MAX_CONSTRAINTS);
}

void Constraint::Delete(uint32_t id) {
	StaticFactory::Delete(id, "Constraint", lookupTable, constraints, MAX_CONSTRAINTS);
}

Constraint* Constraint::GetFront() {
	return constraints;
}

uint32_t Constraint::GetCount() {
	return MAX_CONSTRAINTS;
}

void Constraint::Initialize()
{
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

void Constraint::enable_bounce(uint32_t axis, bool on_off)
{
	if (axis > 5) throw std::runtime_error("Error: axis must be in range [0, 6)");
	enableBounce[axis] = on_off;
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

void Constraint::set_frame_a(glm::mat4 frame)
{
	frame_a = frame;
}

void Constraint::set_frame_b(glm::mat4 frame)
{
	frame_b = frame;
}

void Constraint::set_rigid_body_a(RigidBody *rigid_body) 
{
	if (!rigid_body) throw std::runtime_error("Error: rigid_body was nullptr");
	if (!rigid_body->is_initialized()) throw std::runtime_error("Error: rigid_body not initialized");
	this->rigid_body_a = rigid_body;
}

void Constraint::set_rigid_body_b(RigidBody *rigid_body) 
{
	if (!rigid_body) throw std::runtime_error("Error: rigid_body was nullptr");
	if (!rigid_body->is_initialized()) throw std::runtime_error("Error: rigid_body not initialized");
	this->rigid_body_b = rigid_body;
}

void Constraint::set_rigid_bodies(RigidBody *rigid_body_a, RigidBody *rigid_body_b) 
{
	if (!rigid_body_a) throw std::runtime_error("Error: rigid_body_a was nullptr");
	if (!rigid_body_a->is_initialized()) throw std::runtime_error("Error: rigid_body_a not initialized");
	
	if (!rigid_body_b) throw std::runtime_error("Error: rigid_body_b was nullptr");
	if (!rigid_body_b->is_initialized()) throw std::runtime_error("Error: rigid_body_b not initialized");
	
	this->rigid_body_a = rigid_body_a;
	this->rigid_body_b = rigid_body_b;
}

void Constraint::clear_rigid_body_a()
{
	this->rigid_body_a = nullptr;
}

void Constraint::clear_rigid_body_b()
{
	this->rigid_body_b = nullptr;
}

void Constraint::clear_rigid_bodies()
{
	this->rigid_body_a = nullptr;
	this->rigid_body_b = nullptr;
}
