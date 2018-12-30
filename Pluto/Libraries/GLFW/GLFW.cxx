#include "GLFW.hxx"


void resize_window_callback(GLFWwindow * ptr, int width, int height) {
    auto key = Libraries::GLFW::Get()->get_key_from_ptr(ptr);
    if (key.size() > 0 && (width > 0) && (height > 0)) {
        Libraries::GLFW::Get()->set_swapchain_out_of_date(key);
    }
}

void cursor_position_callback(GLFWwindow * ptr, double xpos, double ypos) {
    auto key = Libraries::GLFW::Get()->get_key_from_ptr(ptr);
    if (key.size() > 0) {
        Libraries::GLFW::Get()->set_cursor_pos(key, xpos, ypos);
    }
}

void mouse_button_callback(GLFWwindow * ptr, int button, int action, int mods)
{
    auto key = Libraries::GLFW::Get()->get_key_from_ptr(ptr);
    if (key.size() > 0) {
        Libraries::GLFW::Get()->set_button_data(key, button, action, mods);
    }
}

namespace Libraries {
    GLFW::GLFW() { }
    GLFW::~GLFW() { }

    GLFW* GLFW::Get() {
        static GLFW instance;
        if (!instance.is_initialized()) instance.initialize();
        return &instance;
    }
    
    bool GLFW::initialize() {
        if (!glfwInit()) {
            cout<<"GLFW failed to initialize!"<<endl;
            return false;
        }
        initialized = true;
        return true;
    }

    unordered_map<string, GLFW::Window> &GLFW::Windows()
    {
        static unordered_map<string, Window> windows;
        return windows;
    }

    bool GLFW::create_window(string key, uint32_t width, uint32_t height, bool floating, bool resizable, bool decorated) {
        /* If uninitialized, or if window already exists, return false */
        if (initialized == false) {
            std::cout << "GLFW: Uninitialized, cannot create window."<<std::endl;
            return false;
        }
        auto ittr = Windows().find(key);
        if ( ittr != Windows().end() ) {
            std::cout << "GLFW: Error, window already exists."<<std::endl;
            return false;
        }

        /* For Vulkan, request no OGL api */
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        glfwWindowHint(GLFW_DECORATED, (decorated) ? GLFW_TRUE : GLFW_FALSE);
        glfwWindowHint(GLFW_RESIZABLE, (resizable) ? GLFW_TRUE : GLFW_FALSE);
        glfwWindowHint(GLFW_FLOATING, (floating) ? GLFW_TRUE : GLFW_FALSE);
        Window window = {};
        auto ptr = glfwCreateWindow(width, height, key.c_str(), NULL, NULL);
        if (!ptr) {
            cout<<"GLFW: Failed to create GLFW window"<<endl;
            return false;
        }

        glfwSetWindowSizeCallback(ptr, &resize_window_callback);
        glfwSetCursorPosCallback(ptr, &cursor_position_callback);
        glfwSetMouseButtonCallback(ptr, &mouse_button_callback);

        window.ptr = ptr;
        Windows()[key] = window;
        return true;
    }

    bool GLFW::resize_window(std::string key, uint32_t width, uint32_t height) {
        /* If uninitialized, or if window already exists, return false */
        if (initialized == false) {
            std::cout << "GLFW: Uninitialized, cannot resize window."<<std::endl;
            return false;
        }
        auto ittr = Windows().find(key);
        if ( ittr == Windows().end() ) {
            std::cout << "GLFW: Error, window does not exists."<<std::endl;
            return false;
        }

        auto window = Windows()[key];
        glfwSetWindowSize(window.ptr, width, height);
        return true;
    }

    bool GLFW::set_window_visibility(std::string key, bool visible) {
        if (initialized == false) {
            std::cout << "GLFW: Uninitialized, cannot set window visibility."<<std::endl;
            return false;
        }
        auto ittr = Windows().find(key);
        if ( ittr == Windows().end() ) {
            std::cout << "GLFW: Error, window does not exists."<<std::endl;
            return false;
        }

        auto window = Windows()[key];
        if (visible)
            glfwShowWindow(window.ptr);
        else 
            glfwHideWindow(window.ptr);
        return true;
    }

