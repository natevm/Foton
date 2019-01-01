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


vk::DescriptorSetLayout Material::componentDescriptorSetLayout;
vk::DescriptorSetLayout Material::textureDescriptorSetLayout;
vk::DescriptorPool Material::componentDescriptorPool;
vk::DescriptorPool Material::textureDescriptorPool;
std::vector<vk::VertexInputBindingDescription> Material::vertexInputBindingDescriptions;
std::vector<vk::VertexInputAttributeDescription> Material::vertexInputAttributeDescriptions;
vk::DescriptorSet Material::componentDescriptorSet;
vk::DescriptorSet Material::textureDescriptorSet;

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
        
        CreatePipeline(shaderStages, vertexInputBindingDescriptions, vertexInputAttributeDescriptions, 
            { componentDescriptorSetLayout, textureDescriptorSetLayout }, 
            uniformColor.pipelineParameters, 
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
        
        CreatePipeline(shaderStages, vertexInputBindingDescriptions, vertexInputAttributeDescriptions, 
            { componentDescriptorSetLayout, textureDescriptorSetLayout }, 
            blinn.pipelineParameters, 
            renderpass, 0, 
            blinn.pipeline, blinn.pipelineLayout);

        device.destroyShaderModule(fragShaderModule);
        device.destroyShaderModule(vertShaderModule);
    }

    /* ------ PBR GRAPHICS PIPELINE  ------ */
    {
        std::string ResourcePath = Options::GetResourcePath();
        auto vertShaderCode = readFile(ResourcePath + std::string("/Shaders/SurfaceMaterials/PBRSurface/vert.spv"));
        auto fragShaderCode = readFile(ResourcePath + std::string("/Shaders/SurfaceMaterials/PBRSurface/frag.spv"));

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
        
        CreatePipeline(shaderStages, vertexInputBindingDescriptions, vertexInputAttributeDescriptions, 
            { componentDescriptorSetLayout, textureDescriptorSetLayout }, 
            pbr.pipelineParameters, 
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
        
        CreatePipeline(shaderStages, vertexInputBindingDescriptions, vertexInputAttributeDescriptions, 
            { componentDescriptorSetLayout, textureDescriptorSetLayout }, 
            normalsurface.pipelineParameters, 
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
        
        CreatePipeline(shaderStages, vertexInputBindingDescriptions, vertexInputAttributeDescriptions, 
            { componentDescriptorSetLayout, textureDescriptorSetLayout }, 
            texcoordsurface.pipelineParameters, 
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
    Material::CreateDescriptorPool();
    Material::CreateVertexInputBindingDescriptions();
    Material::CreateVertexAttributeDescriptions();
    Material::CreateSSBO();
    Material::CreateDescriptorSets();
}

void Material::CreateDescriptorSetLayouts()
{
    /* Descriptor set layouts are standardized across shaders for optimized runtime binding */

    auto vulkan = Libraries::Vulkan::Get();
    auto device = vulkan->get_device();

    /* SSBO descriptor bindings */

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

    std::array<vk::DescriptorSetLayoutBinding, 5> ssbobindings = { eboLayoutBinding, tboLayoutBinding, cboLayoutBinding, mboLayoutBinding, lboLayoutBinding};
    vk::DescriptorSetLayoutCreateInfo ssboLayoutInfo;
    ssboLayoutInfo.bindingCount = (uint32_t)ssbobindings.size();
    ssboLayoutInfo.pBindings = ssbobindings.data();
    
    /* Texture descriptor bindings */
    
    // Texture samplers
    vk::DescriptorSetLayoutBinding samplerBinding;
    samplerBinding.descriptorCount = MAX_TEXTURES;
    samplerBinding.binding = 0;
    samplerBinding.descriptorType = vk::DescriptorType::eSampler;
    samplerBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
    samplerBinding.pImmutableSamplers = 0;

    // 2D Textures
    vk::DescriptorSetLayoutBinding texture2DsBinding;
    texture2DsBinding.descriptorCount = MAX_TEXTURES;
    texture2DsBinding.binding = 1;
    texture2DsBinding.descriptorType = vk::DescriptorType::eSampledImage;
    texture2DsBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
    texture2DsBinding.pImmutableSamplers = 0;

    // // Texture Cubes
    // vk::DescriptorSetLayoutBinding textureCubesBinding;
    // textureCubesBinding.descriptorCount = MAX_TEXTURES;
    // textureCubesBinding.binding = 2;
    // textureCubesBinding.descriptorType = vk::DescriptorType::eSampledImage;
    // textureCubesBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
    // textureCubesBinding.pImmutableSamplers = 0;

    // // 3D Textures
    // vk::DescriptorSetLayoutBinding texture3DsBinding;
    // texture3DsBinding.descriptorCount = MAX_TEXTURES;
    // texture3DsBinding.binding = 3;
    // texture3DsBinding.descriptorType = vk::DescriptorType::eSampledImage;
    // texture3DsBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
    // texture3DsBinding.pImmutableSamplers = 0;

    std::array<vk::DescriptorSetLayoutBinding, 2> bindings = {samplerBinding, texture2DsBinding /*, textureCubesBinding, texture3DsBinding */ };
    vk::DescriptorSetLayoutCreateInfo textureLayoutInfo;
    textureLayoutInfo.bindingCount = (uint32_t)bindings.size();
    textureLayoutInfo.pBindings = bindings.data();

    // Create the layouts
    componentDescriptorSetLayout = device.createDescriptorSetLayout(ssboLayoutInfo);
    textureDescriptorSetLayout = device.createDescriptorSetLayout(textureLayoutInfo);
}

void Material::CreateDescriptorPool()
{
    /* Since the descriptor layout is consistent across shaders, the descriptor
        pool can be shared. */

    auto vulkan = Libraries::Vulkan::Get();
    auto device = vulkan->get_device();

    /* SSBO Descriptor Pool Info */
    std::array<vk::DescriptorPoolSize, 5> ssboPoolSizes = {};
    
    // Entity SSBO
    ssboPoolSizes[0].type = vk::DescriptorType::eStorageBuffer;
    ssboPoolSizes[0].descriptorCount = MAX_MATERIALS;
    
    // Transform SSBO
    ssboPoolSizes[1].type = vk::DescriptorType::eStorageBuffer;
    ssboPoolSizes[1].descriptorCount = MAX_MATERIALS;
    
    // Camera SSBO
    ssboPoolSizes[2].type = vk::DescriptorType::eStorageBuffer;
    ssboPoolSizes[2].descriptorCount = MAX_MATERIALS;
    
    // Material SSBO
    ssboPoolSizes[3].type = vk::DescriptorType::eStorageBuffer;
    ssboPoolSizes[3].descriptorCount = MAX_MATERIALS;
    
    // Light SSBO
    ssboPoolSizes[4].type = vk::DescriptorType::eStorageBuffer;
    ssboPoolSizes[4].descriptorCount = MAX_MATERIALS;

    vk::DescriptorPoolCreateInfo ssboPoolInfo;
    ssboPoolInfo.poolSizeCount = (uint32_t)ssboPoolSizes.size();
    ssboPoolInfo.pPoolSizes = ssboPoolSizes.data();
    ssboPoolInfo.maxSets = MAX_MATERIALS;
    ssboPoolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

    /* Texture Descriptor Pool Info */
    std::array<vk::DescriptorPoolSize, 2> texturePoolSizes = {};
    
    // Sampler
    texturePoolSizes[0].type = vk::DescriptorType::eSampler;
    texturePoolSizes[0].descriptorCount = MAX_MATERIALS;
    
    // Texture array
    texturePoolSizes[1].type = vk::DescriptorType::eSampledImage;
    texturePoolSizes[1].descriptorCount = MAX_MATERIALS;
    
    vk::DescriptorPoolCreateInfo texturePoolInfo;
    texturePoolInfo.poolSizeCount = (uint32_t)texturePoolSizes.size();
    texturePoolInfo.pPoolSizes = texturePoolSizes.data();
    texturePoolInfo.maxSets = MAX_MATERIALS;
    texturePoolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

    // Create the pools
    componentDescriptorPool = device.createDescriptorPool(ssboPoolInfo);
    textureDescriptorPool = device.createDescriptorPool(texturePoolInfo);
}

void Material::CreateDescriptorSets()
{
    auto vulkan = Libraries::Vulkan::Get();
    auto device = vulkan->get_device();
    
    /* ------ Component Descriptor Set  ------ */
    vk::DescriptorSetLayout ssboLayouts[] = { componentDescriptorSetLayout };
    std::array<vk::WriteDescriptorSet, 5> ssboDescriptorWrites = {};
    if (componentDescriptorSet == vk::DescriptorSet())
    {
        vk::DescriptorSetAllocateInfo allocInfo;
        allocInfo.descriptorPool = componentDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = ssboLayouts;
        componentDescriptorSet = device.allocateDescriptorSets(allocInfo)[0];
    }

    // Entity SSBO
    vk::DescriptorBufferInfo entityBufferInfo;
    entityBufferInfo.buffer = Entity::GetSSBO();
    entityBufferInfo.offset = 0;
    entityBufferInfo.range = Entity::GetSSBOSize();

    ssboDescriptorWrites[0].dstSet = componentDescriptorSet;
    ssboDescriptorWrites[0].dstBinding = 0;
    ssboDescriptorWrites[0].dstArrayElement = 0;
    ssboDescriptorWrites[0].descriptorType = vk::DescriptorType::eStorageBuffer;
    ssboDescriptorWrites[0].descriptorCount = 1;
    ssboDescriptorWrites[0].pBufferInfo = &entityBufferInfo;

    // Transform SSBO
    vk::DescriptorBufferInfo transformBufferInfo;
    transformBufferInfo.buffer = Transform::GetSSBO();
    transformBufferInfo.offset = 0;
    transformBufferInfo.range = Transform::GetSSBOSize();

    ssboDescriptorWrites[1].dstSet = componentDescriptorSet;
    ssboDescriptorWrites[1].dstBinding = 1;
    ssboDescriptorWrites[1].dstArrayElement = 0;
    ssboDescriptorWrites[1].descriptorType = vk::DescriptorType::eStorageBuffer;
    ssboDescriptorWrites[1].descriptorCount = 1;
    ssboDescriptorWrites[1].pBufferInfo = &transformBufferInfo;

    // Camera SSBO
    vk::DescriptorBufferInfo cameraBufferInfo;
    cameraBufferInfo.buffer = Camera::GetSSBO();
    cameraBufferInfo.offset = 0;
    cameraBufferInfo.range = Camera::GetSSBOSize();

    ssboDescriptorWrites[2].dstSet = componentDescriptorSet;
    ssboDescriptorWrites[2].dstBinding = 2;
    ssboDescriptorWrites[2].dstArrayElement = 0;
    ssboDescriptorWrites[2].descriptorType = vk::DescriptorType::eStorageBuffer;
    ssboDescriptorWrites[2].descriptorCount = 1;
    ssboDescriptorWrites[2].pBufferInfo = &cameraBufferInfo;

    // Material SSBO
    vk::DescriptorBufferInfo materialBufferInfo;
    materialBufferInfo.buffer = Material::GetSSBO();
    materialBufferInfo.offset = 0;
    materialBufferInfo.range = Material::GetSSBOSize();

    ssboDescriptorWrites[3].dstSet = componentDescriptorSet;
    ssboDescriptorWrites[3].dstBinding = 3;
    ssboDescriptorWrites[3].dstArrayElement = 0;
    ssboDescriptorWrites[3].descriptorType = vk::DescriptorType::eStorageBuffer;
    ssboDescriptorWrites[3].descriptorCount = 1;
    ssboDescriptorWrites[3].pBufferInfo = &materialBufferInfo;

    // Light SSBO
    vk::DescriptorBufferInfo lightBufferInfo;
    lightBufferInfo.buffer = Light::GetSSBO();
    lightBufferInfo.offset = 0;
    lightBufferInfo.range = Light::GetSSBOSize();

    ssboDescriptorWrites[4].dstSet = componentDescriptorSet;
    ssboDescriptorWrites[4].dstBinding = 4;
    ssboDescriptorWrites[4].dstArrayElement = 0;
    ssboDescriptorWrites[4].descriptorType = vk::DescriptorType::eStorageBuffer;
    ssboDescriptorWrites[4].descriptorCount = 1;
    ssboDescriptorWrites[4].pBufferInfo = &lightBufferInfo;
    
    device.updateDescriptorSets((uint32_t)ssboDescriptorWrites.size(), ssboDescriptorWrites.data(), 0, nullptr);
    
    /* ------ Texture Descriptor Set  ------ */
    vk::DescriptorSetLayout textureLayouts[] = { textureDescriptorSetLayout };
    std::array<vk::WriteDescriptorSet, 2> textureDescriptorWrites = {};
    
    if (textureDescriptorSet == vk::DescriptorSet())
    {
        vk::DescriptorSetAllocateInfo allocInfo;
        allocInfo.descriptorPool = textureDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = textureLayouts;

        textureDescriptorSet = device.allocateDescriptorSets(allocInfo)[0];
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

    textureDescriptorWrites[0].dstSet = textureDescriptorSet;
    textureDescriptorWrites[0].dstBinding = 0;
    textureDescriptorWrites[0].dstArrayElement = 0;
    textureDescriptorWrites[0].descriptorType = vk::DescriptorType::eSampler;
    textureDescriptorWrites[0].descriptorCount = MAX_TEXTURES;
    textureDescriptorWrites[0].pImageInfo = samplerDescriptorInfos;

    // Textures
    vk::DescriptorImageInfo textureDescriptorInfos[MAX_TEXTURES];
    for (int i = 0; i < MAX_TEXTURES; ++i) 
    {
        textureDescriptorInfos[i].sampler = nullptr;
        textureDescriptorInfos[i].imageLayout = imageLayouts[i];
        textureDescriptorInfos[i].imageView = imageViews[i];
    }

    textureDescriptorWrites[1].dstSet = textureDescriptorSet;
    textureDescriptorWrites[1].dstBinding = 1;
    textureDescriptorWrites[1].dstArrayElement = 0;
    textureDescriptorWrites[1].descriptorType = vk::DescriptorType::eSampledImage;
    textureDescriptorWrites[1].descriptorCount = MAX_TEXTURES;
    textureDescriptorWrites[1].pImageInfo = textureDescriptorInfos;
    
    device.updateDescriptorSets((uint32_t)textureDescriptorWrites.size(), textureDescriptorWrites.data(), 0, nullptr);
}

void Material::CreateVertexInputBindingDescriptions() {
    /* Vertex input bindings are consistent across shaders */
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

    vertexInputBindingDescriptions = { pointBindingDescription, colorBindingDescription, normalBindingDescription, texcoordBindingDescription };
}

void Material::CreateVertexAttributeDescriptions() {
    /* Vertex attribute descriptions are consistent across shaders */
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

    vertexInputAttributeDescriptions = attributeDescriptions;
}

bool Material::BindDescriptorSets(vk::CommandBuffer &command_buffer) 
{
    std::vector<vk::DescriptorSet> descriptorSets = {componentDescriptorSet, textureDescriptorSet};
    command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, normalsurface.pipelineLayout, 0, 2, descriptorSets.data(), 0, nullptr);
    command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, blinn.pipelineLayout, 0, 2, descriptorSets.data(), 0, nullptr);
    command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, texcoordsurface.pipelineLayout, 0, 2, descriptorSets.data(), 0, nullptr);
    command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pbr.pipelineLayout, 0, 2, descriptorSets.data(), 0, nullptr);
    command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, blinn.pipelineLayout, 0, 2, descriptorSets.data(), 0, nullptr);
    return true;
}

