#include "./Light.hxx"
#include "Pluto/Camera/Camera.hxx"
#include <math.h>

Light Light::lights[MAX_LIGHTS];
LightStruct* Light::pinnedMemory;
std::map<std::string, uint32_t> Light::lookupTable;
vk::Buffer Light::ssbo;
vk::DeviceMemory Light::ssboMemory;
std::vector<Camera*> Light::shadowCameras;

Light::Light()
{
    this->initialized = false;
}

Light::Light(std::string name, uint32_t id)
{
    this->initialized = true;
    this->name = name;
    this->id = id;
    this->light_struct.coneAngle = 0.0;
    this->light_struct.coneSoftness = 0.5;
    this->light_struct.type = 0;
    this->light_struct.ambient = glm::vec4(1.0, 1.0, 1.0, 1.0);
    this->light_struct.color = glm::vec4(1.0, 1.0, 1.0, 1.0);
    this->light_struct.intensity = 1.0;
    this->light_struct.flags = 0;
}

void Light::set_color(float r, float g, float b)
{
    light_struct.color = glm::vec4(r, g, b, 1.0);
}

void Light::set_temperature(float kelvin)
{
    float temp = kelvin / 100.0f;

    float red, green, blue;

    if ( temp <= 66 ){ 
        red = 255;         
        green = temp;
        green = 99.4708025861f * logf(green) - 161.1195681661f;

        if( temp <= 19){
            blue = 0;
        } else {
            blue = temp-10.f;
            blue = 138.5177312231f * logf(blue) - 305.0447927307f;
        }
    } else {
        red = temp - 60.f;
        red = 329.698727446f * powf(red, -0.1332047592f);
        
        green = temp - 60;
        green = 288.1221695283f * powf(green, -0.0755148492f );

        blue = 255;
    }

    light_struct.color = glm::vec4(red / 255.f, green / 255.f, blue / 255.f, 1.0);
}

void Light::set_intensity(float intensity)
{
    light_struct.intensity = intensity;
}

void Light::set_double_sided(bool double_sided)
{
    if (double_sided) {
        light_struct.flags |= (1 << 0);
    }
    else
    {
        light_struct.flags &= (~(1 << 0));
    }
}

void Light::show_end_caps(bool show_end_caps)
{
    if (show_end_caps) {
        light_struct.flags |= (1 << 1);
    }
    else
    {
        light_struct.flags &= (~(1 << 1));
    }
}

void Light::set_cone_angle(float angle)
{
    light_struct.coneAngle = angle;
}

void Light::set_cone_softness(float softness)
{
    light_struct.coneSoftness = softness;
}

void Light::use_point()
{
    light_struct.type = 0;
}

void Light::use_plane()
{
    light_struct.type = 1;
}

void Light::use_disk()
{
    light_struct.type = 2;
}

void Light::use_rod()
{
    light_struct.type = 3;
}

void Light::use_sphere()
{
    light_struct.type = 4;
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

void Light::CreateShadowCameras()
{
    /* Create shadow map textures */
    for (uint32_t i = 0; i < MAX_LIGHTS; ++i) {
        shadowCameras.push_back(Camera::Create("ShadowCam_" + std::to_string(i), true, true, 512, 512));
    }
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
