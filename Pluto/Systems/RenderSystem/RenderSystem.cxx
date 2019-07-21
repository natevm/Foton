// #pragma optimize("", off)

#include "Pluto/Prefabs/Prefabs.hxx"

#include "Pluto/Tools/Options.hxx"
#include "Pluto/Tools/FileReader.hxx"

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
    push_constants.frame = 0;
    push_constants.environment_roughness = 0.0f;
    push_constants.target_id = -1;
    push_constants.camera_id = -1;
    push_constants.brdf_lut_id = -1;
    push_constants.ltc_mat_lut_id = -1;
    push_constants.ltc_amp_lut_id = -1;
    push_constants.environment_id = -1;
    push_constants.diffuse_environment_id = -1;
    push_constants.specular_environment_id = -1;
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

    Texture* sobel = nullptr;
    Texture* ranking = nullptr;
    Texture* scramble = nullptr;

    try {
        brdf = Texture::Get("BRDF");
        ltc_mat = Texture::Get("LTCMAT");
        ltc_amp = Texture::Get("LTCAMP");
        ranking = Texture::Get("RANKINGTILE");
        scramble = Texture::Get("SCRAMBLETILE");
        sobel = Texture::Get("SOBELTILE");
    } catch (...) {}
    if ((!brdf) || (!ltc_mat) || (!ltc_amp) || (!sobel) || (!ranking) || (!scramble)) return false;

    /* Update some push constants */
    auto brdf_id = brdf->get_id();
    auto ltc_mat_id = ltc_mat->get_id();
    auto ltc_amp_id = ltc_amp->get_id();
    push_constants.brdf_lut_id = brdf_id;
    push_constants.ltc_mat_lut_id = ltc_mat_id;
    push_constants.ltc_amp_lut_id = ltc_amp_id;
    push_constants.sobel_tile_id = sobel->get_id();
    push_constants.ranking_tile_id = ranking->get_id();
    push_constants.scramble_tile_id = scramble->get_id();
    push_constants.time = (float) glfwGetTime();
    push_constants.frame++;
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
            reset_bound_material();
            
            /* Get the renderpass for the current camera */
            vk::RenderPass rp = camera->get_depth_prepass(rp_idx);

            /* Bind all descriptor sets to that renderpass.
                Note that we're using a single bind. The same descriptors are shared across pipelines. */
            bind_descriptor_sets(command_buffer, rp);

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
                    draw_entity(command_buffer, rp, *mask_entity, push_constants, RenderMode::RENDER_MODE_VRMASK);
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

                    draw_entity(command_buffer, rp, *visible_entities[rp_idx][i].entity, push_constants, 
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
        reset_bound_material();

        /* Get the renderpass for the current camera */
        vk::RenderPass rp = camera->get_renderpass(rp_idx);

        /* Bind all descriptor sets to that renderpass.
            Note that we're using a single bind. The same descriptors are shared across pipelines. */
        bind_descriptor_sets(command_buffer, rp);
        
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
                draw_entity(command_buffer, rp, *mask_entity, push_constants, RenderMode::RENDER_MODE_VRMASK);
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
                draw_entity(command_buffer, rp, *visible_entities[rp_idx][i].entity, push_constants, camera->get_rendermode_override());
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
                draw_entity(command_buffer, rp, *visible_entities[rp_idx][i].entity, push_constants, 
                    camera->get_rendermode_override(), PipelineType::PIPELINE_TYPE_DEPTH_TEST_GREATER);
            }
        }
        camera->end_renderpass(command_buffer, rp_idx);
    }
}

