#include "OpenVR.hxx"
#include <iostream>
#include <openvr.h>
#include "glm/gtc/matrix_transform.hpp"
#include "Foton/Libraries/Vulkan/Vulkan.hxx"
#include "Foton/Transform/Transform.hxx"
#include "Foton/Texture/Texture.hxx"
#include "Foton/Mesh/Mesh.hxx"
#include "Foton/Material/Material.hxx"
#include "Foton/Entity/Entity.hxx"
#include "Foton/Camera/Camera.hxx"
#include "Foton/Tools/Options.hxx"

#include <algorithm>
#include <cctype>
#include <string>

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
	uint32_t nBufferSize = VRCompositor()->GetVulkanDeviceExtensionsRequired((VkPhysicalDevice_T*)physicalDevice, nullptr, 0);
	if (nBufferSize > 0)
	{
		// Allocate memory for the space separated list and query for it
		char *pExtensionStr = new char[nBufferSize];
		pExtensionStr[0] = 0;
		VRCompositor()->GetVulkanDeviceExtensionsRequired((VkPhysicalDevice_T*)physicalDevice, pExtensionStr, nBufferSize);

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

bool OpenVR::is_hmd_present()
{
	return VR_IsHmdPresent();
}

bool OpenVR::is_runtime_installed()
{
	return VR_IsRuntimeInstalled();
}

std::string OpenVR::vr_runtime_path()
{
	return VR_RuntimePath();
}

std::string OpenVR::get_vr_init_error_as_symbol(vr::EVRInitError error)
{
	return std::string(VR_GetVRInitErrorAsSymbol(error));
}

std::string OpenVR::get_vr_init_error_as_english_description(vr::EVRInitError error)
{
	return std::string(VR_GetVRInitErrorAsEnglishDescription(error));
}

glm::ivec2 OpenVR::get_recommended_render_target_size()
{
	if (!system)
		return glm::ivec2(-1, -1);
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
		2.0f / (right - left), 0.0f, 0.0f, 0.0f,
		0.0f, 2.0f / (bottom - top), 0.0f, 0.0f,
		(right + left) / (right - left), (bottom + top) / (bottom - top), 0.0f, -1.0f,
		0.0f, 0.0f, near_clip, 0.0f);
}

glm::mat4 OpenVR::m34_to_mat4(const vr::HmdMatrix34_t &t)
{
	/* Traditionally right handed, +y is up, +x is right, -z is forward. */
	/* Want y is forward, x is right, z is up. */
	/* Also, converting from row major to column major */

	auto mat1 = glm::mat4(
		t.m[0][0], t.m[1][0], t.m[2][0], 0,
		t.m[0][1], t.m[1][1], t.m[2][1], 0,
		t.m[0][2], t.m[1][2], t.m[2][2], 0,
		t.m[0][3], t.m[1][3], t.m[2][3], 1);

	return mat1;
}

glm::mat4 OpenVR::m44_to_mat4(const vr::HmdMatrix44_t &t)
{
	/* Traditionally right handed, +y is up, +x is right, -z is forward. */
	/* Want y is forward, x is right, z is up. */
	/* Also, converting from row major to column major */

	auto mat1 = glm::mat4(
		t.m[0][0], t.m[1][0], t.m[2][0], t.m[3][0],
		t.m[0][1], t.m[1][1], t.m[2][1], t.m[3][1],
		t.m[0][2], t.m[1][2], t.m[2][2], t.m[3][2],
		t.m[0][3], t.m[1][3], t.m[2][3], t.m[3][3]);

	return mat1;
}

glm::mat4 OpenVR::get_left_projection_matrix(float near_)
{
	/* Pull the projection matrix from OpenVR */
	float left, right, top, bottom;
	system->GetProjectionRaw(Eye_Left, &left, &right, &top, &bottom);

	auto ovr_matrix = make_projection_matrix(left, right, top, bottom, near_);

	return ovr_matrix;
}

glm::mat4 OpenVR::get_right_projection_matrix(float near_)
{
	/* Pull the projection matrix from OpenVR */
	float left, right, top, bottom;
	system->GetProjectionRaw(Eye_Right, &left, &right, &top, &bottom);

	auto ovr_matrix = make_projection_matrix(left, right, top, bottom, near_);

	return ovr_matrix;
}

