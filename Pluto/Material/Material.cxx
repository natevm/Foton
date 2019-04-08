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
vk::Buffer Material::SSBO;
vk::DeviceMemory Material::SSBOMemory;
vk::Buffer Material::stagingSSBO;
vk::DeviceMemory Material::stagingSSBOMemory;

RenderMode Material::currentlyBoundRenderMode = RenderMode::NONE;
vk::RenderPass Material::currentRenderpass = vk::RenderPass();;

vk::DescriptorSetLayout Material::componentDescriptorSetLayout;
vk::DescriptorSetLayout Material::textureDescriptorSetLayout;
vk::DescriptorSetLayout Material::raytracingDescriptorSetLayout;
vk::DescriptorPool Material::componentDescriptorPool;
vk::DescriptorPool Material::textureDescriptorPool;
vk::DescriptorPool Material::raytracingDescriptorPool;
std::vector<vk::VertexInputBindingDescription> Material::vertexInputBindingDescriptions;
std::vector<vk::VertexInputAttributeDescription> Material::vertexInputAttributeDescriptions;
vk::DescriptorSet Material::componentDescriptorSet;
vk::DescriptorSet Material::textureDescriptorSet;
std::mutex Material::creation_mutex;
bool Material::Initialized = false;

std::map<vk::RenderPass, Material::RasterPipelineResources> Material::uniformColor;
std::map<vk::RenderPass, Material::RasterPipelineResources> Material::blinn;
std::map<vk::RenderPass, Material::RasterPipelineResources> Material::pbr;
std::map<vk::RenderPass, Material::RasterPipelineResources> Material::texcoordsurface;
std::map<vk::RenderPass, Material::RasterPipelineResources> Material::normalsurface;
std::map<vk::RenderPass, Material::RasterPipelineResources> Material::skybox;
std::map<vk::RenderPass, Material::RasterPipelineResources> Material::depth;
std::map<vk::RenderPass, Material::RasterPipelineResources> Material::volume;

std::map<vk::RenderPass, Material::RasterPipelineResources> Material::shadowmap;
std::map<vk::RenderPass, Material::RasterPipelineResources> Material::fragmentdepth;
std::map<vk::RenderPass, Material::RasterPipelineResources> Material::fragmentposition;

std::map<vk::RenderPass, Material::RaytracingPipelineResources> Material::rttest;

Material::Material() {
    this->initialized = false;
}

Material::Material(std::string name, uint32_t id)
{
    this->initialized = true;
    this->name = name;
    this->id = id;

    /* Working off blender's principled BSDF */
    material_struct.base_color = vec4(.8, .8, .8, 1.0);
    material_struct.subsurface_radius = vec4(1.0, .2, .1, 1.0);
    material_struct.subsurface_color = vec4(.8, .8, .8, 1.0);
    material_struct.subsurface = 0.0;
    material_struct.metallic = 0.0;
    material_struct.specular = .5;
    material_struct.specular_tint = 0.0;
    material_struct.roughness = .5;
    material_struct.anisotropic = 0.0;
    material_struct.anisotropic_rotation = 0.0;
    material_struct.sheen = 0.0;
    material_struct.sheen_tint = 0.5;
    material_struct.clearcoat = 0.0;
    material_struct.clearcoat_roughness = .03f;
    material_struct.ior = 1.45f;
    material_struct.transmission = 0.0;
    material_struct.transmission_roughness = 0.0;
    material_struct.volume_texture_id = -1;
    material_struct.flags = 0;
    material_struct.base_color_texture_id = -1;
    material_struct.roughness_texture_id = -1;
    material_struct.occlusion_texture_id = -1;
    material_struct.transfer_function_texture_id = -1;
}

