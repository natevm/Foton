#pragma once
#include <set>

#include "Tools/Options.hxx"

#include "Libraries/GLFW/GLFW.hxx"
#include "Libraries/Vulkan/Vulkan.hxx"
#include "Libraries/OpenVR/OpenVR.hxx"

#include "Pluto/Camera/Camera.hxx"
#include "Pluto/Texture/Texture.hxx"
#include "Pluto/Transform/Transform.hxx"
#include "Pluto/Material/Material.hxx"
#include "Pluto/Mesh/Mesh.hxx"
#include "Pluto/Light/Light.hxx"
#include "Pluto/Entity/Entity.hxx"

#include "Systems/EventSystem/EventSystem.hxx"
#include "Systems/RenderSystem/RenderSystem.hxx"

void Initialize(
    bool useGLFW = true, 
    bool useOpenVR = true,
    std::set<std::string> validation_layers = {"VK_LAYER_LUNARG_standard_validation"}, 
    std::set<std::string> instance_extensions = {}, 
    std::set<std::string> device_extensions = {}, 
    std::set<std::string> device_features = {"vertexPipelineStoresAndAtomics", "fragmentStoresAndAtomics"}
) {
    auto glfw = Libraries::GLFW::Get();
    auto vulkan = Libraries::Vulkan::Get();

    auto event_system = Systems::EventSystem::Get();
    auto render_system = Systems::RenderSystem::Get();

    if (useGLFW) event_system->create_window("Window", 512, 512, true, true, true);
    vulkan->create_instance(validation_layers.size() > 0, validation_layers, instance_extensions, useOpenVR);
    
    auto surface = (useGLFW) ? glfw->create_vulkan_surface(vulkan, "Window") : vk::SurfaceKHR();
    vulkan->create_device(device_extensions, device_features, 8, surface, useOpenVR);
    if (useGLFW) glfw->create_vulkan_swapchain("Window", useOpenVR);

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
    transform->set_scale(100, 100, 100);
    skybox->set_mesh(plane);
    skybox->set_transform(transform);
}

void CleanUp()
{
    auto glfw = Libraries::GLFW::Get();
    auto vulkan = Libraries::Vulkan::Get();
    auto openvr = Libraries::Vulkan::Get();
    
    Material::CleanUp();
    Transform::CleanUp();
    Light::CleanUp();
    Camera::CleanUp();
    Entity::CleanUp();
}