glm::mat4 OpenVR::get_left_view_matrix()
{
	if (system && (headset_id != -1))
	{
		auto conversion = glm::mat4(
			1.0, 0.0, 0.0, 0.0,
			0.0, 0.0, -1.0, 0.0,
			0.0, 1.0, 0.0, 0.0,
			0.0, 0.0, 0.0, 1.0);

		auto ovr_hmd_matrix = glm::inverse(m34_to_mat4(tracked_device_poses[headset_id].mDeviceToAbsoluteTracking)) * conversion;
		auto ovr_eye_matrix = glm::inverse(m34_to_mat4(system->GetEyeToHeadTransform(vr::Eye_Left)));
		return ovr_eye_matrix * ovr_hmd_matrix;
	}
	return glm::mat4();
}

glm::mat4 OpenVR::get_right_view_matrix()
{
	if (system && (headset_id != -1))
	{
		auto conversion = glm::mat4(
			1.0, 0.0, 0.0, 0.0,
			0.0, 0.0, -1.0, 0.0,
			0.0, 1.0, 0.0, 0.0,
			0.0, 0.0, 0.0, 1.0);

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
	if (!system)
		return false;
	system->TriggerHapticPulse(left_hand_id, axis_id, duration_in_microseconds);
	return true;
}

bool OpenVR::trigger_right_haptic_pulse(uint32_t axis_id, uint32_t duration_in_microseconds)
{
	if (!system)
		return false;
	system->TriggerHapticPulse(right_hand_id, axis_id, duration_in_microseconds);
	return true;
}

bool OpenVR::poll_event()
{
	if (!initialized)
		return false;
	if (!system)
		return false;
	VREvent_t event;
	system->PollNextEvent(&event, sizeof event);

	/* Process OpenVR events */
	switch (event.eventType)
	{
	case vr::VREvent_Quit:
		std::cout << "Quitting via menu triggered" << std::endl;
		// quit = true;
		break;
	case vr::VREvent_ButtonPress:
		std::cout << "Button pressed " << std::endl;
		break;
	case vr::VREvent_ButtonUnpress:
		std::cout << "Button unpressed " << std::endl;
		break;
	default:
		break;
	}

	right_hand_id = system->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand);
	left_hand_id = system->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand);
	headset_id = 0;
	
	right_hand_prev_state = right_hand_state;
	system->GetControllerState(right_hand_id, &right_hand_state, sizeof right_hand_state);

	left_hand_prev_state = left_hand_state;
	system->GetControllerState(left_hand_id, &left_hand_state, sizeof left_hand_state);

	return true;
}

EVRButtonId OpenVR::get_button_id(std::string key)
{
	std::transform(key.begin(), key.end(), key.begin(), [](char c) { return std::toupper(c); });

	if (key.compare("SYSTEM") == 0)
		return k_EButton_System;
	else if (key.compare("APPLICATION_MENU") == 0)
		return k_EButton_ApplicationMenu;
	else if (key.compare("TOUCH_B") == 0)
		return k_EButton_ApplicationMenu;
	else if (key.compare("TOUCH_Y") == 0)
		return k_EButton_ApplicationMenu;
	else if (key.compare("MENU") == 0)
		return k_EButton_ApplicationMenu;
	else if (key.compare("GRIP") == 0)
		return k_EButton_Grip;
	else if (key.compare("DPAD_LEFT") == 0)
		return k_EButton_DPad_Left;
	else if (key.compare("LEFT") == 0)
		return k_EButton_DPad_Left;
	else if (key.compare("DPAD_UP") == 0)
		return k_EButton_DPad_Up;
	else if (key.compare("UP") == 0)
		return k_EButton_DPad_Up;
	else if (key.compare("DPAD_RIGHT") == 0)
		return k_EButton_DPad_Right;
	else if (key.compare("RIGHT") == 0)
		return k_EButton_DPad_Right;
	else if (key.compare("DPAD_DOWN") == 0)
		return k_EButton_DPad_Down;
	else if (key.compare("DOWN") == 0)
		return k_EButton_DPad_Down;
	else if (key.compare("TOUCH_A") == 0)
		return k_EButton_A;
	else if (key.compare("TOUCH_X") == 0)
		return k_EButton_A;
	else if (key.compare("PROXIMITY_SENSOR") == 0)
		return k_EButton_ProximitySensor;
	else if (key.compare("AXIS_0") == 0)
		return k_EButton_Axis0;
	else if (key.compare("AXIS_1") == 0)
		return k_EButton_Axis1;
	else if (key.compare("AXIS_2") == 0)
		return k_EButton_Axis2;
	else if (key.compare("AXIS_3") == 0)
		return k_EButton_Axis3;
	else if (key.compare("AXIS_4") == 0)
		return k_EButton_Axis4;
	else if (key.compare("STEAMVR_TOUCHPAD") == 0)
		return k_EButton_SteamVR_Touchpad;
	else if (key.compare("TOUCHPAD") == 0)
		return k_EButton_SteamVR_Touchpad;
	else if (key.compare("STEAMVR_TRIGGER") == 0)
		return k_EButton_SteamVR_Trigger;
	else if (key.compare("TRIGGER") == 0)
		return k_EButton_SteamVR_Trigger;
	else if (key.compare("DASHBOARD_BACK") == 0)
		return k_EButton_Dashboard_Back;
	else if (key.compare("BACK") == 0)
		return k_EButton_Dashboard_Back;
	else if (key.compare("KNUCKLES_A") == 0)
		return k_EButton_Knuckles_A;
	else if (key.compare("KNUCKLES_B") == 0)
		return k_EButton_Knuckles_B;
	else if (key.compare("JOYSTICK") == 0)
		return k_EButton_Knuckles_JoyStick;
	else if (key.compare("KNUCKLES_JOYSTICK") == 0)
		return k_EButton_Knuckles_JoyStick;
	else
		return k_EButton_Max;
}