std::string Material::to_string() {
    std::string output;
    output += "{\n";
    output += "\ttype: \"Material\",\n";
    output += "\tname: \"" + name + "\"\n";
    output += "\tbase_color: \"" + glm::to_string(material_struct.base_color) + "\"\n";
    output += "\tsubsurface: \"" + std::to_string(material_struct.subsurface) + "\"\n";
    output += "\tsubsurface_radius: \"" + glm::to_string(material_struct.subsurface_radius) + "\"\n";
    output += "\tsubsurface_color: \"" + glm::to_string(material_struct.subsurface_color) + "\"\n";
    output += "\tmetallic: \"" + std::to_string(material_struct.metallic) + "\"\n";
    output += "\tspecular: \"" + std::to_string(material_struct.specular) + "\"\n";
    output += "\tspecular_tint: \"" + std::to_string(material_struct.specular_tint) + "\"\n";
    output += "\troughness: \"" + std::to_string(material_struct.roughness) + "\"\n";
    output += "\tanisotropic: \"" + std::to_string(material_struct.anisotropic) + "\"\n";
    output += "\tanisotropic_rotation: \"" + std::to_string(material_struct.anisotropic_rotation) + "\"\n";
    output += "\tsheen: \"" + std::to_string(material_struct.sheen) + "\"\n";
    output += "\tsheen_tint: \"" + std::to_string(material_struct.sheen_tint) + "\"\n";
    output += "\tclearcoat: \"" + std::to_string(material_struct.clearcoat) + "\"\n";
    output += "\tclearcoat_roughness: \"" + std::to_string(material_struct.clearcoat_roughness) + "\"\n";
    output += "\tior: \"" + std::to_string(material_struct.ior) + "\"\n";
    output += "\ttransmission: \"" + std::to_string(material_struct.transmission) + "\"\n";
    output += "\ttransmission_roughness: \"" + std::to_string(material_struct.transmission_roughness) + "\"\n";
    output += "}";
    return output;
}

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
void Material::CreateRasterPipeline(
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
    range.size = sizeof(PushConsts);
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

/* Compiles all shaders */
void Material::SetupGraphicsPipelines(vk::RenderPass renderpass, uint32_t sampleCount, bool use_depth_prepass)
{
    auto vulkan = Libraries::Vulkan::Get();
    auto device = vulkan->get_device();

    auto sampleFlag = vulkan->highest(vulkan->min(vulkan->get_closest_sample_count_flag(sampleCount), vulkan->get_msaa_sample_flags()));

    #ifndef DISABLE_REVERSE_Z
    auto depthCompareOp = vk::CompareOp::eGreater;
    auto depthCompareOpEqual = vk::CompareOp::eGreaterOrEqual;
    #else
    auto depthCompareOp = vk::CompareOp::eLess;
    auto depthCompareOpEqual = vk::CompareOp::eLessOrEqual;
    #endif

    /* RASTER GRAPHICS PIPELINES */

    /* ------ UNIFORM COLOR  ------ */
    {
        uniformColor[renderpass] = RasterPipelineResources();

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
        vertShaderStageInfo.pName = "main";

        vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
        fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertShaderStageInfo, fragShaderStageInfo };
        
        /* Account for possibly multiple samples */
        uniformColor[renderpass].pipelineParameters.multisampling.sampleShadingEnable = (sampleFlag == vk::SampleCountFlagBits::e1) ? false : true;
        uniformColor[renderpass].pipelineParameters.multisampling.rasterizationSamples = sampleFlag;

        CreateRasterPipeline(shaderStages, vertexInputBindingDescriptions, vertexInputAttributeDescriptions, 
            { componentDescriptorSetLayout, textureDescriptorSetLayout }, 
            uniformColor[renderpass].pipelineParameters, 
            renderpass, 0, 
            uniformColor[renderpass].pipeline, uniformColor[renderpass].pipelineLayout);

        device.destroyShaderModule(fragShaderModule);
        device.destroyShaderModule(vertShaderModule);
    }


    /* ------ BLINN GRAPHICS ------ */
    {
        blinn[renderpass] = RasterPipelineResources();

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
        vertShaderStageInfo.pName = "main";

        vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
        fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertShaderStageInfo, fragShaderStageInfo };
        
        /* Account for possibly multiple samples */
        blinn[renderpass].pipelineParameters.multisampling.sampleShadingEnable = (sampleFlag == vk::SampleCountFlagBits::e1) ? false : true;
        blinn[renderpass].pipelineParameters.multisampling.rasterizationSamples = sampleFlag;

        CreateRasterPipeline(shaderStages, vertexInputBindingDescriptions, vertexInputAttributeDescriptions, 
            { componentDescriptorSetLayout, textureDescriptorSetLayout }, 
            blinn[renderpass].pipelineParameters, 
            renderpass, 0, 
            blinn[renderpass].pipeline, blinn[renderpass].pipelineLayout);

        device.destroyShaderModule(fragShaderModule);
        device.destroyShaderModule(vertShaderModule);
    }

    /* ------ PBR  ------ */
    {
        pbr[renderpass] = RasterPipelineResources();

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
        vertShaderStageInfo.pName = "main";

        vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
        fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertShaderStageInfo, fragShaderStageInfo };
        
        /* Account for possibly multiple samples */
        pbr[renderpass].pipelineParameters.multisampling.sampleShadingEnable = (sampleFlag == vk::SampleCountFlagBits::e1) ? false : true;
        pbr[renderpass].pipelineParameters.multisampling.rasterizationSamples = sampleFlag;
        
        /* Because we use a depth prepass */
        if (use_depth_prepass) {
            pbr[renderpass].pipelineParameters.depthStencil.depthTestEnable = true;
            pbr[renderpass].pipelineParameters.depthStencil.depthWriteEnable = false; // not VK_TRUE since we have a depth prepass
            pbr[renderpass].pipelineParameters.depthStencil.depthCompareOp = depthCompareOpEqual;
        }
		// depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL; //not VK_COMPARE_OP_LESS since we have a depth prepass;
        
        CreateRasterPipeline(shaderStages, vertexInputBindingDescriptions, vertexInputAttributeDescriptions, 
            { componentDescriptorSetLayout, textureDescriptorSetLayout }, 
            pbr[renderpass].pipelineParameters, 
            renderpass, 0, 
            pbr[renderpass].pipeline, pbr[renderpass].pipelineLayout);

        device.destroyShaderModule(fragShaderModule);
        device.destroyShaderModule(vertShaderModule);
    }

    /* ------ NORMAL SURFACE ------ */
    {
        normalsurface[renderpass] = RasterPipelineResources();

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
        vertShaderStageInfo.pName = "main";

        vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
        fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertShaderStageInfo, fragShaderStageInfo };
        
        /* Account for possibly multiple samples */
        normalsurface[renderpass].pipelineParameters.multisampling.sampleShadingEnable = (sampleFlag == vk::SampleCountFlagBits::e1) ? false : true;
        normalsurface[renderpass].pipelineParameters.multisampling.rasterizationSamples = sampleFlag;

        CreateRasterPipeline(shaderStages, vertexInputBindingDescriptions, vertexInputAttributeDescriptions, 
            { componentDescriptorSetLayout, textureDescriptorSetLayout }, 
            normalsurface[renderpass].pipelineParameters, 
            renderpass, 0, 
            normalsurface[renderpass].pipeline, normalsurface[renderpass].pipelineLayout);

        device.destroyShaderModule(fragShaderModule);
        device.destroyShaderModule(vertShaderModule);
    }

    /* ------ TEXCOORD SURFACE  ------ */
    {
        texcoordsurface[renderpass] = RasterPipelineResources();

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
        vertShaderStageInfo.pName = "main";

        vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
        fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertShaderStageInfo, fragShaderStageInfo };
        
        /* Account for possibly multiple samples */
        texcoordsurface[renderpass].pipelineParameters.multisampling.sampleShadingEnable = (sampleFlag == vk::SampleCountFlagBits::e1) ? false : true;
        texcoordsurface[renderpass].pipelineParameters.multisampling.rasterizationSamples = sampleFlag;

        CreateRasterPipeline(shaderStages, vertexInputBindingDescriptions, vertexInputAttributeDescriptions, 
            { componentDescriptorSetLayout, textureDescriptorSetLayout }, 
            texcoordsurface[renderpass].pipelineParameters, 
            renderpass, 0, 
            texcoordsurface[renderpass].pipeline, texcoordsurface[renderpass].pipelineLayout);

        device.destroyShaderModule(fragShaderModule);
        device.destroyShaderModule(vertShaderModule);
    }

    /* ------ SKYBOX  ------ */
    {
        skybox[renderpass] = RasterPipelineResources();

        std::string ResourcePath = Options::GetResourcePath();
        auto vertShaderCode = readFile(ResourcePath + std::string("/Shaders/SurfaceMaterials/Skybox/vert.spv"));
        auto fragShaderCode = readFile(ResourcePath + std::string("/Shaders/SurfaceMaterials/Skybox/frag.spv"));

        /* Create shader modules */
        auto vertShaderModule = CreateShaderModule(vertShaderCode);
        auto fragShaderModule = CreateShaderModule(fragShaderCode);

        /* Info for shader stages */
        vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
        vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";

        vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
        fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertShaderStageInfo, fragShaderStageInfo };
        
        /* Skyboxes don't do back face culling. */
        skybox[renderpass].pipelineParameters.rasterizer.setCullMode(vk::CullModeFlagBits::eNone);

        /* Account for possibly multiple samples */
        skybox[renderpass].pipelineParameters.multisampling.sampleShadingEnable = (sampleFlag == vk::SampleCountFlagBits::e1) ? false : true;
        skybox[renderpass].pipelineParameters.multisampling.rasterizationSamples = sampleFlag;

        CreateRasterPipeline(shaderStages, vertexInputBindingDescriptions, vertexInputAttributeDescriptions, 
            { componentDescriptorSetLayout, textureDescriptorSetLayout }, 
            skybox[renderpass].pipelineParameters, 
            renderpass, 0, 
            skybox[renderpass].pipeline, skybox[renderpass].pipelineLayout);

        device.destroyShaderModule(fragShaderModule);
        device.destroyShaderModule(vertShaderModule);
    }

    /* ------ DEPTH  ------ */
    {
        depth[renderpass] = RasterPipelineResources();

        std::string ResourcePath = Options::GetResourcePath();
        auto vertShaderCode = readFile(ResourcePath + std::string("/Shaders/SurfaceMaterials/Depth/vert.spv"));
        auto fragShaderCode = readFile(ResourcePath + std::string("/Shaders/SurfaceMaterials/Depth/frag.spv"));

        /* Create shader modules */
        auto vertShaderModule = CreateShaderModule(vertShaderCode);
        auto fragShaderModule = CreateShaderModule(fragShaderCode);

        /* Info for shader stages */
        vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
        vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";

        vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
        fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertShaderStageInfo, fragShaderStageInfo };
        
        /* Account for possibly multiple samples */
        depth[renderpass].pipelineParameters.multisampling.sampleShadingEnable = (sampleFlag == vk::SampleCountFlagBits::e1) ? false : true;
        depth[renderpass].pipelineParameters.multisampling.rasterizationSamples = sampleFlag;
        // depth[renderpass].pipelineParameters.rasterizer.cullMode = vk::CullModeFlagBits::eNone;

        /* Because we use a depth prepass */
        if (use_depth_prepass) {
            depth[renderpass].pipelineParameters.depthStencil.depthTestEnable = true;
            depth[renderpass].pipelineParameters.depthStencil.depthWriteEnable = false; // not VK_TRUE since we have a depth prepass
            depth[renderpass].pipelineParameters.depthStencil.depthCompareOp = depthCompareOpEqual;
        }

        CreateRasterPipeline(shaderStages, vertexInputBindingDescriptions, vertexInputAttributeDescriptions, 
            { componentDescriptorSetLayout, textureDescriptorSetLayout }, 
            depth[renderpass].pipelineParameters, 
            renderpass, 0, 
            depth[renderpass].pipeline, depth[renderpass].pipelineLayout);

        device.destroyShaderModule(fragShaderModule);
        device.destroyShaderModule(vertShaderModule);
    }

    /* ------ Volume  ------ */
    {
        volume[renderpass] = RasterPipelineResources();

        std::string ResourcePath = Options::GetResourcePath();
        auto vertShaderCode = readFile(ResourcePath + std::string("/Shaders/VolumeMaterials/Volume/vert.spv"));
        auto fragShaderCode = readFile(ResourcePath + std::string("/Shaders/VolumeMaterials/Volume/frag.spv"));

        /* Create shader modules */
        auto vertShaderModule = CreateShaderModule(vertShaderCode);
        auto fragShaderModule = CreateShaderModule(fragShaderCode);

        /* Info for shader stages */
        vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
        vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";

        vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
        fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertShaderStageInfo, fragShaderStageInfo };
        
        /* Account for possibly multiple samples */
        volume[renderpass].pipelineParameters.multisampling.sampleShadingEnable = (sampleFlag == vk::SampleCountFlagBits::e1) ? false : true;
        volume[renderpass].pipelineParameters.multisampling.rasterizationSamples = sampleFlag;

        CreateRasterPipeline(shaderStages, vertexInputBindingDescriptions, vertexInputAttributeDescriptions, 
            { componentDescriptorSetLayout, textureDescriptorSetLayout }, 
            volume[renderpass].pipelineParameters, 
            renderpass, 0, 
            volume[renderpass].pipeline, volume[renderpass].pipelineLayout);

        device.destroyShaderModule(fragShaderModule);
        device.destroyShaderModule(vertShaderModule);
    }

    /*  G BUFFERS  */

    /* ------ Fragment Depth  ------ */
    {
        fragmentdepth[renderpass] = RasterPipelineResources();

        std::string ResourcePath = Options::GetResourcePath();
        auto vertShaderCode = readFile(ResourcePath + std::string("/Shaders/GBuffers/FragmentDepth/vert.spv"));

        /* Create shader modules */
        auto vertShaderModule = CreateShaderModule(vertShaderCode);

        /* Info for shader stages */
        vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
        vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";

        std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertShaderStageInfo };
        
        /* Account for possibly multiple samples */
        fragmentdepth[renderpass].pipelineParameters.multisampling.sampleShadingEnable = (sampleFlag == vk::SampleCountFlagBits::e1) ? false : true;
        fragmentdepth[renderpass].pipelineParameters.multisampling.rasterizationSamples = sampleFlag;

        fragmentdepth[renderpass].pipelineParameters.depthStencil.depthWriteEnable = true;
        fragmentdepth[renderpass].pipelineParameters.depthStencil.depthCompareOp = depthCompareOpEqual;
        fragmentdepth[renderpass].pipelineParameters.rasterizer.cullMode = vk::CullModeFlagBits::eNone;

        CreateRasterPipeline(shaderStages, vertexInputBindingDescriptions, vertexInputAttributeDescriptions, 
            { componentDescriptorSetLayout, textureDescriptorSetLayout }, 
            fragmentdepth[renderpass].pipelineParameters, 
            renderpass, 0, 
            fragmentdepth[renderpass].pipeline, fragmentdepth[renderpass].pipelineLayout);

        device.destroyShaderModule(vertShaderModule);
    }

    /* ------ SHADOWMAP  ------ */

    {
        shadowmap[renderpass] = RasterPipelineResources();

        std::string ResourcePath = Options::GetResourcePath();
        auto vertShaderCode = readFile(ResourcePath + std::string("/Shaders/GBuffers/ShadowMap/vert.spv"));
        auto fragShaderCode = readFile(ResourcePath + std::string("/Shaders/GBuffers/ShadowMap/frag.spv"));

        /* Create shader modules */
        auto vertShaderModule = CreateShaderModule(vertShaderCode);
        auto fragShaderModule = CreateShaderModule(fragShaderCode);

        /* Info for shader stages */
        vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
        vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";

        vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
        fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertShaderStageInfo, fragShaderStageInfo };
        
        /* Account for possibly multiple samples */
        shadowmap[renderpass].pipelineParameters.multisampling.sampleShadingEnable = (sampleFlag == vk::SampleCountFlagBits::e1) ? false : true;
        shadowmap[renderpass].pipelineParameters.multisampling.rasterizationSamples = sampleFlag;
        shadowmap[renderpass].pipelineParameters.rasterizer.cullMode = vk::CullModeFlagBits::eNone;

        /* Because we use a depth prepass */
        if (use_depth_prepass) {
            shadowmap[renderpass].pipelineParameters.depthStencil.depthTestEnable = true;
            shadowmap[renderpass].pipelineParameters.depthStencil.depthWriteEnable = false; // not VK_TRUE since we have a depth prepass
            shadowmap[renderpass].pipelineParameters.depthStencil.depthCompareOp = depthCompareOpEqual;
        }

        CreateRasterPipeline(shaderStages, vertexInputBindingDescriptions, vertexInputAttributeDescriptions, 
            { componentDescriptorSetLayout, textureDescriptorSetLayout }, 
            shadowmap[renderpass].pipelineParameters, 
            renderpass, 0, 
            shadowmap[renderpass].pipeline, shadowmap[renderpass].pipelineLayout);

        device.destroyShaderModule(fragShaderModule);
        device.destroyShaderModule(vertShaderModule);
    }

    /* ------ FRAGMENT POSITION  ------ */

    {
        fragmentposition[renderpass] = RasterPipelineResources();

        std::string ResourcePath = Options::GetResourcePath();
        auto vertShaderCode = readFile(ResourcePath + std::string("/Shaders/GBuffers/FragmentPosition/vert.spv"));
        auto fragShaderCode = readFile(ResourcePath + std::string("/Shaders/GBuffers/FragmentPosition/frag.spv"));

        /* Create shader modules */
        auto vertShaderModule = CreateShaderModule(vertShaderCode);
        auto fragShaderModule = CreateShaderModule(fragShaderCode);

        /* Info for shader stages */
        vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
        vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";

        vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
        fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertShaderStageInfo, fragShaderStageInfo };
        
        /* Account for possibly multiple samples */
        fragmentposition[renderpass].pipelineParameters.multisampling.sampleShadingEnable = (sampleFlag == vk::SampleCountFlagBits::e1) ? false : true;
        fragmentposition[renderpass].pipelineParameters.multisampling.rasterizationSamples = sampleFlag;
        fragmentposition[renderpass].pipelineParameters.rasterizer.cullMode = vk::CullModeFlagBits::eNone;

        /* Because we use a depth prepass */
        if (use_depth_prepass) {
            fragmentposition[renderpass].pipelineParameters.depthStencil.depthTestEnable = true;
            fragmentposition[renderpass].pipelineParameters.depthStencil.depthWriteEnable = false; // not VK_TRUE since we have a depth prepass
            fragmentposition[renderpass].pipelineParameters.depthStencil.depthCompareOp = depthCompareOpEqual;
        }

        CreateRasterPipeline(shaderStages, vertexInputBindingDescriptions, vertexInputAttributeDescriptions, 
            { componentDescriptorSetLayout, textureDescriptorSetLayout }, 
            fragmentposition[renderpass].pipelineParameters, 
            renderpass, 0, 
            fragmentposition[renderpass].pipeline, fragmentposition[renderpass].pipelineLayout);

        device.destroyShaderModule(fragShaderModule);
        device.destroyShaderModule(vertShaderModule);
    }

    if (!vulkan->is_ray_tracing_enabled()) return;
    auto dldi = vulkan->get_dldi();

    /* RAY TRACING PIPELINES */
    {
        rttest[renderpass] = RaytracingPipelineResources();

        std::string ResourcePath = Options::GetResourcePath();
        auto raygenShaderCode = readFile(ResourcePath + std::string("/Shaders/RaytracedMaterials/TutorialShaders/rgen.spv"));
        
        /* Create shader modules */
        auto raygenShaderModule = CreateShaderModule(raygenShaderCode);

        /* Info for shader stages */
        vk::PipelineShaderStageCreateInfo raygenShaderStageInfo;
        raygenShaderStageInfo.stage = vk::ShaderStageFlagBits::eRaygenNV;
        raygenShaderStageInfo.module = raygenShaderModule;
        raygenShaderStageInfo.pName = "main"; 

        std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { raygenShaderStageInfo };
        
        vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
        pipelineLayoutCreateInfo.setLayoutCount = 1;
        pipelineLayoutCreateInfo.pSetLayouts = &raytracingDescriptorSetLayout;
        pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
        pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

        rttest[renderpass].pipelineLayout = device.createPipelineLayout(pipelineLayoutCreateInfo);

        std::vector<vk::RayTracingShaderGroupCreateInfoNV> shaderGroups;
        vk::RayTracingShaderGroupCreateInfoNV rayGenGroupInfo;
        rayGenGroupInfo.type = vk::RayTracingShaderGroupTypeNV::eGeneral;
        rayGenGroupInfo.generalShader = 0;
        rayGenGroupInfo.closestHitShader = VK_SHADER_UNUSED_NV;
        rayGenGroupInfo.anyHitShader = VK_SHADER_UNUSED_NV;
        rayGenGroupInfo.intersectionShader = VK_SHADER_UNUSED_NV;
        shaderGroups.push_back(rayGenGroupInfo);

        vk::RayTracingPipelineCreateInfoNV rayPipelineInfo;
        rayPipelineInfo.stageCount = (uint32_t) shaderStages.size();
        rayPipelineInfo.pStages = shaderStages.data();
        rayPipelineInfo.groupCount = (uint32_t) shaderGroups.size();
        rayPipelineInfo.pGroups = shaderGroups.data();
        rayPipelineInfo.maxRecursionDepth = 1;
        rayPipelineInfo.layout = rttest[renderpass].pipelineLayout;
        rayPipelineInfo.basePipelineHandle = vk::Pipeline();
        rayPipelineInfo.basePipelineIndex = 0;

        rttest[renderpass].pipeline = device.createRayTracingPipelinesNV(vk::PipelineCache(), 
            {rayPipelineInfo}, nullptr, dldi)[0];

        device.destroyShaderModule(raygenShaderModule);
    }

    SetupRaytracingShaderBindingTable(renderpass);
}

