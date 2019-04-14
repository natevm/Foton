#include "EventSystem.hxx"

#include "Pluto/Prefabs/Prefabs.hxx"
#if BUILD_OPENVR
#include "Pluto/Libraries/OpenVR/OpenVR.hxx"
#endif
#if BUILD_SPACEMOUSE
#include "Pluto/Libraries/SpaceMouse/SpaceMouse.hxx"
#endif

namespace Systems 
{
    std::condition_variable EventSystem::cv;
    std::mutex EventSystem::qMutex;
    std::queue<EventSystem::Command> EventSystem::commandQueue = {};


    EventSystem* EventSystem::Get() {
        static EventSystem instance;
        return &instance;
    }

    bool EventSystem::initialize() {
        using namespace Libraries;
        if (initialized) return false;
        
        initialized = true;
        close = false;
        return true;
    }

    bool EventSystem::start() {
        if (!initialized) return false;
        if (running) return false;

        // std::cout<<"Starting EventSystem, thread id: " << std::this_thread::get_id()<<std::endl;

        using namespace Libraries;
        
        running = true;

        auto lastTime = glfwGetTime();
       
        while (!close)
        {
            auto glfw = GLFW::Get();

            auto currentTime = glfwGetTime();
            if ((currentTime - lastTime) < .008) continue;
            lastTime = currentTime;

            if (glfw) {
                glfw->poll_events();
                glfw->update_swapchains();

                // glfw->wait_events(); // THIS CALL IS BLOCKING
                {
                    std::lock_guard<std::mutex> lock(qMutex);
                    while (!commandQueue.empty()) {
                        auto item = commandQueue.front();
                        item.function();
                        try {
                            item.promise->set_value();
                        }
                        catch (std::future_error& e) {
                            if (e.code() == std::make_error_condition(std::future_errc::promise_already_satisfied))
                                std::cout << "EventSystem: [promise already satisfied]\n";
                            else
                                std::cout << "EventSystem: [unknown exception]\n";
                        }
                        commandQueue.pop();
                    }
                }
            }
#if BUILD_OPENVR
            if (useOpenVR) {
                auto OpenVR = OpenVR::Get();
                if (OpenVR) {
                    OpenVR->poll_event();
                }
            }
#endif

#if BUILD_SPACEMOUSE
            auto sm = SpaceMouse::Get();
            if (sm->is_initialized())
            {
                sm->poll_event();
            }
#endif
            Prefabs::Update();
        }
        return true;
    }

    std::future<void> EventSystem::enqueueCommand(std::function<void()> function)
    {
        std::lock_guard<std::mutex> lock(qMutex);
        Command c;
        c.function = function;
        c.promise = std::make_shared<std::promise<void>>();
        auto new_future = c.promise->get_future();
        commandQueue.push(c);
        // cv.notify_one();
        return new_future;
    }

    /* These commands can be called from separate threads, but must be run on the event thread. */

    void EventSystem::create_window(string key, uint32_t width, uint32_t height, bool floating, bool resizable, bool decorated, bool create_vulkan_resources) {
        using namespace Libraries;
        auto glfw = GLFW::Get();
        if (glfw->does_window_exist(key))
            throw std::runtime_error( std::string("Error: window already exists, cannot create window"));
        
        auto createWindow = [key, width, height, floating, resizable, decorated, create_vulkan_resources]() {
            using namespace Libraries;
            auto glfw = GLFW::Get();
            glfw->create_window(key, width, height, floating, resizable, decorated);

            // if (create_vulkan_resources) {
                // auto vulkan = Vulkan::Get();
                // glfw->create_vulkan_surface(vulkan, key);
                // glfw->create_vulkan_swapchain(key, false);
            // }
        };

        auto future = enqueueCommand(createWindow);
        future.wait();
    }

    void EventSystem::destroy_window(string key) {
        using namespace Libraries;
        auto glfw = GLFW::Get();
        if (!glfw->does_window_exist(key))
            throw std::runtime_error( std::string("Error: window does not exist, cannot close window"));
        
        auto closeWindow = [key]() {
            using namespace Libraries;
            auto glfw = GLFW::Get();
            glfw->destroy_window(key);
        };

        auto future = enqueueCommand(closeWindow);
        future.wait();
    }

    void EventSystem::resize_window(string key, uint32_t width, uint32_t height) {
        using namespace Libraries;
        auto glfw = GLFW::Get();
        if (!glfw->does_window_exist(key)){
            throw std::runtime_error("Error: Window does not exist");
        }

        auto resizeWindow = [key, width, height] () {
            using namespace Libraries;
            auto glfw = GLFW::Get();
            glfw->resize_window(key, width, height);
        };

        auto future = enqueueCommand(resizeWindow);
        future.wait();
    }

