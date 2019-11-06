#include "./Light.hxx"
#include "Foton/Camera/Camera.hxx"
#include "Foton/Material/Material.hxx"
#include "Foton/Entity/Entity.hxx"
#include <math.h>

Light Light::lights[MAX_LIGHTS];
LightStruct* Light::pinnedMemory;
std::map<std::string, uint32_t> Light::lookupTable;

vk::Buffer Light::SSBO;
vk::DeviceMemory Light::SSBOMemory;
vk::Buffer Light::stagingSSBO;
vk::DeviceMemory Light::stagingSSBOMemory;

vk::Buffer Light::LightEntitiesSSBO;
vk::DeviceMemory Light::LightEntitiesSSBOMemory;
vk::Buffer Light::stagingLightEntitiesSSBO;
vk::DeviceMemory Light::stagingLightEntitiesSSBOMemory;

std::vector<Camera*> Light::shadowCameras;
std::shared_ptr<std::mutex> Light::creation_mutex;
bool Light::Initialized = false;
bool Light::Dirty = true;

uint32_t Light::numLightEntities = 0;

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
    this->light_struct.ambient = glm::vec4(1.0, 1.0, 1.0, 1.0);
    this->light_struct.color = glm::vec4(1.0, 1.0, 1.0, 1.0);
    this->light_struct.intensity = 1.0;
    this->light_struct.flags = 0;
    this->light_struct.softnessSamples = 20;
    this->light_struct.softnessRadius = .1;
    enable_vsm(true);
}

void Light::set_shadow_softness_samples(uint32_t samples)
{
    light_struct.softnessSamples = std::min(std::max(samples, (uint32_t)1), (uint32_t)20);
    mark_dirty();
}

void Light::set_shadow_softness_radius(float radius)
{
    light_struct.softnessRadius = std::max(radius, 0.0f);
    mark_dirty();
}

void Light::set_color(float r, float g, float b)
{
    light_struct.color = glm::vec4(r, g, b, 1.0);
    mark_dirty();
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
    mark_dirty();
}

void Light::set_intensity(float intensity)
{
    light_struct.intensity = intensity;
    mark_dirty();
}

void Light::set_double_sided(bool double_sided)
{
    if (double_sided) {
        light_struct.flags |= LIGHT_FLAGS_DOUBLE_SIDED;
    }
    else
    {
        light_struct.flags &= (~LIGHT_FLAGS_DOUBLE_SIDED);
    }
    mark_dirty();
}

void Light::show_end_caps(bool show_end_caps)
{
    if (show_end_caps) {
        light_struct.flags |= LIGHT_FLAGS_SHOW_END_CAPS;
    }
    else
    {
        light_struct.flags &= (~LIGHT_FLAGS_SHOW_END_CAPS);
    }
    mark_dirty();
}

void Light::disable(bool disabled) {
    if (disabled) {
        light_struct.flags |= LIGHT_FLAGS_DISABLED;
    }
    else
    {
        light_struct.flags &= (~LIGHT_FLAGS_DISABLED);
    }
    mark_dirty();
}

void Light::cast_dynamic_shadows(bool enable_cast_dynamic_shadows)
{
    castsDynamicShadows = enable_cast_dynamic_shadows;
    mark_dirty();
}

bool Light::should_cast_dynamic_shadows()
{
    return castsDynamicShadows;
}


void Light::cast_shadows(bool enable_cast_shadows)
{
    if (enable_cast_shadows) {
        light_struct.flags |= LIGHT_FLAGS_CAST_SHADOWS;
    }
    else
    {
        light_struct.flags &= (~LIGHT_FLAGS_CAST_SHADOWS);
    }
    mark_dirty();
}

bool Light::should_cast_shadows()
{
    if ((light_struct.flags & LIGHT_FLAGS_CAST_SHADOWS) != 0)
        return true;
    return false;
}

void Light::enable_vsm(bool enabled) {
    enableVSM = enabled;

    if (enabled) {
        light_struct.flags |= LIGHT_FLAGS_USE_VSM;
    }
    else
    {
        light_struct.flags &= (~LIGHT_FLAGS_USE_VSM);
    }
    mark_dirty();
}

bool Light::should_use_vsm() {
    return enableVSM;
}

void Light::set_cone_angle(float angle)
{
    light_struct.coneAngle = angle;
    mark_dirty();
}

void Light::set_cone_softness(float softness)
{
    light_struct.coneSoftness = softness;
    mark_dirty();
}