void Material::SetupRaytracingShaderBindingTable(vk::RenderPass renderpass)
{
    auto vulkan = Libraries::Vulkan::Get();
    if (!vulkan->is_initialized())
        throw std::runtime_error("Error: Vulkan not initialized");
    
    if (!vulkan->is_ray_tracing_enabled())
        throw std::runtime_error("Error: Vulkan raytracing is not enabled. ");

    auto device = vulkan->get_device();
    auto dldi = vulkan->get_dldi();

    auto rayTracingProps = vulkan->get_physical_device_ray_tracing_properties();


    /* Currently only works with rttest */

    
    const uint32_t groupNum = 1; // 1 group is listed in pGroupNumbers in VkRayTracingPipelineCreateInfoNV
    const uint32_t shaderBindingTableSize = rayTracingProps.shaderGroupHandleSize * groupNum;

    /* Create binding table buffer */
    vk::BufferCreateInfo bufferInfo;
    bufferInfo.size = shaderBindingTableSize;
    bufferInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;
    rttest[renderpass].shaderBindingTable = device.createBuffer(bufferInfo);

    /* Create memory for binding table */
    vk::MemoryRequirements memRequirements = device.getBufferMemoryRequirements(rttest[renderpass].shaderBindingTable);
    vk::MemoryAllocateInfo allocInfo;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = vulkan->find_memory_type(memRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible);
    rttest[renderpass].shaderBindingTableMemory = device.allocateMemory(allocInfo);

    /* Bind buffer to memeory */
    device.bindBufferMemory(rttest[renderpass].shaderBindingTable, rttest[renderpass].shaderBindingTableMemory, 0);

    /* Map the binding table, then fill with shader group handles */
    void* mappedMemory = device.mapMemory(rttest[renderpass].shaderBindingTableMemory, 0, shaderBindingTableSize, vk::MemoryMapFlags());
    device.getRayTracingShaderGroupHandlesNV(rttest[renderpass].pipeline, 0, groupNum, shaderBindingTableSize, mappedMemory, dldi);
    device.unmapMemory(rttest[renderpass].shaderBindingTableMemory);
}

