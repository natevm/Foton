#pragma once

#include <stdint.h>
#include <glm/glm.hpp>
#include "CameraPrefab.hxx"

class Prefabs {
    public:
        static CameraPrefab CreatePrefabCamera(std::string mode, uint32_t width, uint32_t height, float fov, uint32_t msaa_samples, float target, bool enable_depth_prepass);
        static void Update();
    private:
        static CameraPrefab camera_prefab;
        Prefabs();
        ~Prefabs();
};