    bool GLFW::set_window_pos(std::string key, uint32_t x, uint32_t y) {
        /* If uninitialized, or if window already exists, return false */
        if (initialized == false) {
            std::cout << "GLFW: Uninitialized, cannot set window pos."<<std::endl;
            return false;
        }
        auto ittr = Windows().find(key);
        if ( ittr == Windows().end() ) {
            std::cout << "GLFW: Error, window does not exists."<<std::endl;
            return false;
        }

        auto window = Windows()[key];
        glfwSetWindowPos(window.ptr, x, y);
        return true;
    }

    bool GLFW::destroy_window(string key) {
        auto vulkan = Libraries::Vulkan::Get();
        auto device = vulkan->get_device();

        if (initialized == false) { 
            std::cout << "GLFW: Uninitialized, cannot destroy window."<<std::endl;
            return false;
        }
        auto ittr = Windows().find(key);
        if ( ittr == Windows().end() ) {
            std::cout << "GLFW: Error, window does not exist."<<std::endl;
            return false;
        }
        auto window = Windows()[key];
        
        if (window.swapchain) {
            device.destroySwapchainKHR(window.swapchain);
            return false;
        }

        glfwDestroyWindow(window.ptr);
        Windows().erase(key);
        return true;
    }

    std::string GLFW::get_key_from_ptr(GLFWwindow* ptr) {
        if (initialized == false) { 
            std::cout << "GLFW: Uninitialized, cannot get window key from ptr."<<std::endl;
            return "";
        }

        for (auto &window : Windows()) {
            if (window.second.ptr == ptr)
                return window.first;
        }
        return "";
    }

    std::vector<std::string> GLFW::get_window_keys() {
        std::vector<std::string> result;

        /* If not initialized, or window doesnt exist, return false. */
        if (initialized == false) { 
            std::cout << "GLFW: Uninitialized, cannot get window keys."<<std::endl;
            return result;
        }

        for (auto &window : Windows()) {
            result.push_back(window.first);
        }
        return result;
    }

    bool GLFW::does_window_exist(string key) {
        /* If not initialized, or window doesnt exist, return false. */
        if (initialized == false) { 
            std::cout << "GLFW: Uninitialized, cannot query window existance."<<std::endl;
            return false;
        }
        auto window = Windows().find(key);
        return ( window != Windows().end() );
    }

    bool GLFW::poll_events() {
        /* If not initialized, or window doesnt exist, return false. */
        if (initialized == false) { 
            std::cout << "GLFW: Uninitialized, cannot poll events."<<std::endl;
            return false;
        }

        if (should_close()) return false;

        glfwPollEvents();

        for (auto &i : Windows()) {
            if (glfwWindowShouldClose(i.second.ptr)) {
                glfwDestroyWindow(i.second.ptr);
                i.second.ptr = nullptr;
                Windows().erase(i.first);
                /* Break here required. Erase modifies iterator used by for loop. */
                break;
            }
        }
        
        return true;
    }

    bool GLFW::wait_events() {
        if (initialized == false) { 
            std::cout << "GLFW: Uninitialized, cannot wait events."<<std::endl;
            return false;
        }

        glfwWaitEvents();
        return true;
    }

    bool GLFW::post_empty_event() {
        /* If not initialized, or window doesnt exist, return false. */
        if (initialized == false) { 
            std::cout << "GLFW: Uninitialized, cannot post an empty event."<<std::endl;
            return false;
        }

        glfwPostEmptyEvent();
        return true;
    }

    bool GLFW::should_close() {
        if (initialized == false) return true;
        if (Windows().size() == 0) return true;
        return false;
    }

    vk::SurfaceKHR GLFW::create_vulkan_surface(const Libraries::Vulkan *vulkan, std::string key) 
    {
        /* vkInstance + GLFWWindow => vkSurfaceKHR */

        if (initialized == false) {
            std::cout << "GLFW: Not initialized, can't create vulkan surface." << std::endl;
            return vk::SurfaceKHR();
        }
        
        if (!does_window_exist(key)) {
            std::cout << "GLFW: Window with key " << key << " missing. Can't create vulkan surface." << std::endl;
            return vk::SurfaceKHR();
        }

        auto instance = vulkan->get_instance();

        /* If the surface already exists, just return the pre-created surface */
        std::cout<<"Creating vulkan surface for window: " << key <<std::endl;
        vk::SurfaceKHR surface{};
        auto psurf = VkSurfaceKHR(surface);
        glfwCreateWindowSurface((VkInstance)instance, Windows().at(key).ptr, NULL, &psurf);

        Windows().at(key).surface = psurf;
        return psurf;
    }