void Material::Initialize()
{
    if (IsInitialized()) return;

    Material::CreateRasterDescriptorSetLayouts();
    Material::CreateRaytracingDescriptorSetLayouts();
    Material::CreateDescriptorPools();
    Material::CreateVertexInputBindingDescriptions();
    Material::CreateVertexAttributeDescriptions();
    Material::CreateSSBO();
    Material::UpdateRasterDescriptorSets();
    Material::UpdateRaytracingDescriptorSets();

    Initialized = true;
}

bool Material::IsInitialized()
{
    return Initialized;
}

void Material::CreateRasterDescriptorSetLayouts()
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

    std::array<vk::DescriptorSetLayoutBinding, 5> SSBObindings = { eboLayoutBinding, tboLayoutBinding, cboLayoutBinding, mboLayoutBinding, lboLayoutBinding};
    vk::DescriptorSetLayoutCreateInfo SSBOLayoutInfo;
    SSBOLayoutInfo.bindingCount = (uint32_t)SSBObindings.size();
    SSBOLayoutInfo.pBindings = SSBObindings.data();
    
    /* Texture descriptor bindings */
    
    // Texture struct
    vk::DescriptorSetLayoutBinding txboLayoutBinding;
    txboLayoutBinding.descriptorCount = 1;
    txboLayoutBinding.binding = 0;
    txboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
    txboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
    txboLayoutBinding.pImmutableSamplers = 0;

    // Texture samplers
    vk::DescriptorSetLayoutBinding samplerBinding;
    samplerBinding.descriptorCount = MAX_SAMPLERS;
    samplerBinding.binding = 1;
    samplerBinding.descriptorType = vk::DescriptorType::eSampler;
    samplerBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
    samplerBinding.pImmutableSamplers = 0;

    // 2D Textures
    vk::DescriptorSetLayoutBinding texture2DsBinding;
    texture2DsBinding.descriptorCount = MAX_TEXTURES;
    texture2DsBinding.binding = 2;
    texture2DsBinding.descriptorType = vk::DescriptorType::eSampledImage;
    texture2DsBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
    texture2DsBinding.pImmutableSamplers = 0;

    // Texture Cubes
    vk::DescriptorSetLayoutBinding textureCubesBinding;
    textureCubesBinding.descriptorCount = MAX_TEXTURES;
    textureCubesBinding.binding = 3;
    textureCubesBinding.descriptorType = vk::DescriptorType::eSampledImage;
    textureCubesBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
    textureCubesBinding.pImmutableSamplers = 0;

    // 3D Textures
    vk::DescriptorSetLayoutBinding texture3DsBinding;
    texture3DsBinding.descriptorCount = MAX_TEXTURES;
    texture3DsBinding.binding = 4;
    texture3DsBinding.descriptorType = vk::DescriptorType::eSampledImage;
    texture3DsBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
    texture3DsBinding.pImmutableSamplers = 0;

    std::array<vk::DescriptorSetLayoutBinding, 5> bindings = {txboLayoutBinding, samplerBinding, texture2DsBinding, textureCubesBinding, texture3DsBinding };
    vk::DescriptorSetLayoutCreateInfo textureLayoutInfo;
    textureLayoutInfo.bindingCount = (uint32_t)bindings.size();
    textureLayoutInfo.pBindings = bindings.data();

    // Create the layouts
    componentDescriptorSetLayout = device.createDescriptorSetLayout(SSBOLayoutInfo);
    textureDescriptorSetLayout = device.createDescriptorSetLayout(textureLayoutInfo);
}

void Material::CreateRaytracingDescriptorSetLayouts()
{
    auto vulkan = Libraries::Vulkan::Get();
    auto device = vulkan->get_device();

    if (!vulkan->is_ray_tracing_enabled()) return;

    vk::DescriptorSetLayoutBinding accelerationStructureLayoutBinding;
    accelerationStructureLayoutBinding.binding = 0;
    accelerationStructureLayoutBinding.descriptorType = vk::DescriptorType::eAccelerationStructureNV;
    accelerationStructureLayoutBinding.descriptorCount = 1;
    accelerationStructureLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eRaygenNV;
    accelerationStructureLayoutBinding.pImmutableSamplers = nullptr;

    vk::DescriptorSetLayoutBinding outputImageLayoutBinding;
    outputImageLayoutBinding.binding = 1;
    outputImageLayoutBinding.descriptorType = vk::DescriptorType::eStorageImage;
    outputImageLayoutBinding.descriptorCount = 1;
    outputImageLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eRaygenNV;
    outputImageLayoutBinding.pImmutableSamplers = nullptr;

    std::vector<vk::DescriptorSetLayoutBinding> bindings({ accelerationStructureLayoutBinding, outputImageLayoutBinding });

    vk::DescriptorSetLayoutCreateInfo layoutInfo;
    layoutInfo.bindingCount = (uint32_t)(bindings.size());
    layoutInfo.pBindings = bindings.data();

    raytracingDescriptorSetLayout = device.createDescriptorSetLayout(layoutInfo);
}

