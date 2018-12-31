#include "./Material.hxx"
#include "Pluto/Tools/Options.hxx"
#include "Pluto/Tools/FileReader.hxx"

#include "Pluto/Entity/Entity.hxx"
#include "Pluto/Transform/Transform.hxx"
#include "Pluto/Camera/Camera.hxx"
#include "Pluto/Light/Light.hxx"
#include "Pluto/Texture/Texture.hxx"

Material Material::materials[MAX_MATERIALS];
MaterialStruct* Material::pinnedMemory;
std::map<std::string, uint32_t> Material::lookupTable;
vk::Buffer Material::ssbo;
vk::DeviceMemory Material::ssboMemory;

/* UNIFORM COLOR PIPELINE */
Material::MaterialResources Material::uniformColor;
Material::MaterialResources Material::blinn;
Material::MaterialResources Material::pbr;
Material::MaterialResources Material::texcoordsurface;
Material::MaterialResources Material::normalsurface;

/* Wrapper for shader module creation */
vk::ShaderModule Material::CreateShaderModule(const std::vector<char>& code) {
    auto vulkan = Libraries::Vulkan::Get();
    auto device = vulkan->get_device();

    vk::ShaderModuleCreateInfo createInfo;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    vk::ShaderModule shaderModule = device.createShaderModule(createInfo);
    return shaderModule;
}


/* Under the hood, all material types have a set of Vulkan pipeline objects. */
void Material::CreatePipeline(
    std::vector<vk::PipelineShaderStageCreateInfo> shaderStages, // yes
    std::vector<vk::VertexInputBindingDescription> bindingDescriptions, // yes
    std::vector<vk::VertexInputAttributeDescription> attributeDescriptions, // yes
    std::vector<vk::DescriptorSetLayout> componentDescriptorSetLayouts, // yes
    PipelineParameters parameters,
    vk::RenderPass renderpass,
    uint32 subpass,
    vk::Pipeline &pipeline,
    vk::PipelineLayout &layout 
) {
    auto vulkan = Libraries::Vulkan::Get();
    auto device = vulkan->get_device();

    /* Vertex Input */
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
    vertexInputInfo.vertexBindingDescriptionCount = (uint32_t)bindingDescriptions.size();
    vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions.data();
    vertexInputInfo.vertexAttributeDescriptionCount = (uint32_t)attributeDescriptions.size();
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    vk::PushConstantRange range;
    range.offset = 0;
    range.size = (2 + MAX_LIGHTS) * sizeof(uint32_t);
    range.stageFlags = vk::ShaderStageFlagBits::eAll;

    /* Connect things together with pipeline layout */
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
    pipelineLayoutInfo.setLayoutCount = (uint32_t)componentDescriptorSetLayouts.size();
    pipelineLayoutInfo.pSetLayouts = componentDescriptorSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1; // TODO: this needs to account for entity id
    pipelineLayoutInfo.pPushConstantRanges = &range; // TODO: this needs to account for entity id

    /* Create the pipeline layout */
    layout = device.createPipelineLayout(pipelineLayoutInfo);
    
    vk::GraphicsPipelineCreateInfo pipelineInfo;
    pipelineInfo.stageCount = (uint32_t)shaderStages.size();
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &parameters.inputAssembly;
    pipelineInfo.pViewportState = &parameters.viewportState;
    pipelineInfo.pRasterizationState = &parameters.rasterizer;
    pipelineInfo.pMultisampleState = &parameters.multisampling;
    pipelineInfo.pDepthStencilState = &parameters.depthStencil;
    pipelineInfo.pColorBlendState = &parameters.colorBlending;
    pipelineInfo.pDynamicState = &parameters.dynamicState; // Optional
    pipelineInfo.layout = layout;
    pipelineInfo.renderPass = renderpass;
    pipelineInfo.subpass = subpass;
    pipelineInfo.basePipelineHandle = vk::Pipeline(); // Optional
    pipelineInfo.basePipelineIndex = -1; // Optional

    /* Create pipeline */
    pipeline = device.createGraphicsPipelines(vk::PipelineCache(), {pipelineInfo})[0];
}

void Material::SetupGraphicsPipelines(uint32_t id, vk::RenderPass renderpass)
{
    auto vulkan = Libraries::Vulkan::Get();
    auto device = vulkan->get_device();

    /* ------ UNIFORM COLOR GRAPHICS PIPELINE  ------ */

    /* Read in shader modules */
    {
        std::string ResourcePath = Options::GetResourcePath();
        auto vertShaderCode = readFile(ResourcePath + std::string("/Shaders/SurfaceMaterials/UniformColor/vert.spv"));
        auto fragShaderCode = readFile(ResourcePath + std::string("/Shaders/SurfaceMaterials/UniformColor/frag.spv"));

        /* Create shader modules */
        auto vertShaderModule = CreateShaderModule(vertShaderCode);
        auto fragShaderModule = CreateShaderModule(fragShaderCode);

        /* Info for shader stages */
        vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
        vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main"; // entry point here? would be nice to combine shaders into one file

        vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
        fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertShaderStageInfo, fragShaderStageInfo };
        
        CreatePipeline(shaderStages, uniformColor.vertexInputBindingDescriptions, 
            uniformColor.vertexInputAttributeDescriptions, { uniformColor.componentDescriptorSetLayout, uniformColor.textureDescriptorSetLayout }, uniformColor.pipelineParameters, 
            renderpass, 0, 
            uniformColor.pipeline, uniformColor.pipelineLayout);

        device.destroyShaderModule(fragShaderModule);
        device.destroyShaderModule(vertShaderModule);
    }


    /* ------ BLINN GRAPHICS PIPELINE  ------ */
    {
        std::string ResourcePath = Options::GetResourcePath();
        auto vertShaderCode = readFile(ResourcePath + std::string("/Shaders/SurfaceMaterials/Blinn/vert.spv"));
        auto fragShaderCode = readFile(ResourcePath + std::string("/Shaders/SurfaceMaterials/Blinn/frag.spv"));

        /* Create shader modules */
        auto vertShaderModule = CreateShaderModule(vertShaderCode);
        auto fragShaderModule = CreateShaderModule(fragShaderCode);

        /* Info for shader stages */
        vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
        vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main"; // entry point here? would be nice to combine shaders into one file

        vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
        fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertShaderStageInfo, fragShaderStageInfo };
        
        CreatePipeline(shaderStages, blinn.vertexInputBindingDescriptions, 
            blinn.vertexInputAttributeDescriptions, { blinn.componentDescriptorSetLayout, blinn.textureDescriptorSetLayout }, blinn.pipelineParameters, 
            renderpass, 0, 
            blinn.pipeline, blinn.pipelineLayout);

        device.destroyShaderModule(fragShaderModule);
        device.destroyShaderModule(vertShaderModule);
    }

    /* ------ PBR GRAPHICS PIPELINE  ------ */
    {
        std::string ResourcePath = Options::GetResourcePath();
        auto vertShaderCode = readFile(ResourcePath + std::string("/Shaders/SurfaceMaterials/Blinn/vert.spv"));
        auto fragShaderCode = readFile(ResourcePath + std::string("/Shaders/SurfaceMaterials/Blinn/frag.spv"));

        /* Create shader modules */
        auto vertShaderModule = CreateShaderModule(vertShaderCode);
        auto fragShaderModule = CreateShaderModule(fragShaderCode);

        /* Info for shader stages */
        vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
        vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main"; // entry point here? would be nice to combine shaders into one file

        vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
        fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertShaderStageInfo, fragShaderStageInfo };
        
        CreatePipeline(shaderStages, pbr.vertexInputBindingDescriptions, 
            pbr.vertexInputAttributeDescriptions, { pbr.componentDescriptorSetLayout, pbr.textureDescriptorSetLayout }, pbr.pipelineParameters, 
            renderpass, 0, 
            pbr.pipeline, pbr.pipelineLayout);

        device.destroyShaderModule(fragShaderModule);
        device.destroyShaderModule(vertShaderModule);
    }

    /* ------ NORMAL SURFACE GRAPHICS PIPELINE  ------ */
    {
        std::string ResourcePath = Options::GetResourcePath();
        auto vertShaderCode = readFile(ResourcePath + std::string("/Shaders/SurfaceMaterials/NormalSurface/vert.spv"));
        auto fragShaderCode = readFile(ResourcePath + std::string("/Shaders/SurfaceMaterials/NormalSurface/frag.spv"));

        /* Create shader modules */
        auto vertShaderModule = CreateShaderModule(vertShaderCode);
        auto fragShaderModule = CreateShaderModule(fragShaderCode);

        /* Info for shader stages */
        vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
        vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main"; // entry point here? would be nice to combine shaders into one file

        vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
        fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertShaderStageInfo, fragShaderStageInfo };
        
        CreatePipeline(shaderStages, normalsurface.vertexInputBindingDescriptions, 
            normalsurface.vertexInputAttributeDescriptions, { normalsurface.componentDescriptorSetLayout, normalsurface.textureDescriptorSetLayout }, normalsurface.pipelineParameters, 
            renderpass, 0, 
            normalsurface.pipeline, normalsurface.pipelineLayout);

        device.destroyShaderModule(fragShaderModule);
        device.destroyShaderModule(vertShaderModule);
    }

    /* ------ TEXCOORD SURFACE GRAPHICS PIPELINE  ------ */
    {
        std::string ResourcePath = Options::GetResourcePath();
        auto vertShaderCode = readFile(ResourcePath + std::string("/Shaders/SurfaceMaterials/TexCoordSurface/vert.spv"));
        auto fragShaderCode = readFile(ResourcePath + std::string("/Shaders/SurfaceMaterials/TexCoordSurface/frag.spv"));

        /* Create shader modules */
        auto vertShaderModule = CreateShaderModule(vertShaderCode);
        auto fragShaderModule = CreateShaderModule(fragShaderCode);

        /* Info for shader stages */
        vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
        vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main"; // entry point here? would be nice to combine shaders into one file

        vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
        fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertShaderStageInfo, fragShaderStageInfo };
        
        CreatePipeline(shaderStages, texcoordsurface.vertexInputBindingDescriptions, 
            texcoordsurface.vertexInputAttributeDescriptions, { texcoordsurface.componentDescriptorSetLayout, texcoordsurface.textureDescriptorSetLayout }, texcoordsurface.pipelineParameters, 
            renderpass, 0, 
            texcoordsurface.pipeline, texcoordsurface.pipelineLayout);

        device.destroyShaderModule(fragShaderModule);
        device.destroyShaderModule(vertShaderModule);
    }
}

