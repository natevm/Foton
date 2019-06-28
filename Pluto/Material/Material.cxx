#include "./Material.hxx"
#include "Pluto/Tools/Options.hxx"
#include "Pluto/Tools/FileReader.hxx"

#include "Pluto/Entity/Entity.hxx"
#include "Pluto/Transform/Transform.hxx"
#include "Pluto/Mesh/Mesh.hxx"
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

RenderMode Material::currentlyBoundRenderMode = RenderMode::RENDER_MODE_NONE;
PipelineType Material::currentlyBoundPipelineType = PipelineType::PIPELINE_TYPE_NORMAL;
vk::RenderPass Material::currentRenderpass = vk::RenderPass();;

vk::DescriptorSetLayout Material::componentDescriptorSetLayout;
vk::DescriptorSetLayout Material::textureDescriptorSetLayout;
vk::DescriptorSetLayout Material::positionsDescriptorSetLayout;
vk::DescriptorSetLayout Material::normalsDescriptorSetLayout;
vk::DescriptorSetLayout Material::colorsDescriptorSetLayout;
vk::DescriptorSetLayout Material::texcoordsDescriptorSetLayout;
vk::DescriptorSetLayout Material::indexDescriptorSetLayout;
vk::DescriptorSetLayout Material::raytracingDescriptorSetLayout;

vk::DescriptorPool Material::componentDescriptorPool;
vk::DescriptorPool Material::textureDescriptorPool;
vk::DescriptorPool Material::positionsDescriptorPool;
vk::DescriptorPool Material::normalsDescriptorPool;
vk::DescriptorPool Material::colorsDescriptorPool;
vk::DescriptorPool Material::texcoordsDescriptorPool;
vk::DescriptorPool Material::indexDescriptorPool;
vk::DescriptorPool Material::raytracingDescriptorPool;

std::vector<vk::VertexInputBindingDescription> Material::vertexInputBindingDescriptions;
std::vector<vk::VertexInputAttributeDescription> Material::vertexInputAttributeDescriptions;

vk::DescriptorSet Material::componentDescriptorSet;
vk::DescriptorSet Material::textureDescriptorSet;
vk::DescriptorSet Material::positionsDescriptorSet;
vk::DescriptorSet Material::normalsDescriptorSet;
vk::DescriptorSet Material::colorsDescriptorSet;
vk::DescriptorSet Material::texcoordsDescriptorSet;
vk::DescriptorSet Material::indexDescriptorSet;
vk::DescriptorSet Material::raytracingDescriptorSet;

std::mutex Material::creation_mutex;
bool Material::Initialized = false;

// std::map<vk::RenderPass, Material::RasterPipelineResources> Material::uniformColor;
// std::map<vk::RenderPass, Material::RasterPipelineResources> Material::blinn;
std::map<vk::RenderPass, Material::RasterPipelineResources> Material::pbr;
// std::map<vk::RenderPass, Material::RasterPipelineResources> Material::texcoordsurface;
// std::map<vk::RenderPass, Material::RasterPipelineResources> Material::normalsurface;
std::map<vk::RenderPass, Material::RasterPipelineResources> Material::skybox;
// std::map<vk::RenderPass, Material::RasterPipelineResources> Material::depth;
std::map<vk::RenderPass, Material::RasterPipelineResources> Material::volume;

std::map<vk::RenderPass, Material::RasterPipelineResources> Material::shadowmap;
std::map<vk::RenderPass, Material::RasterPipelineResources> Material::fragmentdepth;
// std::map<vk::RenderPass, Material::RasterPipelineResources> Material::fragmentposition;
std::map<vk::RenderPass, Material::RasterPipelineResources> Material::vrmask;

std::map<vk::RenderPass, Material::RasterPipelineResources> Material::gbuffers;

std::map<vk::RenderPass, Material::RaytracingPipelineResources> Material::rttest;