bool Material::DrawEntity(vk::CommandBuffer &command_buffer, Entity &entity, int32_t &camera_id, std::vector<int32_t> &light_entity_ids)
{
    /* Need a mesh to render. */
    auto mesh_id = entity.get_mesh();
    if (mesh_id < 0 || mesh_id >= MAX_MESHES) return false;
    auto m = Mesh::Get((uint32_t) mesh_id);
    if (!m) return false;

    /* Need a transform to render. */
    auto transform_id = entity.get_transform();
    if (transform_id < 0 || transform_id >= MAX_TRANSFORMS) return false;

    /* Need a material to render. */
    auto material_id = entity.get_material();
    if (material_id < 0 || material_id >= MAX_MATERIALS) return false;
    auto material = Material::Get(material_id);
    if (!material) return false;


    // Push constants
    {
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
    }

    // Render normal
    if (material->renderMode == 1) {
        command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, Material::normalsurface.pipeline);
    }
    // Render Vertex Colors
    else if (material->renderMode == 2) {
        command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, Material::blinn.pipeline);
    }
    // Render UVs
    else if (material->renderMode == 3) {
        command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, Material::texcoordsurface.pipeline);
    }
    // Render PBR
    if (material->renderMode == 4) {
        command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, Material::pbr.pipeline);
    }
    // Render material
    else {
        command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, Material::blinn.pipeline);
    }

    command_buffer.bindVertexBuffers(0, {m->get_point_buffer(), m->get_color_buffer(), m->get_normal_buffer(), m->get_texcoord_buffer()}, {0,0,0,0});
    command_buffer.bindIndexBuffer(m->get_index_buffer(), 0, vk::IndexType::eUint32);
    command_buffer.drawIndexed(m->get_total_indices(), 1, 0, 0, 0);

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

    device.destroyDescriptorSetLayout(componentDescriptorSetLayout);
    device.destroyDescriptorPool(componentDescriptorPool);

    device.destroyDescriptorSetLayout(textureDescriptorSetLayout);
    device.destroyDescriptorPool(textureDescriptorPool);
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