void Light::use_point()
{
    light_struct.flags &= (~LIGHT_FLAGS_SPHERE);
    light_struct.flags &= (~LIGHT_FLAGS_ROD);
    light_struct.flags &= (~LIGHT_FLAGS_DISK);
    light_struct.flags &= (~LIGHT_FLAGS_PLANE);
    light_struct.flags |= LIGHT_FLAGS_POINT;
    mark_dirty();
}

void Light::use_plane()
{
    light_struct.flags &= (~LIGHT_FLAGS_SPHERE);
    light_struct.flags &= (~LIGHT_FLAGS_ROD);
    light_struct.flags &= (~LIGHT_FLAGS_DISK);
    light_struct.flags &= (~LIGHT_FLAGS_POINT);
    light_struct.flags |= LIGHT_FLAGS_PLANE;
    mark_dirty();
}

void Light::use_disk()
{
    light_struct.flags &= (~LIGHT_FLAGS_SPHERE);
    light_struct.flags &= (~LIGHT_FLAGS_ROD);
    light_struct.flags &= (~LIGHT_FLAGS_POINT);
    light_struct.flags &= (~LIGHT_FLAGS_PLANE);
    light_struct.flags |= LIGHT_FLAGS_DISK;
    mark_dirty();
}

void Light::use_rod()
{
    light_struct.flags &= (~LIGHT_FLAGS_SPHERE);
    light_struct.flags &= (~LIGHT_FLAGS_POINT);
    light_struct.flags &= (~LIGHT_FLAGS_DISK);
    light_struct.flags &= (~LIGHT_FLAGS_PLANE);
    light_struct.flags |= LIGHT_FLAGS_ROD;
    mark_dirty();
}

void Light::use_sphere()
{
    light_struct.flags &= (~LIGHT_FLAGS_POINT);
    light_struct.flags &= (~LIGHT_FLAGS_ROD);
    light_struct.flags &= (~LIGHT_FLAGS_DISK);
    light_struct.flags &= (~LIGHT_FLAGS_PLANE);
    light_struct.flags |= LIGHT_FLAGS_SPHERE;
    mark_dirty();
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

    {
        vk::BufferCreateInfo bufferInfo = {};
        bufferInfo.size = MAX_LIGHTS * sizeof(int32_t);
        bufferInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;
        bufferInfo.sharingMode = vk::SharingMode::eExclusive;
        stagingLightEntitiesSSBO = device.createBuffer(bufferInfo);

        vk::MemoryRequirements memReqs = device.getBufferMemoryRequirements(stagingLightEntitiesSSBO);
        vk::MemoryAllocateInfo allocInfo = {};
        allocInfo.allocationSize = memReqs.size;

        vk::PhysicalDeviceMemoryProperties memProperties = physical_device.getMemoryProperties();
        vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        allocInfo.memoryTypeIndex = vulkan->find_memory_type(memReqs.memoryTypeBits, properties);

        stagingLightEntitiesSSBOMemory = device.allocateMemory(allocInfo);
        device.bindBufferMemory(stagingLightEntitiesSSBO, stagingLightEntitiesSSBOMemory, 0);
    }

    {
        vk::BufferCreateInfo bufferInfo = {};
        bufferInfo.size = MAX_LIGHTS * sizeof(int32_t);
        bufferInfo.usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst;
        bufferInfo.sharingMode = vk::SharingMode::eExclusive;
        LightEntitiesSSBO = device.createBuffer(bufferInfo);

        vk::MemoryRequirements memReqs = device.getBufferMemoryRequirements(LightEntitiesSSBO);
        vk::MemoryAllocateInfo allocInfo = {};
        allocInfo.allocationSize = memReqs.size;

        vk::PhysicalDeviceMemoryProperties memProperties = physical_device.getMemoryProperties();
        vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
        allocInfo.memoryTypeIndex = vulkan->find_memory_type(memReqs.memoryTypeBits, properties);

        LightEntitiesSSBOMemory = device.allocateMemory(allocInfo);
        device.bindBufferMemory(LightEntitiesSSBO, LightEntitiesSSBOMemory, 0);
    }

    creation_mutex = std::make_shared<std::mutex>();
    numLightEntities = 0;

    Initialized = true;
}

bool Light::IsInitialized()
{
    return Initialized;
}

uint32_t Light::GetNumActiveLights()
{
    return numLightEntities;
}