void RenderSystem::record_ray_trace_pass(Entity &camera_entity)
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
    
    if (!Material::IsInitialized()) return;

    texture->make_general(command_buffer);

    for(uint32_t rp_idx = 0; rp_idx < camera->get_num_renderpasses(); rp_idx++) {
        reset_bound_material();

        /* Get the renderpass for the current camera */
        vk::RenderPass rp = camera->get_renderpass(rp_idx);
        
        update_raytracing_descriptor_sets(topAS, camera_entity);
        bind_raytracing_descriptor_sets(command_buffer, rp);

        push_constants.target_id = -1;
        push_constants.camera_id = camera_entity.get_id();
        push_constants.viewIndex = rp_idx;
        push_constants.flags = (!camera->should_use_multiview()) ? (push_constants.flags | 1 << 0) : (push_constants.flags & ~(1 << 0)); 
        
        trace_rays(command_buffer, rp, push_constants, *texture);
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

        if (ray_tracing_enabled && vulkan->is_ray_tracing_enabled() && (entities[entity_id].get_light() == nullptr))
        {
            record_ray_trace_pass(entities[entity_id]);
        }
        else if (!ray_tracing_enabled)
        {
            auto visible_entities = camera->get_visible_entities(entity_id);
            record_depth_prepass(entities[entity_id], visible_entities);
            record_raster_renderpass(entities[entity_id], visible_entities);
        }

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

    if (Material::IsInitialized()) {
        update_raster_descriptor_sets();
        
        if (update_push_constants() == true) {
            record_cameras();
        }
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
    semaphores_submitted();
    fences_submitted();
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
    device.freeCommandBuffers(vulkan->get_graphics_command_pool(), {main_command_buffer});
    main_command_buffer = vk::CommandBuffer();
    // device.destroyFence(main_fence);
    // for (auto &semaphore : main_command_buffer_semaphores)
    //     device.destroySemaphore(semaphore);

    device.destroyDescriptorSetLayout(componentDescriptorSetLayout);
	device.destroyDescriptorPool(componentDescriptorPool);

	device.destroyDescriptorSetLayout(textureDescriptorSetLayout);
	device.destroyDescriptorPool(textureDescriptorPool);

	device.destroyDescriptorSetLayout(positionsDescriptorSetLayout);
	device.destroyDescriptorPool(positionsDescriptorPool);

	device.destroyDescriptorSetLayout(normalsDescriptorSetLayout);
	device.destroyDescriptorPool(normalsDescriptorPool);

	device.destroyDescriptorSetLayout(colorsDescriptorSetLayout);
	device.destroyDescriptorPool(colorsDescriptorPool);

	device.destroyDescriptorSetLayout(texcoordsDescriptorSetLayout);
	device.destroyDescriptorPool(texcoordsDescriptorPool);

	device.destroyDescriptorSetLayout(indexDescriptorSetLayout);
	device.destroyDescriptorPool(indexDescriptorPool);

    componentDescriptorSetLayout = vk::DescriptorSetLayout();
	componentDescriptorPool = vk::DescriptorPool();
	textureDescriptorSetLayout = vk::DescriptorSetLayout();
	textureDescriptorPool = vk::DescriptorPool();
	positionsDescriptorSetLayout = vk::DescriptorSetLayout();
	positionsDescriptorPool = vk::DescriptorPool();
	normalsDescriptorSetLayout = vk::DescriptorSetLayout();
	normalsDescriptorPool = vk::DescriptorPool();
	colorsDescriptorSetLayout = vk::DescriptorSetLayout();
	colorsDescriptorPool = vk::DescriptorPool();
	texcoordsDescriptorSetLayout = vk::DescriptorSetLayout();
	texcoordsDescriptorPool = vk::DescriptorPool();
	indexDescriptorSetLayout = vk::DescriptorSetLayout();
	indexDescriptorPool = vk::DescriptorPool();

	for (auto item : shaderModuleCache) {
		device.destroyShaderModule(item.second);
	}
    
    vulkan_resources_created = false;
}

void RenderSystem::allocate_vulkan_resources()
{
    /* Don't create vulkan resources more than once. */
    if (vulkan_resources_created) return;

    create_descriptor_set_layouts();
	create_descriptor_pools();
	create_vertex_input_binding_descriptions();
	create_vertex_attribute_descriptions();
	update_raster_descriptor_sets();

    auto vulkan = Vulkan::Get();
    auto device = vulkan->get_device();
    auto dldi = vulkan->get_dldi();
    
    /* Create a main command buffer if one does not already exist */
    vk::CommandBufferAllocateInfo mainCmdAllocInfo;
    mainCmdAllocInfo.level = vk::CommandBufferLevel::ePrimary;
    mainCmdAllocInfo.commandPool = vulkan->get_graphics_command_pool();
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
            || (entities[i].material()->get_name().compare("SkyboxMaterial") == 0)
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

            uint64_t handle = entities[i].mesh()->get_low_level_bvh_handle();
            /* Create Instance */
            
            auto local_to_world = entities[i].transform()->get_local_to_world_matrix();
            // Matches a working example
            float transform[12] = {
                local_to_world[0][0], local_to_world[1][0], local_to_world[2][0], local_to_world[3][0],
                local_to_world[0][1], local_to_world[1][1], local_to_world[2][1], local_to_world[3][1],
                local_to_world[0][2], local_to_world[1][2], local_to_world[2][2], local_to_world[3][2],
            };

            memcpy(instance.transform, transform, sizeof(instance.transform));
            instance.instanceId = i;
            instance.mask = 0xff;
            instance.instanceOffset = 0;
            // instance.flags = (uint32_t) (vk::GeometryInstanceFlagBitsNV::e);
            instance.accelerationStructureHandle = handle;
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
			asInfo.geometryCount = 0;//(uint32_t) geometries.size();
			// asInfo.pGeometries = geometries.data();

            // TODO: change this to update instead of fresh build
			cmd.buildAccelerationStructureNV(&asInfo, 
				instanceBuffer, 0, VK_FALSE, 
				topAS, vk::AccelerationStructureNV(),
				accelerationStructureScratchBuffer, 0, dldi);
		}
		
		// cmd.pipelineBarrier(
		//     vk::PipelineStageFlagBits::eAccelerationStructureBuildNV, 
		//     vk::PipelineStageFlagBits::eRayTracingShaderNV, 
		//     vk::DependencyFlags(), {memoryBarrier}, {}, {});

        // vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, 0, 1, &memoryBarrier, 0, 0, 0, 0);

		if (submit_immediately)
			vulkan->end_one_time_graphics_command_immediately(cmd, "build acceleration structure", true);
		else
			vulkan->end_one_time_graphics_command(cmd, "build acceleration structure", true);

        /* Get a handle to the acceleration structure */
	    device.getAccelerationStructureHandleNV(topAS, sizeof(uint64_t), &topASHandle, dldi);
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

/* Wrapper for shader module creation */
vk::ShaderModule RenderSystem::create_shader_module(std::string name, const std::vector<char>& code) {
	if (shaderModuleCache.find(name) != shaderModuleCache.end()) return shaderModuleCache[name];
	auto vulkan = Libraries::Vulkan::Get();
	auto device = vulkan->get_device();

	vk::ShaderModuleCreateInfo createInfo;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	vk::ShaderModule shaderModule = device.createShaderModule(createInfo);
	shaderModuleCache[name] = shaderModule;
	return shaderModule;
}

/* Under the hood, all material types have a set of Vulkan pipeline objects. */
void RenderSystem::create_raster_pipeline(
	std::vector<vk::PipelineShaderStageCreateInfo> shaderStages, // yes
	std::vector<vk::VertexInputBindingDescription> bindingDescriptions, // yes
	std::vector<vk::VertexInputAttributeDescription> attributeDescriptions, // yes
	std::vector<vk::DescriptorSetLayout> componentDescriptorSetLayouts, // yes
	PipelineParameters parameters,
	vk::RenderPass renderpass,
	uint32 subpass,
	std::unordered_map<PipelineType, vk::Pipeline> &pipelines,
	vk::PipelineLayout &layout 
) {
	auto vulkan = Libraries::Vulkan::Get();
	auto device = vulkan->get_device();

	/* Vertex Input */
	vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
	vertexInputInfo.vertexBindingDescriptionCount = (uint32_t)bindingDescriptions.size();
	vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions.data();
	vertexInputInfo.vertexAttributeDescriptionCount = (uint32_t)attributeDescriptions.size();
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	vk::PushConstantRange range;
	range.offset = 0;
	range.size = sizeof(PushConsts);
	range.stageFlags = vk::ShaderStageFlagBits::eAll;

	/* Connect things together with pipeline layout */
	vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
	pipelineLayoutInfo.setLayoutCount = (uint32_t)componentDescriptorSetLayouts.size();
	pipelineLayoutInfo.pSetLayouts = componentDescriptorSetLayouts.data();
	pipelineLayoutInfo.pushConstantRangeCount = 1; // TODO: this needs to account for entity id
	pipelineLayoutInfo.pPushConstantRanges = &range; // TODO: this needs to account for entity id

	/* Create the pipeline layout */
	layout = device.createPipelineLayout(pipelineLayoutInfo);
	
	vk::GraphicsPipelineCreateInfo pipelineInfo;
	pipelineInfo.stageCount = (uint32_t)shaderStages.size();
	pipelineInfo.pStages = shaderStages.data();
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &parameters.inputAssembly;
	pipelineInfo.pViewportState = &parameters.viewportState;
	pipelineInfo.pRasterizationState = &parameters.rasterizer;
	pipelineInfo.pMultisampleState = &parameters.multisampling;
	pipelineInfo.pDepthStencilState = &parameters.depthStencil;
	pipelineInfo.pColorBlendState = &parameters.colorBlending;
	pipelineInfo.pDynamicState = &parameters.dynamicState; // Optional
	pipelineInfo.layout = layout;
	pipelineInfo.renderPass = renderpass;
	pipelineInfo.subpass = subpass;
	pipelineInfo.basePipelineHandle = vk::Pipeline(); // Optional
	pipelineInfo.basePipelineIndex = -1; // Optional

	#ifndef DISABLE_REVERSE_Z
	auto prepassDepthCompareOp = vk::CompareOp::eGreater;
	auto prepassDepthCompareOpEqual = vk::CompareOp::eGreaterOrEqual;
	auto depthCompareOpGreaterEqual = vk::CompareOp::eGreaterOrEqual;
	auto depthCompareOpLessEqual = vk::CompareOp::eLessOrEqual;
	#else
	auto prepassDepthCompareOp = vk::CompareOp::eLess;
	auto prepassDepthCompareOpEqual = vk::CompareOp::eLessOrEqual;
	auto depthCompareOpGreaterEqual = vk::CompareOp::eLessOrEqual;
	auto depthCompareOpLessEqual = vk::CompareOp::eGreaterOrEqual;
	#endif	


	/* Create pipeline */
	pipelines[PIPELINE_TYPE_NORMAL] = device.createGraphicsPipelines(vk::PipelineCache(), {pipelineInfo})[0];
	
	auto old_depth_test_enable_setting = parameters.depthStencil.depthTestEnable;
	auto old_depth_write_enable_setting = parameters.depthStencil.depthWriteEnable;
	auto old_cull_mode = parameters.rasterizer.cullMode;
	auto old_depth_compare_op = parameters.depthStencil.depthCompareOp;

	parameters.depthStencil.setDepthTestEnable(true);
	parameters.depthStencil.setDepthWriteEnable(false); // possibly rename this?

	// fragmentdepth[renderpass].pipelineParameters.depthStencil.depthCompareOp = depthCompareOpEqual;
	// fragmentdepth[renderpass].pipelineParameters.rasterizer.cullMode = vk::CullModeFlagBits::eNone;
	parameters.depthStencil.setDepthCompareOp(prepassDepthCompareOpEqual);	// Needed for transparent objects.
	// parameters.depthStencil.setDepthCompareOp(prepassDepthCompareOpEqual);	
	
	parameters.rasterizer.setCullMode(vk::CullModeFlagBits::eNone); // possibly rename this?
	pipelines[PIPELINE_TYPE_DEPTH_WRITE_DISABLED] = device.createGraphicsPipelines(vk::PipelineCache(), {pipelineInfo})[0];
	
	parameters.depthStencil.setDepthCompareOp(prepassDepthCompareOpEqual);	
	parameters.rasterizer.setCullMode(old_cull_mode); // possibly rename this?
	parameters.depthStencil.setDepthWriteEnable(true); // possibly rename this?

	// parameters.depthStencil.setDepthWriteEnable(old_depth_write_enable_setting);
	// parameters.depthStencil.setDepthTestEnable(true);
	parameters.depthStencil.setDepthCompareOp(depthCompareOpGreaterEqual);
	pipelines[PIPELINE_TYPE_DEPTH_TEST_GREATER] = device.createGraphicsPipelines(vk::PipelineCache(), {pipelineInfo})[0];
	parameters.depthStencil.setDepthCompareOp(depthCompareOpLessEqual);
	pipelines[PIPELINE_TYPE_DEPTH_TEST_LESS] = device.createGraphicsPipelines(vk::PipelineCache(), {pipelineInfo})[0];
	parameters.depthStencil.setDepthTestEnable(old_depth_test_enable_setting);
	parameters.depthStencil.setDepthCompareOp(old_depth_compare_op);
	parameters.depthStencil.setDepthWriteEnable(old_depth_write_enable_setting);


	// auto old_fill_setting = pipelineInfo.pRasterizationState->polygonMode;
	// pipelineInfo.pRasterizationState->polygonMode = vk::Poly

	auto old_polygon_mode = parameters.rasterizer.polygonMode;
	parameters.rasterizer.polygonMode = vk::PolygonMode::eLine;
	pipelines[PIPELINE_TYPE_WIREFRAME] = device.createGraphicsPipelines(vk::PipelineCache(), {pipelineInfo})[0];
	parameters.rasterizer.polygonMode = old_polygon_mode;
}


/* Compiles all shaders */
void RenderSystem::setup_graphics_pipelines(vk::RenderPass renderpass, uint32_t sampleCount, bool use_depth_prepass)
{
	auto vulkan = Libraries::Vulkan::Get();
	auto device = vulkan->get_device();

	auto sampleFlag = vulkan->highest(vulkan->min(vulkan->get_closest_sample_count_flag(sampleCount), vulkan->get_msaa_sample_flags()));

	#ifndef DISABLE_REVERSE_Z
	auto depthCompareOp = vk::CompareOp::eGreater;
	auto depthCompareOpEqual = vk::CompareOp::eGreaterOrEqual;
	#else
	auto depthCompareOp = vk::CompareOp::eLess;
	auto depthCompareOpEqual = vk::CompareOp::eLessOrEqual;
	#endif

	/* RASTER GRAPHICS PIPELINES */

	/* ------ UNIFORM COLOR  ------ */
	// {
	// 	uniformColor[renderpass] = RasterPipelineResources();

	// 	std::string ResourcePath = Options::GetResourcePath();
	// 	auto vertShaderCode = readFile(ResourcePath + std::string("/Shaders/SurfaceMaterials/UniformColor/vert.spv"));
	// 	auto fragShaderCode = readFile(ResourcePath + std::string("/Shaders/SurfaceMaterials/UniformColor/frag.spv"));

	// 	/* Create shader modules */
	// 	auto vertShaderModule = create_shader_module("uniform_color_vert", vertShaderCode);
	// 	auto fragShaderModule = create_shader_module("uniform_color_frag", fragShaderCode);

	// 	/* Info for shader stages */
	// 	vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
	// 	vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
	// 	vertShaderStageInfo.module = vertShaderModule;
	// 	vertShaderStageInfo.pName = "main";

	// 	vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
	// 	fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
	// 	fragShaderStageInfo.module = fragShaderModule;
	// 	fragShaderStageInfo.pName = "main";

	// 	std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertShaderStageInfo, fragShaderStageInfo };
		
	// 	/* Account for possibly multiple samples */
	// 	uniformColor[renderpass].pipelineParameters.multisampling.sampleShadingEnable = (sampleFlag == vk::SampleCountFlagBits::e1) ? false : true;
	// 	uniformColor[renderpass].pipelineParameters.multisampling.rasterizationSamples = sampleFlag;

	// 	create_raster_pipeline(shaderStages, vertexInputBindingDescriptions, vertexInputAttributeDescriptions, 
	// 		{ componentDescriptorSetLayout, textureDescriptorSetLayout }, 
	// 		uniformColor[renderpass].pipelineParameters, 
	// 		renderpass, 0, 
	// 		uniformColor[renderpass].pipelines, uniformColor[renderpass].pipelineLayout);

	// }


	/* ------ BLINN GRAPHICS ------ */
	// {
	// 	blinn[renderpass] = RasterPipelineResources();

	// 	std::string ResourcePath = Options::GetResourcePath();
	// 	auto vertShaderCode = readFile(ResourcePath + std::string("/Shaders/SurfaceMaterials/Blinn/vert.spv"));
	// 	auto fragShaderCode = readFile(ResourcePath + std::string("/Shaders/SurfaceMaterials/Blinn/frag.spv"));

	// 	/* Create shader modules */
	// 	auto vertShaderModule = create_shader_module("blinn_vert", vertShaderCode);
	// 	auto fragShaderModule = create_shader_module("blinn_frag", fragShaderCode);

	// 	/* Info for shader stages */
	// 	vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
	// 	vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
	// 	vertShaderStageInfo.module = vertShaderModule;
	// 	vertShaderStageInfo.pName = "main";

	// 	vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
	// 	fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
	// 	fragShaderStageInfo.module = fragShaderModule;
	// 	fragShaderStageInfo.pName = "main";

	// 	std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertShaderStageInfo, fragShaderStageInfo };
		
	// 	/* Account for possibly multiple samples */
	// 	blinn[renderpass].pipelineParameters.multisampling.sampleShadingEnable = (sampleFlag == vk::SampleCountFlagBits::e1) ? false : true;
	// 	blinn[renderpass].pipelineParameters.multisampling.rasterizationSamples = sampleFlag;

	// 	create_raster_pipeline(shaderStages, vertexInputBindingDescriptions, vertexInputAttributeDescriptions, 
	// 		{ componentDescriptorSetLayout, textureDescriptorSetLayout }, 
	// 		blinn[renderpass].pipelineParameters, 
	// 		renderpass, 0, 
	// 		blinn[renderpass].pipelines, blinn[renderpass].pipelineLayout);

	// }

	/* ------ PBR  ------ */
	{
		pbr[renderpass] = RasterPipelineResources();

		std::string ResourcePath = Options::GetResourcePath();
		auto vertShaderCode = readFile(ResourcePath + std::string("/Shaders/SurfaceMaterials/PBRSurface/vert.spv"));
		auto fragShaderCode = readFile(ResourcePath + std::string("/Shaders/SurfaceMaterials/PBRSurface/frag.spv"));

		/* Create shader modules */
		auto vertShaderModule = create_shader_module("pbr_vert", vertShaderCode);
		auto fragShaderModule = create_shader_module("pbr_frag", fragShaderCode);

		/* Info for shader stages */
		vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
		vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName = "main";

		vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
		fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
		fragShaderStageInfo.module = fragShaderModule;
		fragShaderStageInfo.pName = "main";

		std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertShaderStageInfo, fragShaderStageInfo };
		
		/* Account for possibly multiple samples */
		pbr[renderpass].pipelineParameters.multisampling.sampleShadingEnable = (sampleFlag == vk::SampleCountFlagBits::e1) ? false : true;
		pbr[renderpass].pipelineParameters.multisampling.rasterizationSamples = sampleFlag;
		
		/* Because we use a depth prepass */
		if (use_depth_prepass) {
			pbr[renderpass].pipelineParameters.depthStencil.depthTestEnable = true;
			pbr[renderpass].pipelineParameters.depthStencil.depthWriteEnable = false; // not VK_TRUE since we have a depth prepass
			pbr[renderpass].pipelineParameters.depthStencil.depthCompareOp = depthCompareOpEqual;
		}
		// depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL; //not VK_COMPARE_OP_LESS since we have a depth prepass;
		
		create_raster_pipeline(shaderStages, vertexInputBindingDescriptions, vertexInputAttributeDescriptions, 
			{ componentDescriptorSetLayout, textureDescriptorSetLayout}, 
			pbr[renderpass].pipelineParameters, 
			renderpass, 0, 
			pbr[renderpass].pipelines, pbr[renderpass].pipelineLayout);

	}

	// /* ------ NORMAL SURFACE ------ */
	// {
	// 	normalsurface[renderpass] = RasterPipelineResources();

	// 	std::string ResourcePath = Options::GetResourcePath();
	// 	auto vertShaderCode = readFile(ResourcePath + std::string("/Shaders/SurfaceMaterials/NormalSurface/vert.spv"));
	// 	auto fragShaderCode = readFile(ResourcePath + std::string("/Shaders/SurfaceMaterials/NormalSurface/frag.spv"));

	// 	/* Create shader modules */
	// 	auto vertShaderModule = create_shader_module("normal_surf_vert", vertShaderCode);
	// 	auto fragShaderModule = create_shader_module("normal_surf_frag", fragShaderCode);

	// 	/* Info for shader stages */
	// 	vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
	// 	vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
	// 	vertShaderStageInfo.module = vertShaderModule;
	// 	vertShaderStageInfo.pName = "main";

	// 	vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
	// 	fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
	// 	fragShaderStageInfo.module = fragShaderModule;
	// 	fragShaderStageInfo.pName = "main";

	// 	std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertShaderStageInfo, fragShaderStageInfo };
		
	// 	/* Account for possibly multiple samples */
	// 	normalsurface[renderpass].pipelineParameters.multisampling.sampleShadingEnable = (sampleFlag == vk::SampleCountFlagBits::e1) ? false : true;
	// 	normalsurface[renderpass].pipelineParameters.multisampling.rasterizationSamples = sampleFlag;

	// 	create_raster_pipeline(shaderStages, vertexInputBindingDescriptions, vertexInputAttributeDescriptions, 
	// 		{ componentDescriptorSetLayout, textureDescriptorSetLayout }, 
	// 		normalsurface[renderpass].pipelineParameters, 
	// 		renderpass, 0, 
	// 		normalsurface[renderpass].pipelines, normalsurface[renderpass].pipelineLayout);

	// }

	/* ------ TEXCOORD SURFACE  ------ */
	// {
	// 	texcoordsurface[renderpass] = RasterPipelineResources();

	// 	std::string ResourcePath = Options::GetResourcePath();
	// 	auto vertShaderCode = readFile(ResourcePath + std::string("/Shaders/SurfaceMaterials/TexCoordSurface/vert.spv"));
	// 	auto fragShaderCode = readFile(ResourcePath + std::string("/Shaders/SurfaceMaterials/TexCoordSurface/frag.spv"));

	// 	/* Create shader modules */
	// 	auto vertShaderModule = create_shader_module("texcoord_vert", vertShaderCode);
	// 	auto fragShaderModule = create_shader_module("texcoord_frag", fragShaderCode);

	// 	/* Info for shader stages */
	// 	vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
	// 	vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
	// 	vertShaderStageInfo.module = vertShaderModule;
	// 	vertShaderStageInfo.pName = "main";

	// 	vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
	// 	fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
	// 	fragShaderStageInfo.module = fragShaderModule;
	// 	fragShaderStageInfo.pName = "main";

	// 	std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertShaderStageInfo, fragShaderStageInfo };
		
	// 	/* Account for possibly multiple samples */
	// 	texcoordsurface[renderpass].pipelineParameters.multisampling.sampleShadingEnable = (sampleFlag == vk::SampleCountFlagBits::e1) ? false : true;
	// 	texcoordsurface[renderpass].pipelineParameters.multisampling.rasterizationSamples = sampleFlag;

	// 	create_raster_pipeline(shaderStages, vertexInputBindingDescriptions, vertexInputAttributeDescriptions, 
	// 		{ componentDescriptorSetLayout, textureDescriptorSetLayout }, 
	// 		texcoordsurface[renderpass].pipelineParameters, 
	// 		renderpass, 0, 
	// 		texcoordsurface[renderpass].pipelines, texcoordsurface[renderpass].pipelineLayout);

	// }

	/* ------ SKYBOX  ------ */
	{
		skybox[renderpass] = RasterPipelineResources();

		std::string ResourcePath = Options::GetResourcePath();
		auto vertShaderCode = readFile(ResourcePath + std::string("/Shaders/SurfaceMaterials/Skybox/vert.spv"));
		auto fragShaderCode = readFile(ResourcePath + std::string("/Shaders/SurfaceMaterials/Skybox/frag.spv"));

		/* Create shader modules */
		auto vertShaderModule = create_shader_module("skybox_vert", vertShaderCode);
		auto fragShaderModule = create_shader_module("skybox_frag", fragShaderCode);

		/* Info for shader stages */
		vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
		vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName = "main";

		vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
		fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
		fragShaderStageInfo.module = fragShaderModule;
		fragShaderStageInfo.pName = "main";

		std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertShaderStageInfo, fragShaderStageInfo };
		
		/* Skyboxes don't do back face culling. */
		skybox[renderpass].pipelineParameters.rasterizer.setCullMode(vk::CullModeFlagBits::eNone);

		/* Account for possibly multiple samples */
		skybox[renderpass].pipelineParameters.multisampling.sampleShadingEnable = (sampleFlag == vk::SampleCountFlagBits::e1) ? false : true;
		skybox[renderpass].pipelineParameters.multisampling.rasterizationSamples = sampleFlag;

		create_raster_pipeline(shaderStages, vertexInputBindingDescriptions, vertexInputAttributeDescriptions, 
			{ componentDescriptorSetLayout, textureDescriptorSetLayout }, 
			skybox[renderpass].pipelineParameters, 
			renderpass, 0, 
			skybox[renderpass].pipelines, skybox[renderpass].pipelineLayout);

	}

	/* ------ DEPTH  ------ */
	// {
	// 	depth[renderpass] = RasterPipelineResources();

	// 	std::string ResourcePath = Options::GetResourcePath();
	// 	auto vertShaderCode = readFile(ResourcePath + std::string("/Shaders/SurfaceMaterials/Depth/vert.spv"));
	// 	auto fragShaderCode = readFile(ResourcePath + std::string("/Shaders/SurfaceMaterials/Depth/frag.spv"));

	// 	/* Create shader modules */
	// 	auto vertShaderModule = create_shader_module("depth_vert", vertShaderCode);
	// 	auto fragShaderModule = create_shader_module("depth_frag", fragShaderCode);

	// 	/* Info for shader stages */
	// 	vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
	// 	vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
	// 	vertShaderStageInfo.module = vertShaderModule;
	// 	vertShaderStageInfo.pName = "main";

	// 	vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
	// 	fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
	// 	fragShaderStageInfo.module = fragShaderModule;
	// 	fragShaderStageInfo.pName = "main";

	// 	std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertShaderStageInfo, fragShaderStageInfo };
		
	// 	/* Account for possibly multiple samples */
	// 	depth[renderpass].pipelineParameters.multisampling.sampleShadingEnable = (sampleFlag == vk::SampleCountFlagBits::e1) ? false : true;
	// 	depth[renderpass].pipelineParameters.multisampling.rasterizationSamples = sampleFlag;
	// 	// depth[renderpass].pipelineParameters.rasterizer.cullMode = vk::CullModeFlagBits::eNone;

	// 	/* Because we use a depth prepass */
	// 	if (use_depth_prepass) {
	// 		depth[renderpass].pipelineParameters.depthStencil.depthTestEnable = true;
	// 		depth[renderpass].pipelineParameters.depthStencil.depthWriteEnable = false; // not VK_TRUE since we have a depth prepass
	// 		depth[renderpass].pipelineParameters.depthStencil.depthCompareOp = depthCompareOpEqual;
	// 	}

	// 	create_raster_pipeline(shaderStages, vertexInputBindingDescriptions, vertexInputAttributeDescriptions, 
	// 		{ componentDescriptorSetLayout, textureDescriptorSetLayout }, 
	// 		depth[renderpass].pipelineParameters, 
	// 		renderpass, 0, 
	// 		depth[renderpass].pipelines, depth[renderpass].pipelineLayout);

	// }

	/* ------ Volume  ------ */
	{
		volume[renderpass] = RasterPipelineResources();

		std::string ResourcePath = Options::GetResourcePath();
		auto vertShaderCode = readFile(ResourcePath + std::string("/Shaders/VolumeMaterials/Volume/vert.spv"));
		auto fragShaderCode = readFile(ResourcePath + std::string("/Shaders/VolumeMaterials/Volume/frag.spv"));

		/* Create shader modules */
		auto vertShaderModule = create_shader_module("volume_vert", vertShaderCode);
		auto fragShaderModule = create_shader_module("volume_frag", fragShaderCode);

		/* Info for shader stages */
		vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
		vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName = "main";

		vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
		fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
		fragShaderStageInfo.module = fragShaderModule;
		fragShaderStageInfo.pName = "main";

		std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertShaderStageInfo, fragShaderStageInfo };
		
		/* Account for possibly multiple samples */
		volume[renderpass].pipelineParameters.multisampling.sampleShadingEnable = (sampleFlag == vk::SampleCountFlagBits::e1) ? false : true;
		volume[renderpass].pipelineParameters.multisampling.rasterizationSamples = sampleFlag;

		create_raster_pipeline(shaderStages, vertexInputBindingDescriptions, vertexInputAttributeDescriptions, 
			{ componentDescriptorSetLayout, textureDescriptorSetLayout}, 
			volume[renderpass].pipelineParameters, 
			renderpass, 0, 
			volume[renderpass].pipelines, volume[renderpass].pipelineLayout);

	}

	/*  G BUFFERS  */

	/* ------ Fragment Depth  ------ */
	{
		fragmentdepth[renderpass] = RasterPipelineResources();

		std::string ResourcePath = Options::GetResourcePath();
		auto vertShaderCode = readFile(ResourcePath + std::string("/Shaders/GBuffers/FragmentDepth/vert.spv"));

		/* Create shader modules */
		auto vertShaderModule = create_shader_module("frag_depth", vertShaderCode);

		/* Info for shader stages */
		vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
		vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName = "main";

		std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertShaderStageInfo };
		
		/* Account for possibly multiple samples */
		fragmentdepth[renderpass].pipelineParameters.multisampling.sampleShadingEnable = (sampleFlag == vk::SampleCountFlagBits::e1) ? false : true;
		fragmentdepth[renderpass].pipelineParameters.multisampling.rasterizationSamples = sampleFlag;

		fragmentdepth[renderpass].pipelineParameters.depthStencil.depthWriteEnable = true;
		fragmentdepth[renderpass].pipelineParameters.depthStencil.depthCompareOp = depthCompareOpEqual;
		fragmentdepth[renderpass].pipelineParameters.rasterizer.cullMode = vk::CullModeFlagBits::eNone;

		create_raster_pipeline(shaderStages, vertexInputBindingDescriptions, vertexInputAttributeDescriptions, 
			{ componentDescriptorSetLayout, textureDescriptorSetLayout }, 
			fragmentdepth[renderpass].pipelineParameters, 
			renderpass, 0, 
			fragmentdepth[renderpass].pipelines, fragmentdepth[renderpass].pipelineLayout);

		// device.destroyShaderModule(vertShaderModule);
	}

	/* ------ All G Buffers  ------ */
	{
		gbuffers[renderpass] = RasterPipelineResources();

		std::string ResourcePath = Options::GetResourcePath();
		auto vertShaderCode = readFile(ResourcePath + std::string("/Shaders/GBuffers/vert.spv"));
		auto fragShaderCode = readFile(ResourcePath + std::string("/Shaders/GBuffers/frag.spv"));

		/* Create shader modules */
		auto vertShaderModule = create_shader_module("gbuffers_vert", vertShaderCode);
		auto fragShaderModule = create_shader_module("gbuffers_frag", fragShaderCode);

		/* Info for shader stages */
		vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
		vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName = "main";

		vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
		fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
		fragShaderStageInfo.module = fragShaderModule;
		fragShaderStageInfo.pName = "main";

		std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertShaderStageInfo, fragShaderStageInfo };
		
		/* Account for possibly multiple samples */
		gbuffers[renderpass].pipelineParameters.multisampling.sampleShadingEnable = (sampleFlag == vk::SampleCountFlagBits::e1) ? false : true;
		gbuffers[renderpass].pipelineParameters.multisampling.rasterizationSamples = sampleFlag;
		gbuffers[renderpass].pipelineParameters.rasterizer.cullMode = vk::CullModeFlagBits::eNone;

		/* Because we use a depth prepass */
		if (use_depth_prepass) {
			gbuffers[renderpass].pipelineParameters.depthStencil.depthTestEnable = true;
			gbuffers[renderpass].pipelineParameters.depthStencil.depthWriteEnable = false; // not VK_TRUE since we have a depth prepass
			gbuffers[renderpass].pipelineParameters.depthStencil.depthCompareOp = depthCompareOpEqual;
		}

		create_raster_pipeline(shaderStages, vertexInputBindingDescriptions, vertexInputAttributeDescriptions, 
			{ componentDescriptorSetLayout, textureDescriptorSetLayout }, 
			gbuffers[renderpass].pipelineParameters, 
			renderpass, 0, 
			gbuffers[renderpass].pipelines, gbuffers[renderpass].pipelineLayout);

	}

	/* ------ SHADOWMAP  ------ */

	{
		shadowmap[renderpass] = RasterPipelineResources();

		std::string ResourcePath = Options::GetResourcePath();
		auto vertShaderCode = readFile(ResourcePath + std::string("/Shaders/GBuffers/ShadowMap/vert.spv"));
		auto fragShaderCode = readFile(ResourcePath + std::string("/Shaders/GBuffers/ShadowMap/frag.spv"));

		/* Create shader modules */
		auto vertShaderModule = create_shader_module("shadow_map_vert", vertShaderCode);
		auto fragShaderModule = create_shader_module("shadow_map_frag", fragShaderCode);

		/* Info for shader stages */
		vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
		vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName = "main";

		vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
		fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
		fragShaderStageInfo.module = fragShaderModule;
		fragShaderStageInfo.pName = "main";

		std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertShaderStageInfo, fragShaderStageInfo };
		
		/* Account for possibly multiple samples */
		shadowmap[renderpass].pipelineParameters.multisampling.sampleShadingEnable = (sampleFlag == vk::SampleCountFlagBits::e1) ? false : true;
		shadowmap[renderpass].pipelineParameters.multisampling.rasterizationSamples = sampleFlag;
		shadowmap[renderpass].pipelineParameters.rasterizer.cullMode = vk::CullModeFlagBits::eNone;

		/* Because we use a depth prepass */
		if (use_depth_prepass) {
			shadowmap[renderpass].pipelineParameters.depthStencil.depthTestEnable = true;
			shadowmap[renderpass].pipelineParameters.depthStencil.depthWriteEnable = false; // not VK_TRUE since we have a depth prepass
			shadowmap[renderpass].pipelineParameters.depthStencil.depthCompareOp = depthCompareOpEqual;
		}

		create_raster_pipeline(shaderStages, vertexInputBindingDescriptions, vertexInputAttributeDescriptions, 
			{ componentDescriptorSetLayout, textureDescriptorSetLayout }, 
			shadowmap[renderpass].pipelineParameters, 
			renderpass, 0, 
			shadowmap[renderpass].pipelines, shadowmap[renderpass].pipelineLayout);

	}

	/* ------ FRAGMENT POSITION  ------ */

	// {
	// 	fragmentposition[renderpass] = RasterPipelineResources();

	// 	std::string ResourcePath = Options::GetResourcePath();
	// 	auto vertShaderCode = readFile(ResourcePath + std::string("/Shaders/GBuffers/FragmentPosition/vert.spv"));
	// 	auto fragShaderCode = readFile(ResourcePath + std::string("/Shaders/GBuffers/FragmentPosition/frag.spv"));

	// 	/* Create shader modules */
	// 	auto vertShaderModule = create_shader_module("frag_pos_vert", vertShaderCode);
	// 	auto fragShaderModule = create_shader_module("frag_pos_frag", fragShaderCode);

	// 	/* Info for shader stages */
	// 	vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
	// 	vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
	// 	vertShaderStageInfo.module = vertShaderModule;
	// 	vertShaderStageInfo.pName = "main";

	// 	vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
	// 	fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
	// 	fragShaderStageInfo.module = fragShaderModule;
	// 	fragShaderStageInfo.pName = "main";

	// 	std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertShaderStageInfo, fragShaderStageInfo };
		
	// 	/* Account for possibly multiple samples */
	// 	fragmentposition[renderpass].pipelineParameters.multisampling.sampleShadingEnable = (sampleFlag == vk::SampleCountFlagBits::e1) ? false : true;
	// 	fragmentposition[renderpass].pipelineParameters.multisampling.rasterizationSamples = sampleFlag;
	// 	fragmentposition[renderpass].pipelineParameters.rasterizer.cullMode = vk::CullModeFlagBits::eNone;

	// 	/* Because we use a depth prepass */
	// 	if (use_depth_prepass) {
	// 		fragmentposition[renderpass].pipelineParameters.depthStencil.depthTestEnable = true;
	// 		fragmentposition[renderpass].pipelineParameters.depthStencil.depthWriteEnable = false; // not VK_TRUE since we have a depth prepass
	// 		fragmentposition[renderpass].pipelineParameters.depthStencil.depthCompareOp = depthCompareOpEqual;
	// 	}

	// 	create_raster_pipeline(shaderStages, vertexInputBindingDescriptions, vertexInputAttributeDescriptions, 
	// 		{ componentDescriptorSetLayout, textureDescriptorSetLayout }, 
	// 		fragmentposition[renderpass].pipelineParameters, 
	// 		renderpass, 0, 
	// 		fragmentposition[renderpass].pipelines, fragmentposition[renderpass].pipelineLayout);

	// }

	/* ------ VR MASK ------ */
	{
		vrmask[renderpass] = RasterPipelineResources();

		std::string ResourcePath = Options::GetResourcePath();
		auto vertShaderCode = readFile(ResourcePath + std::string("/Shaders/GBuffers/VRMask/vert.spv"));

		/* Create shader modules */
		auto vertShaderModule = create_shader_module("vr_mask", vertShaderCode);

		/* Info for shader stages */
		vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
		vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName = "main";

		std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertShaderStageInfo };
		
		/* Account for possibly multiple samples */
		vrmask[renderpass].pipelineParameters.multisampling.sampleShadingEnable = (sampleFlag == vk::SampleCountFlagBits::e1) ? false : true;
		vrmask[renderpass].pipelineParameters.multisampling.rasterizationSamples = sampleFlag;

		vrmask[renderpass].pipelineParameters.depthStencil.depthWriteEnable = true;
		vrmask[renderpass].pipelineParameters.depthStencil.depthCompareOp = depthCompareOpEqual;
		vrmask[renderpass].pipelineParameters.rasterizer.cullMode = vk::CullModeFlagBits::eNone;
		
		create_raster_pipeline(shaderStages, vertexInputBindingDescriptions, vertexInputAttributeDescriptions, 
			{ componentDescriptorSetLayout, textureDescriptorSetLayout }, 
			vrmask[renderpass].pipelineParameters, 
			renderpass, 0, 
			vrmask[renderpass].pipelines, vrmask[renderpass].pipelineLayout);

		// device.destroyShaderModule(vertShaderModule);
	}

	if (!vulkan->is_ray_tracing_enabled()) return;
	auto dldi = vulkan->get_dldi();

	/* RAY TRACING PIPELINES */
	{
		rttest[renderpass] = RaytracingPipelineResources();

		/* RAY GEN SHADERS */
		std::string ResourcePath = Options::GetResourcePath();
		auto raygenShaderCode = readFile(ResourcePath + std::string("/Shaders/RaytracedMaterials/TutorialShaders/rgen.spv"));
		auto raychitShaderCode = readFile(ResourcePath + std::string("/Shaders/RaytracedMaterials/TutorialShaders/rchit.spv"));
		auto raymissShaderCode = readFile(ResourcePath + std::string("/Shaders/RaytracedMaterials/TutorialShaders/rmiss.spv"));
		
		/* Create shader modules */
		auto raygenShaderModule = create_shader_module("ray_tracing_gen", raygenShaderCode);
		auto raychitShaderModule = create_shader_module("ray_tracing_chit", raychitShaderCode);
		auto raymissShaderModule = create_shader_module("ray_tracing_miss", raymissShaderCode);

		/* Info for shader stages */
		vk::PipelineShaderStageCreateInfo raygenShaderStageInfo;
		raygenShaderStageInfo.stage = vk::ShaderStageFlagBits::eRaygenNV;
		raygenShaderStageInfo.module = raygenShaderModule;
		raygenShaderStageInfo.pName = "main"; 

		vk::PipelineShaderStageCreateInfo raymissShaderStageInfo;
		raymissShaderStageInfo.stage = vk::ShaderStageFlagBits::eMissNV;
		raymissShaderStageInfo.module = raymissShaderModule;
		raymissShaderStageInfo.pName = "main"; 

		vk::PipelineShaderStageCreateInfo raychitShaderStageInfo;
		raychitShaderStageInfo.stage = vk::ShaderStageFlagBits::eClosestHitNV;
		raychitShaderStageInfo.module = raychitShaderModule;
		raychitShaderStageInfo.pName = "main"; 

		std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { raygenShaderStageInfo, raymissShaderStageInfo, raychitShaderStageInfo};
		
		vk::PushConstantRange range;
		range.offset = 0;
		range.size = sizeof(PushConsts);
		range.stageFlags = vk::ShaderStageFlagBits::eAll;

		std::vector<vk::DescriptorSetLayout> layouts = { componentDescriptorSetLayout, textureDescriptorSetLayout, 
			positionsDescriptorSetLayout, normalsDescriptorSetLayout, colorsDescriptorSetLayout, texcoordsDescriptorSetLayout, indexDescriptorSetLayout,
			raytracingDescriptorSetLayout };
		vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo;

		pipelineLayoutCreateInfo.setLayoutCount = (uint32_t)layouts.size();
		pipelineLayoutCreateInfo.pSetLayouts = layouts.data();
		pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
		pipelineLayoutCreateInfo.pPushConstantRanges = &range;
		
		rttest[renderpass].pipelineLayout = device.createPipelineLayout(pipelineLayoutCreateInfo);

		std::vector<vk::RayTracingShaderGroupCreateInfoNV> shaderGroups;
		
		// Ray gen group
		vk::RayTracingShaderGroupCreateInfoNV rayGenGroupInfo;
		rayGenGroupInfo.type = vk::RayTracingShaderGroupTypeNV::eGeneral;
		rayGenGroupInfo.generalShader = 0;
		rayGenGroupInfo.closestHitShader = VK_SHADER_UNUSED_NV;
		rayGenGroupInfo.anyHitShader = VK_SHADER_UNUSED_NV;
		rayGenGroupInfo.intersectionShader = VK_SHADER_UNUSED_NV;
		shaderGroups.push_back(rayGenGroupInfo);
		
		// Miss group
		vk::RayTracingShaderGroupCreateInfoNV rayMissGroupInfo;
		rayMissGroupInfo.type = vk::RayTracingShaderGroupTypeNV::eGeneral;
		rayMissGroupInfo.generalShader = 1;
		rayMissGroupInfo.closestHitShader = VK_SHADER_UNUSED_NV;
		rayMissGroupInfo.anyHitShader = VK_SHADER_UNUSED_NV;
		rayMissGroupInfo.intersectionShader = VK_SHADER_UNUSED_NV;
		shaderGroups.push_back(rayMissGroupInfo);
		
		// Intersection group
		vk::RayTracingShaderGroupCreateInfoNV rayCHitGroupInfo; //VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_NV
		rayCHitGroupInfo.type = vk::RayTracingShaderGroupTypeNV::eTrianglesHitGroup;
		rayCHitGroupInfo.generalShader = VK_SHADER_UNUSED_NV;
		rayCHitGroupInfo.closestHitShader = 2;
		rayCHitGroupInfo.anyHitShader = VK_SHADER_UNUSED_NV;
		rayCHitGroupInfo.intersectionShader = VK_SHADER_UNUSED_NV;
		shaderGroups.push_back(rayCHitGroupInfo);

		vk::RayTracingPipelineCreateInfoNV rayPipelineInfo;
		rayPipelineInfo.stageCount = (uint32_t) shaderStages.size();
		rayPipelineInfo.pStages = shaderStages.data();
		rayPipelineInfo.groupCount = (uint32_t) shaderGroups.size();
		rayPipelineInfo.pGroups = shaderGroups.data();
		rayPipelineInfo.maxRecursionDepth = 5;
		rayPipelineInfo.layout = rttest[renderpass].pipelineLayout;
		rayPipelineInfo.basePipelineHandle = vk::Pipeline();
		rayPipelineInfo.basePipelineIndex = 0;

		rttest[renderpass].pipeline = device.createRayTracingPipelinesNV(vk::PipelineCache(), 
			{rayPipelineInfo}, nullptr, dldi)[0];

		// device.destroyShaderModule(raygenShaderModule);
	}

	setup_raytracing_shader_binding_table(renderpass);
}