/* SSBO logic */
void Material::Initialize()
{
    Material::CreateDescriptorSetLayouts();
    Material::CreateDescriptorPools();
    Material::CreateVertexInputBindingDescriptions();
    Material::CreateVertexAttributeDescriptions();
    Material::CreateSSBO();
    Material::CreateDescriptorSets();
}

void Material::CreateDescriptorSetLayouts()
{
    auto vulkan = Libraries::Vulkan::Get();
    auto device = vulkan->get_device();

    /* ------ UNIFORM COLOR LAYOUT ------ */
    {
        // Entity SSBO
        vk::DescriptorSetLayoutBinding eboLayoutBinding;
        eboLayoutBinding.binding = 0;
        eboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
        eboLayoutBinding.descriptorCount = 1;
        eboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
        eboLayoutBinding.pImmutableSamplers = nullptr; // Optional

        // Transform SSBO
        vk::DescriptorSetLayoutBinding tboLayoutBinding;
        tboLayoutBinding.binding = 1;
        tboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
        tboLayoutBinding.descriptorCount = 1;
        tboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
        tboLayoutBinding.pImmutableSamplers = nullptr; // Optional

        // Camera SSBO
        vk::DescriptorSetLayoutBinding cboLayoutBinding;
        cboLayoutBinding.binding = 2;
        cboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
        cboLayoutBinding.descriptorCount = 1;
        cboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
        cboLayoutBinding.pImmutableSamplers = nullptr; // Optional

        // Material SSBO
        vk::DescriptorSetLayoutBinding mboLayoutBinding;
        mboLayoutBinding.binding = 3;
        mboLayoutBinding.descriptorCount = 1;
        mboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
        mboLayoutBinding.pImmutableSamplers = nullptr;
        mboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;

        // Light SSBO
        vk::DescriptorSetLayoutBinding lboLayoutBinding;
        lboLayoutBinding.binding = 4;
        lboLayoutBinding.descriptorCount = 1;
        lboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
        lboLayoutBinding.pImmutableSamplers = nullptr;
        lboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;

        std::array<vk::DescriptorSetLayoutBinding, 5> bindings = { eboLayoutBinding, tboLayoutBinding, cboLayoutBinding, mboLayoutBinding, lboLayoutBinding};
        vk::DescriptorSetLayoutCreateInfo layoutInfo;
        layoutInfo.bindingCount = (uint32_t)bindings.size();
        layoutInfo.pBindings = bindings.data();

        uniformColor.componentDescriptorSetLayout = device.createDescriptorSetLayout(layoutInfo);
    }

    {
        // 2D Texture samplers
        vk::DescriptorSetLayoutBinding sampler2DBinding;
        sampler2DBinding.descriptorCount = MAX_TEXTURES;
        sampler2DBinding.binding = 0;
        sampler2DBinding.descriptorType = vk::DescriptorType::eSampler;
        sampler2DBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
        sampler2DBinding.pImmutableSamplers = 0;

        // 2D Textures
        vk::DescriptorSetLayoutBinding texture2DBinding;
        texture2DBinding.descriptorCount = MAX_TEXTURES;
        texture2DBinding.binding = 1;
        texture2DBinding.descriptorType = vk::DescriptorType::eSampledImage;
        texture2DBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
        texture2DBinding.pImmutableSamplers = 0;

        std::array<vk::DescriptorSetLayoutBinding, 2> bindings = {sampler2DBinding, texture2DBinding};
        vk::DescriptorSetLayoutCreateInfo layoutInfo;
        layoutInfo.bindingCount = (uint32_t)bindings.size();
        layoutInfo.pBindings = bindings.data();

        uniformColor.textureDescriptorSetLayout = device.createDescriptorSetLayout(layoutInfo);
    }

    /* ------ BLINN LAYOUT ------ */
    {
        // Entity SSBO
        vk::DescriptorSetLayoutBinding eboLayoutBinding;
        eboLayoutBinding.binding = 0;
        eboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
        eboLayoutBinding.descriptorCount = 1;
        eboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
        eboLayoutBinding.pImmutableSamplers = nullptr; // Optional

        // Transform SSBO
        vk::DescriptorSetLayoutBinding tboLayoutBinding;
        tboLayoutBinding.binding = 1;
        tboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
        tboLayoutBinding.descriptorCount = 1;
        tboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
        tboLayoutBinding.pImmutableSamplers = nullptr; // Optional

        // Camera SSBO
        vk::DescriptorSetLayoutBinding cboLayoutBinding;
        cboLayoutBinding.binding = 2;
        cboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
        cboLayoutBinding.descriptorCount = 1;
        cboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
        cboLayoutBinding.pImmutableSamplers = nullptr; // Optional

        // Material SSBO
        vk::DescriptorSetLayoutBinding mboLayoutBinding;
        mboLayoutBinding.binding = 3;
        mboLayoutBinding.descriptorCount = 1;
        mboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
        mboLayoutBinding.pImmutableSamplers = nullptr;
        mboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;

        // Light SSBO
        vk::DescriptorSetLayoutBinding lboLayoutBinding;
        lboLayoutBinding.binding = 4;
        lboLayoutBinding.descriptorCount = 1;
        lboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
        lboLayoutBinding.pImmutableSamplers = nullptr;
        lboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;

        std::array<vk::DescriptorSetLayoutBinding, 5> bindings = { eboLayoutBinding, tboLayoutBinding, cboLayoutBinding, mboLayoutBinding, lboLayoutBinding};
        vk::DescriptorSetLayoutCreateInfo layoutInfo;
        layoutInfo.bindingCount = (uint32_t)bindings.size();
        layoutInfo.pBindings = bindings.data();

        blinn.componentDescriptorSetLayout = device.createDescriptorSetLayout(layoutInfo);
    }

    {
        // 2D Texture samplers
        vk::DescriptorSetLayoutBinding sampler2DBinding;
        sampler2DBinding.descriptorCount = MAX_TEXTURES;
        sampler2DBinding.binding = 0;
        sampler2DBinding.descriptorType = vk::DescriptorType::eSampler;
        sampler2DBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
        sampler2DBinding.pImmutableSamplers = 0;

        // 2D Textures
        vk::DescriptorSetLayoutBinding texture2DBinding;
        texture2DBinding.descriptorCount = MAX_TEXTURES;
        texture2DBinding.binding = 1;
        texture2DBinding.descriptorType = vk::DescriptorType::eSampledImage;
        texture2DBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
        texture2DBinding.pImmutableSamplers = 0;

        std::array<vk::DescriptorSetLayoutBinding, 2> bindings = {sampler2DBinding, texture2DBinding};
        vk::DescriptorSetLayoutCreateInfo layoutInfo;
        layoutInfo.bindingCount = (uint32_t)bindings.size();
        layoutInfo.pBindings = bindings.data();

        blinn.textureDescriptorSetLayout = device.createDescriptorSetLayout(layoutInfo);
    }

    /* ------ PBR LAYOUT ------ */
    {
        // Entity SSBO
        vk::DescriptorSetLayoutBinding eboLayoutBinding;
        eboLayoutBinding.binding = 0;
        eboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
        eboLayoutBinding.descriptorCount = 1;
        eboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
        eboLayoutBinding.pImmutableSamplers = nullptr; // Optional

        // Transform SSBO
        vk::DescriptorSetLayoutBinding tboLayoutBinding;
        tboLayoutBinding.binding = 1;
        tboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
        tboLayoutBinding.descriptorCount = 1;
        tboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
        tboLayoutBinding.pImmutableSamplers = nullptr; // Optional

        // Camera SSBO
        vk::DescriptorSetLayoutBinding cboLayoutBinding;
        cboLayoutBinding.binding = 2;
        cboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
        cboLayoutBinding.descriptorCount = 1;
        cboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
        cboLayoutBinding.pImmutableSamplers = nullptr; // Optional

        // Material SSBO
        vk::DescriptorSetLayoutBinding mboLayoutBinding;
        mboLayoutBinding.binding = 3;
        mboLayoutBinding.descriptorCount = 1;
        mboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
        mboLayoutBinding.pImmutableSamplers = nullptr;
        mboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;

        // Light SSBO
        vk::DescriptorSetLayoutBinding lboLayoutBinding;
        lboLayoutBinding.binding = 4;
        lboLayoutBinding.descriptorCount = 1;
        lboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
        lboLayoutBinding.pImmutableSamplers = nullptr;
        lboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;

        std::array<vk::DescriptorSetLayoutBinding, 5> bindings = { eboLayoutBinding, tboLayoutBinding, cboLayoutBinding, mboLayoutBinding, lboLayoutBinding};
        vk::DescriptorSetLayoutCreateInfo layoutInfo;
        layoutInfo.bindingCount = (uint32_t)bindings.size();
        layoutInfo.pBindings = bindings.data();

        pbr.componentDescriptorSetLayout = device.createDescriptorSetLayout(layoutInfo);
    }

    {
        // 2D Texture samplers
        vk::DescriptorSetLayoutBinding sampler2DBinding;
        sampler2DBinding.descriptorCount = MAX_TEXTURES;
        sampler2DBinding.binding = 0;
        sampler2DBinding.descriptorType = vk::DescriptorType::eSampler;
        sampler2DBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
        sampler2DBinding.pImmutableSamplers = 0;

        // 2D Textures
        vk::DescriptorSetLayoutBinding texture2DBinding;
        texture2DBinding.descriptorCount = MAX_TEXTURES;
        texture2DBinding.binding = 1;
        texture2DBinding.descriptorType = vk::DescriptorType::eSampledImage;
        texture2DBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
        texture2DBinding.pImmutableSamplers = 0;

        std::array<vk::DescriptorSetLayoutBinding, 2> bindings = {sampler2DBinding, texture2DBinding};
        vk::DescriptorSetLayoutCreateInfo layoutInfo;
        layoutInfo.bindingCount = (uint32_t)bindings.size();
        layoutInfo.pBindings = bindings.data();

        pbr.textureDescriptorSetLayout = device.createDescriptorSetLayout(layoutInfo);
    }

    /* ------ TEXCOORD SURFACE LAYOUT ------ */
    {
        // Entity SSBO
        vk::DescriptorSetLayoutBinding eboLayoutBinding;
        eboLayoutBinding.binding = 0;
        eboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
        eboLayoutBinding.descriptorCount = 1;
        eboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
        eboLayoutBinding.pImmutableSamplers = nullptr; // Optional

        // Transform SSBO
        vk::DescriptorSetLayoutBinding tboLayoutBinding;
        tboLayoutBinding.binding = 1;
        tboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
        tboLayoutBinding.descriptorCount = 1;
        tboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
        tboLayoutBinding.pImmutableSamplers = nullptr; // Optional

        // Camera SSBO
        vk::DescriptorSetLayoutBinding cboLayoutBinding;
        cboLayoutBinding.binding = 2;
        cboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
        cboLayoutBinding.descriptorCount = 1;
        cboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
        cboLayoutBinding.pImmutableSamplers = nullptr; // Optional

        // Material SSBO
        vk::DescriptorSetLayoutBinding mboLayoutBinding;
        mboLayoutBinding.binding = 3;
        mboLayoutBinding.descriptorCount = 1;
        mboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
        mboLayoutBinding.pImmutableSamplers = nullptr;
        mboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;

        // Light SSBO
        vk::DescriptorSetLayoutBinding lboLayoutBinding;
        lboLayoutBinding.binding = 4;
        lboLayoutBinding.descriptorCount = 1;
        lboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
        lboLayoutBinding.pImmutableSamplers = nullptr;
        lboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;

        std::array<vk::DescriptorSetLayoutBinding, 5> bindings = { eboLayoutBinding, tboLayoutBinding, cboLayoutBinding, mboLayoutBinding, lboLayoutBinding};
        vk::DescriptorSetLayoutCreateInfo layoutInfo;
        layoutInfo.bindingCount = (uint32_t)bindings.size();
        layoutInfo.pBindings = bindings.data();

        texcoordsurface.componentDescriptorSetLayout = device.createDescriptorSetLayout(layoutInfo);
    }

    {
        // 2D Texture samplers
        vk::DescriptorSetLayoutBinding sampler2DBinding;
        sampler2DBinding.descriptorCount = MAX_TEXTURES;
        sampler2DBinding.binding = 0;
        sampler2DBinding.descriptorType = vk::DescriptorType::eSampler;
        sampler2DBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
        sampler2DBinding.pImmutableSamplers = 0;

        // 2D Textures
        vk::DescriptorSetLayoutBinding texture2DBinding;
        texture2DBinding.descriptorCount = MAX_TEXTURES;
        texture2DBinding.binding = 1;
        texture2DBinding.descriptorType = vk::DescriptorType::eSampledImage;
        texture2DBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
        texture2DBinding.pImmutableSamplers = 0;

        std::array<vk::DescriptorSetLayoutBinding, 2> bindings = {sampler2DBinding, texture2DBinding};
        vk::DescriptorSetLayoutCreateInfo layoutInfo;
        layoutInfo.bindingCount = (uint32_t)bindings.size();
        layoutInfo.pBindings = bindings.data();

        texcoordsurface.textureDescriptorSetLayout = device.createDescriptorSetLayout(layoutInfo);
    }

    /* ------ NORMAL SURFACE LAYOUT ------ */
    {
        // Entity SSBO
        vk::DescriptorSetLayoutBinding eboLayoutBinding;
        eboLayoutBinding.binding = 0;
        eboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
        eboLayoutBinding.descriptorCount = 1;
        eboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
        eboLayoutBinding.pImmutableSamplers = nullptr; // Optional

        // Transform SSBO
        vk::DescriptorSetLayoutBinding tboLayoutBinding;
        tboLayoutBinding.binding = 1;
        tboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
        tboLayoutBinding.descriptorCount = 1;
        tboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
        tboLayoutBinding.pImmutableSamplers = nullptr; // Optional

        // Camera SSBO
        vk::DescriptorSetLayoutBinding cboLayoutBinding;
        cboLayoutBinding.binding = 2;
        cboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
        cboLayoutBinding.descriptorCount = 1;
        cboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
        cboLayoutBinding.pImmutableSamplers = nullptr; // Optional

        // Material SSBO
        vk::DescriptorSetLayoutBinding mboLayoutBinding;
        mboLayoutBinding.binding = 3;
        mboLayoutBinding.descriptorCount = 1;
        mboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
        mboLayoutBinding.pImmutableSamplers = nullptr;
        mboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;

        // Light SSBO
        vk::DescriptorSetLayoutBinding lboLayoutBinding;
        lboLayoutBinding.binding = 4;
        lboLayoutBinding.descriptorCount = 1;
        lboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
        lboLayoutBinding.pImmutableSamplers = nullptr;
        lboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;

        std::array<vk::DescriptorSetLayoutBinding, 5> bindings = { eboLayoutBinding, tboLayoutBinding, cboLayoutBinding, mboLayoutBinding, lboLayoutBinding};
        vk::DescriptorSetLayoutCreateInfo layoutInfo;
        layoutInfo.bindingCount = (uint32_t)bindings.size();
        layoutInfo.pBindings = bindings.data();

        normalsurface.componentDescriptorSetLayout = device.createDescriptorSetLayout(layoutInfo);
    }

    {
        // 2D Texture samplers
        vk::DescriptorSetLayoutBinding sampler2DBinding;
        sampler2DBinding.descriptorCount = MAX_TEXTURES;
        sampler2DBinding.binding = 0;
        sampler2DBinding.descriptorType = vk::DescriptorType::eSampler;
        sampler2DBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
        sampler2DBinding.pImmutableSamplers = 0;

        // 2D Textures
        vk::DescriptorSetLayoutBinding texture2DBinding;
        texture2DBinding.descriptorCount = MAX_TEXTURES;
        texture2DBinding.binding = 1;
        texture2DBinding.descriptorType = vk::DescriptorType::eSampledImage;
        texture2DBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
        texture2DBinding.pImmutableSamplers = 0;

        std::array<vk::DescriptorSetLayoutBinding, 2> bindings = {sampler2DBinding, texture2DBinding};
        vk::DescriptorSetLayoutCreateInfo layoutInfo;
        layoutInfo.bindingCount = (uint32_t)bindings.size();
        layoutInfo.pBindings = bindings.data();

        normalsurface.textureDescriptorSetLayout = device.createDescriptorSetLayout(layoutInfo);
    }
}