    void EventSystem::connect_camera_to_window(string key, Camera* camera, uint32_t layer_idx)
    {
        /* This function can be called directly. */
        using namespace Libraries;
        auto glfw = GLFW::Get();
        glfw->connect_camera_to_window(key, camera, layer_idx);
    }

    void EventSystem::connect_camera_to_vr(Camera* camera)
    {
        #if BUILD_OPENVR
        /* This function can be called directly. */
        using namespace Libraries;
        auto ovr = OpenVR::Get();
        ovr->connect_camera(camera);
        #else
        throw std::runtime_error("Error: Pluto was not built with OpenVR support enabled!");
        #endif
    }

    void EventSystem::connect_camera_transform_to_vr(Transform* camera_transform)
    {
        #if BUILD_OPENVR
        /* This function can be called directly. */
        using namespace Libraries;
        auto ovr = OpenVR::Get();
        ovr->connect_camera_transform(camera_transform);
        #else
        throw std::runtime_error("Error: Pluto was not built with OpenVR support enabled!");
        #endif
    }

    void EventSystem::connect_left_hand_transform_to_vr(Transform* left_hand_transform)
    {
        #if BUILD_OPENVR
        /* This function can be called directly. */
        using namespace Libraries;
        auto ovr = OpenVR::Get();
        ovr->connect_left_hand_transform(left_hand_transform);
        #else
        throw std::runtime_error("Error: Pluto was not built with OpenVR support enabled!");
        #endif
    }

    void EventSystem::connect_right_hand_transform_to_vr(Transform* right_hand_transform)
    {
        #if BUILD_OPENVR
        /* This function can be called directly. */
        using namespace Libraries;
        auto ovr = OpenVR::Get();
        ovr->connect_right_hand_transform(right_hand_transform);
        #else
        throw std::runtime_error("Error: Pluto was not built with OpenVR support enabled!");
        #endif
    }

    void EventSystem::connect_texture_to_window(string key, Texture* texture, uint32_t layer_idx)
    {
        /* This function can be called directly. */
        using namespace Libraries;
        auto glfw = GLFW::Get();
        glfw->connect_texture_to_window(key, texture, layer_idx);
    }

    void EventSystem::set_window_pos(string key, uint32_t x, uint32_t y) {
        using namespace Libraries;
        auto glfw = GLFW::Get();
        if (!glfw->does_window_exist(key)){
            throw std::runtime_error("Error: Window does not exist");
        }

        auto setWindowPos = [key, x, y] () {
            using namespace Libraries;
            auto glfw = GLFW::Get();
            glfw->set_window_pos(key, x, y);
        };

        auto future = enqueueCommand(setWindowPos);
        future.wait();
    }

    void EventSystem::set_window_visibility(string key, bool visible) {
        using namespace Libraries;
        auto glfw = GLFW::Get();
        if (!glfw->does_window_exist(key)){
            throw std::runtime_error("Error: Window does not exist");
        }

        auto setWindowVisibility = [key, visible] () {
            using namespace Libraries;
            auto glfw = GLFW::Get();
            glfw->set_window_visibility(key, visible);
        };

        auto future = enqueueCommand(setWindowVisibility);
        future.wait();
    }

    void EventSystem::toggle_window_fullscreen(string key)
    {
        using namespace Libraries;
        auto glfw = GLFW::Get();
        if (!glfw->does_window_exist(key)){
            throw std::runtime_error("Error: Window does not exist");
        }
        
        auto setWindowFullscreen = [key] () {
            using namespace Libraries;
            auto glfw = GLFW::Get();
            glfw->toggle_fullscreen(key);
        };

        auto future = enqueueCommand(setWindowFullscreen);
        future.wait();
    }

    bool EventSystem::stop() {
        using namespace Libraries;
        if (!initialized) return false;
        if (!running) return false;

        close = true;
        auto glfw = GLFW::Get();
        if (glfw) glfw->post_empty_event();
        running = false;

        // exitSignal.set_value();

        initialized = false;
        
        return true;
    }

    bool EventSystem::should_close() {
        using namespace Libraries;
        using namespace Systems;
        auto glfw = GLFW::Get();
        if (glfw) glfw->post_empty_event();
        return close;
    }

    void EventSystem::use_openvr(bool useOpenVR) {
        this->useOpenVR = useOpenVR;
    }

    EventSystem::EventSystem() {}
    EventSystem::~EventSystem() {}    
}