void RenderSystem::setup_raytracing_shader_binding_table(vk::RenderPass renderpass)
{
	auto vulkan = Libraries::Vulkan::Get();
	if (!vulkan->is_initialized())
		throw std::runtime_error("Error: Vulkan not initialized");
	
	if (!vulkan->is_ray_tracing_enabled())
		throw std::runtime_error("Error: Vulkan raytracing is not enabled. ");

	auto device = vulkan->get_device();
	auto dldi = vulkan->get_dldi();

	auto rayTracingProps = vulkan->get_physical_device_ray_tracing_properties();


	/* Currently only works with rttest */

	
	const uint32_t groupNum = 3; // 1 group is listed in pGroupNumbers in VkRayTracingPipelineCreateInfoNV
	const uint32_t shaderBindingTableSize = rayTracingProps.shaderGroupHandleSize * groupNum;

	/* Create binding table buffer */
	vk::BufferCreateInfo bufferInfo;
	bufferInfo.size = shaderBindingTableSize;
	bufferInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;
	bufferInfo.sharingMode = vk::SharingMode::eExclusive;
	rttest[renderpass].shaderBindingTable = device.createBuffer(bufferInfo);

	/* Create memory for binding table */
	vk::MemoryRequirements memRequirements = device.getBufferMemoryRequirements(rttest[renderpass].shaderBindingTable);
	vk::MemoryAllocateInfo allocInfo;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = vulkan->find_memory_type(memRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible);
	rttest[renderpass].shaderBindingTableMemory = device.allocateMemory(allocInfo);

	/* Bind buffer to memeory */
	device.bindBufferMemory(rttest[renderpass].shaderBindingTable, rttest[renderpass].shaderBindingTableMemory, 0);

	/* Map the binding table, then fill with shader group handles */
	void* mappedMemory = device.mapMemory(rttest[renderpass].shaderBindingTableMemory, 0, shaderBindingTableSize, vk::MemoryMapFlags());
	device.getRayTracingShaderGroupHandlesNV(rttest[renderpass].pipeline, 0, groupNum, shaderBindingTableSize, mappedMemory, dldi);
	device.unmapMemory(rttest[renderpass].shaderBindingTableMemory);
}