std::unordered_map<std::string, vk::ShaderModule> Material::shaderModuleCache;

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
	material_struct.bump_texture_id = -1;
	material_struct.alpha_texture_id = -1;
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
vk::ShaderModule Material::CreateShaderModule(std::string name, const std::vector<char>& code) {
	if (shaderModuleCache.find(name) != shaderModuleCache.end()) return shaderModuleCache[name];
	auto vulkan = Libraries::Vulkan::Get();
	auto device = vulkan->get_device();

	vk::ShaderModuleCreateInfo createInfo;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	vk::ShaderModule shaderModule = device.createShaderModule(createInfo);
	shaderModuleCache[name] = shaderModule;
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
	std::unordered_map<PipelineType, vk::Pipeline> &pipelines,
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

	#ifndef DISABLE_REVERSE_Z
	auto prepassDepthCompareOp = vk::CompareOp::eGreater;
	auto prepassDepthCompareOpEqual = vk::CompareOp::eGreaterOrEqual;
	auto depthCompareOpGreaterEqual = vk::CompareOp::eGreaterOrEqual;
	auto depthCompareOpLessEqual = vk::CompareOp::eLessOrEqual;
	#else
	auto prepassDepthCompareOp = vk::CompareOp::eLess;
	auto prepassDepthCompareOpEqual = vk::CompareOp::eLessOrEqual;
	auto depthCompareOpGreaterEqual = vk::CompareOp::eLessOrEqual;
	auto depthCompareOpLessEqual = vk::CompareOp::eGreaterOrEqual;
	#endif	


	/* Create pipeline */
	pipelines[PIPELINE_TYPE_NORMAL] = device.createGraphicsPipelines(vk::PipelineCache(), {pipelineInfo})[0];
	
	auto old_depth_test_enable_setting = parameters.depthStencil.depthTestEnable;
	auto old_depth_write_enable_setting = parameters.depthStencil.depthWriteEnable;
	auto old_cull_mode = parameters.rasterizer.cullMode;
	auto old_depth_compare_op = parameters.depthStencil.depthCompareOp;

	parameters.depthStencil.setDepthTestEnable(true);
	parameters.depthStencil.setDepthWriteEnable(false); // possibly rename this?

	// fragmentdepth[renderpass].pipelineParameters.depthStencil.depthCompareOp = depthCompareOpEqual;
	// fragmentdepth[renderpass].pipelineParameters.rasterizer.cullMode = vk::CullModeFlagBits::eNone;
	parameters.depthStencil.setDepthCompareOp(prepassDepthCompareOpEqual);	// Needed for transparent objects.
	// parameters.depthStencil.setDepthCompareOp(prepassDepthCompareOpEqual);	
	
	parameters.rasterizer.setCullMode(vk::CullModeFlagBits::eNone); // possibly rename this?
	pipelines[PIPELINE_TYPE_DEPTH_WRITE_DISABLED] = device.createGraphicsPipelines(vk::PipelineCache(), {pipelineInfo})[0];
	
	parameters.depthStencil.setDepthCompareOp(prepassDepthCompareOpEqual);	
	parameters.rasterizer.setCullMode(old_cull_mode); // possibly rename this?
	parameters.depthStencil.setDepthWriteEnable(true); // possibly rename this?

	// parameters.depthStencil.setDepthWriteEnable(old_depth_write_enable_setting);
	// parameters.depthStencil.setDepthTestEnable(true);
	parameters.depthStencil.setDepthCompareOp(depthCompareOpGreaterEqual);
	pipelines[PIPELINE_TYPE_DEPTH_TEST_GREATER] = device.createGraphicsPipelines(vk::PipelineCache(), {pipelineInfo})[0];
	parameters.depthStencil.setDepthCompareOp(depthCompareOpLessEqual);
	pipelines[PIPELINE_TYPE_DEPTH_TEST_LESS] = device.createGraphicsPipelines(vk::PipelineCache(), {pipelineInfo})[0];
	parameters.depthStencil.setDepthTestEnable(old_depth_test_enable_setting);
	parameters.depthStencil.setDepthCompareOp(old_depth_compare_op);
	parameters.depthStencil.setDepthWriteEnable(old_depth_write_enable_setting);


	// auto old_fill_setting = pipelineInfo.pRasterizationState->polygonMode;
	// pipelineInfo.pRasterizationState->polygonMode = vk::Poly

	auto old_polygon_mode = parameters.rasterizer.polygonMode;
	parameters.rasterizer.polygonMode = vk::PolygonMode::eLine;
	pipelines[PIPELINE_TYPE_WIREFRAME] = device.createGraphicsPipelines(vk::PipelineCache(), {pipelineInfo})[0];
	parameters.rasterizer.polygonMode = old_polygon_mode;
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
	// {
	// 	uniformColor[renderpass] = RasterPipelineResources();

	// 	std::string ResourcePath = Options::GetResourcePath();
	// 	auto vertShaderCode = readFile(ResourcePath + std::string("/Shaders/SurfaceMaterials/UniformColor/vert.spv"));
	// 	auto fragShaderCode = readFile(ResourcePath + std::string("/Shaders/SurfaceMaterials/UniformColor/frag.spv"));

	// 	/* Create shader modules */
	// 	auto vertShaderModule = CreateShaderModule("uniform_color_vert", vertShaderCode);
	// 	auto fragShaderModule = CreateShaderModule("uniform_color_frag", fragShaderCode);

	// 	/* Info for shader stages */
	// 	vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
	// 	vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
	// 	vertShaderStageInfo.module = vertShaderModule;
	// 	vertShaderStageInfo.pName = "main";

	// 	vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
	// 	fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
	// 	fragShaderStageInfo.module = fragShaderModule;
	// 	fragShaderStageInfo.pName = "main";

	// 	std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertShaderStageInfo, fragShaderStageInfo };
		
	// 	/* Account for possibly multiple samples */
	// 	uniformColor[renderpass].pipelineParameters.multisampling.sampleShadingEnable = (sampleFlag == vk::SampleCountFlagBits::e1) ? false : true;
	// 	uniformColor[renderpass].pipelineParameters.multisampling.rasterizationSamples = sampleFlag;

	// 	CreateRasterPipeline(shaderStages, vertexInputBindingDescriptions, vertexInputAttributeDescriptions, 
	// 		{ componentDescriptorSetLayout, textureDescriptorSetLayout }, 
	// 		uniformColor[renderpass].pipelineParameters, 
	// 		renderpass, 0, 
	// 		uniformColor[renderpass].pipelines, uniformColor[renderpass].pipelineLayout);

	// }


	/* ------ BLINN GRAPHICS ------ */
	// {
	// 	blinn[renderpass] = RasterPipelineResources();

	// 	std::string ResourcePath = Options::GetResourcePath();
	// 	auto vertShaderCode = readFile(ResourcePath + std::string("/Shaders/SurfaceMaterials/Blinn/vert.spv"));
	// 	auto fragShaderCode = readFile(ResourcePath + std::string("/Shaders/SurfaceMaterials/Blinn/frag.spv"));

	// 	/* Create shader modules */
	// 	auto vertShaderModule = CreateShaderModule("blinn_vert", vertShaderCode);
	// 	auto fragShaderModule = CreateShaderModule("blinn_frag", fragShaderCode);

	// 	/* Info for shader stages */
	// 	vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
	// 	vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
	// 	vertShaderStageInfo.module = vertShaderModule;
	// 	vertShaderStageInfo.pName = "main";

	// 	vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
	// 	fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
	// 	fragShaderStageInfo.module = fragShaderModule;
	// 	fragShaderStageInfo.pName = "main";

	// 	std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertShaderStageInfo, fragShaderStageInfo };
		
	// 	/* Account for possibly multiple samples */
	// 	blinn[renderpass].pipelineParameters.multisampling.sampleShadingEnable = (sampleFlag == vk::SampleCountFlagBits::e1) ? false : true;
	// 	blinn[renderpass].pipelineParameters.multisampling.rasterizationSamples = sampleFlag;

	// 	CreateRasterPipeline(shaderStages, vertexInputBindingDescriptions, vertexInputAttributeDescriptions, 
	// 		{ componentDescriptorSetLayout, textureDescriptorSetLayout }, 
	// 		blinn[renderpass].pipelineParameters, 
	// 		renderpass, 0, 
	// 		blinn[renderpass].pipelines, blinn[renderpass].pipelineLayout);

	// }

	/* ------ PBR  ------ */
	{
		pbr[renderpass] = RasterPipelineResources();

		std::string ResourcePath = Options::GetResourcePath();
		auto vertShaderCode = readFile(ResourcePath + std::string("/Shaders/SurfaceMaterials/PBRSurface/vert.spv"));
		auto fragShaderCode = readFile(ResourcePath + std::string("/Shaders/SurfaceMaterials/PBRSurface/frag.spv"));

		/* Create shader modules */
		auto vertShaderModule = CreateShaderModule("pbr_vert", vertShaderCode);
		auto fragShaderModule = CreateShaderModule("pbr_frag", fragShaderCode);

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
			{ componentDescriptorSetLayout, textureDescriptorSetLayout,
				positionsDescriptorSetLayout, normalsDescriptorSetLayout, colorsDescriptorSetLayout, texcoordsDescriptorSetLayout, indexDescriptorSetLayout, }, 
			pbr[renderpass].pipelineParameters, 
			renderpass, 0, 
			pbr[renderpass].pipelines, pbr[renderpass].pipelineLayout);

	}

	// /* ------ NORMAL SURFACE ------ */
	// {
	// 	normalsurface[renderpass] = RasterPipelineResources();

	// 	std::string ResourcePath = Options::GetResourcePath();
	// 	auto vertShaderCode = readFile(ResourcePath + std::string("/Shaders/SurfaceMaterials/NormalSurface/vert.spv"));
	// 	auto fragShaderCode = readFile(ResourcePath + std::string("/Shaders/SurfaceMaterials/NormalSurface/frag.spv"));

	// 	/* Create shader modules */
	// 	auto vertShaderModule = CreateShaderModule("normal_surf_vert", vertShaderCode);
	// 	auto fragShaderModule = CreateShaderModule("normal_surf_frag", fragShaderCode);

	// 	/* Info for shader stages */
	// 	vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
	// 	vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
	// 	vertShaderStageInfo.module = vertShaderModule;
	// 	vertShaderStageInfo.pName = "main";

	// 	vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
	// 	fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
	// 	fragShaderStageInfo.module = fragShaderModule;
	// 	fragShaderStageInfo.pName = "main";

	// 	std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertShaderStageInfo, fragShaderStageInfo };
		
	// 	/* Account for possibly multiple samples */
	// 	normalsurface[renderpass].pipelineParameters.multisampling.sampleShadingEnable = (sampleFlag == vk::SampleCountFlagBits::e1) ? false : true;
	// 	normalsurface[renderpass].pipelineParameters.multisampling.rasterizationSamples = sampleFlag;

	// 	CreateRasterPipeline(shaderStages, vertexInputBindingDescriptions, vertexInputAttributeDescriptions, 
	// 		{ componentDescriptorSetLayout, textureDescriptorSetLayout }, 
	// 		normalsurface[renderpass].pipelineParameters, 
	// 		renderpass, 0, 
	// 		normalsurface[renderpass].pipelines, normalsurface[renderpass].pipelineLayout);

	// }

	/* ------ TEXCOORD SURFACE  ------ */
	// {
	// 	texcoordsurface[renderpass] = RasterPipelineResources();

	// 	std::string ResourcePath = Options::GetResourcePath();
	// 	auto vertShaderCode = readFile(ResourcePath + std::string("/Shaders/SurfaceMaterials/TexCoordSurface/vert.spv"));
	// 	auto fragShaderCode = readFile(ResourcePath + std::string("/Shaders/SurfaceMaterials/TexCoordSurface/frag.spv"));

	// 	/* Create shader modules */
	// 	auto vertShaderModule = CreateShaderModule("texcoord_vert", vertShaderCode);
	// 	auto fragShaderModule = CreateShaderModule("texcoord_frag", fragShaderCode);

	// 	/* Info for shader stages */
	// 	vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
	// 	vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
	// 	vertShaderStageInfo.module = vertShaderModule;
	// 	vertShaderStageInfo.pName = "main";

	// 	vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
	// 	fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
	// 	fragShaderStageInfo.module = fragShaderModule;
	// 	fragShaderStageInfo.pName = "main";

	// 	std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertShaderStageInfo, fragShaderStageInfo };
		
	// 	/* Account for possibly multiple samples */
	// 	texcoordsurface[renderpass].pipelineParameters.multisampling.sampleShadingEnable = (sampleFlag == vk::SampleCountFlagBits::e1) ? false : true;
	// 	texcoordsurface[renderpass].pipelineParameters.multisampling.rasterizationSamples = sampleFlag;

	// 	CreateRasterPipeline(shaderStages, vertexInputBindingDescriptions, vertexInputAttributeDescriptions, 
	// 		{ componentDescriptorSetLayout, textureDescriptorSetLayout }, 
	// 		texcoordsurface[renderpass].pipelineParameters, 
	// 		renderpass, 0, 
	// 		texcoordsurface[renderpass].pipelines, texcoordsurface[renderpass].pipelineLayout);

	// }

	/* ------ SKYBOX  ------ */
	{
		skybox[renderpass] = RasterPipelineResources();

		std::string ResourcePath = Options::GetResourcePath();
		auto vertShaderCode = readFile(ResourcePath + std::string("/Shaders/SurfaceMaterials/Skybox/vert.spv"));
		auto fragShaderCode = readFile(ResourcePath + std::string("/Shaders/SurfaceMaterials/Skybox/frag.spv"));

		/* Create shader modules */
		auto vertShaderModule = CreateShaderModule("skybox_vert", vertShaderCode);
		auto fragShaderModule = CreateShaderModule("skybox_frag", fragShaderCode);

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
			{ componentDescriptorSetLayout, textureDescriptorSetLayout,
				positionsDescriptorSetLayout, normalsDescriptorSetLayout, colorsDescriptorSetLayout, texcoordsDescriptorSetLayout, indexDescriptorSetLayout, }, 
			skybox[renderpass].pipelineParameters, 
			renderpass, 0, 
			skybox[renderpass].pipelines, skybox[renderpass].pipelineLayout);

	}

	/* ------ DEPTH  ------ */
	// {
	// 	depth[renderpass] = RasterPipelineResources();

	// 	std::string ResourcePath = Options::GetResourcePath();
	// 	auto vertShaderCode = readFile(ResourcePath + std::string("/Shaders/SurfaceMaterials/Depth/vert.spv"));
	// 	auto fragShaderCode = readFile(ResourcePath + std::string("/Shaders/SurfaceMaterials/Depth/frag.spv"));

	// 	/* Create shader modules */
	// 	auto vertShaderModule = CreateShaderModule("depth_vert", vertShaderCode);
	// 	auto fragShaderModule = CreateShaderModule("depth_frag", fragShaderCode);

	// 	/* Info for shader stages */
	// 	vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
	// 	vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
	// 	vertShaderStageInfo.module = vertShaderModule;
	// 	vertShaderStageInfo.pName = "main";

	// 	vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
	// 	fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
	// 	fragShaderStageInfo.module = fragShaderModule;
	// 	fragShaderStageInfo.pName = "main";

	// 	std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertShaderStageInfo, fragShaderStageInfo };
		
	// 	/* Account for possibly multiple samples */
	// 	depth[renderpass].pipelineParameters.multisampling.sampleShadingEnable = (sampleFlag == vk::SampleCountFlagBits::e1) ? false : true;
	// 	depth[renderpass].pipelineParameters.multisampling.rasterizationSamples = sampleFlag;
	// 	// depth[renderpass].pipelineParameters.rasterizer.cullMode = vk::CullModeFlagBits::eNone;

	// 	/* Because we use a depth prepass */
	// 	if (use_depth_prepass) {
	// 		depth[renderpass].pipelineParameters.depthStencil.depthTestEnable = true;
	// 		depth[renderpass].pipelineParameters.depthStencil.depthWriteEnable = false; // not VK_TRUE since we have a depth prepass
	// 		depth[renderpass].pipelineParameters.depthStencil.depthCompareOp = depthCompareOpEqual;
	// 	}

	// 	CreateRasterPipeline(shaderStages, vertexInputBindingDescriptions, vertexInputAttributeDescriptions, 
	// 		{ componentDescriptorSetLayout, textureDescriptorSetLayout }, 
	// 		depth[renderpass].pipelineParameters, 
	// 		renderpass, 0, 
	// 		depth[renderpass].pipelines, depth[renderpass].pipelineLayout);

	// }

	/* ------ Volume  ------ */
	{
		volume[renderpass] = RasterPipelineResources();

		std::string ResourcePath = Options::GetResourcePath();
		auto vertShaderCode = readFile(ResourcePath + std::string("/Shaders/VolumeMaterials/Volume/vert.spv"));
		auto fragShaderCode = readFile(ResourcePath + std::string("/Shaders/VolumeMaterials/Volume/frag.spv"));

		/* Create shader modules */
		auto vertShaderModule = CreateShaderModule("volume_vert", vertShaderCode);
		auto fragShaderModule = CreateShaderModule("volume_frag", fragShaderCode);

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
			{ componentDescriptorSetLayout, textureDescriptorSetLayout,
				positionsDescriptorSetLayout, normalsDescriptorSetLayout, colorsDescriptorSetLayout, texcoordsDescriptorSetLayout, indexDescriptorSetLayout, }, 
			volume[renderpass].pipelineParameters, 
			renderpass, 0, 
			volume[renderpass].pipelines, volume[renderpass].pipelineLayout);

	}

	/*  G BUFFERS  */

	/* ------ Fragment Depth  ------ */
	{
		fragmentdepth[renderpass] = RasterPipelineResources();

		std::string ResourcePath = Options::GetResourcePath();
		auto vertShaderCode = readFile(ResourcePath + std::string("/Shaders/GBuffers/FragmentDepth/vert.spv"));

		/* Create shader modules */
		auto vertShaderModule = CreateShaderModule("frag_depth", vertShaderCode);

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
			{ componentDescriptorSetLayout, textureDescriptorSetLayout,
				positionsDescriptorSetLayout, normalsDescriptorSetLayout, colorsDescriptorSetLayout, texcoordsDescriptorSetLayout, indexDescriptorSetLayout, }, 
			fragmentdepth[renderpass].pipelineParameters, 
			renderpass, 0, 
			fragmentdepth[renderpass].pipelines, fragmentdepth[renderpass].pipelineLayout);

		// device.destroyShaderModule(vertShaderModule);
	}

	/* ------ All G Buffers  ------ */
	{
		gbuffers[renderpass] = RasterPipelineResources();

		std::string ResourcePath = Options::GetResourcePath();
		auto vertShaderCode = readFile(ResourcePath + std::string("/Shaders/GBuffers/vert.spv"));
		auto fragShaderCode = readFile(ResourcePath + std::string("/Shaders/GBuffers/frag.spv"));

		/* Create shader modules */
		auto vertShaderModule = CreateShaderModule("gbuffers_vert", vertShaderCode);
		auto fragShaderModule = CreateShaderModule("gbuffers_frag", fragShaderCode);

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
		gbuffers[renderpass].pipelineParameters.multisampling.sampleShadingEnable = (sampleFlag == vk::SampleCountFlagBits::e1) ? false : true;
		gbuffers[renderpass].pipelineParameters.multisampling.rasterizationSamples = sampleFlag;
		gbuffers[renderpass].pipelineParameters.rasterizer.cullMode = vk::CullModeFlagBits::eNone;

		/* Because we use a depth prepass */
		if (use_depth_prepass) {
			gbuffers[renderpass].pipelineParameters.depthStencil.depthTestEnable = true;
			gbuffers[renderpass].pipelineParameters.depthStencil.depthWriteEnable = false; // not VK_TRUE since we have a depth prepass
			gbuffers[renderpass].pipelineParameters.depthStencil.depthCompareOp = depthCompareOpEqual;
		}

		CreateRasterPipeline(shaderStages, vertexInputBindingDescriptions, vertexInputAttributeDescriptions, 
			{ componentDescriptorSetLayout, textureDescriptorSetLayout,
				positionsDescriptorSetLayout, normalsDescriptorSetLayout, colorsDescriptorSetLayout, texcoordsDescriptorSetLayout, indexDescriptorSetLayout, }, 
			gbuffers[renderpass].pipelineParameters, 
			renderpass, 0, 
			gbuffers[renderpass].pipelines, gbuffers[renderpass].pipelineLayout);

	}

	/* ------ SHADOWMAP  ------ */

	{
		shadowmap[renderpass] = RasterPipelineResources();

		std::string ResourcePath = Options::GetResourcePath();
		auto vertShaderCode = readFile(ResourcePath + std::string("/Shaders/GBuffers/ShadowMap/vert.spv"));
		auto fragShaderCode = readFile(ResourcePath + std::string("/Shaders/GBuffers/ShadowMap/frag.spv"));

		/* Create shader modules */
		auto vertShaderModule = CreateShaderModule("shadow_map_vert", vertShaderCode);
		auto fragShaderModule = CreateShaderModule("shadow_map_frag", fragShaderCode);

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
			{ componentDescriptorSetLayout, textureDescriptorSetLayout,
				positionsDescriptorSetLayout, normalsDescriptorSetLayout, colorsDescriptorSetLayout, texcoordsDescriptorSetLayout, indexDescriptorSetLayout, }, 
			shadowmap[renderpass].pipelineParameters, 
			renderpass, 0, 
			shadowmap[renderpass].pipelines, shadowmap[renderpass].pipelineLayout);

	}

	/* ------ FRAGMENT POSITION  ------ */

	// {
	// 	fragmentposition[renderpass] = RasterPipelineResources();

	// 	std::string ResourcePath = Options::GetResourcePath();
	// 	auto vertShaderCode = readFile(ResourcePath + std::string("/Shaders/GBuffers/FragmentPosition/vert.spv"));
	// 	auto fragShaderCode = readFile(ResourcePath + std::string("/Shaders/GBuffers/FragmentPosition/frag.spv"));

	// 	/* Create shader modules */
	// 	auto vertShaderModule = CreateShaderModule("frag_pos_vert", vertShaderCode);
	// 	auto fragShaderModule = CreateShaderModule("frag_pos_frag", fragShaderCode);

	// 	/* Info for shader stages */
	// 	vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
	// 	vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
	// 	vertShaderStageInfo.module = vertShaderModule;
	// 	vertShaderStageInfo.pName = "main";

	// 	vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
	// 	fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
	// 	fragShaderStageInfo.module = fragShaderModule;
	// 	fragShaderStageInfo.pName = "main";

	// 	std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertShaderStageInfo, fragShaderStageInfo };
		
	// 	/* Account for possibly multiple samples */
	// 	fragmentposition[renderpass].pipelineParameters.multisampling.sampleShadingEnable = (sampleFlag == vk::SampleCountFlagBits::e1) ? false : true;
	// 	fragmentposition[renderpass].pipelineParameters.multisampling.rasterizationSamples = sampleFlag;
	// 	fragmentposition[renderpass].pipelineParameters.rasterizer.cullMode = vk::CullModeFlagBits::eNone;

	// 	/* Because we use a depth prepass */
	// 	if (use_depth_prepass) {
	// 		fragmentposition[renderpass].pipelineParameters.depthStencil.depthTestEnable = true;
	// 		fragmentposition[renderpass].pipelineParameters.depthStencil.depthWriteEnable = false; // not VK_TRUE since we have a depth prepass
	// 		fragmentposition[renderpass].pipelineParameters.depthStencil.depthCompareOp = depthCompareOpEqual;
	// 	}

	// 	CreateRasterPipeline(shaderStages, vertexInputBindingDescriptions, vertexInputAttributeDescriptions, 
	// 		{ componentDescriptorSetLayout, textureDescriptorSetLayout }, 
	// 		fragmentposition[renderpass].pipelineParameters, 
	// 		renderpass, 0, 
	// 		fragmentposition[renderpass].pipelines, fragmentposition[renderpass].pipelineLayout);

	// }

	/* ------ VR MASK ------ */
	{
		vrmask[renderpass] = RasterPipelineResources();

		std::string ResourcePath = Options::GetResourcePath();
		auto vertShaderCode = readFile(ResourcePath + std::string("/Shaders/GBuffers/VRMask/vert.spv"));

		/* Create shader modules */
		auto vertShaderModule = CreateShaderModule("vr_mask", vertShaderCode);

		/* Info for shader stages */
		vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
		vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName = "main";

		std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertShaderStageInfo };
		
		/* Account for possibly multiple samples */
		vrmask[renderpass].pipelineParameters.multisampling.sampleShadingEnable = (sampleFlag == vk::SampleCountFlagBits::e1) ? false : true;
		vrmask[renderpass].pipelineParameters.multisampling.rasterizationSamples = sampleFlag;

		vrmask[renderpass].pipelineParameters.depthStencil.depthWriteEnable = true;
		vrmask[renderpass].pipelineParameters.depthStencil.depthCompareOp = depthCompareOpEqual;
		vrmask[renderpass].pipelineParameters.rasterizer.cullMode = vk::CullModeFlagBits::eNone;
		
		CreateRasterPipeline(shaderStages, vertexInputBindingDescriptions, vertexInputAttributeDescriptions, 
			{ componentDescriptorSetLayout, textureDescriptorSetLayout,
			positionsDescriptorSetLayout, normalsDescriptorSetLayout, colorsDescriptorSetLayout, texcoordsDescriptorSetLayout, indexDescriptorSetLayout }, 
			vrmask[renderpass].pipelineParameters, 
			renderpass, 0, 
			vrmask[renderpass].pipelines, vrmask[renderpass].pipelineLayout);

		// device.destroyShaderModule(vertShaderModule);
	}

	if (!vulkan->is_ray_tracing_enabled()) return;
	auto dldi = vulkan->get_dldi();

	/* RAY TRACING PIPELINES */
	{
		rttest[renderpass] = RaytracingPipelineResources();

		/* RAY GEN SHADERS */
		std::string ResourcePath = Options::GetResourcePath();
		auto raygenShaderCode = readFile(ResourcePath + std::string("/Shaders/RaytracedMaterials/TutorialShaders/rgen.spv"));
		auto raychitShaderCode = readFile(ResourcePath + std::string("/Shaders/RaytracedMaterials/TutorialShaders/rchit.spv"));
		auto raymissShaderCode = readFile(ResourcePath + std::string("/Shaders/RaytracedMaterials/TutorialShaders/rmiss.spv"));
		
		/* Create shader modules */
		auto raygenShaderModule = CreateShaderModule("ray_tracing_gen", raygenShaderCode);
		auto raychitShaderModule = CreateShaderModule("ray_tracing_chit", raychitShaderCode);
		auto raymissShaderModule = CreateShaderModule("ray_tracing_miss", raymissShaderCode);

		/* Info for shader stages */
		vk::PipelineShaderStageCreateInfo raygenShaderStageInfo;
		raygenShaderStageInfo.stage = vk::ShaderStageFlagBits::eRaygenNV;
		raygenShaderStageInfo.module = raygenShaderModule;
		raygenShaderStageInfo.pName = "main"; 

		vk::PipelineShaderStageCreateInfo raymissShaderStageInfo;
		raymissShaderStageInfo.stage = vk::ShaderStageFlagBits::eMissNV;
		raymissShaderStageInfo.module = raymissShaderModule;
		raymissShaderStageInfo.pName = "main"; 

		vk::PipelineShaderStageCreateInfo raychitShaderStageInfo;
		raychitShaderStageInfo.stage = vk::ShaderStageFlagBits::eClosestHitNV;
		raychitShaderStageInfo.module = raychitShaderModule;
		raychitShaderStageInfo.pName = "main"; 

		std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { raygenShaderStageInfo, raymissShaderStageInfo, raychitShaderStageInfo};
		
		vk::PushConstantRange range;
		range.offset = 0;
		range.size = sizeof(PushConsts);
		range.stageFlags = vk::ShaderStageFlagBits::eAll;

		std::vector<vk::DescriptorSetLayout> layouts = { componentDescriptorSetLayout, textureDescriptorSetLayout, 
			positionsDescriptorSetLayout, normalsDescriptorSetLayout, colorsDescriptorSetLayout, texcoordsDescriptorSetLayout, indexDescriptorSetLayout,
			raytracingDescriptorSetLayout };
		vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo;

		pipelineLayoutCreateInfo.setLayoutCount = (uint32_t)layouts.size();
		pipelineLayoutCreateInfo.pSetLayouts = layouts.data();
		pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
		pipelineLayoutCreateInfo.pPushConstantRanges = &range;
		
		rttest[renderpass].pipelineLayout = device.createPipelineLayout(pipelineLayoutCreateInfo);

		std::vector<vk::RayTracingShaderGroupCreateInfoNV> shaderGroups;
		
		// Ray gen group
		vk::RayTracingShaderGroupCreateInfoNV rayGenGroupInfo;
		rayGenGroupInfo.type = vk::RayTracingShaderGroupTypeNV::eGeneral;
		rayGenGroupInfo.generalShader = 0;
		rayGenGroupInfo.closestHitShader = VK_SHADER_UNUSED_NV;
		rayGenGroupInfo.anyHitShader = VK_SHADER_UNUSED_NV;
		rayGenGroupInfo.intersectionShader = VK_SHADER_UNUSED_NV;
		shaderGroups.push_back(rayGenGroupInfo);
		
		// Miss group
		vk::RayTracingShaderGroupCreateInfoNV rayMissGroupInfo;
		rayMissGroupInfo.type = vk::RayTracingShaderGroupTypeNV::eGeneral;
		rayMissGroupInfo.generalShader = 1;
		rayMissGroupInfo.closestHitShader = VK_SHADER_UNUSED_NV;
		rayMissGroupInfo.anyHitShader = VK_SHADER_UNUSED_NV;
		rayMissGroupInfo.intersectionShader = VK_SHADER_UNUSED_NV;
		shaderGroups.push_back(rayMissGroupInfo);
		
		// Intersection group
		vk::RayTracingShaderGroupCreateInfoNV rayCHitGroupInfo; //VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_NV
		rayCHitGroupInfo.type = vk::RayTracingShaderGroupTypeNV::eTrianglesHitGroup;
		rayCHitGroupInfo.generalShader = VK_SHADER_UNUSED_NV;
		rayCHitGroupInfo.closestHitShader = 2;
		rayCHitGroupInfo.anyHitShader = VK_SHADER_UNUSED_NV;
		rayCHitGroupInfo.intersectionShader = VK_SHADER_UNUSED_NV;
		shaderGroups.push_back(rayCHitGroupInfo);

		vk::RayTracingPipelineCreateInfoNV rayPipelineInfo;
		rayPipelineInfo.stageCount = (uint32_t) shaderStages.size();
		rayPipelineInfo.pStages = shaderStages.data();
		rayPipelineInfo.groupCount = (uint32_t) shaderGroups.size();
		rayPipelineInfo.pGroups = shaderGroups.data();
		rayPipelineInfo.maxRecursionDepth = 3;
		rayPipelineInfo.layout = rttest[renderpass].pipelineLayout;
		rayPipelineInfo.basePipelineHandle = vk::Pipeline();
		rayPipelineInfo.basePipelineIndex = 0;

		rttest[renderpass].pipeline = device.createRayTracingPipelinesNV(vk::PipelineCache(), 
			{rayPipelineInfo}, nullptr, dldi)[0];

		// device.destroyShaderModule(raygenShaderModule);
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

	
	const uint32_t groupNum = 3; // 1 group is listed in pGroupNumbers in VkRayTracingPipelineCreateInfoNV
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

	Material::CreateDescriptorSetLayouts();
	Material::CreateDescriptorPools();
	Material::CreateVertexInputBindingDescriptions();
	Material::CreateVertexAttributeDescriptions();
	Material::CreateSSBO();
	Material::UpdateRasterDescriptorSets();
	
	Initialized = true;
}

