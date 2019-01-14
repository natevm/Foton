#pragma once

#include "Pluto/Tools/System.hxx"
#include "Pluto/Libraries/GLFW/GLFW.hxx"
#include "Pluto/Libraries/Vulkan/Vulkan.hxx"

class Texture;

namespace Systems 
{
    class RenderSystem : public System {
        public:
            static RenderSystem* Get();
            bool initialize();
            bool start();
            bool stop();
            void *socket;
            std::string ip;
            void set_gamma(float gamma);
            void set_exposure(float exposure);

            void set_environment_map(int32_t id);
            void set_environment_map(Texture *texture);
            void clear_environment_map();
            
            void set_irradiance_map(int32_t id);
            void set_irradiance_map(Texture *texture);
            void clear_irradiance_map();

            void set_diffuse_map(int32_t id);
            void set_diffuse_map(Texture *texture);
            void clear_diffuse_map();
            void use_openvr(bool useOpenVR);
        private:

            bool using_openvr = false;
            void *zmq_context;
            float gamma = 1.0;
            float exposure = 1.0;
            int environment_id = -1;
            int diffuse_id = -1;
            int irradiance_id = -1;

            double lastTime, currentTime;

            Texture* swapchain_texture = nullptr;

            struct Bucket
            {
                int x, y, width, height;
                float data[4 * 16 * 16];
            };
            bool vulkan_resources_created = false;
            vk::SwapchainKHR swapchain;
            uint32_t swapchain_index;
            uint32_t currentFrame = 0;
            
            std::vector<vk::CommandBuffer> maincmds;
            std::vector<vk::Fence> maincmd_fences;
            std::vector<vk::Semaphore> imageAvailableSemaphores;
            std::vector<vk::Semaphore> renderCompleteSemaphores;
            vk::CommandBuffer maincmd;
            vk::Fence main_fence;
            uint32_t max_frames_in_flight = 3;

            void acquire_swapchain_images(vk::SwapchainKHR &swapchain, uint32_t &swapchain_index, Texture *&swapchain_texture, vk::Semaphore &semaphore);
            void record_render_commands();
            void enqueue_render_commands();

            void stream_frames();
            void present_openvr_frames();
            void present_glfw_frames();
            void allocate_vulkan_resources();
            void release_vulkan_resources();
            RenderSystem();
            ~RenderSystem();            
    };
}