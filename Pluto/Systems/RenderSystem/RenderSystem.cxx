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

// #ifndef _WIN32
// #include <unistd.h>
// #else
// #include <windows.h>

// #define sleep(n)    Sleep(n)
// #endif

struct Bucket
{
    int x, y, width, height;
    float data[4 * 16 * 16];
};

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

bool RenderSystem::start()
{
    /* Dont start unless initialied. Dont start twice. */
    if (!initialized)
        return false;
    if (running)
        return false;

    auto loop = [](future<void> futureObj) {
        using namespace Libraries;
        auto glfw = GLFW::Get();
        auto vulkan = Vulkan::Get();
        auto lastTime = glfwGetTime();
        auto currentTime = glfwGetTime();
        Texture* swapchain_texture = nullptr;
        Bucket bucket = {};
        vk::SwapchainKHR swapchain;
        uint32_t index;
        vk::CommandBuffer maincmd;
        vk::SemaphoreCreateInfo semaphoreInfo;
        vk::Semaphore imageAvailableSemaphore;
        vk::Semaphore renderCompleteSemaphore;
        vk::Semaphore presentCompleteSemaphore;

        std::cout << "Starting RenderSystem, thread id: " << std::this_thread::get_id() << std::endl;

        while (futureObj.wait_for(std::chrono::seconds(0)) != future_status::ready)
        {
            auto currentTime = glfwGetTime();
            if ((currentTime - lastTime) < .008)
                continue;

            lastTime = currentTime;

            if (!vulkan->is_initialized())
                continue;

            auto device = vulkan->get_device();

            /* Create a main command buffer if one does not already exist */
            if (maincmd == vk::CommandBuffer())
            {
                vk::CommandBufferAllocateInfo mainCmdAllocInfo;
                mainCmdAllocInfo.level = vk::CommandBufferLevel::ePrimary;
                mainCmdAllocInfo.commandPool = vulkan->get_command_pool(2);
                mainCmdAllocInfo.commandBufferCount = 1;
                maincmd = device.allocateCommandBuffers(mainCmdAllocInfo)[0];
            }
            /* Likewise, create semaphores if they do not already exist */
            if (imageAvailableSemaphore == vk::Semaphore())
                imageAvailableSemaphore = device.createSemaphore(semaphoreInfo);
            if (renderCompleteSemaphore == vk::Semaphore())
                renderCompleteSemaphore = device.createSemaphore(semaphoreInfo);
            if (presentCompleteSemaphore == vk::Semaphore())
                presentCompleteSemaphore = device.createSemaphore(semaphoreInfo);

            /* Upload SSBO data */
            Material::UploadSSBO();
            Transform::UploadSSBO();
            Light::UploadSSBO();
            Camera::UploadSSBO();
            Entity::UploadSSBO();

            /* 1. Optionally aquire swapchain image. Signal image available semaphore. */
            if (glfw->does_window_exist("Window"))
            {
                swapchain = glfw->get_swapchain("Window");
                if (swapchain)
                {
                    index = device.acquireNextImageKHR(swapchain, std::numeric_limits<uint32_t>::max(), imageAvailableSemaphore, vk::Fence()).value;
                    swapchain_texture = glfw->get_texture("Window", index);
                }
            }

            /* 2. Record render commands. */
            vk::CommandBufferBeginInfo beginInfo;
            beginInfo.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse;
            maincmd.begin(beginInfo);

            /* Render cameras (one renderpass for now) */
            auto id = Entity::GetEntityFromWindow("Window");
            Camera* current_camera = nullptr;
            if (id != -1)
            {
                auto entity = Entity::Get(id);
                auto cam_id = entity->get_camera();
                if (cam_id != -1)
                {
                    current_camera = Camera::Get(cam_id);
                    if (current_camera)
                    {
                        if (!Options::IsClient())
                        {
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

                            Material::CreateDescriptorSets();
                            Material::BindDescriptorSets(maincmd) ;

                            int32_t camera_id = current_camera->get_id();
                            current_camera->begin_renderpass(maincmd);

                            for (uint32_t i = 0; i < Entity::GetCount(); ++i)
                            {
                                if (entities[i].is_initialized())
                                {
                                    Material::DrawEntity(maincmd, entities[i], id, light_entity_ids);
                                }
                            }

                            current_camera->end_renderpass(maincmd);
                        }

                        /* 3. Optionally blit main camera to swapchain image. */
                        if (current_camera && swapchain && swapchain_texture && current_camera->get_texture())
                        {
                            current_camera->get_texture()->record_blit_to(maincmd, swapchain_texture);
                        }
                    }
                }
            }

            maincmd.end();

            /* 4. Wait on image available. Enqueue graphics commands. Optionally signal render complete semaphore. */
            auto submitPipelineStages = vk::PipelineStageFlags();
            submitPipelineStages |= vk::PipelineStageFlagBits::eColorAttachmentOutput;

            vk::SubmitInfo submit_info;
            submit_info.waitSemaphoreCount = (swapchain && swapchain_texture) ? 1 : 0;
            submit_info.pWaitSemaphores = &imageAvailableSemaphore;
            submit_info.pWaitDstStageMask = &submitPipelineStages;
            submit_info.commandBufferCount = 1;
            submit_info.pCommandBuffers = &maincmd;
            submit_info.signalSemaphoreCount = (swapchain && swapchain_texture) ? 1 : 0;
            submit_info.pSignalSemaphores = &renderCompleteSemaphore;

            vk::FenceCreateInfo fenceInfo;
            vk::Fence fence = device.createFence(fenceInfo);
            vulkan->enqueue_graphics_commands(submit_info, fence);

            /* If doing network rendering... */
            if (Options::IsServer() || Options::IsClient())
            {
                /* Download complete frame, send over UDP. */
                if (Options::IsServer())
                {
                    std::vector<float> color_data = current_camera->get_texture()->download_color_data(16, 16, 1, 0);

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

                /* Receive complete frame over UDP. Upload. */
                if (Options::IsClient())
                {
                    for (int i = 0; i < 100; ++i)
                    {
                        int rc = zmq_recv(RenderSystem::Get()->socket, &bucket, sizeof(Bucket), ZMQ_NOBLOCK);
                    }
                    std::vector<float> color_data(16 * 16 * 4);
                    memcpy(color_data.data(), bucket.data, 16 * 16 * 4 * sizeof(float));
                    current_camera->get_texture()->upload_color_data(16, 16, 1, color_data, 0);
                }
            }

            /* Submit enqueued graphics commands */
            vulkan->submit_graphics_commands();

            // /* Wait for GPU to finish execution */
            device.waitForFences(fence, true, 100000000000);
            device.destroyFence(fence);

            /* 5. Optional: Wait on render complete. Present a frame. */
            if (swapchain && swapchain_texture)
            {
                vk::PresentInfoKHR presentInfo;
                presentInfo.swapchainCount = 1;
                presentInfo.pSwapchains = &swapchain;
                presentInfo.pImageIndices = &index;
                presentInfo.pWaitSemaphores = &renderCompleteSemaphore;
                presentInfo.waitSemaphoreCount = 1;

                vulkan->enqueue_present_commands(presentInfo);
                if (!vulkan->submit_present_commands() || glfw->is_swapchain_out_of_date("Window"))
                {
                    /* Conditionally recreate the swapchain resources if out of date. */
                    glfw->create_vulkan_swapchain("Window", true);
                    swapchain = glfw->get_swapchain("Window");
                }
            }
        }

        /* Release vulkan resources */
        auto device = vulkan->get_device();
        if (device != vk::Device())
        {
            if (maincmd != vk::CommandBuffer())
                device.freeCommandBuffers(vulkan->get_command_pool(2), maincmd);
            if (imageAvailableSemaphore != vk::Semaphore())
                device.destroySemaphore(imageAvailableSemaphore);
            if (renderCompleteSemaphore != vk::Semaphore())
                device.destroySemaphore(renderCompleteSemaphore);
            if (presentCompleteSemaphore != vk::Semaphore())
                device.destroySemaphore(presentCompleteSemaphore);
        }
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

} // namespace Systems