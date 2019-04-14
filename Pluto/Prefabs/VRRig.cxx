#include "VRRig.hxx"

#include "Pluto/Entity/Entity.hxx"
#include "Pluto/Camera/Camera.hxx"
#include "Pluto/Entity/Entity.hxx"
#include "Pluto/Transform/Transform.hxx"
#include "Pluto/Texture/Texture.hxx"

#include "Pluto/Systems/EventSystem/EventSystem.hxx"

#include "Pluto/Libraries/GLFW/GLFW.hxx"

#ifdef BUILD_OPENVR
#include "Pluto/Libraries/OpenVR/OpenVR.hxx"
#endif

#include "Pluto/Libraries/GLFW/GLFW.hxx"

VRRig::VRRig(){}

VRRig::VRRig(float resolution_quality, uint32_t msaa_samples, bool show_chaperone_window, bool enable_depth_prepass, bool enable_multiview)
{
    #ifdef BUILD_OPENVR
    auto es = Systems::EventSystem::Get();
    auto ovr = Libraries::OpenVR::Get();

    camera_entity = Entity::Create("vr_rig_entity");
    camera_transform = Transform::Create("vr_rig_camera_transform");

    auto target_size = ovr->get_recommended_render_target_size();
    camera = Camera::Create("vr_rig_camera", 
        (uint32_t)(target_size[0] * resolution_quality), (uint32_t)(target_size[1] * resolution_quality), 
        msaa_samples, 2, enable_depth_prepass, enable_multiview);

    camera_entity->set_camera(camera);
    camera_entity->set_transform(camera_transform);

    es->connect_camera_to_vr(camera);
    
    if (show_chaperone_window) {
        es->create_window("chaperone_window", 512, 512);
        es->connect_camera_to_window("chaperone_window", camera);
    }


    // Create controllers
    right_controller_entity = Entity::Create("right_controller_entity");
    right_controller_mesh = Mesh::Get(ovr->get_right_knuckles_mesh("right_controller_mesh"));
    right_controller_material = Material::Create("right_controller_material");
    right_controller_transform = Transform::Create("vr_rig_left_controller_transform");
    right_controller_base_color_texture = Texture::Get(ovr->get_right_knuckles_basecolor_texture("right_controller_base_color"));
    right_controller_roughness_texture = Texture::Get(ovr->get_right_knuckles_roughness_texture("right_controller_roughness"));
    
    right_controller_entity->set_mesh(right_controller_mesh);
    right_controller_entity->set_material(right_controller_material);
    right_controller_entity->set_transform(right_controller_transform);

    right_controller_material->set_base_color_texture(right_controller_base_color_texture);
    right_controller_material->set_roughness_texture(right_controller_roughness_texture);
    
    left_controller_entity = Entity::Create("left_controller_entity");
    left_controller_mesh = Mesh::Get(ovr->get_left_knuckles_mesh("left_controller_mesh"));
    left_controller_material = Material::Create("left_controller_material");
    left_controller_transform = Transform::Create("vr_rig_right_controller_transform");
    left_controller_base_color_texture = Texture::Get(ovr->get_left_knuckles_basecolor_texture("left_controller_base_color"));
    left_controller_roughness_texture = Texture::Get(ovr->get_left_knuckles_roughness_texture("left_controller_roughness"));

    left_controller_entity->set_mesh(left_controller_mesh);
    left_controller_entity->set_material(left_controller_material);
    left_controller_entity->set_transform(left_controller_transform);

    left_controller_material->set_base_color_texture(left_controller_base_color_texture);
    left_controller_material->set_roughness_texture(left_controller_roughness_texture);

    es->connect_right_hand_transform_to_vr(right_controller_transform);
    es->connect_left_hand_transform_to_vr(left_controller_transform);

    initialized = true;

    #else
    throw std::runtime_error("Error: Pluto was not built with OpenVR enabled");
    #endif
}

void VRRig::update()
{
    update_controllers();
    update_camera();
}

void VRRig::update_controllers()
{

}

void VRRig::update_camera()
{

}