EVRControllerAxisType OpenVR::get_axis_id(std::string key)
{
	std::transform(key.begin(), key.end(), key.begin(), [](char c) { return std::toupper(c); });
	
	if (key.compare("TRACKPAD") == 0)
		return k_eControllerAxis_TrackPad;
	else if (key.compare("JOYSTICK") == 0)
		return k_eControllerAxis_Joystick;
	else if (key.compare("TRIGGER") == 0)
		return k_eControllerAxis_Trigger;
	else
		return k_eControllerAxis_None;
}

uint32_t OpenVR::get_right_button_action(std::string button)
{
	auto id = get_button_id(button);
	bool touch_1 = right_hand_prev_state.ulButtonTouched & vr::ButtonMaskFromId(id);
	bool touch_2 = right_hand_state.ulButtonTouched & vr::ButtonMaskFromId(id);

	bool press_1 = right_hand_prev_state.ulButtonPressed & vr::ButtonMaskFromId(id);
	bool press_2 = right_hand_state.ulButtonPressed & vr::ButtonMaskFromId(id);

	// Press held
	if (press_1 && press_2)
		return 4;
	// Just pressed
	else if (press_2)
		return 3;
	// Touch held
	else if (touch_1 && touch_2)
		return 2;
	// Just touched
	else if (touch_1)
		return 1;
	// None
	else
		return 0;
}

uint32_t OpenVR::get_left_button_action(std::string button)
{
	auto id = get_button_id(button);
	bool touch_1 = left_hand_prev_state.ulButtonTouched & vr::ButtonMaskFromId(id);
	bool touch_2 = left_hand_state.ulButtonTouched & vr::ButtonMaskFromId(id);

	bool press_1 = left_hand_prev_state.ulButtonPressed & vr::ButtonMaskFromId(id);
	bool press_2 = left_hand_state.ulButtonPressed & vr::ButtonMaskFromId(id);

	// Press held
	if (press_1 && press_2)
		return 4;
	// Just pressed
	else if (press_2)
		return 3;
	// Touch held
	else if (touch_1 && touch_2)
		return 2;
	// Just touched
	else if (touch_1)
		return 1;
	// None
	else
		return 0;
}

glm::vec2 OpenVR::get_right_analog_value(std::string button)
{
	if (!initialized) return glm::vec2(0.0, 0.0);
	auto id = get_axis_id(button);

	uint32_t i;
	for (i = 0; i < k_unControllerStateAxisCount; ++i)
	{
		int32_t axis_type = system->GetInt32TrackedDeviceProperty(right_hand_id,
																  static_cast<vr::TrackedDeviceProperty>(vr::Prop_Axis0Type_Int32 + i));
		if (axis_type == id)
			break;
	}

	if (i >= k_unControllerStateAxisCount)
		return glm::vec2(0.0, 0.0);
	else
		return glm::vec2(right_hand_state.rAxis[i].x, right_hand_state.rAxis[i].y);
}

glm::vec2 OpenVR::get_left_analog_value(std::string button)
{
	if (!initialized) return glm::vec2(0.0, 0.0);
	auto id = get_axis_id(button);

	uint32_t i;
	for (i = 0; i < vr::k_unControllerStateAxisCount; ++i)
	{
		int32_t axis_type = system->GetInt32TrackedDeviceProperty(left_hand_id,
																  static_cast<vr::TrackedDeviceProperty>(vr::Prop_Axis0Type_Int32 + i));
		if (axis_type == id)
			break;
	}

	if (i >= k_unControllerStateAxisCount)
		return glm::vec2(0.0, 0.0);
	else
		return glm::vec2(left_hand_state.rAxis[i].x, left_hand_state.rAxis[i].y);
}