    bool GLFW::create_vulkan_swapchain(std::string key)
    {
        auto vulkan = Libraries::Vulkan::Get();

        /*
            VkSurfaceKHR + imageCount + surface format and color space + present mode => vkSwapChainKHR
            depth attachment + color attachment + subpass info => renderpass
        */
        #pragma region ErrorChecking
        if (initialized == false) {
            std::cout << "GLFW: Not initialized, can't create vulkan swapchain." << std::endl;
            return false;
        }
        
        if (!does_window_exist(key)) {
            std::cout << "GLFW: Window with key " << key << " missing. Can't create vulkan swapchain." << std::endl;
            return false;
        }

        if (!vulkan->is_initialized()) {
            std::cout << "GLFW: Vulkan library not initialized. Can't create vulkan swapchain." << std::endl;
            return false;
        }
        #pragma endregion

        Window &window = Windows().at(key);

        if (!window.surface) {
            create_vulkan_surface(vulkan, key);
            if (!window.surface) return false;
        }

        auto physicalDevice = vulkan->get_physical_device();
        auto device = vulkan->get_device();
        auto graphicsFamily = vulkan->get_graphics_family();
        auto presentFamily = vulkan->get_present_family();
        auto result = physicalDevice.getSurfaceSupportKHR(presentFamily, window.surface);
        
        window.surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(window.surface);
        std::vector<vk::SurfaceFormatKHR> surfaceFormats = physicalDevice.getSurfaceFormatsKHR(window.surface);
        std::vector<vk::PresentModeKHR> presentModes = physicalDevice.getSurfacePresentModesKHR(window.surface);

        /* To make these resources available from the component manager, we'll fill out this struct and register it. */
        #pragma region QueryFormat
        /* Try to use SRGB, which has a more accurate color space. */

        /* Select the first supported format by default */
        if (surfaceFormats.size() >= 1) {
            if (surfaceFormats[0].format == vk::Format::eUndefined) {
                window.surfaceFormat.format = vk::Format::eR8G8B8A8Unorm;
                window.surfaceFormat.colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
            } else {
                window.surfaceFormat = surfaceFormats[0];
            }
        }

        /* Switch to a prefered format if we can. */
        for (const auto& surfaceFormat : surfaceFormats) {
            if (surfaceFormat.format == vk::Format::eR8G8B8A8Unorm 
                && surfaceFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
                window.surfaceFormat = surfaceFormat;
            }
        }
        #pragma endregion
        #pragma region QueryPresentationMode
        /* Now, choose a presentation mode. 
            There are four possible presentation modes
            VK_PRESENT_MODE_IMMEDIATE_KHR - submit images right away. Might tear. 
            VK_PRESENT_MODE_FIFO_KHR - display takes image on refresh. Most similar to v-sync
            VK_PRESENT_MODE_FIFO_RELAXED_KHR - similar to last. If queue is empty, submit image right away. Might tear.
            VK_PRESENT_MODE_MAILBOX_KHR - Instead of waiting for queue to empty, replace old images with new. Can be used for tripple buffering
        */

        /* Prefer mailbox mode */
    
        /* Only this one is guaranteed, so by default use that.*/
        window.presentMode = vk::PresentModeKHR::eImmediate;

        /* Switch to prefered present mode if we can. */
        for (const auto& presentMode : presentModes) {
            if (presentMode == vk::PresentModeKHR::eMailbox) {
                window.presentMode = presentMode;
            }
        }
        #pragma endregion
        #pragma region QuerySurfaceExtent
        /* Determine the extent of our swapchain (almost always the same as our windows size.) */
        window.surfaceExtent = window.surfaceCapabilities.currentExtent;
        if (window.surfaceCapabilities.currentExtent.width == std::numeric_limits<uint32_t>::max()) {
            uint32_t width, height;
            glfwGetWindowSize(window.ptr, (int*)&width, (int*)&height);
            window.surfaceExtent = { width, height };
            window.surfaceExtent.width = std::max(window.surfaceCapabilities.minImageExtent.width, 
                std::min(window.surfaceCapabilities.maxImageExtent.width, window.surfaceExtent.width));
            window.surfaceExtent.height = std::max(window.surfaceCapabilities.minImageExtent.height, 
                std::min(window.surfaceCapabilities.maxImageExtent.height, window.surfaceExtent.height));
        }
        #pragma endregion
        #pragma region QueryImageCount
        /* Check to see how many images we can submit to the swapchain. 0 means within memory limits. */
        uint32_t imageCount = window.surfaceCapabilities.minImageCount + 1;
        if (window.surfaceCapabilities.maxImageCount > 0 && imageCount > window.surfaceCapabilities.maxImageCount) {
            imageCount = window.surfaceCapabilities.maxImageCount;
        }
        #pragma endregion
        #pragma region CreateSwapchain
        /* Create the swapchain */
        if (window.swapchain) {
            device.destroySwapchainKHR(window.swapchain);
        }

        auto info = vk::SwapchainCreateInfoKHR();
        info.surface = window.surface;
        info.minImageCount = imageCount;
        info.imageFormat = window.surfaceFormat.format;
        info.imageColorSpace = window.surfaceFormat.colorSpace;
        info.imageExtent = window.surfaceExtent;
        info.imageArrayLayers = 1; // For now...
        info.imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst; // What we'll use the images for. Here, just directly rendering them

        /* Query for our present and graphics queues */
        uint32_t queueFamilyIndices[] = { graphicsFamily, presentFamily };

        /* Handle the special case that our graphics and present queues are seperate */
        if (graphicsFamily != presentFamily) {
            info.imageSharingMode = vk::SharingMode::eConcurrent;
            info.queueFamilyIndexCount = 2;
            info.pQueueFamilyIndices = queueFamilyIndices;
        }
        else {
            info.imageSharingMode = vk::SharingMode::eExclusive;
            info.queueFamilyIndexCount = 0; // Optional
            info.pQueueFamilyIndices = nullptr; // Optional
        }

        /* We can specify a particular transform to happen to images in the swapchain. */
        info.preTransform = window.surfaceCapabilities.currentTransform;

        /* We could also blend the contents of the window with other windows on the system. */
        info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;

        /* from chooseSwapPresentMode */
        info.presentMode = window.presentMode;
    
        /* Clip pixels which are obscured by other windows. */
        info.clipped = VK_TRUE;
       
        /* Create the swap chain */
        std::cout<<"Creating vulkan swapchain for window: " << key <<std::endl;        
        window.swapchain = device.createSwapchainKHR(info);

        if (!window.swapchain) {
            std::cout << "GLFW: Failed to create swapchain." << std::endl;
            return false;
        }

        /* Retrieve handles to the swap chain images */
        window.swapchainColorImages = device.getSwapchainImagesKHR(window.swapchain);

        #pragma endregion

        for (uint32_t i = 0; i < imageCount; ++i) {
            Texture::Data data = {};
            data.colorFormat = window.surfaceFormat.format;
            data.colorImage = window.swapchainColorImages[i];
            data.colorImageLayout = vk::ImageLayout::ePresentSrcKHR;
            data.width = window.surfaceExtent.width;
            data.height = window.surfaceExtent.height;
            data.depth = 1;
            data.colorMipLevels = 1;
            data.layers = 1;
            data.viewType = vk::ImageViewType::e2D;
            data.imageType = vk::ImageType::e2D;
            
            if (window.textures.size() <= i)
                window.textures.push_back(Texture::CreateFromExternalData(key + std::to_string(i), data));
            else
                window.textures[i]->setData(data);

            /* Transition to a usable format */
            vk::ImageSubresourceRange subresourceRange;
            subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
            subresourceRange.baseMipLevel = 0;
            subresourceRange.levelCount = 1;
            subresourceRange.baseArrayLayer = 0;
            subresourceRange.layerCount = 1;

            vk::CommandBuffer cmdBuffer = vulkan->begin_one_time_graphics_command(0);
            window.textures[i]->setImageLayout( cmdBuffer, data.colorImage, vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR, subresourceRange);
            vulkan->end_one_time_graphics_command(cmdBuffer, 0, true, true);
        }
        
        window.swapchain_out_of_date = false;
        return true;
    }

