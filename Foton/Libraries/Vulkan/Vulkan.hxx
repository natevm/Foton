#pragma once
#ifdef _WIN32
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
#endif
#include <vulkan/vulkan.hpp>
#include <thread>
#include <mutex>
#include <future>
#include <vector>
#include <set>
#include <condition_variable>
#include <queue>

#include "Foton/Tools/Singleton.hxx"

namespace Libraries {
    using namespace std;
    class Vulkan : public Singleton
    {
    public:
        static Vulkan* Get();

        std::vector<std::string> get_validation_layers();

        bool create_instance(bool enable_validation_layers = true, 
            set<string> validation_layers = set<string>(),
            set<string> instance_extensions = set<string>(),
            bool use_openvr = false );
        bool destroy_instance();

        bool create_device(
            set<string> device_extensions = set<string>(),
            set<string> device_features = set<string>(),
            uint32_t num_command_pools = 8, 
            vk::SurfaceKHR surface = vk::SurfaceKHR(),
            bool use_openvr = false
        );
        bool destroy_device();

        void register_main_thread();

        /* Accessors */
        vk::Instance get_instance() const;
        vk::PhysicalDevice get_physical_device() const;
        vk::PhysicalDeviceProperties get_physical_device_properties() const;
        vk::PhysicalDeviceLimits get_physical_device_limits() const;
        vk::PhysicalDeviceRayTracingPropertiesNV get_physical_device_ray_tracing_properties() const;
        vk::Device get_device() const;
        uint32_t get_graphics_family() const;
        uint32_t get_present_family() const;
        vk::CommandPool get_graphics_command_pool() const;
        vk::CommandPool get_compute_command_pool() const;
        vk::Queue get_graphics_queue(uint32_t index = 0) const;
        vk::Queue get_present_queue(uint32_t index = 0) const;
        vk::DispatchLoaderDynamic get_dispatch_loader_dynamic() const;
        vk::DispatchLoaderDynamic get_dldi();
        
        /* Memory type search */
        uint32_t find_memory_type(uint32_t typeFilter, vk::MemoryPropertyFlags properties);

        /* Command queues */
        struct CommandQueueItem {
            std::string hint;
            std::vector<vk::SwapchainKHR> swapchains;
            std::vector<uint32_t> swapchain_indices;
            std::vector<vk::PipelineStageFlags> waitDstStageMask;
            std::vector<vk::Semaphore> waitSemaphores;
            std::vector<vk::CommandBuffer> commandBuffers;
            std::vector<vk::Semaphore> signalSemaphores;
            vk::Fence fence;
            uint32_t queue_idx = 0;
            std::shared_ptr<std::promise<void>> promise;
            bool should_free;
        };
        
        std::future<void> enqueue_graphics_commands(CommandQueueItem item);
        std::future<void> enqueue_graphics_commands(
            std::vector<vk::CommandBuffer> commandBuffers, 
            std::vector<vk::Semaphore> waitSemaphores,
            std::vector<vk::PipelineStageFlags> waitDstStageMasks,
            std::vector<vk::Semaphore> signalSemaphores,
            vk::Fence fence,
            std::string hint,
            uint32_t queue_idx);

        std::future<void> enqueue_compute_commands(CommandQueueItem item);
        std::future<void> enqueue_compute_commands(
            std::vector<vk::CommandBuffer> commandBuffers, 
            std::vector<vk::Semaphore> waitSemaphores,
            std::vector<vk::PipelineStageFlags> waitDstStageMasks,
            std::vector<vk::Semaphore> signalSemaphores,
            vk::Fence fence,
            std::string hint,
            uint32_t queue_idx);
        
        std::future<void> enqueue_present_commands(
            std::vector<vk::SwapchainKHR> swapchains, 
            std::vector<uint32_t> swapchain_indices, 
            std::vector<vk::Semaphore> waitSemaphores
        );

        vk::CommandBuffer begin_one_time_graphics_command();
        bool end_one_time_graphics_command(vk::CommandBuffer command_buffer, std::string hint, bool free_after_use = true, uint32_t queue_idx = 0);
        bool end_one_time_graphics_command_immediately(vk::CommandBuffer command_buffer, std::string hint, bool free_after_use = true, uint32_t queue_idx = 0);
        bool submit_graphics_commands();
        bool submit_compute_commands();
        bool submit_present_commands();
        bool flush_queues();

        /* Common queries */
        bool is_ray_tracing_enabled();
        bool is_ASTC_supported();
        bool is_ETC2_supported();
        bool is_BC_supported();
        bool is_draw_indirect_first_instance_supported();
        bool is_multidraw_indirect_supported();

        vk::SampleCountFlags min(vk::SampleCountFlags A, vk::SampleCountFlags B);
        vk::SampleCountFlagBits highest(vk::SampleCountFlags flags);
        vk::SampleCountFlagBits get_closest_sample_count_flag(uint32_t samples);
        vk::SampleCountFlags get_msaa_sample_flags();
        float get_max_anisotropy();
        
    private:
        uint32_t registered_main_thread = -1;
        uint32_t registered_threads = 0; 
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
        int32_t computeFamilyIndex = -1;
        int32_t presentFamilyIndex = -1;
        std::vector<vk::CommandPool> graphicsCommandPools;
        std::vector<vk::CommandPool> computeCommandPools;

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
        vk::PhysicalDeviceRayTracingPropertiesNV deviceRaytracingProperties;
        vk::PhysicalDeviceFeatures deviceFeatures;
        vk::PhysicalDeviceFeatures2 deviceFeatures2;
        
        vk::Device device;
        std::vector<vk::Queue> graphicsQueues;
        std::vector<vk::Queue> computeQueues;
        std::vector<vk::Queue> presentQueues;	
        // vk::SubmitInfo submission;

        std::mutex graphics_queue_mutex;
        std::mutex compute_queue_mutex;
        std::mutex present_queue_mutex;
        std::mutex thread_id_mutex;
        std::mutex end_one_time_command_mutex;
        std::queue<CommandQueueItem> graphicsCommandQueue;
        std::queue<CommandQueueItem> computeCommandQueue;
        std::queue<CommandQueueItem> presentCommandQueue;
        
        vk::DebugReportCallbackEXT internalCallback;
        function<void()> externalCallback;

        bool GetFeaturesFromList(set<string> device_features, vk::PhysicalDeviceFeatures &supportedFeatures);

        uint32_t get_thread_id();
    };
}
