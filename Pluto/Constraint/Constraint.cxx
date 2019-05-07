#include "Constraint.hxx"
#include "Pluto/Collider/Collider.hxx"

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
	try {
		std::lock_guard<std::mutex> lock(creation_mutex);
		auto Constraint = StaticFactory::Create(name, "Constraint", lookupTable, constraints, MAX_CONSTRAINTS);
		return Constraint;
	} catch (...) {
		StaticFactory::DeleteIfExists(name, "Constraint", lookupTable, constraints, MAX_CONSTRAINTS);
		throw;
	}
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