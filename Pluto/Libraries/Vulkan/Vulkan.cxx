// #pragma optimize("", off)

#include <cstring>
#include <chrono>
#include <assert.h>
#include <iostream>
#include <set>
#include <string>
#include <thread>
// #include <hex>

#include <vulkan/vulkan.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "Vulkan.hxx"
#include "../GLFW/GLFW.hxx"
#if BUILD_OPENVR
#include "../OpenVR/OpenVR.hxx"
#endif

thread_local int32_t thread_id = -1;

namespace Libraries
{

VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugReportFlagsEXT flags,
    VkDebugReportObjectTypeEXT objType,
    uint64_t obj,
    size_t location,
    int32_t code,
    const char *layerPrefix,
    const char *msg,
    void *userData)
{
    std::cerr << "validation layer: " << msg << std::endl << std::endl;
    return VK_FALSE;
}

Vulkan *Vulkan::Get()
{
    static Vulkan instance;
    return &instance;
}

Vulkan::Vulkan() {}

Vulkan::~Vulkan() {}

std::vector<std::string> Vulkan::get_validation_layers()
{
    std::vector<std::string> validation_layers;

    /* Check to see if we support the requested validation layers */
    auto layerProperties = vk::enumerateInstanceLayerProperties();

    /* Check to see if the validation layers we have selected are available */
    for (auto layerProp : layerProperties) {
        validation_layers.push_back(layerProp.layerName);
    }
    
    return validation_layers;
}

/* Vulkan Instance */
bool Vulkan::create_instance(bool enable_validation_layers, set<string> validation_layers, set<string> instance_extensions, bool use_openvr)
{
    /* Prevent multiple instance creation */
    cout << "Creating vulkan instance" << endl;

    auto appInfo = vk::ApplicationInfo();
    appInfo.pApplicationName = "Pluto";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 1, 96);
    appInfo.pEngineName = "Pluto";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 1, 96);
    appInfo.apiVersion = VK_MAKE_VERSION(1, 1, 96);

    /* Determine the required instance extensions */
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    instanceExtensions.clear();
    for (uint32_t i = 0; i < glfwExtensionCount; ++i)
        instanceExtensions.insert(glfwExtensions[i]);

    validationEnabled = enable_validation_layers;
    if (validationEnabled) 
        instanceExtensions.insert(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);

    for (auto &string : instance_extensions){
        instanceExtensions.insert(string);
        std::cout<<"Enabling instance extension: " << string << std::endl;
    }
    
#if BUILD_OPENVR
    if (use_openvr) {
        std::vector<std::string> additional_instance_extensions;
        auto openvr = Libraries::OpenVR::Get();
        bool result = openvr->get_required_vulkan_instance_extensions(additional_instance_extensions);
        if (result == true) {
            for (int i = 0; i < additional_instance_extensions.size(); ++i) {
                instanceExtensions.insert(additional_instance_extensions[i]);
                std::cout<< "OpenVR: Adding instance extension " << additional_instance_extensions[i]<<std::endl;
            }
        }
    }
#endif

    /* Verify those extensions are supported */
    auto extensionProperties = vk::enumerateInstanceExtensionProperties();

    /* Check to see if the extensions we have are what are required by GLFW */
    for (auto requestedExtension : instanceExtensions)
    {
        bool extensionFound = false;
        for (auto extensionProp : extensionProperties)
            if (requestedExtension.compare(extensionProp.extensionName) == 0) {
                extensionFound = true;
                break;
            }
        if (!extensionFound) {
            throw std::runtime_error("Error: missing extension " + string(requestedExtension));
        }
    }

    /* Check to see if we support the requested validation layers */
    auto layerProperties = vk::enumerateInstanceLayerProperties();
    validationLayers.clear();
    for (auto &string : validation_layers)
        validationLayers.insert(string);

    /* Check to see if the validation layers we have selected are available */
    for (auto requestedLayer : validationLayers)
    {
        bool layerFound = false;
        for (auto layerProp : layerProperties)
            if (requestedLayer.compare(layerProp.layerName) == 0) {
                layerFound = true;
                break;
            }
        if (!layerFound)
        {
            std::string available_validation_layers = "";
            for (auto layerProp : layerProperties) {
                available_validation_layers += std::string(layerProp.layerName) + std::string(" , ");
            }
            throw std::runtime_error("Error: missing validation layer " + string(requestedLayer) + "\n" 
            + "Available layers are " + available_validation_layers);
        }
    }

    /* Instance Info: Specifies global extensions and validation layers we'd like to use */
    vector<const char *> ext, vl;
    for (const auto &string : instanceExtensions)
        ext.push_back(string.c_str());
    for (const auto &string : validationLayers)
        vl.push_back(string.c_str());
    auto info = vk::InstanceCreateInfo();
    info.pApplicationInfo = &appInfo;
    info.enabledExtensionCount = (uint32_t)(ext.size());
    info.ppEnabledExtensionNames = ext.data();
    info.enabledLayerCount = (validationEnabled) ? (uint32_t)vl.size() : 0;
    info.ppEnabledLayerNames = (validationEnabled) ? vl.data() : nullptr;
    instance = vk::createInstance(info);

    /* This dispatch loader allows us to call extension functions */
    dldi = vk::DispatchLoaderDynamic(instance, device);

    /* Add an internal callback for the validation layers */
    if (validationEnabled)
    {
        auto createCallbackInfo = vk::DebugReportCallbackCreateInfoEXT();
        createCallbackInfo.flags = vk::DebugReportFlagBitsEXT::eDebug | /*vk::DebugReportFlagBitsEXT::eInformation | */
            vk::DebugReportFlagBitsEXT::ePerformanceWarning | vk::DebugReportFlagBitsEXT::eWarning | vk::DebugReportFlagBitsEXT::eError ;
        createCallbackInfo.pfnCallback = DebugCallback;

        /* The function to assign a callback for validation layers isn't loaded by default. Here, we get that function. */
        instance.createDebugReportCallbackEXT(createCallbackInfo, nullptr, dldi);
    }

    return true;
}

