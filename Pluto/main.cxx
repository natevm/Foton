#include <thread>
#include <iostream>

#include "Tools/Options.hxx"

#include "Libraries/GLFW/GLFW.hxx"
#include "Libraries/Vulkan/Vulkan.hxx"

#include "Systems/EventSystem/EventSystem.hxx"
#include "Systems/PythonSystem/PythonSystem.hxx"
#include "Systems/RenderSystem/RenderSystem.hxx"
#include "Systems/PhysicsSystem/PhysicsSystem.hxx"

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
    Options::ProcessArgs(argc, argv);
    Pluto::StartSystems(true);
    Pluto::StopSystems();

    return 0;
}