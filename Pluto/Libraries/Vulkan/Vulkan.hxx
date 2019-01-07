#pragma once
#include <vulkan/vulkan.hpp>
#include <thread>
#include <future>
#include <vector>
#include <set>
#include <condition_variable>

#include "Pluto/Tools/Singleton.hxx"
#include "threadsafe_queue.hpp"


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
            uint32_t num_command_pools = 8, 
            vk::SurfaceKHR surface = vk::SurfaceKHR()
        );
        bool destroy_device();


        vk::Instance get_instance() const;
        vk::PhysicalDevice get_physical_device() const;
        vk::PhysicalDeviceProperties get_physical_device_properties() const;
        vk::Device get_device() const;
        uint32_t get_graphics_family() const;
        uint32_t get_present_family() const;
        vk::CommandPool get_command_pool(uint32_t index = 0) const;
        vk::Queue get_graphics_queue(uint32_t index = 0) const;
        vk::Queue get_present_queue(uint32_t index = 0) const;
        vk::DispatchLoaderDynamic get_dispatch_loader_dynamic() const;
        
        uint32_t find_memory_type(uint32_t typeFilter, vk::MemoryPropertyFlags properties);
        
        std::future<void> enqueue_graphics_commands(vk::SubmitInfo submit_info, vk::Fence fence);
        std::future<void> enqueue_present_commands(vk::PresentInfoKHR presentInfo);
        bool submit_graphics_commands();
        bool submit_present_commands();
        bool is_ray_tracing_enabled();

        vk::SampleCountFlags min(vk::SampleCountFlags A, vk::SampleCountFlags B);
        vk::SampleCountFlagBits highest(vk::SampleCountFlags flags);
        vk::SampleCountFlagBits get_closest_sample_count_flag(uint32_t samples);
        vk::SampleCountFlags get_msaa_sample_flags();

        vk::CommandBuffer begin_one_time_graphics_command(uint32_t pool_id = 0);
        bool end_one_time_graphics_command(vk::CommandBuffer command_buffer, uint32_t pool_id = 0, bool free_after_use = true, bool submit_immediately = false);

    private:
        bool validationEnabled = true;
        bool rayTracingEnabled = false;
        vk::SampleCountFlags supportedMSAASamples;
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
        std::vector<vk::CommandPool> commandPools;

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
        std::vector<vk::Queue> graphicsQueues;
        std::vector<vk::Queue> presentQueues;	

        struct CommandQueueItem {
            vk::PresentInfoKHR presentation;
            vk::SubmitInfo submission;
            vk::Fence fence;
            std::shared_ptr<std::promise<void>> promise;
            bool should_free;
        };

        // struct CommandQueue {
        //     std::vector<> presentations;
        //     std::vector<vk::SubmitInfo> submissions;
        //     std::vector<vk::Fence> fences;
        //     threadsafe_queue<std::shared_ptr<std::promise<void>>> promises;
        //     std::vector<bool> should_free;
        // };

        threadsafe_queue<CommandQueueItem> graphicsCommandQueue;
        threadsafe_queue<CommandQueueItem> presentCommandQueue;
        
        vk::DebugReportCallbackEXT internalCallback;
        function<void()> externalCallback;

        bool GetFeaturesFromList(vector<string> device_features, vk::PhysicalDeviceFeatures &supportedFeatures, vk::PhysicalDeviceFeatures &requestedFeatures);
    };
}