glm::mat4 OpenVR::get_right_controller_transform()
{
	if (system && (right_hand_id != -1))
	{
		auto ovr_matrix = m34_to_mat4(tracked_device_poses[right_hand_id].mDeviceToAbsoluteTracking);
		auto conversion = glm::mat4(
			1.0, 0.0, 0.0, 0.0,
			0.0, 0.0, 1.0, 0.0,
			0.0, -1.0, 0.0, 0.0,
			0.0, 0.0, 0.0, 1.0);
		return conversion * ovr_matrix;
	}
	return glm::mat4();
}

glm::mat4 OpenVR::get_left_controller_transform()
{

	if (system && (left_hand_id != -1))
	{
		auto ovr_matrix = m34_to_mat4(tracked_device_poses[left_hand_id].mDeviceToAbsoluteTracking);
		auto conversion = glm::mat4(
			1.0, 0.0, 0.0, 0.0,
			0.0, 0.0, 1.0, 0.0,
			0.0, -1.0, 0.0, 0.0,
			0.0, 0.0, 0.0, 1.0);
		return conversion * ovr_matrix;
	}
	return glm::mat4();
}

bool OpenVR::is_left_controller_connected()
{
	if (system && (left_hand_id != -1))
	{
		return tracked_device_poses[left_hand_id].bDeviceIsConnected;
	}
	return false;
}

bool OpenVR::is_right_controller_connected()
{
	if (system && (right_hand_id != -1))
	{
		return tracked_device_poses[right_hand_id].bDeviceIsConnected;
	}
	return false;
}

bool OpenVR::is_headset_connected()
{
	if (system && (headset_id != -1))
	{
		return tracked_device_poses[headset_id].bDeviceIsConnected;
	}
	return false;
}

bool OpenVR::is_left_controller_pose_valid()
{
	if (system && (left_hand_id != -1))
	{
		return tracked_device_poses[left_hand_id].bPoseIsValid;
	}
	return false;
}

bool OpenVR::is_right_controller_pose_valid()
{
	if (system && (right_hand_id != -1))
	{
		return tracked_device_poses[right_hand_id].bPoseIsValid;
	}
	return false;
}

bool OpenVR::is_headset_pose_valid()
{
	if (system && (headset_id != -1))
	{
		return tracked_device_poses[headset_id].bPoseIsValid;
	}
	return false;
}

bool OpenVR::create_eye_textures()
{
	auto resolution = get_recommended_render_target_size();
	right_eye_texture = Texture::Create2D("right_eye_view", resolution[0], resolution[1], true, false, 1, 1);
	left_eye_texture = Texture::Create2D("left_eye_view", resolution[0], resolution[1], true, false, 1, 1);
	if ((right_eye_texture == nullptr) && (left_eye_texture == nullptr))
		return false;
	return true;
}

Texture *OpenVR::get_right_eye_texture()
{
	return right_eye_texture;
}

Texture *OpenVR::get_left_eye_texture()
{
	return left_eye_texture;
}

bool OpenVR::submit_textures()
{
	if ((right_eye_texture == nullptr) || (left_eye_texture == nullptr))
		return false;
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
	right_texture_data.m_nImage = (uint64_t)(static_cast<VkImage>(right_eye_texture->get_color_image())); // VkImage
	right_texture_data.m_pDevice = (VkDevice_T *)device;
	right_texture_data.m_pPhysicalDevice = (VkPhysicalDevice_T *)physicalDevice;
	right_texture_data.m_pInstance = (VkInstance_T *)instance;
	right_texture_data.m_pQueue = (VkQueue_T *)queue;
	right_texture_data.m_nQueueFamilyIndex = vk->get_graphics_family();
	right_texture_data.m_nWidth = right_eye_texture->get_width();
	right_texture_data.m_nHeight = right_eye_texture->get_height();
	right_texture_data.m_nFormat = (uint32_t)(static_cast<VkFormat>(right_eye_texture->get_color_format()));
	right_texture_data.m_nSampleCount = (uint32_t)(static_cast<VkSampleCountFlagBits>(right_eye_texture->get_sample_count()));

	VRVulkanTextureData_t left_texture_data = {};
	left_texture_data.m_nImage = (uint64_t)(static_cast<VkImage>(left_eye_texture->get_color_image())); // VkImage
	left_texture_data.m_pDevice = (VkDevice_T *)device;
	left_texture_data.m_pPhysicalDevice = (VkPhysicalDevice_T *)physicalDevice;
	left_texture_data.m_pInstance = (VkInstance_T *)instance;
	left_texture_data.m_pQueue = (VkQueue_T *)queue;
	left_texture_data.m_nQueueFamilyIndex = vk->get_graphics_family();
	left_texture_data.m_nWidth = left_eye_texture->get_width();
	left_texture_data.m_nHeight = left_eye_texture->get_height();
	left_texture_data.m_nFormat = (uint32_t)(static_cast<VkFormat>(left_eye_texture->get_color_format()));
	left_texture_data.m_nSampleCount = (uint32_t)(static_cast<VkSampleCountFlagBits>(left_eye_texture->get_sample_count()));

	Texture_t right_eye = {};
	right_eye.eType = TextureType_Vulkan;
	right_eye.eColorSpace = ColorSpace_Gamma; // Is this optimal?
	right_eye.handle = (void *)&right_texture_data;

	Texture_t left_eye = {};
	left_eye.eType = TextureType_Vulkan;
	left_eye.eColorSpace = ColorSpace_Gamma; // Is this optimal?
	left_eye.handle = (void *)&left_texture_data;

	{
		auto error = VRCompositor()->Submit(Eye_Left, &left_eye, &bounds);
		if (error != EVRCompositorError::VRCompositorError_None)
		{
			std::cout << "Error submitting: " << error << std::endl;
		}
	}

	{
		auto error = VRCompositor()->Submit(Eye_Right, &right_eye, &bounds);
		if (error != EVRCompositorError::VRCompositorError_None)
		{
			std::cout << "Error submitting: " << error << std::endl;
		}
	}

	return true;
}

