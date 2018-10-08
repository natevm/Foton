#include "VinylEngine/BaseClasses/System.hxx"
#include "VinylEngine/Libraries/GLFW/GLFW.hxx"
#include "VinylEngine/Libraries/Vulkan/Vulkan.hxx"

namespace Systems 
{
    enum Integrator {
        Unselected = -1,
        Default = 0,
        Direct = 1,
        VXGI = 2
    };

    class RenderSystem : public System {
        public:
            static RenderSystem* Get();
            bool initialize(Libraries::Vulkan* vulkan, vk::SurfaceKHR surface);
            bool set_integrator(Integrator integrator);
            bool set_background_color(float r, float g, float b);
            bool start();
            bool stop();
        private:
            float r, g, b;
            Integrator currentIntegrator = Integrator::Unselected;
            vk::SurfaceKHR surface;
            Libraries::Vulkan* vulkan;
            RenderSystem();
            ~RenderSystem();            
    };   
}