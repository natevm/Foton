#define NOMINMAX
#include <zmq.h>

#include <string>
#include <iostream>
#include <assert.h>

#include "./RenderSystem.hxx"
#include "Pluto/Tools/Colors.hxx"
#include "Pluto/Camera/Camera.hxx"
#include "Pluto/Entity/Entity.hxx"
#include "Pluto/Tools/Options.hxx"
#include "Pluto/Material/Material.hxx"

#include "Pluto/Material/PushConstants.hxx"

#include "Pluto/Libraries/OpenVR/OpenVR.hxx"

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

    initialized = true;
    return true;
}

void RenderSystem::acquire_swapchain_images(
    vk::SwapchainKHR &swapchain, uint32_t &swapchain_index, Texture* &swapchain_texture, vk::Semaphore &semaphore)
{
    auto glfw = GLFW::Get();
    auto vulkan = Vulkan::Get();
    auto device = vulkan->get_device();

    /* Todo: acquire more than one window's swapchain image. */
    if (!glfw->does_window_exist("Window")) return;
    
    /* The current window must have a valid swapchain which we can acquire from. */
    swapchain = glfw->get_swapchain("Window");
    if (!swapchain) return;
    
    /* Acquire a swapchain image, waiting on the acquire fence. 
        I believe this fence is used for handling vsync, but I could be wrong... */

    auto acquireFence = device.createFence(vk::FenceCreateInfo());
    swapchain_index = device.acquireNextImageKHR(swapchain, std::numeric_limits<uint32_t>::max(), semaphore, acquireFence).value;
    swapchain_texture = glfw->get_texture("Window", swapchain_index);
    device.waitForFences(acquireFence, true, 100000000000);
    device.destroyFence(acquireFence);
}

void RenderSystem::record_render_commands()
{
    maincmd = maincmds[currentFrame];

    vk::CommandBufferBeginInfo beginInfo;
    // beginInfo.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse;
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    maincmd.begin(beginInfo);

    /* For now, only render the camera associated with the window named "Window" */
    auto entity_id = Entity::GetEntityFromWindow("Window");
    
    /* Dont render anything if no entity is connected to the window. */
    if (entity_id == -1) return;

    auto entity = Entity::Get(entity_id);
    auto cam_id = entity->get_camera();

    /* Dont render anything if the connected entity does not have a camera component attached. */
    if (cam_id == -1) return;
    Camera* current_camera = Camera::Get(cam_id);

    /* Don't render anything if the camera component doesn't have a color texture. */
    Texture * texture = current_camera->get_texture();
    if (texture == nullptr) return;


    /* If using openvr, wait for poses before rendering. */
    if (using_openvr) {
        auto ovr = OpenVR::Get();
        ovr->wait_get_poses();
        current_camera->set_view(ovr->get_left_view_matrix(), 0);
        current_camera->set_custom_projection(ovr->get_left_projection_matrix(.1f), .1, 0);
        current_camera->set_view(ovr->get_right_view_matrix(), 1);
        current_camera->set_custom_projection(ovr->get_right_projection_matrix(.1f), .1, 1);
    }

    /* Upload SSBO data */
    Material::UploadSSBO();
    Transform::UploadSSBO();
    Light::UploadSSBO();
    Camera::UploadSSBO();
    Entity::UploadSSBO();
    Material::UpdateDescriptorSets();

    /* If we're the client, we recieve color data from "stream_frames". Only render a scene if not the client. */
    if (!Options::IsClient())
    {
        /* Get a pointer to all entitites. */
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

        /* Get the renderpass for the current camera */
        vk::RenderPass rp = current_camera->get_renderpass();

        /* Bind all descriptor sets to that renderpass. 
            Note that we're using a single bind. The same descriptors are shared across pipelines. */
        Material::BindDescriptorSets(maincmd, rp);

        current_camera->begin_renderpass(maincmd);
        for (uint32_t i = 0; i < Entity::GetCount(); ++i)
        {
            if (entities[i].is_initialized())
            {
                Material::DrawEntity(maincmd, rp, entities[i], entity_id, environment_id, diffuse_id, irradiance_id, gamma, exposure, light_entity_ids);
            }
        }
        current_camera->end_renderpass(maincmd);
    }

    /* Record blit to swapchain */
    if (swapchain && swapchain_texture) {
        texture->record_blit_to(maincmd, swapchain_texture, 0);
    }
    
    /* Record blit to OpenVR eyes. */
    if (using_openvr) {
        auto ovr = OpenVR::Get();
        auto left_eye_texture = ovr->get_left_eye_texture();
        auto right_eye_texture = ovr->get_right_eye_texture();
        if (left_eye_texture)  texture->record_blit_to(maincmd, left_eye_texture, 0);
        if (right_eye_texture) texture->record_blit_to(maincmd, right_eye_texture, 1);
    }
    
    /* End this recording. */
    maincmd.end();
}

