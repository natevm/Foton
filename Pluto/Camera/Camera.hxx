// ┌──────────────────────────────────────────────────────────────────┐
// │  Camera                                                          │
// └──────────────────────────────────────────────────────────────────┘

#pragma once

#include "Pluto/Tools/StaticFactory.hxx"
#include "Pluto/Texture/Texture.hxx"
#include "Pluto/Material/Material.hxx"
#include "Pluto/Libraries/Vulkan/Vulkan.hxx"
#include "Pluto/Camera/CameraStruct.hxx"

class Camera : public StaticFactory
{
  private:
    /* Instance fields */
    uint32_t usedViews = 1;
    CameraStruct camera_struct;

    vk::RenderPass renderpass;
    vk::Framebuffer framebuffer;

    Texture* renderTexture = nullptr;
    Texture* resolveTexture = nullptr;
    bool allow_recording = false;

    glm::vec4 clearColor = glm::vec4(0.0); 
    float clearDepth = 1.0f;
    uint32_t clearStencil = 0;

    /* Static fields */
    static Camera cameras[MAX_CAMERAS];
	static CameraStruct* pinnedMemory;
    static std::map<std::string, uint32_t> lookupTable;
    static vk::Buffer ssbo;
    static vk::DeviceMemory ssboMemory;

    /* private instance functions */
    void setup(bool allow_recording = false, bool cubemap = false, uint32_t tex_width = 0, uint32_t tex_height = 0, uint32_t msaa_samples = 1)
    {
        set_view(glm::mat4(1.0), 0);
        use_orthographic(-1, 1, -1, 1, -1, 1);
        if (allow_recording)
        {
            this->allow_recording = true;
            
            if (cubemap)
            {
                renderTexture = Texture::CreateCubemap(name, tex_width, tex_height, true, true);
            }
            else
            {
                renderTexture = Texture::Create2D(name, tex_width, tex_height, true, true, msaa_samples);
                resolveTexture = Texture::Create2D(name + "_resolve", tex_width, tex_height, true, true, 1);
            }
            create_render_pass(tex_width, tex_height, (cubemap) ? 6 : 1, msaa_samples);
            create_frame_buffer();
            Material::SetupGraphicsPipelines(renderpass, msaa_samples);
        }
    }

