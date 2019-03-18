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

#include <string>
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
	/* TODO */
	static std::mutex creation_mutex;
	static Entity entities[MAX_ENTITIES];
	static EntityStruct* pinnedMemory;
    static std::map<std::string, uint32_t> lookupTable;
    static vk::Buffer SSBO;
    static vk::DeviceMemory SSBOMemory;
	static vk::Buffer stagingSSBO;
    static vk::DeviceMemory stagingSSBOMemory;

	static std::map<std::string, uint32_t> windowToEntity;
	static std::map<uint32_t, std::string> entityToWindow;
	static uint32_t entityToVR;

public:
	static Entity* Create(std::string name);
	static Entity* Get(std::string name);
	static Entity* Get(uint32_t id);
	static Entity* GetFront();
	static uint32_t GetCount();
	static void Delete(std::string name);
	static void Delete(uint32_t id);
	
    static void Initialize();
    static void UploadSSBO(vk::CommandBuffer command_buffer);
    static vk::Buffer GetSSBO();
	static uint32_t GetSSBOSize();
    static void CleanUp();	

	Entity();

	Entity(std::string name, uint32_t id);

	std::string to_string();

	void connect_to_vr();

	static int32_t GetEntityFromWindow(std::string key);

	static int32_t GetEntityForVR();

	void set_transform(int32_t transform_id);

	void set_transform(Transform* transform);

	void clear_transform();

	int32_t get_transform();

	void set_camera(int32_t camera_id);

	void set_camera(Camera *camera);

	void clear_camera();

	int32_t get_camera();

	void set_material(int32_t material_id);

	void set_material(Material *material);

	void clear_material();

	int32_t get_material();

	void set_light(int32_t light_id);

	void set_light(Light* light);

	void clear_light();

	int32_t get_light();

	void set_mesh(int32_t mesh_id);

	void set_mesh(Mesh* mesh);

	void clear_mesh();

	int32_t get_mesh();

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
