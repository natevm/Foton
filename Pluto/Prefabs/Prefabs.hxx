#pragma once

#include <stdint.h>
#include <glm/glm.hpp>

class Entity;
class Camera;
class Transform;

struct CameraPrefab {
    bool initialized = false;
    Entity* entity = nullptr;
    Camera* camera = nullptr;
    Transform* transform = nullptr;
    float target;
};

class Prefabs {
    public:
        static CameraPrefab CreateArcBallCamera(uint32_t width, uint32_t height, float fov, uint32_t msaa_samples, float target, bool enable_depth_prepass);
        static void Update();
    private:
        static CameraPrefab camera_prefab;
        Prefabs();
        ~Prefabs();
};
