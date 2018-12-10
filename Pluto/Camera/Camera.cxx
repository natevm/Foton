#include "./Camera.hxx"

#pragma optimize("", off)

int32_t Camera::current_camera = -1;
Camera Camera::cameras[MAX_CAMERAS];
CameraStruct* Camera::pinnedMemory;
std::map<std::string, uint32_t> Camera::lookupTable;
vk::Buffer Camera::ssbo;
vk::DeviceMemory Camera::ssboMemory;

bool Camera::MakeCurrent(std::string name) {
    if (StaticFactory::DoesItemExist(lookupTable, name)) {
        current_camera = lookupTable[name];
        return true;
    }

    std::cout << "Error, Camera \"" << name << "\" does not exist." << std::endl;
    return false;
}

Camera* Camera::GetCurrent()
{
    if ((current_camera < 0) || (current_camera >= MAX_CAMERAS))
        return nullptr;
    
    if (cameras[current_camera].initialized == false)
        return nullptr;

    return &cameras[current_camera];
}

/* SSBO Logic */
void Camera::Initialize()
{
    auto vulkan = Libraries::Vulkan::Get();
    auto device = vulkan->get_device();
    auto physical_device = vulkan->get_physical_device();

    vk::BufferCreateInfo bufferInfo = {};
    bufferInfo.size = MAX_CAMERAS * sizeof(CameraStruct);
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
    pinnedMemory = (CameraStruct*) device.mapMemory(ssboMemory, 0, MAX_CAMERAS * sizeof(CameraStruct));
}

void Camera::UploadSSBO()
{
    if (pinnedMemory == nullptr) return;
    
    /* TODO: remove this for loop */
    for (int i = 0; i < MAX_CAMERAS; ++i) {
        if (!cameras[i].is_initialized()) continue;
        pinnedMemory[i] = cameras[i].camera_struct;

        for (int j = 0; j < MAX_MULTIVIEW; ++j) {
            pinnedMemory[i].multiviews[j].viewinv = glm::inverse(pinnedMemory[i].multiviews[j].view);
            pinnedMemory[i].multiviews[j].projinv = glm::inverse(pinnedMemory[i].multiviews[j].proj);
        }
    };
}

vk::Buffer Camera::GetSSBO()
{
    return ssbo;
}

uint32_t Camera::GetSSBOSize()
{
    return MAX_CAMERAS * sizeof(CameraStruct);
}

void Camera::CleanUp()
{
    auto vulkan = Libraries::Vulkan::Get();
    auto device = vulkan->get_device();
    device.destroyBuffer(ssbo);
    device.unmapMemory(ssboMemory);
    device.freeMemory(ssboMemory);
}	


/* Static Factory Implementations */
Camera* Camera::Create(std::string name, bool allow_recording, bool cubemap, uint32_t tex_width, uint32_t tex_height)
{
    auto camera = StaticFactory::Create(name, "Camera", lookupTable, cameras, MAX_CAMERAS);
    camera->setup(allow_recording, cubemap, tex_width, tex_height);
    return camera;
}

Camera* Camera::Get(std::string name) {
    return StaticFactory::Get(name, "Camera", lookupTable, cameras, MAX_CAMERAS);
}

Camera* Camera::Get(uint32_t id) {
    return StaticFactory::Get(id, "Camera", lookupTable, cameras, MAX_CAMERAS);
}

bool Camera::Delete(std::string name) {
    return StaticFactory::Delete(name, "Camera", lookupTable, cameras, MAX_CAMERAS);
}

bool Camera::Delete(uint32_t id) {
    return StaticFactory::Delete(id, "Camera", lookupTable, cameras, MAX_CAMERAS);
}

Camera* Camera::GetFront() {
    return cameras;
}

uint32_t Camera::GetCount() {
    return MAX_CAMERAS;
}
