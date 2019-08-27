#pragma once
#include "Pluto/Libraries/Vulkan/Vulkan.hxx"

/* If this is more than 0, the corresponding g buffers must be written to by all
fragment shaders. */

// See Descriptors.hxx fragment output attachments 
#define USED_PRIMARY_VISIBILITY_G_BUFFERS 6
#define USED_SHADOW_MAP_G_BUFFERS 1

struct PipelineParameters {
	vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
	vk::PipelineRasterizationStateCreateInfo rasterizer;
	vk::PipelineViewportStateCreateInfo viewportState;
	vk::PipelineMultisampleStateCreateInfo multisampling;
	vk::PipelineDepthStencilStateCreateInfo depthStencil;
	std::vector<vk::PipelineColorBlendAttachmentState> blendAttachments;
	vk::PipelineColorBlendStateCreateInfo colorBlending;
	vk::PipelineDynamicStateCreateInfo dynamicState;

	vk::DynamicState dynamicStates[3] = {
		vk::DynamicState::eViewport,
		vk::DynamicState::eScissor,
		vk::DynamicState::eLineWidth
	};

	// static std::shared_ptr<PipelineParameters> Create(PipelineKey key) {
	// 	std::cout << "ComponentManager: Registering pipeline settings for renderpass " << key.renderpass
	// 		<< " subpass " << key.subpass << " pipeline index " << key.pipelineIdx << std::endl;
	// 	auto pp = std::make_shared<PipelineParameters>();
	// 	Systems::ComponentManager::PipelineSettings[key] = pp;
	// 	return pp;
	// }

	PipelineParameters() {}

	void initialize(uint32_t num_g_buffers)	
	{
		/* Default input assembly */
		inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
		inputAssembly.primitiveRestartEnable = false;

		/* Default rasterizer */
		rasterizer.depthClampEnable = false; // Helpful for shadow maps.
		rasterizer.rasterizerDiscardEnable = false; // if true, geometry never goes through rasterization, or to frame buffer
		rasterizer.polygonMode = vk::PolygonMode::eFill; // could be line or point too. those require GPU features to be enabled.
		rasterizer.lineWidth = 1.0f; // anything thicker than 1.0 requires wideLines GPU feature
		rasterizer.cullMode = vk::CullModeFlagBits::eNone;
		rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
		rasterizer.depthBiasEnable = false;
		rasterizer.depthBiasConstantFactor = 0.0f;
		rasterizer.depthBiasClamp = 0.0f;
		rasterizer.depthBiasSlopeFactor = 0.0f;

		/* Default Viewport and Scissors */
		// Note: This is actually overriden by the dynamic states (see below)
		viewportState.viewportCount = 1;
		viewportState.scissorCount = 1;
		viewportState.pViewports = nullptr;
		viewportState.pScissors = nullptr;

		/* Default Multisampling */
		multisampling.sampleShadingEnable = false;
		multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
		multisampling.minSampleShading = 1.0f; // Optional
		multisampling.pSampleMask = nullptr; // Optional
		multisampling.alphaToCoverageEnable = false; // Optional
		multisampling.alphaToOneEnable = false; // Optional

		/* Default Depth and Stencil Testing */
		depthStencil.depthTestEnable = true;
		depthStencil.depthWriteEnable = true;
		#ifndef DISABLE_REVERSE_Z
		depthStencil.depthCompareOp = vk::CompareOp::eGreater;
		#else
		depthStencil.depthCompareOp = vk::CompareOp::eLess;
		#endif
		depthStencil.depthBoundsTestEnable = false;
		depthStencil.minDepthBounds = 0.0f; // Optional
		depthStencil.maxDepthBounds = 1.0f; // Optional
		depthStencil.stencilTestEnable = false;
		depthStencil.front = vk::StencilOpState(); // Optional
		depthStencil.back = vk::StencilOpState(); // Optional

		/* Default Color Blending Attachment States */

		/* G Buffers */
		blendAttachments.resize(num_g_buffers);
		for (uint32_t g_idx = 0; g_idx < num_g_buffers; ++g_idx) {
			blendAttachments[g_idx].colorWriteMask = vk::ColorComponentFlagBits::eR | 
												vk::ColorComponentFlagBits::eG | 
												vk::ColorComponentFlagBits::eB | 
												vk::ColorComponentFlagBits::eA;
			blendAttachments[g_idx].blendEnable = true;
			blendAttachments[g_idx].srcColorBlendFactor = vk::BlendFactor::eSrcAlpha; // Optional /* TRANSPARENCY STUFF HERE! */
			blendAttachments[g_idx].dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha; // Optional
			blendAttachments[g_idx].colorBlendOp = vk::BlendOp::eAdd; // Optional
			blendAttachments[g_idx].srcAlphaBlendFactor = vk::BlendFactor::eOne; // Optional
			blendAttachments[g_idx].dstAlphaBlendFactor = vk::BlendFactor::eZero; // Optional
			blendAttachments[g_idx].alphaBlendOp = vk::BlendOp::eAdd; // Optional
		}
		
		/* Default Color Blending State */
		colorBlending.logicOpEnable = false;
		colorBlending.logicOp = vk::LogicOp::eCopy; // Optional
		colorBlending.attachmentCount = (uint32_t)blendAttachments.size();
		colorBlending.pAttachments = blendAttachments.data();
		colorBlending.blendConstants[0] = 0.0f; // Optional
		colorBlending.blendConstants[1] = 0.0f; // Optional
		colorBlending.blendConstants[2] = 0.0f; // Optional
		colorBlending.blendConstants[3] = 0.0f; // Optional

		/* Default Dynamic State */
		dynamicState.dynamicStateCount = 3;
		dynamicState.pDynamicStates = dynamicStates;
	}
};