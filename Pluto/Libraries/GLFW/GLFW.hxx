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

#include "Pluto/Tools/Singleton.hxx"
#include "Pluto/Libraries/Vulkan/Vulkan.hxx"
#include "Pluto/Texture/Texture.hxx"
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
        bool resize_window(std::string key, uint32_t width, uint32_t height);
        bool set_window_pos(std::string key, uint32_t x, uint32_t y);
        bool destroy_window(string key);
        std::vector<std::string> get_window_keys();
        bool poll_events();
        bool wait_events();
        bool does_window_exist(string key);
        bool post_empty_event();
        bool should_close();
        vk::SwapchainKHR get_swapchain(std::string key);
        vk::SurfaceKHR create_vulkan_surface(const Libraries::Vulkan *vulkan, std::string key);
        bool create_vulkan_swapchain(std::string key);
        std::shared_ptr<Texture> get_texture(std::string key, uint32_t index);
        std::string get_key_from_ptr(GLFWwindow* ptr);
        void set_swapchain_out_of_date(std::string key);
        bool is_swapchain_out_of_date(std::string key);
        
        private:
        GLFW();
        ~GLFW();    

        struct Window {
            GLFWwindow* ptr;
            vk::SurfaceKHR surface;
            vk::SurfaceCapabilitiesKHR surfaceCapabilities;
            vk::SurfaceFormatKHR surfaceFormat;
            vk::PresentModeKHR presentMode;
            vk::Extent2D surfaceExtent;
            vk::SwapchainKHR swapchain;
            std::vector<vk::Image> swapchainColorImages;
            std::vector<std::shared_ptr<Texture>> textures; 
            bool swapchain_out_of_date;
        };

        static unordered_map<string, Window> &Windows();
    };
}