void RenderSystem::create_descriptor_set_layouts()
{
	/* Descriptor set layouts are standardized across shaders for optimized runtime binding */

	auto vulkan = Libraries::Vulkan::Get();
	auto device = vulkan->get_device();

	/* SSBO descriptor bindings */

	// Entity SSBO
	vk::DescriptorSetLayoutBinding eboLayoutBinding;
	eboLayoutBinding.binding = 0;
	eboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
	eboLayoutBinding.descriptorCount = 1;
	eboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
	eboLayoutBinding.pImmutableSamplers = nullptr; // Optional

	// Transform SSBO
	vk::DescriptorSetLayoutBinding tboLayoutBinding;
	tboLayoutBinding.binding = 1;
	tboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
	tboLayoutBinding.descriptorCount = 1;
	tboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
	tboLayoutBinding.pImmutableSamplers = nullptr; // Optional

	// Camera SSBO
	vk::DescriptorSetLayoutBinding cboLayoutBinding;
	cboLayoutBinding.binding = 2;
	cboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
	cboLayoutBinding.descriptorCount = 1;
	cboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
	cboLayoutBinding.pImmutableSamplers = nullptr; // Optional

	// Material SSBO
	vk::DescriptorSetLayoutBinding mboLayoutBinding;
	mboLayoutBinding.binding = 3;
	mboLayoutBinding.descriptorCount = 1;
	mboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
	mboLayoutBinding.pImmutableSamplers = nullptr;
	mboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;

	// Light SSBO
	vk::DescriptorSetLayoutBinding lboLayoutBinding;
	lboLayoutBinding.binding = 4;
	lboLayoutBinding.descriptorCount = 1;
	lboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
	lboLayoutBinding.pImmutableSamplers = nullptr;
	lboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;

	// Mesh SSBO
	vk::DescriptorSetLayoutBinding meshssboLayoutBinding;
	meshssboLayoutBinding.binding = 5;
	meshssboLayoutBinding.descriptorCount = 1;
	meshssboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
	meshssboLayoutBinding.pImmutableSamplers = nullptr;
	meshssboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;

	// Light Entity SSBO
	vk::DescriptorSetLayoutBinding lidboLayoutBinding;
	lidboLayoutBinding.binding = 6;
	lidboLayoutBinding.descriptorCount = 1;
	lidboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
	lidboLayoutBinding.pImmutableSamplers = nullptr;
	lidboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;

	std::array<vk::DescriptorSetLayoutBinding, 7> SSBObindings = { eboLayoutBinding, tboLayoutBinding, cboLayoutBinding, mboLayoutBinding, lboLayoutBinding, meshssboLayoutBinding, lidboLayoutBinding};
	vk::DescriptorSetLayoutCreateInfo SSBOLayoutInfo;
	SSBOLayoutInfo.bindingCount = (uint32_t)SSBObindings.size();
	SSBOLayoutInfo.pBindings = SSBObindings.data();
	
	/* Texture descriptor bindings */
	
	// Texture struct
	vk::DescriptorSetLayoutBinding txboLayoutBinding;
	txboLayoutBinding.descriptorCount = 1;
	txboLayoutBinding.binding = 0;
	txboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
	txboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
	txboLayoutBinding.pImmutableSamplers = 0;

	// Texture samplers
	vk::DescriptorSetLayoutBinding samplerBinding;
	samplerBinding.descriptorCount = MAX_SAMPLERS;
	samplerBinding.binding = 1;
	samplerBinding.descriptorType = vk::DescriptorType::eSampler;
	samplerBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
	samplerBinding.pImmutableSamplers = 0;

	// 2D Textures
	vk::DescriptorSetLayoutBinding texture2DsBinding;
	texture2DsBinding.descriptorCount = MAX_TEXTURES;
	texture2DsBinding.binding = 2;
	texture2DsBinding.descriptorType = vk::DescriptorType::eSampledImage;
	texture2DsBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
	texture2DsBinding.pImmutableSamplers = 0;

	// Texture Cubes
	vk::DescriptorSetLayoutBinding textureCubesBinding;
	textureCubesBinding.descriptorCount = MAX_TEXTURES;
	textureCubesBinding.binding = 3;
	textureCubesBinding.descriptorType = vk::DescriptorType::eSampledImage;
	textureCubesBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
	textureCubesBinding.pImmutableSamplers = 0;

	// 3D Textures
	vk::DescriptorSetLayoutBinding texture3DsBinding;
	texture3DsBinding.descriptorCount = MAX_TEXTURES;
	texture3DsBinding.binding = 4;
	texture3DsBinding.descriptorType = vk::DescriptorType::eSampledImage;
	texture3DsBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
	texture3DsBinding.pImmutableSamplers = 0;

	std::array<vk::DescriptorSetLayoutBinding, 5> textureBindings = {txboLayoutBinding, samplerBinding, texture2DsBinding, textureCubesBinding, texture3DsBinding };
	vk::DescriptorSetLayoutCreateInfo textureLayoutInfo;
	textureLayoutInfo.bindingCount = (uint32_t)textureBindings.size();
	textureLayoutInfo.pBindings = textureBindings.data();

	// Create the layouts
	componentDescriptorSetLayout = device.createDescriptorSetLayout(SSBOLayoutInfo);
	textureDescriptorSetLayout = device.createDescriptorSetLayout(textureLayoutInfo);

	/* Vertex descriptors (mainly for ray tracing access) */
	vk::DescriptorBindingFlagsEXT bindingFlag = vk::DescriptorBindingFlagBitsEXT::eVariableDescriptorCount;
	vk::DescriptorSetLayoutBindingFlagsCreateInfoEXT bindingFlags;
    bindingFlags.pBindingFlags = &bindingFlag;
    bindingFlags.bindingCount = 1;

    vk::DescriptorSetLayoutBinding positionBinding;
    positionBinding.binding = 0;
    positionBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
    positionBinding.descriptorCount = MAX_MESHES;
    positionBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;

	vk::DescriptorSetLayoutBinding normalBinding;
    normalBinding.binding = 0;
    normalBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
    normalBinding.descriptorCount = MAX_MESHES;
    normalBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;

	vk::DescriptorSetLayoutBinding colorBinding;
    colorBinding.binding = 0;
    colorBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
    colorBinding.descriptorCount = MAX_MESHES;
    colorBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;

	vk::DescriptorSetLayoutBinding texcoordBinding;
    texcoordBinding.binding = 0;
    texcoordBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
    texcoordBinding.descriptorCount = MAX_MESHES;
    texcoordBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;

	vk::DescriptorSetLayoutBinding indexBinding;
    indexBinding.binding = 0;
    indexBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
    indexBinding.descriptorCount = MAX_MESHES;
    indexBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
	
	// std::array<vk::DescriptorSetLayoutBinding, 5> vertexBindings = {positionBinding, normalBinding, colorBinding, texcoordBinding, indexBinding };
	if (vulkan->is_ray_tracing_enabled()) {
		vk::DescriptorSetLayoutCreateInfo positionLayoutInfo;
		positionLayoutInfo.bindingCount = 1;
		positionLayoutInfo.pBindings = &positionBinding;
		positionLayoutInfo.pNext = &bindingFlags;
		positionsDescriptorSetLayout = device.createDescriptorSetLayout(positionLayoutInfo);

		vk::DescriptorSetLayoutCreateInfo normalLayoutInfo;
		normalLayoutInfo.bindingCount = 1;
		normalLayoutInfo.pBindings = &normalBinding;
		normalLayoutInfo.pNext = &bindingFlags;
		normalsDescriptorSetLayout = device.createDescriptorSetLayout(normalLayoutInfo);

		vk::DescriptorSetLayoutCreateInfo colorLayoutInfo;
		colorLayoutInfo.bindingCount = 1;
		colorLayoutInfo.pBindings = &colorBinding;
		colorLayoutInfo.pNext = &bindingFlags;
		colorsDescriptorSetLayout = device.createDescriptorSetLayout(colorLayoutInfo);

		vk::DescriptorSetLayoutCreateInfo texcoordLayoutInfo;
		texcoordLayoutInfo.bindingCount = 1;
		texcoordLayoutInfo.pBindings = &texcoordBinding;
		texcoordLayoutInfo.pNext = &bindingFlags;
		texcoordsDescriptorSetLayout = device.createDescriptorSetLayout(texcoordLayoutInfo);

		vk::DescriptorSetLayoutCreateInfo indexLayoutInfo;
		indexLayoutInfo.bindingCount = 1;
		indexLayoutInfo.pBindings = &indexBinding;
		indexLayoutInfo.pNext = &bindingFlags;
		indexDescriptorSetLayout = device.createDescriptorSetLayout(indexLayoutInfo);

		vk::DescriptorSetLayoutBinding accelerationStructureLayoutBinding;
		accelerationStructureLayoutBinding.binding = 0;
		accelerationStructureLayoutBinding.descriptorType = vk::DescriptorType::eAccelerationStructureNV;
		accelerationStructureLayoutBinding.descriptorCount = 1;
		accelerationStructureLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
		accelerationStructureLayoutBinding.pImmutableSamplers = nullptr;

		vk::DescriptorSetLayoutBinding outputImageLayoutBinding;
		outputImageLayoutBinding.binding = 1;
		outputImageLayoutBinding.descriptorType = vk::DescriptorType::eStorageImage;
		outputImageLayoutBinding.descriptorCount = 1;
		outputImageLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
		outputImageLayoutBinding.pImmutableSamplers = nullptr;

		vk::DescriptorSetLayoutBinding gbufferImageLayoutBinding;
		gbufferImageLayoutBinding.binding = 2;
		gbufferImageLayoutBinding.descriptorType = vk::DescriptorType::eStorageImage;
		gbufferImageLayoutBinding.descriptorCount = MAX_G_BUFFERS;
		gbufferImageLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
		gbufferImageLayoutBinding.pImmutableSamplers = nullptr;

		std::vector<vk::DescriptorSetLayoutBinding> bindings({ accelerationStructureLayoutBinding, 
			outputImageLayoutBinding, gbufferImageLayoutBinding});

		vk::DescriptorSetLayoutCreateInfo layoutInfo;
		layoutInfo.bindingCount = (uint32_t)(bindings.size());
		layoutInfo.pBindings = bindings.data();

		raytracingDescriptorSetLayout = device.createDescriptorSetLayout(layoutInfo);
	}
}

