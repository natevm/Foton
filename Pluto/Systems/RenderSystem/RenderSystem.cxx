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

// #ifndef _WIN32
// #include <unistd.h>
// #else
// #include <windows.h>

// #define sleep(n)    Sleep(n)
// #endif
using namespace Libraries;

namespace Systems
{
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

    push_constants.gamma = 2.2f;
    push_constants.exposure = 2.0f;
    push_constants.environment_id = -1;
    push_constants.diffuse_environment_id = -1;
    push_constants.specular_environment_id = -1;

    initialized = true;
    return true;
}

void RenderSystem::record_render_commands()
{
    auto glfw = GLFW::Get();

    /* We need some windows in order to render. */
    auto keys = glfw->get_window_keys();
    if (keys.size() == 0) return;

#if BUILD_OPENVR
    if (using_openvr) {
        auto entity_id = Entity::GetEntityForVR();
        
        /* If there's an entity connected to VR */
        if (entity_id != -1) {
            auto entity = Entity::Get(entity_id);
            auto cam_id = entity->get_camera();

            /* If that entity has a camera */
            if (cam_id != -1) {
                Camera* current_camera = Camera::Get(cam_id);

                /* Wait get poses. Move the camera to where the headset is. */
                auto ovr = OpenVR::Get();
                ovr->wait_get_poses();
                current_camera->set_view(ovr->get_left_view_matrix(), 0);
                current_camera->set_custom_projection(ovr->get_left_projection_matrix(.1f), .1f, 0);
                current_camera->set_view(ovr->get_right_view_matrix(), 1);
                current_camera->set_custom_projection(ovr->get_right_projection_matrix(.1f), .1f, 1);
            }
        }
    }
#endif


    /* Upload SSBO data */
    Material::UploadSSBO();
    Transform::UploadSSBO();
    Light::UploadSSBO();
    Camera::UploadSSBO();
    Entity::UploadSSBO();
    Texture::UploadSSBO();
    Material::UpdateRasterDescriptorSets();
    Material::UpdateRaytracingDescriptorSets();

    Texture* brdf = nullptr;
    try {
        brdf = Texture::Get("BRDF");
    } catch (...) {}
    if (!brdf) return;
    auto brdf_id = brdf->get_id();
    push_constants.brdf_lut_id = brdf_id;
    push_constants.time = (float) glfwGetTime();

    auto entities = Entity::GetFront();

    /* Get light list */
    std::vector<int32_t> light_entity_ids(MAX_LIGHTS, -1);
    int32_t light_count = 0;
    for (uint32_t i = 0; i < Entity::GetCount(); ++i)
    {
        if (entities[i].is_initialized() && (entities[i].get_light() != -1))
        {
            light_entity_ids[light_count] = i;
            light_count++;
        }
        if (light_count == MAX_LIGHTS)
            break;
    }
    memcpy(push_constants.light_entity_ids, light_entity_ids.data(), sizeof(push_constants.light_entity_ids)/*sizeof(int32_t) * light_entity_ids.size()*/);


    /* Render all cameras */
    auto cameras = Camera::GetFront();
    for (uint32_t entity_id = 0; entity_id < Entity::GetCount(); ++entity_id) {
        /* Entity must be initialized */
        if (!entities[entity_id].is_initialized()) continue;

        /* Entity needs a camera */
        auto cam_id = entities[entity_id].get_camera();
        if (cam_id < 0) continue;

        /* Camera must allow recording. */
        if (!cameras[cam_id].allows_recording()) continue;

        /* Camera needs a texture */
        Texture * texture = cameras[cam_id].get_texture();
        if (!texture) continue;

        auto command_buffer = cameras[cam_id].get_command_buffer();
        vk::CommandBufferBeginInfo beginInfo;
        // beginInfo.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse;
        beginInfo.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse;
        command_buffer.begin(beginInfo);

        /* If we're the client, we recieve color data from "stream_frames". Only render a scene if not the client. */
        if (!Options::IsClient())
        {
            for(uint32_t rp_idx = 0; rp_idx < cameras[cam_id].get_num_renderpasses(); rp_idx++) {
                /* Get the renderpass for the current camera */
                vk::RenderPass rp = cameras[cam_id].get_renderpass(rp_idx);

                /* Bind all descriptor sets to that renderpass.
                    Note that we're using a single bind. The same descriptors are shared across pipelines. */
                Material::BindDescriptorSets(command_buffer, rp);

                cameras[cam_id].begin_renderpass(command_buffer, rp_idx);
                for (uint32_t i = 0; i < Entity::GetCount(); ++i)
                {
                    if (entities[rp_idx].is_initialized())
                    {
                        // Push constants
                        push_constants.target_id = i;
                        push_constants.camera_id = entity_id;
                        push_constants.viewIndex = rp_idx;
                        Material::DrawEntity(command_buffer, rp, entities[i], push_constants);
                    }
                }
                
                /* Draw volumes last */
                for (uint32_t i = 0; i < Entity::GetCount(); ++i)
                {
                    if (entities[i].is_initialized())
                    {
                        // Push constants
                        push_constants.target_id = i;
                        push_constants.camera_id = entity_id;
                        push_constants.viewIndex = rp_idx;
                        Material::DrawVolume(command_buffer, rp, entities[i], push_constants);
                    }
                }

                cameras[cam_id].end_renderpass(command_buffer, rp_idx);
            }
        }

        /* See if we should blit to a GLFW window. */
        auto connected_window_key = entities[entity_id].get_connected_window();
        if (connected_window_key.size() > 0)
        {
            /* It's possible the connected window was destroyed. Make sure it still exists... */
            if (glfw->does_window_exist(connected_window_key)) {
                
                /* Window needs a swapchain */
                auto swapchain = glfw->get_swapchain(connected_window_key);
                if (!swapchain) {
                    command_buffer.end();
                    continue;
                }

                /* Need to be able to get swapchain/texture by key...*/
                auto swapchain_texture = glfw->get_texture(connected_window_key);

                /* Record blit to swapchain */
                if (swapchain_texture && swapchain_texture->is_initialized()) {
                    texture->record_blit_to(command_buffer, swapchain_texture, 0);
                }
            }
        }

        /* Record blit to OpenVR eyes. */
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
    auto glfw = GLFW::Get();
    std::vector<vk::CommandBuffer> commands;

    auto entities = Entity::GetFront();
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

        auto command_buffer = cameras[cam_id].get_command_buffer();
        commands.push_back(command_buffer);
    }

    auto submitPipelineStages = vk::PipelineStageFlags();
    submitPipelineStages = vk::PipelineStageFlagBits::eColorAttachmentOutput;

    std::vector<vk::Semaphore> waitSemaphores = glfw->get_image_available_semaphores(currentFrame);
    std::vector<vk::PipelineStageFlags> waitDstStageMask;
    std::vector<vk::Semaphore> signalSemaphores;

    if (waitSemaphores.size() > 0) {
        signalSemaphores.push_back(renderCompleteSemaphores[currentFrame]);
    }
    for (uint32_t i = 0; i < waitSemaphores.size(); ++i) {
        waitDstStageMask.push_back(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    }

    vulkan->enqueue_graphics_commands(commands, waitSemaphores, waitDstStageMask, signalSemaphores, maincmd_fences[currentFrame], "drawcalls");
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
    device.freeCommandBuffers(vulkan->get_command_pool(2), maincmds);
    
    for (int idx = 0; idx < maincmd_fences.size(); ++idx) {
        device.destroyFence(maincmd_fences[idx]);
    }

    for (int idx = 0; idx < renderCompleteSemaphores.size(); ++idx) {
        device.destroySemaphore(renderCompleteSemaphores[idx]);
    }
    
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
    mainCmdAllocInfo.commandPool = vulkan->get_command_pool(2);
    mainCmdAllocInfo.commandBufferCount = max_frames_in_flight;
    maincmds = device.allocateCommandBuffers(mainCmdAllocInfo);
    
    maincmd_fences.resize(max_frames_in_flight);
    for (uint32_t idx = 0; idx < max_frames_in_flight; ++idx) {
        vk::FenceCreateInfo fenceInfo;
        // fenceInfo.flags |= vk::FenceCreateFlagBits::eSignaled;
        maincmd_fences[idx] = device.createFence(fenceInfo);
    }

    /* Create semaphores to synchronize GPU between renderpasses and presenting. */
    renderCompleteSemaphores.resize(max_frames_in_flight);

    vk::SemaphoreCreateInfo semaphoreInfo;
    for (uint32_t idx = 0; idx < max_frames_in_flight; ++idx) {
        renderCompleteSemaphores[idx] = device.createSemaphore(semaphoreInfo);
    }

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
                auto device = vulkan->get_device();
                device.waitForFences(maincmd_fences[currentFrame], true, 100);
                device.resetFences(maincmd_fences[currentFrame]);

                /* 4. Optional: Wait on render complete. Present a frame. */
                stream_frames();
                present_openvr_frames();
                glfw->present_glfw_frames({renderCompleteSemaphores[currentFrame]});
                vulkan->submit_present_commands();
            }

            glfw->update_swapchains();

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

RenderSystem::RenderSystem() {}
RenderSystem::~RenderSystem() {}


void RenderSystem::set_gamma(float gamma)
{
    this->push_constants.gamma = gamma;
}

void RenderSystem::set_exposure(float exposure)
{
    this->push_constants.exposure = exposure;
}

void RenderSystem::set_environment_map(int32_t id) 
{
    this->push_constants.environment_id = id;
}

void RenderSystem::set_environment_map(Texture *texture) 
{
    this->push_constants.environment_id = texture->get_id();
}

void RenderSystem::clear_environment_map()
{
    this->push_constants.environment_id = -1;
}

void RenderSystem::set_irradiance_map(int32_t id)
{
    this->push_constants.specular_environment_id = id;
}

void RenderSystem::set_irradiance_map(Texture *texture)
{
    this->push_constants.specular_environment_id = texture->get_id();
}

void RenderSystem::clear_irradiance_map()
{
    this->push_constants.specular_environment_id = -1;
}

void RenderSystem::set_diffuse_map(int32_t id)
{
    this->push_constants.diffuse_environment_id = id;
}

void RenderSystem::set_diffuse_map(Texture *texture)
{
    this->push_constants.diffuse_environment_id = texture->get_id();
}

void RenderSystem::clear_diffuse_map()
{
    this->push_constants.diffuse_environment_id = -1;
}

void RenderSystem::set_top_sky_color(glm::vec3 color) 
{
    push_constants.top_sky_color = glm::vec4(color.r, color.g, color.b, 1.0);
}

void RenderSystem::set_bottom_sky_color(glm::vec3 color) 
{
    push_constants.bottom_sky_color = glm::vec4(color.r, color.g, color.b, 1.0);
}

void RenderSystem::set_sky_transition(float transition) 
{
    push_constants.sky_transition = transition;
}


void RenderSystem::use_openvr(bool useOpenVR) {
    this->using_openvr = useOpenVR;
}
} // namespace Systems
