
#pragma once

#include "Pluto/Tools/System.hxx"
#include "Pluto/Libraries/GLFW/GLFW.hxx"

namespace Systems 
{
    class EventSystem : public System {
        public:
            static EventSystem* Get();
            bool initialize();
            bool start();
            bool stop();
            bool should_close();
            bool create_window(string key, uint32_t width = 512, uint32_t height = 512, bool floating = true, bool resizable = true, bool decorated = true, bool create_vulkan_resources = true);
            bool destroy_window(string key);
            bool resize_window(string key, uint32_t width, uint32_t height);
            bool set_window_pos(string key, uint32_t x, uint32_t y);
            bool set_window_visibility(string key, bool visible);
            void use_openvr(bool useOpenVR);

        private:
            bool close = true;
            bool useOpenVR = false;
            EventSystem();
            ~EventSystem();

            struct Command {
                std::function<void()> function;
                std::shared_ptr<std::promise<void>> promise;
            };

            static std::condition_variable cv;
            static std::mutex qMutex;
            static std::vector<Command> commandQueue;

            std::future<void> enqueueCommand(std::function<void()> function);
    };   
}
