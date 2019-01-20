#include "./Light.hxx"

Light Light::lights[MAX_LIGHTS];
LightStruct* Light::pinnedMemory;
std::map<std::string, uint32_t> Light::lookupTable;
vk::Buffer Light::ssbo;
vk::DeviceMemory Light::ssboMemory;

Light::Light()
{
    this->initialized = false;
}

Light::Light(std::string name, uint32_t id)
{
    this->initialized = true;
    this->name = name;
    this->id = id;
}

void Light::set_color(float r, float g, float b)
{
    light_struct.ambient = glm::vec4(r, g, b, 1.0);
    light_struct.diffuse = glm::vec4(r, g, b, 1.0);
    light_struct.specular = glm::vec4(r, g, b, 1.0);
}

std::string Light::to_string() {
    std::string output;
    output += "{\n";
    output += "\ttype: \"Light\",\n";
    output += "\tname: \"" + name + "\"\n";
    output += "}";
    return output;
}

/* SSBO logic */
void Light::Initialize()
{
    auto vulkan = Libraries::Vulkan::Get();
    if (!vulkan->is_initialized())
        throw std::runtime_error( std::string("Vulkan library is not initialized"));

    auto device = vulkan->get_device();
    if (device == vk::Device())
        throw std::runtime_error( std::string("Invalid vulkan device"));

    auto physical_device = vulkan->get_physical_device();
    if (physical_device == vk::PhysicalDevice())
        throw std::runtime_error( std::string("Invalid vulkan physical device"));

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
Light* Light::Create(std::string name) {
    return StaticFactory::Create(name, "Light", lookupTable, lights, MAX_LIGHTS);
}

Light* Light::Get(std::string name) {
    return StaticFactory::Get(name, "Light", lookupTable, lights, MAX_LIGHTS);
}

Light* Light::Get(uint32_t id) {
    return StaticFactory::Get(id, "Light", lookupTable, lights, MAX_LIGHTS);
}

void Light::Delete(std::string name) {
    StaticFactory::Delete(name, "Light", lookupTable, lights, MAX_LIGHTS);
}

void Light::Delete(uint32_t id) {
    StaticFactory::Delete(id, "Light", lookupTable, lights, MAX_LIGHTS);
}

Light* Light::GetFront() {
    return lights;
}

uint32_t Light::GetCount() {
    return MAX_LIGHTS;
}
