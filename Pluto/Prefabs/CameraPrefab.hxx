#pragma once

#include <stdint.h>
#include <string>

class Entity;
class Camera;
class Transform;

class CameraPrefab {
    public:
    CameraPrefab();
    CameraPrefab(std::string mode, uint32_t width, uint32_t height, float fov, uint32_t msaa_samples, float target, bool enable_depth_prepass);

    bool initialized = false;
    Entity* entity = nullptr;
    Camera* camera = nullptr;
    Transform* transform = nullptr;
    float target;
    std::string mode;
    void update();

    private:
    void update_arcball();
    void update_fps();
    void update_spacemouse();
};