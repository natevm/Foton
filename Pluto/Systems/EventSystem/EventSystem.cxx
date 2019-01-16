#include "EventSystem.hxx"

#include "Pluto/Libraries/OpenVR/OpenVR.hxx"

namespace Systems 
{
    std::condition_variable EventSystem::cv;
    std::mutex EventSystem::qMutex;
    std::vector<EventSystem::Command> EventSystem::commandQueue = {};


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
                std::unique_lock<std::mutex> lock(qMutex);
                if (cv.wait_for(lock, 4ms) == std::cv_status::no_timeout)
                {
                    for (int i = 0; i < commandQueue.size(); ++i) {
                        commandQueue[i].function();
                        commandQueue[i].promise->set_value();
                    }
                    commandQueue.clear();
                }
            }
            if (useOpenVR) {
                auto OpenVR = OpenVR::Get();
                if (OpenVR) {
                    OpenVR->poll_event();
                }
            }
            //close |= (glfw->should_close());
            /* TODO: REGULATE FREQUENCY */
        }
        
        return true;
    }

    std::future<void> EventSystem::enqueueCommand(std::function<void()> function)
    {
        std::lock_guard<std::mutex> lock(qMutex);
        Command c;
        c.function = function;
        c.promise = std::make_shared<std::promise<void>>();
        commandQueue.push_back(c);
        cv.notify_one();
        return c.promise->get_future();
    }

    /* These commands can be called from separate threads, but must be run on the event thread. */

    bool EventSystem::create_window(string key, uint32_t width, uint32_t height, bool floating, bool resizable, bool decorated) {
        auto createWindow = [key, width, height, floating, resizable, decorated]() {
            using namespace Libraries;
            auto glfw = GLFW::Get();
            glfw->create_window(key, width, height, floating, resizable, decorated);
        };

        auto future = enqueueCommand(createWindow);
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
