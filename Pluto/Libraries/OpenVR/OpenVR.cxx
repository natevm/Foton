#include "OpenVR.hxx"
#include <iostream>
#include <openvr.h>
#include "glm/gtc/matrix_transform.hpp"
#include "Pluto/Libraries/Vulkan/Vulkan.hxx"
#include "Pluto/Texture/Texture.hxx"

namespace Libraries
{
using namespace vr;

OpenVR *OpenVR::Get()
{
	static OpenVR instance;
	if (!instance.is_initialized())
	{
		vr::EVRInitError error;
		instance.system = VR_Init(&error, vr::EVRApplicationType::VRApplication_Scene, nullptr);
		if (error != vr::EVRInitError::VRInitError_None)
		{
			std::cout << "OpenVR Error during initialization. " << instance.get_vr_init_error_as_english_description(error) << std::endl;
			return nullptr;
		}
		instance.compositor = (vr::IVRCompositor *) vr::VR_GetGenericInterface(vr::IVRCompositor_Version, &error);
		if (error != vr::EVRInitError::VRInitError_None)
		{
			std::cout << "OpenVR Error during compositor aquisition. " << instance.get_vr_init_error_as_english_description(error) << std::endl;
			return nullptr;
		}
		instance.chaperone = (vr::IVRChaperone *)vr::VR_GetGenericInterface(vr::IVRChaperone_Version, &error);
		if (error != vr::VRInitError_None) {
			std::cout << "OpenVR Error during chaperone aquisition. " << instance.get_vr_init_error_as_english_description(error) << std::endl;
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

	auto mat1 = glm::mat4(
		t.m[0][0], t.m[1][0], t.m[2][0], 0,
		t.m[0][1], t.m[1][1], t.m[2][1], 0,
		t.m[0][2], t.m[1][2], t.m[2][2], 0,
		t.m[0][3], t.m[1][3], t.m[2][3], 1
	);
	
	return mat1;

}

glm::mat4 OpenVR::m44_to_mat4(const vr::HmdMatrix44_t &t) {
	/* Traditionally right handed, +y is up, +x is right, -z is forward. */
	/* Want y is forward, x is right, z is up. */
	/* Also, converting from row major to column major */

	auto mat1 = glm::mat4(
		t.m[0][0], t.m[1][0], t.m[2][0], t.m[3][0],
		t.m[0][1], t.m[1][1], t.m[2][1], t.m[3][1],
		t.m[0][2], t.m[1][2], t.m[2][2], t.m[3][2],
		t.m[0][3], t.m[1][3], t.m[2][3], t.m[3][3]
	);
	
	return mat1;

}

glm::mat4 OpenVR::get_left_projection_matrix(float near)
{
	/* Pull the projection matrix from OpenVR */
	float left, right, top, bottom;
	system->GetProjectionRaw(Eye_Left, &left, &right, &top, &bottom);

	auto ovr_matrix = make_projection_matrix(left, right, top, bottom, near);

	return ovr_matrix;
}

glm::mat4 OpenVR::get_right_projection_matrix(float near)
{
	/* Pull the projection matrix from OpenVR */
	float left, right, top, bottom;
	system->GetProjectionRaw(Eye_Right, &left, &right, &top, &bottom);

	auto ovr_matrix = make_projection_matrix(left, right, top, bottom, near);

	return ovr_matrix;
}

glm::mat4 OpenVR::get_left_view_matrix() {
	if (system && (headset_id != -1)) {
		auto conversion = glm::mat4(
			1.0,  0.0,  0.0, 0.0,
			0.0,  0.0, -1.0, 0.0,
			0.0,  1.0,  0.0, 0.0,
			0.0,  0.0,  0.0, 1.0
		);

		auto ovr_hmd_matrix = glm::inverse(m34_to_mat4(tracked_device_poses[headset_id].mDeviceToAbsoluteTracking)) * conversion;
		auto ovr_eye_matrix = glm::inverse(m34_to_mat4(system->GetEyeToHeadTransform(vr::Eye_Left)));
		return ovr_eye_matrix * ovr_hmd_matrix;
	}
	return glm::mat4();
}

glm::mat4 OpenVR::get_right_view_matrix() {
	if (system && (headset_id != -1)) {
		auto conversion = glm::mat4(
			1.0,  0.0,  0.0, 0.0,
			0.0,  0.0, -1.0, 0.0,
			0.0,  1.0,  0.0, 0.0,
			0.0,  0.0,  0.0, 1.0
		);

		auto ovr_hmd_matrix = glm::inverse(m34_to_mat4(tracked_device_poses[headset_id].mDeviceToAbsoluteTracking)) * conversion;
		auto ovr_eye_matrix = glm::inverse(m34_to_mat4(system->GetEyeToHeadTransform(vr::Eye_Right)));
		return ovr_eye_matrix * ovr_hmd_matrix;
	}
	return glm::mat4();
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

	return true;
}

glm::mat4 OpenVR::get_right_controller_transform()
{
	if (system && (right_hand_id != -1)) {
		auto ovr_matrix = m34_to_mat4(tracked_device_poses[right_hand_id].mDeviceToAbsoluteTracking);
		auto conversion = glm::mat4 (
			1.0,  0.0,  0.0, 0.0,
			0.0,  0.0,  1.0, 0.0,
			0.0,  -1.0, 0.0, 0.0,
			0.0,  0.0,  0.0, 1.0
		);
		return conversion * ovr_matrix;
	}
	return glm::mat4();
}

glm::mat4 OpenVR::get_left_controller_transform()
{

	if (system && (left_hand_id != -1)) {
		auto ovr_matrix = m34_to_mat4(tracked_device_poses[left_hand_id].mDeviceToAbsoluteTracking);
		auto conversion = glm::mat4 (
			1.0,  0.0,  0.0, 0.0,
			0.0,  0.0,  1.0, 0.0,
			0.0, -1.0,  0.0, 0.0,
			0.0,  0.0,  0.0, 1.0
		);
		return conversion * ovr_matrix;
	}
	return glm::mat4();
}

bool OpenVR::is_left_controller_connected() {
	if (system && (left_hand_id != -1)) {
		return tracked_device_poses[left_hand_id].bDeviceIsConnected;
	}
	return false;
}

bool OpenVR::is_right_controller_connected() {
	if (system && (right_hand_id != -1)) {
		return tracked_device_poses[right_hand_id].bDeviceIsConnected;
	}
	return false;
}

bool OpenVR::is_headset_connected() {
	if (system && (headset_id != -1)) {
		return tracked_device_poses[headset_id].bDeviceIsConnected;
	}
	return false;
}

bool OpenVR::is_left_controller_pose_valid() {
	if (system && (left_hand_id != -1)) {
		return tracked_device_poses[left_hand_id].bPoseIsValid;
	}
	return false;
}

bool OpenVR::is_right_controller_pose_valid() {
	if (system && (right_hand_id != -1)) {
		return tracked_device_poses[right_hand_id].bPoseIsValid;
	}
	return false;
}

bool OpenVR::is_headset_pose_valid() {
	if (system && (headset_id != -1)) {
		return tracked_device_poses[headset_id].bPoseIsValid;
	}
	return false;
}


bool OpenVR::create_eye_textures() {
	auto resolution = get_recommended_render_target_size();
	right_eye_texture = Texture::Create2D("right_eye_view", resolution[0], resolution[1], true, false, 1, 1);
	left_eye_texture = Texture::Create2D("left_eye_view", resolution[0], resolution[1], true, false, 1, 1);
	if ((right_eye_texture == nullptr) && (left_eye_texture == nullptr)) return false;
	return true;
}

Texture *OpenVR::get_right_eye_texture() {
	return right_eye_texture;
}

Texture *OpenVR::get_left_eye_texture() {
	return left_eye_texture;
}

bool OpenVR::submit_textures() 
{
	if ((right_eye_texture == nullptr) || (left_eye_texture == nullptr)) return false;
	auto vk = Vulkan::Get();

	auto device = static_cast<VkDevice>(vk->get_device());
	auto physicalDevice = static_cast<VkPhysicalDevice>(vk->get_physical_device());
	auto instance = static_cast<VkInstance>(vk->get_instance());
	auto queue = static_cast<VkQueue>(vk->get_graphics_queue(0));

	vr::VRTextureBounds_t bounds;
	bounds.uMin = 0.0f;
	bounds.uMax = 1.0f;
	bounds.vMin = 0.0f;
	bounds.vMax = 1.0f;

	VRVulkanTextureData_t right_texture_data = {};
	right_texture_data.m_nImage = (uint64_t)(static_cast<VkImage>(right_eye_texture->get_color_image()));// VkImage
	right_texture_data.m_pDevice = (VkDevice_T *) device;
	right_texture_data.m_pPhysicalDevice = (VkPhysicalDevice_T *) physicalDevice;
	right_texture_data.m_pInstance = (VkInstance_T *) instance;
	right_texture_data.m_pQueue = (VkQueue_T *) queue;
	right_texture_data.m_nQueueFamilyIndex = vk->get_graphics_family();
	right_texture_data.m_nWidth = right_eye_texture->get_width();
	right_texture_data.m_nHeight = right_eye_texture->get_height();
	right_texture_data.m_nFormat = (uint32_t)(static_cast<VkFormat>(right_eye_texture->get_color_format()));
	right_texture_data.m_nSampleCount = (uint32_t)(static_cast<VkSampleCountFlagBits>(right_eye_texture->get_sample_count()));

	VRVulkanTextureData_t left_texture_data = {};
	left_texture_data.m_nImage = (uint64_t)(static_cast<VkImage>(left_eye_texture->get_color_image()));// VkImage
	left_texture_data.m_pDevice = (VkDevice_T *) device;
	left_texture_data.m_pPhysicalDevice = (VkPhysicalDevice_T *) physicalDevice;
	left_texture_data.m_pInstance = (VkInstance_T *) instance;
	left_texture_data.m_pQueue = (VkQueue_T *) queue;
	left_texture_data.m_nQueueFamilyIndex = vk->get_graphics_family();
	left_texture_data.m_nWidth = left_eye_texture->get_width();
	left_texture_data.m_nHeight = left_eye_texture->get_height();
	left_texture_data.m_nFormat = (uint32_t)(static_cast<VkFormat>(left_eye_texture->get_color_format()));
	left_texture_data.m_nSampleCount = (uint32_t)(static_cast<VkSampleCountFlagBits>(left_eye_texture->get_sample_count()));
	
	Texture_t right_eye = {};
	right_eye.eType = TextureType_Vulkan;
	right_eye.eColorSpace = ColorSpace_Gamma; // Is this optimal?
	right_eye.handle = (void*)&right_texture_data;

	Texture_t left_eye = {};
	left_eye.eType = TextureType_Vulkan;
	left_eye.eColorSpace = ColorSpace_Gamma; // Is this optimal?
	left_eye.handle = (void*)&left_texture_data;

	{	
		auto error = compositor->Submit(Eye_Left, &left_eye, &bounds);
		if (error != EVRCompositorError::VRCompositorError_None) 
		{
			std::cout<<"Error submitting: " << error << std::endl;
		}
	}

	{	
		auto error = compositor->Submit(Eye_Right, &right_eye, &bounds);
		if (error != EVRCompositorError::VRCompositorError_None) 
		{
			std::cout<<"Error submitting: " << error << std::endl;
		}
	}
	
	return true;
}

bool OpenVR::wait_get_poses() {
	// system->GetDeviceToAbsoluteTrackingPose(TrackingUniverseStanding, 0.0, tracked_device_poses, vr::k_unMaxTrackedDeviceCount);

	compositor->WaitGetPoses(tracked_device_poses, vr::k_unMaxTrackedDeviceCount, NULL, 0);
	return true;
}

vk::PhysicalDevice OpenVR::get_output_device(vk::Instance instance) {
	uint64_t device;
	auto instance_ = static_cast<VkInstance>(instance);

	system->GetOutputDevice(&device, vr::ETextureType::TextureType_Vulkan, (VkInstance_T *) instance_);
	return vk::PhysicalDevice((VkPhysicalDevice)device);
}

} // namespace Libraries