bool OpenVR::wait_get_poses()
{
	// system->GetDeviceToAbsoluteTrackingPose(TrackingUniverseStanding, 0.0, tracked_device_poses, vr::k_unMaxTrackedDeviceCount);

	VRCompositor()->WaitGetPoses(tracked_device_poses, vr::k_unMaxTrackedDeviceCount, NULL, 0);
	return true;
}

vk::PhysicalDevice OpenVR::get_output_device(vk::Instance instance)
{
	uint64_t device;
	auto instance_ = static_cast<VkInstance>(instance);

	system->GetOutputDevice(&device, vr::ETextureType::TextureType_Vulkan, (VkInstance_T *)instance_);
	return vk::PhysicalDevice((VkPhysicalDevice)device);
}

int32_t OpenVR::get_left_knuckles_mesh(std::string name)
{
	std::string ResourcePath = Options::GetResourcePath();
	auto full_path = ResourcePath + std::string("/Defaults/VRModels/valve_controller_knu_ev2_0_left/valve_controller_knu_ev2_0_left.obj");
	auto component = Mesh::CreateFromOBJ(name, full_path);
	if (component)
		return component->get_id();
	return -1;
}

int32_t OpenVR::get_left_knuckles_basecolor_texture(std::string name)
{
	auto vk = Vulkan::Get();
	std::string ResourcePath = Options::GetResourcePath();
	auto full_path = ResourcePath;
	if (vk->is_ASTC_supported()) full_path += std::string("/Defaults/VRModels/valve_controller_knu_ev2_0_left/knucklesev2_0_basecolor_left_astc8x8.ktx");
	else if (vk->is_ETC2_supported()) full_path += std::string("/Defaults/VRModels/valve_controller_knu_ev2_0_left/knucklesev2_0_basecolor_left_etc2.ktx");
	else if (vk->is_BC_supported()) full_path += std::string("/Defaults/VRModels/valve_controller_knu_ev2_0_left/knucklesev2_0_basecolor_left_bc3.ktx");
	else {
		std::cout<<"OpenVR Error: no texture compression format supported! " << std::endl;
		return -1;
	}
	auto component = Texture::CreateFromKTX(name, full_path);
	if (component)
		return component->get_id();
	return -1;
}

int32_t OpenVR::get_left_knuckles_roughness_texture(std::string name)
{
	auto vk = Vulkan::Get();
	std::string ResourcePath = Options::GetResourcePath();
	auto full_path = ResourcePath;
	if (vk->is_ASTC_supported()) full_path += std::string("/Defaults/VRModels/valve_controller_knu_ev2_0_left/knucklesev2_0_rough_left_astc8x8.ktx");
	else if (vk->is_BC_supported()) full_path += std::string("/Defaults/VRModels/valve_controller_knu_ev2_0_left/knucklesev2_0_rough_left_bc3.ktx");
	else if (vk->is_ETC2_supported()) full_path += std::string("/Defaults/VRModels/valve_controller_knu_ev2_0_left/knucklesev2_0_rough_left_etc2.ktx");
	else {
		std::cout<<"OpenVR Error: no texture compression format supported! " << std::endl;
		return -1;
	}
	auto component = Texture::CreateFromKTX(name, full_path);
	if (component)
		return component->get_id();
	return -1;
}

