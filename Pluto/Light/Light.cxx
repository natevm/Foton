#include "./Light.hxx"

Light Light::lights[MAX_LIGHTS];
LightStruct* Light::pinnedMemory;
std::map<std::string, uint32_t> Light::lookupTable;
vk::Buffer Light::ssbo;
vk::DeviceMemory Light::ssboMemory;

/* SSBO logic */
void Light::Initialize()
{
    auto vulkan = Libraries::Vulkan::Get();
    auto device = vulkan->get_device();
    auto physical_device = vulkan->get_physical_device();

    vk::BufferCreateInfo bufferInfo = {};
    bufferInfo.size = MAX_LIGHTS * sizeof(LightStruct);
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
    pinnedMemory = (LightStruct*) device.mapMemory(ssboMemory, 0, MAX_LIGHTS * sizeof(LightStruct));
}

void Light::UploadSSBO()
{
    if (pinnedMemory == nullptr) return;
    LightStruct light_structs[MAX_LIGHTS];
    
    /* TODO: remove this for loop */
    for (int i = 0; i < MAX_LIGHTS; ++i) {
        if (!lights[i].is_initialized()) continue;

        light_structs[i] = lights[i].light_struct;
    };

    /* Copy to GPU mapped memory */
    memcpy(pinnedMemory, light_structs, sizeof(light_structs));
}

vk::Buffer Light::GetSSBO()
{
    return ssbo;
}

uint32_t Light::GetSSBOSize()
{
    return MAX_LIGHTS * sizeof(LightStruct);
}

void Light::CleanUp()
{
    auto vulkan = Libraries::Vulkan::Get();
    auto device = vulkan->get_device();
    device.destroyBuffer(ssbo);
    device.unmapMemory(ssboMemory);
    device.freeMemory(ssboMemory);
}	

/* Static Factory Implementations */
Light* Light::Create(std::string name) {
    return StaticFactory::Create(name, "Light", lookupTable, lights, MAX_LIGHTS);
}

Light* Light::Get(std::string name) {
    return StaticFactory::Get(name, "Light", lookupTable, lights, MAX_LIGHTS);
}

Light* Light::Get(uint32_t id) {
    return StaticFactory::Get(id, "Light", lookupTable, lights, MAX_LIGHTS);
}

bool Light::Delete(std::string name) {
    return StaticFactory::Delete(name, "Light", lookupTable, lights, MAX_LIGHTS);
}

bool Light::Delete(uint32_t id) {
    return StaticFactory::Delete(id, "Light", lookupTable, lights, MAX_LIGHTS);
}

Light* Light::GetFront() {
    return lights;
}

uint32_t Light::GetCount() {
    return MAX_LIGHTS;
}
