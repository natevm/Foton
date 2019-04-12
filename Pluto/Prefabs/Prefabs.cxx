#include "./Prefabs.hxx"

#include "Pluto/Entity/Entity.hxx"
#include "Pluto/Camera/Camera.hxx"
#include "Pluto/Entity/Entity.hxx"

#include "Pluto/Systems/EventSystem/EventSystem.hxx"

#include "Pluto/Libraries/GLFW/GLFW.hxx"

#include <stdexcept>

CameraPrefab Prefabs::camera_prefab;

Prefabs::Prefabs() { }
Prefabs::~Prefabs() { }

CameraPrefab Prefabs::CreatePrefabCamera(std::string mode, uint32_t width, uint32_t height, float fov, uint32_t msaa_samples, float target, bool enable_depth_prepass) {
    camera_prefab = CameraPrefab(mode, width, height, fov, msaa_samples, target, enable_depth_prepass);
    return camera_prefab;
}



void Prefabs::Update()
{
    if (camera_prefab.initialized) camera_prefab.update();
}