bool Material::IsInitialized()
{
	return Initialized;
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
	eboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
	eboLayoutBinding.pImmutableSamplers = nullptr; // Optional

	// Transform SSBO
	vk::DescriptorSetLayoutBinding tboLayoutBinding;
	tboLayoutBinding.binding = 1;
	tboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
	tboLayoutBinding.descriptorCount = 1;
	tboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
	tboLayoutBinding.pImmutableSamplers = nullptr; // Optional

	// Camera SSBO
	vk::DescriptorSetLayoutBinding cboLayoutBinding;
	cboLayoutBinding.binding = 2;
	cboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
	cboLayoutBinding.descriptorCount = 1;
	cboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
	cboLayoutBinding.pImmutableSamplers = nullptr; // Optional

	// Material SSBO
	vk::DescriptorSetLayoutBinding mboLayoutBinding;
	mboLayoutBinding.binding = 3;
	mboLayoutBinding.descriptorCount = 1;
	mboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
	mboLayoutBinding.pImmutableSamplers = nullptr;
	mboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;

	// Light SSBO
	vk::DescriptorSetLayoutBinding lboLayoutBinding;
	lboLayoutBinding.binding = 4;
	lboLayoutBinding.descriptorCount = 1;
	lboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
	lboLayoutBinding.pImmutableSamplers = nullptr;
	lboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;

	// Mesh SSBO
	vk::DescriptorSetLayoutBinding meshssboLayoutBinding;
	meshssboLayoutBinding.binding = 5;
	meshssboLayoutBinding.descriptorCount = 1;
	meshssboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
	meshssboLayoutBinding.pImmutableSamplers = nullptr;
	meshssboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;

	std::array<vk::DescriptorSetLayoutBinding, 6> SSBObindings = { eboLayoutBinding, tboLayoutBinding, cboLayoutBinding, mboLayoutBinding, lboLayoutBinding, meshssboLayoutBinding};
	vk::DescriptorSetLayoutCreateInfo SSBOLayoutInfo;
	SSBOLayoutInfo.bindingCount = (uint32_t)SSBObindings.size();
	SSBOLayoutInfo.pBindings = SSBObindings.data();
	
	/* Texture descriptor bindings */
	
	// Texture struct
	vk::DescriptorSetLayoutBinding txboLayoutBinding;
	txboLayoutBinding.descriptorCount = 1;
	txboLayoutBinding.binding = 0;
	txboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
	txboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
	txboLayoutBinding.pImmutableSamplers = 0;

	// Texture samplers
	vk::DescriptorSetLayoutBinding samplerBinding;
	samplerBinding.descriptorCount = MAX_SAMPLERS;
	samplerBinding.binding = 1;
	samplerBinding.descriptorType = vk::DescriptorType::eSampler;
	samplerBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
	samplerBinding.pImmutableSamplers = 0;

	// 2D Textures
	vk::DescriptorSetLayoutBinding texture2DsBinding;
	texture2DsBinding.descriptorCount = MAX_TEXTURES;
	texture2DsBinding.binding = 2;
	texture2DsBinding.descriptorType = vk::DescriptorType::eSampledImage;
	texture2DsBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
	texture2DsBinding.pImmutableSamplers = 0;

	// Texture Cubes
	vk::DescriptorSetLayoutBinding textureCubesBinding;
	textureCubesBinding.descriptorCount = MAX_TEXTURES;
	textureCubesBinding.binding = 3;
	textureCubesBinding.descriptorType = vk::DescriptorType::eSampledImage;
	textureCubesBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
	textureCubesBinding.pImmutableSamplers = 0;

	// 3D Textures
	vk::DescriptorSetLayoutBinding texture3DsBinding;
	texture3DsBinding.descriptorCount = MAX_TEXTURES;
	texture3DsBinding.binding = 4;
	texture3DsBinding.descriptorType = vk::DescriptorType::eSampledImage;
	texture3DsBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
	texture3DsBinding.pImmutableSamplers = 0;

	std::array<vk::DescriptorSetLayoutBinding, 5> textureBindings = {txboLayoutBinding, samplerBinding, texture2DsBinding, textureCubesBinding, texture3DsBinding };
	vk::DescriptorSetLayoutCreateInfo textureLayoutInfo;
	textureLayoutInfo.bindingCount = (uint32_t)textureBindings.size();
	textureLayoutInfo.pBindings = textureBindings.data();

	// Create the layouts
	componentDescriptorSetLayout = device.createDescriptorSetLayout(SSBOLayoutInfo);
	textureDescriptorSetLayout = device.createDescriptorSetLayout(textureLayoutInfo);

	/* Vertex descriptors (mainly for ray tracing access) */
	vk::DescriptorBindingFlagsEXT bindingFlag = vk::DescriptorBindingFlagBitsEXT::eVariableDescriptorCount;
	vk::DescriptorSetLayoutBindingFlagsCreateInfoEXT bindingFlags;
    bindingFlags.pBindingFlags = &bindingFlag;
    bindingFlags.bindingCount = 1;

    vk::DescriptorSetLayoutBinding positionBinding;
    positionBinding.binding = 0;
    positionBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
    positionBinding.descriptorCount = MAX_MESHES;
    positionBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;

	vk::DescriptorSetLayoutBinding normalBinding;
    normalBinding.binding = 0;
    normalBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
    normalBinding.descriptorCount = MAX_MESHES;
    normalBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;

	vk::DescriptorSetLayoutBinding colorBinding;
    colorBinding.binding = 0;
    colorBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
    colorBinding.descriptorCount = MAX_MESHES;
    colorBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;

	vk::DescriptorSetLayoutBinding texcoordBinding;
    texcoordBinding.binding = 0;
    texcoordBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
    texcoordBinding.descriptorCount = MAX_MESHES;
    texcoordBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;

	vk::DescriptorSetLayoutBinding indexBinding;
    indexBinding.binding = 0;
    indexBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
    indexBinding.descriptorCount = MAX_MESHES;
    indexBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
	
	// std::array<vk::DescriptorSetLayoutBinding, 5> vertexBindings = {positionBinding, normalBinding, colorBinding, texcoordBinding, indexBinding };

	vk::DescriptorSetLayoutCreateInfo positionLayoutInfo;
	positionLayoutInfo.bindingCount = 1;
	positionLayoutInfo.pBindings = &positionBinding;
    positionLayoutInfo.pNext = &bindingFlags;
	positionsDescriptorSetLayout = device.createDescriptorSetLayout(positionLayoutInfo);

	vk::DescriptorSetLayoutCreateInfo normalLayoutInfo;
	normalLayoutInfo.bindingCount = 1;
	normalLayoutInfo.pBindings = &normalBinding;
    normalLayoutInfo.pNext = &bindingFlags;
	normalsDescriptorSetLayout = device.createDescriptorSetLayout(normalLayoutInfo);

	vk::DescriptorSetLayoutCreateInfo colorLayoutInfo;
	colorLayoutInfo.bindingCount = 1;
	colorLayoutInfo.pBindings = &colorBinding;
    colorLayoutInfo.pNext = &bindingFlags;
	colorsDescriptorSetLayout = device.createDescriptorSetLayout(colorLayoutInfo);

	vk::DescriptorSetLayoutCreateInfo texcoordLayoutInfo;
	texcoordLayoutInfo.bindingCount = 1;
	texcoordLayoutInfo.pBindings = &texcoordBinding;
    texcoordLayoutInfo.pNext = &bindingFlags;
	texcoordsDescriptorSetLayout = device.createDescriptorSetLayout(texcoordLayoutInfo);

	vk::DescriptorSetLayoutCreateInfo indexLayoutInfo;
	indexLayoutInfo.bindingCount = 1;
	indexLayoutInfo.pBindings = &indexBinding;
    indexLayoutInfo.pNext = &bindingFlags;
	indexDescriptorSetLayout = device.createDescriptorSetLayout(indexLayoutInfo);

	if (vulkan->is_ray_tracing_enabled()) {
		vk::DescriptorSetLayoutBinding accelerationStructureLayoutBinding;
		accelerationStructureLayoutBinding.binding = 0;
		accelerationStructureLayoutBinding.descriptorType = vk::DescriptorType::eAccelerationStructureNV;
		accelerationStructureLayoutBinding.descriptorCount = 1;
		accelerationStructureLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
		accelerationStructureLayoutBinding.pImmutableSamplers = nullptr;

		vk::DescriptorSetLayoutBinding outputImageLayoutBinding;
		outputImageLayoutBinding.binding = 1;
		outputImageLayoutBinding.descriptorType = vk::DescriptorType::eStorageImage;
		outputImageLayoutBinding.descriptorCount = 1;
		outputImageLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
		outputImageLayoutBinding.pImmutableSamplers = nullptr;

		vk::DescriptorSetLayoutBinding positionImageLayoutBinding;
		positionImageLayoutBinding.binding = 2;
		positionImageLayoutBinding.descriptorType = vk::DescriptorType::eStorageImage;
		positionImageLayoutBinding.descriptorCount = 1;
		positionImageLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
		positionImageLayoutBinding.pImmutableSamplers = nullptr;

		vk::DescriptorSetLayoutBinding normalImageLayoutBinding;
		normalImageLayoutBinding.binding = 3;
		normalImageLayoutBinding.descriptorType = vk::DescriptorType::eStorageImage;
		normalImageLayoutBinding.descriptorCount = 1;
		normalImageLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
		normalImageLayoutBinding.pImmutableSamplers = nullptr;

		std::vector<vk::DescriptorSetLayoutBinding> bindings({ accelerationStructureLayoutBinding, 
			outputImageLayoutBinding, positionImageLayoutBinding, normalImageLayoutBinding});

		vk::DescriptorSetLayoutCreateInfo layoutInfo;
		layoutInfo.bindingCount = (uint32_t)(bindings.size());
		layoutInfo.pBindings = bindings.data();

		raytracingDescriptorSetLayout = device.createDescriptorSetLayout(layoutInfo);
	}
}

