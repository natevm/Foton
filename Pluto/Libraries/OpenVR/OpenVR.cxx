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

} // namespace Libraries
