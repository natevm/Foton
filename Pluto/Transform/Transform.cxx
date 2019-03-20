#include "./Transform.hxx"

Transform Transform::transforms[MAX_TRANSFORMS];
TransformStruct* Transform::pinnedMemory;
std::map<std::string, uint32_t> Transform::lookupTable;
vk::Buffer Transform::stagingSSBO;
vk::Buffer Transform::SSBO;
vk::DeviceMemory Transform::stagingSSBOMemory;
vk::DeviceMemory Transform::SSBOMemory;
std::mutex Transform::creation_mutex;

void Transform::Initialize()
{
    auto vulkan = Libraries::Vulkan::Get();
    auto device = vulkan->get_device();
    auto physical_device = vulkan->get_physical_device();

    {
        vk::BufferCreateInfo bufferInfo = {};
        bufferInfo.size = MAX_TRANSFORMS * sizeof(TransformStruct);
        bufferInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;
        bufferInfo.sharingMode = vk::SharingMode::eExclusive;
        stagingSSBO = device.createBuffer(bufferInfo);

        vk::MemoryRequirements memReqs = device.getBufferMemoryRequirements(stagingSSBO);
        vk::MemoryAllocateInfo allocInfo = {};
        allocInfo.allocationSize = memReqs.size;

        vk::PhysicalDeviceMemoryProperties memProperties = physical_device.getMemoryProperties();
        vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        allocInfo.memoryTypeIndex = vulkan->find_memory_type(memReqs.memoryTypeBits, properties);

        stagingSSBOMemory = device.allocateMemory(allocInfo);
        device.bindBufferMemory(stagingSSBO, stagingSSBOMemory, 0);
    }

    {
        vk::BufferCreateInfo bufferInfo = {};
        bufferInfo.size = MAX_TRANSFORMS * sizeof(TransformStruct);
        bufferInfo.usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst;
        bufferInfo.sharingMode = vk::SharingMode::eExclusive;
        SSBO = device.createBuffer(bufferInfo);

        vk::MemoryRequirements memReqs = device.getBufferMemoryRequirements(SSBO);
        vk::MemoryAllocateInfo allocInfo = {};
        allocInfo.allocationSize = memReqs.size;

        vk::PhysicalDeviceMemoryProperties memProperties = physical_device.getMemoryProperties();
        vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
        allocInfo.memoryTypeIndex = vulkan->find_memory_type(memReqs.memoryTypeBits, properties);

        SSBOMemory = device.allocateMemory(allocInfo);
        device.bindBufferMemory(SSBO, SSBOMemory, 0);
    }

}

void Transform::UploadSSBO(vk::CommandBuffer command_buffer) 
{
    auto vulkan = Libraries::Vulkan::Get();
    auto device = vulkan->get_device();

    if (SSBOMemory == vk::DeviceMemory()) return;
    if (stagingSSBOMemory == vk::DeviceMemory()) return;
    
    auto bufferSize = MAX_TRANSFORMS * sizeof(TransformStruct);

    pinnedMemory = (TransformStruct*) device.mapMemory(stagingSSBOMemory, 0, bufferSize);
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
        transformObjects[i].worldToLocalTranslation = transforms[i].parent_to_local_translation();
        transformObjects[i].localToWorldTranslation = transforms[i].local_to_parent_translation();
        transformObjects[i].worldToLocalScale = transforms[i].parent_to_local_scale();
        transformObjects[i].localToWorldScale = transforms[i].local_to_parent_scale();
    };

    /* Copy to GPU mapped memory */
    memcpy(pinnedMemory, transformObjects, sizeof(transformObjects));

    device.unmapMemory(stagingSSBOMemory);

	vk::BufferCopy copyRegion;
	copyRegion.size = bufferSize;
    command_buffer.copyBuffer(stagingSSBO, SSBO, copyRegion);

}

vk::Buffer Transform::GetSSBO() 
{
    if ((SSBO != vk::Buffer()) && (SSBOMemory != vk::DeviceMemory()))
        return SSBO;
    else return vk::Buffer();
}

uint32_t Transform::GetSSBOSize()
{
    return MAX_TRANSFORMS * sizeof(TransformStruct);
}

void Transform::CleanUp() 
{
    auto vulkan = Libraries::Vulkan::Get();
    auto device = vulkan->get_device();
    device.destroyBuffer(SSBO);
    device.freeMemory(SSBOMemory);

    device.destroyBuffer(stagingSSBO);
    device.freeMemory(stagingSSBOMemory);
}


/* Static Factory Implementations */
Transform* Transform::Create(std::string name) {
    std::lock_guard<std::mutex> lock(creation_mutex);
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