void RenderSystem::present_glfw_frames()
{
    auto glfw = GLFW::Get();
    auto vulkan = Vulkan::Get();

    /* Require the swapchain and texture be acquired by "acquire_swapchain_image". */
    /* Todo: allow multiple swapchains/swapchain textures... */
    if (!swapchain) return;
    if (!swapchain_texture) return;

    /* Wait for the final renderpass to complete, then present to the given swapchain. */
    vk::PresentInfoKHR presentInfo;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &swapchain_index;
    presentInfo.pWaitSemaphores = &renderCompleteSemaphores[currentFrame];
    presentInfo.waitSemaphoreCount = 1;
    vulkan->enqueue_present_commands(presentInfo);

    /* If our swapchain is out of date at this point, recreate it. */
    if (!vulkan->submit_present_commands() || glfw->is_swapchain_out_of_date("Window"))
    {
        /* Conditionally recreate the swapchain resources if out of date. */
        glfw->create_vulkan_swapchain("Window", true);
        swapchain = glfw->get_swapchain("Window");
    }
}


void RenderSystem::present_openvr_frames()
{
    /* Ignore if openvr isn't in use. */
    if (!using_openvr) return;
    
    /* Submit the left and right eye textures to OpenVR */
    auto ovr = OpenVR::Get();
    ovr->submit_textures();
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

    /* If we're the server, send the frame over UDP */
    if (Options::IsServer())
    {
        std::vector<float> color_data = texture->download_color_data(16, 16, 1, 0);

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
}

void RenderSystem::enqueue_render_commands() {
    auto vulkan = Vulkan::Get();

    auto submitPipelineStages = vk::PipelineStageFlags();
    submitPipelineStages |= vk::PipelineStageFlagBits::eColorAttachmentOutput;

    vk::SubmitInfo submit_info;
    submit_info.waitSemaphoreCount = (swapchain && swapchain_texture) ? 1 : 0;
    submit_info.pWaitSemaphores = &imageAvailableSemaphores[currentFrame];
    submit_info.pWaitDstStageMask = &submitPipelineStages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &maincmd;
    submit_info.signalSemaphoreCount = (swapchain && swapchain_texture) ? 1 : 0;
    submit_info.pSignalSemaphores = &renderCompleteSemaphores[currentFrame];

    vulkan->enqueue_graphics_commands(submit_info, maincmd_fences[currentFrame]);
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

    for (int idx = 0; idx < imageAvailableSemaphores.size(); ++idx) {
        device.destroySemaphore(imageAvailableSemaphores[idx]);
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
        fenceInfo.flags |= vk::FenceCreateFlagBits::eSignaled;
        maincmd_fences[idx] = device.createFence(fenceInfo);
    }

    /* Create semaphores to synchronize GPU between renderpasses and presenting. */
    imageAvailableSemaphores.resize(max_frames_in_flight);
    renderCompleteSemaphores.resize(max_frames_in_flight);

    vk::SemaphoreCreateInfo semaphoreInfo;
    for (uint32_t idx = 0; idx < max_frames_in_flight; ++idx) {
        imageAvailableSemaphores[idx] = device.createSemaphore(semaphoreInfo);
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

        while (futureObj.wait_for(std::chrono::seconds(0)) != future_status::ready)
        {
            /* Regulate the framerate. */
            currentTime = glfwGetTime();
            if ((!using_openvr) && ((currentTime - lastTime) < .008)) continue;
            lastTime = currentTime;

            /* Without vulkan, we cannot render. Wait until vulkan is initialized before rendering.*/
            auto vulkan = Vulkan::Get();
            if (!vulkan->is_initialized()) continue;

            /* 0. Allocate the resources we'll need to render this scene. */
            allocate_vulkan_resources();

            /* 1. Optionally aquire swapchain image. Signal image available semaphore. */
            acquire_swapchain_images(swapchain, swapchain_index, swapchain_texture, imageAvailableSemaphores[currentFrame]);

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
            device.waitForFences(maincmd_fences[currentFrame], true, 100000000000);
            device.resetFences(maincmd_fences[currentFrame]);

            /* 4. Optional: Wait on render complete. Present a frame. */
            stream_frames();
            present_openvr_frames();
            present_glfw_frames();

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
        zmq_close(socket);
        zmq_ctx_destroy(zmq_context);
    }

    running = false;
    return true;
}

RenderSystem::RenderSystem() {}
RenderSystem::~RenderSystem() {}


void RenderSystem::set_gamma(float gamma)
{
    this->gamma = gamma;
}

void RenderSystem::set_exposure(float exposure)
{
    this->exposure = exposure;
}

void RenderSystem::set_environment_map(int32_t id) 
{
    this->environment_id = id;
}

void RenderSystem::set_environment_map(Texture *texture) 
{
    this->environment_id = texture->get_id();
}

void RenderSystem::clear_environment_map()
{
    this->environment_id = -1;
}

void RenderSystem::set_irradiance_map(int32_t id)
{
    this->irradiance_id = id;
}

void RenderSystem::set_irradiance_map(Texture *texture)
{
    this->irradiance_id = texture->get_id();
}

void RenderSystem::clear_irradiance_map()
{
    this->irradiance_id = -1;
}

void RenderSystem::set_diffuse_map(int32_t id)
{
    this->diffuse_id = id;
}

void RenderSystem::set_diffuse_map(Texture *texture)
{
    this->diffuse_id = texture->get_id();
}

void RenderSystem::clear_diffuse_map()
{
    this->diffuse_id = -1;
}

void RenderSystem::use_openvr(bool useOpenVR) {
    this->using_openvr = useOpenVR;
}
} // namespace Systems