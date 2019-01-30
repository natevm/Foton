#include "Pluto.hxx"

bool Initialized = false;
#include <thread>
#include <iostream>

#include "Tools/Options.hxx"

#include "Libraries/GLFW/GLFW.hxx"
#include "Libraries/Vulkan/Vulkan.hxx"
#if BUILD_OPENVR
#include "Libraries/OpenVR/OpenVR.hxx"
#endif

#include "Systems/EventSystem/EventSystem.hxx"
#include "Systems/PythonSystem/PythonSystem.hxx"
#include "Systems/RenderSystem/RenderSystem.hxx"

#include "Pluto/Camera/Camera.hxx"
#include "Pluto/Texture/Texture.hxx"
#include "Pluto/Transform/Transform.hxx"
#include "Pluto/Material/Material.hxx"
#include "Pluto/Mesh/Mesh.hxx"
#include "Pluto/Light/Light.hxx"
#include "Pluto/Entity/Entity.hxx"



void Initialize(
    bool useGLFW,
    bool useOpenVR,
    std::set<std::string> validation_layers,
    std::set<std::string> instance_extensions,
    std::set<std::string> device_extensions,
    std::set<std::string> device_features
) {
    if (Initialized) 
        throw std::runtime_error("Error: Pluto already initialized");
    Initialized = true;

    auto glfw = Libraries::GLFW::Get();
    auto vulkan = Libraries::Vulkan::Get();
    
    auto event_system = Systems::EventSystem::Get();
    auto render_system = Systems::RenderSystem::Get();

    event_system->use_openvr(useOpenVR);
    render_system->use_openvr(useOpenVR);

    if (useGLFW) event_system->create_window("Window", 1, 1, false, false, false, false);
    vulkan->create_instance(validation_layers.size() > 0, validation_layers, instance_extensions, useOpenVR);
    
    auto surface = (useGLFW) ? glfw->create_vulkan_surface(vulkan, "Window") : vk::SurfaceKHR();
    vulkan->create_device(device_extensions, device_features, 8, surface, useOpenVR);
    if (useGLFW) event_system->destroy_window("Window");
    
    /* Initialize Component Factories. Order is important. */
    Transform::Initialize();
    Light::Initialize();
    Camera::Initialize();
    Entity::Initialize();
    Texture::Initialize();
    Mesh::Initialize();
    Material::Initialize();

    auto skybox = Entity::Create("Skybox");
    auto plane = Mesh::Get("DefaultSphere");
    auto transform = Transform::Create("SkyboxTransform");
    auto material = Material::Create("SkyboxMaterial");
    material->show_environment();
    transform->set_scale(1000, 1000, 1000);
    skybox->set_mesh(plane);
    skybox->set_material(material);
    skybox->set_transform(transform);

#if BUILD_OPENVR
	if (useOpenVR) {
		auto ovr = Libraries::OpenVR::Get();
        ovr->create_eye_textures();
	}
#endif
}

void CleanUp()
{
    Initialized = false;

    auto glfw = Libraries::GLFW::Get();
    auto vulkan = Libraries::Vulkan::Get();
    auto openvr = Libraries::Vulkan::Get();

    if (vulkan->is_initialized())
    {
        Material::CleanUp();
        Transform::CleanUp();
        Light::CleanUp();
        Camera::CleanUp();
        Entity::CleanUp();
    }
}
