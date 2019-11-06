#pragma once

#include "Foton/Tools/Singleton.hxx"
#include <glm/glm.hpp>

namespace Libraries {
    using namespace std;
    class SpaceMouse : public Singleton
    {
        public:
            /* Retieves the SpaceMouse singleton */
            static SpaceMouse* Get();
        
            bool connect_to_window(std::string window);
            glm::vec3 get_translation();
            glm::vec3 get_rotation();
            bool poll_event();

        private:
            glm::vec3 translation;
            glm::vec3 rotation;

            SpaceMouse();
            ~SpaceMouse();

            void SbMotionEvent();
            void SbZeroEvent();
            void HandleDeviceChangeEvent();
            void HandleV3DCMDEvent();
            void HandleAppEvent();

    };
}