void RenderSystem::create_descriptor_pools()
{
	/* Since the descriptor layout is consistent across shaders, the descriptor
		pool can be shared. */

	auto vulkan = Libraries::Vulkan::Get();
	auto device = vulkan->get_device();

	/* SSBO Descriptor Pool Info */
	std::array<vk::DescriptorPoolSize, 6> SSBOPoolSizes = {};
	
	// Entity SSBO
	SSBOPoolSizes[0].type = vk::DescriptorType::eStorageBuffer;
	SSBOPoolSizes[0].descriptorCount = MAX_MATERIALS;
	
	// Transform SSBO
	SSBOPoolSizes[1].type = vk::DescriptorType::eStorageBuffer;
	SSBOPoolSizes[1].descriptorCount = MAX_MATERIALS;
	
	// Camera SSBO
	SSBOPoolSizes[2].type = vk::DescriptorType::eStorageBuffer;
	SSBOPoolSizes[2].descriptorCount = MAX_MATERIALS;
	
	// Material SSBO
	SSBOPoolSizes[3].type = vk::DescriptorType::eStorageBuffer;
	SSBOPoolSizes[3].descriptorCount = MAX_MATERIALS;
	
	// Light SSBO
	SSBOPoolSizes[4].type = vk::DescriptorType::eStorageBuffer;
	SSBOPoolSizes[4].descriptorCount = MAX_MATERIALS;

	// Mesh SSBO
	SSBOPoolSizes[5].type = vk::DescriptorType::eStorageBuffer;
	SSBOPoolSizes[5].descriptorCount = MAX_MESHES;

	vk::DescriptorPoolCreateInfo SSBOPoolInfo;
	SSBOPoolInfo.poolSizeCount = (uint32_t)SSBOPoolSizes.size();
	SSBOPoolInfo.pPoolSizes = SSBOPoolSizes.data();
	SSBOPoolInfo.maxSets = MAX_MATERIALS;
	SSBOPoolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

	/* Texture Descriptor Pool Info */
	std::array<vk::DescriptorPoolSize, 5> texturePoolSizes = {};
	
	// TextureSSBO
	texturePoolSizes[0].type = vk::DescriptorType::eStorageBuffer;
	texturePoolSizes[0].descriptorCount = MAX_MATERIALS;

	// Sampler
	texturePoolSizes[1].type = vk::DescriptorType::eSampler;
	texturePoolSizes[1].descriptorCount = MAX_MATERIALS;
	
	// 2D Texture array
	texturePoolSizes[2].type = vk::DescriptorType::eSampledImage;
	texturePoolSizes[2].descriptorCount = MAX_MATERIALS;

	// Texture Cube array
	texturePoolSizes[3].type = vk::DescriptorType::eSampledImage;
	texturePoolSizes[3].descriptorCount = MAX_MATERIALS;

	// 3D Texture array
	texturePoolSizes[4].type = vk::DescriptorType::eSampledImage;
	texturePoolSizes[4].descriptorCount = MAX_MATERIALS;
	
	vk::DescriptorPoolCreateInfo texturePoolInfo;
	texturePoolInfo.poolSizeCount = (uint32_t)texturePoolSizes.size();
	texturePoolInfo.pPoolSizes = texturePoolSizes.data();
	texturePoolInfo.maxSets = MAX_MATERIALS;
	texturePoolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

	/* Vertex Descriptor Pool Info */

	// PositionSSBO
	vk::DescriptorPoolSize positionsPoolSize;
	positionsPoolSize.type = vk::DescriptorType::eStorageBuffer;
	positionsPoolSize.descriptorCount = MAX_MESHES;

	// NormalSSBO
	vk::DescriptorPoolSize normalsPoolSize;
	normalsPoolSize.type = vk::DescriptorType::eStorageBuffer;
	normalsPoolSize.descriptorCount = MAX_MESHES;
	
	// ColorSSBO
	vk::DescriptorPoolSize colorsPoolSize;
	colorsPoolSize.type = vk::DescriptorType::eStorageBuffer;
	colorsPoolSize.descriptorCount = MAX_MESHES;

	// TexCoordSSBO
	vk::DescriptorPoolSize texcoordsPoolSize;
	texcoordsPoolSize.type = vk::DescriptorType::eStorageBuffer;
	texcoordsPoolSize.descriptorCount = MAX_MESHES;

	// IndexSSBO
	vk::DescriptorPoolSize indexPoolSize;
	indexPoolSize.type = vk::DescriptorType::eStorageBuffer;
	indexPoolSize.descriptorCount = MAX_MESHES;

	vk::DescriptorPoolCreateInfo positionsPoolInfo;
	positionsPoolInfo.poolSizeCount = 1;
	positionsPoolInfo.pPoolSizes = &positionsPoolSize;
	positionsPoolInfo.maxSets = MAX_MESHES;
	positionsPoolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

	vk::DescriptorPoolCreateInfo normalsPoolInfo;
	normalsPoolInfo.poolSizeCount = 1;
	normalsPoolInfo.pPoolSizes = &normalsPoolSize;
	normalsPoolInfo.maxSets = MAX_MESHES;
	normalsPoolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
	
	vk::DescriptorPoolCreateInfo colorsPoolInfo;
	colorsPoolInfo.poolSizeCount = 1;
	colorsPoolInfo.pPoolSizes = &colorsPoolSize;
	colorsPoolInfo.maxSets = MAX_MESHES;
	colorsPoolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

	vk::DescriptorPoolCreateInfo texcoordsPoolInfo;
	texcoordsPoolInfo.poolSizeCount = 1;
	texcoordsPoolInfo.pPoolSizes = &texcoordsPoolSize;
	texcoordsPoolInfo.maxSets = MAX_MESHES;
	texcoordsPoolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

	vk::DescriptorPoolCreateInfo indexPoolInfo;
	indexPoolInfo.poolSizeCount = 1;
	indexPoolInfo.pPoolSizes = &indexPoolSize;
	indexPoolInfo.maxSets = MAX_MESHES;
	indexPoolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
	
	/* Raytrace Descriptor Pool Info */
	std::array<vk::DescriptorPoolSize, 3> raytracingPoolSizes = {};
	
	// Acceleration Structure
	raytracingPoolSizes[0].type = vk::DescriptorType::eAccelerationStructureNV;
	raytracingPoolSizes[0].descriptorCount = 1;

	// Textures
	raytracingPoolSizes[1].type = vk::DescriptorType::eStorageImage;
	raytracingPoolSizes[1].descriptorCount = 1;

	// G Buffers
	raytracingPoolSizes[2].type = vk::DescriptorType::eStorageImage;
	raytracingPoolSizes[2].descriptorCount = MAX_G_BUFFERS;
	
	vk::DescriptorPoolCreateInfo raytracingPoolInfo;
	raytracingPoolInfo.poolSizeCount = (uint32_t)raytracingPoolSizes.size();
	raytracingPoolInfo.pPoolSizes = raytracingPoolSizes.data();
	raytracingPoolInfo.maxSets = 1;
	raytracingPoolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

	// Create the pools
	componentDescriptorPool = device.createDescriptorPool(SSBOPoolInfo);
	textureDescriptorPool = device.createDescriptorPool(texturePoolInfo);

	if (vulkan->is_ray_tracing_enabled()) {
		positionsDescriptorPool = device.createDescriptorPool(positionsPoolInfo);
		normalsDescriptorPool = device.createDescriptorPool(normalsPoolInfo);
		colorsDescriptorPool = device.createDescriptorPool(colorsPoolInfo);
		texcoordsDescriptorPool = device.createDescriptorPool(texcoordsPoolInfo);
		indexDescriptorPool = device.createDescriptorPool(indexPoolInfo);
		raytracingDescriptorPool = device.createDescriptorPool(raytracingPoolInfo);
	}
}