void Material::CreateDescriptorPools()
{
    /* Since the descriptor layout is consistent across shaders, the descriptor
        pool can be shared. */

    auto vulkan = Libraries::Vulkan::Get();
    auto device = vulkan->get_device();

    /* SSBO Descriptor Pool Info */
    std::array<vk::DescriptorPoolSize, 5> SSBOPoolSizes = {};
    
    // Entity SSBO
    SSBOPoolSizes[0].type = vk::DescriptorType::eStorageBuffer;
    SSBOPoolSizes[0].descriptorCount = MAX_MATERIALS;
    
    // Transform SSBO
    SSBOPoolSizes[1].type = vk::DescriptorType::eStorageBuffer;
    SSBOPoolSizes[1].descriptorCount = MAX_MATERIALS;
    
    // Camera SSBO
    SSBOPoolSizes[2].type = vk::DescriptorType::eStorageBuffer;
    SSBOPoolSizes[2].descriptorCount = MAX_MATERIALS;
    
    // Material SSBO
    SSBOPoolSizes[3].type = vk::DescriptorType::eStorageBuffer;
    SSBOPoolSizes[3].descriptorCount = MAX_MATERIALS;
    
    // Light SSBO
    SSBOPoolSizes[4].type = vk::DescriptorType::eStorageBuffer;
    SSBOPoolSizes[4].descriptorCount = MAX_MATERIALS;

    vk::DescriptorPoolCreateInfo SSBOPoolInfo;
    SSBOPoolInfo.poolSizeCount = (uint32_t)SSBOPoolSizes.size();
    SSBOPoolInfo.pPoolSizes = SSBOPoolSizes.data();
    SSBOPoolInfo.maxSets = MAX_MATERIALS;
    SSBOPoolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

    /* Texture Descriptor Pool Info */
    std::array<vk::DescriptorPoolSize, 5> texturePoolSizes = {};
    
    // TextureSSBO
    texturePoolSizes[0].type = vk::DescriptorType::eStorageBuffer;
    texturePoolSizes[0].descriptorCount = MAX_MATERIALS;

    // Sampler
    texturePoolSizes[1].type = vk::DescriptorType::eSampler;
    texturePoolSizes[1].descriptorCount = MAX_MATERIALS;
    
    // 2D Texture array
    texturePoolSizes[2].type = vk::DescriptorType::eSampledImage;
    texturePoolSizes[2].descriptorCount = MAX_MATERIALS;

    // Texture Cube array
    texturePoolSizes[3].type = vk::DescriptorType::eSampledImage;
    texturePoolSizes[3].descriptorCount = MAX_MATERIALS;

    // 3D Texture array
    texturePoolSizes[4].type = vk::DescriptorType::eSampledImage;
    texturePoolSizes[4].descriptorCount = MAX_MATERIALS;
    
    vk::DescriptorPoolCreateInfo texturePoolInfo;
    texturePoolInfo.poolSizeCount = (uint32_t)texturePoolSizes.size();
    texturePoolInfo.pPoolSizes = texturePoolSizes.data();
    texturePoolInfo.maxSets = MAX_MATERIALS;
    texturePoolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

    /* Raytrace Descriptor Pool Info */
    std::array<vk::DescriptorPoolSize, 2> raytracingPoolSizes = {};
    
    // Sampler
    raytracingPoolSizes[0].type = vk::DescriptorType::eStorageImage;
    raytracingPoolSizes[0].descriptorCount = 1;
    
    // 2D Texture array
    raytracingPoolSizes[1].type = vk::DescriptorType::eAccelerationStructureNV;
    raytracingPoolSizes[1].descriptorCount = 1;
    
    vk::DescriptorPoolCreateInfo raytracingPoolInfo;
    raytracingPoolInfo.poolSizeCount = (uint32_t)raytracingPoolSizes.size();
    raytracingPoolInfo.pPoolSizes = raytracingPoolSizes.data();
    raytracingPoolInfo.maxSets = 1;
    raytracingPoolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

    // Create the pools
    componentDescriptorPool = device.createDescriptorPool(SSBOPoolInfo);
    textureDescriptorPool = device.createDescriptorPool(texturePoolInfo);

    if (vulkan->is_ray_tracing_enabled())
        raytracingDescriptorPool = device.createDescriptorPool(raytracingPoolInfo);
}

