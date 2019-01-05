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

    auto loop = [this](future<void> futureObj) {
        using namespace Libraries;
        auto glfw = GLFW::Get();
        auto vulkan = Vulkan::Get();
        auto lastTime = glfwGetTime();
        auto currentTime = glfwGetTime();
        Texture* swapchain_texture = nullptr;
        Bucket bucket = {};
        vk::SwapchainKHR swapchain;
        uint32_t frame_index;
        uint32_t currentFrame = 0;
        
        std::vector<vk::CommandBuffer> maincmds;
        std::vector<vk::Fence> maincmd_fences;
        vk::Fence acquireFence;
        std::vector<vk::Semaphore> imageAvailableSemaphores;
        std::vector<vk::Semaphore> renderCompleteSemaphores;
        vk::CommandBuffer maincmd;
        vk::Fence main_fence;
        uint32_t MAX_FRAMES_IN_FLIGHT = 3;


        vk::SemaphoreCreateInfo semaphoreInfo;
        // vk::Semaphore imageAvailableSemaphore;
        // vk::Semaphore renderCompleteSemaphore;
        // vk::Semaphore presentCompleteSemaphore;

        std::cout << "Starting RenderSystem, thread id: " << std::this_thread::get_id() << std::endl;

        while (futureObj.wait_for(std::chrono::seconds(0)) != future_status::ready)
        {
            auto currentTime = glfwGetTime();
            if ((currentTime - lastTime) < .008)
                continue;

            std::cout<<"\r time: " << std::to_string(currentTime - lastTime);

            lastTime = currentTime;

            if (!vulkan->is_initialized())
                continue;

            auto device = vulkan->get_device();

            /* Create a main command buffer if one does not already exist */
            if (maincmds.size() == 0)
            {
                vk::CommandBufferAllocateInfo mainCmdAllocInfo;
                mainCmdAllocInfo.level = vk::CommandBufferLevel::ePrimary;
                mainCmdAllocInfo.commandPool = vulkan->get_command_pool(2);
                mainCmdAllocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
                maincmds = device.allocateCommandBuffers(mainCmdAllocInfo);
            }
            if (imageAvailableSemaphores.size() == 0) {
                imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
                renderCompleteSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
                for (uint32_t idx = 0; idx < MAX_FRAMES_IN_FLIGHT; ++idx) {
                    imageAvailableSemaphores[idx] = device.createSemaphore(semaphoreInfo);
                    renderCompleteSemaphores[idx] = device.createSemaphore(semaphoreInfo);
                }
            }

            /* Likewise, create semaphores if they do not already exist */
            if (maincmd_fences.size() == 0)
            {
                maincmd_fences.resize(MAX_FRAMES_IN_FLIGHT);
                for (uint32_t idx = 0; idx < MAX_FRAMES_IN_FLIGHT; ++idx) {
                    vk::FenceCreateInfo fenceInfo;
                    fenceInfo.flags |= vk::FenceCreateFlagBits::eSignaled;
                    maincmd_fences[idx] = device.createFence(fenceInfo);
                }
            }

            if (acquireFence == vk::Fence())
            {
                vk::FenceCreateInfo fenceInfo;
                acquireFence = device.createFence(fenceInfo);
            }

            /* Upload SSBO data */
            Material::UploadSSBO();
            Transform::UploadSSBO();
            Light::UploadSSBO();
            Camera::UploadSSBO();
            Entity::UploadSSBO();
            Material::CreateDescriptorSets();


            /* 1. Optionally aquire swapchain image. Signal image available semaphore. */
            if (glfw->does_window_exist("Window"))
            {
                swapchain = glfw->get_swapchain("Window");
                if (swapchain)
                {
                    frame_index = device.acquireNextImageKHR(swapchain, std::numeric_limits<uint32_t>::max(), imageAvailableSemaphores[currentFrame], acquireFence).value;
                    swapchain_texture = glfw->get_texture("Window", frame_index);
                    
                    device.waitForFences(acquireFence, true, 100000000000);
                    device.resetFences(acquireFence);
                }
            }

            maincmd = maincmds[currentFrame];

            /* 2. Record render commands. */
            vk::CommandBufferBeginInfo beginInfo;
            // beginInfo.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse;
            beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
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

                            Material::BindDescriptorSets(maincmd) ;

                            int32_t camera_id = current_camera->get_id();
                            current_camera->begin_renderpass(maincmd);
                            
                            for (uint32_t i = 0; i < Entity::GetCount(); ++i)
                            {
                                if (entities[i].is_initialized())
                                {
                                    Material::DrawEntity(maincmd, entities[i], id, environment_id, diffuse_id, irradiance_id, gamma, exposure, light_entity_ids);
                                }
                            }

                            Material::DrawSkyBox(maincmd, id, environment_id, gamma, exposure);

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
            submit_info.pWaitSemaphores = &imageAvailableSemaphores[currentFrame];
            submit_info.pWaitDstStageMask = &submitPipelineStages;
            submit_info.commandBufferCount = 1;
            submit_info.pCommandBuffers = &maincmd;
            submit_info.signalSemaphoreCount = (swapchain && swapchain_texture) ? 1 : 0;
            submit_info.pSignalSemaphores = &renderCompleteSemaphores[currentFrame];

            vulkan->enqueue_graphics_commands(submit_info, maincmd_fences[currentFrame]);

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
            // If i don't fence here, triangles seem to spread apart. I think this is an issue with using the SSBO from frame to frame...
            device.waitForFences(maincmd_fences[currentFrame], true, 100000000000);
            device.resetFences(maincmd_fences[currentFrame]);

            /* 5. Optional: Wait on render complete. Present a frame. */
            if (swapchain && swapchain_texture)
            {
                vk::PresentInfoKHR presentInfo;
                presentInfo.swapchainCount = 1;
                presentInfo.pSwapchains = &swapchain;
                presentInfo.pImageIndices = &frame_index;
                presentInfo.pWaitSemaphores = &renderCompleteSemaphores[currentFrame];
                presentInfo.waitSemaphoreCount = 1;

                vulkan->enqueue_present_commands(presentInfo);
                if (!vulkan->submit_present_commands() || glfw->is_swapchain_out_of_date("Window"))
                {
                    /* Conditionally recreate the swapchain resources if out of date. */
                    glfw->create_vulkan_swapchain("Window", true);
                    swapchain = glfw->get_swapchain("Window");
                }
            }

            currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        }

        /* Release vulkan resources */
        auto device = vulkan->get_device();
        if (device != vk::Device())
        {
            if (maincmds.size() != 0)
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

} // namespace Systems