void Material::CreateDescriptorPools()
{
    auto vulkan = Libraries::Vulkan::Get();
    auto device = vulkan->get_device();

    /* ------ UNIFORM COLOR DESCRIPTOR POOL  ------ */
    {
        std::array<vk::DescriptorPoolSize, 5> poolSizes = {};
    
        // Entity SSBO
        poolSizes[0].type = vk::DescriptorType::eStorageBuffer;
        poolSizes[0].descriptorCount = MAX_MATERIALS;
        
        // Transform SSBO
        poolSizes[1].type = vk::DescriptorType::eStorageBuffer;
        poolSizes[1].descriptorCount = MAX_MATERIALS;
        
        // Camera SSBO
        poolSizes[2].type = vk::DescriptorType::eStorageBuffer;
        poolSizes[2].descriptorCount = MAX_MATERIALS;
        
        // Material SSBO
        poolSizes[3].type = vk::DescriptorType::eStorageBuffer;
        poolSizes[3].descriptorCount = MAX_MATERIALS;
        
        // Light SSBO
        poolSizes[4].type = vk::DescriptorType::eStorageBuffer;
        poolSizes[4].descriptorCount = MAX_MATERIALS;

        vk::DescriptorPoolCreateInfo poolInfo;
        poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = MAX_MATERIALS;
        poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

        uniformColor.componentDescriptorPool = device.createDescriptorPool(poolInfo);
    }

    {
        std::array<vk::DescriptorPoolSize, 2> poolSizes = {};
    
        // Sampler
        poolSizes[0].type = vk::DescriptorType::eSampler;
        poolSizes[0].descriptorCount = MAX_MATERIALS;
        
        // Texture array
        poolSizes[1].type = vk::DescriptorType::eSampledImage;
        poolSizes[1].descriptorCount = MAX_MATERIALS;
        
        vk::DescriptorPoolCreateInfo poolInfo;
        poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = MAX_MATERIALS;
        poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

        uniformColor.textureDescriptorPool = device.createDescriptorPool(poolInfo);
    }
    
    /* ------ BLINN DESCRIPTOR POOL  ------ */
    {
        std::array<vk::DescriptorPoolSize, 5> poolSizes = {};
    
        // Entity SSBO
        poolSizes[0].type = vk::DescriptorType::eStorageBuffer;
        poolSizes[0].descriptorCount = MAX_MATERIALS;
        
        // Transform SSBO
        poolSizes[1].type = vk::DescriptorType::eStorageBuffer;
        poolSizes[1].descriptorCount = MAX_MATERIALS;
        
        // Camera SSBO
        poolSizes[2].type = vk::DescriptorType::eStorageBuffer;
        poolSizes[2].descriptorCount = MAX_MATERIALS;
        
        // Material SSBO
        poolSizes[3].type = vk::DescriptorType::eStorageBuffer;
        poolSizes[3].descriptorCount = MAX_MATERIALS;
        
        // Light SSBO
        poolSizes[4].type = vk::DescriptorType::eStorageBuffer;
        poolSizes[4].descriptorCount = MAX_MATERIALS;

        vk::DescriptorPoolCreateInfo poolInfo;
        poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = MAX_MATERIALS;
        poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

        blinn.componentDescriptorPool = device.createDescriptorPool(poolInfo);
    }

    {
        std::array<vk::DescriptorPoolSize, 2> poolSizes = {};
    
        // Sampler
        poolSizes[0].type = vk::DescriptorType::eSampler;
        poolSizes[0].descriptorCount = MAX_MATERIALS;
        
        // Texture array
        poolSizes[1].type = vk::DescriptorType::eSampledImage;
        poolSizes[1].descriptorCount = MAX_MATERIALS;
        
        vk::DescriptorPoolCreateInfo poolInfo;
        poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = MAX_MATERIALS;
        poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

        blinn.textureDescriptorPool = device.createDescriptorPool(poolInfo);
    }

    /* ------ PBR DESCRIPTOR POOL  ------ */
    {
        std::array<vk::DescriptorPoolSize, 5> poolSizes = {};
    
        // Entity SSBO
        poolSizes[0].type = vk::DescriptorType::eStorageBuffer;
        poolSizes[0].descriptorCount = MAX_MATERIALS;
        
        // Transform SSBO
        poolSizes[1].type = vk::DescriptorType::eStorageBuffer;
        poolSizes[1].descriptorCount = MAX_MATERIALS;
        
        // Camera SSBO
        poolSizes[2].type = vk::DescriptorType::eStorageBuffer;
        poolSizes[2].descriptorCount = MAX_MATERIALS;
        
        // Material SSBO
        poolSizes[3].type = vk::DescriptorType::eStorageBuffer;
        poolSizes[3].descriptorCount = MAX_MATERIALS;
        
        // Light SSBO
        poolSizes[4].type = vk::DescriptorType::eStorageBuffer;
        poolSizes[4].descriptorCount = MAX_MATERIALS;

        vk::DescriptorPoolCreateInfo poolInfo;
        poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = MAX_MATERIALS;
        poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

        pbr.componentDescriptorPool = device.createDescriptorPool(poolInfo);
    }

    {
        std::array<vk::DescriptorPoolSize, 2> poolSizes = {};
    
        // Sampler
        poolSizes[0].type = vk::DescriptorType::eSampler;
        poolSizes[0].descriptorCount = MAX_MATERIALS;
        
        // Texture array
        poolSizes[1].type = vk::DescriptorType::eSampledImage;
        poolSizes[1].descriptorCount = MAX_MATERIALS;
        
        vk::DescriptorPoolCreateInfo poolInfo;
        poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = MAX_MATERIALS;
        poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

        pbr.textureDescriptorPool = device.createDescriptorPool(poolInfo);
    }

    /* ------ TEXCOORD SURFACE DESCRIPTOR POOL  ------ */
    {
        std::array<vk::DescriptorPoolSize, 5> poolSizes = {};
    
        // Entity SSBO
        poolSizes[0].type = vk::DescriptorType::eStorageBuffer;
        poolSizes[0].descriptorCount = MAX_MATERIALS;
        
        // Transform SSBO
        poolSizes[1].type = vk::DescriptorType::eStorageBuffer;
        poolSizes[1].descriptorCount = MAX_MATERIALS;
        
        // Camera SSBO
        poolSizes[2].type = vk::DescriptorType::eStorageBuffer;
        poolSizes[2].descriptorCount = MAX_MATERIALS;
        
        // Material SSBO
        poolSizes[3].type = vk::DescriptorType::eStorageBuffer;
        poolSizes[3].descriptorCount = MAX_MATERIALS;
        
        // Light SSBO
        poolSizes[4].type = vk::DescriptorType::eStorageBuffer;
        poolSizes[4].descriptorCount = MAX_MATERIALS;

        vk::DescriptorPoolCreateInfo poolInfo;
        poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = MAX_MATERIALS;
        poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

        texcoordsurface.componentDescriptorPool = device.createDescriptorPool(poolInfo);
    }

    {
        std::array<vk::DescriptorPoolSize, 2> poolSizes = {};
    
        // Sampler
        poolSizes[0].type = vk::DescriptorType::eSampler;
        poolSizes[0].descriptorCount = MAX_MATERIALS;
        
        // Texture array
        poolSizes[1].type = vk::DescriptorType::eSampledImage;
        poolSizes[1].descriptorCount = MAX_MATERIALS;
        
        vk::DescriptorPoolCreateInfo poolInfo;
        poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = MAX_MATERIALS;
        poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

        texcoordsurface.textureDescriptorPool = device.createDescriptorPool(poolInfo);
    }

    /* ------ NORMAL SURFACE DESCRIPTOR POOL  ------ */
    {
        std::array<vk::DescriptorPoolSize, 5> poolSizes = {};
    
        // Entity SSBO
        poolSizes[0].type = vk::DescriptorType::eStorageBuffer;
        poolSizes[0].descriptorCount = MAX_MATERIALS;
        
        // Transform SSBO
        poolSizes[1].type = vk::DescriptorType::eStorageBuffer;
        poolSizes[1].descriptorCount = MAX_MATERIALS;
        
        // Camera SSBO
        poolSizes[2].type = vk::DescriptorType::eStorageBuffer;
        poolSizes[2].descriptorCount = MAX_MATERIALS;
        
        // Material SSBO
        poolSizes[3].type = vk::DescriptorType::eStorageBuffer;
        poolSizes[3].descriptorCount = MAX_MATERIALS;
        
        // Light SSBO
        poolSizes[4].type = vk::DescriptorType::eStorageBuffer;
        poolSizes[4].descriptorCount = MAX_MATERIALS;

        vk::DescriptorPoolCreateInfo poolInfo;
        poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = MAX_MATERIALS;
        poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

        normalsurface.componentDescriptorPool = device.createDescriptorPool(poolInfo);
    }

    {
        std::array<vk::DescriptorPoolSize, 2> poolSizes = {};
    
        // Sampler
        poolSizes[0].type = vk::DescriptorType::eSampler;
        poolSizes[0].descriptorCount = MAX_MATERIALS;
        
        // Texture array
        poolSizes[1].type = vk::DescriptorType::eSampledImage;
        poolSizes[1].descriptorCount = MAX_MATERIALS;
        
        vk::DescriptorPoolCreateInfo poolInfo;
        poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = MAX_MATERIALS;
        poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

        normalsurface.textureDescriptorPool = device.createDescriptorPool(poolInfo);
    }
}