void Material::UpdateRasterDescriptorSets()
{
    if (  (componentDescriptorPool == vk::DescriptorPool()) || (textureDescriptorPool == vk::DescriptorPool())) return;
    auto vulkan = Libraries::Vulkan::Get();
    auto device = vulkan->get_device();
    
    /* ------ Component Descriptor Set  ------ */
    vk::DescriptorSetLayout SSBOLayouts[] = { componentDescriptorSetLayout };
    std::array<vk::WriteDescriptorSet, 5> SSBODescriptorWrites = {};
    if (componentDescriptorSet == vk::DescriptorSet())
    {
        vk::DescriptorSetAllocateInfo allocInfo;
        allocInfo.descriptorPool = componentDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = SSBOLayouts;
        componentDescriptorSet = device.allocateDescriptorSets(allocInfo)[0];
    }

    // Entity SSBO
    vk::DescriptorBufferInfo entityBufferInfo;
    entityBufferInfo.buffer = Entity::GetSSBO();
    entityBufferInfo.offset = 0;
    entityBufferInfo.range = Entity::GetSSBOSize();

    if (entityBufferInfo.buffer == vk::Buffer()) return;

    SSBODescriptorWrites[0].dstSet = componentDescriptorSet;
    SSBODescriptorWrites[0].dstBinding = 0;
    SSBODescriptorWrites[0].dstArrayElement = 0;
    SSBODescriptorWrites[0].descriptorType = vk::DescriptorType::eStorageBuffer;
    SSBODescriptorWrites[0].descriptorCount = 1;
    SSBODescriptorWrites[0].pBufferInfo = &entityBufferInfo;

    // Transform SSBO
    vk::DescriptorBufferInfo transformBufferInfo;
    transformBufferInfo.buffer = Transform::GetSSBO();
    transformBufferInfo.offset = 0;
    transformBufferInfo.range = Transform::GetSSBOSize();

    if (transformBufferInfo.buffer == vk::Buffer()) return;

    SSBODescriptorWrites[1].dstSet = componentDescriptorSet;
    SSBODescriptorWrites[1].dstBinding = 1;
    SSBODescriptorWrites[1].dstArrayElement = 0;
    SSBODescriptorWrites[1].descriptorType = vk::DescriptorType::eStorageBuffer;
    SSBODescriptorWrites[1].descriptorCount = 1;
    SSBODescriptorWrites[1].pBufferInfo = &transformBufferInfo;

    // Camera SSBO
    vk::DescriptorBufferInfo cameraBufferInfo;
    cameraBufferInfo.buffer = Camera::GetSSBO();
    cameraBufferInfo.offset = 0;
    cameraBufferInfo.range = Camera::GetSSBOSize();

    if (cameraBufferInfo.buffer == vk::Buffer()) return;

    SSBODescriptorWrites[2].dstSet = componentDescriptorSet;
    SSBODescriptorWrites[2].dstBinding = 2;
    SSBODescriptorWrites[2].dstArrayElement = 0;
    SSBODescriptorWrites[2].descriptorType = vk::DescriptorType::eStorageBuffer;
    SSBODescriptorWrites[2].descriptorCount = 1;
    SSBODescriptorWrites[2].pBufferInfo = &cameraBufferInfo;

    // Material SSBO
    vk::DescriptorBufferInfo materialBufferInfo;
    materialBufferInfo.buffer = Material::GetSSBO();
    materialBufferInfo.offset = 0;
    materialBufferInfo.range = Material::GetSSBOSize();

    if (materialBufferInfo.buffer == vk::Buffer()) return;

    SSBODescriptorWrites[3].dstSet = componentDescriptorSet;
    SSBODescriptorWrites[3].dstBinding = 3;
    SSBODescriptorWrites[3].dstArrayElement = 0;
    SSBODescriptorWrites[3].descriptorType = vk::DescriptorType::eStorageBuffer;
    SSBODescriptorWrites[3].descriptorCount = 1;
    SSBODescriptorWrites[3].pBufferInfo = &materialBufferInfo;

    // Light SSBO
    vk::DescriptorBufferInfo lightBufferInfo;
    lightBufferInfo.buffer = Light::GetSSBO();
    lightBufferInfo.offset = 0;
    lightBufferInfo.range = Light::GetSSBOSize();

    if (lightBufferInfo.buffer == vk::Buffer()) return;

    SSBODescriptorWrites[4].dstSet = componentDescriptorSet;
    SSBODescriptorWrites[4].dstBinding = 4;
    SSBODescriptorWrites[4].dstArrayElement = 0;
    SSBODescriptorWrites[4].descriptorType = vk::DescriptorType::eStorageBuffer;
    SSBODescriptorWrites[4].descriptorCount = 1;
    SSBODescriptorWrites[4].pBufferInfo = &lightBufferInfo;
    
    device.updateDescriptorSets((uint32_t)SSBODescriptorWrites.size(), SSBODescriptorWrites.data(), 0, nullptr);
    
    /* ------ Texture Descriptor Set  ------ */
    vk::DescriptorSetLayout textureLayouts[] = { textureDescriptorSetLayout };
    std::array<vk::WriteDescriptorSet, 5> textureDescriptorWrites = {};
    
    if (textureDescriptorSet == vk::DescriptorSet())
    {
        vk::DescriptorSetAllocateInfo allocInfo;
        allocInfo.descriptorPool = textureDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = textureLayouts;

        textureDescriptorSet = device.allocateDescriptorSets(allocInfo)[0];
    }

    auto texture2DLayouts = Texture::GetLayouts(vk::ImageViewType::e2D);
    auto texture2DViews = Texture::GetImageViews(vk::ImageViewType::e2D);
    auto textureCubeLayouts = Texture::GetLayouts(vk::ImageViewType::eCube);
    auto textureCubeViews = Texture::GetImageViews(vk::ImageViewType::eCube);
    auto texture3DLayouts = Texture::GetLayouts(vk::ImageViewType::e3D);
    auto texture3DViews = Texture::GetImageViews(vk::ImageViewType::e3D);
    auto samplers = Texture::GetSamplers();

    // Texture SSBO
    vk::DescriptorBufferInfo textureBufferInfo;
    textureBufferInfo.buffer = Texture::GetSSBO();
    textureBufferInfo.offset = 0;
    textureBufferInfo.range = Texture::GetSSBOSize();

    if (textureBufferInfo.buffer == vk::Buffer()) return;

    textureDescriptorWrites[0].dstSet = textureDescriptorSet;
    textureDescriptorWrites[0].dstBinding = 0;
    textureDescriptorWrites[0].dstArrayElement = 0;
    textureDescriptorWrites[0].descriptorType = vk::DescriptorType::eStorageBuffer;
    textureDescriptorWrites[0].descriptorCount = 1;
    textureDescriptorWrites[0].pBufferInfo = &textureBufferInfo;

    // Samplers
    vk::DescriptorImageInfo samplerDescriptorInfos[MAX_SAMPLERS];
    for (int i = 0; i < MAX_SAMPLERS; ++i) 
    {
        samplerDescriptorInfos[i].sampler = samplers[i];
    }

    textureDescriptorWrites[1].dstSet = textureDescriptorSet;
    textureDescriptorWrites[1].dstBinding = 1;
    textureDescriptorWrites[1].dstArrayElement = 0;
    textureDescriptorWrites[1].descriptorType = vk::DescriptorType::eSampler;
    textureDescriptorWrites[1].descriptorCount = MAX_SAMPLERS;
    textureDescriptorWrites[1].pImageInfo = samplerDescriptorInfos;

    // 2D Textures
    vk::DescriptorImageInfo texture2DDescriptorInfos[MAX_TEXTURES];
    for (int i = 0; i < MAX_TEXTURES; ++i) 
    {
        texture2DDescriptorInfos[i].sampler = nullptr;
        texture2DDescriptorInfos[i].imageLayout = texture2DLayouts[i];
        texture2DDescriptorInfos[i].imageView = texture2DViews[i];
    }

    textureDescriptorWrites[2].dstSet = textureDescriptorSet;
    textureDescriptorWrites[2].dstBinding = 2;
    textureDescriptorWrites[2].dstArrayElement = 0;
    textureDescriptorWrites[2].descriptorType = vk::DescriptorType::eSampledImage;
    textureDescriptorWrites[2].descriptorCount = MAX_TEXTURES;
    textureDescriptorWrites[2].pImageInfo = texture2DDescriptorInfos;

    // Texture Cubes
    vk::DescriptorImageInfo textureCubeDescriptorInfos[MAX_TEXTURES];
    for (int i = 0; i < MAX_TEXTURES; ++i) 
    {
        textureCubeDescriptorInfos[i].sampler = nullptr;
        textureCubeDescriptorInfos[i].imageLayout = textureCubeLayouts[i];
        textureCubeDescriptorInfos[i].imageView = textureCubeViews[i];
    }

    textureDescriptorWrites[3].dstSet = textureDescriptorSet;
    textureDescriptorWrites[3].dstBinding = 3;
    textureDescriptorWrites[3].dstArrayElement = 0;
    textureDescriptorWrites[3].descriptorType = vk::DescriptorType::eSampledImage;
    textureDescriptorWrites[3].descriptorCount = MAX_TEXTURES;
    textureDescriptorWrites[3].pImageInfo = textureCubeDescriptorInfos;


    // 3D Textures
    vk::DescriptorImageInfo texture3DDescriptorInfos[MAX_TEXTURES];
    for (int i = 0; i < MAX_TEXTURES; ++i) 
    {
        texture3DDescriptorInfos[i].sampler = nullptr;
        texture3DDescriptorInfos[i].imageLayout = texture3DLayouts[i];
        texture3DDescriptorInfos[i].imageView = texture3DViews[i];
    }

    textureDescriptorWrites[4].dstSet = textureDescriptorSet;
    textureDescriptorWrites[4].dstBinding = 4;
    textureDescriptorWrites[4].dstArrayElement = 0;
    textureDescriptorWrites[4].descriptorType = vk::DescriptorType::eSampledImage;
    textureDescriptorWrites[4].descriptorCount = MAX_TEXTURES;
    textureDescriptorWrites[4].pImageInfo = texture3DDescriptorInfos;
    
    device.updateDescriptorSets((uint32_t)textureDescriptorWrites.size(), textureDescriptorWrites.data(), 0, nullptr);
}

void Material::UpdateRaytracingDescriptorSets()
{
    // TODO
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

void Material::BindDescriptorSets(vk::CommandBuffer &command_buffer, vk::RenderPass &render_pass) 
{
    std::vector<vk::DescriptorSet> descriptorSets = {componentDescriptorSet, textureDescriptorSet};
    command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, normalsurface[render_pass].pipelineLayout, 0, 2, descriptorSets.data(), 0, nullptr);
    command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, blinn[render_pass].pipelineLayout, 0, 2, descriptorSets.data(), 0, nullptr);
    command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, texcoordsurface[render_pass].pipelineLayout, 0, 2, descriptorSets.data(), 0, nullptr);
    command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pbr[render_pass].pipelineLayout, 0, 2, descriptorSets.data(), 0, nullptr);
    command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, skybox[render_pass].pipelineLayout, 0, 2, descriptorSets.data(), 0, nullptr);
    command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, depth[render_pass].pipelineLayout, 0, 2, descriptorSets.data(), 0, nullptr);
    command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, volume[render_pass].pipelineLayout, 0, 2, descriptorSets.data(), 0, nullptr);
}

