#pragma once
#include <vulkan/vulkan.hpp>
#include <thread>
#include <future>
#include <vector>
#include <set>

#include "VinylEngine/BaseClasses/Singleton.hxx"

namespace Libraries {
    using namespace std;
    class Vulkan : public Singleton
    {
    public:
        static Vulkan* Get();

        bool create_instance(bool enable_validation_layers = true, 
            vector<string> validation_layers = vector<string>(),
            vector<string> instance_extensions = vector<string>() );
        bool destroy_instance();


        bool create_device(
            vector<string> device_extensions = vector<string>(),
            vector<string> device_features = vector<string>(),
            vk::SurfaceKHR surface = vk::SurfaceKHR()
        );
        bool destroy_device();


        vk::Instance get_instance() const;
        vk::PhysicalDevice get_physical_device() const;
        vk::Device get_device() const;
        uint32_t get_graphics_family() const;
        uint32_t get_present_family() const;
        vk::CommandPool get_command_pool() const;
        vk::Queue get_graphics_queue() const;
        vk::Queue get_present_queue() const;
        vk::DispatchLoaderDynamic get_dispatch_loader_dynamic() const;
        
        // bool set_validation_callback(std::function<void()> debugCallback);
    
    private:
        bool validationEnabled = true;
        set<string> validationLayers;
        set<string> instanceExtensions;
        set<string> deviceExtensions;
        vk::Instance instance;
        vk::DispatchLoaderDynamic dldi;

        Vulkan();
        ~Vulkan();
        
        promise<void> exitSignal;
        thread eventThread;
        int32_t graphicsFamilyIndex = -1;
        int32_t presentFamilyIndex = -1;
        vk::CommandPool commandPool;


        struct QueueFamilyIndices {
            int graphicsFamily = -1;
            int presentFamily = -1;

            bool isComplete() {
                return graphicsFamily >= 0 && presentFamily >= 0;
            }
        };

        struct SwapChainSupportDetails {
            VkSurfaceCapabilitiesKHR capabilities;
            std::vector<VkSurfaceFormatKHR> formats;
            std::vector<VkPresentModeKHR> presentModes;
        };
        
        vk::PhysicalDevice physicalDevice;
        vk::PhysicalDeviceProperties deviceProperties;
        vk::PhysicalDeviceFeatures deviceFeatures;
        
        vk::Device device;
        //const std::vector<const char*> deviceExtensions = {
        //    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        //};
        vk::Queue graphicsQueue;
        vk::Queue presentQueue;	
        
        vk::DebugReportCallbackEXT internalCallback;
        function<void()> externalCallback;

        bool GetFeaturesFromList(vector<string> device_features, vk::PhysicalDeviceFeatures &supportedFeatures, vk::PhysicalDeviceFeatures &requestedFeatures);


        // void DestroyDebugReportCallback(VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks* pAllocator);
        // void DestroyInstance();

        // QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);
        // bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
        // bool IsDeviceSuitable(VkPhysicalDevice device);
        // SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);
    };
}