bool Vulkan::destroy_instance()
{
    try
    {
        /* Don't delete null instance */
        if (!instance)
            return false;

        /* Delete instance validation if enabled */
        if (internalCallback)
        {
            instance.destroyDebugReportCallbackEXT(internalCallback, nullptr, dldi);
        }

        // if (device)
        // {
        //     device.destroy();
        // }

        if (instance) {
            instance.destroy();
        }
        instanceExtensions.clear();
    }

    catch (const std::exception &e)
    {
        cout << "Exception thrown while destroying instance!" << e.what();
        return false;
    }

    return true;
}

vk::Instance Vulkan::get_instance() const
{
    return instance;
}

vk::DispatchLoaderDynamic Vulkan::get_dldi()
{
    return dldi;
}

/* Vulkan Device */ 
bool Vulkan::create_device(set<string> device_extensions, set<string> device_features, uint32_t num_command_pools, vk::SurfaceKHR surface, bool use_openvr)
{
    if (instance == vk::Instance())
        throw std::runtime_error("Error: Cannot create device. Vulkan instance is null.");

    if (device)
        return false;

    cout << "Picking first capable device" << endl;

    /* We need to look for and select a graphics card in the system that supports the features we need */
    /* We could technically choose multiple graphics cards and use them simultaneously, but for now just choose the first one */
    auto devices = instance.enumeratePhysicalDevices();
    if (devices.size() == 0)
    {
        cout << "Failed to find GPUs with Vulkan support!" << endl;
        return false;
    }

    /* There are three things we're going to look for. We need to support requested device extensions, support a
        graphics queue, and if targeting a surface, we need a present queue as well as an adequate swap chain.*/
    presentFamilyIndex = -1;
    graphicsFamilyIndex = -1;
    uint32_t numGraphicsQueues = -1;
    uint32_t numPresentQueues = -1;
    bool swapChainAdequate, extensionsSupported, featuresSupported, queuesFound;

    /* vector of string to vector of c string... */
    deviceExtensions.clear();
    for (auto &string : device_extensions)
    {
        deviceExtensions.insert(string);
        if (string.compare("VK_NV_ray_tracing") == 0) {
            rayTracingEnabled = true;
        }
    }
    if (surface)
        deviceExtensions.insert(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    
    std::string devicename;

    /* If we're using OpenVR, we need to select the physical device that OpenVR requires us to use. */
#if BUILD_OPENVR
    if (use_openvr && false) { // get_output_device is always null?
        auto ovr = OpenVR::Get();
        physicalDevice = ovr->get_output_device(instance);
        deviceProperties = physicalDevice.getProperties();
        auto supportedFeatures = physicalDevice.getFeatures();
        auto queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

        /* Look for a queue family which supports what we need (graphics, maybe also present) */
        int32_t i = 0;
        for (auto queueFamily : queueFamilyProperties)
        {
            auto hasQueues = (queueFamily.queueCount > 0);
            auto presentSupport = (surface) ? physicalDevice.getSurfaceSupportKHR(i, surface) : false;
            if (hasQueues && presentSupport) {
                presentFamilyIndex = i;
                numPresentQueues = queueFamily.queueCount;
            }
            if (hasQueues && queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) {
                graphicsFamilyIndex = i;
                numGraphicsQueues = queueFamily.queueCount;
            }
            if (((!surface) || presentFamilyIndex != -1) && graphicsFamilyIndex != -1)
            {
                queuesFound = true;
                break;
            }
            i++;
        }

        /* Check if the device meets our extension requirements */
        auto availableExtensions = physicalDevice.enumerateDeviceExtensionProperties();
        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

        /* Indicate extensions found by removing them from this set. */
        for (const auto &extension : availableExtensions)
            requiredExtensions.erase(extension.extensionName);
        extensionsSupported = requiredExtensions.empty();

        /* If presentation required, see if swapchain support is adequate */
        /* For this example, all we require is one supported image format and one supported presentation mode */
        if (surface && extensionsSupported)
        {
            Vulkan::SwapChainSupportDetails details;
            auto capabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
            auto formats = physicalDevice.getSurfaceFormatsKHR(surface);
            auto presentModes = physicalDevice.getSurfacePresentModesKHR(surface);
            swapChainAdequate = !formats.empty() && !presentModes.empty();
        }

        /* Check if the device supports the featrues we want */
        featuresSupported = GetFeaturesFromList(device_features, supportedFeatures);

        if (queuesFound && extensionsSupported && featuresSupported && ((!surface) || swapChainAdequate))
        {
            deviceFeatures = supportedFeatures;
            devicename = deviceProperties.deviceName;
            supportedMSAASamples = min(deviceProperties.limits.framebufferColorSampleCounts, deviceProperties.limits.framebufferDepthSampleCounts);
        }
        else {
            std::cout<<"Error: OpenVR is running on a device which does not support the requested extensions. "<<std::endl;
            return false;
        }
        
    } else
#endif
    {
        /* Check and see if any physical devices are suitable, since not all cards are equal */
        physicalDevice = vk::PhysicalDevice();
        for (const auto &device : devices)
        {
            swapChainAdequate = extensionsSupported = queuesFound = false;
            deviceProperties = device.getProperties();
            auto supportedFeatures = device.getFeatures();
            auto queueFamilyProperties = device.getQueueFamilyProperties();
            std::cout<<"\tAvailable device: " << deviceProperties.deviceName <<std::endl;

            /* Look for a queue family which supports what we need (graphics, maybe also present) */
            int32_t i = 0;
            for (auto queueFamily : queueFamilyProperties)
            {
                auto hasQueues = (queueFamily.queueCount > 0);
                auto presentSupport = (surface) ? device.getSurfaceSupportKHR(i, surface) : false;
                if (hasQueues && presentSupport) {
                    presentFamilyIndex = i;
                    numPresentQueues = queueFamily.queueCount;
                }
                if (hasQueues && queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) {
                    graphicsFamilyIndex = i;
                    numGraphicsQueues = queueFamily.queueCount;
                }
                if (((!surface) || presentFamilyIndex != -1) && graphicsFamilyIndex != -1)
                {
                    queuesFound = true;
                    break;
                }
                i++;
            }

            /* Check if the device meets our extension requirements */
            auto availableExtensions = device.enumerateDeviceExtensionProperties();
            std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

            /* Indicate extensions found by removing them from this set. */
            for (const auto &extension : availableExtensions)
                requiredExtensions.erase(extension.extensionName);
            extensionsSupported = requiredExtensions.empty();

            /* If presentation required, see if swapchain support is adequate */
            /* For this example, all we require is one supported image format and one supported presentation mode */
            if (surface && extensionsSupported)
            {
                Vulkan::SwapChainSupportDetails details;
                auto capabilities = device.getSurfaceCapabilitiesKHR(surface);
                auto formats = device.getSurfaceFormatsKHR(surface);
                auto presentModes = device.getSurfacePresentModesKHR(surface);
                swapChainAdequate = !formats.empty() && !presentModes.empty();
            }

            /* Check if the device supports the featrues we want */
            featuresSupported = GetFeaturesFromList(device_features, supportedFeatures);

            if (queuesFound && extensionsSupported && featuresSupported && ((!surface) || swapChainAdequate))
            {
                deviceFeatures = supportedFeatures;
                physicalDevice = device;
                devicename = deviceProperties.deviceName;
                supportedMSAASamples = min(deviceProperties.limits.framebufferColorSampleCounts, deviceProperties.limits.framebufferDepthSampleCounts);

                if (deviceProperties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
                    break;
                }
            }
        }
    }

    /* At this point, we can provide the physical device to OpenVR, and get back any additional device extensions which may be required. */
#if BUILD_OPENVR
    if (use_openvr) {
        std::vector<std::string> additional_device_extensions;
        auto openvr = Libraries::OpenVR::Get();
        bool result = openvr->get_required_vulkan_device_extensions(physicalDevice, additional_device_extensions);
        if (result == true) {
            for (int i = 0; i < additional_device_extensions.size(); ++i) {
                deviceExtensions.insert(additional_device_extensions[i]);
                std::cout<< "OpenVR: Adding device extension " << additional_device_extensions[i]<<std::endl;
            }
        }
    }
#endif
    
    cout << "\tChoosing device " << std::string(deviceProperties.deviceName) << endl;

    if (!physicalDevice)
    {
        throw std::runtime_error("Failed to find a GPU which meets demands!" );
        return false;
    }

    /* If we're using raytracing, query raytracing properties */
    if (rayTracingEnabled) {
        vk::PhysicalDeviceProperties2 props;
        props.pNext = &deviceRaytracingProperties;
        physicalDevice.getProperties2(&props, dldi);
    }
    
    /* We now need to create a logical device, which is like an instance of a physical device */
    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    
    std::cout<<"TEMPORARY4"<<std::endl;
    numGraphicsQueues = 1;
    numPresentQueues = 1;
    
    /* Add these queue create infos to a vector to be used when creating the logical device */
    /* Vulkan allows you to specify a queue priority between 0 and one, which influences scheduling */
    
    std::vector<float> queuePriorities(numGraphicsQueues, 1.0f);
    
    /* Graphics queue */
    vk::DeviceQueueCreateInfo gQueueInfo;
    gQueueInfo.queueFamilyIndex = graphicsFamilyIndex;
    gQueueInfo.queueCount = numGraphicsQueues;
    gQueueInfo.pQueuePriorities = queuePriorities.data();
    queueCreateInfos.push_back(gQueueInfo);

    /* Present queue (if different than graphics queue) */
    if (surface && (presentFamilyIndex != graphicsFamilyIndex)) {
        vk::DeviceQueueCreateInfo pQueueInfo;
        pQueueInfo.queueFamilyIndex = presentFamilyIndex;
        pQueueInfo.queueCount = numPresentQueues;
        pQueueInfo.pQueuePriorities = queuePriorities.data();
        queueCreateInfos.push_back(pQueueInfo);
    }

    auto createInfo = vk::DeviceCreateInfo();

    /* Add pointers to the device features and queue creation structs */
    createInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;

    /* We can specify device specific extensions, like "VK_KHR_swapchain", which may not be
    available for particular compute only devices. */
    vector<const char *> ext;
    for (auto &string : deviceExtensions)
        ext.push_back(string.c_str());
    createInfo.enabledExtensionCount = (uint32_t)(ext.size());
    createInfo.ppEnabledExtensionNames = ext.data();

    /* Now create the logical device! */
    device = physicalDevice.createDevice(createInfo);

    /* Queues are implicitly created when creating device. This just gets handles. */
    for (uint32_t i = 0; i < numGraphicsQueues; ++i) {
        graphicsQueues.push_back(device.getQueue(graphicsFamilyIndex, i));
        std::cout<<"TEMPORARY1"<<std::endl;
        numGraphicsQueues = 1;
        break;
    }
    if (surface)
        for (uint32_t i = 0; i < numPresentQueues; ++i)
        {
            presentQueues.push_back(device.getQueue(presentFamilyIndex, i));
            std::cout<<"TEMPORARY2"<<std::endl;
            numPresentQueues = 1;
            break;
        }

    /* Command pools manage the memory that is used to store the buffers and command buffers are allocated from them */
    for (uint32_t i = 0; i < num_command_pools; ++i) {
        auto poolInfo = vk::CommandPoolCreateInfo();
        poolInfo.queueFamilyIndex = graphicsFamilyIndex;
        poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer; // Optional
        commandPools.push_back(device.createCommandPool(poolInfo));
    }


    initialized = true;
    return true;
}

bool Vulkan::destroy_device()
{
    try
    {
        if (!device)
            return false;

        // for (uint32_t i = 0; i < commandPools.size(); ++i) {
        //     if (commandPools[i] != vk::CommandPool())
        //         device.destroyCommandPool(commandPools[i]);
        // }
        device.destroy();
        
        return true;
    }
    catch (const std::exception &e)
    {
        cout << "Exception thrown while destroying device!" << e.what();
        return false;
    }
    return true;
}

vk::PhysicalDevice Vulkan::get_physical_device() const
{
    if (!physicalDevice)
        return vk::PhysicalDevice();
    return physicalDevice;
}

vk::PhysicalDeviceProperties Vulkan::get_physical_device_properties() const
{
    if (!physicalDevice)
        return vk::PhysicalDeviceProperties();
    return deviceProperties;
}

vk::PhysicalDeviceRayTracingPropertiesNV Vulkan::get_physical_device_ray_tracing_properties() const
{
    if (!physicalDevice)
        throw std::runtime_error("Error: physical device is null");
    if (!rayTracingEnabled)
        throw std::runtime_error("Error: raytracing is not enabled");
    return deviceRaytracingProperties;
}


vk::Device Vulkan::get_device() const
{
    if (!device)
        return vk::Device();
    return device;
}

/* Todo, macro this */
bool Vulkan::GetFeaturesFromList(set<string> device_features, vk::PhysicalDeviceFeatures &supportedFeatures)
{
    for (auto feature : device_features)
    {
        if (feature.compare("robustBufferAccess") == 0)
        {
            if (!supportedFeatures.robustBufferAccess) return false;
        }
        else if (feature.compare("fullDrawIndexUint32") == 0)
        {
            if (!supportedFeatures.fullDrawIndexUint32) return false;
        }
        else if (feature.compare("imageCubeArray") == 0)
        {
            if (!supportedFeatures.imageCubeArray) return false;
        }
        else if (feature.compare("independentBlend") == 0)
        {
            if (!supportedFeatures.independentBlend) return false;
        }
        else if (feature.compare("geometryShader") == 0)
        {
            if (!supportedFeatures.geometryShader) return false;
        }
        else if (feature.compare("tessellationShader") == 0)
        {
            if (!supportedFeatures.tessellationShader) return false;
        }
        else if (feature.compare("sampleRateShading") == 0)
        {
            if (!supportedFeatures.sampleRateShading) return false;
        }
        else if (feature.compare("dualSrcBlend") == 0)
        {
            if (!supportedFeatures.dualSrcBlend) return false;
        }
        else if (feature.compare("logicOp") == 0)
        {
            if (!supportedFeatures.logicOp) return false;
        }
        else if (feature.compare("multiDrawIndirect") == 0)
        {
            if (!supportedFeatures.multiDrawIndirect) return false;
        }
        else if (feature.compare("drawIndirectFirstInstance") == 0)
        {
            if (!supportedFeatures.drawIndirectFirstInstance) return false;
        }
        else if (feature.compare("depthClamp") == 0)
        {
            if (!supportedFeatures.depthClamp) return false;
        }
        else if (feature.compare("depthBiasClamp") == 0)
        {
            if (!supportedFeatures.depthBiasClamp) return false;
        }
        else if (feature.compare("fillModeNonSolid") == 0)
        {
            if (!supportedFeatures.fillModeNonSolid) return false;
        }
        else if (feature.compare("depthBounds") == 0)
        {
            if (!supportedFeatures.depthBounds) return false;
        }
        else if (feature.compare("wideLines") == 0)
        {
            if (!supportedFeatures.wideLines) return false;
        }
        else if (feature.compare("largePoints") == 0)
        {
            if (!supportedFeatures.largePoints) return false;
        }
        else if (feature.compare("alphaToOne") == 0)
        {
            if (!supportedFeatures.alphaToOne) return false;
        }
        else if (feature.compare("multiViewport") == 0)
        {
            if (!supportedFeatures.multiViewport) return false;
        }
        else if (feature.compare("samplerAnisotropy") == 0)
        {
            if (!supportedFeatures.samplerAnisotropy) return false;
        }
        else if (feature.compare("textureCompressionETC2") == 0)
        {
            if (!supportedFeatures.textureCompressionETC2) return false;
        }
        else if (feature.compare("textureCompressionASTC_LDR") == 0)
        {
            if (!supportedFeatures.textureCompressionASTC_LDR) return false;
        }
        else if (feature.compare("textureCompressionBC") == 0)
        {
            if (!supportedFeatures.textureCompressionBC) return false;
        }
        else if (feature.compare("occlusionQueryPrecise") == 0)
        {
            if (!supportedFeatures.occlusionQueryPrecise) return false;
        }
        else if (feature.compare("pipelineStatisticsQuery") == 0)
        {
            if (!supportedFeatures.pipelineStatisticsQuery) return false;
        }
        else if (feature.compare("vertexPipelineStoresAndAtomics") == 0)
        {
            if (!supportedFeatures.vertexPipelineStoresAndAtomics) return false;
        }
        else if (feature.compare("fragmentStoresAndAtomics") == 0)
        {
            if (!supportedFeatures.fragmentStoresAndAtomics) return false;
        }
        else if (feature.compare("shaderTessellationAndGeometryPointSize") == 0)
        {
            if (!supportedFeatures.shaderTessellationAndGeometryPointSize) return false;
        }
        else if (feature.compare("shaderImageGatherExtended") == 0)
        {
            if (!supportedFeatures.shaderImageGatherExtended) return false;
        }
        else if (feature.compare("shaderStorageImageExtendedFormats") == 0)
        {
            if (!supportedFeatures.shaderStorageImageExtendedFormats) return false;
        }
        else if (feature.compare("shaderStorageImageMultisample") == 0)
        {
            if (!supportedFeatures.shaderStorageImageMultisample) return false;
        }
        else if (feature.compare("shaderStorageImageReadWithoutFormat") == 0)
        {
            if (!supportedFeatures.shaderStorageImageReadWithoutFormat) return false;
        }
        else if (feature.compare("shaderStorageImageWriteWithoutFormat") == 0)
        {
            if (!supportedFeatures.shaderStorageImageWriteWithoutFormat) return false;
        }
        else if (feature.compare("shaderUniformBufferArrayDynamicIndexing") == 0)
        {
            if (!supportedFeatures.shaderUniformBufferArrayDynamicIndexing) return false;
        }
        else if (feature.compare("shaderSampledImageArrayDynamicIndexing") == 0)
        {
            if (!supportedFeatures.shaderSampledImageArrayDynamicIndexing) return false;
        }
        else if (feature.compare("shaderStorageBufferArrayDynamicIndexing") == 0)
        {
            if (!supportedFeatures.shaderStorageBufferArrayDynamicIndexing) return false;
        }
        else if (feature.compare("shaderStorageImageArrayDynamicIndexing") == 0)
        {
            if (!supportedFeatures.shaderStorageImageArrayDynamicIndexing) return false;
        }
        else if (feature.compare("shaderClipDistance") == 0)
        {
            if (!supportedFeatures.shaderClipDistance) return false;
        }
        else if (feature.compare("shaderCullDistance") == 0)
        {
            if (!supportedFeatures.shaderCullDistance) return false;
        }
        else if (feature.compare("shaderFloat64") == 0)
        {
            if (!supportedFeatures.shaderFloat64) return false;
        }
        else if (feature.compare("shaderInt64") == 0)
        {
            if (!supportedFeatures.shaderInt64) return false;
        }
        else if (feature.compare("shaderInt16") == 0)
        {
            if (!supportedFeatures.shaderInt16) return false;
        }
        else if (feature.compare("shaderResourceResidency") == 0)
        {
            if (!supportedFeatures.shaderResourceResidency) return false;
        }
        else if (feature.compare("shaderResourceMinLod") == 0)
        {
            if (!supportedFeatures.shaderResourceMinLod) return false;
        }
        else if (feature.compare("sparseBinding") == 0)
        {
            if (!supportedFeatures.sparseBinding) return false;
        }
        else if (feature.compare("sparseResidencyBuffer") == 0)
        {
            if (!supportedFeatures.sparseResidencyBuffer) return false;
        }
        else if (feature.compare("sparseResidencyImage2D") == 0)
        {
            if (!supportedFeatures.sparseResidencyImage2D) return false;
        }
        else if (feature.compare("sparseResidencyImage3D") == 0)
        {
            if (!supportedFeatures.sparseResidencyImage3D) return false;
        }
        else if (feature.compare("sparseResidency2Samples") == 0)
        {
            if (!supportedFeatures.sparseResidency2Samples) return false;
        }
        else if (feature.compare("sparseResidency4Samples") == 0)
        {
            if (!supportedFeatures.sparseResidency4Samples) return false;
        }
        else if (feature.compare("sparseResidency8Samples") == 0)
        {
            if (!supportedFeatures.sparseResidency8Samples) return false;
        }
        else if (feature.compare("sparseResidency16Samples") == 0)
        {
            if (!supportedFeatures.sparseResidency16Samples) return false;
        }
        else if (feature.compare("sparseResidencyAliased") == 0)
        {
            if (!supportedFeatures.sparseResidencyAliased) return false;
        }
        else if (feature.compare("variableMultisampleRate") == 0)
        {
            if (!supportedFeatures.variableMultisampleRate) return false;
        }
        else if (feature.compare("inheritedQueries") == 0)
        {
            if (!supportedFeatures.inheritedQueries) return false;
        }
        else
        {
            return false;
        }
    }
    return true;
}

/* Accessors */
uint32_t Vulkan::get_graphics_family() const
{
    return graphicsFamilyIndex;
}

uint32_t Vulkan::get_present_family() const
{
    return presentFamilyIndex;
}

vk::CommandPool Vulkan::get_command_pool() const
{
    auto vulkan = Get();
    uint32_t index = vulkan->get_thread_id();
    if (index >= commandPools.size())
        throw std::runtime_error("Error: max command pool index is " + std::to_string(commandPools.size() - 1) );

    if (commandPools[index] == vk::CommandPool())
        throw std::runtime_error("Error: command pool at index " + std::to_string(index) + " was null!" );
        
    return commandPools[index];
}

vk::Queue Vulkan::get_graphics_queue(uint32_t index) const
{
    if (index >= graphicsQueues.size()) {
        std::cout<<"Error, max graphics queue index is " << graphicsQueues.size() - 1 << std::endl;
        return vk::Queue();
    }
    return graphicsQueues[index];
}
vk::Queue Vulkan::get_present_queue(uint32_t index) const
{
    if (index >= presentQueues.size()) {
        std::cout<<"Error, max command pool index is " << presentQueues.size() - 1 << std::endl;
        return vk::Queue();
    }
    return presentQueues[index];
}

vk::DispatchLoaderDynamic Vulkan::get_dispatch_loader_dynamic() const
{
    return dldi;
}

/* Utility functions */

uint32_t Vulkan::find_memory_type(uint32_t typeBits, vk::MemoryPropertyFlags properties)
{
    /* Query available device memory properties */
    vk::PhysicalDeviceMemoryProperties memoryProperties = physicalDevice.getMemoryProperties();
    
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
    {
        if ((typeBits & 1) == 1)
        {
            if ((memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                return i;
            }
        }
        typeBits >>= 1;
    }

    throw std::runtime_error("Could not find a matching memory type");
}

vk::CommandBuffer Vulkan::begin_one_time_graphics_command() {
    vk::CommandBufferAllocateInfo cmdAllocInfo;
    cmdAllocInfo.commandPool = get_command_pool();
    cmdAllocInfo.level = vk::CommandBufferLevel::ePrimary;
    cmdAllocInfo.commandBufferCount = 1;
    vk::CommandBuffer cmdBuffer = device.allocateCommandBuffers(cmdAllocInfo)[0];

    vk::CommandBufferBeginInfo beginInfo;
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    cmdBuffer.begin(beginInfo);
    return cmdBuffer;
}

bool Vulkan::end_one_time_graphics_command(vk::CommandBuffer command_buffer, std::string hint, bool free_after_use, bool submit_immediately, uint32_t queue_idx) {
    command_buffer.end();

    uint32_t pool_id = get_thread_id();

    vk::FenceCreateInfo fenceInfo;
    vk::Fence fence = device.createFence(fenceInfo);

    std::future<void> fut = enqueue_graphics_commands({command_buffer}, {},{}, {}, fence, hint, queue_idx);

    if (submit_immediately) submit_graphics_commands();

    // std::cout<<"Pool id: " << pool_id << " pool " << std::to_string((uint32_t)VkCommandPool(get_command_pool())) << " msg " << hint << std::endl;

    fut.wait();

    auto result = device.waitForFences(fence, true, 10000000000);
    if (result != vk::Result::eSuccess) {
        std::cout<<"Fence timeout: " << hint << std::endl; 
    }

    if (free_after_use)
        device.freeCommandBuffers(get_command_pool(), {command_buffer});
    device.destroyFence(fence);
    return true;
}

// This could probably use a mutex...
std::future<void> Vulkan::enqueue_graphics_commands
(
    vector<vk::CommandBuffer> commandBuffers, 
    vector<vk::Semaphore> waitSemaphores,
    vector<vk::PipelineStageFlags> waitDstStageMasks,
    vector<vk::Semaphore> signalSemaphores,
    vk::Fence fence,
    std::string hint,
    uint32_t queue_idx
) {
    std::lock_guard<std::mutex> lock(graphics_queue_mutex);

    CommandQueueItem item;
    item.queue_idx = (item.queue_idx < (graphicsQueues.size() - 1)) ? queue_idx : 0;
    item.commandBuffers = commandBuffers;
    item.waitSemaphores = waitSemaphores;
    item.waitDstStageMask = waitDstStageMasks;
    item.signalSemaphores = signalSemaphores;
    item.fence = fence;
    item.promise = std::make_shared<std::promise<void>>();
    item.should_free = false;
    item.hint = hint;
    auto new_future = item.promise->get_future();
    graphicsCommandQueue.push(item);

    return new_future;
}

std::future<void> Vulkan::enqueue_present_commands(
    std::vector<vk::SwapchainKHR> swapchains, 
    std::vector<uint32_t> swapchain_indices, 
    std::vector<vk::Semaphore> waitSemaphores) 
{
    std::lock_guard<std::mutex> lock(present_queue_mutex);

    CommandQueueItem item;
    item.swapchains = swapchains;
    item.swapchain_indices = swapchain_indices;
    item.waitSemaphores = waitSemaphores;
    item.promise = std::make_shared<std::promise<void>>();
    auto new_future = item.promise->get_future();
    presentCommandQueue.push(item);
    return new_future;
}

bool Vulkan::submit_graphics_commands() {
    std::lock_guard<std::mutex> lock(graphics_queue_mutex);

    while (!graphicsCommandQueue.empty()) {
        auto item = graphicsCommandQueue.front();

        vk::SubmitInfo submit_info;
        submit_info.waitSemaphoreCount = (uint32_t) item.waitSemaphores.size();
        submit_info.pWaitSemaphores = item.waitSemaphores.data();
        submit_info.pWaitDstStageMask = item.waitDstStageMask.data();
        submit_info.commandBufferCount = (uint32_t) item.commandBuffers.size();
        submit_info.pCommandBuffers = item.commandBuffers.data();
        submit_info.signalSemaphoreCount = (uint32_t) item.signalSemaphores.size();
        submit_info.pSignalSemaphores = item.signalSemaphores.data();

        graphicsQueues[item.queue_idx].submit(submit_info, item.fence);
        try {
            item.promise->set_value();
        }
        catch (std::future_error& e) {
            if (e.code() == std::make_error_condition(std::future_errc::promise_already_satisfied))
                std::cout << "[promise already satisfied]\n";
            else
                std::cout << "[unknown exception]\n";
        }
        graphicsCommandQueue.pop();
    }

    return true;
}

bool Vulkan::submit_present_commands() {
    std::lock_guard<std::mutex> lock(present_queue_mutex);

    bool result = true;
    while (!presentCommandQueue.empty()) {
        auto item = presentCommandQueue.front();
        try {

            if (item.swapchains.size() > 0)
            {
                vk::PresentInfoKHR presentInfo;
                presentInfo.swapchainCount = (uint32_t) item.swapchains.size();
                presentInfo.pSwapchains = item.swapchains.data();
                presentInfo.pImageIndices = item.swapchain_indices.data();
                presentInfo.pWaitSemaphores = item.waitSemaphores.data();
                presentInfo.waitSemaphoreCount = (uint32_t) item.waitSemaphores.size();

                presentQueues[0].presentKHR(presentInfo);
                
                /* TEMPORARY!!!!! REMOVE THIS WAIT IDLE */
                // presentQueues[0].waitIdle();
            } else 
            {
                std::vector<vk::PipelineStageFlags> waitDstStageMasks;
                for (uint32_t i = 0; i < item.waitSemaphores.size(); ++i) {
                    waitDstStageMasks.push_back(vk::PipelineStageFlagBits::eBottomOfPipe);
                }
                vk::FenceCreateInfo fenceInfo;
                auto fence = device.createFence(fenceInfo);

                vk::SubmitInfo submit_info;
                submit_info.waitSemaphoreCount = (uint32_t) item.waitSemaphores.size();
                submit_info.pWaitSemaphores = item.waitSemaphores.data();
                submit_info.pWaitDstStageMask = waitDstStageMasks.data();
                submit_info.commandBufferCount = 0;
                submit_info.pCommandBuffers = nullptr;
                submit_info.signalSemaphoreCount = 0;
                submit_info.pSignalSemaphores = nullptr;
                graphicsQueues[0].submit(submit_info, fence);
                device.waitForFences(fence, true, 10000000000);

            }
        }
        catch (...) {
            result = false;
        }
        item.promise->set_value();

        presentCommandQueue.pop();
    }
    return result;
}

bool Vulkan::flush_queues()
{
    presentQueues[0].waitIdle();
    graphicsQueues[0].waitIdle();
    return true;
}

uint32_t Vulkan::get_thread_id() {
    /* Todo: make this thread safe */
    if (thread_id == -1) {
        std::lock_guard<std::mutex> lock(thread_id_mutex);
        thread_id = registered_threads;
        std::cout<<"Designating " << std::hex << thread_id << " to thread id " << std::this_thread::get_id() << std::endl;
        registered_threads++;
    }
    return thread_id;
}

bool Vulkan::is_ray_tracing_enabled() {
    return rayTracingEnabled;
}

/* Flag comparison not supported by c++ vulkan bindings. *sigh* */
vk::SampleCountFlags Vulkan::min(vk::SampleCountFlags A, vk::SampleCountFlags B) {
    int a, b;

    if (A & vk::SampleCountFlagBits::e64) { a = 7; }
    else if (A & vk::SampleCountFlagBits::e32) { a = 6; }
    else if (A & vk::SampleCountFlagBits::e16) { a = 5; }
    else if (A & vk::SampleCountFlagBits::e8) { a = 4; }
    else if (A & vk::SampleCountFlagBits::e4) { a = 3; }
    else if (A & vk::SampleCountFlagBits::e2) { a = 2; }
    else a = 1;

    if (B & vk::SampleCountFlagBits::e64) { b = 7; }
    else if (B & vk::SampleCountFlagBits::e32) { b = 6; }
    else if (B & vk::SampleCountFlagBits::e16) { b = 5; }
    else if (B & vk::SampleCountFlagBits::e8) { b = 4; }
    else if (B & vk::SampleCountFlagBits::e4) { b = 3; }
    else if (B & vk::SampleCountFlagBits::e2) { b = 2; }
    else b = 1;
    if (a == 1) return A;
    if (a < b) return A;
    return B;
}

vk::SampleCountFlagBits Vulkan::highest(vk::SampleCountFlags flags) {
    if (flags & vk::SampleCountFlagBits::e64) { return vk::SampleCountFlagBits::e64; }
    else if (flags & vk::SampleCountFlagBits::e32) { return vk::SampleCountFlagBits::e32; }
    else if (flags & vk::SampleCountFlagBits::e16) { return vk::SampleCountFlagBits::e16; }
    else if (flags & vk::SampleCountFlagBits::e8) { return vk::SampleCountFlagBits::e8; }
    else if (flags & vk::SampleCountFlagBits::e4) { return vk::SampleCountFlagBits::e4; }
    else if (flags & vk::SampleCountFlagBits::e2) { return vk::SampleCountFlagBits::e2; }
    else return vk::SampleCountFlagBits::e1;
}

vk::SampleCountFlagBits Vulkan::get_closest_sample_count_flag(uint32_t samples) {
    if ( samples >= 64) { return vk::SampleCountFlagBits::e64; }
    else if (samples >= 32) { return vk::SampleCountFlagBits::e32; }
    else if (samples >= 16) { return vk::SampleCountFlagBits::e16; }
    else if (samples >= 8) { return vk::SampleCountFlagBits::e8; }
    else if (samples >= 4) { return vk::SampleCountFlagBits::e4; }
    else if (samples >= 2) { return vk::SampleCountFlagBits::e2; }
    else return vk::SampleCountFlagBits::e1;
}

vk::SampleCountFlags Vulkan::get_msaa_sample_flags() {
    return supportedMSAASamples;
}

float Vulkan::get_max_anisotropy() {
    return deviceProperties.limits.maxSamplerAnisotropy;
}

bool Vulkan::is_ASTC_supported()
{
    return deviceFeatures.textureCompressionASTC_LDR;
}

bool Vulkan::is_ETC2_supported()
{
    return deviceFeatures.textureCompressionETC2;
}

bool Vulkan::is_BC_supported()
{
    return deviceFeatures.textureCompressionBC;
}


} // namespace Libraries
