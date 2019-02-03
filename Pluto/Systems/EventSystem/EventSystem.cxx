#include "EventSystem.hxx"

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
       
        while (!close)
        {
            auto glfw = GLFW::Get();

            if (glfw) {
                glfw->poll_events();

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
            std::this_thread::sleep_for (std::chrono::milliseconds(10));
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

    bool EventSystem::create_window(string key, uint32_t width, uint32_t height, bool floating, bool resizable, bool decorated, bool create_vulkan_resources) {
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
        return true;
    }

    bool EventSystem::destroy_window(string key) {
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
        return true;
    }

    bool EventSystem::resize_window(string key, uint32_t width, uint32_t height) {
        auto resizeWindow = [key, width, height] () {
            using namespace Libraries;
            auto glfw = GLFW::Get();
            glfw->resize_window(key, width, height);
        };

        auto future = enqueueCommand(resizeWindow);
        future.wait();
        return true;
    }

    bool EventSystem::set_window_pos(string key, uint32_t x, uint32_t y) {
        auto setWindowPos = [key, x, y] () {
            using namespace Libraries;
            auto glfw = GLFW::Get();
            glfw->set_window_pos(key, x, y);
        };

        auto future = enqueueCommand(setWindowPos);
        future.wait();
        return true;
    }

    bool EventSystem::set_window_visibility(string key, bool visible) {
        auto setWindowVisibility = [key, visible] () {
            using namespace Libraries;
            auto glfw = GLFW::Get();
            glfw->set_window_visibility(key, visible);
        };

        auto future = enqueueCommand(setWindowVisibility);
        future.wait();
        return true;
    }


    bool EventSystem::stop() {
        using namespace Libraries;
        if (!initialized) return false;
        if (!running) return false;

        close = true;
        auto glfw = GLFW::Get();
        if (glfw) glfw->post_empty_event();
        running = false;

        exitSignal.set_value();
        
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
