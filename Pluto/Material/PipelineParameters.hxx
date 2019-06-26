#pragma once
#include "Pluto/Libraries/Vulkan/Vulkan.hxx"

struct PipelineParameters {
	vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
	vk::PipelineRasterizationStateCreateInfo rasterizer;
	vk::PipelineViewportStateCreateInfo viewportState;
	vk::PipelineMultisampleStateCreateInfo multisampling;
	vk::PipelineDepthStencilStateCreateInfo depthStencil;
	std::array<vk::PipelineColorBlendAttachmentState, 3> blendAttachments;
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

	PipelineParameters() {
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

		/* G Buffer 0 */
		blendAttachments[0].colorWriteMask = vk::ColorComponentFlagBits::eR | 
											vk::ColorComponentFlagBits::eG | 
											vk::ColorComponentFlagBits::eB | 
											vk::ColorComponentFlagBits::eA;
		blendAttachments[0].blendEnable = true;
		blendAttachments[0].srcColorBlendFactor = vk::BlendFactor::eSrcAlpha; // Optional /* TRANSPARENCY STUFF HERE! */
		blendAttachments[0].dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha; // Optional
		blendAttachments[0].colorBlendOp = vk::BlendOp::eAdd; // Optional
		blendAttachments[0].srcAlphaBlendFactor = vk::BlendFactor::eOne; // Optional
		blendAttachments[0].dstAlphaBlendFactor = vk::BlendFactor::eZero; // Optional
		blendAttachments[0].alphaBlendOp = vk::BlendOp::eAdd; // Optional

		/* G Buffer 1 */
		blendAttachments[1].colorWriteMask = vk::ColorComponentFlagBits::eR | 
											vk::ColorComponentFlagBits::eG | 
											vk::ColorComponentFlagBits::eB | 
											vk::ColorComponentFlagBits::eA;
		blendAttachments[1].blendEnable = true;
		blendAttachments[1].srcColorBlendFactor = vk::BlendFactor::eSrcAlpha; // Optional /* TRANSPARENCY STUFF HERE! */
		blendAttachments[1].dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha; // Optional
		blendAttachments[1].colorBlendOp = vk::BlendOp::eAdd; // Optional
		blendAttachments[1].srcAlphaBlendFactor = vk::BlendFactor::eOne; // Optional
		blendAttachments[1].dstAlphaBlendFactor = vk::BlendFactor::eZero; // Optional
		blendAttachments[1].alphaBlendOp = vk::BlendOp::eAdd; // Optional

		/* G Buffer 2 */
		blendAttachments[2].colorWriteMask = vk::ColorComponentFlagBits::eR | 
											vk::ColorComponentFlagBits::eG | 
											vk::ColorComponentFlagBits::eB | 
											vk::ColorComponentFlagBits::eA;
		blendAttachments[2].blendEnable = true;
		blendAttachments[2].srcColorBlendFactor = vk::BlendFactor::eSrcAlpha; // Optional /* TRANSPARENCY STUFF HERE! */
		blendAttachments[2].dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha; // Optional
		blendAttachments[2].colorBlendOp = vk::BlendOp::eAdd; // Optional
		blendAttachments[2].srcAlphaBlendFactor = vk::BlendFactor::eOne; // Optional
		blendAttachments[2].dstAlphaBlendFactor = vk::BlendFactor::eZero; // Optional
		blendAttachments[2].alphaBlendOp = vk::BlendOp::eAdd; // Optional

		/* Default Color Blending State */
		colorBlending.logicOpEnable = false;
		colorBlending.logicOp = vk::LogicOp::eCopy; // Optional
		colorBlending.attachmentCount = blendAttachments.size();
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