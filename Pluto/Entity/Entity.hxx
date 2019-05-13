// ┌──────────────────────────────────────────────────────────────────┐
// │  Entity                                                          │
// └──────────────────────────────────────────────────────────────────┘

#pragma once

#include "Pluto/Tools/StaticFactory.hxx"
#include "Pluto/Libraries/Vulkan/Vulkan.hxx"
#include "Pluto/Entity/EntityStruct.hxx"

class Camera;
class Transform;
class Material;
class Light;
class Mesh;
class RigidBody;
class Collider;

#include <string>
class Entity : public StaticFactory {
	friend class StaticFactory;
private:
	/* If an entity isn't active, its callbacks arent called */
	bool active = true;

	EntityStruct entity_struct;

	//std::shared_ptr<Callbacks> callbacks;
	//std::map<std::type_index, std::vector<std::shared_ptr<Component>>> components;
	
	/* Static fields */
	/* TODO */
	static std::mutex creation_mutex;
	/* TODO */
	static bool Initialized;
	
	static Entity entities[MAX_ENTITIES];
	static EntityStruct* pinnedMemory;
    static std::map<std::string, uint32_t> lookupTable;
    static vk::Buffer SSBO;
    static vk::DeviceMemory SSBOMemory;
	static vk::Buffer stagingSSBO;
    static vk::DeviceMemory stagingSSBOMemory;

	static std::map<std::string, uint32_t> windowToEntity;
	static std::map<uint32_t, std::string> entityToWindow;

	Entity();
	Entity(std::string name, uint32_t id);

public:
	static Entity* Create(std::string name, 
		Transform* transform = nullptr, 
		Camera* camera = nullptr,
		Material* material = nullptr,
		Light* light = nullptr,
		Mesh* mesh = nullptr,
		RigidBody* rigid_body = nullptr,
		Collider* collider = nullptr
	);
	static Entity* Get(std::string name);
	static Entity* Get(uint32_t id);
	static Entity* GetFront();
	static uint32_t GetCount();
	static void Delete(std::string name);
	static void Delete(uint32_t id);
	
    static void Initialize();
	static bool IsInitialized();
    static void UploadSSBO(vk::CommandBuffer command_buffer);
    static vk::Buffer GetSSBO();
	static uint32_t GetSSBOSize();
    static void CleanUp();	

	std::string to_string();
	
	void set_rigid_body(int32_t rigid_body_id);
	void set_rigid_body(RigidBody* rigid_body);
	void clear_rigid_body();
	int32_t get_rigid_body_id();
	RigidBody* get_rigid_body();
	RigidBody* rigid_body();

	void set_collider(int32_t collider_id);
	void set_collider(Collider* collider);
	void clear_collider();
	int32_t get_collider_id();
	Collider* get_collider();
	Collider* collider();

	void set_transform(int32_t transform_id);
	void set_transform(Transform* transform);
	void clear_transform();
	int32_t get_transform_id();
	Transform* get_transform();
	Transform* transform();

	void set_camera(int32_t camera_id);
	void set_camera(Camera *camera);
	void clear_camera();
	int32_t get_camera_id();
	Camera* get_camera();
	Camera* camera();

	void set_material(int32_t material_id);
	void set_material(Material *material);
	void clear_material();
	int32_t get_material_id();
	Material* get_material();
	Material* material();

	void set_light(int32_t light_id);
	void set_light(Light* light);
	void clear_light();
	int32_t get_light_id();
	Light* get_light();
	Light* light();

	void set_mesh(int32_t mesh_id);
	void set_mesh(Mesh* mesh);
	void clear_mesh();
	int32_t get_mesh_id();
	Mesh* get_mesh();
	Mesh* mesh();
};
