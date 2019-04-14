#pragma once

#include <stdint.h>
#include <string>

class Entity;
class Camera;
class Transform;
class Mesh;
class Material;
class Texture;

class VRRig {
    public:
    VRRig();
    VRRig(float resolution_quality, uint32_t msaa_samples, bool show_chaperone_window = true, bool enable_depth_prepass = true, bool enable_multiview = false);

    bool initialized = false;
    Entity* camera_entity = nullptr;
    Entity* left_controller_entity = nullptr;
    Entity* right_controller_entity = nullptr;

    Transform* camera_transform = nullptr;
    Transform* right_controller_transform = nullptr;
    Transform* left_controller_transform = nullptr;

    Mesh* left_controller_mesh = nullptr;
    Mesh* right_controller_mesh = nullptr;

    Material* left_controller_material = nullptr;
    Material* right_controller_material = nullptr;

    Texture* left_controller_base_color_texture = nullptr;
    Texture* left_controller_roughness_texture = nullptr;

    Texture* right_controller_base_color_texture = nullptr;
    Texture* right_controller_roughness_texture = nullptr;

    Camera* camera = nullptr;
    
    void update();

    private:
    void update_controllers();
    void update_camera();
};