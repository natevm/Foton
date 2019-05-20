#include "Pluto.hxx"

bool Initialized = false;
#include <thread>
#include <iostream>
#include <map>
#include <string>

#include "Tools/Options.hxx"

#include "Libraries/GLFW/GLFW.hxx"
#include "Libraries/Vulkan/Vulkan.hxx"
#include "Libraries/OpenVR/OpenVR.hxx"

#include "Systems/EventSystem/EventSystem.hxx"
#include "Systems/PythonSystem/PythonSystem.hxx"
#include "Systems/RenderSystem/RenderSystem.hxx"
#include "Systems/PhysicsSystem/PhysicsSystem.hxx"

#include "Pluto/Camera/Camera.hxx"
#include "Pluto/Texture/Texture.hxx"
#include "Pluto/Transform/Transform.hxx"
#include "Pluto/Material/Material.hxx"
#include "Pluto/Mesh/Mesh.hxx"
#include "Pluto/Light/Light.hxx"
#include "Pluto/Entity/Entity.hxx"

#include "Pluto/Prefabs/Prefabs.hxx"

#include <tiny_obj_loader.h>
#include <glm/glm.hpp>

#include <pbrtParser/impl/semantic/SemanticParser.h>

#include "stb_image_write.h"

namespace Pluto {

    std::thread callback_thread;
    void StartSystems(bool use_python, std::function<void()> callback)
    {
        auto glfw = Libraries::GLFW::Get();
        auto vulkan = Libraries::Vulkan::Get();

        auto python_system = Systems::PythonSystem::Get();
        auto event_system = Systems::EventSystem::Get();
        auto render_system = Systems::RenderSystem::Get();
        auto physics_system = Systems::PhysicsSystem::Get();

        if (use_python) {
            python_system->initialize();
        }
        event_system->initialize();
        render_system->initialize();
        physics_system->initialize();
        
        if (use_python) {
            python_system->start();
        }
        render_system->start();
        physics_system->start();

        //Initialize();
        if (callback)
        {
            if (callback_thread.joinable()) callback_thread.join();
            callback_thread = std::thread(callback);
        }

        event_system->start();
    }

    void WaitForStartupCallback()
    {
        if (callback_thread.joinable()) callback_thread.join();
    }

    void Initialize(
        bool useGLFW,
        bool useOpenVR,
        std::set<std::string> validation_layers,
        std::set<std::string> instance_extensions,
        std::set<std::string> device_extensions,
        std::set<std::string> device_features
    ) {
        if (Initialized) throw std::runtime_error("Error: Pluto already initialized");

        auto glfw = Libraries::GLFW::Get();
        // if (useGLFW && !glfw->is_initialized()) throw std::runtime_error("Error: GLFW was not initialized");

        auto vulkan = Libraries::Vulkan::Get();
        // if (!vulkan->is_initialized()) throw std::runtime_error("Error: Vulkan was not initialized");
        
        auto event_system = Systems::EventSystem::Get();
        // if (!event_system->running) throw std::runtime_error("Error: Event system is not running");

        auto render_system = Systems::RenderSystem::Get();
        // if (!render_system->running) throw std::runtime_error("Error: Event system is not running");
        
        while (!event_system->running) ;

        Initialized = true;

        event_system->use_openvr(useOpenVR);
        render_system->use_openvr(useOpenVR);

        if (useGLFW) event_system->create_window("TEMP", 1, 1, false, false, false, false);
        vulkan->create_instance(validation_layers.size() > 0, validation_layers, instance_extensions, useOpenVR);
        
        auto surface = (useGLFW) ? glfw->create_vulkan_surface(vulkan, "TEMP") : vk::SurfaceKHR();
        vulkan->create_device(device_extensions, device_features, 8, surface, useOpenVR);
        
        /* Initialize Component Factories. Order is important. */
        Transform::Initialize();
        Light::Initialize();
        Camera::Initialize();
        Entity::Initialize();
        Texture::Initialize();
        Mesh::Initialize();
        Material::Initialize();
        Light::CreateShadowCameras();

        auto skybox = Entity::Create("Skybox");
        auto sphere = Mesh::CreateSphere("SkyboxSphere");
        auto transform = Transform::Create("SkyboxTransform");
        auto material = Material::Create("SkyboxMaterial");
        material->show_environment();
        transform->set_scale(100000, 100000, 100000);
        skybox->set_mesh(sphere);
        skybox->set_material(material);
        skybox->set_transform(transform);

        if (useOpenVR) {
            auto ovr = Libraries::OpenVR::Get();
            ovr->create_eye_textures();

            ovr->get_left_eye_hidden_area_entity();
            ovr->get_right_eye_hidden_area_entity();
        }
        if (useGLFW) event_system->destroy_window("TEMP");
    }

    void StopSystems()
    {
        auto glfw = Libraries::GLFW::Get();
        auto vulkan = Libraries::Vulkan::Get();

        auto python_system = Systems::PythonSystem::Get();
        auto event_system = Systems::EventSystem::Get();
        auto render_system = Systems::RenderSystem::Get();
        auto physics_system = Systems::PhysicsSystem::Get();

        render_system->stop();
        python_system->stop();
        physics_system->stop();
        event_system->stop();


        /* All systems must be stopped before we can cleanup (Causes DeviceLost otherwise) */
        Pluto::CleanUp();
        
        std::cout<<"Shutting down Pluto"<<std::endl;
        
        Py_Finalize();
    }

    void CleanUp()
    {
        /* Note: CleanUp is safe to call more than once */
        Initialized = false;

        Mesh::CleanUp();
        Material::CleanUp();
        Transform::CleanUp();
        Light::CleanUp();
        Camera::CleanUp();
        Entity::CleanUp();
    }
}
