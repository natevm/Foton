#include <thread>
#include <iostream>

#include "Tools/Options.hxx"

#include "Libraries/GLFW/GLFW.hxx"
#include "Libraries/Vulkan/Vulkan.hxx"

#include "Systems/EventSystem/EventSystem.hxx"
#include "Systems/PythonSystem/PythonSystem.hxx"
#include "Systems/RenderSystem/RenderSystem.hxx"

#include "Pluto/Camera/Camera.hxx"
#include "Pluto/Texture/Texture.hxx"
#include "Pluto/Transform/Transform.hxx"
#include "Pluto/Material/Material.hxx"
#include "Pluto/Light/Light.hxx"
#include "Pluto/Entity/Entity.hxx"
#include "Pluto.hxx"

using namespace std;


int main(int argc, char** argv)
{
    std::cout<<"Starting Pluto, thread id: " << std::this_thread::get_id()<<std::endl;

    Options::ProcessArgs(argc, argv);
    
    auto glfw = Libraries::GLFW::Get();
    auto vulkan = Libraries::Vulkan::Get();

    auto python_system = Systems::PythonSystem::Get();
    auto event_system = Systems::EventSystem::Get();
    auto render_system = Systems::RenderSystem::Get();

    python_system->initialize();
    event_system->initialize();
    render_system->initialize();
    
    python_system->start();
    render_system->start();

    //Initialize();

    event_system->start();

    render_system->stop();
    python_system->stop();
    event_system->stop();

    /* event system currently stopped by Python thread */
    CleanUp();
    
    std::cout<<"Shutting down Pluto"<<std::endl;
    return 0;
}