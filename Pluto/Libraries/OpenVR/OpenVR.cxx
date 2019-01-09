#include "OpenVR.hxx"
#include <iostream>
#include <openvr.h>

namespace Libraries
{
using namespace vr;

OpenVR *OpenVR::Get()
{
	static OpenVR instance;
	if (!instance.is_initialized())
	{
		vr::EVRInitError initError;
		instance.system = VR_Init(&initError, vr::EVRApplicationType::VRApplication_Scene, nullptr);
		if (initError != vr::EVRInitError::VRInitError_None)
		{
			std::cout << "OpenVR Error during initialization. " << instance.get_vr_init_error_as_english_description(initError) << std::endl;
			return nullptr;
		}
		instance.initialized = true;
	}
	return &instance;
}

OpenVR::OpenVR() {}

OpenVR::~OpenVR()
{
	if (initialized)
	{
		VR_Shutdown();
	}
}

bool OpenVR::get_required_vulkan_instance_extensions(std::vector<std::string> &outInstanceExtensionList)
{
	if (!VRCompositor())
	{
		return false;
	}

	outInstanceExtensionList.clear();
	uint32_t nBufferSize = VRCompositor()->GetVulkanInstanceExtensionsRequired(nullptr, 0);
	if (nBufferSize > 0)
	{
		// Allocate memory for the space separated list and query for it
		char *pExtensionStr = new char[nBufferSize];
		pExtensionStr[0] = 0;
		VRCompositor()->GetVulkanInstanceExtensionsRequired(pExtensionStr, nBufferSize);

		// Break up the space separated list into entries on the CUtlStringList
		std::string curExtStr;
		uint32_t nIndex = 0;
		while (pExtensionStr[nIndex] != 0 && (nIndex < nBufferSize))
		{
			if (pExtensionStr[nIndex] == ' ')
			{
				outInstanceExtensionList.push_back(curExtStr);
				curExtStr.clear();
			}
			else
			{
				curExtStr += pExtensionStr[nIndex];
			}
			nIndex++;
		}
		if (curExtStr.size() > 0)
		{
			outInstanceExtensionList.push_back(curExtStr);
		}

		delete[] pExtensionStr;
	}

	return true;
}

bool OpenVR::get_required_vulkan_device_extensions(vk::PhysicalDevice &physicalDevice, std::vector<std::string> &outDeviceExtensionList)
{
	if (!VRCompositor())
	{
		return false;
	}

	if (physicalDevice == vk::PhysicalDevice())
	{
		return false;
	}

	outDeviceExtensionList.clear();
	uint32_t nBufferSize = VRCompositor()->GetVulkanDeviceExtensionsRequired(physicalDevice, nullptr, 0);
	if (nBufferSize > 0)
	{
		// Allocate memory for the space separated list and query for it
		char *pExtensionStr = new char[nBufferSize];
		pExtensionStr[0] = 0;
		VRCompositor()->GetVulkanDeviceExtensionsRequired(physicalDevice, pExtensionStr, nBufferSize);

		// Break up the space separated list into entries on the CUtlStringList
		std::string curExtStr;
		uint32_t nIndex = 0;
		while (pExtensionStr[nIndex] != 0 && (nIndex < nBufferSize))
		{
			if (pExtensionStr[nIndex] == ' ')
			{
				outDeviceExtensionList.push_back(curExtStr);
				curExtStr.clear();
			}
			else
			{
				curExtStr += pExtensionStr[nIndex];
			}
			nIndex++;
		}
		if (curExtStr.size() > 0)
		{
			outDeviceExtensionList.push_back(curExtStr);
		}

		delete[] pExtensionStr;
	}

	return true;
}

bool OpenVR::is_hmd_present() {
	return VR_IsHmdPresent();
}

bool OpenVR::is_runtime_installed() {
	return VR_IsRuntimeInstalled();
}

std::string OpenVR::vr_runtime_path() {
	return VR_RuntimePath();
}

std::string OpenVR::get_vr_init_error_as_symbol(vr::EVRInitError error) {
	return std::string(VR_GetVRInitErrorAsSymbol(error));
}

std::string OpenVR::get_vr_init_error_as_english_description(vr::EVRInitError error) {
	return std::string(VR_GetVRInitErrorAsEnglishDescription(error));
}

glm::ivec2 OpenVR::get_recommended_render_target_size()
{
	if (!system) return glm::ivec2(-1, -1);
	uint32_t width, height;
	system->GetRecommendedRenderTargetSize(&width, &height);
	return glm::ivec2(width, height);
}

// Reversed z projection matrix
// https://nlguillemot.wordpress.com/2016/12/07/reversed-z-in-opengl/
glm::mat4 OpenVR::make_projection_matrix(const float left, const float right,
		const float top, const float bottom, const float near_clip)
{
	return glm::mat4(
		2.0f/(right - left), 0.0f, 0.0f, 0.0f,
		0.0f, 2.0f/(bottom - top), 0.0f, 0.0f,
		(right + left)/(right - left), (bottom + top)/(bottom - top), 0.0f, -1.0f,
		0.0f, 0.0f, near_clip, 0.0f
	);
}

glm::mat4 OpenVR::m34_to_mat4(const vr::HmdMatrix34_t &t) {
	/* Traditionally right handed, +y is up, +x is right, -z is forward. */
	/* Want y is forward, x is right, z is up. */
	/* Also, converting from row major to column major */
	return glm::mat4(
		t.m[0][0], t.m[1][0], t.m[2][0], 0,
		t.m[0][1], t.m[1][1], t.m[2][1], 0,
		t.m[0][2], t.m[1][2], t.m[2][2], 0,
		t.m[0][3], t.m[1][3], t.m[2][3], 0
	);
}

glm::mat4 OpenVR::get_left_projection_matrix(float near)
{
	/* Pull the projection matrix from OpenVR */
	float left, right, top, bottom;
	system->GetProjectionRaw(Eye_Left, &left, &right, &top, &bottom);
	return make_projection_matrix(left, right, top, bottom, near);
}

glm::mat4 OpenVR::get_right_projection_matrix(float near)
{
	/* Pull the projection matrix from OpenVR */
	float left, right, top, bottom;
	system->GetProjectionRaw(Eye_Right, &left, &right, &top, &bottom);
	return make_projection_matrix(left, right, top, bottom, near);
}

glm::mat4 OpenVR::get_left_view_matrix()
{
	return glm::inverse(m34_to_mat4(system->GetEyeToHeadTransform(vr::Eye_Left)));
}

glm::mat4 OpenVR::get_right_view_matrix()
{
	return glm::inverse(m34_to_mat4(system->GetEyeToHeadTransform(vr::Eye_Right)));
}

float OpenVR::get_time_since_last_vsync()
{
	float last_time;
	uint64_t pullFrameCounter;
	system->GetTimeSinceLastVsync(&last_time, &pullFrameCounter);
	return last_time;
}

bool OpenVR::trigger_left_haptic_pulse(uint32_t axis_id, uint32_t duration_in_microseconds)
{
	if (!system) return false;
	system->TriggerHapticPulse(left_hand_id, axis_id, duration_in_microseconds);
	return true;
}

bool OpenVR::trigger_right_haptic_pulse(uint32_t axis_id, uint32_t duration_in_microseconds)
{
	if (!system) return false;
	system->TriggerHapticPulse(right_hand_id, axis_id, duration_in_microseconds);
	return true;
}

bool OpenVR::poll_event()
{
	if (!system) return false;
	VREvent_t event;
	system->PollNextEvent(&event, sizeof event);

	right_hand_id = system->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand);
	left_hand_id = system->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand);
	headset_id = 0;

	switch (event.eventType) {
		case vr::VREvent_Quit:
			std::cout << "Quitting via steam menu" << std::endl;
			// quit = true;
			break;
		case vr::VREvent_TrackedDeviceActivated:
		std::cout << "Device Activated "<<std::endl;
			// if (!right_gesture && right_hand_evt) {
			// 	right_gesture = std::make_unique<GestureSwipe>(vr_system, event.trackedDeviceIndex);
			// }
			break;
		case vr::VREvent_TrackedDeviceRoleChanged:
			std::cout << "Tracked Device Role Changed "<<std::endl;
			// // if role is assigned to left or right hand the event.trackedDeviceIndex is -1 (it lags by one event)
			// // thus we always make sure the assignment of hands is correct no matter the event.trackedDeviceIndex
			// if (vr_system->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand) != -1) {
			// 	assignments.left_hand.tracked_device_id =
			// 		vr_system->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand);
			// }
			// if (vr_system->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand) != -1) {
			// 	assignments.right_hand.tracked_device_id =
			// 		vr_system->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand);
			// }
			break;
		default:
			break;
	}
	
	system->GetDeviceToAbsoluteTrackingPose(TrackingUniverseStanding, 0.0, tracked_device_poses, vr::k_unMaxTrackedDeviceCount);
	
	return true;
}

