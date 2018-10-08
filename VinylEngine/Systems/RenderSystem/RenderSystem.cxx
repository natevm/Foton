#include "RenderSystem.hxx"
#include "VinylEngine/Tools/Colors.hxx"

namespace Systems 
{
    RenderSystem* RenderSystem::Get() {
        static RenderSystem instance;
        return &instance;
    }

    bool RenderSystem::initialize(Libraries::Vulkan* vulkan, vk::SurfaceKHR surface) {
        using namespace Libraries;
        if (initialized) return false;
        if (!vulkan->is_initialized()) return false;
        if (!surface) return false;

        this->surface = surface;
        this->vulkan = vulkan;

        initialized = true;
        return true;
    }

    bool RenderSystem::set_integrator(Integrator integrator) {
        this->currentIntegrator = integrator;
        return true;
    }

    bool RenderSystem::set_background_color(float r, float g, float b) {
        this->r = r;
        this->g = g;
        this->b = b;
        return true;
    }

    bool RenderSystem::start() {
        if (!initialized) return false;
        if (running) return false;

        auto loop = [this](future<void> futureObj) {
            using namespace Libraries;
            auto vulkan = Vulkan::Get();
            auto glfw = GLFW::Get();

            auto device = vulkan->get_device();

            auto lastTime = glfwGetTime();
            auto currentTime = glfwGetTime();
            while (futureObj.wait_for(std::chrono::milliseconds((uint32_t) (100 * (.16 - (currentTime - lastTime))))) == future_status::timeout)
            {
                lastTime = currentTime;
                glfw->acquire_images(vulkan);
                /* TEST CODE: generate clear color */
                std::array<float, 3> color = Colors::hsvToRgb({(float)glfwGetTime() * .1f, 1.0f, 1.0f});
                glfw->test_clear_images(vulkan, r, g, b);
                glfw->submit_cmd_buffers(vulkan);
            }
        };

        exitSignal = promise<void>();
        future<void> futureObj = exitSignal.get_future();
        eventThread = thread(loop, move(futureObj));

        running = true;
        return true;
    }

    bool RenderSystem::stop() {
        if (!initialized) return false;
        if (!running) return false;

        exitSignal.set_value();
        eventThread.join();

        running = false;
        return true;
    }

    RenderSystem::RenderSystem() {}
    RenderSystem::~RenderSystem() {}


    
}