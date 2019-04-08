#pragma once
#include "Pluto/Libraries/Vulkan/Vulkan.hxx"

struct PipelineParameters {
	vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
	vk::PipelineRasterizationStateCreateInfo rasterizer;
	vk::PipelineViewportStateCreateInfo viewportState;
	vk::PipelineMultisampleStateCreateInfo multisampling;
	vk::PipelineDepthStencilStateCreateInfo depthStencil;
	vk::PipelineColorBlendAttachmentState colorBlendAttachment;
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
		rasterizer.cullMode = vk::CullModeFlagBits::eBack;
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

		/* Default Color Blending Attachment State */
		colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
		colorBlendAttachment.blendEnable = true;
		colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha; // Optional /* TRANSPARENCY STUFF HERE! */
		colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha; // Optional
		colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd; // Optional
		colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne; // Optional
		colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eZero; // Optional
		colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd; // Optional

		/* Default Color Blending State */
		colorBlending.logicOpEnable = false;
		colorBlending.logicOp = vk::LogicOp::eCopy; // Optional
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;
		colorBlending.blendConstants[0] = 0.0f; // Optional
		colorBlending.blendConstants[1] = 0.0f; // Optional
		colorBlending.blendConstants[2] = 0.0f; // Optional
		colorBlending.blendConstants[3] = 0.0f; // Optional

		/* Default Dynamic State */
		dynamicState.dynamicStateCount = 3;
		dynamicState.pDynamicStates = dynamicStates;
	}
};