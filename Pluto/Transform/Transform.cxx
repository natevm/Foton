#include "./Transform.hxx"

Transform Transform::transforms[MAX_TRANSFORMS];
TransformStruct* Transform::pinnedMemory;
std::map<std::string, uint32_t> Transform::lookupTable;
vk::Buffer Transform::transformSSBO;
vk::DeviceMemory Transform::transformSSBOMemory;

void Transform::Initialize()
{
    auto vulkan = Libraries::Vulkan::Get();
    auto device = vulkan->get_device();
    auto physical_device = vulkan->get_physical_device();

    vk::BufferCreateInfo bufferInfo = {};
    bufferInfo.size = MAX_TRANSFORMS * sizeof(TransformStruct);
    bufferInfo.usage = vk::BufferUsageFlagBits::eStorageBuffer;
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;
    transformSSBO = device.createBuffer(bufferInfo);

    vk::MemoryRequirements memReqs = device.getBufferMemoryRequirements(transformSSBO);
    vk::MemoryAllocateInfo allocInfo = {};
    allocInfo.allocationSize = memReqs.size;

    vk::PhysicalDeviceMemoryProperties memProperties = physical_device.getMemoryProperties();
    vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
    allocInfo.memoryTypeIndex = vulkan->find_memory_type(memReqs.memoryTypeBits, properties);

    transformSSBOMemory = device.allocateMemory(allocInfo);
    device.bindBufferMemory(transformSSBO, transformSSBOMemory, 0);

    /* Pin the buffer */
    pinnedMemory = (TransformStruct*) device.mapMemory(transformSSBOMemory, 0, MAX_TRANSFORMS * sizeof(TransformStruct));
}

void Transform::UploadSSBO() 
{
    if (pinnedMemory == nullptr) return;
    TransformStruct transformObjects[MAX_TRANSFORMS];
    
    /* TODO: remove this for loop */
    for (int i = 0; i < MAX_TRANSFORMS; ++i) {
        if (!transforms[i].is_initialized()) continue;

        /* TODO: account for parent transforms */
        transformObjects[i].worldToLocal = transforms[i].parent_to_local_matrix();
        transformObjects[i].localToWorld = transforms[i].local_to_parent_matrix();
        transformObjects[i].worldToLocalRotation = transforms[i].parent_to_local_rotation();
        transformObjects[i].localToWorldRotation = transforms[i].local_to_parent_rotation();
        transformObjects[i].worldToLocalTranslation = transforms[i].parent_to_local_position();
        transformObjects[i].localToWorldTranslation = transforms[i].local_to_parent_position();
        transformObjects[i].worldToLocalScale = transforms[i].parent_to_local_scale();
        transformObjects[i].localToWorldScale = transforms[i].local_to_parent_scale();
    };

    /* Copy to GPU mapped memory */
    memcpy(pinnedMemory, transformObjects, sizeof(transformObjects));
}

vk::Buffer Transform::GetSSBO() 
{
    return transformSSBO;
}

uint32_t Transform::GetSSBOSize()
{
    return MAX_TRANSFORMS * sizeof(TransformStruct);
}

void Transform::CleanUp() 
{
    auto vulkan = Libraries::Vulkan::Get();
    auto device = vulkan->get_device();
    device.destroyBuffer(transformSSBO);
    device.unmapMemory(transformSSBOMemory);
    device.freeMemory(transformSSBOMemory);
}


/* Static Factory Implementations */
Transform* Transform::Create(std::string name) {
    return StaticFactory::Create(name, "Transform", lookupTable, transforms, MAX_TRANSFORMS);
}

Transform* Transform::Get(std::string name) {
    return StaticFactory::Get(name, "Transform", lookupTable, transforms, MAX_TRANSFORMS);
}

Transform* Transform::Get(uint32_t id) {
    return StaticFactory::Get(id, "Transform", lookupTable, transforms, MAX_TRANSFORMS);
}

void Transform::Delete(std::string name) {
    StaticFactory::Delete(name, "Transform", lookupTable, transforms, MAX_TRANSFORMS);
}

void Transform::Delete(uint32_t id) {
    StaticFactory::Delete(id, "Transform", lookupTable, transforms, MAX_TRANSFORMS);
}

Transform* Transform::GetFront() {
    return transforms;
}

uint32_t Transform::GetCount() {
    return MAX_TRANSFORMS;
}