glm::mat4 OpenVR::get_right_controller_transform()
{
	if (system && (right_hand_id != -1)) {
		return glm::inverse(m34_to_mat4(tracked_device_poses[right_hand_id].mDeviceToAbsoluteTracking));
	}
	return glm::mat4();
}

glm::mat4 OpenVR::get_left_controller_transform()
{
	if (system && (left_hand_id != -1)) {
		return glm::inverse(m34_to_mat4(tracked_device_poses[left_hand_id].mDeviceToAbsoluteTracking));
	}
	return glm::mat4();
}

glm::mat4 OpenVR::get_headset_transform()
{
	if (system && (headset_id != -1)) {
		return glm::inverse(m34_to_mat4(tracked_device_poses[headset_id].mDeviceToAbsoluteTracking));
	}
	return glm::mat4();
}

bool OpenVr::is_left_controller_connected() {
	if (system && (left_hand_id != -1)) {
		return tracked_device_poses[left_hand_id].bDeviceIsConnected;
	}
	return false;
}

bool OpenVr::is_right_controller_connected() {
	if (system && (right_hand_id != -1)) {
		return tracked_device_poses[right_hand_id].bDeviceIsConnected;
	}
	return false;
}

bool OpenVr::is_headset_connected() {
	if (system && (headset_id != -1)) {
		return tracked_device_poses[headset_id].bDeviceIsConnected;
	}
	return false;
}

} // namespace Libraries