int32_t OpenVR::get_right_knuckles_mesh(std::string name)
{
	std::string ResourcePath = Options::GetResourcePath();
	auto full_path = ResourcePath + std::string("/Defaults/VRModels/valve_controller_knu_ev2_0_right/valve_controller_knu_ev2_0_right.obj");
	auto component = Mesh::CreateFromOBJ(name, full_path);
	if (component)
		return component->get_id();
	return -1;
}

int32_t OpenVR::get_right_knuckles_basecolor_texture(std::string name)
{
	auto vk = Vulkan::Get();
	std::string ResourcePath = Options::GetResourcePath();
	auto full_path = ResourcePath;
	if (vk->is_ASTC_supported()) full_path += std::string("/Defaults/VRModels/valve_controller_knu_ev2_0_right/knucklesev2_0_basecolor_right_astc8x8.ktx");
	else if (vk->is_ETC2_supported()) full_path += std::string("/Defaults/VRModels/valve_controller_knu_ev2_0_right/knucklesev2_0_basecolor_right_etc2.ktx");
	else if (vk->is_BC_supported()) full_path += std::string("/Defaults/VRModels/valve_controller_knu_ev2_0_right/knucklesev2_0_basecolor_right_bc3.ktx");
	else {
		std::cout<<"OpenVR Error: no texture compression format supported! " << std::endl;
		return -1;
	}
	auto component = Texture::CreateFromKTX(name, full_path);
	if (component)
		return component->get_id();
	return -1;
}

int32_t OpenVR::get_right_knuckles_roughness_texture(std::string name)
{
	auto vk = Vulkan::Get();
	std::string ResourcePath = Options::GetResourcePath();
	auto full_path = ResourcePath;
	if (vk->is_ASTC_supported()) full_path += std::string("/Defaults/VRModels/valve_controller_knu_ev2_0_right/knucklesev2_0_rough_right_astc8x8.ktx");
	else if (vk->is_ETC2_supported()) full_path += std::string("/Defaults/VRModels/valve_controller_knu_ev2_0_right/knucklesev2_0_rough_right_etc2.ktx");
	else if (vk->is_BC_supported()) full_path += std::string("/Defaults/VRModels/valve_controller_knu_ev2_0_right/knucklesev2_0_rough_right_bc3.ktx");
	else {
		std::cout<<"OpenVR Error: no texture compression format supported! " << std::endl;
		return -1;
	}
	auto component = Texture::CreateFromKTX(name, full_path);
	if (component)
		return component->get_id();
	return -1;
}

int32_t OpenVR::get_vive_controller_mesh(std::string name)
{
	std::string ResourcePath = Options::GetResourcePath();
	auto full_path = ResourcePath + std::string("/Defaults/VRModels/vr_controller_vive_1_5/vr_controller_vive_1_5.obj");
	auto component = Mesh::CreateFromOBJ(name, full_path);
	if (component)
		return component->get_id();
	return -1;
}

int32_t OpenVR::get_vive_controller_basecolor_texture(std::string name)
{
	auto vk = Vulkan::Get();
	std::string ResourcePath = Options::GetResourcePath();
	auto full_path = ResourcePath;
	if (vk->is_ASTC_supported()) full_path += std::string("/Defaults/VRModels/vr_controller_vive_1_5/onepointfive_basecolor_astc8x8.ktx");
	else if (vk->is_ETC2_supported()) full_path += std::string("/Defaults/VRModels/vr_controller_vive_1_5/onepointfive_basecolor_etc2.ktx");
	else if (vk->is_BC_supported()) full_path += std::string("/Defaults/VRModels/vr_controller_vive_1_5/onepointfive_basecolor_bc3.ktx");
	else {
		std::cout<<"OpenVR Error: no texture compression format supported! " << std::endl;
		return -1;
	}
	auto component = Texture::CreateFromKTX(name, full_path);
	if (component)
		return component->get_id();
	return -1;
}

int32_t OpenVR::get_vive_controller_roughness_texture(std::string name)
{
	auto vk = Vulkan::Get();
	std::string ResourcePath = Options::GetResourcePath();
	auto full_path = ResourcePath;
	if (vk->is_ASTC_supported()) full_path += std::string("/Defaults/VRModels/vr_controller_vive_1_5/onepointfive_rough_astc8x8.ktx");
	else if (vk->is_ETC2_supported()) full_path += std::string("/Defaults/VRModels/vr_controller_vive_1_5/onepointfive_rough_etc2.ktx");
	else if (vk->is_BC_supported()) full_path += std::string("/Defaults/VRModels/vr_controller_vive_1_5/onepointfive_rough_bc3.ktx");
	else {
		std::cout<<"OpenVR Error: no texture compression format supported! " << std::endl;
		return -1;
	}
	auto component = Texture::CreateFromKTX(name, full_path);
	if (component)
		return component->get_id();
	return -1;
}

