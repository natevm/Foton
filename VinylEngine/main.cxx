#include <thread>
#include <iostream>

#include "Libraries/GLFW/GLFW.hxx"
#include "Libraries/Vulkan/Vulkan.hxx"

#include "Systems/EventSystem/EventSystem.hxx"
#include "Systems/PythonSystem/PythonSystem.hxx"
#include "Systems/RenderSystem/RenderSystem.hxx"

using namespace std;

int main(int argc, char** argv)
{
    std::string connectionFile = "";
    if (argc > 2) {
        std::cout<< "This should be -f:" <<std::string(argv[1])<<std::endl;
        std::cout<< "This should be the connection file:" <<std::string(argv[2])<<std::endl;
        connectionFile = std::string(argv[2]);
    }

    auto glfw = Libraries::GLFW::Get();
    auto vulkan = Libraries::Vulkan::Get();

    glfw->create_window("Window");
    // glfw->create_window("Window2");
    vulkan->create_instance(true, {"VK_LAYER_LUNARG_standard_validation"}, {});
    auto surface = glfw->create_vulkan_surface(vulkan, "Window");
    vulkan->create_device({}, {}, surface);
    glfw->create_vulkan_resources(vulkan, "Window");
    glfw->test_clear_images(vulkan, 1.0, 0.0, 0.0);
    
    auto event_system = Systems::EventSystem::Get();
    auto python_system = Systems::PythonSystem::Get();
    auto render_system = Systems::RenderSystem::Get();

    event_system->initialize(glfw);
    python_system->initialize();
    render_system->initialize(vulkan, surface);

    render_system->start();
    python_system->start(argc, argv);
    event_system->start();

    render_system->stop();
    python_system->stop();
    event_system->stop();
    
    vulkan->destroy_device();
    vulkan->destroy_instance();
    
    return 0;
}