void RenderSystem::update_raster_descriptor_sets()
{
	if (  (componentDescriptorPool == vk::DescriptorPool()) || (textureDescriptorPool == vk::DescriptorPool())) return;
	auto vulkan = Libraries::Vulkan::Get();
	auto device = vulkan->get_device();
	
	/* ------ Component Descriptor Set  ------ */
	vk::DescriptorSetLayout SSBOLayouts[] = { componentDescriptorSetLayout };
	std::array<vk::WriteDescriptorSet, 7> SSBODescriptorWrites = {};
	if (componentDescriptorSet == vk::DescriptorSet())
	{
		vk::DescriptorSetAllocateInfo allocInfo;
		allocInfo.descriptorPool = componentDescriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = SSBOLayouts;
		componentDescriptorSet = device.allocateDescriptorSets(allocInfo)[0];
	}

	// Entity SSBO
	vk::DescriptorBufferInfo entityBufferInfo;
	entityBufferInfo.buffer = Entity::GetSSBO();
	entityBufferInfo.offset = 0;
	entityBufferInfo.range = Entity::GetSSBOSize();

	if (entityBufferInfo.buffer == vk::Buffer()) return;

	SSBODescriptorWrites[0].dstSet = componentDescriptorSet;
	SSBODescriptorWrites[0].dstBinding = 0;
	SSBODescriptorWrites[0].dstArrayElement = 0;
	SSBODescriptorWrites[0].descriptorType = vk::DescriptorType::eStorageBuffer;
	SSBODescriptorWrites[0].descriptorCount = 1;
	SSBODescriptorWrites[0].pBufferInfo = &entityBufferInfo;

	// Transform SSBO
	vk::DescriptorBufferInfo transformBufferInfo;
	transformBufferInfo.buffer = Transform::GetSSBO();
	transformBufferInfo.offset = 0;
	transformBufferInfo.range = Transform::GetSSBOSize();

	if (transformBufferInfo.buffer == vk::Buffer()) return;

	SSBODescriptorWrites[1].dstSet = componentDescriptorSet;
	SSBODescriptorWrites[1].dstBinding = 1;
	SSBODescriptorWrites[1].dstArrayElement = 0;
	SSBODescriptorWrites[1].descriptorType = vk::DescriptorType::eStorageBuffer;
	SSBODescriptorWrites[1].descriptorCount = 1;
	SSBODescriptorWrites[1].pBufferInfo = &transformBufferInfo;

	// Camera SSBO
	vk::DescriptorBufferInfo cameraBufferInfo;
	cameraBufferInfo.buffer = Camera::GetSSBO();
	cameraBufferInfo.offset = 0;
	cameraBufferInfo.range = Camera::GetSSBOSize();

	if (cameraBufferInfo.buffer == vk::Buffer()) return;

	SSBODescriptorWrites[2].dstSet = componentDescriptorSet;
	SSBODescriptorWrites[2].dstBinding = 2;
	SSBODescriptorWrites[2].dstArrayElement = 0;
	SSBODescriptorWrites[2].descriptorType = vk::DescriptorType::eStorageBuffer;
	SSBODescriptorWrites[2].descriptorCount = 1;
	SSBODescriptorWrites[2].pBufferInfo = &cameraBufferInfo;

	// Material SSBO
	vk::DescriptorBufferInfo materialBufferInfo;
	materialBufferInfo.buffer = Material::GetSSBO();
	materialBufferInfo.offset = 0;
	materialBufferInfo.range = Material::GetSSBOSize();

	if (materialBufferInfo.buffer == vk::Buffer()) return;

	SSBODescriptorWrites[3].dstSet = componentDescriptorSet;
	SSBODescriptorWrites[3].dstBinding = 3;
	SSBODescriptorWrites[3].dstArrayElement = 0;
	SSBODescriptorWrites[3].descriptorType = vk::DescriptorType::eStorageBuffer;
	SSBODescriptorWrites[3].descriptorCount = 1;
	SSBODescriptorWrites[3].pBufferInfo = &materialBufferInfo;

	// Light SSBO
	vk::DescriptorBufferInfo lightBufferInfo;
	lightBufferInfo.buffer = Light::GetSSBO();
	lightBufferInfo.offset = 0;
	lightBufferInfo.range = Light::GetSSBOSize();

	if (lightBufferInfo.buffer == vk::Buffer()) return;

	SSBODescriptorWrites[4].dstSet = componentDescriptorSet;
	SSBODescriptorWrites[4].dstBinding = 4;
	SSBODescriptorWrites[4].dstArrayElement = 0;
	SSBODescriptorWrites[4].descriptorType = vk::DescriptorType::eStorageBuffer;
	SSBODescriptorWrites[4].descriptorCount = 1;
	SSBODescriptorWrites[4].pBufferInfo = &lightBufferInfo;

	// Mesh SSBO
	vk::DescriptorBufferInfo meshBufferInfo;
	meshBufferInfo.buffer = Mesh::GetSSBO();
	meshBufferInfo.offset = 0;
	meshBufferInfo.range = Mesh::GetSSBOSize();

	SSBODescriptorWrites[5].dstSet = componentDescriptorSet;
	SSBODescriptorWrites[5].dstBinding = 5;
	SSBODescriptorWrites[5].dstArrayElement = 0;
	SSBODescriptorWrites[5].descriptorType = vk::DescriptorType::eStorageBuffer;
	SSBODescriptorWrites[5].descriptorCount = 1;
	SSBODescriptorWrites[5].pBufferInfo = &meshBufferInfo;

	// Light Entities SSBO
	vk::DescriptorBufferInfo lightEntityBufferInfo;
	lightEntityBufferInfo.buffer = Light::GetLightEntitiesSSBO();
	lightEntityBufferInfo.offset = 0;
	lightEntityBufferInfo.range = Light::GetLightEntitiesSSBOSize();

	if (lightEntityBufferInfo.buffer == vk::Buffer()) return;

	SSBODescriptorWrites[6].dstSet = componentDescriptorSet;
	SSBODescriptorWrites[6].dstBinding = 6;
	SSBODescriptorWrites[6].dstArrayElement = 0;
	SSBODescriptorWrites[6].descriptorType = vk::DescriptorType::eStorageBuffer;
	SSBODescriptorWrites[6].descriptorCount = 1;
	SSBODescriptorWrites[6].pBufferInfo = &lightEntityBufferInfo;
	
	device.updateDescriptorSets((uint32_t)SSBODescriptorWrites.size(), SSBODescriptorWrites.data(), 0, nullptr);
	
	/* ------ Texture Descriptor Set  ------ */
	vk::DescriptorSetLayout textureLayouts[] = { textureDescriptorSetLayout };
	std::array<vk::WriteDescriptorSet, 5> textureDescriptorWrites = {};
	
	if (textureDescriptorSet == vk::DescriptorSet())
	{
		vk::DescriptorSetAllocateInfo allocInfo;
		allocInfo.descriptorPool = textureDescriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = textureLayouts;

		textureDescriptorSet = device.allocateDescriptorSets(allocInfo)[0];
	}

	auto texture2DLayouts = Texture::GetLayouts(vk::ImageViewType::e2D);
	auto texture2DViews = Texture::GetImageViews(vk::ImageViewType::e2D);
	auto textureCubeLayouts = Texture::GetLayouts(vk::ImageViewType::eCube);
	auto textureCubeViews = Texture::GetImageViews(vk::ImageViewType::eCube);
	auto texture3DLayouts = Texture::GetLayouts(vk::ImageViewType::e3D);
	auto texture3DViews = Texture::GetImageViews(vk::ImageViewType::e3D);
	auto samplers = Texture::GetSamplers();

	if (texture3DLayouts.size() <= 0) return;
	if (texture2DLayouts.size() <= 0) return;
	if (textureCubeLayouts.size() <= 0) return;

	// Texture SSBO
	vk::DescriptorBufferInfo textureBufferInfo;
	textureBufferInfo.buffer = Texture::GetSSBO();
	textureBufferInfo.offset = 0;
	textureBufferInfo.range = Texture::GetSSBOSize();

	if (textureBufferInfo.buffer == vk::Buffer()) return;

	textureDescriptorWrites[0].dstSet = textureDescriptorSet;
	textureDescriptorWrites[0].dstBinding = 0;
	textureDescriptorWrites[0].dstArrayElement = 0;
	textureDescriptorWrites[0].descriptorType = vk::DescriptorType::eStorageBuffer;
	textureDescriptorWrites[0].descriptorCount = 1;
	textureDescriptorWrites[0].pBufferInfo = &textureBufferInfo;

	// Samplers
	vk::DescriptorImageInfo samplerDescriptorInfos[MAX_SAMPLERS];
	for (int i = 0; i < MAX_SAMPLERS; ++i) 
	{
		samplerDescriptorInfos[i].sampler = samplers[i];
	}

	textureDescriptorWrites[1].dstSet = textureDescriptorSet;
	textureDescriptorWrites[1].dstBinding = 1;
	textureDescriptorWrites[1].dstArrayElement = 0;
	textureDescriptorWrites[1].descriptorType = vk::DescriptorType::eSampler;
	textureDescriptorWrites[1].descriptorCount = MAX_SAMPLERS;
	textureDescriptorWrites[1].pImageInfo = samplerDescriptorInfos;

	// 2D Textures
	vk::DescriptorImageInfo texture2DDescriptorInfos[MAX_TEXTURES];
	for (int i = 0; i < MAX_TEXTURES; ++i) 
	{
		texture2DDescriptorInfos[i].sampler = nullptr;
		texture2DDescriptorInfos[i].imageLayout = texture2DLayouts[i];
		texture2DDescriptorInfos[i].imageView = texture2DViews[i];
	}

	textureDescriptorWrites[2].dstSet = textureDescriptorSet;
	textureDescriptorWrites[2].dstBinding = 2;
	textureDescriptorWrites[2].dstArrayElement = 0;
	textureDescriptorWrites[2].descriptorType = vk::DescriptorType::eSampledImage;
	textureDescriptorWrites[2].descriptorCount = MAX_TEXTURES;
	textureDescriptorWrites[2].pImageInfo = texture2DDescriptorInfos;

	// Texture Cubes
	vk::DescriptorImageInfo textureCubeDescriptorInfos[MAX_TEXTURES];
	for (int i = 0; i < MAX_TEXTURES; ++i) 
	{
		textureCubeDescriptorInfos[i].sampler = nullptr;
		textureCubeDescriptorInfos[i].imageLayout = textureCubeLayouts[i];
		textureCubeDescriptorInfos[i].imageView = textureCubeViews[i];
	}

	textureDescriptorWrites[3].dstSet = textureDescriptorSet;
	textureDescriptorWrites[3].dstBinding = 3;
	textureDescriptorWrites[3].dstArrayElement = 0;
	textureDescriptorWrites[3].descriptorType = vk::DescriptorType::eSampledImage;
	textureDescriptorWrites[3].descriptorCount = MAX_TEXTURES;
	textureDescriptorWrites[3].pImageInfo = textureCubeDescriptorInfos;

	// 3D Textures
	vk::DescriptorImageInfo texture3DDescriptorInfos[MAX_TEXTURES];
	for (int i = 0; i < MAX_TEXTURES; ++i) 
	{
		texture3DDescriptorInfos[i].sampler = nullptr;
		texture3DDescriptorInfos[i].imageLayout = texture3DLayouts[i];
		texture3DDescriptorInfos[i].imageView = texture3DViews[i];
	}

	textureDescriptorWrites[4].dstSet = textureDescriptorSet;
	textureDescriptorWrites[4].dstBinding = 4;
	textureDescriptorWrites[4].dstArrayElement = 0;
	textureDescriptorWrites[4].descriptorType = vk::DescriptorType::eSampledImage;
	textureDescriptorWrites[4].descriptorCount = MAX_TEXTURES;
	textureDescriptorWrites[4].pImageInfo = texture3DDescriptorInfos;
	
	device.updateDescriptorSets((uint32_t)textureDescriptorWrites.size(), textureDescriptorWrites.data(), 0, nullptr);

	if (vulkan->is_ray_tracing_enabled()) {
		/* ------ Vertex Descriptor Set  ------ */
		vk::DescriptorSetLayout positionsLayouts[] = { positionsDescriptorSetLayout };
		vk::DescriptorSetLayout normalsLayouts[] = { normalsDescriptorSetLayout };
		vk::DescriptorSetLayout colorsLayouts[] = { colorsDescriptorSetLayout };
		vk::DescriptorSetLayout texcoordsLayouts[] = { texcoordsDescriptorSetLayout };
		vk::DescriptorSetLayout indexLayouts[] = { indexDescriptorSetLayout };
		std::array<vk::WriteDescriptorSet, 1> positionsDescriptorWrites = {};
		std::array<vk::WriteDescriptorSet, 1> normalsDescriptorWrites = {};
		std::array<vk::WriteDescriptorSet, 1> colorsDescriptorWrites = {};
		std::array<vk::WriteDescriptorSet, 1> texcoordsDescriptorWrites = {};
		std::array<vk::WriteDescriptorSet, 1> indexDescriptorWrites = {};
		
		if (positionsDescriptorSet == vk::DescriptorSet())
		{
			vk::DescriptorSetAllocateInfo allocInfo;
			allocInfo.descriptorPool = positionsDescriptorPool;
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = positionsLayouts;

			positionsDescriptorSet = device.allocateDescriptorSets(allocInfo)[0];
		}

		if (normalsDescriptorSet == vk::DescriptorSet())
		{
			vk::DescriptorSetAllocateInfo allocInfo;
			allocInfo.descriptorPool = normalsDescriptorPool;
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = normalsLayouts;

			normalsDescriptorSet = device.allocateDescriptorSets(allocInfo)[0];
		}

		if (colorsDescriptorSet == vk::DescriptorSet())
		{
			vk::DescriptorSetAllocateInfo allocInfo;
			allocInfo.descriptorPool = colorsDescriptorPool;
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = colorsLayouts;

			colorsDescriptorSet = device.allocateDescriptorSets(allocInfo)[0];
		}

		if (texcoordsDescriptorSet == vk::DescriptorSet())
		{
			vk::DescriptorSetAllocateInfo allocInfo;
			allocInfo.descriptorPool = texcoordsDescriptorPool;
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = texcoordsLayouts;

			texcoordsDescriptorSet = device.allocateDescriptorSets(allocInfo)[0];
		}

		if (indexDescriptorSet == vk::DescriptorSet())
		{
			vk::DescriptorSetAllocateInfo allocInfo;
			allocInfo.descriptorPool = indexDescriptorPool;
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = indexLayouts;

			indexDescriptorSet = device.allocateDescriptorSets(allocInfo)[0];
		}

		auto positionSSBOs = Mesh::GetPositionSSBOs();
		auto positionSSBOSizes = Mesh::GetPositionSSBOSizes();
		auto normalSSBOs = Mesh::GetNormalSSBOs();
		auto normalSSBOSizes = Mesh::GetNormalSSBOSizes();
		auto colorSSBOs = Mesh::GetColorSSBOs();
		auto colorSSBOSizes = Mesh::GetColorSSBOSizes();
		auto texcoordSSBOs = Mesh::GetTexCoordSSBOs();
		auto texcoordSSBOSizes = Mesh::GetTexCoordSSBOSizes();
		auto indexSSBOs = Mesh::GetIndexSSBOs();
		auto indexSSBOSizes = Mesh::GetIndexSSBOSizes();
		
		// Positions SSBO
		vk::DescriptorBufferInfo positionBufferInfos[MAX_MESHES];
		for (int i = 0; i < MAX_MESHES; ++i) 
		{
			if (positionSSBOs[i] == vk::Buffer()) return;
			positionBufferInfos[i].buffer = positionSSBOs[i];
			positionBufferInfos[i].offset = 0;
			positionBufferInfos[i].range = positionSSBOSizes[i];
		}

		positionsDescriptorWrites[0].dstSet = positionsDescriptorSet;
		positionsDescriptorWrites[0].dstBinding = 0;
		positionsDescriptorWrites[0].dstArrayElement = 0;
		positionsDescriptorWrites[0].descriptorType = vk::DescriptorType::eStorageBuffer;
		positionsDescriptorWrites[0].descriptorCount = MAX_MESHES;
		positionsDescriptorWrites[0].pBufferInfo = positionBufferInfos;

		// Normals SSBO
		vk::DescriptorBufferInfo normalBufferInfos[MAX_MESHES];
		for (int i = 0; i < MAX_MESHES; ++i) 
		{
			if (normalSSBOs[i] == vk::Buffer()) return;
			normalBufferInfos[i].buffer = normalSSBOs[i];
			normalBufferInfos[i].offset = 0;
			normalBufferInfos[i].range = normalSSBOSizes[i];
		}

		normalsDescriptorWrites[0].dstSet = normalsDescriptorSet;
		normalsDescriptorWrites[0].dstBinding = 0;
		normalsDescriptorWrites[0].dstArrayElement = 0;
		normalsDescriptorWrites[0].descriptorType = vk::DescriptorType::eStorageBuffer;
		normalsDescriptorWrites[0].descriptorCount = MAX_MESHES;
		normalsDescriptorWrites[0].pBufferInfo = normalBufferInfos;

		// Colors SSBO
		vk::DescriptorBufferInfo colorBufferInfos[MAX_MESHES];
		for (int i = 0; i < MAX_MESHES; ++i) 
		{
			if (colorSSBOs[i] == vk::Buffer()) return;
			colorBufferInfos[i].buffer = colorSSBOs[i];
			colorBufferInfos[i].offset = 0;
			colorBufferInfos[i].range = colorSSBOSizes[i];
		}

		colorsDescriptorWrites[0].dstSet = colorsDescriptorSet;
		colorsDescriptorWrites[0].dstBinding = 0;
		colorsDescriptorWrites[0].dstArrayElement = 0;
		colorsDescriptorWrites[0].descriptorType = vk::DescriptorType::eStorageBuffer;
		colorsDescriptorWrites[0].descriptorCount = MAX_MESHES;
		colorsDescriptorWrites[0].pBufferInfo = colorBufferInfos;

		// TexCoords SSBO
		vk::DescriptorBufferInfo texcoordBufferInfos[MAX_MESHES];
		for (int i = 0; i < MAX_MESHES; ++i) 
		{
			if (texcoordSSBOs[i] == vk::Buffer()) return;
			texcoordBufferInfos[i].buffer = texcoordSSBOs[i];
			texcoordBufferInfos[i].offset = 0;
			texcoordBufferInfos[i].range = texcoordSSBOSizes[i];
		}

		texcoordsDescriptorWrites[0].dstSet = texcoordsDescriptorSet;
		texcoordsDescriptorWrites[0].dstBinding = 0;
		texcoordsDescriptorWrites[0].dstArrayElement = 0;
		texcoordsDescriptorWrites[0].descriptorType = vk::DescriptorType::eStorageBuffer;
		texcoordsDescriptorWrites[0].descriptorCount = MAX_MESHES;
		texcoordsDescriptorWrites[0].pBufferInfo = texcoordBufferInfos;

		// Indices SSBO
		vk::DescriptorBufferInfo indexBufferInfos[MAX_MESHES];
		for (int i = 0; i < MAX_MESHES; ++i) 
		{
			if (indexSSBOs[i] == vk::Buffer()) return;
			indexBufferInfos[i].buffer = indexSSBOs[i];
			indexBufferInfos[i].offset = 0;
			indexBufferInfos[i].range = indexSSBOSizes[i];
		}

		indexDescriptorWrites[0].dstSet = indexDescriptorSet;
		indexDescriptorWrites[0].dstBinding = 0;
		indexDescriptorWrites[0].dstArrayElement = 0;
		indexDescriptorWrites[0].descriptorType = vk::DescriptorType::eStorageBuffer;
		indexDescriptorWrites[0].descriptorCount = MAX_MESHES;
		indexDescriptorWrites[0].pBufferInfo = indexBufferInfos;

		device.updateDescriptorSets((uint32_t)positionsDescriptorWrites.size(), positionsDescriptorWrites.data(), 0, nullptr);
		device.updateDescriptorSets((uint32_t)normalsDescriptorWrites.size(), normalsDescriptorWrites.data(), 0, nullptr);
		device.updateDescriptorSets((uint32_t)colorsDescriptorWrites.size(), colorsDescriptorWrites.data(), 0, nullptr);
		device.updateDescriptorSets((uint32_t)texcoordsDescriptorWrites.size(), texcoordsDescriptorWrites.data(), 0, nullptr);
		device.updateDescriptorSets((uint32_t)indexDescriptorWrites.size(), indexDescriptorWrites.data(), 0, nullptr);
	}
}

