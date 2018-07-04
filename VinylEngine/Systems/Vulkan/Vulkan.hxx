#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vulkan/vulkan.h>

#include "../GLFW/GLFW.hxx"

#include <iostream>
#include <thread>
#include <iostream>
#include <assert.h>
#include <chrono>
#include <future>
#include <vector>
#include <cstring>

namespace Systems {
    using namespace std;
    class Vulkan
    {
    public:
        bool initialized = false;
        bool running = false;

        static Vulkan& Get()
        {
            static Vulkan instance;
            return instance;
        }

        bool initialize() {
            if (initialized == true) return false;
            
            create_vulkan_instance();
            initialized = true;
            return true;
        }

        bool create_vulkan_instance(vector<string> validation_layers = {});

        bool set_validation_callback(std::function<void()> debugCallback) {
            if (validationLayers.size() == 0) return false;
            cout<<"Adding validation layer callback"<<endl;
            // this->externalCallback = callback;
        }

        bool pick_first_capable_device() {
            cout<<"Picking first capable device"<<endl;
            return false;
        }

        bool create_command_pools() {
            cout<<"Creating command pools"<<endl;
            return false;
        }

        bool start() {
            if (running == true || initialized == false) return false;

            auto func = [](future<void> futureObj) {
                while (futureObj.wait_for(chrono::milliseconds(1)) == future_status::timeout)
                {

                }
            };

            exitSignal = promise<void>();
            future<void> futureObj = exitSignal.get_future();
            eventThread = thread(func, move(futureObj));

            running = true;
            return true;
        }

        bool stop() {
            if (running == false) return false;

            exitSignal.set_value();
            eventThread.join();
            
            running = false;
            return true;
        }

    private:
        Vulkan();
        ~Vulkan();
    
        Vulkan(const Vulkan&) = delete;
        Vulkan& operator=(const Vulkan&) = delete;
        Vulkan(Vulkan&&) = delete;
        Vulkan& operator=(Vulkan&&) = delete;
        
        promise<void> exitSignal;
        thread eventThread;

        /* ------------------------------------------*/
        /* VULKAN FIELDS                             */
        /* ------------------------------------------*/
        VkInstance instance;
        vector<const char*> validationLayers;
        VkDebugReportCallbackEXT internalCallback;
        function<void()> externalCallback;

        vector<const char*> GetRequiredExtensions(bool enableValidationLayers) {
            /* Here, we specify that we'll need an extension to render to GLFW. */
            uint32_t glfwExtensionCount = 0;
            const char** glfwExtensions;
            glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

            vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

            /* Conditionally require the following extension, which allows us to get debug output from validation layers */
            if (enableValidationLayers) extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);

            /* Now let's check out the supported vulkan extensions for this platform. */

            /* First, get the total number of supported extensions. */
            uint32_t extensionCount = 0;
            vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

            /*Next, query the extension details */
            vector<VkExtensionProperties> extensionProperties(extensionCount);
            vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensionProperties.data());

            /* Check to see if the extensions we have are what are required by GLFW */
            char** extensionstring = (char**)glfwExtensions;
            for (int i = 0; i < glfwExtensionCount; ++i) {
                string currentextension = string(*extensionstring);
                extensionstring += 1;
                bool extensionFound = false;
                for (int j = 0; j < extensions.size(); ++j)
                    if (currentextension.compare(extensions[j]) == 0)
                        extensionFound = true;
                if (!extensionFound) throw runtime_error("Missing extension " + currentextension);
            }

            return extensions;
        }

        bool CheckValidationLayerSupport(vector<string> validation_layers) {
            /* Checks if all validation layers are supported */
        
            /* Determine how many validation layers are supported*/
            uint32_t layerCount;
            vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

            /* Get the available layers */
            std::vector<VkLayerProperties> availableLayers(layerCount);
            vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

            /* Check to see if the validation layers we have selected are available */
            for (string layerName : validation_layers) {
                bool layerFound = false;

                for (const auto& layerProperties : availableLayers) {
                    if (strcmp(layerName.c_str(), layerProperties.layerName) == 0) {
                        layerFound = true;
                        break;
                    }
                }

                if (!layerFound) return false;
            }

            return true;
        }

        void DestroyDebugReportCallback(VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks* pAllocator) {
            if (validationLayers.size() > 0 && internalCallback != nullptr) {
                auto func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
                if (func != nullptr) {
                    func(instance, internalCallback, pAllocator);
                }
            }
        }
        void DestroyInstance() {
            if (instance) vkDestroyInstance(instance, nullptr);
        }
    };
}
