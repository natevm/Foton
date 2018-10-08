#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>

#include <thread>
#include <iostream>
#include <assert.h>
#include <chrono>
#include <future>
#include <vector>
#include <unordered_map>

#include "VinylEngine/BaseClasses/Singleton.hxx"
#include "VinylEngine/Libraries/Vulkan/Vulkan.hxx"
#include  <vulkan/vulkan.hpp>

namespace Libraries {
    using namespace std;
    class GLFW : public Singleton
    {
    public:
        static GLFW* Get();

        bool initialized = false;
        bool initialize();
        bool create_window(string key);
        bool destroy_window(string key);
        std::vector<std::string> get_window_keys();
        bool poll_events();
        bool wait_events();
        bool does_window_exist(string key);
        bool post_empty_event();
        bool should_close();
        vk::SurfaceKHR create_vulkan_surface(const Libraries::Vulkan *vulkan, std::string key);
        bool create_vulkan_resources(const Libraries::Vulkan *vulkan, std::string key);
        bool acquire_images(const Libraries::Vulkan *vulkan);
        bool submit_cmd_buffers(const Libraries::Vulkan *vulkan);

        bool test_clear_images(const Libraries::Vulkan *vulkan, float r, float g, float);
        

        private:
        GLFW();
        ~GLFW();    

        struct Window {
            GLFWwindow* ptr;
            vk::SurfaceKHR surface;
            vk::SurfaceCapabilitiesKHR surfaceCapabilities;
            vk::SurfaceFormatKHR surfaceFormat;
            vk::Format depthFormat;
            vk::PresentModeKHR presentMode;
            vk::Extent2D surfaceExtent;
            vk::SwapchainKHR swapchain;
            std::vector<vk::Image> swapchainColorImages;	
            std::vector<vk::Image> swapchainDepthImages;	
            std::vector<vk::DeviceMemory> swapchainDepthImageMemory;
	        std::vector<vk::ImageView> swapchainColorImageViews;
	        std::vector<vk::ImageView> swapchainDepthImageViews;
	        std::vector<vk::Framebuffer> swapchainFramebuffers;
            std::vector<vk::CommandBuffer> drawCmdBuffers;
            std::vector<vk::Fence> drawCmdBufferFences;
            vk::Semaphore imageAvailableSemaphore;
            vk::Semaphore renderCompleteSemaphore;
            vk::Semaphore presentCompleteSemaphore;
            vk::RenderPass renderPass;
            uint32_t imageIndex;
        };

        static unordered_map<string, Window> &Windows();
    };
}
