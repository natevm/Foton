#include "EventSystem.hxx"

namespace Systems 
{
    EventSystem* EventSystem::Get() {
        static EventSystem instance;
        return &instance;
    }

    bool EventSystem::initialize(Libraries::GLFW* glfw) {
        this->glfw = glfw;
        using namespace Libraries;
        if (initialized) return false;
        if (!glfw->initialized) return false;
        
        initialized = true;
        close = false;
        return true;
    }

    bool EventSystem::start() {
        if (!initialized) return false;
        if (running) return false;

        using namespace Libraries;
        
        auto glfw = GLFW::Get();
        running = true;
       
        while (!close)
        {
            glfw->poll_events();
            glfw->wait_events();
            close |= (glfw->should_close());
        }
        
        return true;
    }

    bool EventSystem::stop() {
        using namespace Libraries;
        if (!initialized) return false;
        if (!running) return false;

        close = true;
        auto glfw = GLFW::Get();
        glfw->post_empty_event();
        running = false;
        return true;
    }

    bool EventSystem::should_close() {
        using namespace Libraries;
        auto glfw = GLFW::Get();
        glfw->post_empty_event();
        return close;
    }

    EventSystem::EventSystem() {}
    EventSystem::~EventSystem() {}    
}
