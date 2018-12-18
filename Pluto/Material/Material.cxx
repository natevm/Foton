#include "./Material.hxx"
#include "Pluto/Tools/Options.hxx"
#include "Pluto/Tools/FileReader.hxx"

#include "Pluto/Entity/Entity.hxx"
#include "Pluto/Transform/Transform.hxx"
#include "Pluto/Camera/Camera.hxx"
#include "Pluto/Light/Light.hxx"

Material Material::materials[MAX_MATERIALS];
MaterialStruct* Material::pinnedMemory;
std::map<std::string, uint32_t> Material::lookupTable;
vk::Buffer Material::ssbo;
vk::DeviceMemory Material::ssboMemory;

/* UNIFORM COLOR PIPELINE */
Material::MaterialResources Material::uniformColor;
Material::MaterialResources Material::blinn;

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
    std::vector<vk::DescriptorSetLayout> descriptorSetLayouts, // yes
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
    pipelineLayoutInfo.setLayoutCount = (uint32_t)descriptorSetLayouts.size();
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
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
            uniformColor.vertexInputAttributeDescriptions, { uniformColor.descriptorSetLayout }, uniformColor.pipelineParameters, 
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
            blinn.vertexInputAttributeDescriptions, { blinn.descriptorSetLayout }, blinn.pipelineParameters, 
            renderpass, 0, 
            blinn.pipeline, blinn.pipelineLayout);

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

        uniformColor.descriptorSetLayout = device.createDescriptorSetLayout(layoutInfo);
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

        blinn.descriptorSetLayout = device.createDescriptorSetLayout(layoutInfo);
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

        uniformColor.descriptorPool = device.createDescriptorPool(poolInfo);
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

        blinn.descriptorPool = device.createDescriptorPool(poolInfo);
    }
}

void Material::CreateDescriptorSets()
{
    auto vulkan = Libraries::Vulkan::Get();
    auto device = vulkan->get_device();
    
    /* ------ UNIFORM COLOR DESCRIPTOR SET  ------ */
    {

        /* This is experimental. Typically descriptor sets aren't static. */
        vk::DescriptorSetLayout layouts[] = { uniformColor.descriptorSetLayout };
        std::array<vk::WriteDescriptorSet, 5> descriptorWrites = {};
        vk::DescriptorSetAllocateInfo allocInfo;
        allocInfo.descriptorPool = uniformColor.descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = layouts;

        uniformColor.descriptorSet = device.allocateDescriptorSets(allocInfo)[0];

        // Entity SSBO
        vk::DescriptorBufferInfo entityBufferInfo;
        entityBufferInfo.buffer = Entity::GetSSBO();
        entityBufferInfo.offset = 0;
        entityBufferInfo.range = Entity::GetSSBOSize();

        descriptorWrites[0].dstSet = uniformColor.descriptorSet;
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

        descriptorWrites[1].dstSet = uniformColor.descriptorSet;
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

        descriptorWrites[2].dstSet = uniformColor.descriptorSet;
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

        descriptorWrites[3].dstSet = uniformColor.descriptorSet;
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

        descriptorWrites[4].dstSet = uniformColor.descriptorSet;
        descriptorWrites[4].dstBinding = 4;
        descriptorWrites[4].dstArrayElement = 0;
        descriptorWrites[4].descriptorType = vk::DescriptorType::eStorageBuffer;
        descriptorWrites[4].descriptorCount = 1;
        descriptorWrites[4].pBufferInfo = &lightBufferInfo;
        
        device.updateDescriptorSets((uint32_t)descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
    }

    /* ------ BLINN DESCRIPTOR SET  ------ */
    {
        /* This is experimental. Typically descriptor sets aren't static. */
        vk::DescriptorSetLayout layouts[] = { blinn.descriptorSetLayout };
        std::array<vk::WriteDescriptorSet, 5> descriptorWrites = {};
        vk::DescriptorSetAllocateInfo allocInfo;
        allocInfo.descriptorPool = blinn.descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = layouts;

        blinn.descriptorSet = device.allocateDescriptorSets(allocInfo)[0];

        // Entity SSBO
        vk::DescriptorBufferInfo entityBufferInfo;
        entityBufferInfo.buffer = Entity::GetSSBO();
        entityBufferInfo.offset = 0;
        entityBufferInfo.range = Entity::GetSSBOSize();

        descriptorWrites[0].dstSet = blinn.descriptorSet;
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

        descriptorWrites[1].dstSet = blinn.descriptorSet;
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

        descriptorWrites[2].dstSet = blinn.descriptorSet;
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

        descriptorWrites[3].dstSet = blinn.descriptorSet;
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

        descriptorWrites[4].dstSet = blinn.descriptorSet;
        descriptorWrites[4].dstBinding = 4;
        descriptorWrites[4].dstArrayElement = 0;
        descriptorWrites[4].descriptorType = vk::DescriptorType::eStorageBuffer;
        descriptorWrites[4].descriptorCount = 1;
        descriptorWrites[4].pBufferInfo = &lightBufferInfo;
        
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
}

bool Material::DrawEntity(vk::CommandBuffer &command_buffer, Entity &entity, int32_t &camera_id, std::vector<int32_t> &light_entity_ids)
{
    auto mesh_id = entity.get_mesh();
    auto transform_id = entity.get_transform();
    
    /* Need a mesh and a transform to render. */
    if (mesh_id < 0 || mesh_id >= MAX_MESHES) return false;
    if (transform_id < 0 || transform_id >= MAX_TRANSFORMS) return false;

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

    command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, Material::blinn.pipeline);
    command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, blinn.pipelineLayout, 0, 1, &blinn.descriptorSet, 0, nullptr);
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

    device.destroyDescriptorSetLayout(uniformColor.descriptorSetLayout);
    device.destroyDescriptorPool(uniformColor.descriptorPool);
//     vk::DescriptorSetLayout Material::uniformColor.descriptorSetLayout;
// vk::DescriptorPool Material::uniformColor.descriptorPool;
// vk::DescriptorSet Material::uniformColor.descriptorSet;
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
