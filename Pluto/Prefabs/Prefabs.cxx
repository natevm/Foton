#include "./Prefabs.hxx"

#include "Pluto/Entity/Entity.hxx"
#include "Pluto/Camera/Camera.hxx"
#include "Pluto/Entity/Entity.hxx"

#include "Pluto/Systems/EventSystem/EventSystem.hxx"

#include "Pluto/Libraries/GLFW/GLFW.hxx"

#include <stdexcept>

CameraPrefab Prefabs::camera_prefab;
VRRig Prefabs::vr_rig;

Prefabs::Prefabs() { }
Prefabs::~Prefabs() { }

CameraPrefab Prefabs::CreatePrefabCamera(std::string mode, uint32_t width, uint32_t height, float fov, uint32_t msaa_samples, float target, bool enable_depth_prepass) {
    camera_prefab = CameraPrefab(mode, width, height, fov, msaa_samples, target, enable_depth_prepass);
    return camera_prefab;
}

VRRig Prefabs::CreateVRRig(float resolution_quality, uint32_t msaa_samples, bool show_chaperone_window, bool enable_depth_prepass, bool enable_multiview)
{
    vr_rig = VRRig(resolution_quality, msaa_samples, show_chaperone_window, enable_depth_prepass, enable_multiview);
    return vr_rig;
}

void Prefabs::Update()
{
    if (camera_prefab.initialized) camera_prefab.update();
    if (vr_rig.initialized) vr_rig.update();
}