    void GLFW::set_swapchain_out_of_date(std::string key) {
        auto it = Windows().find(key);
        if (it == Windows().end()) {
            std::cout<<"Window " << key << "does not exist, cannot mark swapchain as out of date"<<std::endl;
            return;
        }

        auto window = Windows()[key].swapchain_out_of_date = true;
    }

    bool GLFW::is_swapchain_out_of_date(std::string key) {
        auto it = Windows().find(key);
        if (it == Windows().end()) {
            std::cout<<"Window " << key << "does not exist, cannot checkk if swapchain is out of date"<<std::endl;
            return false;
        }

        auto window = Windows()[key];
        return window.swapchain_out_of_date;
    }

    vk::SwapchainKHR GLFW::get_swapchain(std::string key) {
        auto it = Windows().find(key);
        if (it == Windows().end())
            return vk::SwapchainKHR();

        auto window = Windows()[key];
        return window.swapchain;
    }

    Texture* GLFW::get_texture(std::string key, uint32_t index) {
        auto it = Windows().find(key);
        if (it == Windows().end())
            return nullptr;

        auto window = Windows()[key];
        if (window.textures.size() > index)
            return window.textures[index];
        return nullptr;
    }

    bool GLFW::set_cursor_pos(std::string key, double xpos, double ypos) {
        if (initialized == false) {
            std::cout << "GLFW: Uninitialized, cannot set cursor position."<<std::endl;
            return false;
        }
        auto ittr = Windows().find(key);
        if ( ittr == Windows().end() ) {
            std::cout << "GLFW: Error, window does not exists."<<std::endl;
            return false;
        }

        auto window = &Windows()[key];
        window->xpos = xpos;
        window->ypos = ypos;
        return true;
    }