void Material::CreateDescriptorPools()
{
	/* Since the descriptor layout is consistent across shaders, the descriptor
		pool can be shared. */

	auto vulkan = Libraries::Vulkan::Get();
	auto device = vulkan->get_device();

	/* SSBO Descriptor Pool Info */
	std::array<vk::DescriptorPoolSize, 6> SSBOPoolSizes = {};
	
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

	// Mesh SSBO
	SSBOPoolSizes[5].type = vk::DescriptorType::eStorageBuffer;
	SSBOPoolSizes[5].descriptorCount = MAX_MESHES;

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

	/* Vertex Descriptor Pool Info */

	// PositionSSBO
	vk::DescriptorPoolSize positionsPoolSize;
	positionsPoolSize.type = vk::DescriptorType::eStorageBuffer;
	positionsPoolSize.descriptorCount = MAX_MESHES;

	// NormalSSBO
	vk::DescriptorPoolSize normalsPoolSize;
	normalsPoolSize.type = vk::DescriptorType::eStorageBuffer;
	normalsPoolSize.descriptorCount = MAX_MESHES;
	
	// ColorSSBO
	vk::DescriptorPoolSize colorsPoolSize;
	colorsPoolSize.type = vk::DescriptorType::eStorageBuffer;
	colorsPoolSize.descriptorCount = MAX_MESHES;

	// TexCoordSSBO
	vk::DescriptorPoolSize texcoordsPoolSize;
	texcoordsPoolSize.type = vk::DescriptorType::eStorageBuffer;
	texcoordsPoolSize.descriptorCount = MAX_MESHES;

	// IndexSSBO
	vk::DescriptorPoolSize indexPoolSize;
	indexPoolSize.type = vk::DescriptorType::eStorageBuffer;
	indexPoolSize.descriptorCount = MAX_MESHES;

	vk::DescriptorPoolCreateInfo positionsPoolInfo;
	positionsPoolInfo.poolSizeCount = 1;
	positionsPoolInfo.pPoolSizes = &positionsPoolSize;
	positionsPoolInfo.maxSets = MAX_MESHES;
	positionsPoolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

	vk::DescriptorPoolCreateInfo normalsPoolInfo;
	normalsPoolInfo.poolSizeCount = 1;
	normalsPoolInfo.pPoolSizes = &normalsPoolSize;
	normalsPoolInfo.maxSets = MAX_MESHES;
	normalsPoolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
	
	vk::DescriptorPoolCreateInfo colorsPoolInfo;
	colorsPoolInfo.poolSizeCount = 1;
	colorsPoolInfo.pPoolSizes = &colorsPoolSize;
	colorsPoolInfo.maxSets = MAX_MESHES;
	colorsPoolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

	vk::DescriptorPoolCreateInfo texcoordsPoolInfo;
	texcoordsPoolInfo.poolSizeCount = 1;
	texcoordsPoolInfo.pPoolSizes = &texcoordsPoolSize;
	texcoordsPoolInfo.maxSets = MAX_MESHES;
	texcoordsPoolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

	vk::DescriptorPoolCreateInfo indexPoolInfo;
	indexPoolInfo.poolSizeCount = 1;
	indexPoolInfo.pPoolSizes = &indexPoolSize;
	indexPoolInfo.maxSets = MAX_MESHES;
	indexPoolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
	
	/* Raytrace Descriptor Pool Info */
	std::array<vk::DescriptorPoolSize, 4> raytracingPoolSizes = {};
	
	// Acceleration Structure
	raytracingPoolSizes[0].type = vk::DescriptorType::eAccelerationStructureNV;
	raytracingPoolSizes[0].descriptorCount = 1;

	// Textures
	raytracingPoolSizes[1].type = vk::DescriptorType::eStorageImage;
	raytracingPoolSizes[1].descriptorCount = 1;

	raytracingPoolSizes[2].type = vk::DescriptorType::eStorageImage;
	raytracingPoolSizes[2].descriptorCount = 1;

	raytracingPoolSizes[3].type = vk::DescriptorType::eStorageImage;
	raytracingPoolSizes[3].descriptorCount = 1;
	
	
	vk::DescriptorPoolCreateInfo raytracingPoolInfo;
	raytracingPoolInfo.poolSizeCount = (uint32_t)raytracingPoolSizes.size();
	raytracingPoolInfo.pPoolSizes = raytracingPoolSizes.data();
	raytracingPoolInfo.maxSets = 1;
	raytracingPoolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

	// Create the pools
	componentDescriptorPool = device.createDescriptorPool(SSBOPoolInfo);
	textureDescriptorPool = device.createDescriptorPool(texturePoolInfo);
	positionsDescriptorPool = device.createDescriptorPool(positionsPoolInfo);
	normalsDescriptorPool = device.createDescriptorPool(normalsPoolInfo);
	colorsDescriptorPool = device.createDescriptorPool(colorsPoolInfo);
	texcoordsDescriptorPool = device.createDescriptorPool(texcoordsPoolInfo);
	indexDescriptorPool = device.createDescriptorPool(indexPoolInfo);

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
	std::array<vk::WriteDescriptorSet, 6> SSBODescriptorWrites = {};
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

	// Mesh SSBO
	vk::DescriptorBufferInfo meshBufferInfo;
	meshBufferInfo.buffer = Mesh::GetSSBO();
	meshBufferInfo.offset = 0;
	meshBufferInfo.range = Mesh::GetSSBOSize();

	SSBODescriptorWrites[5].dstSet = componentDescriptorSet;
	SSBODescriptorWrites[5].dstBinding = 5;
	SSBODescriptorWrites[5].dstArrayElement = 0;
	SSBODescriptorWrites[5].descriptorType = vk::DescriptorType::eStorageBuffer;
	SSBODescriptorWrites[5].descriptorCount = 1;
	SSBODescriptorWrites[5].pBufferInfo = &meshBufferInfo;
	
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

	/* ------ Vertex Descriptor Set  ------ */
	vk::DescriptorSetLayout positionsLayouts[] = { positionsDescriptorSetLayout };
	vk::DescriptorSetLayout normalsLayouts[] = { normalsDescriptorSetLayout };
	vk::DescriptorSetLayout colorsLayouts[] = { colorsDescriptorSetLayout };
	vk::DescriptorSetLayout texcoordsLayouts[] = { texcoordsDescriptorSetLayout };
	vk::DescriptorSetLayout indexLayouts[] = { indexDescriptorSetLayout };
	std::array<vk::WriteDescriptorSet, 1> positionsDescriptorWrites = {};
	std::array<vk::WriteDescriptorSet, 1> normalsDescriptorWrites = {};
	std::array<vk::WriteDescriptorSet, 1> colorsDescriptorWrites = {};
	std::array<vk::WriteDescriptorSet, 1> texcoordsDescriptorWrites = {};
	std::array<vk::WriteDescriptorSet, 1> indexDescriptorWrites = {};
	
	if (positionsDescriptorSet == vk::DescriptorSet())
	{
		vk::DescriptorSetAllocateInfo allocInfo;
		allocInfo.descriptorPool = positionsDescriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = positionsLayouts;

		positionsDescriptorSet = device.allocateDescriptorSets(allocInfo)[0];
	}

	if (normalsDescriptorSet == vk::DescriptorSet())
	{
		vk::DescriptorSetAllocateInfo allocInfo;
		allocInfo.descriptorPool = normalsDescriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = normalsLayouts;

		normalsDescriptorSet = device.allocateDescriptorSets(allocInfo)[0];
	}

	if (colorsDescriptorSet == vk::DescriptorSet())
	{
		vk::DescriptorSetAllocateInfo allocInfo;
		allocInfo.descriptorPool = colorsDescriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = colorsLayouts;

		colorsDescriptorSet = device.allocateDescriptorSets(allocInfo)[0];
	}

	if (texcoordsDescriptorSet == vk::DescriptorSet())
	{
		vk::DescriptorSetAllocateInfo allocInfo;
		allocInfo.descriptorPool = texcoordsDescriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = texcoordsLayouts;

		texcoordsDescriptorSet = device.allocateDescriptorSets(allocInfo)[0];
	}

	if (indexDescriptorSet == vk::DescriptorSet())
	{
		vk::DescriptorSetAllocateInfo allocInfo;
		allocInfo.descriptorPool = indexDescriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = indexLayouts;

		indexDescriptorSet = device.allocateDescriptorSets(allocInfo)[0];
	}

	auto positionSSBOs = Mesh::GetPositionSSBOs();
	auto positionSSBOSizes = Mesh::GetPositionSSBOSizes();
	auto normalSSBOs = Mesh::GetNormalSSBOs();
	auto normalSSBOSizes = Mesh::GetNormalSSBOSizes();
	auto colorSSBOs = Mesh::GetColorSSBOs();
	auto colorSSBOSizes = Mesh::GetColorSSBOSizes();
	auto texcoordSSBOs = Mesh::GetTexCoordSSBOs();
	auto texcoordSSBOSizes = Mesh::GetTexCoordSSBOSizes();
	auto indexSSBOs = Mesh::GetIndexSSBOs();
	auto indexSSBOSizes = Mesh::GetIndexSSBOSizes();
	
	// Positions SSBO
	vk::DescriptorBufferInfo positionBufferInfos[MAX_MESHES];
	for (int i = 0; i < MAX_MESHES; ++i) 
	{
		if (positionSSBOs[i] == vk::Buffer()) return;
		positionBufferInfos[i].buffer = positionSSBOs[i];
		positionBufferInfos[i].offset = 0;
		positionBufferInfos[i].range = positionSSBOSizes[i];
	}

	positionsDescriptorWrites[0].dstSet = positionsDescriptorSet;
	positionsDescriptorWrites[0].dstBinding = 0;
	positionsDescriptorWrites[0].dstArrayElement = 0;
	positionsDescriptorWrites[0].descriptorType = vk::DescriptorType::eStorageBuffer;
	positionsDescriptorWrites[0].descriptorCount = MAX_MESHES;
	positionsDescriptorWrites[0].pBufferInfo = positionBufferInfos;

	// Normals SSBO
	vk::DescriptorBufferInfo normalBufferInfos[MAX_MESHES];
	for (int i = 0; i < MAX_MESHES; ++i) 
	{
		if (normalSSBOs[i] == vk::Buffer()) return;
		normalBufferInfos[i].buffer = normalSSBOs[i];
		normalBufferInfos[i].offset = 0;
		normalBufferInfos[i].range = normalSSBOSizes[i];
	}

	normalsDescriptorWrites[0].dstSet = normalsDescriptorSet;
	normalsDescriptorWrites[0].dstBinding = 0;
	normalsDescriptorWrites[0].dstArrayElement = 0;
	normalsDescriptorWrites[0].descriptorType = vk::DescriptorType::eStorageBuffer;
	normalsDescriptorWrites[0].descriptorCount = MAX_MESHES;
	normalsDescriptorWrites[0].pBufferInfo = normalBufferInfos;

	// Colors SSBO
	vk::DescriptorBufferInfo colorBufferInfos[MAX_MESHES];
	for (int i = 0; i < MAX_MESHES; ++i) 
	{
		if (colorSSBOs[i] == vk::Buffer()) return;
		colorBufferInfos[i].buffer = colorSSBOs[i];
		colorBufferInfos[i].offset = 0;
		colorBufferInfos[i].range = colorSSBOSizes[i];
	}

	colorsDescriptorWrites[0].dstSet = colorsDescriptorSet;
	colorsDescriptorWrites[0].dstBinding = 0;
	colorsDescriptorWrites[0].dstArrayElement = 0;
	colorsDescriptorWrites[0].descriptorType = vk::DescriptorType::eStorageBuffer;
	colorsDescriptorWrites[0].descriptorCount = MAX_MESHES;
	colorsDescriptorWrites[0].pBufferInfo = colorBufferInfos;

	// TexCoords SSBO
	vk::DescriptorBufferInfo texcoordBufferInfos[MAX_MESHES];
	for (int i = 0; i < MAX_MESHES; ++i) 
	{
		if (texcoordSSBOs[i] == vk::Buffer()) return;
		texcoordBufferInfos[i].buffer = texcoordSSBOs[i];
		texcoordBufferInfos[i].offset = 0;
		texcoordBufferInfos[i].range = texcoordSSBOSizes[i];
	}

	texcoordsDescriptorWrites[0].dstSet = texcoordsDescriptorSet;
	texcoordsDescriptorWrites[0].dstBinding = 0;
	texcoordsDescriptorWrites[0].dstArrayElement = 0;
	texcoordsDescriptorWrites[0].descriptorType = vk::DescriptorType::eStorageBuffer;
	texcoordsDescriptorWrites[0].descriptorCount = MAX_MESHES;
	texcoordsDescriptorWrites[0].pBufferInfo = texcoordBufferInfos;

	// Indices SSBO
	vk::DescriptorBufferInfo indexBufferInfos[MAX_MESHES];
	for (int i = 0; i < MAX_MESHES; ++i) 
	{
		if (indexSSBOs[i] == vk::Buffer()) return;
		indexBufferInfos[i].buffer = indexSSBOs[i];
		indexBufferInfos[i].offset = 0;
		indexBufferInfos[i].range = indexSSBOSizes[i];
	}

	indexDescriptorWrites[0].dstSet = indexDescriptorSet;
	indexDescriptorWrites[0].dstBinding = 0;
	indexDescriptorWrites[0].dstArrayElement = 0;
	indexDescriptorWrites[0].descriptorType = vk::DescriptorType::eStorageBuffer;
	indexDescriptorWrites[0].descriptorCount = MAX_MESHES;
	indexDescriptorWrites[0].pBufferInfo = indexBufferInfos;

	device.updateDescriptorSets((uint32_t)positionsDescriptorWrites.size(), positionsDescriptorWrites.data(), 0, nullptr);
	device.updateDescriptorSets((uint32_t)normalsDescriptorWrites.size(), normalsDescriptorWrites.data(), 0, nullptr);
	device.updateDescriptorSets((uint32_t)colorsDescriptorWrites.size(), colorsDescriptorWrites.data(), 0, nullptr);
	device.updateDescriptorSets((uint32_t)texcoordsDescriptorWrites.size(), texcoordsDescriptorWrites.data(), 0, nullptr);
	device.updateDescriptorSets((uint32_t)indexDescriptorWrites.size(), indexDescriptorWrites.data(), 0, nullptr);
}

