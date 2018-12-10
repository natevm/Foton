#pragma once
#include "Tools/Options.hxx"

#include "Libraries/GLFW/GLFW.hxx"
#include "Libraries/Vulkan/Vulkan.hxx"

#include "Pluto/Camera/Camera.hxx"
#include "Pluto/Texture/Texture.hxx"
#include "Pluto/Transform/Transform.hxx"
#include "Pluto/Material/Material.hxx"
#include "Pluto/Light/Light.hxx"
#include "Pluto/Entity/Entity.hxx"

#include "Systems/EventSystem/EventSystem.hxx"

void Initialize(
    bool useGLFW = true, 
    std::vector<std::string> validation_layers = {"VK_LAYER_LUNARG_standard_validation"}, 
    std::vector<std::string> instance_extensions = {}, 
    std::vector<std::string> device_extensions = {}, 
    std::vector<std::string> device_features = {"vertexPipelineStoresAndAtomics", "fragmentStoresAndAtomics"}
) {
    auto glfw = Libraries::GLFW::Get();
    auto vulkan = Libraries::Vulkan::Get();

    auto event_system = Systems::EventSystem::Get();

    if (useGLFW) event_system->create_window("Window");
    vulkan->create_instance(validation_layers.size() > 0, validation_layers, instance_extensions);
    
    auto surface = (useGLFW) ? glfw->create_vulkan_surface(vulkan, "Window") : vk::SurfaceKHR();
    vulkan->create_device(device_extensions, device_features, 8, surface);
    if (useGLFW) glfw->create_vulkan_swapchain("Window");

    /* Initialize Component Factories */
    Transform::Initialize();
    Light::Initialize();
    Camera::Initialize();
    Entity::Initialize();
    Material::Initialize();
}

void CleanUp()
{
    auto glfw = Libraries::GLFW::Get();
    auto vulkan = Libraries::Vulkan::Get();
    
    Material::CleanUp();
    Transform::CleanUp();
    Light::CleanUp();
    Camera::CleanUp();
    Entity::CleanUp();
}