    void create_render_pass(uint32_t framebufferWidth, uint32_t framebufferHeight, uint32_t layers = 1, uint32_t sample_count = 1)
    {
        auto vulkan = Libraries::Vulkan::Get();
        auto device = vulkan->get_device();

        auto sampleFlag = vulkan->highest(vulkan->min(vulkan->get_closest_sample_count_flag(sample_count), vulkan->get_msaa_sample_flags()));

        #pragma region ColorAttachment
        // Color attachment
        vk::AttachmentDescription colorAttachment;
        colorAttachment.format = renderTexture->get_color_format(); // TODO
        colorAttachment.samples = sampleFlag;
        colorAttachment.loadOp = vk::AttachmentLoadOp::eClear; // clears image to black
        colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
        colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
        colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
        colorAttachment.finalLayout = renderTexture->get_color_image_layout();

        vk::AttachmentReference colorAttachmentRef;
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = renderTexture->get_color_image_layout();
        #pragma endregion

        #pragma region CreateDepthAttachment
        vk::AttachmentDescription depthAttachment;
        depthAttachment.format = renderTexture->get_depth_format();
        depthAttachment.samples = sampleFlag;
        depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
        depthAttachment.storeOp = vk::AttachmentStoreOp::eStore;
        depthAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
        depthAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        depthAttachment.initialLayout = vk::ImageLayout::eUndefined;
        depthAttachment.finalLayout = renderTexture->get_depth_image_layout();

        vk::AttachmentReference depthAttachmentRef;
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = renderTexture->get_depth_image_layout();
        #pragma endregion

        #pragma region ColorAttachmentResolve
        // Color attachment
        vk::AttachmentDescription colorAttachmentResolve;
        colorAttachmentResolve.format = renderTexture->get_color_format(); // TODO
        colorAttachmentResolve.samples = vk::SampleCountFlagBits::e1;
        colorAttachmentResolve.loadOp = vk::AttachmentLoadOp::eDontCare; // dont clear
        colorAttachmentResolve.storeOp = vk::AttachmentStoreOp::eStore;
        colorAttachmentResolve.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
        colorAttachmentResolve.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        colorAttachmentResolve.initialLayout = vk::ImageLayout::eUndefined;
        colorAttachmentResolve.finalLayout = renderTexture->get_color_image_layout();

        vk::AttachmentReference colorAttachmentResolveRef;
        colorAttachmentResolveRef.attachment = 2;
        colorAttachmentResolveRef.layout = renderTexture->get_color_image_layout();
        #pragma endregion

        // #pragma region CreateDepthAttachmentResolve
        // vk::AttachmentDescription depthAttachmentResolve;
        // depthAttachmentResolve.format = renderTexture->get_depth_format();
        // depthAttachmentResolve.samples = vk::SampleCountFlagBits::e1;
        // depthAttachmentResolve.loadOp = vk::AttachmentLoadOp::eDontCare;
        // depthAttachmentResolve.storeOp = vk::AttachmentStoreOp::eStore;
        // depthAttachmentResolve.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
        // depthAttachmentResolve.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        // depthAttachmentResolve.initialLayout = vk::ImageLayout::eUndefined;
        // depthAttachmentResolve.finalLayout = renderTexture->get_depth_image_layout();

        // vk::AttachmentReference depthAttachmentResolveRef;
        // depthAttachmentResolveRef.attachment = 3;
        // depthAttachmentResolveRef.layout = renderTexture->get_depth_image_layout();
        // #pragma endregion



        #pragma region CreateSubpass
        vk::SubpassDescription subpass;
        subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;
        subpass.pResolveAttachments = &colorAttachmentResolveRef;

        // Use subpass dependencies for layout transitions
        std::array<vk::SubpassDependency, 2> dependencies;
        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = 0;
        dependencies[0].srcStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
        dependencies[0].srcAccessMask = vk::AccessFlagBits::eMemoryRead;
        dependencies[0].dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        dependencies[0].dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
        dependencies[0].dependencyFlags = vk::DependencyFlagBits::eByRegion;

        dependencies[1].srcSubpass = 0;
        dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[1].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        dependencies[1].srcAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
        dependencies[1].dstStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
        dependencies[1].dstAccessMask = vk::AccessFlagBits::eMemoryRead;
        dependencies[1].dependencyFlags = vk::DependencyFlagBits::eByRegion;
        #pragma endregion

        #pragma region CreateRenderPass

        uint32_t mask = 0;
        for (uint32_t i = 0; i < layers; ++i)
            mask |= 1 << i;

        // Support for multiview
        const uint32_t viewMasks[] = {mask};
        const uint32_t correlationMasks[] = {mask};

        vk::RenderPassMultiviewCreateInfo renderPassMultiviewInfo;
        renderPassMultiviewInfo.subpassCount = 1;
        renderPassMultiviewInfo.pViewMasks = viewMasks;
        renderPassMultiviewInfo.dependencyCount = 0;
        renderPassMultiviewInfo.pViewOffsets = NULL;
        renderPassMultiviewInfo.correlationMaskCount = 1;
        renderPassMultiviewInfo.pCorrelationMasks = correlationMasks;

        /* Create the render pass */
        std::array<vk::AttachmentDescription, 3> attachments = { colorAttachment, depthAttachment, colorAttachmentResolve };
        vk::RenderPassCreateInfo renderPassInfo;
        renderPassInfo.attachmentCount = (uint32_t) attachments.size();
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = (uint32_t) dependencies.size();
        renderPassInfo.pDependencies = dependencies.data();
        renderPassInfo.pNext = &renderPassMultiviewInfo;

        renderpass = device.createRenderPass(renderPassInfo);

        if (renderpass == vk::RenderPass()) {
            std::cout << "GLFW: Not initialized, can't create vulkan resources." << std::endl;
            return;
        }
        #pragma endregion
    }

