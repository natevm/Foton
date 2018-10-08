#include "GLFW.hxx"

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

    bool GLFW::create_window(string key) {
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
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        Window window = {};
        auto ptr = glfwCreateWindow(1920, 1080, "test", NULL, NULL);
        if (!ptr) {
            cout<<"GLFW: Failed to create GLFW window"<<endl;
            return false;
        }
        window.ptr = ptr;
        Windows()[key] = window;
        return true;
    }

    bool GLFW::destroy_window(string key) {
        /* If not initialized, or window doesnt exist, return false. */
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
        glfwDestroyWindow(window.ptr);
        Windows().erase(key);
        return true;
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

        glfwPollEvents();

        for (auto i : Windows()) {
            if (glfwWindowShouldClose(i.second.ptr)) {
                glfwDestroyWindow(i.second.ptr);
                Windows().erase(i.first);
                break;
            }
        }
        
        return true;
    }

    bool GLFW::wait_events() {
        /* If not initialized, or window doesnt exist, return false. */
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

        if (!vulkan->is_initialized()) {
            std::cout << "GLFW: Vulkan library not initialized. Can't create vulkan surface." << std::endl;
            return vk::SurfaceKHR();
        }
        auto instance = vulkan->get_instance();

        /* If the surface already exists, just return the pre-created surface */
        
        vk::SurfaceKHR surface{};
        auto psurf = VkSurfaceKHR(surface);
        glfwCreateWindowSurface((VkInstance)instance, Windows().at(key).ptr, NULL, &psurf);

        Windows().at(key).surface = psurf;
        return psurf;
    }

    bool GLFW::create_vulkan_resources(const Libraries::Vulkan *vulkan, std::string key)
    {
        std::cout<<"Creating vulkan resources for " << key <<std::endl;
        /*
            VkSurfaceKHR + imageCount + surface format and color space + present mode => vkSwapChainKHR
            depth attachment + color attachment + subpass info => renderpass
        */
        #pragma region ErrorChecking
        if (initialized == false) {
            std::cout << "GLFW: Not initialized, can't create vulkan resources." << std::endl;
            return false;
        }
        
        if (!does_window_exist(key)) {
            std::cout << "GLFW: Window with key " << key << " missing. Can't create vulkan resources." << std::endl;
            return false;
        }

        if (!vulkan->is_initialized()) {
            std::cout << "GLFW: Vulkan library not initialized. Can't create vulkan resources." << std::endl;
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
        auto command_pool = vulkan->get_command_pool();
        auto result = physicalDevice.getSurfaceSupportKHR(presentFamily, window.surface);

        window.surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(window.surface);
        std::vector<vk::SurfaceFormatKHR> surfaceFormats = physicalDevice.getSurfaceFormatsKHR(window.surface);
        std::vector<vk::PresentModeKHR> presentModes = physicalDevice.getSurfacePresentModesKHR(window.surface);
        
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
        info.imageUsage = vk::ImageUsageFlagBits::eColorAttachment; // What we'll use the images for. Here, just directly rendering them

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
        window.swapchain = device.createSwapchainKHR(info);

        if (!window.swapchain) {
            std::cout << "GLFW: Failed to create swapchain." << std::endl;
            return false;
        }

        window.imageIndex = 0;

        /* Retrieve handles to the swap chain images */
        window.swapchainColorImages = device.getSwapchainImagesKHR(window.swapchain);

        #pragma endregion
        #pragma region ColorAttachment
        /* Create a renderpass for the swapchain */
        vk::AttachmentDescription colorAttachment;
        colorAttachment.format = window.surfaceFormat.format;
        colorAttachment.samples = vk::SampleCountFlagBits::e1;
        colorAttachment.loadOp = vk::AttachmentLoadOp::eClear; // clears image to black
        colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
        colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
        colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
        colorAttachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

        vk::AttachmentReference colorAttachmentRef;
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;
        #pragma endregion
        #pragma region DepthFormat
        /* Determine the prefered depth format. */
        std::vector<vk::Format> candidates = { vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint };
        vk::ImageTiling tiling = vk::ImageTiling::eOptimal;
        vk::FormatFeatureFlagBits features = vk::FormatFeatureFlagBits::eDepthStencilAttachment;
        
        for (vk::Format format : candidates) {
            vk::FormatProperties props = physicalDevice.getFormatProperties(format);
            if ((props.optimalTilingFeatures & features) == features) {
                window.depthFormat = format; 
                break;
            }
        }

        if (window.depthFormat == vk::Format()) {
            /* Depth format not found. Complain. */
            std::cout << "GLFW: Failed to find suitable depth format." << std::endl;
            return false;
        }
        #pragma endregion

        /* At this point we should have depth format. Create depth resources */
        window.swapchainDepthImages.resize(window.swapchainColorImages.size());
        window.swapchainDepthImageMemory.resize(window.swapchainColorImages.size());
        for (int i = 0; i < window.swapchainColorImages.size(); ++i) {
            #pragma region CreateDepthImage
            if (window.swapchainDepthImages[i])
                device.destroyImage(window.swapchainDepthImages[i]);

            vk::ImageCreateInfo info;
            info.imageType = vk::ImageType::e2D;
            info.extent.width = window.surfaceExtent.width;
            info.extent.height = window.surfaceExtent.height;
            info.extent.depth = 1;
            info.mipLevels = 1;
            info.arrayLayers = 1;
            info.format = window.depthFormat;
            info.tiling = vk::ImageTiling::eOptimal;
            info.initialLayout = vk::ImageLayout::eUndefined;
            info.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
            info.samples = vk::SampleCountFlagBits::e1;
            info.sharingMode = vk::SharingMode::eExclusive;
            window.swapchainDepthImages[i] = device.createImage(info);

            if (!window.swapchainDepthImages[i]) {
                std::cout << "GLFW: Failed to create swapchain depth image." << std::endl;
                return false;
            }
            #pragma endregion
            #pragma region CreateDepthMemory
            if (window.swapchainDepthImageMemory[i])
                device.freeMemory(window.swapchainDepthImageMemory[i]);

            vk::MemoryRequirements memRequirements = device.getImageMemoryRequirements(window.swapchainDepthImages[i]);

            vk::MemoryAllocateInfo allocInfo;
            allocInfo.allocationSize = memRequirements.size;
            
            vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();
            
            /* Try to find some memory that matches the type we'd like. */
            for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
                if ((memRequirements.memoryTypeBits & (1 << i)) && 
                    (memProperties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) == vk::MemoryPropertyFlagBits::eDeviceLocal) {
                    allocInfo.memoryTypeIndex = i;
                    break;
                }
                if (i == memProperties.memoryTypeCount - 1) {
                    std::cout << "GLFW: Failed to find suitable memory type for depth image." << std::endl; 
                    return false;
                }
            }

            window.swapchainDepthImageMemory[i] = device.allocateMemory(allocInfo);

            if (!window.swapchainDepthImageMemory[i]) {
                std::cout << "GLFW: Failed to allocate depth image memory." << std::endl;
                return false;
            }

            device.bindImageMemory(window.swapchainDepthImages[i], window.swapchainDepthImageMemory[i], 0);
            #pragma endregion
        }
        
        #pragma region CreateDepthAttachment
        vk::AttachmentDescription depthAttachment;
        depthAttachment.format = window.depthFormat;
        depthAttachment.samples = vk::SampleCountFlagBits::e1;
        depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
        depthAttachment.storeOp = vk::AttachmentStoreOp::eDontCare;
        depthAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
        depthAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        depthAttachment.initialLayout = vk::ImageLayout::eUndefined;
        depthAttachment.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

        vk::AttachmentReference depthAttachmentRef;
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
        #pragma endregion
        #pragma region CreateSubpass
        vk::SubpassDescription subpass;
        subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;

        vk::SubpassDependency dependency;
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        dependency.srcAccessMask = vk::AccessFlagBits::eColorAttachmentRead; // Was previously zero?
        dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
        #pragma endregion
        #pragma region CreateRenderPass
        if (window.renderPass)
            device.destroyRenderPass(window.renderPass);

        /* Create the render pass */
        std::array<vk::AttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
        vk::RenderPassCreateInfo renderPassInfo;
        renderPassInfo.attachmentCount = (uint32_t) attachments.size();
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;
        
        vk::RenderPass renderPass = device.createRenderPass(renderPassInfo);

        if (renderPass == vk::RenderPass()) {
            std::cout << "GLFW: Not initialized, can't create vulkan resources." << std::endl;
            return false;
        }
        window.renderPass = renderPass;
        #pragma endregion

        /* Create a framebuffer for each swap chain image. */
        window.swapchainColorImageViews.resize(window.swapchainColorImages.size());
        window.swapchainDepthImageViews.resize(window.swapchainColorImages.size());
        window.swapchainFramebuffers.resize(window.swapchainColorImages.size());
        window.drawCmdBufferFences.resize(window.swapchainColorImages.size());
        for (int i = 0; i < window.swapchainColorImages.size(); ++i) {
            #pragma region CreateImageViews
            if (window.swapchainColorImageViews[i])
                device.destroyImageView(window.swapchainColorImageViews[i]);

            auto cinfo = vk::ImageViewCreateInfo();
            cinfo.image = window.swapchainColorImages[i];
            cinfo.viewType = vk::ImageViewType::e2D;
            cinfo.format = window.surfaceFormat.format;
            cinfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
            cinfo.subresourceRange.baseMipLevel = 0;
            cinfo.subresourceRange.levelCount = 1;
            cinfo.subresourceRange.baseArrayLayer = 0;
            cinfo.subresourceRange.layerCount = 1;
            window.swapchainColorImageViews[i] = device.createImageView(cinfo);

            if (window.swapchainDepthImageViews[i])
                device.destroyImageView(window.swapchainDepthImageViews[i]);
            auto dinfo = vk::ImageViewCreateInfo();
            dinfo.image = window.swapchainDepthImages[i];
            dinfo.viewType = vk::ImageViewType::e2D;
            dinfo.format = window.depthFormat;
            dinfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
            dinfo.subresourceRange.baseMipLevel = 0;
            dinfo.subresourceRange.levelCount = 1;
            dinfo.subresourceRange.baseArrayLayer = 0;
            dinfo.subresourceRange.layerCount = 1;
            window.swapchainDepthImageViews[i] = device.createImageView(dinfo);

            if (!window.swapchainColorImageViews[i] || !window.swapchainDepthImageViews[i]) {
                std::cout<<"GLFW: Failed to create image view for swapchain!"<<std::endl;
                return false;
            }
            #pragma endregion
            #pragma region CreateFramebuffer
            if (window.swapchainFramebuffers[i])
                device.destroyFramebuffer(window.swapchainFramebuffers[i]);
            std::array<vk::ImageView, 2> attachments = {
				window.swapchainColorImageViews[i],
				window.swapchainDepthImageViews[i]
			};

			vk::FramebufferCreateInfo framebufferInfo;
			framebufferInfo.renderPass = renderPass;
			framebufferInfo.attachmentCount = 2;
			framebufferInfo.pAttachments = attachments.data();
			framebufferInfo.width = window.surfaceExtent.width;
			framebufferInfo.height = window.surfaceExtent.height;
			framebufferInfo.layers = 1;
		
            window.swapchainFramebuffers[i] = device.createFramebuffer(framebufferInfo);
			if (!window.swapchainFramebuffers[i]) {
                std::cout << "GLFW: Failed to create framebuffer." << std::endl;
                return false;
			}
            #pragma endregion

            /* Create a fence for each command buffer, so we know when it's not pending. */
            if (window.drawCmdBufferFences[i])
                device.destroyFence(window.drawCmdBufferFences[i]);
            vk::FenceCreateInfo fenceInfo;
            fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;
            window.drawCmdBufferFences[i] = device.createFence(fenceInfo);
        }

        #pragma region CreateCommandBuffers
        if (window.drawCmdBufferFences[0])
            device.freeCommandBuffers(command_pool, window.drawCmdBuffers);
        vk::CommandBufferAllocateInfo allocInfo;
        allocInfo.commandPool = vulkan->get_command_pool();
        allocInfo.level = vk::CommandBufferLevel::ePrimary;
        allocInfo.commandBufferCount = (uint32_t)window.swapchainColorImages.size();
        window.drawCmdBuffers = device.allocateCommandBuffers(allocInfo);
        if (window.drawCmdBuffers.size() != window.swapchainColorImages.size()) {
            std::cout << "GLFW: Failed to allocate command buffers." << std::endl;
            return false;
        }
        #pragma endregion

        vk::SemaphoreCreateInfo semaphoreInfo;
        if (window.imageAvailableSemaphore) device.destroySemaphore(window.imageAvailableSemaphore);
        if (window.renderCompleteSemaphore) device.destroySemaphore(window.renderCompleteSemaphore);
        if (window.presentCompleteSemaphore) device.destroySemaphore(window.presentCompleteSemaphore);
        window.imageAvailableSemaphore = device.createSemaphore(semaphoreInfo);
        window.renderCompleteSemaphore = device.createSemaphore(semaphoreInfo);
        window.presentCompleteSemaphore = device.createSemaphore(semaphoreInfo);
        return true;
    }

    bool GLFW::acquire_images(const Libraries::Vulkan *vulkan) {
        #pragma region ErrorChecking
        if (initialized == false) {
            std::cout << "GLFW: Not initialized, can't acquire images." << std::endl;
            return false;
        }
        
        if (!vulkan->is_initialized()) {
            std::cout << "GLFW: Vulkan library not initialized. Can't acquire images." << std::endl;
            return false;
        }
        #pragma endregion

        auto device = vulkan->get_device();

        vector<vk::SwapchainKHR> swapchains;
        vector<uint32_t> imageIndexes;

        for (auto &i : Windows()) {
            if (!i.second.swapchain) {
                create_vulkan_resources(vulkan, i.first);
            }

            try {
                i.second.imageIndex = device.acquireNextImageKHR(i.second.swapchain, std::numeric_limits<uint32_t>::max(), i.second.imageAvailableSemaphore, vk::Fence()).value;
            }
            catch (...) {
                create_vulkan_resources(vulkan, i.first);
                i.second.imageIndex = device.acquireNextImageKHR(i.second.swapchain, std::numeric_limits<uint32_t>::max(), i.second.imageAvailableSemaphore, vk::Fence()).value;
            }
            
            swapchains.push_back(i.second.swapchain);
            imageIndexes.push_back(i.second.imageIndex);
        }

        return true;
    }

    bool GLFW::submit_cmd_buffers(const Libraries::Vulkan *vulkan) {
        #pragma region ErrorChecking
        if (initialized == false) {
            std::cout << "GLFW: Not initialized, can't submit command buffers." << std::endl;
            return false;
        }
        
        if (!vulkan->is_initialized()) {
            std::cout << "GLFW: Vulkan library not initialized. Can't submit command buffers" << std::endl;
            return false;
        }
        #pragma endregion

        auto submitPipelineStages = vk::PipelineStageFlags();
        submitPipelineStages |= vk::PipelineStageFlagBits::eColorAttachmentOutput;
        // vector<vk::CommandBuffer> commandBuffers;
        // vector<vk::Semaphore> waitSemaphores;
		vector<vk::Semaphore> signalSemaphores;
		vector<vk::Fence> fences;
		vk::Queue graphicsQueue = vulkan->get_graphics_queue();
		vk::Queue presentQueue = vulkan->get_present_queue();

        std::vector<vk::SwapchainKHR> swapchains;
        std::vector<uint32_t> imageIndexes;
        for (auto i : Windows()) {
            // commandBuffers.push_back(i.second.drawCmdBuffers[i.second.imageIndex]);
            // fences.push_back(i.second.drawCmdBufferFences[i.second.imageIndex]);
            // waitSemaphores.push_back();
            signalSemaphores.push_back(i.second.renderCompleteSemaphore);
            swapchains.push_back(i.second.swapchain);
            imageIndexes.push_back(i.second.imageIndex);


            /* Submit command buffer to graphics queue for rendering */
            vk::SubmitInfo info;
            info.waitSemaphoreCount = 1;
            info.pWaitSemaphores = &i.second.imageAvailableSemaphore;
            info.pWaitDstStageMask = &submitPipelineStages;
            info.commandBufferCount = 1;
            info.pCommandBuffers = &i.second.drawCmdBuffers[i.second.imageIndex];
            info.signalSemaphoreCount = 1;
            info.pSignalSemaphores = &i.second.renderCompleteSemaphore;

            vk::Result result = graphicsQueue.submit(1, &info, vk::Fence());
            if(result != vk::Result::eSuccess) {
                std::cout << "GLFW: Failed to submit draw command buffers to graphics queue." << std::endl;
                return false;
            }
        }

        
        vk::PresentInfoKHR presentInfo;
		presentInfo.swapchainCount = (uint32_t)swapchains.size();
		presentInfo.pSwapchains = swapchains.data();
		presentInfo.pImageIndices = imageIndexes.data();
        presentInfo.pWaitSemaphores = signalSemaphores.data();
        presentInfo.waitSemaphoreCount = (uint32_t)signalSemaphores.size();

        try {
            presentQueue.presentKHR(presentInfo);
        }
        catch (...) {
            for (auto &i : Windows()) {
                presentQueue.waitIdle();
                create_vulkan_resources(vulkan, i.first);
            }
        }

        // if (result != vk::Result::eSuccess) {
        //     std::cout << "GLFW: Failed to submit image to present queue." << std::endl;
        //     return false;
        // }
        presentQueue.waitIdle();

        return false;
    }

    bool GLFW::test_clear_images(const Libraries::Vulkan *vulkan, float r, float g, float b) {
        auto device = vulkan->get_device();
        if (!device) return false;

        vector<vk::SwapchainKHR> swapchains;
        vector<uint32_t> imageIndexes;

        for (auto i : Windows()) {
            swapchains.push_back(i.second.swapchain);
            imageIndexes.push_back(i.second.imageIndex);

            /* Starting command buffer recording */
            vk::CommandBufferBeginInfo beginInfo;
            beginInfo.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse;
            for (int j = 0; j < i.second.drawCmdBuffers.size(); ++j) {
                // device.waitForFences(1, &i.second.drawCmdBufferFences[j], true, std::numeric_limits<uint32_t>::max());
                // device.resetFences(1, &i.second.drawCmdBufferFences[j]);
                i.second.drawCmdBuffers[j].begin(beginInfo);

                /* information about this particular render pass */
                vk::RenderPassBeginInfo rpInfo;
                rpInfo.renderPass = i.second.renderPass;
                rpInfo.framebuffer = i.second.swapchainFramebuffers[j];
                rpInfo.renderArea.offset = {0,0};
                rpInfo.renderArea.extent = i.second.surfaceExtent;

                std::array<vk::ClearValue, 2> clearValues = {};
                clearValues[0].color = vk::ClearColorValue(std::array<float, 4>{r, g, b,1.0});
                clearValues[1].depthStencil = vk::ClearDepthStencilValue(1.0, 0);

                rpInfo.clearValueCount = (uint32_t)clearValues.size();
                rpInfo.pClearValues = clearValues.data();

                /* Start the render pass */
                i.second.drawCmdBuffers[j].beginRenderPass(rpInfo, vk::SubpassContents::eInline);
                i.second.drawCmdBuffers[j].endRenderPass();

                /* End this render pass */
                i.second.drawCmdBuffers[j].end();
                // if (vkEndCommandBuffer(drawCmdBuffers[swapIndex]) != VK_SUCCESS) {
                //     throw std::runtime_error("failed to record command buffer!");
                // }
            }
        }

        return false;
    }
}