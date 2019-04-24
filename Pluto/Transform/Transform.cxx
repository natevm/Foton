#include "./Transform.hxx"

Transform Transform::transforms[MAX_TRANSFORMS];
TransformStruct* Transform::pinnedMemory;
std::map<std::string, uint32_t> Transform::lookupTable;
vk::Buffer Transform::stagingSSBO;
vk::Buffer Transform::SSBO;
vk::DeviceMemory Transform::stagingSSBOMemory;
vk::DeviceMemory Transform::SSBOMemory;
std::mutex Transform::creation_mutex;
bool Transform::Initialized = false;

void Transform::Initialize()
{
    if (IsInitialized()) return;

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

    Initialized = true;
}

bool Transform::IsInitialized()
{
    return Initialized;
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

// get_local_to_world_matrix

        /* TODO: account for parent transforms */
        transformObjects[i].worldToLocal = transforms[i].world_to_local_matrix();
        transformObjects[i].localToWorld = transforms[i].local_to_world_matrix();
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
    if (!IsInitialized()) return;

    for (auto &transform : transforms) {
		if (transform.initialized) {
			Transform::Delete(transform.id);
		}
	}

    auto vulkan = Libraries::Vulkan::Get();
    auto device = vulkan->get_device();
    device.destroyBuffer(SSBO);
    device.freeMemory(SSBOMemory);

    device.destroyBuffer(stagingSSBO);
    device.freeMemory(stagingSSBOMemory);

    SSBO = vk::Buffer();
    SSBOMemory = vk::DeviceMemory();
    stagingSSBO = vk::Buffer();
    stagingSSBOMemory = vk::DeviceMemory();

    Initialized = false;
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

void Transform::set_parent(uint32_t parent) {
    if ((parent < 0) || (parent >= MAX_TRANSFORMS))
        throw std::runtime_error(std::string("Error: parent must be between 0 and ") + std::to_string(MAX_TRANSFORMS));
    
    if (parent == this->get_id())
        throw std::runtime_error(std::string("Error: a component cannot be the parent of itself"));

    this->parent = parent;
    transforms[parent].children.insert(this->id);
}

void Transform::add_child(uint32_t object) {
    if ((object < 0) || (object >= MAX_TRANSFORMS))
        throw std::runtime_error(std::string("Error: child must be between 0 and ") + std::to_string(MAX_TRANSFORMS));
    
    if (object == this->get_id())
        throw std::runtime_error(std::string("Error: a component cannot be it's own child"));

    children.insert(object);
    transforms[object].parent = this->id;
}

void Transform::remove_child(uint32_t object) {
    children.erase(object);
}

/* Ideally these would be cheaper. Dont use matrix inversion. Bake these as the transform changes. */
glm::mat4 Transform::world_to_local_matrix() {
    glm::mat4 parentMatrix = glm::mat4(1.0);
    if (parent != -1) {
        parentMatrix = transforms[parent].world_to_local_matrix();
        return parent_to_local_matrix() * parentMatrix;
    }
    else return parent_to_local_matrix();
}

glm::mat4 Transform::local_to_world_matrix() {
    return glm::inverse(world_to_local_matrix());
}

glm::quat Transform::get_local_rotation() {
    glm::quat parentquat;
    if (parent != -1) {
        parentquat = transforms[parent].get_local_rotation();
        return rotation * parentquat;
    }
    else return parentToLocalRotation;
}

glm::quat Transform::get_world_rotation() {
    return glm::inverse(get_local_rotation());
}

glm::vec3 Transform::get_world_position() {
    auto mat = local_to_world_matrix();
    glm::vec4 p = mat[3];
    return glm::vec3(p.x, p.y, p.z);
}