    void create_frame_buffer() {
        auto vulkan = Libraries::Vulkan::Get();
        auto device = vulkan->get_device();

        vk::ImageView attachments[3];
        attachments[0] = renderTexture->get_color_image_view();
        attachments[1] = renderTexture->get_depth_image_view();
        attachments[2] = resolveTexture->get_color_image_view();

        vk::FramebufferCreateInfo fbufCreateInfo;
        fbufCreateInfo.renderPass = renderpass;
        fbufCreateInfo.attachmentCount = 3;
        fbufCreateInfo.pAttachments = attachments;
        fbufCreateInfo.width = renderTexture->get_width();
        fbufCreateInfo.height = renderTexture->get_height();
        fbufCreateInfo.layers = renderTexture->get_total_layers();

        framebuffer = device.createFramebuffer(fbufCreateInfo);
    }

    void update_used_views(uint32_t multiview) {
        usedViews = (usedViews >= multiview) ? usedViews : multiview;
    }

    bool check_multiview_index(uint32_t multiview) {
        if (multiview >= MAX_MULTIVIEW) {
            std::cout<<"Error, multiview index is larger than MAX_MULTIVIEW"<<std::endl;
            return false;
        }
        return true;
    }

  public:
    /* Public static functions */
    static Camera* Create(std::string name, bool allow_recording = false, bool cubemap = false, uint32_t tex_width = 0, uint32_t tex_height = 0, uint32_t msaa_samples = 1);
    static Camera* Get(std::string name);
    static Camera* Get(uint32_t id);
    static Camera* GetFront();
	static uint32_t GetCount();
    static bool Delete(std::string name);
    static bool Delete(uint32_t id);

    static void Initialize();
    static void UploadSSBO();
    static vk::Buffer GetSSBO();
    static uint32_t GetSSBOSize();
    static void CleanUp();	

    /* Public instance functions */
    Camera() {
		this->initialized = false;
	}

	Camera(std::string name, uint32_t id) {
		this->initialized = true;
		this->name = name;
		this->id = id;

        camera_struct.multiviews[0].near_pos = .01f;
        camera_struct.multiviews[0].far_pos = 1000.f;
        camera_struct.multiviews[0].fov = 90.f;
	}

    bool use_orthographic(float left, float right, float bottom, float top, float near_pos, float far_pos, uint32_t multiview = 0)
    {
        if (!check_multiview_index(multiview)) return false;
        this->set_near_pos(near_pos);
        this->set_far_pos(far_pos);
        this->set_projection(glm::ortho(left, right, bottom, top, near_pos, far_pos), multiview);
        return true;
    };

    bool use_perspective(float fov_in_radians, float width, float height, float near_pos, float far_pos, uint32_t multiview = 0)
    {
        if (!check_multiview_index(multiview)) return false;
        this->set_near_pos(near_pos, multiview);
        this->set_far_pos(far_pos, multiview);
        this->set_projection(glm::perspectiveFov(fov_in_radians, width, height, near_pos, far_pos), multiview);
        return true;
    };
    
    float get_near_pos(uint32_t multiview = 0) { 
        if (!check_multiview_index(multiview)) return -1;
        return camera_struct.multiviews[multiview].near_pos; 
    }
    
    bool set_near_pos(float near_pos, uint32_t multiview = 0)
    {
        if (!check_multiview_index(multiview)) return false;
        update_used_views(multiview);
        camera_struct.multiviews[multiview].near_pos = near_pos;
        return true;
    }

    float get_far_pos(uint32_t multiview = 0) { 
        if (!check_multiview_index(multiview)) return -1;
        return camera_struct.multiviews[multiview].far_pos; 
    }

    bool set_far_pos(float far_pos, uint32_t multiview = 0)
    {
        if (!check_multiview_index(multiview)) return false;
        update_used_views(multiview);
        camera_struct.multiviews[multiview].far_pos = far_pos;
        return true;
    }

    glm::mat4 get_view(uint32_t multiview = 0) { 
        if (!check_multiview_index(multiview)) return glm::mat4();
        return camera_struct.multiviews[multiview].view; 
    };
    
