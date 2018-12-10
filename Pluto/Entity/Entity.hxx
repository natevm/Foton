// ┌──────────────────────────────────────────────────────────────────┐
// │  Entity                                                          │
// └──────────────────────────────────────────────────────────────────┘

#pragma once

#include "Pluto/Tools/StaticFactory.hxx"
#include "Pluto/Libraries/Vulkan/Vulkan.hxx"
#include "Pluto/Camera/Camera.hxx"
#include "Pluto/Transform/Transform.hxx"
#include "Pluto/Material/Material.hxx"
#include "Pluto/Light/Light.hxx"
#include "Pluto/Mesh/Mesh.hxx"
#include "Pluto/Entity/EntityStruct.hxx"

class Entity : public StaticFactory {
private:
	/* If an entity isn't active, its callbacks arent called */
	bool active = true;
	int32_t parent = -1;
	std::set<int32_t> children;

	EntityStruct entity_struct;

	//std::shared_ptr<Callbacks> callbacks;
	//std::map<std::type_index, std::vector<std::shared_ptr<Component>>> components;
	
	/* Static fields */
	static Entity entities[MAX_ENTITIES];
	static EntityStruct* pinnedMemory;
    static std::map<std::string, uint32_t> lookupTable;
    static vk::Buffer ssbo;
    static vk::DeviceMemory ssboMemory;

public:
	static Entity* Create(std::string name);
	static Entity* Get(std::string name);
	static Entity* Get(uint32_t id);
	static Entity* GetFront();
	static uint32_t GetCount();
	static bool Delete(std::string name);
	static bool Delete(uint32_t id);
	
    static void Initialize();
    static void UploadSSBO();
    static vk::Buffer GetSSBO();
	static uint32_t GetSSBOSize();
    static void CleanUp();	

	Entity() {
		this->initialized = false;
		entity_struct.initialized = false;
		entity_struct.transform_id = -1;
		entity_struct.camera_id = -1;
		entity_struct.material_id = -1;
		entity_struct.light_id = -1;
		entity_struct.mesh_id = -1;
	}

	Entity(std::string name, uint32_t id) {
		this->initialized = true;
		this->name = name;
		this->id = id;
		entity_struct.initialized = true;
		entity_struct.transform_id = -1;
		entity_struct.camera_id = -1;
		entity_struct.material_id = -1;
		entity_struct.light_id = -1;
		entity_struct.mesh_id = -1;
	}

	std::string to_string()
    {
        std::string output;
        output += "{\n";
        output += "\ttype: \"Entity\",\n";
        output += "\tname: \"" + name + "\",\n";
        output += "\tid: \"" + std::to_string(id) + "\",\n";
        output += "\ttransform_id: " + std::to_string(entity_struct.transform_id) + "\n";
        output += "\tcamera_id: " + std::to_string(entity_struct.camera_id) + "\n";
        output += "\tmaterial_id: " + std::to_string(entity_struct.material_id) + "\n";
        output += "\tlight_id: " + std::to_string(entity_struct.light_id) + "\n";
        output += "\tmesh_id: " + std::to_string(entity_struct.mesh_id) + "\n";
        output += "}";
        return output;
    }

	uint32_t get_id() 
	{
		return id;
	}

	void set_transform(uint32_t transform_id) 
	{
		this->entity_struct.transform_id = transform_id;
	}

	void set_transform(Transform* transform) 
	{
		if (!transform) return;
		this->entity_struct.transform_id = transform->get_id();
	}

	int32_t get_transform() 
	{
		return this->entity_struct.transform_id;
	}

	void set_camera(uint32_t camera_id) 
	{
		this->entity_struct.camera_id = camera_id;
	}

	void set_camera(Camera *camera) 
	{
		if (!camera) return;
		this->entity_struct.camera_id = camera->get_id();
	}

	int32_t get_camera() 
	{
		return this->entity_struct.camera_id;
	}

	void set_material(uint32_t material_id) 
	{
		this->entity_struct.material_id = material_id;
	}

	void set_material(Material *material) 
	{
		if (!material) return;
		this->entity_struct.material_id = material->get_id();
	}

	int32_t get_material() 
	{
		return this->entity_struct.material_id;
	}

	void set_light(uint32_t light_id) 
	{
		this->entity_struct.light_id = light_id;
	}

	void set_light(Light* light) 
	{
		if (!light) return;
		this->entity_struct.light_id = light->get_id();
	}

	int32_t get_light() 
	{
		return this->entity_struct.light_id;
	}

	void set_mesh(uint32_t mesh_id) 
	{
		this->entity_struct.mesh_id = mesh_id;
	}

	void set_mesh(Mesh* mesh) 
	{
		if (!mesh) return;
		this->entity_struct.mesh_id = mesh->get_id();
	}

	int32_t get_mesh() 
	{
		return this->entity_struct.mesh_id;
	}

	void setParent(uint32_t parent);

	void addChild(uint32_t object);

	void removeChild(uint32_t object);

	// glm::mat4 getWorldToLocalMatrix() {
	// 	glm::mat4 parentMatrix = glm::mat4(1.0);
	// 	if (parent != nullptr) {
	// 		parentMatrix = parent->getWorldToLocalMatrix();
	// 		return transform->ParentToLocalMatrix() * parentMatrix;
	// 	}
	// 	else return transform->ParentToLocalMatrix();
	// }

	// glm::mat4 getLocalToWorldMatrix() {
	// 	return glm::inverse(getWorldToLocalMatrix());
	// }
};