void OpenVR::connect_camera(Camera *camera)
{
	if (initialized == false)
		throw std::runtime_error( std::string("Error: Uninitialized, cannot connect camera to VR."));
	if (!camera) throw std::runtime_error("Error: camera was null!");
	if (camera->get_max_views() != 2) 
		throw std::runtime_error(
			std::string("Error: camera needs exactly two views, but has ") 
			+ std::to_string(camera->get_max_views())
		);
	
	connected_camera = camera;
}

void OpenVR::connect_camera_transform(Transform *camera_transform)
{
	if (initialized == false)
		throw std::runtime_error( std::string("Error: Uninitialized, cannot connect transform to VR."));
	if (!camera_transform) throw std::runtime_error("Error: transform was null!");
	
	connected_camera_transform = camera_transform;
}

void OpenVR::connect_left_hand_transform(Transform *left_hand_transform)
{
	if (initialized == false)
		throw std::runtime_error( std::string("Error: Uninitialized, cannot connect transform to VR."));
	if (!left_hand_transform) throw std::runtime_error("Error: transform was null!");
	
	connected_left_hand_transform = left_hand_transform;
}

void OpenVR::connect_right_hand_transform(Transform *right_hand_transform)
{
	if (initialized == false)
		throw std::runtime_error( std::string("Error: Uninitialized, cannot connect transform to VR."));
	if (!right_hand_transform) throw std::runtime_error("Error: transform was null!");
	
	connected_right_hand_transform = right_hand_transform;
}

Camera* OpenVR::get_connected_camera()
{
	return connected_camera;
}

Transform* OpenVR::get_connected_camera_transform()
{
	return connected_camera_transform;
}

Transform* OpenVR::get_connected_left_hand_transform()
{
	return connected_left_hand_transform;
}

Transform* OpenVR::get_connected_right_hand_transform()
{
	return connected_right_hand_transform;
}

Mesh* OpenVR::get_left_eye_hidden_area_mesh()
{
	if (!system) throw std::runtime_error("Error: OpenVR not initialized");
	if (left_eye_hidden_area_mesh != nullptr) return left_eye_hidden_area_mesh;

	vr::HiddenAreaMesh_t hidden_area_mesh = system->GetHiddenAreaMesh(vr::Hmd_Eye::Eye_Left);
	if (hidden_area_mesh.unTriangleCount == 0) return nullptr;

	std::vector<glm::vec4> positions(hidden_area_mesh.unTriangleCount * 3);
	for (uint32_t i = 0; i < hidden_area_mesh.unTriangleCount; ++i) {
		positions[i * 3 + 0] = glm::vec4( hidden_area_mesh.pVertexData[i * 3 + 0].v[0] * 2.0 - 1.0, hidden_area_mesh.pVertexData[i * 3 + 0].v[1] * 2.0 - 1.0, 0.0, 1.0);
		positions[i * 3 + 1] = glm::vec4( hidden_area_mesh.pVertexData[i * 3 + 1].v[0] * 2.0 - 1.0, hidden_area_mesh.pVertexData[i * 3 + 1].v[1] * 2.0 - 1.0, 0.0, 1.0);
		positions[i * 3 + 2] = glm::vec4( hidden_area_mesh.pVertexData[i * 3 + 2].v[0] * 2.0 - 1.0, hidden_area_mesh.pVertexData[i * 3 + 2].v[1] * 2.0 - 1.0, 0.0, 1.0);
	}
	left_eye_hidden_area_mesh = Mesh::CreateFromRaw("ovr_left_eye_hidden_area_mesh", positions);
	return left_eye_hidden_area_mesh;
}