void Material::DrawEntity(vk::CommandBuffer &command_buffer, vk::RenderPass &render_pass, Entity &entity, PushConsts &push_constants, RenderMode rendermode_override) //int32_t camera_id, int32_t environment_id, int32_t diffuse_id, int32_t irradiance_id, float gamma, float exposure, std::vector<int32_t> &light_entity_ids, double time)
{    
    /* Need a mesh to render. */
    auto mesh_id = entity.get_mesh();
    if (mesh_id < 0 || mesh_id >= MAX_MESHES) return;
    auto m = Mesh::Get((uint32_t) mesh_id);
    if (!m) return;

    /* Need a transform to render. */
    auto transform_id = entity.get_transform();
    if (transform_id < 0 || transform_id >= MAX_TRANSFORMS) return;

    // push_constants.show_bounding_box = false;
    // if (m->should_show_bounding_box()) {
    //     push_constants.show_bounding_box = true;
    //     m = Mesh::Get("BoundingBox");
    //     push_constants.bounding_box_min = glm::vec4(m->get_min_aabb_corner(), 1.0f);
    //     push_constants.bounding_box_max = glm::vec4(m->get_max_aabb_corner(), 1.0f);
    // }

    /* Need a material to render. */
    auto material_id = entity.get_material();
    if (material_id < 0 || material_id >= MAX_MATERIALS) return;
    auto material = Material::Get(material_id);
    if (!material) return;

    auto rendermode = (rendermode_override == RenderMode::NONE) ? material->renderMode : rendermode_override;

    /* Dont render volumes yet. */
    if (rendermode == VOLUME) return;
    if (rendermode == HIDDEN) return;

    if (rendermode == NORMAL) {
        command_buffer.pushConstants(normalsurface[render_pass].pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
        
        if ((currentlyBoundRenderMode != NORMAL) || (currentRenderpass != render_pass)) {
            command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, normalsurface[render_pass].pipeline);
            currentlyBoundRenderMode = NORMAL;
            currentRenderpass = render_pass;
        }
    }
    else if (rendermode == BLINN) {
        command_buffer.pushConstants(blinn[render_pass].pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
        if ((currentlyBoundRenderMode != BLINN) || (currentRenderpass != render_pass)) {
            command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, blinn[render_pass].pipeline);
            currentlyBoundRenderMode = BLINN;
            currentRenderpass = render_pass;
        }
    }
    else if (rendermode == TEXCOORD) {
        command_buffer.pushConstants(texcoordsurface[render_pass].pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
        if ((currentlyBoundRenderMode != TEXCOORD) || (currentRenderpass != render_pass)) {
            command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, texcoordsurface[render_pass].pipeline);
            currentlyBoundRenderMode = TEXCOORD;
            currentRenderpass = render_pass;
        }
    }
    else if (rendermode == PBR) {
        command_buffer.pushConstants(pbr[render_pass].pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
        if ((currentlyBoundRenderMode != PBR) || (currentRenderpass != render_pass)) {
            command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pbr[render_pass].pipeline);
            currentlyBoundRenderMode = PBR;
            currentRenderpass = render_pass;
        }
    }
    else if (rendermode == DEPTH) {
        command_buffer.pushConstants(depth[render_pass].pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
        if ((currentlyBoundRenderMode != DEPTH) || (currentRenderpass != render_pass)) {
            command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, depth[render_pass].pipeline);
            currentlyBoundRenderMode = DEPTH;
            currentRenderpass = render_pass;
        }
    }
    else if (rendermode == SKYBOX) {
        command_buffer.pushConstants(skybox[render_pass].pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
        if ((currentlyBoundRenderMode != SKYBOX) || (currentRenderpass != render_pass)) {
            command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, skybox[render_pass].pipeline);
            currentlyBoundRenderMode = SKYBOX;
            currentRenderpass = render_pass;
        }
    }
    else if (rendermode == FRAGMENTDEPTH) {
        command_buffer.pushConstants(fragmentdepth[render_pass].pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
        if ((currentlyBoundRenderMode != FRAGMENTDEPTH) || (currentRenderpass != render_pass)) {
            command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, fragmentdepth[render_pass].pipeline);
            currentlyBoundRenderMode = FRAGMENTDEPTH;
            currentRenderpass = render_pass;
        }
    }
    else if (rendermode == FRAGMENTPOSITION) {
        command_buffer.pushConstants(fragmentposition[render_pass].pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
        if ((currentlyBoundRenderMode != FRAGMENTPOSITION) || (currentRenderpass != render_pass)) {
            command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, fragmentposition[render_pass].pipeline);
            currentlyBoundRenderMode = FRAGMENTPOSITION;
            currentRenderpass = render_pass;
        }
    }
    else if (rendermode == SHADOWMAP) {
        command_buffer.pushConstants(shadowmap[render_pass].pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
        if ((currentlyBoundRenderMode != SHADOWMAP) || (currentRenderpass != render_pass)) {
            command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, shadowmap[render_pass].pipeline);
            currentlyBoundRenderMode = SHADOWMAP;
            currentRenderpass = render_pass;
        }
    }
    
    command_buffer.bindVertexBuffers(0, {m->get_point_buffer(), m->get_color_buffer(), m->get_normal_buffer(), m->get_texcoord_buffer()}, {0,0,0,0});
    command_buffer.bindIndexBuffer(m->get_index_buffer(), 0, vk::IndexType::eUint32);
    command_buffer.drawIndexed(m->get_total_indices(), 1, 0, 0, 0);
}

void Material::DrawVolume(vk::CommandBuffer &command_buffer, vk::RenderPass &render_pass, Entity &entity, PushConsts &push_constants, RenderMode rendermode_override) //int32_t camera_id, int32_t environment_id, int32_t diffuse_id, int32_t irradiance_id, float gamma, float exposure, std::vector<int32_t> &light_entity_ids, double time)
{    
    /* Need a mesh to render. */
    auto mesh_id = entity.get_mesh();
    if (mesh_id < 0 || mesh_id >= MAX_MESHES) return;
    auto m = Mesh::Get((uint32_t) mesh_id);
    if (!m) return;

    /* Need a transform to render. */
    auto transform_id = entity.get_transform();
    if (transform_id < 0 || transform_id >= MAX_TRANSFORMS) return;

    /* Need a material to render. */
    auto material_id = entity.get_material();
    if (material_id < 0 || material_id >= MAX_MATERIALS) return;
    auto material = Material::Get(material_id);
    if (!material) return;

    auto rendermode = (rendermode_override == RenderMode::NONE) ? material->renderMode : rendermode_override;

    if (rendermode != VOLUME) return;
    
    {
        command_buffer.pushConstants(volume[render_pass].pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
        
        if ((currentlyBoundRenderMode != VOLUME) || (currentRenderpass != render_pass)) {
            command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, volume[render_pass].pipeline);
            currentlyBoundRenderMode = VOLUME;
            currentRenderpass = render_pass;
        }
    }
    
    command_buffer.bindVertexBuffers(0, {m->get_point_buffer(), m->get_color_buffer(), m->get_normal_buffer(), m->get_texcoord_buffer()}, {0,0,0,0});
    command_buffer.bindIndexBuffer(m->get_index_buffer(), 0, vk::IndexType::eUint32);
    command_buffer.drawIndexed(m->get_total_indices(), 1, 0, 0, 0);
}

void Material::CreateSSBO() 
{
    auto vulkan = Libraries::Vulkan::Get();
    auto device = vulkan->get_device();
    auto physical_device = vulkan->get_physical_device();

    {
        vk::BufferCreateInfo bufferInfo = {};
        bufferInfo.size = MAX_MATERIALS * sizeof(MaterialStruct);
        bufferInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;
        bufferInfo.sharingMode = vk::SharingMode::eExclusive;
        stagingSSBO = device.createBuffer(bufferInfo);

        vk::MemoryRequirements memReqs = device.getBufferMemoryRequirements(stagingSSBO);
        vk::MemoryAllocateInfo allocInfo = {};
        allocInfo.allocationSize = memReqs.size;

        vk::PhysicalDeviceMemoryProperties memProperties = physical_device.getMemoryProperties();
        vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        allocInfo.memoryTypeIndex = vulkan->find_memory_type(memReqs.memoryTypeBits, properties);

        stagingSSBOMemory = device.allocateMemory(allocInfo);
        device.bindBufferMemory(stagingSSBO, stagingSSBOMemory, 0);
    }

    {
        vk::BufferCreateInfo bufferInfo = {};
        bufferInfo.size = MAX_MATERIALS * sizeof(MaterialStruct);
        bufferInfo.usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst;
        bufferInfo.sharingMode = vk::SharingMode::eExclusive;
        SSBO = device.createBuffer(bufferInfo);

        vk::MemoryRequirements memReqs = device.getBufferMemoryRequirements(SSBO);
        vk::MemoryAllocateInfo allocInfo = {};
        allocInfo.allocationSize = memReqs.size;

        vk::PhysicalDeviceMemoryProperties memProperties = physical_device.getMemoryProperties();
        vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
        allocInfo.memoryTypeIndex = vulkan->find_memory_type(memReqs.memoryTypeBits, properties);

        SSBOMemory = device.allocateMemory(allocInfo);
        device.bindBufferMemory(SSBO, SSBOMemory, 0);
    }
}