    bool set_view(glm::mat4 view, uint32_t multiview = 0)
    {
        if (!check_multiview_index(multiview)) return false;
        update_used_views(multiview);
        usedViews = (usedViews >= multiview) ? usedViews : multiview;
        camera_struct.multiviews[multiview].view = view;
        camera_struct.multiviews[multiview].viewinv = glm::inverse(view);
        return true;
    };

    glm::mat4 get_projection(uint32_t multiview = 0) { 
        if (!check_multiview_index(multiview)) return glm::mat4();
        return camera_struct.multiviews[multiview].proj; 
    };

    bool set_projection(glm::mat4 projection, uint32_t multiview = 0)
    {
        if (!check_multiview_index(multiview)) return false;
        update_used_views(multiview);
        camera_struct.multiviews[multiview].proj = projection;
        camera_struct.multiviews[multiview].projinv = glm::inverse(projection);
        return true;
    };

    Texture* get_texture()
    {
        return resolveTexture;
    }

    std::string to_string()
    {
        std::string output;
        output += "{\n";
        output += "\ttype: \"Camera\",\n";
        output += "\tname: \"" + name + "\",\n";
        output += "\tused_views: " + std::to_string(usedViews) + ",\n";
        output += "\tprojections: [\n";
        for (uint32_t i = 0; i < usedViews; ++i)
        {
            output += "\t\t";
            output += glm::to_string(camera_struct.multiviews[i].proj);
            output += (i == (usedViews - 1)) ? "\n" : ",\n";
        }
        output += "\t],\n";
        output += "\tviews: [\n";
        for (uint32_t i = 0; i < usedViews; ++i)
        {
            output += "\t\t";
            output += glm::to_string(camera_struct.multiviews[i].view);
            output += (i == (usedViews - 1)) ? "\n" : ",\n";
            output += "\t\tnear_pos: " + std::to_string(camera_struct.multiviews[i].near_pos) + ",\n";
            output += "\t\tfar_pos: " + std::to_string(camera_struct.multiviews[i].far_pos) + ",\n";
            output += "\t\tfov: " + std::to_string(camera_struct.multiviews[i].fov) + "\n";
        }
        output += "\t],\n";
        output += "}";
        return output;
    }

    // this should be in the render system...
    bool begin_renderpass(vk::CommandBuffer command_buffer)
    {
        /* Not all cameras allow recording. */
        if (!allow_recording)
            return false;

        vk::RenderPassBeginInfo rpInfo;
        rpInfo.renderPass = renderpass;
        rpInfo.framebuffer = framebuffer;
        rpInfo.renderArea.offset = {0, 0};
        rpInfo.renderArea.extent = {renderTexture->get_width(), renderTexture->get_height()};

        std::array<vk::ClearValue, 2> clearValues = {};
        clearValues[0].color = vk::ClearColorValue(std::array<float, 4>{clearColor.r, clearColor.g, clearColor.b, clearColor.a});
        clearValues[1].depthStencil = vk::ClearDepthStencilValue(clearDepth, clearStencil);

        rpInfo.clearValueCount = (uint32_t)clearValues.size();
        rpInfo.pClearValues = clearValues.data();

        /* Start the render pass */
        command_buffer.beginRenderPass(rpInfo, vk::SubpassContents::eInline);

        /* Set viewport*/
		vk::Viewport viewport;
		viewport.width = (float)renderTexture->get_width();
		viewport.height = (float)renderTexture->get_height();
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

        command_buffer.setViewport(0, {viewport});

		/* Set Scissors */
		vk::Rect2D rect2D;
		rect2D.extent.width = renderTexture->get_width();
		rect2D.extent.height = renderTexture->get_height();
		rect2D.offset.x = 0;
		rect2D.offset.y = 0;

        command_buffer.setScissor(0, {rect2D});

        return true;
    }

    bool end_renderpass(vk::CommandBuffer command_buffer) {
        if (!allow_recording) 
            return false;
            
        command_buffer.endRenderPass();
        return true;
    }

    void set_clear_color(float r, float g, float b, float a) {
        clearColor = glm::vec4(r, g, b, a);
    }

    void set_clear_stencil(uint32_t stencil) {
        clearStencil = stencil;
    }

    void set_clear_depth(float depth) {
        clearDepth = depth;
    }
};