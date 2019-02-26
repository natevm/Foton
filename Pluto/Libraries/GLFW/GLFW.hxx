#pragma once

#define GLFW_INCLUDE_VULKAN
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#include <iostream>

#include <thread>
#include <iostream>
#include <assert.h>
#include <chrono>
#include <future>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <map>
#include <utility>

#include "Pluto/Tools/Singleton.hxx"
#include "Pluto/Libraries/Vulkan/Vulkan.hxx"

class Texture;
class Camera;

// /* For access to HWND handle on windows. */
// #ifdef WIN32
// #ifndef _WINDEF_
// // class HWIND; // Forward or never
// #endif
// #endif

namespace Libraries {
    using namespace std;
    class GLFW : public Singleton
    {
    public:
        static GLFW* Get();

        bool initialize();
        bool create_window(string key, uint32_t width = 512, uint32_t height = 512, bool floating = true, bool resizable = true, bool decorated = true);
        bool resize_window(std::string key, uint32_t width, uint32_t height);
        bool set_window_visibility(std::string key, bool visible);
        bool toggle_fullscreen(std::string key);
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
        Texture* get_texture(std::string key);
        std::string get_key_from_ptr(GLFWwindow* ptr);
        void set_swapchain_out_of_date(std::string key);
        bool is_swapchain_out_of_date(std::string key);
        void update_swapchains();
        bool set_cursor_pos(std::string key, double xpos, double ypos);
        std::vector<double> get_cursor_pos(std::string key);
        bool set_button_data(std::string key, int button, int action, int mods);
        int get_button_action(std::string key, int button);
        int get_button_mods(std::string key, int button);

        bool set_key_data(std::string window_key, int key, int scancode, int action, int mods);
        int get_key_scancode(std::string window_key, int key);
        int get_key_action(std::string window_key, int key);
        int get_key_mods(std::string window_key, int key);
        static int get_key_code(std::string key);
        GLFWwindow* get_ptr(std::string key);
        std::shared_ptr<std::mutex> get_mutex();
        double get_time();
        
        void acquire_swapchain_images(uint32_t current_frame);
        std::vector<vk::Semaphore> get_image_available_semaphores(uint32_t current_frame);
        void present_glfw_frames(std::vector<vk::Semaphore> semaphores);

        void connect_camera_to_window(std::string key, Camera* camera, uint32_t layer_idx = 0);
        void connect_texture_to_window(std::string key, Texture* texture, uint32_t layer_idx = 0);
        std::map<std::string, std::pair<Camera*, uint32_t>> get_window_to_camera_map();
        std::map<std::string, std::pair<Texture*, uint32_t>> get_window_to_texture_map();

        private:
        GLFW();
        ~GLFW();    

        struct Button {
            unsigned char action;
            unsigned char mods;
        };

        struct Key {
            int scancode;
            unsigned char action;
            unsigned char mods;
        };

        // mutex used to guarantee exclusive access to windows
        std::shared_ptr<std::mutex> window_mutex;
        
        struct Window {
            std::vector<vk::Semaphore> imageAvailableSemaphores;
            uint32_t current_image_index;
            GLFWwindow* ptr;
            vk::SurfaceKHR surface;
            vk::SurfaceCapabilitiesKHR surfaceCapabilities;
            vk::SurfaceFormatKHR surfaceFormat;
            vk::PresentModeKHR presentMode;
            vk::Extent2D surfaceExtent;
            vk::SwapchainKHR swapchain;
            std::vector<vk::Image> swapchainColorImages;
            std::vector<Texture*> textures; 
            bool swapchain_out_of_date;
            bool image_acquired = false;
            double xpos;
            double ypos;
            Button buttons[8];
            Key keys[349]; // This is a bit inefficient, but allows lookup by GLFW key

            Camera* connectedCamera = nullptr;
            Texture* connectedTexture = nullptr;
            uint32_t selected_layer = 0;
        };

        static unordered_map<string, Window> &Windows();
    };
}
