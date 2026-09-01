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

/*
 * VulkanShader.h
 *
 * The GLSLShader analogue: SPIR-V modules, pipeline layouts and graphics
 * pipelines. Everything a Supermodel renderer varies is a field of PipelineDesc
 * -- the state space is genuinely tiny (see Src/Graphics/Vul3D/README.md), so
 * this is a description rather than a builder.
 *
 * Fixed for every pipeline this port creates, because it is fixed in the GL
 * sources too: cull mode NONE (New3D emulates back-face culling in the shader),
 * fill mode FILL, one sample, and dynamic viewport, scissor and the three
 * stencil values.
 */

#ifndef INCLUDED_VULKAN_VULKANSHADER_H
#define INCLUDED_VULKAN_VULKANSHADER_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include "VulkanContext.h"

namespace Vulkan {

/* The two stencil configurations New3D uses, plus "no stencil test". */
enum class StencilMode
{
	Off,
	/*
	 * The line-of-sight write: always pass, and replace on depth-pass. The
	 * reference, compare mask and write mask are dynamic (R3DShader drives them
	 * from noLosReturn).
	 */
	Los,
	/*
	 * The layered-polygon pass: pass only where the stencil still equals the
	 * reference, and count up on every fragment that gets there.
	 */
	Layered
};

struct VertexAttribute
{
	uint32_t	location;
	VkFormat	format;
	uint32_t	offset;
};

struct PipelineDesc
{
	VkShaderModule				vertexShader	= VK_NULL_HANDLE;
	VkShaderModule				fragmentShader	= VK_NULL_HANDLE;
	const VkSpecializationInfo	*vertexSpec		= nullptr;
	const VkSpecializationInfo	*fragmentSpec	= nullptr;

	VkPipelineLayout			layout			= VK_NULL_HANDLE;
	VkRenderPass				renderPass		= VK_NULL_HANDLE;

	/* 0 = no vertex buffer at all (the full-screen quads index off gl_VertexIndex). */
	uint32_t					vertexStride	= 0;
	std::vector<VertexAttribute> attributes;

	VkPrimitiveTopology			topology		= VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

	/*
	 * Must equal the render pass's colour attachment count. writeMask[i] is
	 * glColorMaski / the write half of glDrawBuffers -- Vul3D's three-attachment
	 * MRT selects a layer by writing to one attachment and masking the others.
	 */
	uint32_t					colourAttachmentCount = 1;
	VkColorComponentFlags		writeMask[3] = {		// R3DFrameBuffers' MRT is the widest target
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
		0, 0
	};

	/* GL_SRC_ALPHA / GL_ONE_MINUS_SRC_ALPHA, GL_FUNC_ADD, on both channels. */
	bool						blend			= false;

	bool						depthTest		= false;
	bool						depthWrite		= false;
	VkCompareOp					depthCompare	= VK_COMPARE_OP_GREATER_OR_EQUAL;

	StencilMode					stencil			= StencilMode::Off;
};

/*
 * The five objects that make up one shader, together because every renderer
 * here creates and destroys exactly this set (the GLSLShader analogue, hence
 * this file). Only Destroy is shared: what the renderers build differs enough
 * -- SPIR-V, specialization, target, blending, vertex layout -- that a factory
 * would take as many arguments as the body it replaced.
 */
struct ShaderProgram
{
	VkShaderModule			vertexShader	= VK_NULL_HANDLE;
	VkShaderModule			fragmentShader	= VK_NULL_HANDLE;
	VkDescriptorSetLayout	setLayout		= VK_NULL_HANDLE;
	VkPipelineLayout		pipelineLayout	= VK_NULL_HANDLE;
	VkPipeline				pipeline		= VK_NULL_HANDLE;

	bool Valid() const { return pipeline != VK_NULL_HANDLE; }
	void Destroy(VulkanContext &context);
};

class VulkanShader
{
public:
	/* sizeBytes, not word count -- the generated headers give sizeof(x_spv). */
	static VkShaderModule	CreateModule(VulkanContext &context, const uint32_t *spirv, size_t sizeBytes);
	static void				DestroyModule(VulkanContext &context, VkShaderModule module);

	static VkPipelineLayout	CreateLayout(VulkanContext &context, const VkDescriptorSetLayout *sets, uint32_t setCount, uint32_t pushConstantBytes, VkShaderStageFlags pushConstantStages);
	static void				DestroyLayout(VulkanContext &context, VkPipelineLayout layout);

	static VkPipeline		CreatePipeline(VulkanContext &context, const PipelineDesc &desc);
	static void				DestroyPipeline(VulkanContext &context, VkPipeline pipeline);
};

} // namespace Vulkan

#endif	// INCLUDED_VULKAN_VULKANSHADER_H