    std::vector<double> GLFW::get_cursor_pos(std::string key) {
        if (initialized == false) {
            std::cout << "GLFW: Uninitialized, cannot get cursor position."<<std::endl;
            return {0.0,0.0};
        }
        auto ittr = Windows().find(key);
        if ( ittr == Windows().end() ) {
            std::cout << "GLFW: Error, window does not exists."<<std::endl;
            return {0.0,0.0};
        }
        auto window = &Windows()[key];
        return {window->xpos, window->ypos};
    }

    bool GLFW::set_button_data(std::string key, int button, int action, int mods) {
        if (initialized == false) {
            std::cout << "GLFW: Uninitialized, cannot set button data."<<std::endl;
            return false;
        }
        auto ittr = Windows().find(key);
        if ( ittr == Windows().end() ) {
            std::cout << "GLFW: Error, window does not exists."<<std::endl;
            return false;
        }
        
        if ((button >= 7) || (button < 0)) {
            std::cout << "GLFW: Error, Button must be between 0 and 7."<<std::endl;
            return false;
        }

        auto window = &Windows()[key];
        window->buttons[button].action = action;
        window->buttons[button].mods = mods;
        return true;
    }

    int GLFW::get_button_action(std::string key, int button) {
        if (initialized == false) {
            std::cout << "GLFW: Uninitialized, cannot get button action."<<std::endl;
            return false;
        }
        auto ittr = Windows().find(key);
        if ( ittr == Windows().end() ) {
            std::cout << "GLFW: Error, window does not exists."<<std::endl;
            return false;
        }
        
        if ((button >= 7) || (button < 0)) {
            std::cout << "GLFW: Error, Button must be between 0 and 7."<<std::endl;
            return false;
        }

        auto window = &Windows()[key];
        return window->buttons[button].action;
    }

    int GLFW::get_button_mods(std::string key, int button) {
        if (initialized == false) {
            std::cout << "GLFW: Uninitialized, cannot get button mods."<<std::endl;
            return false;
        }
        auto ittr = Windows().find(key);
        if ( ittr == Windows().end() ) {
            std::cout << "GLFW: Error, window does not exists."<<std::endl;
            return false;
        }
        
        if ((button >= 7) || (button < 0)) {
            std::cout << "GLFW: Error, Button must be between 0 and 7."<<std::endl;
            return false;
        }

        auto window = &Windows()[key];
        return window->buttons[button].mods;
    }
}