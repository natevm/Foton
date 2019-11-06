#include <thread>
#include <iostream>

#include "Tools/Options.hxx"

#include "Libraries/GLFW/GLFW.hxx"
#include "Libraries/Vulkan/Vulkan.hxx"

#include "Systems/EventSystem/EventSystem.hxx"
#include "Systems/PythonSystem/PythonSystem.hxx"
#include "Systems/RenderSystem/RenderSystem.hxx"
#include "Systems/PhysicsSystem/PhysicsSystem.hxx"

#include "Foton/Camera/Camera.hxx"
#include "Foton/Texture/Texture.hxx"
#include "Foton/Transform/Transform.hxx"
#include "Foton/Material/Material.hxx"
#include "Foton/Light/Light.hxx"
#include "Foton/Entity/Entity.hxx"
#include "Foton.hxx"

using namespace std;

int main(int argc, char** argv)
{
    Options::ProcessArgs(argc, argv);
    Foton::StartSystems(true);
    Foton::StopSystems();

    return 0;
}