void Material::CreateDescriptorSets()
{
    auto vulkan = Libraries::Vulkan::Get();
    auto device = vulkan->get_device();
    
    /* ------ UNIFORM COLOR DESCRIPTOR SET  ------ */
    {

        /* This is experimental. Typically descriptor sets aren't static. */
        vk::DescriptorSetLayout layouts[] = { uniformColor.componentDescriptorSetLayout };
        std::array<vk::WriteDescriptorSet, 5> descriptorWrites = {};
        if (uniformColor.componentDescriptorSet == vk::DescriptorSet())
        {
            vk::DescriptorSetAllocateInfo allocInfo;
            allocInfo.descriptorPool = uniformColor.componentDescriptorPool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = layouts;
            uniformColor.componentDescriptorSet = device.allocateDescriptorSets(allocInfo)[0];
        }

        // Entity SSBO
        vk::DescriptorBufferInfo entityBufferInfo;
        entityBufferInfo.buffer = Entity::GetSSBO();
        entityBufferInfo.offset = 0;
        entityBufferInfo.range = Entity::GetSSBOSize();

        descriptorWrites[0].dstSet = uniformColor.componentDescriptorSet;
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = vk::DescriptorType::eStorageBuffer;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &entityBufferInfo;

        // Transform SSBO
        vk::DescriptorBufferInfo transformBufferInfo;
        transformBufferInfo.buffer = Transform::GetSSBO();
        transformBufferInfo.offset = 0;
        transformBufferInfo.range = Transform::GetSSBOSize();

        descriptorWrites[1].dstSet = uniformColor.componentDescriptorSet;
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = vk::DescriptorType::eStorageBuffer;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &transformBufferInfo;

        // Camera SSBO
        vk::DescriptorBufferInfo cameraBufferInfo;
        cameraBufferInfo.buffer = Camera::GetSSBO();
        cameraBufferInfo.offset = 0;
        cameraBufferInfo.range = Camera::GetSSBOSize();

        descriptorWrites[2].dstSet = uniformColor.componentDescriptorSet;
        descriptorWrites[2].dstBinding = 2;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType = vk::DescriptorType::eStorageBuffer;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pBufferInfo = &cameraBufferInfo;

        // Material SSBO
        vk::DescriptorBufferInfo materialBufferInfo;
        materialBufferInfo.buffer = Material::GetSSBO();
        materialBufferInfo.offset = 0;
        materialBufferInfo.range = Material::GetSSBOSize();

        descriptorWrites[3].dstSet = uniformColor.componentDescriptorSet;
        descriptorWrites[3].dstBinding = 3;
        descriptorWrites[3].dstArrayElement = 0;
        descriptorWrites[3].descriptorType = vk::DescriptorType::eStorageBuffer;
        descriptorWrites[3].descriptorCount = 1;
        descriptorWrites[3].pBufferInfo = &materialBufferInfo;

        // Light SSBO
        vk::DescriptorBufferInfo lightBufferInfo;
        lightBufferInfo.buffer = Light::GetSSBO();
        lightBufferInfo.offset = 0;
        lightBufferInfo.range = Light::GetSSBOSize();

        descriptorWrites[4].dstSet = uniformColor.componentDescriptorSet;
        descriptorWrites[4].dstBinding = 4;
        descriptorWrites[4].dstArrayElement = 0;
        descriptorWrites[4].descriptorType = vk::DescriptorType::eStorageBuffer;
        descriptorWrites[4].descriptorCount = 1;
        descriptorWrites[4].pBufferInfo = &lightBufferInfo;
        
        device.updateDescriptorSets((uint32_t)descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
    }

    {
        vk::DescriptorSetLayout layouts[] = { uniformColor.textureDescriptorSetLayout };
        std::array<vk::WriteDescriptorSet, 2> descriptorWrites = {};
        
        if (uniformColor.textureDescriptorSet == vk::DescriptorSet())
        {
            vk::DescriptorSetAllocateInfo allocInfo;
            allocInfo.descriptorPool = uniformColor.textureDescriptorPool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = layouts;

            uniformColor.textureDescriptorSet = device.allocateDescriptorSets(allocInfo)[0];
        }

        auto imageLayouts = Texture::Get2DLayouts();
        auto imageViews = Texture::Get2DImageViews();
        auto samplers = Texture::Get2DSamplers();

        // Samplers
        vk::DescriptorImageInfo samplerDescriptorInfos[MAX_TEXTURES];
        for (int i = 0; i < MAX_TEXTURES; ++i) 
        {
            samplerDescriptorInfos[i].sampler = samplers[i];
        }

        descriptorWrites[0].dstSet = uniformColor.textureDescriptorSet;
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = vk::DescriptorType::eSampler;
        descriptorWrites[0].descriptorCount = MAX_TEXTURES;
        descriptorWrites[0].pImageInfo = samplerDescriptorInfos;

        // Textures
        vk::DescriptorImageInfo textureDescriptorInfos[MAX_TEXTURES];
        for (int i = 0; i < MAX_TEXTURES; ++i) 
        {
            textureDescriptorInfos[i].sampler = nullptr;
            textureDescriptorInfos[i].imageLayout = imageLayouts[i];
            textureDescriptorInfos[i].imageView = imageViews[i];
        }

        descriptorWrites[1].dstSet = uniformColor.textureDescriptorSet;
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = vk::DescriptorType::eSampledImage;
        descriptorWrites[1].descriptorCount = MAX_TEXTURES;
        descriptorWrites[1].pImageInfo = textureDescriptorInfos;
        
        device.updateDescriptorSets((uint32_t)descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
    }

    /* ------ BLINN DESCRIPTOR SET  ------ */
    {
        /* This is experimental. Typically descriptor sets aren't static. */
        vk::DescriptorSetLayout layouts[] = { blinn.componentDescriptorSetLayout };
        std::array<vk::WriteDescriptorSet, 5> descriptorWrites = {};
        if (blinn.componentDescriptorSet == vk::DescriptorSet())
        {
            vk::DescriptorSetAllocateInfo allocInfo;
            allocInfo.descriptorPool = blinn.componentDescriptorPool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = layouts;
            blinn.componentDescriptorSet = device.allocateDescriptorSets(allocInfo)[0];
        }

        // Entity SSBO
        vk::DescriptorBufferInfo entityBufferInfo;
        entityBufferInfo.buffer = Entity::GetSSBO();
        entityBufferInfo.offset = 0;
        entityBufferInfo.range = Entity::GetSSBOSize();

        descriptorWrites[0].dstSet = blinn.componentDescriptorSet;
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = vk::DescriptorType::eStorageBuffer;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &entityBufferInfo;

        // Transform SSBO
        vk::DescriptorBufferInfo transformBufferInfo;
        transformBufferInfo.buffer = Transform::GetSSBO();
        transformBufferInfo.offset = 0;
        transformBufferInfo.range = Transform::GetSSBOSize();

        descriptorWrites[1].dstSet = blinn.componentDescriptorSet;
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = vk::DescriptorType::eStorageBuffer;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &transformBufferInfo;

        // Camera SSBO
        vk::DescriptorBufferInfo cameraBufferInfo;
        cameraBufferInfo.buffer = Camera::GetSSBO();
        cameraBufferInfo.offset = 0;
        cameraBufferInfo.range = Camera::GetSSBOSize();

        descriptorWrites[2].dstSet = blinn.componentDescriptorSet;
        descriptorWrites[2].dstBinding = 2;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType = vk::DescriptorType::eStorageBuffer;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pBufferInfo = &cameraBufferInfo;

        // Material SSBO
        vk::DescriptorBufferInfo materialBufferInfo;
        materialBufferInfo.buffer = Material::GetSSBO();
        materialBufferInfo.offset = 0;
        materialBufferInfo.range = Material::GetSSBOSize();

        descriptorWrites[3].dstSet = blinn.componentDescriptorSet;
        descriptorWrites[3].dstBinding = 3;
        descriptorWrites[3].dstArrayElement = 0;
        descriptorWrites[3].descriptorType = vk::DescriptorType::eStorageBuffer;
        descriptorWrites[3].descriptorCount = 1;
        descriptorWrites[3].pBufferInfo = &materialBufferInfo;

        // Light SSBO
        vk::DescriptorBufferInfo lightBufferInfo;
        lightBufferInfo.buffer = Light::GetSSBO();
        lightBufferInfo.offset = 0;
        lightBufferInfo.range = Light::GetSSBOSize();

        descriptorWrites[4].dstSet = blinn.componentDescriptorSet;
        descriptorWrites[4].dstBinding = 4;
        descriptorWrites[4].dstArrayElement = 0;
        descriptorWrites[4].descriptorType = vk::DescriptorType::eStorageBuffer;
        descriptorWrites[4].descriptorCount = 1;
        descriptorWrites[4].pBufferInfo = &lightBufferInfo;
        
        device.updateDescriptorSets((uint32_t)descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
    }

    {
        vk::DescriptorSetLayout layouts[] = { blinn.textureDescriptorSetLayout };
        std::array<vk::WriteDescriptorSet, 2> descriptorWrites = {};
        if (blinn.textureDescriptorSet == vk::DescriptorSet())
        {
            vk::DescriptorSetAllocateInfo allocInfo;
            allocInfo.descriptorPool = blinn.textureDescriptorPool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = layouts;

            blinn.textureDescriptorSet = device.allocateDescriptorSets(allocInfo)[0];
        }

        auto imageLayouts = Texture::Get2DLayouts();
        auto imageViews = Texture::Get2DImageViews();
        auto samplers = Texture::Get2DSamplers();

        // Samplers
        vk::DescriptorImageInfo samplerDescriptorInfos[MAX_TEXTURES];
        for (int i = 0; i < MAX_TEXTURES; ++i) 
        {
            samplerDescriptorInfos[i].sampler = samplers[i];
        }

        descriptorWrites[0].dstSet = blinn.textureDescriptorSet;
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = vk::DescriptorType::eSampler;
        descriptorWrites[0].descriptorCount = MAX_TEXTURES;
        descriptorWrites[0].pImageInfo = samplerDescriptorInfos;

        // Textures
        vk::DescriptorImageInfo textureDescriptorInfos[MAX_TEXTURES];
        for (int i = 0; i < MAX_TEXTURES; ++i) 
        {
            textureDescriptorInfos[i].sampler = nullptr;
            textureDescriptorInfos[i].imageLayout = imageLayouts[i];
            textureDescriptorInfos[i].imageView = imageViews[i];
        }

        descriptorWrites[1].dstSet = blinn.textureDescriptorSet;
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = vk::DescriptorType::eSampledImage;
        descriptorWrites[1].descriptorCount = MAX_TEXTURES;
        descriptorWrites[1].pImageInfo = textureDescriptorInfos;
        
        device.updateDescriptorSets((uint32_t)descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
    }

    /* ------ PBR DESCRIPTOR SET  ------ */
    {
        /* This is experimental. Typically descriptor sets aren't static. */
        vk::DescriptorSetLayout layouts[] = { pbr.componentDescriptorSetLayout };
        std::array<vk::WriteDescriptorSet, 5> descriptorWrites = {};
        if (pbr.componentDescriptorSet == vk::DescriptorSet())
        {
            vk::DescriptorSetAllocateInfo allocInfo;
            allocInfo.descriptorPool = pbr.componentDescriptorPool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = layouts;
            pbr.componentDescriptorSet = device.allocateDescriptorSets(allocInfo)[0];
        }

        // Entity SSBO
        vk::DescriptorBufferInfo entityBufferInfo;
        entityBufferInfo.buffer = Entity::GetSSBO();
        entityBufferInfo.offset = 0;
        entityBufferInfo.range = Entity::GetSSBOSize();

        descriptorWrites[0].dstSet = pbr.componentDescriptorSet;
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = vk::DescriptorType::eStorageBuffer;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &entityBufferInfo;

        // Transform SSBO
        vk::DescriptorBufferInfo transformBufferInfo;
        transformBufferInfo.buffer = Transform::GetSSBO();
        transformBufferInfo.offset = 0;
        transformBufferInfo.range = Transform::GetSSBOSize();

        descriptorWrites[1].dstSet = pbr.componentDescriptorSet;
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = vk::DescriptorType::eStorageBuffer;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &transformBufferInfo;

        // Camera SSBO
        vk::DescriptorBufferInfo cameraBufferInfo;
        cameraBufferInfo.buffer = Camera::GetSSBO();
        cameraBufferInfo.offset = 0;
        cameraBufferInfo.range = Camera::GetSSBOSize();

        descriptorWrites[2].dstSet = pbr.componentDescriptorSet;
        descriptorWrites[2].dstBinding = 2;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType = vk::DescriptorType::eStorageBuffer;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pBufferInfo = &cameraBufferInfo;

        // Material SSBO
        vk::DescriptorBufferInfo materialBufferInfo;
        materialBufferInfo.buffer = Material::GetSSBO();
        materialBufferInfo.offset = 0;
        materialBufferInfo.range = Material::GetSSBOSize();

        descriptorWrites[3].dstSet = pbr.componentDescriptorSet;
        descriptorWrites[3].dstBinding = 3;
        descriptorWrites[3].dstArrayElement = 0;
        descriptorWrites[3].descriptorType = vk::DescriptorType::eStorageBuffer;
        descriptorWrites[3].descriptorCount = 1;
        descriptorWrites[3].pBufferInfo = &materialBufferInfo;

        // Light SSBO
        vk::DescriptorBufferInfo lightBufferInfo;
        lightBufferInfo.buffer = Light::GetSSBO();
        lightBufferInfo.offset = 0;
        lightBufferInfo.range = Light::GetSSBOSize();

        descriptorWrites[4].dstSet = pbr.componentDescriptorSet;
        descriptorWrites[4].dstBinding = 4;
        descriptorWrites[4].dstArrayElement = 0;
        descriptorWrites[4].descriptorType = vk::DescriptorType::eStorageBuffer;
        descriptorWrites[4].descriptorCount = 1;
        descriptorWrites[4].pBufferInfo = &lightBufferInfo;
        
        device.updateDescriptorSets((uint32_t)descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
    }

    {
        vk::DescriptorSetLayout layouts[] = { pbr.textureDescriptorSetLayout };
        std::array<vk::WriteDescriptorSet, 2> descriptorWrites = {};
        if (pbr.textureDescriptorSet == vk::DescriptorSet())
        {
            vk::DescriptorSetAllocateInfo allocInfo;
            allocInfo.descriptorPool = pbr.textureDescriptorPool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = layouts;

            pbr.textureDescriptorSet = device.allocateDescriptorSets(allocInfo)[0];
        }

        auto imageLayouts = Texture::Get2DLayouts();
        auto imageViews = Texture::Get2DImageViews();
        auto samplers = Texture::Get2DSamplers();

        // Samplers
        vk::DescriptorImageInfo samplerDescriptorInfos[MAX_TEXTURES];
        for (int i = 0; i < MAX_TEXTURES; ++i) 
        {
            samplerDescriptorInfos[i].sampler = samplers[i];
        }

        descriptorWrites[0].dstSet = pbr.textureDescriptorSet;
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = vk::DescriptorType::eSampler;
        descriptorWrites[0].descriptorCount = MAX_TEXTURES;
        descriptorWrites[0].pImageInfo = samplerDescriptorInfos;

        // Textures
        vk::DescriptorImageInfo textureDescriptorInfos[MAX_TEXTURES];
        for (int i = 0; i < MAX_TEXTURES; ++i) 
        {
            textureDescriptorInfos[i].sampler = nullptr;
            textureDescriptorInfos[i].imageLayout = imageLayouts[i];
            textureDescriptorInfos[i].imageView = imageViews[i];
        }

        descriptorWrites[1].dstSet = pbr.textureDescriptorSet;
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = vk::DescriptorType::eSampledImage;
        descriptorWrites[1].descriptorCount = MAX_TEXTURES;
        descriptorWrites[1].pImageInfo = textureDescriptorInfos;
        
        device.updateDescriptorSets((uint32_t)descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
    }

    /* ------ TEXCOORD SURFACE DESCRIPTOR SET  ------ */
    {
        /* This is experimental. Typically descriptor sets aren't static. */
        vk::DescriptorSetLayout layouts[] = { texcoordsurface.componentDescriptorSetLayout };
        std::array<vk::WriteDescriptorSet, 5> descriptorWrites = {};
        if (texcoordsurface.componentDescriptorSet == vk::DescriptorSet())
        {
            vk::DescriptorSetAllocateInfo allocInfo;
            allocInfo.descriptorPool = texcoordsurface.componentDescriptorPool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = layouts;
            texcoordsurface.componentDescriptorSet = device.allocateDescriptorSets(allocInfo)[0];
        }

        // Entity SSBO
        vk::DescriptorBufferInfo entityBufferInfo;
        entityBufferInfo.buffer = Entity::GetSSBO();
        entityBufferInfo.offset = 0;
        entityBufferInfo.range = Entity::GetSSBOSize();

        descriptorWrites[0].dstSet = texcoordsurface.componentDescriptorSet;
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = vk::DescriptorType::eStorageBuffer;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &entityBufferInfo;

        // Transform SSBO
        vk::DescriptorBufferInfo transformBufferInfo;
        transformBufferInfo.buffer = Transform::GetSSBO();
        transformBufferInfo.offset = 0;
        transformBufferInfo.range = Transform::GetSSBOSize();

        descriptorWrites[1].dstSet = texcoordsurface.componentDescriptorSet;
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = vk::DescriptorType::eStorageBuffer;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &transformBufferInfo;

        // Camera SSBO
        vk::DescriptorBufferInfo cameraBufferInfo;
        cameraBufferInfo.buffer = Camera::GetSSBO();
        cameraBufferInfo.offset = 0;
        cameraBufferInfo.range = Camera::GetSSBOSize();

        descriptorWrites[2].dstSet = texcoordsurface.componentDescriptorSet;
        descriptorWrites[2].dstBinding = 2;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType = vk::DescriptorType::eStorageBuffer;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pBufferInfo = &cameraBufferInfo;

        // Material SSBO
        vk::DescriptorBufferInfo materialBufferInfo;
        materialBufferInfo.buffer = Material::GetSSBO();
        materialBufferInfo.offset = 0;
        materialBufferInfo.range = Material::GetSSBOSize();

        descriptorWrites[3].dstSet = texcoordsurface.componentDescriptorSet;
        descriptorWrites[3].dstBinding = 3;
        descriptorWrites[3].dstArrayElement = 0;
        descriptorWrites[3].descriptorType = vk::DescriptorType::eStorageBuffer;
        descriptorWrites[3].descriptorCount = 1;
        descriptorWrites[3].pBufferInfo = &materialBufferInfo;

        // Light SSBO
        vk::DescriptorBufferInfo lightBufferInfo;
        lightBufferInfo.buffer = Light::GetSSBO();
        lightBufferInfo.offset = 0;
        lightBufferInfo.range = Light::GetSSBOSize();

        descriptorWrites[4].dstSet = texcoordsurface.componentDescriptorSet;
        descriptorWrites[4].dstBinding = 4;
        descriptorWrites[4].dstArrayElement = 0;
        descriptorWrites[4].descriptorType = vk::DescriptorType::eStorageBuffer;
        descriptorWrites[4].descriptorCount = 1;
        descriptorWrites[4].pBufferInfo = &lightBufferInfo;
        
        device.updateDescriptorSets((uint32_t)descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
    }

    {
        vk::DescriptorSetLayout layouts[] = { texcoordsurface.textureDescriptorSetLayout };
        std::array<vk::WriteDescriptorSet, 2> descriptorWrites = {};
        if (texcoordsurface.textureDescriptorSet == vk::DescriptorSet())
        {
            vk::DescriptorSetAllocateInfo allocInfo;
            allocInfo.descriptorPool = texcoordsurface.textureDescriptorPool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = layouts;

            texcoordsurface.textureDescriptorSet = device.allocateDescriptorSets(allocInfo)[0];
        }

        auto imageLayouts = Texture::Get2DLayouts();
        auto imageViews = Texture::Get2DImageViews();
        auto samplers = Texture::Get2DSamplers();

        // Samplers
        vk::DescriptorImageInfo samplerDescriptorInfos[MAX_TEXTURES];
        for (int i = 0; i < MAX_TEXTURES; ++i) 
        {
            samplerDescriptorInfos[i].sampler = samplers[i];
        }

        descriptorWrites[0].dstSet = texcoordsurface.textureDescriptorSet;
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = vk::DescriptorType::eSampler;
        descriptorWrites[0].descriptorCount = MAX_TEXTURES;
        descriptorWrites[0].pImageInfo = samplerDescriptorInfos;

        // Textures
        vk::DescriptorImageInfo textureDescriptorInfos[MAX_TEXTURES];
        for (int i = 0; i < MAX_TEXTURES; ++i) 
        {
            textureDescriptorInfos[i].sampler = nullptr;
            textureDescriptorInfos[i].imageLayout = imageLayouts[i];
            textureDescriptorInfos[i].imageView = imageViews[i];
        }

        descriptorWrites[1].dstSet = texcoordsurface.textureDescriptorSet;
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = vk::DescriptorType::eSampledImage;
        descriptorWrites[1].descriptorCount = MAX_TEXTURES;
        descriptorWrites[1].pImageInfo = textureDescriptorInfos;
        
        device.updateDescriptorSets((uint32_t)descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
    }

    /* ------ NORMAL SURFACE DESCRIPTOR SET  ------ */
    {
        /* This is experimental. Typically descriptor sets aren't static. */
        vk::DescriptorSetLayout layouts[] = { normalsurface.componentDescriptorSetLayout };
        std::array<vk::WriteDescriptorSet, 5> descriptorWrites = {};
        if (normalsurface.componentDescriptorSet == vk::DescriptorSet())
        {
            vk::DescriptorSetAllocateInfo allocInfo;
            allocInfo.descriptorPool = normalsurface.componentDescriptorPool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = layouts;
            normalsurface.componentDescriptorSet = device.allocateDescriptorSets(allocInfo)[0];
        }

        // Entity SSBO
        vk::DescriptorBufferInfo entityBufferInfo;
        entityBufferInfo.buffer = Entity::GetSSBO();
        entityBufferInfo.offset = 0;
        entityBufferInfo.range = Entity::GetSSBOSize();

        descriptorWrites[0].dstSet = normalsurface.componentDescriptorSet;
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = vk::DescriptorType::eStorageBuffer;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &entityBufferInfo;

        // Transform SSBO
        vk::DescriptorBufferInfo transformBufferInfo;
        transformBufferInfo.buffer = Transform::GetSSBO();
        transformBufferInfo.offset = 0;
        transformBufferInfo.range = Transform::GetSSBOSize();

        descriptorWrites[1].dstSet = normalsurface.componentDescriptorSet;
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = vk::DescriptorType::eStorageBuffer;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &transformBufferInfo;

        // Camera SSBO
        vk::DescriptorBufferInfo cameraBufferInfo;
        cameraBufferInfo.buffer = Camera::GetSSBO();
        cameraBufferInfo.offset = 0;
        cameraBufferInfo.range = Camera::GetSSBOSize();

        descriptorWrites[2].dstSet = normalsurface.componentDescriptorSet;
        descriptorWrites[2].dstBinding = 2;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType = vk::DescriptorType::eStorageBuffer;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pBufferInfo = &cameraBufferInfo;

        // Material SSBO
        vk::DescriptorBufferInfo materialBufferInfo;
        materialBufferInfo.buffer = Material::GetSSBO();
        materialBufferInfo.offset = 0;
        materialBufferInfo.range = Material::GetSSBOSize();

        descriptorWrites[3].dstSet = normalsurface.componentDescriptorSet;
        descriptorWrites[3].dstBinding = 3;
        descriptorWrites[3].dstArrayElement = 0;
        descriptorWrites[3].descriptorType = vk::DescriptorType::eStorageBuffer;
        descriptorWrites[3].descriptorCount = 1;
        descriptorWrites[3].pBufferInfo = &materialBufferInfo;

        // Light SSBO
        vk::DescriptorBufferInfo lightBufferInfo;
        lightBufferInfo.buffer = Light::GetSSBO();
        lightBufferInfo.offset = 0;
        lightBufferInfo.range = Light::GetSSBOSize();

        descriptorWrites[4].dstSet = normalsurface.componentDescriptorSet;
        descriptorWrites[4].dstBinding = 4;
        descriptorWrites[4].dstArrayElement = 0;
        descriptorWrites[4].descriptorType = vk::DescriptorType::eStorageBuffer;
        descriptorWrites[4].descriptorCount = 1;
        descriptorWrites[4].pBufferInfo = &lightBufferInfo;
        
        device.updateDescriptorSets((uint32_t)descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
    }

    {
        vk::DescriptorSetLayout layouts[] = { normalsurface.textureDescriptorSetLayout };
        std::array<vk::WriteDescriptorSet, 2> descriptorWrites = {};
        if (normalsurface.textureDescriptorSet == vk::DescriptorSet())
        {
            vk::DescriptorSetAllocateInfo allocInfo;
            allocInfo.descriptorPool = normalsurface.textureDescriptorPool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = layouts;

            normalsurface.textureDescriptorSet = device.allocateDescriptorSets(allocInfo)[0];
        }

        auto imageLayouts = Texture::Get2DLayouts();
        auto imageViews = Texture::Get2DImageViews();
        auto samplers = Texture::Get2DSamplers();

        // Samplers
        vk::DescriptorImageInfo samplerDescriptorInfos[MAX_TEXTURES];
        for (int i = 0; i < MAX_TEXTURES; ++i) 
        {
            samplerDescriptorInfos[i].sampler = samplers[i];
        }

        descriptorWrites[0].dstSet = normalsurface.textureDescriptorSet;
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = vk::DescriptorType::eSampler;
        descriptorWrites[0].descriptorCount = MAX_TEXTURES;
        descriptorWrites[0].pImageInfo = samplerDescriptorInfos;

        // Textures
        vk::DescriptorImageInfo textureDescriptorInfos[MAX_TEXTURES];
        for (int i = 0; i < MAX_TEXTURES; ++i) 
        {
            textureDescriptorInfos[i].sampler = nullptr;
            textureDescriptorInfos[i].imageLayout = imageLayouts[i];
            textureDescriptorInfos[i].imageView = imageViews[i];
        }

        descriptorWrites[1].dstSet = normalsurface.textureDescriptorSet;
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = vk::DescriptorType::eSampledImage;
        descriptorWrites[1].descriptorCount = MAX_TEXTURES;
        descriptorWrites[1].pImageInfo = textureDescriptorInfos;
        
        device.updateDescriptorSets((uint32_t)descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
    }
}

void Material::CreateVertexInputBindingDescriptions() {
    /* UNIFORM COLOR */
    {
        vk::VertexInputBindingDescription pointBindingDescription;
        pointBindingDescription.binding = 0;
        pointBindingDescription.stride = 3 * sizeof(float);
        pointBindingDescription.inputRate = vk::VertexInputRate::eVertex;

        vk::VertexInputBindingDescription colorBindingDescription;
        colorBindingDescription.binding = 1;
        colorBindingDescription.stride = 4 * sizeof(float);
        colorBindingDescription.inputRate = vk::VertexInputRate::eVertex;

        vk::VertexInputBindingDescription normalBindingDescription;
        normalBindingDescription.binding = 2;
        normalBindingDescription.stride = 3 * sizeof(float);
        normalBindingDescription.inputRate = vk::VertexInputRate::eVertex;

        vk::VertexInputBindingDescription texcoordBindingDescription;
        texcoordBindingDescription.binding = 3;
        texcoordBindingDescription.stride = 2 * sizeof(float);
        texcoordBindingDescription.inputRate = vk::VertexInputRate::eVertex;

        uniformColor.vertexInputBindingDescriptions = { pointBindingDescription, colorBindingDescription, normalBindingDescription, texcoordBindingDescription };
    }

    /* BLINN */
    {
        vk::VertexInputBindingDescription pointBindingDescription;
        pointBindingDescription.binding = 0;
        pointBindingDescription.stride = 3 * sizeof(float);
        pointBindingDescription.inputRate = vk::VertexInputRate::eVertex;

        vk::VertexInputBindingDescription colorBindingDescription;
        colorBindingDescription.binding = 1;
        colorBindingDescription.stride = 4 * sizeof(float);
        colorBindingDescription.inputRate = vk::VertexInputRate::eVertex;

        vk::VertexInputBindingDescription normalBindingDescription;
        normalBindingDescription.binding = 2;
        normalBindingDescription.stride = 3 * sizeof(float);
        normalBindingDescription.inputRate = vk::VertexInputRate::eVertex;

        vk::VertexInputBindingDescription texcoordBindingDescription;
        texcoordBindingDescription.binding = 3;
        texcoordBindingDescription.stride = 2 * sizeof(float);
        texcoordBindingDescription.inputRate = vk::VertexInputRate::eVertex;

        blinn.vertexInputBindingDescriptions = { pointBindingDescription, colorBindingDescription, normalBindingDescription, texcoordBindingDescription };
    }

    /* PBR */
    {
        vk::VertexInputBindingDescription pointBindingDescription;
        pointBindingDescription.binding = 0;
        pointBindingDescription.stride = 3 * sizeof(float);
        pointBindingDescription.inputRate = vk::VertexInputRate::eVertex;

        vk::VertexInputBindingDescription colorBindingDescription;
        colorBindingDescription.binding = 1;
        colorBindingDescription.stride = 4 * sizeof(float);
        colorBindingDescription.inputRate = vk::VertexInputRate::eVertex;

        vk::VertexInputBindingDescription normalBindingDescription;
        normalBindingDescription.binding = 2;
        normalBindingDescription.stride = 3 * sizeof(float);
        normalBindingDescription.inputRate = vk::VertexInputRate::eVertex;

        vk::VertexInputBindingDescription texcoordBindingDescription;
        texcoordBindingDescription.binding = 3;
        texcoordBindingDescription.stride = 2 * sizeof(float);
        texcoordBindingDescription.inputRate = vk::VertexInputRate::eVertex;

        pbr.vertexInputBindingDescriptions = { pointBindingDescription, colorBindingDescription, normalBindingDescription, texcoordBindingDescription };
    }

    /* TEXCOORD SURFACE */
    {
        vk::VertexInputBindingDescription pointBindingDescription;
        pointBindingDescription.binding = 0;
        pointBindingDescription.stride = 3 * sizeof(float);
        pointBindingDescription.inputRate = vk::VertexInputRate::eVertex;

        vk::VertexInputBindingDescription colorBindingDescription;
        colorBindingDescription.binding = 1;
        colorBindingDescription.stride = 4 * sizeof(float);
        colorBindingDescription.inputRate = vk::VertexInputRate::eVertex;

        vk::VertexInputBindingDescription normalBindingDescription;
        normalBindingDescription.binding = 2;
        normalBindingDescription.stride = 3 * sizeof(float);
        normalBindingDescription.inputRate = vk::VertexInputRate::eVertex;

        vk::VertexInputBindingDescription texcoordBindingDescription;
        texcoordBindingDescription.binding = 3;
        texcoordBindingDescription.stride = 2 * sizeof(float);
        texcoordBindingDescription.inputRate = vk::VertexInputRate::eVertex;

        texcoordsurface.vertexInputBindingDescriptions = { pointBindingDescription, colorBindingDescription, normalBindingDescription, texcoordBindingDescription };
    }

    /* NORMAL SURFACE */
    {
        vk::VertexInputBindingDescription pointBindingDescription;
        pointBindingDescription.binding = 0;
        pointBindingDescription.stride = 3 * sizeof(float);
        pointBindingDescription.inputRate = vk::VertexInputRate::eVertex;

        vk::VertexInputBindingDescription colorBindingDescription;
        colorBindingDescription.binding = 1;
        colorBindingDescription.stride = 4 * sizeof(float);
        colorBindingDescription.inputRate = vk::VertexInputRate::eVertex;

        vk::VertexInputBindingDescription normalBindingDescription;
        normalBindingDescription.binding = 2;
        normalBindingDescription.stride = 3 * sizeof(float);
        normalBindingDescription.inputRate = vk::VertexInputRate::eVertex;

        vk::VertexInputBindingDescription texcoordBindingDescription;
        texcoordBindingDescription.binding = 3;
        texcoordBindingDescription.stride = 2 * sizeof(float);
        texcoordBindingDescription.inputRate = vk::VertexInputRate::eVertex;

        normalsurface.vertexInputBindingDescriptions = { pointBindingDescription, colorBindingDescription, normalBindingDescription, texcoordBindingDescription };
    }
}

void Material::CreateVertexAttributeDescriptions() {
    /* UNIFORM COLOR */
    {
        std::vector<vk::VertexInputAttributeDescription> attributeDescriptions(4);

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[0].offset = 0;

        attributeDescriptions[1].binding = 1;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = vk::Format::eR32G32B32A32Sfloat;
        attributeDescriptions[1].offset = 0;

        attributeDescriptions[2].binding = 2;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[2].offset = 0;

        attributeDescriptions[3].binding = 3;
        attributeDescriptions[3].location = 3;
        attributeDescriptions[3].format = vk::Format::eR32G32Sfloat;
        attributeDescriptions[3].offset = 0;

        uniformColor.vertexInputAttributeDescriptions = attributeDescriptions;
    }

    /* BLINN */
    {
        std::vector<vk::VertexInputAttributeDescription> attributeDescriptions(4);

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[0].offset = 0;

        attributeDescriptions[1].binding = 1;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = vk::Format::eR32G32B32A32Sfloat;
        attributeDescriptions[1].offset = 0;

        attributeDescriptions[2].binding = 2;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[2].offset = 0;

        attributeDescriptions[3].binding = 3;
        attributeDescriptions[3].location = 3;
        attributeDescriptions[3].format = vk::Format::eR32G32Sfloat;
        attributeDescriptions[3].offset = 0;

        blinn.vertexInputAttributeDescriptions = attributeDescriptions;
    }

    /* PBR */
    {
        std::vector<vk::VertexInputAttributeDescription> attributeDescriptions(4);

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[0].offset = 0;

        attributeDescriptions[1].binding = 1;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = vk::Format::eR32G32B32A32Sfloat;
        attributeDescriptions[1].offset = 0;

        attributeDescriptions[2].binding = 2;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[2].offset = 0;

        attributeDescriptions[3].binding = 3;
        attributeDescriptions[3].location = 3;
        attributeDescriptions[3].format = vk::Format::eR32G32Sfloat;
        attributeDescriptions[3].offset = 0;

        pbr.vertexInputAttributeDescriptions = attributeDescriptions;
    }

    /* NORMAL SURFACE */
    {
        std::vector<vk::VertexInputAttributeDescription> attributeDescriptions(4);

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[0].offset = 0;

        attributeDescriptions[1].binding = 1;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = vk::Format::eR32G32B32A32Sfloat;
        attributeDescriptions[1].offset = 0;

        attributeDescriptions[2].binding = 2;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[2].offset = 0;

        attributeDescriptions[3].binding = 3;
        attributeDescriptions[3].location = 3;
        attributeDescriptions[3].format = vk::Format::eR32G32Sfloat;
        attributeDescriptions[3].offset = 0;

        normalsurface.vertexInputAttributeDescriptions = attributeDescriptions;
    }

    /* TEXCOORD SURFACE */
    {
        std::vector<vk::VertexInputAttributeDescription> attributeDescriptions(4);

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[0].offset = 0;

        attributeDescriptions[1].binding = 1;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = vk::Format::eR32G32B32A32Sfloat;
        attributeDescriptions[1].offset = 0;

        attributeDescriptions[2].binding = 2;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[2].offset = 0;

        attributeDescriptions[3].binding = 3;
        attributeDescriptions[3].location = 3;
        attributeDescriptions[3].format = vk::Format::eR32G32Sfloat;
        attributeDescriptions[3].offset = 0;

        texcoordsurface.vertexInputAttributeDescriptions = attributeDescriptions;
    }
}

bool Material::DrawEntity(vk::CommandBuffer &command_buffer, Entity &entity, int32_t &camera_id, std::vector<int32_t> &light_entity_ids)
{
    auto mesh_id = entity.get_mesh();
    auto transform_id = entity.get_transform();
    auto material_id = entity.get_material();
    
    /* Need a mesh and a transform to render. */
    if (mesh_id < 0 || mesh_id >= MAX_MESHES) return false;
    if (transform_id < 0 || transform_id >= MAX_TRANSFORMS) return false;
    if (material_id < 0 || material_id >= MAX_MATERIALS) return false;

    auto m = Mesh::Get((uint32_t) mesh_id);
    if (!m) return false;

    // Setup push constant data
    std::vector<int32_t> push_constants(2 + MAX_LIGHTS);
    push_constants[0] = entity.get_id();
    push_constants[1] = camera_id;
    memcpy(&push_constants[2], light_entity_ids.data(), sizeof(int32_t) * light_entity_ids.size());

    command_buffer.pushConstants(
        blinn.pipelineLayout, 
        vk::ShaderStageFlagBits::eAll,
        0, 
        (2 + MAX_LIGHTS) * sizeof(int32_t),
        push_constants.data()
        );

    auto material = Material::Get(material_id);
    // Render normal
    if (material->renderMode == 1) {
        std::vector<vk::DescriptorSet> descriptorSets = {normalsurface.componentDescriptorSet, normalsurface.textureDescriptorSet};

        command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, Material::normalsurface.pipeline);
        command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, normalsurface.pipelineLayout, 0, 2, descriptorSets.data(), 0, nullptr);
        command_buffer.bindVertexBuffers(0, {m->get_point_buffer(), m->get_color_buffer(), m->get_normal_buffer(), m->get_texcoord_buffer()}, {0,0,0,0});
        command_buffer.bindIndexBuffer(m->get_index_buffer(), 0, vk::IndexType::eUint32);
        command_buffer.drawIndexed(m->get_total_indices(), 1, 0, 0, 0);
    }
    // Render Vertex Colors
    else if (material->renderMode == 2) {
        std::vector<vk::DescriptorSet> descriptorSets = {blinn.componentDescriptorSet, blinn.textureDescriptorSet};

        command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, Material::blinn.pipeline);
        command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, blinn.pipelineLayout, 0, 2, descriptorSets.data(), 0, nullptr);
        command_buffer.bindVertexBuffers(0, {m->get_point_buffer(), m->get_color_buffer(), m->get_normal_buffer(), m->get_texcoord_buffer()}, {0,0,0,0});
        command_buffer.bindIndexBuffer(m->get_index_buffer(), 0, vk::IndexType::eUint32);
        command_buffer.drawIndexed(m->get_total_indices(), 1, 0, 0, 0);
    }
    // Render UVs
    else if (material->renderMode == 3) {
        std::vector<vk::DescriptorSet> descriptorSets = {texcoordsurface.componentDescriptorSet, texcoordsurface.textureDescriptorSet};

        command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, Material::texcoordsurface.pipeline);
        command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, texcoordsurface.pipelineLayout, 0, 2, descriptorSets.data(), 0, nullptr);
        command_buffer.bindVertexBuffers(0, {m->get_point_buffer(), m->get_color_buffer(), m->get_normal_buffer(), m->get_texcoord_buffer()}, {0,0,0,0});
        command_buffer.bindIndexBuffer(m->get_index_buffer(), 0, vk::IndexType::eUint32);
        command_buffer.drawIndexed(m->get_total_indices(), 1, 0, 0, 0);
    }
    // Render PBR
    if (material->renderMode == 4) {
        std::vector<vk::DescriptorSet> descriptorSets = {pbr.componentDescriptorSet, pbr.textureDescriptorSet};

        command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, Material::pbr.pipeline);
        command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pbr.pipelineLayout, 0, 2, descriptorSets.data(), 0, nullptr);
        command_buffer.bindVertexBuffers(0, {m->get_point_buffer(), m->get_color_buffer(), m->get_normal_buffer(), m->get_texcoord_buffer()}, {0,0,0,0});
        command_buffer.bindIndexBuffer(m->get_index_buffer(), 0, vk::IndexType::eUint32);
        command_buffer.drawIndexed(m->get_total_indices(), 1, 0, 0, 0);
    }
    // Render material
    else {
        std::vector<vk::DescriptorSet> descriptorSets = {blinn.componentDescriptorSet, blinn.textureDescriptorSet};

        command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, Material::blinn.pipeline);
        command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, blinn.pipelineLayout, 0, 2, descriptorSets.data(), 0, nullptr);
        command_buffer.bindVertexBuffers(0, {m->get_point_buffer(), m->get_color_buffer(), m->get_normal_buffer(), m->get_texcoord_buffer()}, {0,0,0,0});
        command_buffer.bindIndexBuffer(m->get_index_buffer(), 0, vk::IndexType::eUint32);
        command_buffer.drawIndexed(m->get_total_indices(), 1, 0, 0, 0);
    }

    return true;
}

