// #pragma optimize("", off)

#include "Pluto/Prefabs/Prefabs.hxx"

#define NOMINMAX
//#include <zmq.h>

#include <string>
#include <iostream>
#include <assert.h>

#include "./RenderSystem.hxx"
#include "Pluto/Tools/Colors.hxx"
#include "Pluto/Camera/Camera.hxx"
#include "Pluto/Texture/Texture.hxx"
#include "Pluto/Entity/Entity.hxx"
#include "Pluto/Tools/Options.hxx"
#include "Pluto/Material/Material.hxx"
#include "Pluto/Light/Light.hxx"
#include "Pluto/Mesh/Mesh.hxx"
#include "Pluto/Transform/Transform.hxx"

#include "Pluto/Libraries/OpenVR/OpenVR.hxx"

using namespace Libraries;

namespace Systems
{

RenderSystem::RenderSystem() {}
RenderSystem::~RenderSystem() {}

RenderSystem *RenderSystem::Get()
{
    static RenderSystem instance;
    return &instance;
}

bool RenderSystem::initialize()
{
    using namespace Libraries;
    if (initialized)
        return false;

    #if ZMQ_BUILD_DRAFT_API == 1
    /* Setup Server/Client */
    if (Options::IsServer() || Options::IsClient())
    {
        zmq_context = zmq_ctx_new();

        ip = "udp://";
        ip += Options::GetIP();
        ip += ":5555";
        if (Options::IsServer())
        {
            socket = zmq_socket(zmq_context, ZMQ_RADIO);
            int rc = zmq_connect(socket, ip.c_str());
            assert(rc == 0);
        }

        else if (Options::IsClient())
        {
            std::cout << "Connecting to hello world server..." << std::endl;
            socket = zmq_socket(zmq_context, ZMQ_DISH);
            int rc = zmq_bind(socket, ip.c_str());
            assert(rc == 0);

            /* Join the message group */
            zmq_join(socket, "PLUTO");
        }

        int64_t rate = 100000;
        zmq_setsockopt(socket, ZMQ_RATE, &rate, sizeof(int64_t));
    }
    #endif

    push_constants.top_sky_color = vec4(0.0f);
    push_constants.bottom_sky_color = vec4(0.0f);
    push_constants.sky_transition = 0.0f;
    push_constants.gamma = 2.2f;
    push_constants.exposure = 2.0f;
    push_constants.time = 0.0f;
    push_constants.environment_roughness = 0.0f;
    push_constants.target_id = -1;
    push_constants.camera_id = -1;
    push_constants.brdf_lut_id = -1;
    push_constants.ltc_mat_lut_id = -1;
    push_constants.ltc_amp_lut_id = -1;
    push_constants.environment_id = -1;
    push_constants.diffuse_environment_id = -1;
    push_constants.specular_environment_id = -1;
    for(int i = 0; i < MAX_LIGHTS; i++) push_constants.light_entity_ids[i] = -1;
    push_constants.viewIndex = -1;

    push_constants.flags = 0;
    
    #ifdef DISABLE_MULTIVIEW
    push_constants.flags |= (1 << 0);
    #endif

    #ifdef DISABLE_REVERSE_Z
    push_constants.flags |= ( 1<< 1 );
    #endif

    initialized = true;
    return true;
}

bool RenderSystem::update_push_constants()
{
    /* Find lookup tables */
    Texture* brdf = nullptr;
    Texture* ltc_mat = nullptr;
    Texture* ltc_amp = nullptr;
    try {
        brdf = Texture::Get("BRDF");
        ltc_mat = Texture::Get("LTCMAT");
        ltc_amp = Texture::Get("LTCAMP");
    } catch (...) {}
    if ((!brdf) || (!ltc_mat) || (!ltc_amp)) return false;

    /* Find shadow maps */
    std::vector<Camera*> shadowCams;
    try {
        for (uint32_t i = 0; i < MAX_LIGHTS; ++i) {
            shadowCams.push_back(Camera::Get("ShadowCam_" + std::to_string(i)));
        }
    } catch (...) {}
    if (shadowCams.size() < MAX_LIGHTS) return false;

    /* Update some push constants */
    auto brdf_id = brdf->get_id();
    auto ltc_mat_id = ltc_mat->get_id();
    auto ltc_amp_id = ltc_amp->get_id();
    push_constants.brdf_lut_id = brdf_id;
    push_constants.ltc_mat_lut_id = ltc_mat_id;
    push_constants.ltc_amp_lut_id = ltc_amp_id;
    push_constants.time = (float) glfwGetTime();

    int32_t light_count = 0;
    /* Get light list */
    auto entities = Entity::GetFront();
    std::vector<int32_t> light_entity_ids(MAX_LIGHTS, -1);
    for (uint32_t i = 0; i < Entity::GetCount(); ++i)
    {
        if (entities[i].is_initialized() && (entities[i].get_light() != nullptr))
        {
            entities[i].set_camera(shadowCams[light_count]);
            light_entity_ids[light_count] = i;
            light_count++;
        }
        if (light_count == MAX_LIGHTS)
            break;
    }
    memcpy(push_constants.light_entity_ids, light_entity_ids.data(), sizeof(push_constants.light_entity_ids));
    return true;
}

void RenderSystem::download_visibility_queries()
{
    auto cameras = Camera::GetFront();
    for (uint32_t i = 0; i < Camera::GetCount(); ++i ) {
        if (!cameras[i].is_initialized()) continue;
        cameras[i].download_query_pool_results();
    }
}

uint32_t shadow_idx = 0;

void RenderSystem::record_depth_prepass(Entity &camera_entity, std::vector<std::vector<VisibleEntityInfo>> &visible_entities)
{
    auto camera = camera_entity.get_camera();
    if (!camera) throw std::runtime_error("Error, camera was null during recording of depth prepass");

    vk::CommandBuffer command_buffer = camera->get_command_buffer();

    /* Record Z prepasses / Occlusion query  */
    camera->reset_query_pool(command_buffer);

    if (camera->should_record_depth_prepass()) {
        for(uint32_t rp_idx = 0; rp_idx < camera->get_num_renderpasses(); rp_idx++) {
            Material::ResetBoundMaterial();
            
            /* Get the renderpass for the current camera */
            vk::RenderPass rp = camera->get_depth_prepass(rp_idx);

            /* Bind all descriptor sets to that renderpass.
                Note that we're using a single bind. The same descriptors are shared across pipelines. */
            Material::BindDescriptorSets(command_buffer, rp);

            camera->begin_depth_prepass(command_buffer, rp_idx);

            if (using_openvr && using_vr_hidden_area_masks)
            {
                auto ovr = Libraries::OpenVR::Get();

                /* Render visibility masks */
                if (camera == ovr->get_connected_camera()) {
                    Entity* mask_entity;
                    if (rp_idx == 0) mask_entity = ovr->get_left_eye_hidden_area_entity();
                    else mask_entity = ovr->get_right_eye_hidden_area_entity();
                    // Push constants
                    push_constants.target_id = mask_entity->get_id();
                    push_constants.camera_id = camera_entity.get_id();
                    push_constants.viewIndex = rp_idx;
                    push_constants.flags = (!camera->should_use_multiview()) ? (push_constants.flags | 1 << 0) : (push_constants.flags & ~(1 << 0)); 
                    Material::DrawEntity(command_buffer, rp, *mask_entity, push_constants, RenderMode::RENDER_MODE_VRMASK);
                }
            }

            // if (using_openvr) {
            //     auto ovr = Libraries::OpenVR::Get();
            //     if (camera == ovr->get_connected_camera()) {
            //         if ( ovr->get_left_eye_hidden_area_entity()->get_id() == target_id) continue;
            //         if ( ovr->get_right_eye_hidden_area_entity()->get_id() == target_id) continue;
            //     }
            // }

            for (uint32_t i = 0; i < visible_entities[rp_idx].size(); ++i) {
                auto target_id = visible_entities[rp_idx][i].entity->get_id();
                if (visible_entities[rp_idx][i].visible && visible_entities[rp_idx][i].distance < camera->get_max_visible_distance()) {
                    camera->begin_visibility_query(command_buffer, rp_idx, visible_entities[rp_idx][i].entity_index);
                    // Push constants
                    push_constants.target_id = target_id;
                    push_constants.camera_id = camera_entity.get_id();
                    push_constants.viewIndex = rp_idx;
                    push_constants.flags = (!camera->should_use_multiview()) ? (push_constants.flags | 1 << 0) : (push_constants.flags & ~(1 << 0)); 
                    
                    bool was_visible_last_frame = camera->is_entity_visible(rp_idx, target_id);
                    bool contains_transparency = visible_entities[rp_idx][i].entity->material()->contains_transparency();
                    bool disable_depth_write = (was_visible_last_frame) ? contains_transparency : true;
                    
                    bool show_bounding_box = !was_visible_last_frame;

                    PipelineType pipeline_type = PipelineType::PIPELINE_TYPE_NORMAL;
                    if (show_bounding_box || contains_transparency)
                        pipeline_type = PipelineType::PIPELINE_TYPE_DEPTH_WRITE_DISABLED;

                    Material::DrawEntity(command_buffer, rp, *visible_entities[rp_idx][i].entity, push_constants, 
                        RenderMode::RENDER_MODE_FRAGMENTDEPTH, pipeline_type, show_bounding_box);
                    camera->end_visibility_query(command_buffer, rp_idx, visible_entities[rp_idx][i].entity_index);
                }


                // /* An object needs a transform, a mesh, a material, and cannot be transparent 
                // in order to be drawn in the depth prepass */
                // if (visible_entities[rp_idx][i].entity->transform() &&
                //     visible_entities[rp_idx][i].entity->mesh() &&
                //     visible_entities[rp_idx][i].entity->material())
                // {


                //     // if (camera->is_entity_visible(target_id) {
                //     /* This is a problem, since a single entity may be drawn multiple times on devices
                //     not supporting multiview, (mac). Might need to increase query pool size to MAX_ENTITIES * renderpass count */
                    

                // }
            }
            camera->end_depth_prepass(command_buffer, rp_idx);
        }
    }
}

void RenderSystem::record_raster_renderpass(Entity &camera_entity, std::vector<std::vector<VisibleEntityInfo>> &visible_entities)
{
    auto camera = camera_entity.get_camera();
    if (!camera) throw std::runtime_error("Error, camera was null during recording of final renderpass");

    vk::CommandBuffer command_buffer = camera->get_command_buffer();

    for(uint32_t rp_idx = 0; rp_idx < camera->get_num_renderpasses(); rp_idx++) {
        Material::ResetBoundMaterial();

        /* Get the renderpass for the current camera */
        vk::RenderPass rp = camera->get_renderpass(rp_idx);

        /* Bind all descriptor sets to that renderpass.
            Note that we're using a single bind. The same descriptors are shared across pipelines. */
        Material::BindDescriptorSets(command_buffer, rp);
        
        camera->begin_renderpass(command_buffer, rp_idx);

        /* Render visibility masks */
        if (using_openvr && using_vr_hidden_area_masks && !(camera->should_record_depth_prepass()))
        {
            auto ovr = Libraries::OpenVR::Get();

            if (camera == ovr->get_connected_camera()) {
                Entity* mask_entity;
                if (rp_idx == 0) mask_entity = ovr->get_left_eye_hidden_area_entity();
                else mask_entity = ovr->get_right_eye_hidden_area_entity();
                // Push constants
                push_constants.target_id = mask_entity->get_id();
                push_constants.camera_id = camera_entity.get_id();
                push_constants.viewIndex = rp_idx;
                push_constants.flags = (!camera->should_use_multiview()) ? (push_constants.flags | 1 << 0) : (push_constants.flags & ~(1 << 0)); 
                Material::DrawEntity(command_buffer, rp, *mask_entity, push_constants, RenderMode::RENDER_MODE_VRMASK);
            }
        }

        /* Render all opaque objects */
        for (uint32_t i = 0; i < visible_entities[rp_idx].size(); ++i) {
            /* An object must be opaque, have a transform, a mesh, and a material to be rendered. */
            if (!visible_entities[rp_idx][i].visible || visible_entities[rp_idx][i].distance > camera->get_max_visible_distance()) continue;
            if (visible_entities[rp_idx][i].entity->material()->contains_transparency()) continue;

            auto target_id = visible_entities[rp_idx][i].entity_index;
            if (camera->is_entity_visible(rp_idx, target_id) || (!camera->should_record_depth_prepass())) {
                push_constants.target_id = target_id;
                push_constants.camera_id = camera_entity.get_id();
                push_constants.viewIndex = rp_idx;
                push_constants.flags = (!camera->should_use_multiview()) ? (push_constants.flags | 1 << 0) : (push_constants.flags & ~(1 << 0)); 
                Material::DrawEntity(command_buffer, rp, *visible_entities[rp_idx][i].entity, push_constants, camera->get_rendermode_override());
            }
        }
        
        /* Draw transparent objects last */
        for (int32_t i = (int32_t)visible_entities[rp_idx].size() - 1; i >= 0; --i)
        {
            if (!visible_entities[rp_idx][i].visible || visible_entities[rp_idx][i].distance > camera->get_max_visible_distance()) continue;
            if (!visible_entities[rp_idx][i].entity->material()->contains_transparency()) continue;

            auto target_id = visible_entities[rp_idx][i].entity_index;
            if (camera->is_entity_visible(rp_idx, target_id) || (!camera->should_record_depth_prepass())) {
                push_constants.target_id = target_id;
                push_constants.camera_id = camera_entity.get_id();
                push_constants.viewIndex = rp_idx;
                push_constants.flags = (!camera->should_use_multiview()) ? (push_constants.flags | 1 << 0) : (push_constants.flags & ~(1 << 0)); 
                Material::DrawEntity(command_buffer, rp, *visible_entities[rp_idx][i].entity, push_constants, 
                    camera->get_rendermode_override(), PipelineType::PIPELINE_TYPE_DEPTH_TEST_GREATER);
            }
        }
        camera->end_renderpass(command_buffer, rp_idx);
    }
}

void RenderSystem::record_ray_trace_pass(Entity &camera_entity, std::vector<std::vector<VisibleEntityInfo>> &visible_entities)
{
    auto vulkan = Vulkan::Get();
    auto device = vulkan->get_device();
    auto dldi = vulkan->get_dldi();

    if (!vulkan->is_ray_tracing_enabled()) return;

    build_top_level_bvh(true);
        
    auto camera = camera_entity.get_camera();
    if (!camera) throw std::runtime_error("Error, camera was null in recording ray trace");
    vk::CommandBuffer command_buffer = camera->get_command_buffer();
    Texture * texture = camera->get_texture();

    /* Camera needs a texture */
    if (!texture) return;

    texture->make_general(command_buffer);

    for(uint32_t rp_idx = 0; rp_idx < camera->get_num_renderpasses(); rp_idx++) {
        Material::ResetBoundMaterial();

        /* Get the renderpass for the current camera */
        vk::RenderPass rp = camera->get_renderpass(rp_idx);
        
        Material::UpdateRaytracingDescriptorSets(topAS, camera_entity);
        Material::BindRayTracingDescriptorSets(command_buffer, rp);

        push_constants.target_id = -1;
        push_constants.camera_id = camera_entity.get_id();
        push_constants.viewIndex = rp_idx;
        push_constants.flags = (!camera->should_use_multiview()) ? (push_constants.flags | 1 << 0) : (push_constants.flags & ~(1 << 0)); 
        
        Material::TraceRays(command_buffer, rp, push_constants, *texture);
    }
}

void RenderSystem::record_blit_camera(Entity &camera_entity, std::map<std::string, std::pair<Camera *, uint32_t>> &window_to_cam)
{
    auto glfw = GLFW::Get();
    auto camera = camera_entity.get_camera();

    if (!camera) throw std::runtime_error("Error, camera was null in recording blits");

    vk::CommandBuffer command_buffer = camera->get_command_buffer();
    Texture * texture = camera->get_texture();

    /* There might be a better way to handle this... */
    if (!texture) throw std::runtime_error("Error, camera texture was null in recording blits");

    /* Blit to GLFW windows */
    for (auto w2c : window_to_cam) {
        if ((w2c.second.first->get_id() == camera_entity.get_camera_id())) {
            /* Window needs a swapchain */
            auto swapchain = glfw->get_swapchain(w2c.first);
            if (!swapchain) {
                continue;
            }

            /* Need to be able to get swapchain/texture by key...*/
            auto swapchain_texture = glfw->get_texture(w2c.first);

            /* Record blit to swapchain */
            if (swapchain_texture && swapchain_texture->is_initialized()) {
                texture->record_blit_to(command_buffer, swapchain_texture, w2c.second.second);
            }
        }
    }

    /* Blit to OpenVR eyes. */
    if (using_openvr) {
        auto ovr = OpenVR::Get();
        if (camera == ovr->get_connected_camera()) {
            auto left_eye_texture = ovr->get_left_eye_texture();
            auto right_eye_texture = ovr->get_right_eye_texture();
            if (left_eye_texture)  texture->record_blit_to(command_buffer, left_eye_texture, 0);
            if (right_eye_texture) texture->record_blit_to(command_buffer, right_eye_texture, 1);
        }
    }
}

void RenderSystem::record_cameras()
{
    auto vulkan = Vulkan::Get();
    auto device = vulkan->get_device();

    /* Get window to camera mapping */
    auto glfw = GLFW::Get();
    auto window_to_cam = glfw->get_window_to_camera_map();
    auto entities = Entity::GetFront();

    /* Render all cameras */
    auto cameras = Camera::GetFront();
    for (uint32_t entity_id = 0; entity_id < Entity::GetCount(); ++entity_id) {
        /* Entity must be initialized */
        if (!entities[entity_id].is_initialized()) continue;

        /* Entity needs a camera */
        auto camera = entities[entity_id].get_camera();
        if (!camera) continue;

        /* Camera needs a texture */
        Texture * texture = camera->get_texture();
        if (!texture) continue;

        /* If entity is a shadow camera (TODO: REFACTOR THIS...) */
        if (entities[entity_id].get_light() != nullptr) {
            /* Skip shadowmaps which shouldn't cast shadows */
            auto light = entities[entity_id].get_light();
            if (!light->should_cast_shadows()) continue;
            if (!light->should_cast_dynamic_shadows()) continue;
        }

        /* Begin recording */
        vk::CommandBufferBeginInfo beginInfo;
        if (camera->needs_command_buffers()) camera->create_command_buffers();
        vk::CommandBuffer command_buffer = camera->get_command_buffer();
        
        beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
        command_buffer.begin(beginInfo);

        auto visible_entities = camera->get_visible_entities(entity_id);

        record_depth_prepass(entities[entity_id], visible_entities);

        record_raster_renderpass(entities[entity_id], visible_entities);

        //record_ray_trace_pass(entities[entity_id], visible_entities);

        record_blit_camera(entities[entity_id], window_to_cam);

        /* End this recording. */
        command_buffer.end();
    }
}

void RenderSystem::record_blit_textures()
{
    /* Blit any textures to windows which request them. */
    auto glfw = GLFW::Get();
    auto window_to_tex = glfw->get_window_to_texture_map();
    vk::CommandBufferBeginInfo beginInfo;
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    main_command_buffer.begin(beginInfo);
    main_command_buffer_recorded = true;
    main_command_buffer_presenting = false;
    for (auto &w2t : window_to_tex) {
        /* Window needs a swapchain */
        auto swapchain = glfw->get_swapchain(w2t.first);
        if (!swapchain) {
            continue;
        }

        /* Need to be able to get swapchain/texture by key...*/
        auto swapchain_texture = glfw->get_texture(w2t.first);

        /* Record blit to swapchain */
        if (swapchain_texture && swapchain_texture->is_initialized()) {
            w2t.second.first->record_blit_to(main_command_buffer, swapchain_texture, w2t.second.second);
            main_command_buffer_presenting = true;
        }
    }

    
    main_command_buffer.end();
}

void RenderSystem::record_render_commands()
{
    auto vulkan = Vulkan::Get();
    auto device = vulkan->get_device();

    update_openvr_transforms();
    
    auto upload_command = vulkan->begin_one_time_graphics_command();

    /* Upload SSBO data */
    Material::UploadSSBO(upload_command);
    Transform::UploadSSBO(upload_command);
    Light::UploadSSBO(upload_command);
    Camera::UploadSSBO(upload_command);
    Entity::UploadSSBO(upload_command);
    Texture::UploadSSBO(upload_command);
    Mesh::UploadSSBO(upload_command);
    
	vulkan->end_one_time_graphics_command_immediately(upload_command, "Upload SSBO Data", true);

    Material::UpdateRasterDescriptorSets();
    
    if (update_push_constants() == true) {
        record_cameras();
    }
    
    record_blit_textures();
}

void RenderSystem::update_openvr_transforms()
{
    /* Set OpenVR transform data right before rendering */
    if (using_openvr) {
        auto ovr = OpenVR::Get();
        ovr->wait_get_poses();

        /* Set camera information */
        auto camera = ovr->get_connected_camera();
        
        if (camera) {
            /* Move the camera to where the headset is. */
            camera->set_view(ovr->get_left_view_matrix(), 0);
            camera->set_custom_projection(ovr->get_left_projection_matrix(.1f), .1f, 0);
            camera->set_view(ovr->get_right_view_matrix(), 1);
            camera->set_custom_projection(ovr->get_right_projection_matrix(.1f), .1f, 1);
        }
        

        /* Set transform information */
        auto left_hand_transform = ovr->get_connected_left_hand_transform();
        auto right_hand_transform = ovr->get_connected_right_hand_transform();
        auto camera_transform = ovr->get_connected_camera_transform();

        if (left_hand_transform)
        {
            left_hand_transform->set_transform(ovr->get_left_controller_transform());
        }

        if (right_hand_transform)
        {
            right_hand_transform->set_transform(ovr->get_right_controller_transform());
        }

        /* TODO... camera transform currently set via view... */
        if (camera_transform)
        {
            
        }
    }
}

void RenderSystem::present_openvr_frames()
{
    /* Ignore if openvr isn't in use. */
    if (!using_openvr) return;

    auto ovr = OpenVR::Get();
    auto camera = ovr->get_connected_camera();

    /* Dont submit anything if no camera is connected to VR. */
    if (!camera) return;

    /* Don't render anything if the camera component doesn't have a color texture. */
    Texture * texture = camera->get_texture();
    if (texture == nullptr) return;
    
    /* Submit the left and right eye textures to OpenVR */
    ovr->submit_textures();
}

void RenderSystem::stream_frames()
{
    // /* Only stream frames if we're in server/client mode. */
    // if (! (Options::IsServer() || Options::IsClient())) return;

    // /* For now, only stream the camera associated with the window named "Window" */
    // auto entity_id = Entity::GetEntityFromWindow("Window");
    
    // /* Dont stream anything if no entity is connected to the window. */
    // if (entity_id == -1) return;

    // auto entity = Entity::Get(entity_id);
    // auto cam_id = entity->get_camera();

    // /* Dont stream anything if the connected entity does not have a camera component attached. */
    // if (cam_id == -1) return;
    // Camera* current_camera = Camera::Get(cam_id);

    // /* Don't stream anything if the camera component doesn't have a color texture. */
    // Texture * texture = current_camera->get_texture();
    // if (texture == nullptr) return;
    
    // Bucket bucket = {};

    // #if ZMQ_BUILD_DRAFT_API == 1
    // /* If we're the server, send the frame over UDP */
    // if (Options::IsServer())
    // {
    //     std::vector<float> color_data = texture->download_color_data(16, 16, 1, true);

    //     bucket.x = 0;
    //     bucket.y = 0;
    //     bucket.width = 16;
    //     bucket.height = 16;
    //     memcpy(bucket.data, color_data.data(), 16 * 16 * 4 * sizeof(float));

    //     /* Set message group */
    //     zmq_msg_t msg;
    //     const char *text = "Hello";
    //     int rc = zmq_msg_init_size(&msg, sizeof(Bucket));
    //     assert(rc == 0);
    //     memcpy(zmq_msg_data(&msg), &bucket, sizeof(Bucket));
    //     zmq_msg_set_group(&msg, "PLUTO");
    //     zmq_msg_send(&msg, RenderSystem::Get()->socket, ZMQ_DONTWAIT);
    // }

    // /* Else we're the client, so upload the frame. */
    // /* TODO: make this work again. */
    // else {
    //     //     for (int i = 0; i < 100; ++i)
    //     //     {
    //     //         int rc = zmq_recv(RenderSystem::Get()->socket, &bucket, sizeof(Bucket), ZMQ_NOBLOCK);
    //     //     }
    //     //     std::vector<float> color_data(16 * 16 * 4);
    //     //     memcpy(color_data.data(), bucket.data, 16 * 16 * 4 * sizeof(float));
    //     //     current_camera->get_texture()->upload_color_data(16, 16, 1, color_data, 0);
    // }
    // #endif
}

void RenderSystem::enqueue_render_commands() {
    auto vulkan = Vulkan::Get(); auto device = vulkan->get_device();
    auto glfw = GLFW::Get();

    auto window_to_cam = glfw->get_window_to_camera_map();
    auto window_to_tex = glfw->get_window_to_texture_map();

    auto entities = Entity::GetFront();
    auto cameras = Camera::GetFront();
    
    std::vector<Vulkan::CommandQueueItem> command_queue;

    compute_graph.clear();
    final_fences.clear();
    final_renderpass_semaphores.clear();

    /* Aggregate command sets */
    std::vector<std::shared_ptr<ComputeNode>> last_level;
    std::vector<std::shared_ptr<ComputeNode>> current_level;

    uint32_t level_idx = 0;
    for (int32_t rp_idx = Camera::GetMinRenderOrder(); rp_idx <= Camera::GetMaxRenderOrder(); ++rp_idx)
    {
        uint32_t queue_idx = 0;
        bool command_found = false;
        for (uint32_t e_id = 0; e_id < Entity::GetCount(); ++e_id) {
            if ((!entities[e_id].is_initialized()) ||
                (!entities[e_id].camera()) ||
                (!entities[e_id].camera()->get_texture()) ||
                (entities[e_id].camera()->get_render_order() != rp_idx)
            ) continue;

            /* If the entity is a shadow camera and shouldn't cast shadows, skip it. (TODO: REFACTOR THIS...) */
            if (entities[e_id].light() && 
                ((!entities[e_id].light()->should_cast_shadows())
                    || (!entities[e_id].light()->should_cast_dynamic_shadows()))) continue; 

            /* Make a compute node for this command buffer */
            auto node = std::make_shared<ComputeNode>();
            node->level = level_idx;
            node->queue_idx = queue_idx;
            node->command_buffer = entities[e_id].camera()->get_command_buffer();

            /* Add any connected windows to this node */
            for (auto w2c : window_to_cam) {
                if ((w2c.second.first->get_id() == entities[e_id].camera()->get_id())) {
                    if (!glfw->get_swapchain(w2c.first)) continue;
                    auto swapchain_texture = glfw->get_texture(w2c.first);
                    if ( (!swapchain_texture) || (!swapchain_texture->is_initialized())) continue;
                    node->connected_windows.push_back(w2c.first);
                }
            }

            /* Mark the previous level as a set of dependencies */
            if (level_idx > 0) node->dependencies = last_level;
            current_level.push_back(node);
            command_found = true;
            queue_idx++;
        }

        if (command_found) {
            /* Keep a reference to the top of the graph. */
            compute_graph.push_back(current_level);

            /* Update children references */
            for (auto &node : last_level) {
                node->children = current_level;
            }

            /* Increase the graph level, and replace the last level with the current one */
            level_idx++;
            last_level = current_level;
            current_level = std::vector<std::shared_ptr<ComputeNode>>();
        }
    }

    /* For other misc commands not specific to any camera */
    if (main_command_buffer_recorded) {
        auto node = std::make_shared<ComputeNode>();
        node->level = level_idx;
        node->queue_idx = 0;
        node->command_buffer = main_command_buffer;
        if (main_command_buffer_presenting) {
            for (auto &w2t : window_to_tex) {
                node->connected_windows.push_back(w2t.first);
            }
        }
        main_command_buffer_recorded = false;
    }

    /* Create semaphores and fences for nodes in the graph */
    for (auto &level : compute_graph) {
        for (auto &node : level) {
            /* All internal nodes need to signal each child */
            for (uint32_t i = 0; i < node->children.size(); ++i) {
                node->signal_semaphores.push_back(get_semaphore());
            }

            /* If any windows are dependent on this node, signal those too. */
            for (auto &window_key : node->connected_windows) {
                auto image_available = glfw->get_image_available_semaphore(window_key, currentFrame);
                if (image_available != vk::Semaphore()) {
                    if (!glfw->get_swapchain(window_key)) continue;
                    if ((!glfw->get_texture(window_key)) || (!glfw->get_texture(window_key)->is_initialized())) continue;
                    
                    auto render_complete = get_semaphore();
                    node->window_signal_semaphores.push_back(render_complete);
                    final_renderpass_semaphores.push_back(render_complete);
                }
            }

            /* If the node has no children, add a fence */
            if (node->children.size() == 0) {
                node->fence = get_fence();
                final_fences.push_back(node->fence);
            }
        }
    }

    level_idx = 0;
    for (auto &level : compute_graph) {
        vk::SemaphoreCreateInfo semaphoreInfo;
        vk::FenceCreateInfo fenceInfo;

        std::set<vk::Semaphore> wait_semaphores_set;
        std::set<vk::Semaphore> signal_semaphores_set;
        std::vector<vk::CommandBuffer> command_buffers;

        Vulkan::CommandQueueItem item;
        
        for (auto &node : level) {
            /* Connect signal semaphores to wait semaphore slots in the graph */
            if (node->dependencies.size() > 0) {
                for (auto &dependency : node->dependencies) {
                    for (uint32_t i = 0; i < dependency->children.size(); ++i) {
                        if (dependency->children[i] == node) {
                            wait_semaphores_set.insert(dependency->signal_semaphores[i]);
                            // Todo: optimize this 
                            // wait_stage_masks.push_back(vk::PipelineStageFlagBits::eColorAttachmentOutput);
                            break;
                        }
                    }
                }
            }

            /* If the node is connected to a window, wait for that window's image to be available */
            for (auto &window_key : node->connected_windows) {
                auto image_available = glfw->get_image_available_semaphore(window_key, currentFrame);
                if (image_available != vk::Semaphore()) {
                    wait_semaphores_set.insert(image_available);
                    // wait_stage_masks.push_back(vk::PipelineStageFlagBits::eColorAttachmentOutput);
                }
            }

            for (auto &signal_semaphore : node->signal_semaphores) 
                signal_semaphores_set.insert(signal_semaphore);

            for (auto &signal_semaphore : node->window_signal_semaphores)
                signal_semaphores_set.insert(signal_semaphore);

            if (node->fence != vk::Fence()) {
                if (item.fence != vk::Fence()) throw std::runtime_error("Error, not expecting more than one fence per level!");
                item.fence = node->fence;
            }

            item.commandBuffers.push_back(node->command_buffer);            
            // For some reason, queues arent actually ran in parallel? Also calling submit many times turns out to be expensive...
            // vulkan->enqueue_graphics_commands(node->command_buffers, wait_semaphores, wait_stage_masks, signal_semaphores, 
            //     node->fence, "draw call lvl " + std::to_string(node->level) + " queue " + std::to_string(node->queue_idx), node->queue_idx);
        }

        /* Making hasty assumptions here about wait stage masks. */
        for (auto &semaphore : wait_semaphores_set) {
            item.waitSemaphores.push_back(semaphore);
            item.waitDstStageMask.push_back(vk::PipelineStageFlagBits::eColorAttachmentOutput);
        }
        
        for (auto &semaphore : signal_semaphores_set) 
            item.signalSemaphores.push_back(semaphore);
        
        item.hint = "draw call lvl " + std::to_string(level_idx) + " queue " + std::to_string(0);
        item.queue_idx = 0;
        command_queue.push_back(item);
        level_idx++;
    }
    
    /* Enqueue the commands */
    for (auto &item : command_queue) vulkan->enqueue_graphics_commands(item);
    semaphoresSubmitted();
    fencesSubmitted();
}

void RenderSystem::mark_cameras_as_render_complete()
{
    /* Render all cameras */
    auto entities = Entity::GetFront();
    auto cameras = Camera::GetFront();
    for (uint32_t entity_id = 0; entity_id < Entity::GetCount(); ++entity_id) {
        /* Entity must be initialized */
        if (!entities[entity_id].is_initialized()) continue;

        /* Entity needs a camera */
        auto camera = entities[entity_id].get_camera();
        if (!camera) continue;

        /* Camera needs a texture */
        Texture * texture = camera->get_texture();
        if (!texture) continue;

        /* If entity is a shadow camera (TODO: REFACTOR THIS...) */
        if (entities[entity_id].get_light() != nullptr) {
            /* Skip shadowmaps which shouldn't cast shadows */
            auto light = entities[entity_id].get_light();
            if (!light->should_cast_shadows()) continue;
            if (!light->should_cast_dynamic_shadows()) continue;
        }

        camera->mark_render_as_complete();
    }

}

void RenderSystem::release_vulkan_resources() 
{
    auto vulkan = Vulkan::Get();

    /* If Vulkan was never initialized, we never allocated anything. */
    if (!vulkan->is_initialized()) return;

    /* Likewise if a device was never created, we never allocated anything. */
    auto device = vulkan->get_device();
    if (device == vk::Device()) return;

    /* Don't free vulkan resources more than once. */
    if (!vulkan_resources_created) return;

    /* Release vulkan resources */
    device.freeCommandBuffers(vulkan->get_command_pool(), {main_command_buffer});
    main_command_buffer = vk::CommandBuffer();
    // device.destroyFence(main_fence);
    // for (auto &semaphore : main_command_buffer_semaphores)
    //     device.destroySemaphore(semaphore);
    
    vulkan_resources_created = false;
}

void RenderSystem::allocate_vulkan_resources()
{
    /* Don't create vulkan resources more than once. */
    if (vulkan_resources_created) return;

    auto vulkan = Vulkan::Get();
    auto device = vulkan->get_device();
    auto dldi = vulkan->get_dldi();
    
    /* Create a main command buffer if one does not already exist */
    vk::CommandBufferAllocateInfo mainCmdAllocInfo;
    mainCmdAllocInfo.level = vk::CommandBufferLevel::ePrimary;
    mainCmdAllocInfo.commandPool = vulkan->get_command_pool();
    mainCmdAllocInfo.commandBufferCount = max_frames_in_flight;

    vk::SemaphoreCreateInfo semaphoreInfo;
    vk::FenceCreateInfo fenceInfo;
    
    main_command_buffer = device.allocateCommandBuffers(mainCmdAllocInfo)[0];

    // for (uint32_t i = 0; i < max_frames_in_flight; ++i) {
    //     main_command_buffer_semaphores.push_back(device.createSemaphore(semaphoreInfo));
    // }
    // main_fence = device.createFence(fenceInfo);


    // Create top level acceleration structure resources
    if (vulkan->is_ray_tracing_enabled()) {
        auto CreateAccelerationStructure = [&](vk::AccelerationStructureTypeNV type, uint32_t geometryCount,
            vk::GeometryNV* geometries, uint32_t instanceCount, vk::AccelerationStructureNV& AS, vk::DeviceMemory& memory)
        {
            vk::AccelerationStructureCreateInfoNV accelerationStructureInfo;
            accelerationStructureInfo.compactedSize = 0;
            accelerationStructureInfo.info.type = type;
            accelerationStructureInfo.info.instanceCount = instanceCount;
            accelerationStructureInfo.info.geometryCount = geometryCount;
            accelerationStructureInfo.info.pGeometries = geometries;

            AS = device.createAccelerationStructureNV(accelerationStructureInfo, nullptr, dldi);

            vk::AccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo;
            memoryRequirementsInfo.accelerationStructure = AS;
            memoryRequirementsInfo.type = vk::AccelerationStructureMemoryRequirementsTypeNV::eObject;

            vk::MemoryRequirements2 memoryRequirements;
            memoryRequirements = device.getAccelerationStructureMemoryRequirementsNV(memoryRequirementsInfo, dldi);

            vk::MemoryAllocateInfo memoryAllocateInfo;
            memoryAllocateInfo.allocationSize = memoryRequirements.memoryRequirements.size;
            memoryAllocateInfo.memoryTypeIndex = vulkan->find_memory_type(memoryRequirements.memoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);

            memory = device.allocateMemory(memoryAllocateInfo);
            
            vk::BindAccelerationStructureMemoryInfoNV bindInfo;
            bindInfo.accelerationStructure = AS;
            bindInfo.memory = memory;
            bindInfo.memoryOffset = 0;
            bindInfo.deviceIndexCount = 0;
            bindInfo.pDeviceIndices = nullptr;

            device.bindAccelerationStructureMemoryNV({bindInfo}, dldi);
        };

        /* Create top level acceleration structure */
        CreateAccelerationStructure(vk::AccelerationStructureTypeNV::eTopLevel,
            0, nullptr, MAX_ENTITIES, topAS, topASMemory);

        /* ----- Create Instance Buffer ----- */
		uint32_t instanceBufferSize = (uint32_t) (sizeof(VkGeometryInstance) * MAX_ENTITIES);

		vk::BufferCreateInfo instanceBufferInfo;
		instanceBufferInfo.size = instanceBufferSize;
		instanceBufferInfo.usage = vk::BufferUsageFlagBits::eRayTracingNV;
		instanceBufferInfo.sharingMode = vk::SharingMode::eExclusive;

		instanceBuffer = device.createBuffer(instanceBufferInfo);

		vk::MemoryRequirements instanceBufferRequirements;
		instanceBufferRequirements = device.getBufferMemoryRequirements(instanceBuffer);

		vk::MemoryAllocateInfo instanceMemoryAllocateInfo;
		instanceMemoryAllocateInfo.allocationSize = instanceBufferRequirements.size;
		instanceMemoryAllocateInfo.memoryTypeIndex = vulkan->find_memory_type(instanceBufferRequirements.memoryTypeBits, 
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent );

		instanceBufferMemory = device.allocateMemory(instanceMemoryAllocateInfo);
		
		device.bindBufferMemory(instanceBuffer, instanceBufferMemory, 0);

        /* Allocate scratch buffer for building */
        auto GetScratchBufferSize = [&](vk::AccelerationStructureNV handle)
        {
            vk::AccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo;
            memoryRequirementsInfo.accelerationStructure = handle;
            memoryRequirementsInfo.type = vk::AccelerationStructureMemoryRequirementsTypeNV::eBuildScratch;

            vk::MemoryRequirements2 memoryRequirements;
            memoryRequirements = device.getAccelerationStructureMemoryRequirementsNV( memoryRequirementsInfo, dldi);

            vk::DeviceSize result = memoryRequirements.memoryRequirements.size;
            return result;
        };

        vk::DeviceSize scratchBufferSize = GetScratchBufferSize(topAS);

		vk::BufferCreateInfo bufferInfo;
		bufferInfo.size = scratchBufferSize;
		bufferInfo.usage = vk::BufferUsageFlagBits::eRayTracingNV;
		bufferInfo.sharingMode = vk::SharingMode::eExclusive;
		accelerationStructureScratchBuffer = device.createBuffer(bufferInfo);
		
		vk::MemoryRequirements scratchBufferRequirements;
		scratchBufferRequirements = device.getBufferMemoryRequirements(accelerationStructureScratchBuffer);
		
		vk::MemoryAllocateInfo scratchMemoryAllocateInfo;
		scratchMemoryAllocateInfo.allocationSize = scratchBufferRequirements.size;
		scratchMemoryAllocateInfo.memoryTypeIndex = vulkan->find_memory_type(scratchBufferRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);

		accelerationStructureScratchMemory = device.allocateMemory(scratchMemoryAllocateInfo);
		device.bindBufferMemory(accelerationStructureScratchBuffer, accelerationStructureScratchMemory, 0);
    }

    vulkan_resources_created = true;
}

void RenderSystem::build_top_level_bvh(bool submit_immediately)
{
	auto vulkan = Libraries::Vulkan::Get();
	if (!vulkan->is_initialized()) throw std::runtime_error("Error: vulkan is not initialized");

	if (!vulkan->is_ray_tracing_enabled()) 
		throw std::runtime_error("Error: Vulkan device extension VK_NVX_raytracing is not currently enabled.");
	
	auto dldi = vulkan->get_dldi();
	auto device = vulkan->get_device();
	if (!device) 
		throw std::runtime_error("Error: vulkan device not initialized");

    auto entities = Entity::GetFront();

	/* Gather Instances */
	std::vector<VkGeometryInstance> instances(MAX_ENTITIES);
	for (uint32_t i = 0; i < MAX_ENTITIES; ++i)
	{
        VkGeometryInstance instance;

        if ((!entities[i].is_initialized())
            || (!entities[i].mesh())
            || (!entities[i].transform())
            || (!entities[i].material())
            || (!entities[i].mesh()->get_low_level_bvh()))
        {

            instance.instanceId = i;
            instance.mask = 0; // means this instance can't be hit.
            instance.instanceOffset = 0;
            instance.accelerationStructureHandle = 0; // not sure if this is allowed...
        }
        else {
            auto llas = entities[i].mesh()->get_low_level_bvh();
            if (!llas) continue;

            /* Create Instance */
            uint64_t accelerationStructureHandle;
            device.getAccelerationStructureHandleNV(llas, sizeof(uint64_t), &accelerationStructureHandle, dldi);
            auto local_to_world = entities[i].transform()->get_local_to_world_matrix();
            float transform[12] = {
                local_to_world[0][0], local_to_world[1][0], local_to_world[2][0], local_to_world[3][0],
                local_to_world[0][1], local_to_world[1][1], local_to_world[2][1], local_to_world[3][1],
                local_to_world[0][2], local_to_world[1][2], local_to_world[2][2], local_to_world[3][2],
            };

            memcpy(instance.transform, transform, sizeof(instance.transform));
            instance.instanceId = i;
            instance.mask = 0xff;
            instance.instanceOffset = 0;
            instance.flags = (uint32_t) vk::GeometryInstanceFlagBitsNV::eTriangleCullDisable;
            instance.accelerationStructureHandle = accelerationStructureHandle;
        }
	
		instances[i] = instance;
	}

	/* ----- Upload Instances ----- */
	{
		uint32_t instanceBufferSize = (uint32_t)(sizeof(VkGeometryInstance) * MAX_ENTITIES);
        void* ptr = device.mapMemory(instanceBufferMemory, 0, instanceBufferSize);
		memcpy(ptr, instances.data(), instanceBufferSize);
		device.unmapMemory(instanceBufferMemory);
	}

	/* Build top level BVH */
	// auto GetScratchBufferSize = [&](vk::AccelerationStructureNV handle)
	// {
	// 	vk::AccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo;
	// 	memoryRequirementsInfo.accelerationStructure = handle;
	// 	memoryRequirementsInfo.type = vk::AccelerationStructureMemoryRequirementsTypeNV::eBuildScratch;

	// 	vk::MemoryRequirements2 memoryRequirements;
	// 	memoryRequirements = device.getAccelerationStructureMemoryRequirementsNV( memoryRequirementsInfo, dldi);

	// 	vk::DeviceSize result = memoryRequirements.memoryRequirements.size;
	// 	return result;
	// };

    /* Get geometry count */
    std::vector<vk::GeometryNV> geometries;
    auto meshes = Mesh::GetFront();
    for (uint32_t i = 0; i < Mesh::GetCount(); ++i) {
        if (!meshes[i].is_initialized()) continue;
        if (meshes[i].get_nv_geometry() != vk::GeometryNV()) geometries.push_back(meshes[i].get_nv_geometry());
    }

	{
		/* Now we can build our acceleration structure */
		vk::MemoryBarrier memoryBarrier;
		memoryBarrier.srcAccessMask  = vk::AccessFlagBits::eAccelerationStructureWriteNV;
		memoryBarrier.srcAccessMask |= vk::AccessFlagBits::eAccelerationStructureReadNV;
		memoryBarrier.dstAccessMask  = vk::AccessFlagBits::eAccelerationStructureWriteNV;
		memoryBarrier.dstAccessMask |= vk::AccessFlagBits::eAccelerationStructureReadNV;

		auto cmd = vulkan->begin_one_time_graphics_command();

		/* TODO: MOVE INSTANCE STUFF INTO HERE */
		{
			vk::AccelerationStructureInfoNV asInfo;
			asInfo.type = vk::AccelerationStructureTypeNV::eTopLevel;
			asInfo.instanceCount = (uint32_t) instances.size();
			asInfo.geometryCount = (uint32_t) geometries.size();
			asInfo.pGeometries = geometries.data();

			cmd.buildAccelerationStructureNV(&asInfo, 
				instanceBuffer, 0, VK_FALSE, 
				topAS, vk::AccelerationStructureNV(),
				accelerationStructureScratchBuffer, 0, dldi);
		}
		
		// cmd.pipelineBarrier(
		//     vk::PipelineStageFlagBits::eAccelerationStructureBuildNV, 
		//     vk::PipelineStageFlagBits::eRayTracingShaderNV, 
		//     vk::DependencyFlags(), {memoryBarrier}, {}, {});

		if (submit_immediately)
			vulkan->end_one_time_graphics_command_immediately(cmd, "build acceleration structure", true);
		else
			vulkan->end_one_time_graphics_command(cmd, "build acceleration structure", true);
	}
}

bool RenderSystem::start()
{
    /* Dont start unless initialied. Dont start twice. */
    if (!initialized) return false;
    if (running) return false;

    auto loop = [this](future<void> futureObj) {
        lastTime = glfwGetTime();
        auto glfw = GLFW::Get();

        while (true)
        {
            if (futureObj.wait_for(.1ms) == future_status::ready) break;

            /* Regulate the framerate. */
            currentTime = glfwGetTime();
            if ((!using_openvr) && ((currentTime - lastTime) < .008)) continue;
            lastTime = currentTime;

            /* Wait until vulkan is initialized before rendering. */
            auto vulkan = Vulkan::Get();
            if (!vulkan->is_initialized()) continue;
            
            /* For error handling purposes related to queue access */
            vulkan->register_main_thread();


            /* 0. Allocate the resources we'll need to render this scene. */
            allocate_vulkan_resources();

            {
                /* Lock the window mutex to get access to swapchains and window textures. */
                std::shared_ptr<std::lock_guard<std::mutex>> window_lock;
                auto window_mutex = glfw->get_mutex();
                auto mutex = window_mutex.get();
                window_lock = std::make_shared<std::lock_guard<std::mutex>>(*mutex);

                /* 1. Optionally aquire swapchain images. Signal image available semaphores. */
                glfw->acquire_swapchain_images(currentFrame);

                /* 2. Record render commands. */
                record_render_commands();

                /* 3. Wait on image available. Enqueue graphics commands. Optionally signal render complete semaphore. */
                enqueue_render_commands();

                /* Submit enqueued graphics commands */
                vulkan->submit_graphics_commands();
                
                /* Wait for GPU to finish execution */
                // If i don't fence here, triangles seem to spread apart. 
                // I think this is an issue with using the SSBO from frame to frame...
                // If the delay on this fence is too short and we receive too many timeouts,
                // the intel graphics driver on windows crashes...
                auto device = vulkan->get_device();
                if (final_fences.size() > 0 )
                {
                    // auto result = device.waitForFences(final_fences, true, 10000000000);
                    // if (result != vk::Result::eSuccess) {
                    //     std::cout<<"Fence timeout in render loop!"<<std::endl;
                    // }

                    /* Cleanup fences. */
                    // for (auto &fence : final_fences) device.destroyFence(fence);
                }

                mark_cameras_as_render_complete();


                /* 4. Optional: Wait on render complete. Present a frame. */
                stream_frames();
                present_openvr_frames();
                glfw->present_glfw_frames(final_renderpass_semaphores);
                vulkan->submit_present_commands();

                download_visibility_queries();

                // vulkan->flush_queues(); // Does this solve some validation issues? EDIT it does. 
                // Need to find a better way to clean up semaphores and things on the compute graph...

                // for (auto &level : compute_graph) {
                //     vk::SemaphoreCreateInfo semaphoreInfo;
                //     vk::FenceCreateInfo fenceInfo;

                //     for (auto &node : level) {
                //         auto device = vulkan->get_device();
                //         for (auto signal_semaphore : node->signal_semaphores) device.destroySemaphore(signal_semaphore);
                //         node->signal_semaphores.clear();
                //         for (auto signal_semaphore : node->window_signal_semaphores) device.destroySemaphore(signal_semaphore);
                //         node->window_signal_semaphores.clear();
                //         if (node->fence != vk::Fence()) device.destroyFence(node->fence);
                //         node->fence = vk::Fence();
                //     }
                // }
            }

            currentFrame = (currentFrame + 1) % max_frames_in_flight;
        }

        release_vulkan_resources();
    };

    /* Use future promise to terminate the loop on stop. */
    exitSignal = promise<void>();
    future<void> futureObj = exitSignal.get_future();
    eventThread = thread(loop, move(futureObj));

    running = true;
    return true;
}

bool RenderSystem::stop()
{
    
    if (!initialized)
        return false;
    if (!running)
        return false;

    exitSignal.set_value();
    eventThread.join();

    running = false;
    return true;
}

/* SETS */
void RenderSystem::set_gamma(float gamma) { this->push_constants.gamma = gamma; }
void RenderSystem::set_exposure(float exposure) { this->push_constants.exposure = exposure; }
void RenderSystem::set_environment_map(int32_t id) { this->push_constants.environment_id = id; }
void RenderSystem::set_environment_map(Texture *texture) { this->push_constants.environment_id = texture->get_id(); }
void RenderSystem::set_environment_roughness(float roughness) { this->push_constants.environment_roughness = roughness; }
void RenderSystem::clear_environment_map() { this->push_constants.environment_id = -1; }
void RenderSystem::set_irradiance_map(int32_t id) { this->push_constants.specular_environment_id = id; }
void RenderSystem::set_irradiance_map(Texture *texture) { this->push_constants.specular_environment_id = texture->get_id(); }
void RenderSystem::clear_irradiance_map() { this->push_constants.specular_environment_id = -1; }
void RenderSystem::set_diffuse_map(int32_t id) { this->push_constants.diffuse_environment_id = id; }
void RenderSystem::set_diffuse_map(Texture *texture) { this->push_constants.diffuse_environment_id = texture->get_id(); }
void RenderSystem::clear_diffuse_map() { this->push_constants.diffuse_environment_id = -1; }
void RenderSystem::set_top_sky_color(glm::vec3 color) { push_constants.top_sky_color = glm::vec4(color.r, color.g, color.b, 1.0); }
void RenderSystem::set_bottom_sky_color(glm::vec3 color) { push_constants.bottom_sky_color = glm::vec4(color.r, color.g, color.b, 1.0); }
void RenderSystem::set_top_sky_color(float r, float g, float b) { set_top_sky_color(glm::vec4(r, g, b, 1.0)); }
void RenderSystem::set_bottom_sky_color(float r, float g, float b) { set_bottom_sky_color(glm::vec4(r, g, b, 1.0)); }
void RenderSystem::set_sky_transition(float transition) { push_constants.sky_transition = transition; }
void RenderSystem::use_openvr(bool useOpenVR) { this->using_openvr = useOpenVR; }
void RenderSystem::use_openvr_hidden_area_masks(bool use_masks) {this->using_vr_hidden_area_masks = use_masks;};

} // namespace Systems
