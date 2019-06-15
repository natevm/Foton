#pragma once

/* This is a bit hacked together for now. 

    Should think of a better way to manage C++ side objects.

*/

#include <stdint.h>
#include <glm/glm.hpp>
#include "CameraPrefab.hxx"
#include "VRRig.hxx"

class Prefabs {
    public:
        static CameraPrefab CreatePrefabCamera(std::string mode, uint32_t width, uint32_t height, float fov, uint32_t msaa_samples, float target, bool enable_depth_prepass, std::string window_name);
        static VRRig CreateVRRig(float resolution_quality, uint32_t msaa_samples, bool show_chaperone_window = true, bool enable_depth_prepass = true, bool enable_multiview = false, bool enable_visibility_mask = true);
        static void Update();
    private:
        static CameraPrefab camera_prefab;
        static VRRig vr_rig;
        Prefabs();
        ~Prefabs();
};
