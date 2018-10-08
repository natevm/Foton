
#include "VinylEngine/BaseClasses/System.hxx"
#include "VinylEngine/Libraries/GLFW/GLFW.hxx"

namespace Systems 
{
    class EventSystem : public System {
        public:
            static EventSystem* Get();
            bool initialize(Libraries::GLFW* glfw);
            bool start();
            bool stop();
            bool should_close();
        private:
            Libraries::GLFW* glfw;
            bool close = true;
            EventSystem();
            ~EventSystem();           
    };   
}
