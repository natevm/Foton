#include "Vulkan.hxx"


namespace Systems {
    VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback (
        VkDebugReportFlagsEXT flags,
        VkDebugReportObjectTypeEXT objType,
        uint64_t obj,
        size_t location,
        int32_t code,
        const char* layerPrefix,
        const char* msg,
        void* userData) {

        std::cerr << "validation layer: " << msg << std::endl;
        throw std::runtime_error("validation layer: " + std::string(msg));
        return VK_FALSE;
    }

    Vulkan::Vulkan() { }
    
    Vulkan::~Vulkan() {
        DestroyDebugReportCallback(instance, internalCallback, nullptr);
        DestroyInstance();
    }

    bool Vulkan::create_vulkan_instance(vector<string> validation_layers) 
    {
        cout<<"Creating vulkan instance"<<endl;
    
        /* Application Info */
        VkApplicationInfo appInfo = {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "TEMPORARY APPLICATION NAME";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 1, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 1, 0);
        appInfo.apiVersion = VK_MAKE_VERSION(1, 1, 0);
    
        /* Instance Info: Specifies global extensions and validation layers we'd like to use */
        VkInstanceCreateInfo createInstanceInfo = {};
        createInstanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInstanceInfo.pApplicationInfo = &appInfo;

        /* Determine the required extensions */
        for(auto& string : validation_layers) validationLayers.push_back(&string.front());
        auto extensions = GetRequiredExtensions(validationLayers.size() > 0);
        createInstanceInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInstanceInfo.ppEnabledExtensionNames = extensions.data();

        /* Check to see if we support the requested validation layers */
        if (validation_layers.size() > 0 && !CheckValidationLayerSupport(validation_layers)) {
            throw runtime_error("validation layers requested, but not available!");
        }

        /* Here, we specify the validation layers to use. */
        if (validation_layers.size() > 0) {
            createInstanceInfo.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
            createInstanceInfo.ppEnabledLayerNames = validationLayers.data();
        }
        else
            createInstanceInfo.enabledLayerCount = 0;
    
        /* Now we have everything we need to create a vulkan instance!*/
        // (struct pointer to create object, custom allocator callback, and a pointer to a vulkan instance)
        if (vkCreateInstance(&createInstanceInfo, nullptr, &instance) != VK_SUCCESS) {
            throw runtime_error("failed to create instance!");
        }

        /* Add an internal callback for the validation layers */
        if (validation_layers.size() > 0) {
            //the debug callback in Vulkan is managed with a handle that needs to be explicitly created and destroyed
            VkDebugReportCallbackCreateInfoEXT createCallbackInfo = {};
            createCallbackInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
            createCallbackInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
            createCallbackInfo.pfnCallback = DebugCallback;

            /* The function to assign a callback for validation layers isn't loaded by default. Here, we get the function. */
            auto func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
            if (func != nullptr) {
                return func(instance, &createCallbackInfo, nullptr, &internalCallback);
            }
            else {
                cout<<"vkCreateDebugReportCallbackEXT not present! "<<endl;
                return false;
            }
        }

        return true;
    }

}