void Material::UpdateRaytracingDescriptorSets(vk::AccelerationStructureNV &tlas, Entity &camera_entity)
{
	auto vulkan = Libraries::Vulkan::Get();
	auto device = vulkan->get_device();

	if (!vulkan->is_ray_tracing_enabled()) return;
	if (tlas == vk::AccelerationStructureNV()) return;
	if (!camera_entity.is_initialized()) return;
	if (!camera_entity.camera()) return;
	if (!camera_entity.camera()->get_texture()) return;
	
	vk::DescriptorSetLayout raytracingLayouts[] = { raytracingDescriptorSetLayout };

	if (raytracingDescriptorSet == vk::DescriptorSet())
	{
		vk::DescriptorSetAllocateInfo allocInfo;
		allocInfo.descriptorPool = raytracingDescriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = raytracingLayouts;

		raytracingDescriptorSet = device.allocateDescriptorSets(allocInfo)[0];
	}

	std::array<vk::WriteDescriptorSet, 4> raytraceDescriptorWrites = {};

	vk::WriteDescriptorSetAccelerationStructureNV descriptorAccelerationStructureInfo;
	descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
	descriptorAccelerationStructureInfo.pAccelerationStructures = &tlas;

	raytraceDescriptorWrites[0].dstSet = raytracingDescriptorSet;
	raytraceDescriptorWrites[0].pNext = &descriptorAccelerationStructureInfo;
	raytraceDescriptorWrites[0].dstBinding = 0;
	raytraceDescriptorWrites[0].dstArrayElement = 0;
	raytraceDescriptorWrites[0].descriptorCount = 1;
	raytraceDescriptorWrites[0].descriptorType = vk::DescriptorType::eAccelerationStructureNV;

	// Output image 
	vk::DescriptorImageInfo descriptorOutputImageInfo;
	descriptorOutputImageInfo.imageLayout = vk::ImageLayout::eGeneral;
	descriptorOutputImageInfo.imageView = camera_entity.camera()->get_texture()->get_color_image_view();		

	raytraceDescriptorWrites[1].dstSet = raytracingDescriptorSet;
	raytraceDescriptorWrites[1].pNext = &descriptorAccelerationStructureInfo;
	raytraceDescriptorWrites[1].dstBinding = 1;
	raytraceDescriptorWrites[1].dstArrayElement = 0;
	raytraceDescriptorWrites[1].descriptorCount = 1;
	raytraceDescriptorWrites[1].descriptorType = vk::DescriptorType::eStorageImage;
	raytraceDescriptorWrites[1].pImageInfo = &descriptorOutputImageInfo;

	// Position G Buffer
	vk::DescriptorImageInfo descriptorPositionImageInfo;
	descriptorPositionImageInfo.imageLayout = vk::ImageLayout::eGeneral;
	descriptorPositionImageInfo.imageView = camera_entity.camera()->get_texture()->get_position_image_view();		

	raytraceDescriptorWrites[2].dstSet = raytracingDescriptorSet;
	raytraceDescriptorWrites[2].pNext = &descriptorAccelerationStructureInfo;
	raytraceDescriptorWrites[2].dstBinding = 2;
	raytraceDescriptorWrites[2].dstArrayElement = 0;
	raytraceDescriptorWrites[2].descriptorCount = 1;
	raytraceDescriptorWrites[2].descriptorType = vk::DescriptorType::eStorageImage;
	raytraceDescriptorWrites[2].pImageInfo = &descriptorPositionImageInfo;

	// Normal G Buffer
	vk::DescriptorImageInfo descriptorNormalImageInfo;
	descriptorNormalImageInfo.imageLayout = vk::ImageLayout::eGeneral;
	descriptorNormalImageInfo.imageView = camera_entity.camera()->get_texture()->get_normal_image_view();		

	raytraceDescriptorWrites[3].dstSet = raytracingDescriptorSet;
	raytraceDescriptorWrites[3].pNext = &descriptorAccelerationStructureInfo;
	raytraceDescriptorWrites[3].dstBinding = 3;
	raytraceDescriptorWrites[3].dstArrayElement = 0;
	raytraceDescriptorWrites[3].descriptorCount = 1;
	raytraceDescriptorWrites[3].descriptorType = vk::DescriptorType::eStorageImage;
	raytraceDescriptorWrites[3].pImageInfo = &descriptorNormalImageInfo;

	device.updateDescriptorSets((uint32_t)raytraceDescriptorWrites.size(), raytraceDescriptorWrites.data(), 0, nullptr);
}

