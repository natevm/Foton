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

#if BUILD_OPENVR
#include "Pluto/Libraries/OpenVR/OpenVR.hxx"
#endif

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
        if (entities[i].is_initialized() && (entities[i].get_light() != -1))
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
        auto cam_id = entities[entity_id].get_camera();
        if (cam_id < 0) continue;

        /* Camera needs a texture */
        Texture * texture = cameras[cam_id].get_texture();
        if (!texture) continue;

        bool skip = false;

        /* If entity is a shadow camera (TODO: REFACTOR THIS...) */
        if (entities[entity_id].get_light() != -1) {
            /* Skip shadowmaps which shouldn't cast shadows */
            auto light = Light::Get(entities[entity_id].get_light());
            if (!light->should_cast_shadows()) continue;
        }

        /* Begin recording */
        vk::CommandBufferBeginInfo beginInfo;
        if (cameras[cam_id].needs_command_buffers()) cameras[cam_id].create_command_buffers();
        vk::CommandBuffer command_buffer = cameras[cam_id].get_command_buffer();
        
        beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
        command_buffer.begin(beginInfo);

        auto visible_entities = cameras[cam_id].get_visible_entities(entity_id);

        /* Record Z prepasses / Occlusion query  */
        cameras[cam_id].reset_query_pool(command_buffer);

        if ((!skip) && (!Options::IsClient()) && (cameras[cam_id].should_record_depth_prepass())) {
            for(uint32_t rp_idx = 0; rp_idx < cameras[cam_id].get_num_renderpasses(); rp_idx++) {
                Material::ResetBoundMaterial();
                
                /* Get the renderpass for the current camera */
                vk::RenderPass rp = cameras[cam_id].get_depth_prepass(rp_idx);

                /* Bind all descriptor sets to that renderpass.
                    Note that we're using a single bind. The same descriptors are shared across pipelines. */
                Material::BindDescriptorSets(command_buffer, rp);
                

                cameras[cam_id].begin_depth_prepass(command_buffer, rp_idx);
                for (uint32_t i = 0; i < visible_entities.size(); ++i) {
                    auto target_id = visible_entities[i].second->get_id();

                    // if (cameras[cam_id].is_entity_visible(target_id) {
                    /* This is a problem, since a single entity may be drawn multiple times on devices
                    not supporting multiview, (mac). Might need to increase query pool size to MAX_ENTITIES * renderpass count */
                    cameras[cam_id].begin_visibility_query(command_buffer, target_id, i);

                    // Push constants
                    push_constants.target_id = target_id;
                    push_constants.camera_id = entity_id;
                    push_constants.viewIndex = rp_idx;
                    Material::DrawEntity(command_buffer, rp, *visible_entities[i].second, push_constants, RenderMode::FRAGMENTDEPTH);

                    cameras[cam_id].end_visibility_query(command_buffer, target_id, i);
                    // }

                }
                cameras[cam_id].end_depth_prepass(command_buffer, rp_idx);
            }
        }

        /* Record forward renderpasses */
        if ((!skip) && (!Options::IsClient())) {
            /* If we're the client, we recieve color data from "stream_frames". Only render a scene if not the client. */
            for(uint32_t rp_idx = 0; rp_idx < cameras[cam_id].get_num_renderpasses(); rp_idx++) {
                Material::ResetBoundMaterial();

                /* Get the renderpass for the current camera */
                vk::RenderPass rp = cameras[cam_id].get_renderpass(rp_idx);

                /* Bind all descriptor sets to that renderpass.
                    Note that we're using a single bind. The same descriptors are shared across pipelines. */
                Material::BindDescriptorSets(command_buffer, rp);
                
                cameras[cam_id].begin_renderpass(command_buffer, rp_idx);
                for (uint32_t i = 0; i < visible_entities.size(); ++i) {
                    auto target_id = visible_entities[i].second->get_id();

                    if (cameras[cam_id].is_entity_visible(target_id) || (!cameras[cam_id].should_record_depth_prepass())) {
                        push_constants.target_id = target_id;
                        push_constants.camera_id = entity_id;
                        push_constants.viewIndex = rp_idx;
                        Material::DrawEntity(command_buffer, rp, *visible_entities[i].second, push_constants, cameras[cam_id].get_rendermode_override());
                    }
                }
                
                /* Draw transparent objects last */
                for (uint32_t i = 0; i < visible_entities.size(); ++i)
                {
                    auto target_id = visible_entities[i].second->get_id();

                    if (cameras[cam_id].is_entity_visible(target_id) || (!cameras[cam_id].should_record_depth_prepass())) {
                        push_constants.target_id = target_id;
                        push_constants.camera_id = entity_id;
                        push_constants.viewIndex = rp_idx;
                        Material::DrawVolume(command_buffer, rp, *visible_entities[i].second, push_constants, cameras[cam_id].get_rendermode_override());
                    }
                }
                cameras[cam_id].end_renderpass(command_buffer, rp_idx);
            }
        }

        /* Blit to GLFW windows */
        for (auto w2c : window_to_cam) {
            if ((w2c.second.first->get_id() == cam_id)) {
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
        #if BUILD_OPENVR
        if (using_openvr) {
            if (entity_id == Entity::GetEntityForVR()) {
                auto ovr = OpenVR::Get();
                auto left_eye_texture = ovr->get_left_eye_texture();
                auto right_eye_texture = ovr->get_right_eye_texture();
                if (left_eye_texture)  texture->record_blit_to(command_buffer, left_eye_texture, 0);
                if (right_eye_texture) texture->record_blit_to(command_buffer, right_eye_texture, 1);
            }
        }
        #endif

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
    Material::UpdateRaytracingDescriptorSets();
    
    if (update_push_constants() == true) {
        record_cameras();
    }
    
    record_blit_textures();
}

void RenderSystem::update_openvr_transforms()
{
    #if BUILD_OPENVR
    /* Set OpenVR transform data right before rendering */
    if (using_openvr) {
        auto entity_id = Entity::GetEntityForVR();
        
        /* If there's an entity connected to VR */
        if (entity_id != -1) {
            auto entity = Entity::Get(entity_id);
            auto cam_id = entity->get_camera();
            auto left = Transform::Get("VRLeftHand");
            auto right = Transform::Get("VRRightHand");
            auto ovr = OpenVR::Get();

            /* If that entity has a camera */
            if (cam_id != -1) {
                Camera* current_camera = Camera::Get(cam_id);

                /* Wait get poses. Move the camera to where the headset is. */
                ovr->wait_get_poses();
                current_camera->set_view(ovr->get_left_view_matrix(), 0);
                current_camera->set_custom_projection(ovr->get_left_projection_matrix(.1f), .1f, 0);
                current_camera->set_view(ovr->get_right_view_matrix(), 1);
                current_camera->set_custom_projection(ovr->get_right_projection_matrix(.1f), .1f, 1);
            }
            if (left) left->set_transform(ovr->get_left_controller_transform());
            if (right) right->set_transform(ovr->get_right_controller_transform());
        }
    }
    #endif
}

void RenderSystem::present_openvr_frames()
{
    #if BUILD_OPENVR
    /* Ignore if openvr isn't in use. */
    if (!using_openvr) return;

    /* TODO: see if the below checks are required for submitting textures to OpenVR... */

    /* Get the entity connected to the headset */
    auto entity_id = Entity::GetEntityForVR();
    
    /* Dont render anything if no entity is connected. */
    if (entity_id == -1) return;

    auto entity = Entity::Get(entity_id);
    auto cam_id = entity->get_camera();

    /* Dont render anything if the connected entity does not have a camera component attached. */
    if (cam_id == -1) return;
    Camera* current_camera = Camera::Get(cam_id);

    /* Don't render anything if the camera component doesn't have a color texture. */
    Texture * texture = current_camera->get_texture();
    if (texture == nullptr) return;
    
    /* Submit the left and right eye textures to OpenVR */
    auto ovr = OpenVR::Get();
    ovr->submit_textures();
    #endif
}

void RenderSystem::stream_frames()
{
    /* Only stream frames if we're in server/client mode. */
    if (! (Options::IsServer() || Options::IsClient())) return;

    /* For now, only stream the camera associated with the window named "Window" */
    auto entity_id = Entity::GetEntityFromWindow("Window");
    
    /* Dont stream anything if no entity is connected to the window. */
    if (entity_id == -1) return;

    auto entity = Entity::Get(entity_id);
    auto cam_id = entity->get_camera();

    /* Dont stream anything if the connected entity does not have a camera component attached. */
    if (cam_id == -1) return;
    Camera* current_camera = Camera::Get(cam_id);

    /* Don't stream anything if the camera component doesn't have a color texture. */
    Texture * texture = current_camera->get_texture();
    if (texture == nullptr) return;
    
    Bucket bucket = {};

    #if ZMQ_BUILD_DRAFT_API == 1
    /* If we're the server, send the frame over UDP */
    if (Options::IsServer())
    {
        std::vector<float> color_data = texture->download_color_data(16, 16, 1, true);

        bucket.x = 0;
        bucket.y = 0;
        bucket.width = 16;
        bucket.height = 16;
        memcpy(bucket.data, color_data.data(), 16 * 16 * 4 * sizeof(float));

        /* Set message group */
        zmq_msg_t msg;
        const char *text = "Hello";
        int rc = zmq_msg_init_size(&msg, sizeof(Bucket));
        assert(rc == 0);
        memcpy(zmq_msg_data(&msg), &bucket, sizeof(Bucket));
        zmq_msg_set_group(&msg, "PLUTO");
        zmq_msg_send(&msg, RenderSystem::Get()->socket, ZMQ_DONTWAIT);
    }

    /* Else we're the client, so upload the frame. */
    /* TODO: make this work again. */
    else {
        //     for (int i = 0; i < 100; ++i)
        //     {
        //         int rc = zmq_recv(RenderSystem::Get()->socket, &bucket, sizeof(Bucket), ZMQ_NOBLOCK);
        //     }
        //     std::vector<float> color_data(16 * 16 * 4);
        //     memcpy(color_data.data(), bucket.data, 16 * 16 * 4 * sizeof(float));
        //     current_camera->get_texture()->upload_color_data(16, 16, 1, color_data, 0);
    }
    #endif
}

void RenderSystem::enqueue_render_commands() {
    auto vulkan = Vulkan::Get();
    auto device = vulkan->get_device();
    auto glfw = GLFW::Get();

    auto window_to_cam = glfw->get_window_to_camera_map();
    auto window_to_tex = glfw->get_window_to_texture_map();

    auto entities = Entity::GetFront();
    auto cameras = Camera::GetFront();
    
    int32_t min_render_idx = Camera::GetMinRenderOrder();
    int32_t max_render_idx = Camera::GetMaxRenderOrder();

    compute_graph.clear();
    final_fences.clear();
    final_renderpass_semaphores.clear();

    /* Aggregate command sets */
    std::vector<std::shared_ptr<ComputeNode>> last_level;
    std::vector<std::shared_ptr<ComputeNode>> current_level;

    uint32_t level_idx = 0;
    for (int32_t rp_idx = min_render_idx; rp_idx <= max_render_idx; ++rp_idx)
    {
        uint32_t queue_idx = 0;
        bool command_found = false;
        for (uint32_t e_id = 0; e_id < Entity::GetCount(); ++e_id) {

            /* Look for entities with valid command buffers */
            if (!entities[e_id].is_initialized()) continue;
            auto c_id = entities[e_id].get_camera();
            if (c_id < 0) continue;
            Texture * texture = cameras[c_id].get_texture();
            if (!texture) continue;
            if (cameras[c_id].get_render_order() != rp_idx) continue;
            
            /* If the entity is a shadow camera (TODO: REFACTOR THIS...) */
            if (entities[e_id].get_light() != -1){
                /* If that light should cast shadows */
                auto light = Light::Get(entities[e_id].get_light());
                if (!light->should_cast_shadows()) continue;
            }

            /* Make a compute node for this command buffer */
        auto node = std::make_shared<ComputeNode>();
            node->level = level_idx;
            node->queue_idx = queue_idx;
            node->command_buffers.push_back(cameras[c_id].get_command_buffer());

            /* Add any connected windows to this node */
            for (auto w2c : window_to_cam) {
                if ((w2c.second.first->get_id() == c_id)) {
                    auto swapchain = glfw->get_swapchain(w2c.first);
                    if (!swapchain) continue;
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
        node->command_buffers.push_back(main_command_buffer);
        if (main_command_buffer_presenting) {
            for (auto &w2t : window_to_tex) {
                node->connected_windows.push_back(w2t.first);
            }
        }
        main_command_buffer_recorded = false;
    }

    /* Create semaphores and fences for nodes in the graph */
    for (auto &level : compute_graph) {
        vk::SemaphoreCreateInfo semaphoreInfo;
        vk::FenceCreateInfo fenceInfo;

        for (auto &node : level) {

            for (uint32_t i = 0; i < node->children.size(); ++i) {
                /* Create a signal semaphore for the child */
                node->signal_semaphores.push_back(device.createSemaphore(semaphoreInfo));
            }

            /* If the current node is connected to any windows, make signal semaphores for those windows */
            for (auto &window_key : node->connected_windows) {
                auto image_available = glfw->get_image_available_semaphore(window_key, currentFrame);
                if (image_available != vk::Semaphore()) {
                    auto swapchain = glfw->get_swapchain(window_key);
                    if (!swapchain) continue;
                    auto swapchain_texture = glfw->get_texture(window_key);
                    if ( (!swapchain_texture) || (!swapchain_texture->is_initialized())) continue;
                    
                    auto render_complete = device.createSemaphore(semaphoreInfo);
                    node->window_signal_semaphores.push_back(render_complete);
                    final_renderpass_semaphores.push_back(render_complete);
                }
            }

            /* If the node has no children, make a fence */
            if (node->children.size() == 0) {
                node->fence = device.createFence(fenceInfo);
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

        vk::Fence possible_fence = vk::Fence();
        
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
                if (possible_fence != vk::Fence()) throw std::runtime_error("Error, not expecting more than one fence per level!");
                possible_fence = node->fence;
            }

            for (auto &command : node->command_buffers)
                command_buffers.push_back(command);
            
            // For some reason, queues arent actually ran in parallel? Also calling submit many times turns out to be expensive...
            // vulkan->enqueue_graphics_commands(node->command_buffers, wait_semaphores, wait_stage_masks, signal_semaphores, 
            //     node->fence, "draw call lvl " + std::to_string(node->level) + " queue " + std::to_string(node->queue_idx), node->queue_idx);
        }

        std::vector<vk::Semaphore> wait_semaphores;
        std::vector<vk::PipelineStageFlags> wait_stage_masks;

        /* Making hasty assumptions here about wait stage masks. */
        for (auto &semaphore : wait_semaphores_set) {
            wait_semaphores.push_back(semaphore);
            wait_stage_masks.push_back(vk::PipelineStageFlagBits::eColorAttachmentOutput);
        }

        std::vector<vk::Semaphore> signal_semaphores;
        for (auto &semaphore : signal_semaphores_set) {
            signal_semaphores.push_back(semaphore);
        }
        
        vulkan->enqueue_graphics_commands(command_buffers, wait_semaphores, wait_stage_masks, signal_semaphores, 
            possible_fence, "draw call lvl " + std::to_string(level_idx) + " queue " + std::to_string(0), 0);
        
        level_idx++;
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

    vulkan_resources_created = true;
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
                    auto result = device.waitForFences(final_fences, true, 10000000000);
                    if (result != vk::Result::eSuccess) {
                        std::cout<<"Fence timeout in render loop!"<<std::endl;
                    }

                    /* Cleanup fences. */
                    // for (auto &fence : final_fences) device.destroyFence(fence);
                }


                /* 4. Optional: Wait on render complete. Present a frame. */
                stream_frames();
                present_openvr_frames();
                glfw->present_glfw_frames(final_renderpass_semaphores);
                vulkan->submit_present_commands();

                download_visibility_queries();

                for (auto &level : compute_graph) {
                    vk::SemaphoreCreateInfo semaphoreInfo;
                    vk::FenceCreateInfo fenceInfo;

                    for (auto &node : level) {
                        auto device = vulkan->get_device();
                        for (auto signal_semaphore : node->signal_semaphores) device.destroySemaphore(signal_semaphore);
                        node->signal_semaphores.clear();
                        for (auto signal_semaphore : node->window_signal_semaphores) device.destroySemaphore(signal_semaphore);
                        node->window_signal_semaphores.clear();
                        if (node->fence != vk::Fence()) device.destroyFence(node->fence);
                        node->fence = vk::Fence();
                    }
                }
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

    if (Options::IsServer() || Options::IsClient())
    {
        //zmq_close(socket);
        //zmq_ctx_destroy(zmq_context);
    }

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

} // namespace Systems
