
#pragma once

#include "Pluto/Tools/System.hxx"
#include "Pluto/Libraries/GLFW/GLFW.hxx"

#include <queue>

class Camera;
class Texture;
class Transform;

namespace Systems 
{
    class EventSystem : public System {
        public:
            static EventSystem* Get();
            bool initialize();
            bool start();
            bool stop();
            bool should_close();
            void create_window(string key, uint32_t width = 512, uint32_t height = 512, bool floating = true, bool resizable = true, bool decorated = true, bool create_vulkan_resources = true);
            void destroy_window(string key);
            void resize_window(string key, uint32_t width, uint32_t height);
            void connect_camera_to_window(string key, Camera* camera, uint32_t layer_idx = 0);
            void connect_camera_to_vr(Camera* camera);
            void connect_camera_transform_to_vr(Transform* camera_transform);
            void connect_left_hand_transform_to_vr(Transform* left_hand_transform);
            void connect_right_hand_transform_to_vr(Transform* right_hand_transform);
            void connect_texture_to_window(string key, Texture* texture, uint32_t layer_idx = 0);
            void set_window_pos(string key, uint32_t x, uint32_t y);
            void set_window_visibility(string key, bool visible);
            void toggle_window_fullscreen(string key);
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
            static std::queue<Command> commandQueue;

            std::future<void> enqueueCommand(std::function<void()> function);
    };   
}