void Material::ResetBoundMaterial()
{
    currentRenderpass = vk::RenderPass();
    currentlyBoundRenderMode = NONE;
}

void Material::UploadSSBO(vk::CommandBuffer command_buffer)
{
    auto vulkan = Libraries::Vulkan::Get();
    auto device = vulkan->get_device();

    if (SSBOMemory == vk::DeviceMemory()) return;
    if (stagingSSBOMemory == vk::DeviceMemory()) return;

    auto bufferSize = MAX_MATERIALS * sizeof(MaterialStruct);

    /* Pin the buffer */
    pinnedMemory = (MaterialStruct*) device.mapMemory(stagingSSBOMemory, 0, bufferSize);

    if (pinnedMemory == nullptr) return;
    MaterialStruct material_structs[MAX_MATERIALS];
    
    /* TODO: remove this for loop */
    for (int i = 0; i < MAX_MATERIALS; ++i) {
        if (!materials[i].is_initialized()) continue;
        material_structs[i] = materials[i].material_struct;
    };

    /* Copy to GPU mapped memory */
    memcpy(pinnedMemory, material_structs, sizeof(material_structs));

    device.unmapMemory(stagingSSBOMemory);

    vk::BufferCopy copyRegion;
	copyRegion.size = bufferSize;
    command_buffer.copyBuffer(stagingSSBO, SSBO, copyRegion);
} 

vk::Buffer Material::GetSSBO()
{
    if ((SSBO != vk::Buffer()) && (SSBOMemory != vk::DeviceMemory()))
        return SSBO;
    else return vk::Buffer();
}

uint32_t Material::GetSSBOSize()
{
    return MAX_MATERIALS * sizeof(MaterialStruct);
}

void Material::CleanUp()
{
    if (!IsInitialized()) return;

    for (auto &material : materials) {
		if (material.initialized) {
			Material::Delete(material.id);
		}
	}

    auto vulkan = Libraries::Vulkan::Get();
    if (!vulkan->is_initialized())
        throw std::runtime_error( std::string("Vulkan library is not initialized"));
    auto device = vulkan->get_device();
    if (device == vk::Device())
        throw std::runtime_error( std::string("Invalid vulkan device"));

    device.destroyBuffer(SSBO);
    device.freeMemory(SSBOMemory);

    device.destroyBuffer(stagingSSBO);
    device.freeMemory(stagingSSBOMemory);

    device.destroyDescriptorSetLayout(componentDescriptorSetLayout);
    device.destroyDescriptorPool(componentDescriptorPool);

    device.destroyDescriptorSetLayout(textureDescriptorSetLayout);
    device.destroyDescriptorPool(textureDescriptorPool);

    SSBO = vk::Buffer();
    SSBOMemory = vk::DeviceMemory();
    stagingSSBO = vk::Buffer();
    stagingSSBOMemory = vk::DeviceMemory();

    componentDescriptorSetLayout = vk::DescriptorSetLayout();
    componentDescriptorPool = vk::DescriptorPool();
    textureDescriptorSetLayout = vk::DescriptorSetLayout();
    textureDescriptorPool = vk::DescriptorPool();

    Initialized = false;
}	

/* Static Factory Implementations */
Material* Material::Create(std::string name) {
    std::lock_guard<std::mutex> lock(creation_mutex);
    return StaticFactory::Create(name, "Material", lookupTable, materials, MAX_MATERIALS);
}

Material* Material::Get(std::string name) {
    return StaticFactory::Get(name, "Material", lookupTable, materials, MAX_MATERIALS);
}

Material* Material::Get(uint32_t id) {
    return StaticFactory::Get(id, "Material", lookupTable, materials, MAX_MATERIALS);
}

void Material::Delete(std::string name) {
    StaticFactory::Delete(name, "Material", lookupTable, materials, MAX_MATERIALS);
}

void Material::Delete(uint32_t id) {
    StaticFactory::Delete(id, "Material", lookupTable, materials, MAX_MATERIALS);
}

Material* Material::GetFront() {
    return materials;
}

uint32_t Material::GetCount() {
    return MAX_MATERIALS;
}

void Material::set_base_color_texture(uint32_t texture_id) 
{
    this->material_struct.base_color_texture_id = texture_id;
}

void Material::set_base_color_texture(Texture *texture) 
{
    if (!texture) 
        throw std::runtime_error( std::string("Invalid texture handle"));
    this->material_struct.base_color_texture_id = texture->get_id();
}

void Material::clear_base_color_texture() {
    this->material_struct.base_color_texture_id = -1;
}

void Material::set_roughness_texture(uint32_t texture_id) 
{
    this->material_struct.roughness_texture_id = texture_id;
}

void Material::set_roughness_texture(Texture *texture) 
{
    if (!texture) 
        throw std::runtime_error( std::string("Invalid texture handle"));
    this->material_struct.roughness_texture_id = texture->get_id();
}

void Material::use_vertex_colors(bool use)
{
    if (use) {
        this->material_struct.flags |= (1 << 0);
    } else {
        this->material_struct.flags &= ~(1 << 0);
    }
}

void Material::set_volume_texture(uint32_t texture_id)
{
    this->material_struct.volume_texture_id = texture_id;
}

void Material::set_volume_texture(Texture *texture)
{
    if (!texture) 
        throw std::runtime_error( std::string("Invalid texture handle"));
    this->material_struct.volume_texture_id = texture->get_id();
}

void Material::clear_roughness_texture() {
    this->material_struct.roughness_texture_id = -1;
}

void Material::set_transfer_function_texture(uint32_t texture_id)
{
    this->material_struct.transfer_function_texture_id = texture_id;
}

void Material::set_transfer_function_texture(Texture *texture)
{
    if (!texture) 
        throw std::runtime_error( std::string("Invalid texture handle"));
    this->material_struct.transfer_function_texture_id = texture->get_id();
}

void Material::clear_transfer_function_texture()
{
    this->material_struct.transfer_function_texture_id = -1;
}


void Material::show_pbr() {
    renderMode = PBR;
}

void Material::show_normals () {
    renderMode = NORMAL;
}

void Material::show_base_color() {
    renderMode = BASECOLOR;
}

void Material::show_texcoords() {
    renderMode = TEXCOORD;
}

void Material::show_blinn() {
    renderMode = BLINN;
}

void Material::show_depth() {
    renderMode = DEPTH;
}

void Material::show_position() {
    renderMode = FRAGMENTPOSITION;
}

void Material::show_volume() {
    renderMode = VOLUME;
}

void Material::show_environment() {
    renderMode = SKYBOX;
}

void Material::hide() {
    renderMode = HIDDEN;
}

void Material::set_base_color(glm::vec4 color) {
    this->material_struct.base_color = color;
}

void Material::set_base_color(float r, float g, float b, float a) {
    this->material_struct.base_color = glm::vec4(r, g, b, a);
}

void Material::set_roughness(float roughness) {
    this->material_struct.roughness = roughness;
}

void Material::set_metallic(float metallic) {
    this->material_struct.metallic = metallic;
}

void Material::set_transmission(float transmission) {
    this->material_struct.transmission = transmission;
}

void Material::set_transmission_roughness(float transmission_roughness) {
    this->material_struct.transmission_roughness = transmission_roughness;
}

void Material::set_ior(float ior) {
    this->material_struct.ior = ior;
}