Mesh* OpenVR::get_right_eye_hidden_area_mesh()
{
	if (!system) throw std::runtime_error("Error: OpenVR not initialized");
	if (right_eye_hidden_area_mesh != nullptr) return right_eye_hidden_area_mesh;

	vr::HiddenAreaMesh_t hidden_area_mesh = system->GetHiddenAreaMesh(vr::Hmd_Eye::Eye_Right);
	if (hidden_area_mesh.unTriangleCount == 0) return nullptr;

	std::vector<glm::vec4> positions(hidden_area_mesh.unTriangleCount * 3);
	for (uint32_t i = 0; i < hidden_area_mesh.unTriangleCount; ++i) {
		positions[i * 3 + 0] = glm::vec4( hidden_area_mesh.pVertexData[i * 3 + 0].v[0] * 2.0 - 1.0, hidden_area_mesh.pVertexData[i * 3 + 0].v[1] * 2.0 - 1.0, 0.0, 1.0);
		positions[i * 3 + 1] = glm::vec4( hidden_area_mesh.pVertexData[i * 3 + 1].v[0] * 2.0 - 1.0, hidden_area_mesh.pVertexData[i * 3 + 1].v[1] * 2.0 - 1.0, 0.0, 1.0);
		positions[i * 3 + 2] = glm::vec4( hidden_area_mesh.pVertexData[i * 3 + 2].v[0] * 2.0 - 1.0, hidden_area_mesh.pVertexData[i * 3 + 2].v[1] * 2.0 - 1.0, 0.0, 1.0);
	}
	right_eye_hidden_area_mesh = Mesh::CreateFromRaw("ovr_right_eye_hidden_area_mesh", positions);
	return right_eye_hidden_area_mesh;
}

Material* OpenVR::get_left_eye_hidden_area_material()
{
	if (!system) throw std::runtime_error("Error: OpenVR not initialized");
	if (left_eye_hidden_area_material != nullptr) return left_eye_hidden_area_material;
	left_eye_hidden_area_material = Material::Create("ovr_left_eye_hidden_area_material");
	left_eye_hidden_area_material->hidden(true);
	return left_eye_hidden_area_material;
}

Material* OpenVR::get_right_eye_hidden_area_material()
{
	if (!system) throw std::runtime_error("Error: OpenVR not initialized");
	if (right_eye_hidden_area_material != nullptr) return right_eye_hidden_area_material;
	right_eye_hidden_area_material = Material::Create("ovr_right_eye_hidden_area_material");
	right_eye_hidden_area_material->hidden(true);
	return right_eye_hidden_area_material;
}

Transform* OpenVR::get_left_eye_hidden_area_transform()
{
	if (!system) throw std::runtime_error("Error: OpenVR not initialized");
	if (left_eye_hidden_area_transform != nullptr) return left_eye_hidden_area_transform;
	left_eye_hidden_area_transform = Transform::Create("ovr_left_eye_hidden_area_transform");
	return left_eye_hidden_area_transform;
}

Transform* OpenVR::get_right_eye_hidden_area_transform()
{
	if (!system) throw std::runtime_error("Error: OpenVR not initialized");
	if (right_eye_hidden_area_transform != nullptr) return right_eye_hidden_area_transform;
	right_eye_hidden_area_transform = Transform::Create("ovr_right_eye_hidden_area_transform");
	return right_eye_hidden_area_transform;
}

Entity* OpenVR::get_left_eye_hidden_area_entity()
{
	if (!system) throw std::runtime_error("Error: OpenVR not initialized");
	if (left_eye_hidden_area_entity != nullptr) return left_eye_hidden_area_entity;
	if (left_eye_hidden_area_mesh == nullptr) get_left_eye_hidden_area_mesh();
	if (left_eye_hidden_area_material == nullptr) get_left_eye_hidden_area_material();
	if (left_eye_hidden_area_transform == nullptr) get_left_eye_hidden_area_transform();

	left_eye_hidden_area_entity = Entity::Create("ovr_left_eye_hidden_area_entity");
	if (left_eye_hidden_area_mesh)
		left_eye_hidden_area_entity->set_mesh(left_eye_hidden_area_mesh);
	left_eye_hidden_area_entity->set_material(left_eye_hidden_area_material);
	left_eye_hidden_area_entity->set_transform(left_eye_hidden_area_transform);

	return left_eye_hidden_area_entity;
}

Entity* OpenVR::get_right_eye_hidden_area_entity()
{
	if (!system) throw std::runtime_error("Error: OpenVR not initialized");
	if (right_eye_hidden_area_entity != nullptr) return right_eye_hidden_area_entity;
	if (right_eye_hidden_area_mesh == nullptr) get_right_eye_hidden_area_mesh();
	if (right_eye_hidden_area_material == nullptr) get_right_eye_hidden_area_material();
	if (right_eye_hidden_area_transform == nullptr) get_right_eye_hidden_area_transform();

	right_eye_hidden_area_entity = Entity::Create("ovr_right_eye_hidden_area_entity");
	if (right_eye_hidden_area_mesh)
		right_eye_hidden_area_entity->set_mesh(right_eye_hidden_area_mesh);
	right_eye_hidden_area_entity->set_material(right_eye_hidden_area_material);
	right_eye_hidden_area_entity->set_transform(right_eye_hidden_area_transform);

	return right_eye_hidden_area_entity;
}

} // namespace Libraries