void Material::CreateVertexInputBindingDescriptions() {
	/* Vertex input bindings are consistent across shaders */
	vk::VertexInputBindingDescription pointBindingDescription;
	pointBindingDescription.binding = 0;
	pointBindingDescription.stride = 4 * sizeof(float);
	pointBindingDescription.inputRate = vk::VertexInputRate::eVertex;

	vk::VertexInputBindingDescription colorBindingDescription;
	colorBindingDescription.binding = 1;
	colorBindingDescription.stride = 4 * sizeof(float);
	colorBindingDescription.inputRate = vk::VertexInputRate::eVertex;

	vk::VertexInputBindingDescription normalBindingDescription;
	normalBindingDescription.binding = 2;
	normalBindingDescription.stride = 4 * sizeof(float);
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
	std::vector<vk::DescriptorSet> descriptorSets = {componentDescriptorSet, textureDescriptorSet, positionsDescriptorSet, normalsDescriptorSet, colorsDescriptorSet, texcoordsDescriptorSet, indexDescriptorSet};
	// command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, normalsurface[render_pass].pipelineLayout, 0, descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	// command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, blinn[render_pass].pipelineLayout, 0, descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	// command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, texcoordsurface[render_pass].pipelineLayout, 0, descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pbr[render_pass].pipelineLayout, 0, descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, skybox[render_pass].pipelineLayout, 0, descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	// command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, depth[render_pass].pipelineLayout, 0, descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, volume[render_pass].pipelineLayout, 0, descriptorSets.size(), descriptorSets.data(), 0, nullptr);
}

