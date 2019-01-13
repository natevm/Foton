#pragma once
#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif

#ifndef GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#endif

#include <thread>
#include <future>
#include <vector>
#include <set>
#include <condition_variable>
#include <vulkan/vulkan.hpp>
#include <iostream>
#include <glm/glm.hpp>
#include <openvr.h>

#include "Pluto/Tools/Singleton.hxx"

class Texture;

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

            /* Provides the game with the minimum size that it should use for its offscreen render target to minimize pixel stretching. 
                This size is matched with the projection matrix and distortion function and will change from display to display depending 
                on resolution, distortion, and field of view. */
            glm::ivec2 get_recommended_render_target_size();

            /* Returns the projection transformation for the HMD's left eye */
            glm::mat4 get_left_projection_matrix(float near);

            /* Returns the projection transformation for the HMD's right eye */
            glm::mat4 get_right_projection_matrix(float near);

            /* Returns the transformation from right controller model space to world space */
            glm::mat4 get_right_controller_transform();

            /* Returns the transformation from left controller model space to world space */
            glm::mat4 get_left_controller_transform();

            /* EXPLAIN THIS */
            glm::mat4 get_left_view_matrix();
            
            /* EXPLAIN THIS */
            glm::mat4 get_right_view_matrix();

            /* Returns true if the left controller is on and connected to OpenVR */
            bool is_left_controller_connected();
            
            /* Returns true if the right controller is on and connected to OpenVR */
            bool is_right_controller_connected();
            
            /* Returns true if the head mounted display is on and connected to OpenVR */
            bool is_headset_connected();

            /* Returns true if the left controller's transform is valid or invalid, for example if tracking was lost. */
            bool is_left_controller_pose_valid();
            
            /* Returns true if the right controller's transform is valid or invalid, for example if tracking was lost. */
            bool is_right_controller_pose_valid();
            
            /* Returns true if the headset's transform is valid or invalid, for example if tracking was lost. */
            bool is_headset_pose_valid();

            /* Returns the number of elapsed seconds since the last recorded vsync event. 
                This will come from  a vsync timer event in the timer if possible or from the application-reported time 
                if that is not available. If no vsync times are available, the function will return zero for vsync time. */
            float get_time_since_last_vsync();

            /* Asynchronously updates all device poses, and polls whether or not a device was recently connected or disconnected. */
            bool poll_event();

            /* Causes the left controller haptics to trigger on the given axis for a specified duration in microseconds. */
            bool trigger_left_haptic_pulse(uint32_t axis_id, uint32_t duration_in_microseconds);
            
            /* Causes the right controller haptics to trigger on the given axis for a specified duration in microseconds. */
            bool trigger_right_haptic_pulse(uint32_t axis_id, uint32_t duration_in_microseconds);

            /* EXPLAIN THIS */
            bool create_eye_textures();

            /* EXPLAIN THIS */
            Texture *get_right_eye_texture();
            
            /* EXPLAIN THIS */
            Texture *get_left_eye_texture();

            /* EXPLAIN THIS */
            bool submit_textures();

            /* EXPLAIN THIS */
            bool wait_get_poses();

            /* EXPLAIN THIS */
            vk::PhysicalDevice get_output_device(vk::Instance instance);
        private:
            OpenVR();
            ~OpenVR();

            /* Allows access to the IVR system API */
            vr::IVRSystem *system = nullptr;

            /* EXPLAIN THIS */
            vr::IVRChaperone *chaperone = nullptr;

            /* EXPLAIN THIS */
            vr::IVRCompositor *compositor = nullptr;

            /* EXPLAIN THIS */
            Texture *right_eye_texture = nullptr;
            Texture *left_eye_texture = nullptr;

            /* IDs used to index into the tracked device poses array. These IDs will change during runtime. */
            vr::TrackedDeviceIndex_t right_hand_id = -1;
            vr::TrackedDeviceIndex_t left_hand_id = -1;
            vr::TrackedDeviceIndex_t headset_id = -1;

            /* An array of all tracked OpenVR devices, which is updated during the poll_event */
            vr::TrackedDevicePose_t tracked_device_poses[vr::k_unMaxTrackedDeviceCount];

            /* This function returns the vr::EVRInitError enum value as a string. It can be called any time, 
                regardless of whether the VR system is started up.*/
            std::string get_vr_init_error_as_symbol(vr::EVRInitError error);

            /* Returns an English string for an EVRInitError. */
            std::string get_vr_init_error_as_english_description(vr::EVRInitError error);

            /* REMOVE THIS? Constructs an infinite reverse Z projection matrix. */
            glm::mat4 make_projection_matrix(float l, float r, float t, float b, float n);
            
            /* Converts a row major HmdMatrix34_t into a column major glm matrix. */
            glm::mat4 m34_to_mat4(const vr::HmdMatrix34_t &t);

            /* EXPLAIN THIS */
            glm::mat4 m44_to_mat4(const vr::HmdMatrix44_t &t);
    };
}