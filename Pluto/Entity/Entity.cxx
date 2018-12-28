#include "./Entity.hxx"

#pragma optimize("", off)


Entity Entity::entities[MAX_ENTITIES];
EntityStruct* Entity::pinnedMemory;
std::map<std::string, uint32_t> Entity::lookupTable;
vk::Buffer Entity::ssbo;
vk::DeviceMemory Entity::ssboMemory;
std::map<std::string, uint32_t> Entity::windowToEntity;

    
int32_t Entity::GetEntityFromWindow(std::string key)
{
    auto it = windowToEntity.find(key);
    bool doesConnectionExist = (it != windowToEntity.end());

    if (!doesConnectionExist) 
        return -1;
    
    return (int32_t) windowToEntity[key];
}

bool Entity::connect_to_window(std::string key) {
    if (!initialized) return false;
    windowToEntity[key] = this->id;
    return true;
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
    auto device = vulkan->get_device();
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

bool Entity::Delete(std::string name) {
    return StaticFactory::Delete(name, "Entity", lookupTable, entities, MAX_ENTITIES);
}

bool Entity::Delete(uint32_t id) {
    return StaticFactory::Delete(id, "Entity", lookupTable, entities, MAX_ENTITIES);
}

Entity* Entity::GetFront() {
    return entities;
}

uint32_t Entity::GetCount() {
    return MAX_ENTITIES;
}