void Material::BindRayTracingDescriptorSets(vk::CommandBuffer &command_buffer, vk::RenderPass &render_pass)
{
	std::vector<vk::DescriptorSet> descriptorSets = {componentDescriptorSet, textureDescriptorSet,  positionsDescriptorSet, normalsDescriptorSet, colorsDescriptorSet, texcoordsDescriptorSet, indexDescriptorSet, raytracingDescriptorSet};
	command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingNV, rttest[render_pass].pipelineLayout, 0, descriptorSets.size(), descriptorSets.data(), 0, nullptr);
}

void Material::DrawEntity(
	vk::CommandBuffer &command_buffer, 
	vk::RenderPass &render_pass, 
	Entity &entity, 
	PushConsts &push_constants, 
	RenderMode rendermode_override, 
	PipelineType pipeline_type_override, 
	bool render_bounding_box_override)
{
	auto pipeline_type = pipeline_type_override;

	/* Need a mesh to render. */
	auto m = entity.get_mesh();
	if (!m) return;

	bool show_bounding_box = m->should_show_bounding_box() | render_bounding_box_override;
	push_constants.show_bounding_box = show_bounding_box;

	/* Need a transform to render. */
	auto transform = entity.get_transform();
	if (!transform) return;

	if (show_bounding_box) {
	    m = Mesh::Get("BoundingBox");
	}

	/* Need a material to render. */
	auto material = entity.get_material();
	if (!material) return;

	auto rendermode = (rendermode_override == RenderMode::RENDER_MODE_NONE) ? material->renderMode : rendermode_override;

	if (rendermode == RENDER_MODE_HIDDEN) return;

	// if (rendermode == RENDER_MODE_NORMAL) {
	// 	command_buffer.pushConstants(normalsurface[render_pass].pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
		
	// 	if ((currentlyBoundRenderMode != RENDER_MODE_NORMAL) || (currentRenderpass != render_pass) || (currentlyBoundPipelineType != pipeline_type)) {
	// 		command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, normalsurface[render_pass].pipelines[pipeline_type]);
	// 		currentlyBoundRenderMode = RENDER_MODE_NORMAL;
	// 		currentRenderpass = render_pass;
	// 	}
	// }
	// else if (rendermode == RENDER_MODE_BLINN) {
	// 	command_buffer.pushConstants(blinn[render_pass].pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
	// 	if ((currentlyBoundRenderMode != RENDER_MODE_BLINN) || (currentRenderpass != render_pass) || (currentlyBoundPipelineType != pipeline_type)) {
	// 		command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, blinn[render_pass].pipelines[pipeline_type]);
	// 		currentlyBoundRenderMode = RENDER_MODE_BLINN;
	// 		currentRenderpass = render_pass;
	// 	}
	// }
	// else if (rendermode == RENDER_MODE_TEXCOORD) {
	// 	command_buffer.pushConstants(texcoordsurface[render_pass].pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
	// 	if ((currentlyBoundRenderMode != RENDER_MODE_TEXCOORD) || (currentRenderpass != render_pass) || (currentlyBoundPipelineType != pipeline_type)) {
	// 		command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, texcoordsurface[render_pass].pipelines[pipeline_type]);
	// 		currentlyBoundRenderMode = RENDER_MODE_TEXCOORD;
	// 		currentRenderpass = render_pass;
	// 	}
	// }
	/* else */ if (rendermode == RENDER_MODE_PBR) {
		command_buffer.pushConstants(pbr[render_pass].pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
		if ((currentlyBoundRenderMode != RENDER_MODE_PBR) || (currentRenderpass != render_pass) || (currentlyBoundPipelineType != pipeline_type)) {
			command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pbr[render_pass].pipelines[pipeline_type]);
			currentlyBoundRenderMode = RENDER_MODE_PBR;
			currentRenderpass = render_pass;
		}
	}
	// else if (rendermode == RENDER_MODE_DEPTH) {
	// 	command_buffer.pushConstants(depth[render_pass].pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
	// 	if ((currentlyBoundRenderMode != RENDER_MODE_DEPTH) || (currentRenderpass != render_pass) || (currentlyBoundPipelineType != pipeline_type)) {
	// 		command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, depth[render_pass].pipelines[pipeline_type]);
	// 		currentlyBoundRenderMode = RENDER_MODE_DEPTH;
	// 		currentRenderpass = render_pass;
	// 	}
	// }
	else if (rendermode == RENDER_MODE_SKYBOX) {
		command_buffer.pushConstants(skybox[render_pass].pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
		if ((currentlyBoundRenderMode != RENDER_MODE_SKYBOX) || (currentRenderpass != render_pass) || (currentlyBoundPipelineType != pipeline_type)) {
			command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, skybox[render_pass].pipelines[pipeline_type]);
			currentlyBoundRenderMode = RENDER_MODE_SKYBOX;
			currentRenderpass = render_pass;
		}
	}
	else if (rendermode == RENDER_MODE_FRAGMENTDEPTH) {
		command_buffer.pushConstants(fragmentdepth[render_pass].pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
		if ((currentlyBoundRenderMode != RENDER_MODE_FRAGMENTDEPTH) || (currentRenderpass != render_pass) || (currentlyBoundPipelineType != pipeline_type)) {
			command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, fragmentdepth[render_pass].pipelines[pipeline_type]);
			currentlyBoundRenderMode = RENDER_MODE_FRAGMENTDEPTH;
			currentRenderpass = render_pass;
		}
	}
	// else if (rendermode == RENDER_MODE_FRAGMENTPOSITION) {
	// 	command_buffer.pushConstants(fragmentposition[render_pass].pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
	// 	if ((currentlyBoundRenderMode != RENDER_MODE_FRAGMENTPOSITION) || (currentRenderpass != render_pass) || (currentlyBoundPipelineType != pipeline_type)) {
	// 		command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, fragmentposition[render_pass].pipelines[pipeline_type]);
	// 		currentlyBoundRenderMode = RENDER_MODE_FRAGMENTPOSITION;
	// 		currentRenderpass = render_pass;
	// 	}
	// }
	else if (rendermode == RENDER_MODE_SHADOWMAP) {
		command_buffer.pushConstants(shadowmap[render_pass].pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
		if ((currentlyBoundRenderMode != RENDER_MODE_SHADOWMAP) || (currentRenderpass != render_pass) || (currentlyBoundPipelineType != pipeline_type)) {
			command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, shadowmap[render_pass].pipelines[pipeline_type]);
			currentlyBoundRenderMode = RENDER_MODE_SHADOWMAP;
			currentRenderpass = render_pass;
		}
	}
	else if (rendermode == RENDER_MODE_VRMASK) {
		command_buffer.pushConstants(vrmask[render_pass].pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
		if ((currentlyBoundRenderMode != RENDER_MODE_VRMASK) || (currentRenderpass != render_pass) || (currentlyBoundPipelineType != pipeline_type)) {
			command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, vrmask[render_pass].pipelines[pipeline_type]);
			currentlyBoundRenderMode = RENDER_MODE_VRMASK;
			currentRenderpass = render_pass;
		}
	}
	else if (rendermode == RENDER_MODE_VOLUME)
	{
		command_buffer.pushConstants(volume[render_pass].pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
		if ((currentlyBoundRenderMode != RENDER_MODE_VOLUME) || (currentRenderpass != render_pass) || (currentlyBoundPipelineType != pipeline_type)) {
			command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, volume[render_pass].pipelines[pipeline_type]);
			currentlyBoundRenderMode = RENDER_MODE_VOLUME;
			currentRenderpass = render_pass;
		}
	}
	else {
		command_buffer.pushConstants(pbr[render_pass].pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
		if ((currentlyBoundRenderMode != RENDER_MODE_PBR) || (currentRenderpass != render_pass) || (currentlyBoundPipelineType != pipeline_type)) {
			command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pbr[render_pass].pipelines[pipeline_type]);
			currentlyBoundRenderMode = RENDER_MODE_PBR;
			currentRenderpass = render_pass;
		}
	}

	
	currentlyBoundPipelineType = pipeline_type;
	
	command_buffer.bindVertexBuffers(0, {m->get_point_buffer(), m->get_color_buffer(), m->get_normal_buffer(), m->get_texcoord_buffer()}, {0,0,0,0});
	command_buffer.bindIndexBuffer(m->get_triangle_index_buffer(), 0, vk::IndexType::eUint32);
	command_buffer.drawIndexed(m->get_total_triangle_indices(), 1, 0, 0, 0);
}