void Material::CreateSSBO()
{
    auto vulkan = Libraries::Vulkan::Get();
    auto device = vulkan->get_device();
    auto physical_device = vulkan->get_physical_device();

    vk::BufferCreateInfo bufferInfo = {};
    bufferInfo.size = MAX_MATERIALS * sizeof(MaterialStruct);
    bufferInfo.usage = vk::BufferUsageFlagBits::eStorageBuffer;
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;
    ssbo = device.createBuffer(bufferInfo);

    vk::MemoryRequirements memReqs = device.getBufferMemoryRequirements(ssbo);
    vk::MemoryAllocateInfo allocInfo = {};
    allocInfo.allocationSize = memReqs.size;

    vk::PhysicalDeviceMemoryProperties memProperties = physical_device.getMemoryProperties();
    vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
    allocInfo.memoryTypeIndex = vulkan->find_memory_type(memReqs.memoryTypeBits, properties);

    ssboMemory = device.allocateMemory(allocInfo);
    device.bindBufferMemory(ssbo, ssboMemory, 0);

    /* Pin the buffer */
    pinnedMemory = (MaterialStruct*) device.mapMemory(ssboMemory, 0, MAX_MATERIALS * sizeof(MaterialStruct));
}

void Material::UploadSSBO()
{
    if (pinnedMemory == nullptr) return;
    MaterialStruct material_structs[MAX_MATERIALS];
    
    /* TODO: remove this for loop */
    for (int i = 0; i < MAX_MATERIALS; ++i) {
        if (!materials[i].is_initialized()) continue;
        material_structs[i] = materials[i].material_struct;
    };

    /* Copy to GPU mapped memory */
    memcpy(pinnedMemory, material_structs, sizeof(material_structs));
}