void RenderSystem::update_raytracing_descriptor_sets(vk::AccelerationStructureNV &tlas, Entity &camera_entity)
{
	auto vulkan = Libraries::Vulkan::Get();
	auto device = vulkan->get_device();

	if (!vulkan->is_ray_tracing_enabled()) return;
	if (tlas == vk::AccelerationStructureNV()) return;
	if (!camera_entity.is_initialized()) return;
	if (!camera_entity.camera()) return;
	if (!camera_entity.camera()->get_texture()) return;
	
	vk::DescriptorSetLayout raytracingLayouts[] = { raytracingDescriptorSetLayout };

	if (raytracingDescriptorSet == vk::DescriptorSet())
	{
		vk::DescriptorSetAllocateInfo allocInfo;
		allocInfo.descriptorPool = raytracingDescriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = raytracingLayouts;

		raytracingDescriptorSet = device.allocateDescriptorSets(allocInfo)[0];
	}

	std::array<vk::WriteDescriptorSet, 3> raytraceDescriptorWrites = {};

	vk::WriteDescriptorSetAccelerationStructureNV descriptorAccelerationStructureInfo;
	descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
	descriptorAccelerationStructureInfo.pAccelerationStructures = &tlas;

	raytraceDescriptorWrites[0].dstSet = raytracingDescriptorSet;
	raytraceDescriptorWrites[0].pNext = &descriptorAccelerationStructureInfo;
	raytraceDescriptorWrites[0].dstBinding = 0;
	raytraceDescriptorWrites[0].dstArrayElement = 0;
	raytraceDescriptorWrites[0].descriptorCount = 1;
	raytraceDescriptorWrites[0].descriptorType = vk::DescriptorType::eAccelerationStructureNV;

	// Output image 
	vk::DescriptorImageInfo descriptorOutputImageInfo;
	descriptorOutputImageInfo.imageLayout = vk::ImageLayout::eGeneral;
	descriptorOutputImageInfo.imageView = camera_entity.camera()->get_texture()->get_color_image_view();		

	raytraceDescriptorWrites[1].dstSet = raytracingDescriptorSet;
	// raytraceDescriptorWrites[1].pNext = &descriptorAccelerationStructureInfo;
	raytraceDescriptorWrites[1].dstBinding = 1;
	raytraceDescriptorWrites[1].dstArrayElement = 0;
	raytraceDescriptorWrites[1].descriptorCount = 1;
	raytraceDescriptorWrites[1].descriptorType = vk::DescriptorType::eStorageImage;
	raytraceDescriptorWrites[1].pImageInfo = &descriptorOutputImageInfo;

	// G Buffers
	std::vector<vk::DescriptorImageInfo> descriptorGBufferImageInfos(MAX_G_BUFFERS);

	for (uint32_t g_idx = 0; g_idx < MAX_G_BUFFERS; ++g_idx) {
		descriptorGBufferImageInfos[g_idx].imageLayout = vk::ImageLayout::eGeneral;
		descriptorGBufferImageInfos[g_idx].imageView = camera_entity.camera()->get_texture()->get_g_buffer_image_view(g_idx);
	}
	
	raytraceDescriptorWrites[2].dstSet = raytracingDescriptorSet;
	// raytraceDescriptorWrites[2].pNext = &descriptorAccelerationStructureInfo;
	raytraceDescriptorWrites[2].dstBinding = 2;
	raytraceDescriptorWrites[2].dstArrayElement = 0;
	raytraceDescriptorWrites[2].descriptorCount = descriptorGBufferImageInfos.size();
	raytraceDescriptorWrites[2].descriptorType = vk::DescriptorType::eStorageImage;
	raytraceDescriptorWrites[2].pImageInfo = descriptorGBufferImageInfos.data();
	
	device.updateDescriptorSets((uint32_t)raytraceDescriptorWrites.size(), raytraceDescriptorWrites.data(), 0, nullptr);
}