void Light::CreateShadowCameras()
{
    std::thread t([](){
        // Right, Left, Up, Down, Far, Near
        /* Create shadow map textures */
        for (uint32_t i = 0; i < MAX_LIGHTS; ++i) {
            auto cam = Camera::Create("ShadowCam_" + std::to_string(i), 256, 256, 1, 6, false, false);
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
            cam->force_render_mode(RenderMode::RENDER_MODE_SHADOWMAP);
            shadowCameras.push_back(cam);
        }
    });

    t.detach();
}

void Light::UploadSSBO(vk::CommandBuffer command_buffer)
{
    if (!Dirty) return;
    Dirty = false;
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


    /* Get light list */

    /* Find shadow maps */
    std::vector<Camera*> shadowCams;
    Camera* cameras = Camera::GetFront();
    for (uint32_t c_id = 0; c_id < Camera::GetCount(); ++c_id) {
        if (!cameras[c_id].is_initialized()) continue;
        if (cameras[c_id].get_name().find("ShadowCam_") != std::string::npos)
            shadowCams.push_back(&cameras[c_id]);
    }
    // if (shadowCams.size() < MAX_LIGHTS) return false;

    numLightEntities = 0;
    auto entities = Entity::GetFront();
    std::vector<int32_t> light_entity_ids(MAX_LIGHTS, -1);
    for (uint32_t i = 0; i < Entity::GetCount(); ++i)
    {
        if (entities[i].is_initialized() && (entities[i].get_light() != nullptr))
        {
            if (shadowCams.size() > numLightEntities) {
                entities[i].set_camera(shadowCams[numLightEntities]);
                light_entity_ids[numLightEntities] = i;
                numLightEntities++;
            }
        }
        if (numLightEntities == MAX_LIGHTS)
            break;
    }
    {
        int32_t* pinnedMemory = (int32_t*) device.mapMemory(stagingLightEntitiesSSBOMemory, 0, light_entity_ids.size() * sizeof(int32_t));
        memcpy(pinnedMemory, light_entity_ids.data(), light_entity_ids.size() * sizeof(int32_t));
        device.unmapMemory(stagingLightEntitiesSSBOMemory);

        vk::BufferCopy copyRegion;
        copyRegion.size = light_entity_ids.size() * sizeof(int32_t);
        command_buffer.copyBuffer(stagingLightEntitiesSSBO, LightEntitiesSSBO, copyRegion);
    }
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

vk::Buffer Light::GetLightEntitiesSSBO()
{
    if ((LightEntitiesSSBO != vk::Buffer()) && (LightEntitiesSSBOMemory != vk::DeviceMemory()))
        return LightEntitiesSSBO;
    else return vk::Buffer();
}

uint32_t Light::GetLightEntitiesSSBOSize()
{
    return MAX_LIGHTS * sizeof(uint32_t);
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
    auto l = StaticFactory::Create(creation_mutex, name, "Light", lookupTable, lights, MAX_LIGHTS);
    Dirty = true;
    return l;
}

Light* Light::CreateTemperature(std::string name, float kelvin, float intensity) {
    auto light = StaticFactory::Create(creation_mutex, name, "Light", lookupTable, lights, MAX_LIGHTS);
    light->set_temperature(kelvin);
    light->set_intensity(intensity);
    return light;
}

Light* Light::CreateRGB(std::string name, float r, float g, float b, float intensity) {
    auto light = StaticFactory::Create(creation_mutex, name, "Light", lookupTable, lights, MAX_LIGHTS);
    light->set_color(r, g, b);
    light->set_intensity(intensity);
    return light;
}

Light* Light::Get(std::string name) {
    return StaticFactory::Get(creation_mutex, name, "Light", lookupTable, lights, MAX_LIGHTS);
}

Light* Light::Get(uint32_t id) {
    return StaticFactory::Get(creation_mutex, id, "Light", lookupTable, lights, MAX_LIGHTS);
}

void Light::Delete(std::string name) {
    StaticFactory::Delete(creation_mutex, name, "Light", lookupTable, lights, MAX_LIGHTS);
    Dirty = true;
}

void Light::Delete(uint32_t id) {
    StaticFactory::Delete(creation_mutex, id, "Light", lookupTable, lights, MAX_LIGHTS);
    Dirty = true;
}

Light* Light::GetFront() {
    return lights;
}

uint32_t Light::GetCount() {
    return MAX_LIGHTS;
}