vk::Buffer Material::GetSSBO()
{
    return ssbo;
}

uint32_t Material::GetSSBOSize()
{
    return MAX_MATERIALS * sizeof(MaterialStruct);
}

void Material::CleanUp()
{
    auto vulkan = Libraries::Vulkan::Get();
    auto device = vulkan->get_device();
    device.destroyBuffer(ssbo);
    device.unmapMemory(ssboMemory);
    device.freeMemory(ssboMemory);

    device.destroyDescriptorSetLayout(uniformColor.componentDescriptorSetLayout);
    device.destroyDescriptorPool(uniformColor.componentDescriptorPool);

    device.destroyDescriptorSetLayout(blinn.componentDescriptorSetLayout);
    device.destroyDescriptorPool(blinn.componentDescriptorPool);

    device.destroyDescriptorSetLayout(texcoordsurface.componentDescriptorSetLayout);
    device.destroyDescriptorPool(texcoordsurface.componentDescriptorPool);

    device.destroyDescriptorSetLayout(normalsurface.componentDescriptorSetLayout);
    device.destroyDescriptorPool(normalsurface.componentDescriptorPool);

    device.destroyDescriptorSetLayout(uniformColor.textureDescriptorSetLayout);
    device.destroyDescriptorPool(uniformColor.textureDescriptorPool);

    device.destroyDescriptorSetLayout(blinn.textureDescriptorSetLayout);
    device.destroyDescriptorPool(blinn.textureDescriptorPool);

    device.destroyDescriptorSetLayout(texcoordsurface.textureDescriptorSetLayout);
    device.destroyDescriptorPool(texcoordsurface.textureDescriptorPool);

    device.destroyDescriptorSetLayout(normalsurface.textureDescriptorSetLayout);
    device.destroyDescriptorPool(normalsurface.textureDescriptorPool);
}	