void RenderSystem::create_vertex_input_binding_descriptions() {
	/* Vertex input bindings are consistent across shaders */
	vk::VertexInputBindingDescription pointBindingDescription;
	pointBindingDescription.binding = 0;
	pointBindingDescription.stride = 4 * sizeof(float);
	pointBindingDescription.inputRate = vk::VertexInputRate::eVertex;

	vk::VertexInputBindingDescription colorBindingDescription;
	colorBindingDescription.binding = 1;
	colorBindingDescription.stride = 4 * sizeof(float);
	colorBindingDescription.inputRate = vk::VertexInputRate::eVertex;

	vk::VertexInputBindingDescription normalBindingDescription;
	normalBindingDescription.binding = 2;
	normalBindingDescription.stride = 4 * sizeof(float);
	normalBindingDescription.inputRate = vk::VertexInputRate::eVertex;

	vk::VertexInputBindingDescription texcoordBindingDescription;
	texcoordBindingDescription.binding = 3;
	texcoordBindingDescription.stride = 2 * sizeof(float);
	texcoordBindingDescription.inputRate = vk::VertexInputRate::eVertex;

	vertexInputBindingDescriptions = { pointBindingDescription, colorBindingDescription, normalBindingDescription, texcoordBindingDescription };
}

void RenderSystem::create_vertex_attribute_descriptions() {
	/* Vertex attribute descriptions are consistent across shaders */
	std::vector<vk::VertexInputAttributeDescription> attributeDescriptions(4);

	attributeDescriptions[0].binding = 0;
	attributeDescriptions[0].location = 0;
	attributeDescriptions[0].format = vk::Format::eR32G32B32Sfloat;
	attributeDescriptions[0].offset = 0;

	attributeDescriptions[1].binding = 1;
	attributeDescriptions[1].location = 1;
	attributeDescriptions[1].format = vk::Format::eR32G32B32A32Sfloat;
	attributeDescriptions[1].offset = 0;

	attributeDescriptions[2].binding = 2;
	attributeDescriptions[2].location = 2;
	attributeDescriptions[2].format = vk::Format::eR32G32B32Sfloat;
	attributeDescriptions[2].offset = 0;

	attributeDescriptions[3].binding = 3;
	attributeDescriptions[3].location = 3;
	attributeDescriptions[3].format = vk::Format::eR32G32Sfloat;
	attributeDescriptions[3].offset = 0;

	vertexInputAttributeDescriptions = attributeDescriptions;
}

void RenderSystem::bind_descriptor_sets(vk::CommandBuffer &command_buffer, vk::RenderPass &render_pass) 
{
	std::vector<vk::DescriptorSet> descriptorSets = {componentDescriptorSet, textureDescriptorSet};
	// command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, normalsurface[render_pass].pipelineLayout, 0, descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	// command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, blinn[render_pass].pipelineLayout, 0, descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	// command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, texcoordsurface[render_pass].pipelineLayout, 0, descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pbr[render_pass].pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, skybox[render_pass].pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	// command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, depth[render_pass].pipelineLayout, 0, descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, volume[render_pass].pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
}

void RenderSystem::bind_raytracing_descriptor_sets(vk::CommandBuffer &command_buffer, vk::RenderPass &render_pass)
{
	std::vector<vk::DescriptorSet> descriptorSets = {componentDescriptorSet, textureDescriptorSet,  positionsDescriptorSet, normalsDescriptorSet, colorsDescriptorSet, texcoordsDescriptorSet, indexDescriptorSet, raytracingDescriptorSet};
	command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingNV, rttest[render_pass].pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
}

void RenderSystem::draw_entity(
	vk::CommandBuffer &command_buffer, 
	vk::RenderPass &render_pass, 
	Entity &entity, 
	PushConsts &push_constants, 
	RenderMode rendermode_override, 
	PipelineType pipeline_type_override, 
	bool render_bounding_box_override)
{
	auto pipeline_type = pipeline_type_override;

	/* Need a mesh to render. */
	auto m = entity.get_mesh();
	if (!m) return;

	bool show_bounding_box = m->should_show_bounding_box() | render_bounding_box_override;
	push_constants.show_bounding_box = show_bounding_box;

	/* Need a transform to render. */
	auto transform = entity.get_transform();
	if (!transform) return;

	if (show_bounding_box) {
	    m = Mesh::Get("BoundingBox");
	}

	/* Need a material to render. */
	auto material = entity.get_material();
	if (!material) return;

	auto rendermode = (rendermode_override == RenderMode::RENDER_MODE_NONE) ? material->renderMode : rendermode_override;

	if (rendermode == RENDER_MODE_HIDDEN) return;

	// if (rendermode == RENDER_MODE_NORMAL) {
	// 	command_buffer.pushConstants(normalsurface[render_pass].pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
		
	// 	if ((currentlyBoundRenderMode != RENDER_MODE_NORMAL) || (currentRenderpass != render_pass) || (currentlyBoundPipelineType != pipeline_type)) {
	// 		command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, normalsurface[render_pass].pipelines[pipeline_type]);
	// 		currentlyBoundRenderMode = RENDER_MODE_NORMAL;
	// 		currentRenderpass = render_pass;
	// 	}
	// }
	// else if (rendermode == RENDER_MODE_BLINN) {
	// 	command_buffer.pushConstants(blinn[render_pass].pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
	// 	if ((currentlyBoundRenderMode != RENDER_MODE_BLINN) || (currentRenderpass != render_pass) || (currentlyBoundPipelineType != pipeline_type)) {
	// 		command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, blinn[render_pass].pipelines[pipeline_type]);
	// 		currentlyBoundRenderMode = RENDER_MODE_BLINN;
	// 		currentRenderpass = render_pass;
	// 	}
	// }
	// else if (rendermode == RENDER_MODE_TEXCOORD) {
	// 	command_buffer.pushConstants(texcoordsurface[render_pass].pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
	// 	if ((currentlyBoundRenderMode != RENDER_MODE_TEXCOORD) || (currentRenderpass != render_pass) || (currentlyBoundPipelineType != pipeline_type)) {
	// 		command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, texcoordsurface[render_pass].pipelines[pipeline_type]);
	// 		currentlyBoundRenderMode = RENDER_MODE_TEXCOORD;
	// 		currentRenderpass = render_pass;
	// 	}
	// }
	/* else */ if (rendermode == RENDER_MODE_PBR) {
		command_buffer.pushConstants(pbr[render_pass].pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
		if ((currentlyBoundRenderMode != RENDER_MODE_PBR) || (currentRenderpass != render_pass) || (currentlyBoundPipelineType != pipeline_type)) {
			command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pbr[render_pass].pipelines[pipeline_type]);
			currentlyBoundRenderMode = RENDER_MODE_PBR;
			currentRenderpass = render_pass;
		}
	}
	// else if (rendermode == RENDER_MODE_DEPTH) {
	// 	command_buffer.pushConstants(depth[render_pass].pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
	// 	if ((currentlyBoundRenderMode != RENDER_MODE_DEPTH) || (currentRenderpass != render_pass) || (currentlyBoundPipelineType != pipeline_type)) {
	// 		command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, depth[render_pass].pipelines[pipeline_type]);
	// 		currentlyBoundRenderMode = RENDER_MODE_DEPTH;
	// 		currentRenderpass = render_pass;
	// 	}
	// }
	else if (rendermode == RENDER_MODE_SKYBOX) {
		command_buffer.pushConstants(skybox[render_pass].pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
		if ((currentlyBoundRenderMode != RENDER_MODE_SKYBOX) || (currentRenderpass != render_pass) || (currentlyBoundPipelineType != pipeline_type)) {
			command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, skybox[render_pass].pipelines[pipeline_type]);
			currentlyBoundRenderMode = RENDER_MODE_SKYBOX;
			currentRenderpass = render_pass;
		}
	}
	else if (rendermode == RENDER_MODE_FRAGMENTDEPTH) {
		command_buffer.pushConstants(fragmentdepth[render_pass].pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
		if ((currentlyBoundRenderMode != RENDER_MODE_FRAGMENTDEPTH) || (currentRenderpass != render_pass) || (currentlyBoundPipelineType != pipeline_type)) {
			command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, fragmentdepth[render_pass].pipelines[pipeline_type]);
			currentlyBoundRenderMode = RENDER_MODE_FRAGMENTDEPTH;
			currentRenderpass = render_pass;
		}
	}
	// else if (rendermode == RENDER_MODE_FRAGMENTPOSITION) {
	// 	command_buffer.pushConstants(fragmentposition[render_pass].pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
	// 	if ((currentlyBoundRenderMode != RENDER_MODE_FRAGMENTPOSITION) || (currentRenderpass != render_pass) || (currentlyBoundPipelineType != pipeline_type)) {
	// 		command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, fragmentposition[render_pass].pipelines[pipeline_type]);
	// 		currentlyBoundRenderMode = RENDER_MODE_FRAGMENTPOSITION;
	// 		currentRenderpass = render_pass;
	// 	}
	// }
	else if (rendermode == RENDER_MODE_SHADOWMAP) {
		command_buffer.pushConstants(shadowmap[render_pass].pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
		if ((currentlyBoundRenderMode != RENDER_MODE_SHADOWMAP) || (currentRenderpass != render_pass) || (currentlyBoundPipelineType != pipeline_type)) {
			command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, shadowmap[render_pass].pipelines[pipeline_type]);
			currentlyBoundRenderMode = RENDER_MODE_SHADOWMAP;
			currentRenderpass = render_pass;
		}
	}
	else if (rendermode == RENDER_MODE_VRMASK) {
		command_buffer.pushConstants(vrmask[render_pass].pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
		if ((currentlyBoundRenderMode != RENDER_MODE_VRMASK) || (currentRenderpass != render_pass) || (currentlyBoundPipelineType != pipeline_type)) {
			command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, vrmask[render_pass].pipelines[pipeline_type]);
			currentlyBoundRenderMode = RENDER_MODE_VRMASK;
			currentRenderpass = render_pass;
		}
	}
	else if (rendermode == RENDER_MODE_VOLUME)
	{
		command_buffer.pushConstants(volume[render_pass].pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
		if ((currentlyBoundRenderMode != RENDER_MODE_VOLUME) || (currentRenderpass != render_pass) || (currentlyBoundPipelineType != pipeline_type)) {
			command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, volume[render_pass].pipelines[pipeline_type]);
			currentlyBoundRenderMode = RENDER_MODE_VOLUME;
			currentRenderpass = render_pass;
		}
	}
	else {
		command_buffer.pushConstants(pbr[render_pass].pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
		if ((currentlyBoundRenderMode != RENDER_MODE_PBR) || (currentRenderpass != render_pass) || (currentlyBoundPipelineType != pipeline_type)) {
			command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pbr[render_pass].pipelines[pipeline_type]);
			currentlyBoundRenderMode = RENDER_MODE_PBR;
			currentRenderpass = render_pass;
		}
	}

	
	currentlyBoundPipelineType = pipeline_type;
	
	command_buffer.bindVertexBuffers(0, {m->get_point_buffer(), m->get_color_buffer(), m->get_normal_buffer(), m->get_texcoord_buffer()}, {0,0,0,0});
	command_buffer.bindIndexBuffer(m->get_triangle_index_buffer(), 0, vk::IndexType::eUint32);
	command_buffer.drawIndexed(m->get_total_triangle_indices(), 1, 0, 0, 0);
}

void RenderSystem::trace_rays(
	vk::CommandBuffer &command_buffer, 
	vk::RenderPass &render_pass, 
	PushConsts &push_constants,
	Texture &texture)
{
	auto vulkan = Libraries::Vulkan::Get();
    auto device = vulkan->get_device();
    auto dldi = vulkan->get_dldi();

	auto rayTracingProps = vulkan->get_physical_device_ray_tracing_properties();

	command_buffer.pushConstants(rttest[render_pass].pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
	command_buffer.bindPipeline(vk::PipelineBindPoint::eRayTracingNV, rttest[render_pass].pipeline);

	/* Need to make the camera's associated texture writable. Perhaps write 
        to a separate "ray tracing" texture?
        Depth buffer access would be handy as well...
    */

   // Here's how the shader binding table looks like in this tutorial:
    // |[ raygen shader ]|
    // |                 |
    // | 0               | 1
    command_buffer.traceRaysNV(
    //     raygenShaderBindingTableBuffer, offset
		rttest[render_pass].shaderBindingTable, 0,
    //     missShaderBindingTableBuffer, offset, stride
		rttest[render_pass].shaderBindingTable, 1 * rayTracingProps.shaderGroupHandleSize, rayTracingProps.shaderGroupHandleSize,
    //     hitShaderBindingTableBuffer, offset, stride
		rttest[render_pass].shaderBindingTable, 2 * rayTracingProps.shaderGroupHandleSize, rayTracingProps.shaderGroupHandleSize,
    //     callableShaderBindingTableBuffer, offset, stride
		vk::Buffer(), 0, 0,
        // width, height, depth, 
		texture.get_width(), texture.get_height(), 1,
        dldi
    );
	
    // vkCmdTraceRaysNVX(commandBuffer,
    //     _shaderBindingTable.Buffer, 0,
    //     _shaderBindingTable.Buffer, 0, _raytracingProperties.shaderHeaderSize,
    //     _shaderBindingTable.Buffer, 0, _raytracingProperties.shaderHeaderSize,
    //     _actualWindowWidth, _actualWindowHeight);
		
}

void RenderSystem::reset_bound_material()
{
	currentRenderpass = vk::RenderPass();
	currentlyBoundRenderMode = RENDER_MODE_NONE;
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
void RenderSystem::enable_ray_tracing(bool enable) {this->ray_tracing_enabled = enable;};

} // namespace Systems
