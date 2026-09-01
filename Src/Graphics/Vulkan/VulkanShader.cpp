/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 **
 ** This file is part of Supermodel.
 **
 ** Supermodel is free software: you can redistribute it and/or modify it under
 ** the terms of the GNU General Public License as published by the Free
 ** Software Foundation, either version 3 of the License, or (at your option)
 ** any later version.
 **
 ** Supermodel is distributed in the hope that it will be useful, but WITHOUT
 ** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 ** FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 ** more details.
 **
 ** You should have received a copy of the GNU General Public License along
 ** with Supermodel.  If not, see <http://www.gnu.org/licenses/>.
 **/

#include "VulkanShader.h"

#include "Supermodel.h"

namespace Vulkan {

void ShaderProgram::Destroy(VulkanContext &context)
{
	VulkanShader::DestroyPipeline(context, pipeline);
	VulkanShader::DestroyLayout(context, pipelineLayout);
	context.DestroyDescriptorSetLayout(setLayout);
	VulkanShader::DestroyModule(context, vertexShader);
	VulkanShader::DestroyModule(context, fragmentShader);
	*this = ShaderProgram();
}

VkShaderModule VulkanShader::CreateModule(VulkanContext &context, const uint32_t *spirv, size_t sizeBytes)
{
	if (!context.IsReady() || spirv == nullptr || sizeBytes == 0)
		return VK_NULL_HANDLE;

	VkShaderModuleCreateInfo info = {};
	info.sType		= VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	info.codeSize	= sizeBytes;
	info.pCode		= spirv;

	VkShaderModule module = VK_NULL_HANDLE;
	if (context.Vk().vkCreateShaderModule(context.Device(), &info, nullptr, &module) != VK_SUCCESS)
	{
		ErrorLog("Vulkan: could not create a shader module.\n");
		return VK_NULL_HANDLE;
	}
	return module;
}

void VulkanShader::DestroyModule(VulkanContext &context, VkShaderModule module)
{
	if (context.Device() != VK_NULL_HANDLE && module != VK_NULL_HANDLE)
		context.Vk().vkDestroyShaderModule(context.Device(), module, nullptr);
}

VkPipelineLayout VulkanShader::CreateLayout(VulkanContext &context, const VkDescriptorSetLayout *sets, uint32_t setCount, uint32_t pushConstantBytes, VkShaderStageFlags pushConstantStages)
{
	if (!context.IsReady())
		return VK_NULL_HANDLE;

	VkPushConstantRange push = {};
	push.stageFlags	= pushConstantStages;
	push.offset		= 0;
	push.size		= pushConstantBytes;

	VkPipelineLayoutCreateInfo info = {};
	info.sType					= VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	info.setLayoutCount			= setCount;
	info.pSetLayouts			= sets;
	info.pushConstantRangeCount	= pushConstantBytes > 0 ? 1 : 0;
	info.pPushConstantRanges	= pushConstantBytes > 0 ? &push : nullptr;

	VkPipelineLayout layout = VK_NULL_HANDLE;
	if (context.Vk().vkCreatePipelineLayout(context.Device(), &info, nullptr, &layout) != VK_SUCCESS)
	{
		ErrorLog("Vulkan: could not create a pipeline layout.\n");
		return VK_NULL_HANDLE;
	}
	return layout;
}

void VulkanShader::DestroyLayout(VulkanContext &context, VkPipelineLayout layout)
{
	if (context.Device() != VK_NULL_HANDLE && layout != VK_NULL_HANDLE)
		context.Vk().vkDestroyPipelineLayout(context.Device(), layout, nullptr);
}

VkPipeline VulkanShader::CreatePipeline(VulkanContext &context, const PipelineDesc &desc)
{
	if (!context.IsReady() || desc.vertexShader == VK_NULL_HANDLE || desc.fragmentShader == VK_NULL_HANDLE ||
		desc.layout == VK_NULL_HANDLE || desc.renderPass == VK_NULL_HANDLE)
	{
		ErrorLog("Vulkan: incomplete pipeline description.\n");
		return VK_NULL_HANDLE;
	}

	VkPipelineShaderStageCreateInfo stages[2] = {};
	stages[0].sType					= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage					= VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module				= desc.vertexShader;
	stages[0].pName					= "main";
	stages[0].pSpecializationInfo	= desc.vertexSpec;
	stages[1].sType					= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage					= VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module				= desc.fragmentShader;
	stages[1].pName					= "main";
	stages[1].pSpecializationInfo	= desc.fragmentSpec;

	VkVertexInputBindingDescription binding = {};
	binding.binding		= 0;
	binding.stride		= desc.vertexStride;
	binding.inputRate	= VK_VERTEX_INPUT_RATE_VERTEX;

	std::vector<VkVertexInputAttributeDescription> attributes(desc.attributes.size());
	for (size_t i = 0; i < desc.attributes.size(); i++)
	{
		attributes[i].location	= desc.attributes[i].location;
		attributes[i].binding	= 0;
		attributes[i].format	= desc.attributes[i].format;
		attributes[i].offset	= desc.attributes[i].offset;
	}

	VkPipelineVertexInputStateCreateInfo vertexInput = {};
	vertexInput.sType							= VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInput.vertexBindingDescriptionCount	= desc.vertexStride > 0 ? 1 : 0;
	vertexInput.pVertexBindingDescriptions		= desc.vertexStride > 0 ? &binding : nullptr;
	vertexInput.vertexAttributeDescriptionCount	= (uint32_t)attributes.size();
	vertexInput.pVertexAttributeDescriptions	= attributes.empty() ? nullptr : attributes.data();

	VkPipelineInputAssemblyStateCreateInfo assembly = {};
	assembly.sType		= VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	assembly.topology	= desc.topology;

	// Both are dynamic; the counts are all the pipeline needs to agree with.
	VkPipelineViewportStateCreateInfo viewport = {};
	viewport.sType			= VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport.viewportCount	= 1;
	viewport.scissorCount	= 1;

	VkPipelineRasterizationStateCreateInfo raster = {};
	raster.sType		= VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	raster.polygonMode	= VK_POLYGON_MODE_FILL;
	raster.cullMode		= VK_CULL_MODE_NONE;		// New3D culls in the shader, so frontFace never matters
	raster.lineWidth	= 1.0f;

	VkPipelineMultisampleStateCreateInfo multisample = {};
	multisample.sType					= VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample.rasterizationSamples	= VK_SAMPLE_COUNT_1_BIT;

	VkStencilOpState stencil = {};
	switch (desc.stencil)
	{
	case StencilMode::Los:
		stencil.compareOp	= VK_COMPARE_OP_ALWAYS;
		stencil.failOp		= VK_STENCIL_OP_KEEP;
		stencil.depthFailOp	= VK_STENCIL_OP_KEEP;
		stencil.passOp		= VK_STENCIL_OP_REPLACE;
		break;
	case StencilMode::Layered:
		stencil.compareOp	= VK_COMPARE_OP_EQUAL;
		stencil.failOp		= VK_STENCIL_OP_KEEP;
		stencil.depthFailOp	= VK_STENCIL_OP_INCREMENT_AND_CLAMP;
		stencil.passOp		= VK_STENCIL_OP_INCREMENT_AND_CLAMP;
		break;
	case StencilMode::Off:
	default:
		stencil.compareOp	= VK_COMPARE_OP_ALWAYS;
		stencil.failOp		= VK_STENCIL_OP_KEEP;
		stencil.depthFailOp	= VK_STENCIL_OP_KEEP;
		stencil.passOp		= VK_STENCIL_OP_KEEP;
		break;
	}
	// reference, compareMask and writeMask are dynamic on every pipeline; see
	// VulkanContext::SetStencil.

	VkPipelineDepthStencilStateCreateInfo depthStencil = {};
	depthStencil.sType				= VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable	= desc.depthTest ? VK_TRUE : VK_FALSE;
	depthStencil.depthWriteEnable	= desc.depthWrite ? VK_TRUE : VK_FALSE;
	depthStencil.depthCompareOp		= desc.depthCompare;
	depthStencil.stencilTestEnable	= desc.stencil != StencilMode::Off ? VK_TRUE : VK_FALSE;
	depthStencil.front				= stencil;
	depthStencil.back				= stencil;

	VkPipelineColorBlendAttachmentState attachments[3] = {};
	const uint32_t colourCount = desc.colourAttachmentCount;
	for (uint32_t i = 0; i < colourCount && i < 3; i++)
	{
		attachments[i].blendEnable			= desc.blend ? VK_TRUE : VK_FALSE;
		attachments[i].srcColorBlendFactor	= VK_BLEND_FACTOR_SRC_ALPHA;
		attachments[i].dstColorBlendFactor	= VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		attachments[i].colorBlendOp			= VK_BLEND_OP_ADD;
		attachments[i].srcAlphaBlendFactor	= VK_BLEND_FACTOR_SRC_ALPHA;
		attachments[i].dstAlphaBlendFactor	= VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		attachments[i].alphaBlendOp			= VK_BLEND_OP_ADD;
		attachments[i].colorWriteMask		= desc.writeMask[i];
	}

	VkPipelineColorBlendStateCreateInfo blend = {};
	blend.sType				= VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend.attachmentCount	= colourCount;
	blend.pAttachments		= attachments;

	static const VkDynamicState dynamicStates[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
		VK_DYNAMIC_STATE_STENCIL_REFERENCE,
		VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
		VK_DYNAMIC_STATE_STENCIL_WRITE_MASK
	};

	VkPipelineDynamicStateCreateInfo dynamic = {};
	dynamic.sType				= VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic.dynamicStateCount	= (uint32_t)(sizeof(dynamicStates) / sizeof(dynamicStates[0]));
	dynamic.pDynamicStates		= dynamicStates;

	VkGraphicsPipelineCreateInfo info = {};
	info.sType				= VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	info.stageCount			= 2;
	info.pStages			= stages;
	info.pVertexInputState	= &vertexInput;
	info.pInputAssemblyState = &assembly;
	info.pViewportState		= &viewport;
	info.pRasterizationState = &raster;
	info.pMultisampleState	= &multisample;
	info.pDepthStencilState	= &depthStencil;
	info.pColorBlendState	= &blend;
	info.pDynamicState		= &dynamic;
	info.layout				= desc.layout;
	info.renderPass			= desc.renderPass;
	info.subpass			= 0;

	VkPipeline pipeline = VK_NULL_HANDLE;
	if (context.Vk().vkCreateGraphicsPipelines(context.Device(), VK_NULL_HANDLE, 1, &info, nullptr, &pipeline) != VK_SUCCESS)
	{
		ErrorLog("Vulkan: could not create a graphics pipeline.\n");
		return VK_NULL_HANDLE;
	}
	return pipeline;
}

void VulkanShader::DestroyPipeline(VulkanContext &context, VkPipeline pipeline)
{
	if (context.Device() != VK_NULL_HANDLE && pipeline != VK_NULL_HANDLE)
		context.Vk().vkDestroyPipeline(context.Device(), pipeline, nullptr);
}

} // namespace Vulkan
