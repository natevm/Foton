#include "./Entity.hxx"

#include "Pluto/Libraries/GLFW/GLFW.hxx"

Entity Entity::entities[MAX_ENTITIES];
EntityStruct* Entity::pinnedMemory;
std::map<std::string, uint32_t> Entity::lookupTable;
vk::Buffer Entity::ssbo;
vk::DeviceMemory Entity::ssboMemory;
std::map<std::string, uint32_t> Entity::windowToEntity;
std::map<uint32_t, std::string> Entity::entityToWindow;
uint32_t Entity::entityToVR = -1;

Entity::Entity() {
    this->initialized = false;
    entity_struct.initialized = false;
    entity_struct.transform_id = -1;
    entity_struct.camera_id = -1;
    entity_struct.material_id = -1;
    entity_struct.light_id = -1;
    entity_struct.mesh_id = -1;
}

Entity::Entity(std::string name, uint32_t id) {
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

std::string Entity::to_string()
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

void Entity::set_transform(int32_t transform_id) 
{
    if (transform_id < -1) 
        throw std::runtime_error( std::string("Transform id must be greater than or equal to -1"));
    this->entity_struct.transform_id = transform_id;
}

void Entity::set_transform(Transform* transform) 
{
    if (!transform) 
        throw std::runtime_error( std::string("Invalid transform handle."));
    this->entity_struct.transform_id = transform->get_id();
}

void Entity::clear_transform()
{
    this->entity_struct.transform_id = -1;
}

int32_t Entity::get_transform() 
{
    return this->entity_struct.transform_id;
}

void Entity::set_camera(int32_t camera_id) 
{
    if (camera_id < -1) 
        throw std::runtime_error( std::string("Camera id must be greater than or equal to -1"));
    this->entity_struct.camera_id = camera_id;
}

void Entity::set_camera(Camera *camera) 
{
    if (!camera)
        throw std::runtime_error( std::string("Invalid camera handle."));
    this->entity_struct.camera_id = camera->get_id();
}

void Entity::clear_camera()
{
    this->entity_struct.camera_id = -1;
}

int32_t Entity::get_camera() 
{
    return this->entity_struct.camera_id;
}

void Entity::set_material(int32_t material_id) 
{
    if (material_id < -1) 
        throw std::runtime_error( std::string("Material id must be greater than or equal to -1"));
    this->entity_struct.material_id = material_id;
}

void Entity::set_material(Material *material) 
{
    if (!material)
        throw std::runtime_error( std::string("Invalid material handle."));
    this->entity_struct.material_id = material->get_id();
}

void Entity::clear_material()
{
    this->entity_struct.material_id = -1;
}

int32_t Entity::get_material() 
{
    return this->entity_struct.material_id;
}

void Entity::set_light(int32_t light_id) 
{
    if (light_id < -1) 
        throw std::runtime_error( std::string("Light id must be greater than or equal to -1"));
    this->entity_struct.light_id = light_id;
}

void Entity::set_light(Light* light) 
{
    if (!light) 
        throw std::runtime_error( std::string("Invalid light handle."));
    this->entity_struct.light_id = light->get_id();
}

void Entity::clear_light()
{
    this->entity_struct.light_id = -1;
}

int32_t Entity::get_light() 
{
    return this->entity_struct.light_id;
}

void Entity::set_mesh(int32_t mesh_id) 
{
    if (mesh_id < -1) 
        throw std::runtime_error( std::string("Mesh id must be greater than or equal to -1"));
    this->entity_struct.mesh_id = mesh_id;
}

void Entity::set_mesh(Mesh* mesh) 
{
    if (!mesh) 
        throw std::runtime_error( std::string("Invalid mesh handle."));
    this->entity_struct.mesh_id = mesh->get_id();
}

void Entity::clear_mesh()
{
    this->entity_struct.mesh_id = -1;
}

int32_t Entity::get_mesh() 
{
    return this->entity_struct.mesh_id;
}

int32_t Entity::GetEntityFromWindow(std::string key)
{
    auto it = windowToEntity.find(key);
    bool doesConnectionExist = (it != windowToEntity.end());

    if (!doesConnectionExist) 
        return -1;
    
    return (int32_t) windowToEntity[key];
}

int32_t Entity::GetEntityForVR()
{
    return entityToVR;
}

void Entity::connect_to_window(std::string key) {
    auto glfw = Libraries::GLFW::Get();
    if (!glfw->does_window_exist(key))
        throw std::runtime_error( std::string("Window with key " + key + " does not exist"));

    if (!initialized) 
        throw std::runtime_error( std::string("Entity not initialized"));
    
    windowToEntity[key] = this->id;
    entityToWindow[this->id] = key;
}

std::string Entity::get_connected_window() {
    auto glfw = Libraries::GLFW::Get();

    if (!initialized) 
        throw std::runtime_error( std::string("Entity not initialized"));

    auto it = entityToWindow.find(this->id);
    if (it == entityToWindow.end()) return "";
    return entityToWindow[this->id];
}

void Entity::connect_to_vr()
{
    if (!initialized) 
        throw std::runtime_error( std::string("Entity not initialized"));
    
    entityToVR = this->id;
}

void Entity::setParent(uint32_t parent) {
    this->parent = parent;
    entities[parent].children.insert(this->id);
}

void Entity::addChild(uint32_t object) {
    children.insert(object);
    entities[object].parent = this->id;
}

void Entity::removeChild(uint32_t object) {
    children.erase(object);
}

/* SSBO logic */
void Entity::Initialize()
{
    auto vulkan = Libraries::Vulkan::Get();
    auto device = vulkan->get_device();
    auto physical_device = vulkan->get_physical_device();

    vk::BufferCreateInfo bufferInfo = {};
    bufferInfo.size = MAX_ENTITIES * sizeof(EntityStruct);
    bufferInfo.usage = vk::BufferUsageFlagBits::eStorageBuffer;
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;
    ssbo = device.createBuffer(bufferInfo);

    vk::MemoryRequirements memReqs = device.getBufferMemoryRequirements(ssbo);
    vk::MemoryAllocateInfo allocInfo = {};
    allocInfo.allocationSize = memReqs.size;

    vk::PhysicalDeviceMemoryProperties memProperties = physical_device.getMemoryProperties();
    vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
    allocInfo.memoryTypeIndex = vulkan->find_memory_type(memReqs.memoryTypeBits, properties);

    ssboMemory = device.allocateMemory(allocInfo);
    device.bindBufferMemory(ssbo, ssboMemory, 0);

    /* Pin the buffer */
    pinnedMemory = (EntityStruct*) device.mapMemory(ssboMemory, 0, MAX_ENTITIES * sizeof(EntityStruct));
}

void Entity::UploadSSBO()
{
    if (pinnedMemory == nullptr) return;
    EntityStruct entity_structs[MAX_ENTITIES];
    
    /* TODO: remove this for loop */
    for (int i = 0; i < MAX_ENTITIES; ++i) {
        // if (!entities[i].is_initialized()) continue;
        /* TODO: account for parent transforms */
        entity_structs[i] = entities[i].entity_struct;
    };

    auto device = Libraries::Vulkan::Get()->get_device();

    /* Copy to GPU mapped memory */
    memcpy(pinnedMemory, entity_structs, sizeof(entity_structs));
}

vk::Buffer Entity::GetSSBO()
{
    return ssbo;
}

uint32_t Entity::GetSSBOSize()
{
    return MAX_ENTITIES * sizeof(EntityStruct);
}

void Entity::CleanUp()
{
    auto vulkan = Libraries::Vulkan::Get();
    if (!vulkan->is_initialized())
        throw std::runtime_error( std::string("Vulkan library is not initialized"));
    auto device = vulkan->get_device();
    if (device == vk::Device())
        throw std::runtime_error( std::string("Invalid vulkan device"));
    
    device.destroyBuffer(ssbo);
    device.unmapMemory(ssboMemory);
    device.freeMemory(ssboMemory);
}	

/* Static Factory Implementations */
Entity* Entity::Create(std::string name) {
    return StaticFactory::Create(name, "Entity", lookupTable, entities, MAX_ENTITIES);
}

Entity* Entity::Get(std::string name) {
    return StaticFactory::Get(name, "Entity", lookupTable, entities, MAX_ENTITIES);
}

Entity* Entity::Get(uint32_t id) {
    return StaticFactory::Get(id, "Entity", lookupTable, entities, MAX_ENTITIES);
}

void Entity::Delete(std::string name) {
    StaticFactory::Delete(name, "Entity", lookupTable, entities, MAX_ENTITIES);
}

void Entity::Delete(uint32_t id) {
    StaticFactory::Delete(id, "Entity", lookupTable, entities, MAX_ENTITIES);
}

Entity* Entity::GetFront() {
    return entities;
}

uint32_t Entity::GetCount() {
    return MAX_ENTITIES;
}