/* Static Factory Implementations */
Material* Material::Create(std::string name) {
    return StaticFactory::Create(name, "Material", lookupTable, materials, MAX_MATERIALS);
}

Material* Material::Get(std::string name) {
    return StaticFactory::Get(name, "Material", lookupTable, materials, MAX_MATERIALS);
}

Material* Material::Get(uint32_t id) {
    return StaticFactory::Get(id, "Material", lookupTable, materials, MAX_MATERIALS);
}

bool Material::Delete(std::string name) {
    return StaticFactory::Delete(name, "Material", lookupTable, materials, MAX_MATERIALS);
}

bool Material::Delete(uint32_t id) {
    return StaticFactory::Delete(id, "Material", lookupTable, materials, MAX_MATERIALS);
}

Material* Material::GetFront() {
    return materials;
}

uint32_t Material::GetCount() {
    return MAX_MATERIALS;
}



void Material::use_base_color_texture(uint32_t texture_id) 
{
    this->baseColorTextureID = texture_id;
    this->useBaseColorTexture = true;
    this->material_struct.base_color_texture_id = texture_id;
}

void Material::use_base_color_texture(Texture *texture) 
{
    if (!texture) return;
    this->baseColorTextureID = texture->get_id();
    this->useBaseColorTexture = true;
    this->material_struct.base_color_texture_id = this->baseColorTextureID;
}

void Material::use_base_color_texture(bool use_texture) {
    if (use_texture) {
        this->material_struct.base_color_texture_id = baseColorTextureID;
    }
    else {
        this->material_struct.base_color_texture_id = -1;
    }
    this->useBaseColorTexture = use_texture;
}