#include "./Light.hxx"
#include "Pluto/Camera/Camera.hxx"
#include "Pluto/Material/Material.hxx"
#include <math.h>

Light Light::lights[MAX_LIGHTS];
LightStruct* Light::pinnedMemory;
std::map<std::string, uint32_t> Light::lookupTable;
vk::Buffer Light::SSBO;
vk::DeviceMemory Light::SSBOMemory;
vk::Buffer Light::stagingSSBO;
vk::DeviceMemory Light::stagingSSBOMemory;
std::vector<Camera*> Light::shadowCameras;
std::mutex Light::creation_mutex;
bool Light::Initialized = false;

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

void Light::cast_shadows(bool enable_cast_shadows)
{
    if (enable_cast_shadows) {
        light_struct.flags |= (1 << 2);
    }
    else
    {
        light_struct.flags &= (~(1 << 2));
    }
}

bool Light::should_cast_shadows()
{
    if ((light_struct.flags & (1 << 2)) != 0)
        return true;
    return false;
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
    if (IsInitialized()) return;

    auto vulkan = Libraries::Vulkan::Get();
    if (!vulkan->is_initialized())
        throw std::runtime_error( std::string("Vulkan library is not initialized"));

    auto device = vulkan->get_device();
    if (device == vk::Device())
        throw std::runtime_error( std::string("Invalid vulkan device"));

    auto physical_device = vulkan->get_physical_device();
    if (physical_device == vk::PhysicalDevice())
        throw std::runtime_error( std::string("Invalid vulkan physical device"));

    {
        vk::BufferCreateInfo bufferInfo = {};
        bufferInfo.size = MAX_LIGHTS * sizeof(LightStruct);
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
        bufferInfo.size = MAX_LIGHTS * sizeof(LightStruct);
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

bool Light::IsInitialized()
{
    return Initialized;
}

void Light::CreateShadowCameras()
{
    // Right, Left, Up, Down, Far, Near
    /* Create shadow map textures */
    for (uint32_t i = 0; i < MAX_LIGHTS; ++i) {
        auto cam = Camera::Create("ShadowCam_" + std::to_string(i), 1024, 1024, 1, 6, false, false);
        cam->set_perspective_projection(3.14f * .5f, 1.f, 1.f, .1f, 0);
        cam->set_perspective_projection(3.14f * .5f, 1.f, 1.f, .1f, 1);
        cam->set_perspective_projection(3.14f * .5f, 1.f, 1.f, .1f, 2);
        cam->set_perspective_projection(3.14f * .5f, 1.f, 1.f, .1f, 3);
        cam->set_perspective_projection(3.14f * .5f, 1.f, 1.f, .1f, 4);
        cam->set_perspective_projection(3.14f * .5f, 1.f, 1.f, .1f, 5);
        cam->set_view(glm::lookAt(glm::vec3(0.0, 0, 0), glm::vec3(-1,  0,  0), glm::vec3(0.0, 0.0, 1.0)), 0);
        cam->set_view(glm::lookAt(glm::vec3(0.0, 0, 0), glm::vec3( 1,  0,  0), glm::vec3(0.0, 0.0, 1.0)), 1);
        cam->set_view(glm::lookAt(glm::vec3(0.0, 0, 0), glm::vec3( 0,  0,  1), glm::vec3(0.0, 1.0, 0.0)), 2);
        cam->set_view(glm::lookAt(glm::vec3(0.0, 0, 0), glm::vec3( 0, 0,  -1), glm::vec3(0.0, -1.0, 0.0)), 3);
        cam->set_view(glm::lookAt(glm::vec3(0.0, 0, 0), glm::vec3( 0,  -1,  0), glm::vec3(0.0, 0.0, 1.0)), 4);
        cam->set_view(glm::lookAt(glm::vec3(0.0, 0, 0), glm::vec3( 0,  1,  0), glm::vec3(0.0, 0.0, 1.0)), 5);
        cam->set_render_order(-1);
        cam->force_render_mode(RenderMode::SHADOWMAP);
        shadowCameras.push_back(cam);
    }
}

void Light::UploadSSBO(vk::CommandBuffer command_buffer)
{
    auto vulkan = Libraries::Vulkan::Get();
    auto device = vulkan->get_device();

    if (SSBOMemory == vk::DeviceMemory()) return;
    if (stagingSSBOMemory == vk::DeviceMemory()) return;
    
    auto bufferSize = MAX_LIGHTS * sizeof(LightStruct);

    pinnedMemory = (LightStruct*) device.mapMemory(stagingSSBOMemory, 0, bufferSize);

    if (pinnedMemory == nullptr) return;
    LightStruct light_structs[MAX_LIGHTS];
    
    /* TODO: remove this for loop */
    for (int i = 0; i < MAX_LIGHTS; ++i) {
        if (!lights[i].is_initialized()) continue;

        light_structs[i] = lights[i].light_struct;
    };

    /* Copy to GPU mapped memory */
    memcpy(pinnedMemory, light_structs, sizeof(light_structs));

    device.unmapMemory(stagingSSBOMemory);

    vk::BufferCopy copyRegion;
	copyRegion.size = bufferSize;
    command_buffer.copyBuffer(stagingSSBO, SSBO, copyRegion);
}

vk::Buffer Light::GetSSBO()
{
    if ((SSBO != vk::Buffer()) && (SSBOMemory != vk::DeviceMemory()))
        return SSBO;
    else return vk::Buffer();
}

uint32_t Light::GetSSBOSize()
{
    return MAX_LIGHTS * sizeof(LightStruct);
}

void Light::CleanUp()
{
    if (!IsInitialized()) return;

    for (auto &light : lights) 
    {
        light.initialized = false;
    }

    auto vulkan = Libraries::Vulkan::Get();
    if (!vulkan->is_initialized())
        throw std::runtime_error( std::string("Vulkan library is not initialized"));
    auto device = vulkan->get_device();
    if (device == vk::Device())
        throw std::runtime_error( std::string("Invalid vulkan device"));

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
Light* Light::Create(std::string name) {
    std::lock_guard<std::mutex> lock(creation_mutex);
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
