#pragma once
#include <openvr.h>
#include <thread>
#include <future>
#include <vector>
#include <set>
#include <condition_variable>
#include <vulkan/vulkan.hpp>

#include "Pluto/Tools/Singleton.hxx"
// #include "threadsafe_queue.hpp"

namespace Libraries {
    using namespace std;
    class OpenVR : public Singleton
    {
        public:
            /* Retieves the OpenVR singleton */
            static OpenVR* Get();

            /* Returns by reference a list of the extensions required to use OpenVR in Vulkan. */
            bool get_required_vulkan_instance_extensions(std::vector<std::string> &outInstanceExtensionList);
        
            /* Given a physical device, returns by reference a list of the device extensions required to use OpenVR in Vulkan. */
            bool get_required_vulkan_device_extensions(vk::PhysicalDevice &physicalDevice, std::vector<std::string> &outDeviceExtensionList);


            /* Returns true if the system believes that an HMD is present on the system. */
            bool is_hmd_present();

            /* This function will return true in situations where vr::VR_Init() will return NULL. it is 
                a quick way to eliminate users that have no VR hardware, but there are some startup conditions
                that can only be detected by starting the system. */
            bool is_runtime_installed();

            /* Returns where the OpenVR runtime is installed. */
            std::string vr_runtime_path();


        private:
            OpenVR();
            ~OpenVR();

            /* Allows access to the IVR system API */
            vr::IVRSystem *system;

            /* This function returns the vr::EVRInitError enum value as a string. It can be called any time, 
                regardless of whether the VR system is started up.*/
            std::string get_vr_init_error_as_symbol(vr::EVRInitError error);

            /* Returns an English string for an EVRInitError. */
            std::string get_vr_init_error_as_english_description(vr::EVRInitError error);
    };
}