void Material::TraceRays(
	vk::CommandBuffer &command_buffer, 
	vk::RenderPass &render_pass, 
	PushConsts &push_constants,
	Texture &texture)
{
	auto vulkan = Libraries::Vulkan::Get();
    auto device = vulkan->get_device();
    auto dldi = vulkan->get_dldi();

	auto rayTracingProps = vulkan->get_physical_device_ray_tracing_properties();

	command_buffer.pushConstants(rttest[render_pass].pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
	command_buffer.bindPipeline(vk::PipelineBindPoint::eRayTracingNV, rttest[render_pass].pipeline);

	/* Need to make the camera's associated texture writable. Perhaps write 
        to a separate "ray tracing" texture?
        Depth buffer access would be handy as well...
    */

   // Here's how the shader binding table looks like in this tutorial:
    // |[ raygen shader ]|
    // |                 |
    // | 0               | 1
    command_buffer.traceRaysNV(
    //     raygenShaderBindingTableBuffer, offset
		rttest[render_pass].shaderBindingTable, 0,
    //     missShaderBindingTableBuffer, offset, stride
		rttest[render_pass].shaderBindingTable, 1 * rayTracingProps.shaderGroupHandleSize, rayTracingProps.shaderGroupHandleSize,
    //     hitShaderBindingTableBuffer, offset, stride
		rttest[render_pass].shaderBindingTable, 2 * rayTracingProps.shaderGroupHandleSize, rayTracingProps.shaderGroupHandleSize,
    //     callableShaderBindingTableBuffer, offset, stride
		vk::Buffer(), 0, 0,
        // width, height, depth, 
		texture.get_width(), texture.get_height(), 1,
        dldi
    );
	
    // vkCmdTraceRaysNVX(commandBuffer,
    //     _shaderBindingTable.Buffer, 0,
    //     _shaderBindingTable.Buffer, 0, _raytracingProperties.shaderHeaderSize,
    //     _shaderBindingTable.Buffer, 0, _raytracingProperties.shaderHeaderSize,
    //     _actualWindowWidth, _actualWindowHeight);
		
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
	currentlyBoundRenderMode = RENDER_MODE_NONE;
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

	device.destroyDescriptorSetLayout(positionsDescriptorSetLayout);
	device.destroyDescriptorPool(positionsDescriptorPool);

	device.destroyDescriptorSetLayout(normalsDescriptorSetLayout);
	device.destroyDescriptorPool(normalsDescriptorPool);

	device.destroyDescriptorSetLayout(colorsDescriptorSetLayout);
	device.destroyDescriptorPool(colorsDescriptorPool);

	device.destroyDescriptorSetLayout(texcoordsDescriptorSetLayout);
	device.destroyDescriptorPool(texcoordsDescriptorPool);

	device.destroyDescriptorSetLayout(indexDescriptorSetLayout);
	device.destroyDescriptorPool(indexDescriptorPool);

	SSBO = vk::Buffer();
	SSBOMemory = vk::DeviceMemory();
	stagingSSBO = vk::Buffer();
	stagingSSBOMemory = vk::DeviceMemory();

	componentDescriptorSetLayout = vk::DescriptorSetLayout();
	componentDescriptorPool = vk::DescriptorPool();
	textureDescriptorSetLayout = vk::DescriptorSetLayout();
	textureDescriptorPool = vk::DescriptorPool();
	positionsDescriptorSetLayout = vk::DescriptorSetLayout();
	positionsDescriptorPool = vk::DescriptorPool();
	normalsDescriptorSetLayout = vk::DescriptorSetLayout();
	normalsDescriptorPool = vk::DescriptorPool();
	colorsDescriptorSetLayout = vk::DescriptorSetLayout();
	colorsDescriptorPool = vk::DescriptorPool();
	texcoordsDescriptorSetLayout = vk::DescriptorSetLayout();
	texcoordsDescriptorPool = vk::DescriptorPool();
	indexDescriptorSetLayout = vk::DescriptorSetLayout();
	indexDescriptorPool = vk::DescriptorPool();

	for (auto item : shaderModuleCache) {
		device.destroyShaderModule(item.second);
	}

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
	if (texture_id > MAX_TEXTURES)
		throw std::runtime_error( std::string("Invalid texture handle"));
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
	if (texture_id > MAX_TEXTURES)
		throw std::runtime_error( std::string("Invalid texture handle"));
	this->material_struct.roughness_texture_id = texture_id;
}

void Material::set_roughness_texture(Texture *texture) 
{
	if (!texture) 
		throw std::runtime_error( std::string("Invalid texture handle"));
	this->material_struct.roughness_texture_id = texture->get_id();
}

void Material::set_bump_texture(uint32_t texture_id)
{
	if (texture_id > MAX_TEXTURES)
		throw std::runtime_error( std::string("Invalid texture handle"));
	this->material_struct.bump_texture_id = texture_id;
}

void Material::set_bump_texture(Texture *texture)
{
	if (!texture)
		throw std::runtime_error( std::string("Invalid texture handle"));
	this->material_struct.bump_texture_id = texture->get_id();
}

void Material::clear_bump_texture()
{
	this->material_struct.bump_texture_id = -1;
}

void Material::set_alpha_texture(uint32_t texture_id)
{
	if (texture_id > MAX_TEXTURES)
		throw std::runtime_error( std::string("Invalid texture handle"));
	this->material_struct.alpha_texture_id = texture_id;
}

void Material::set_alpha_texture(Texture *texture)
{
	if (!texture)
		throw std::runtime_error( std::string("Invalid texture handle"));
	this->material_struct.alpha_texture_id = texture->get_id();
}

void Material::clear_alpha_texture()
{
	this->material_struct.alpha_texture_id = -1;
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
	renderMode = RENDER_MODE_PBR;
}

void Material::show_normals () {
	renderMode = RENDER_MODE_NORMAL;
}

void Material::show_base_color() {
	renderMode = RENDER_MODE_BASECOLOR;
}

void Material::show_texcoords() {
	renderMode = RENDER_MODE_TEXCOORD;
}

void Material::show_blinn() {
	renderMode = RENDER_MODE_BLINN;
}

void Material::show_depth() {
	renderMode = RENDER_MODE_DEPTH;
}

void Material::show_fragment_depth() {
	renderMode = RENDER_MODE_FRAGMENTDEPTH;
}

void Material::show_vr_mask() {
	renderMode = RENDER_MODE_VRMASK;
}

void Material::show_position() {
	renderMode = RENDER_MODE_FRAGMENTPOSITION;
}

void Material::show_volume() {
	renderMode = RENDER_MODE_VOLUME;
}

void Material::show_environment() {
	renderMode = RENDER_MODE_SKYBOX;
}

void Material::hide() {
	renderMode = RENDER_MODE_HIDDEN;
}

void Material::set_base_color(glm::vec4 color) {
	this->material_struct.base_color = color;
}

void Material::set_base_color(float r, float g, float b, float a) {
	this->material_struct.base_color = glm::vec4(r, g, b, a);
}

glm::vec4 Material::get_base_color() {
	return this->material_struct.base_color;
}

void Material::set_roughness(float roughness) {
	this->material_struct.roughness = roughness;
}

float Material::get_roughness() {
	return this->material_struct.roughness;
}

void Material::set_metallic(float metallic) {
	this->material_struct.metallic = metallic;
}

float Material::get_metallic() {
	return this->material_struct.metallic;
}

void Material::set_transmission(float transmission) {
	this->material_struct.transmission = transmission;
}

float Material::get_transmission() {
	return this->material_struct.transmission;
}

void Material::set_transmission_roughness(float transmission_roughness) {
	this->material_struct.transmission_roughness = transmission_roughness;
}

float Material::get_transmission_roughness() {
	return this->material_struct.transmission_roughness;
}

void Material::set_ior(float ior) {
	this->material_struct.ior = ior;
}

bool Material::contains_transparency() {
	/* We can expand this to other transparency cases if needed */
	if (this->material_struct.alpha_texture_id != -1) return true;
	if (this->material_struct.base_color.a < 1.0f) return true;
	if (this->renderMode == RENDER_MODE_VOLUME) return true;
	return false;
}