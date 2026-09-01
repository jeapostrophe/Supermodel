/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2003-2026 The Supermodel Team
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

#include "R3DScrollFog.h"

#include <cstddef>
#include <cstring>
#include <iterator>

#include "Supermodel.h"
#include "Shaders/R3DScrollFog.vert.spv.h"
#include "Shaders/R3DScrollFog.frag.spv.h"
#include "Shaders/R3DBlockOffsets.h"

namespace Vul3D {

	using Vulkan::VulkanContext;
	using Vulkan::VulkanShader;

	// The GLSL is the source of truth: R3DBlockOffsets.h is generated out of
	// the compiled SPIR-V by Scripts/build-spirv.sh, so these compare the C++
	// mirror against the shader rather than against a hand-typed copy of it.
	static_assert(offsetof(R3DScrollFog::ScrollFogBlock, fogColour)		== VUL3D_STD140_ScrollFogBlock_fogColour,		"ScrollFogBlock.fogColour");
	static_assert(offsetof(R3DScrollFog::ScrollFogBlock, spotEllipse)	== VUL3D_STD140_ScrollFogBlock_spotEllipse,		"ScrollFogBlock.spotEllipse");
	static_assert(offsetof(R3DScrollFog::ScrollFogBlock, spotFogColor)	== VUL3D_STD140_ScrollFogBlock_spotFogColor,	"ScrollFogBlock.spotFogColor");
	static_assert(offsetof(R3DScrollFog::ScrollFogBlock, fogAttenuation)== VUL3D_STD140_ScrollFogBlock_fogAttenuation,	"ScrollFogBlock.fogAttenuation");
	static_assert(offsetof(R3DScrollFog::ScrollFogBlock, fogAmbient)	== VUL3D_STD140_ScrollFogBlock_fogAmbient,		"ScrollFogBlock.fogAmbient");
	// ...and that fogAmbient really is the LAST member: an append to the GLSL
	// moves no existing offset, so without this every assert above would still
	// pass while this mirror was silently short.
	static_assert(offsetof(R3DScrollFog::ScrollFogBlock, fogAmbient)	== VUL3D_STD140_ScrollFogBlock_LASTOFFSET,		"ScrollFogBlock has a member this mirror does not");
	static_assert(sizeof(R3DScrollFog::ScrollFogBlock) % 16 == 0, "std140 blocks are a multiple of 16 bytes");

	R3DScrollFog::R3DScrollFog()
	{
		AllocResources();
	}

	R3DScrollFog::~R3DScrollFog()
	{
		DeallocResources();
	}

	void R3DScrollFog::DrawScrollFog(float rgba[4], float attenuation, float ambient, float spotRGB[3], float spotEllipse[4])
	{
		VulkanContext &context = VulkanContext::Get();

		if (!m_program.Valid() || !context.FrameOpen()) {
			return;
		}

		// glDepthMask(GL_FALSE) and glDisable(GL_DEPTH_TEST) -- and the
		// glDisable(GL_BLEND) / glDepthMask(GL_TRUE) that put them back -- are
		// all pipeline state here, so they have no counterpart.

		ScrollFogBlock block = {};
		std::memcpy(block.fogColour,	rgba,			sizeof(block.fogColour));
		std::memcpy(block.spotEllipse,	spotEllipse,	sizeof(block.spotEllipse));
		std::memcpy(block.spotFogColor,	spotRGB,		sizeof(block.spotFogColor));
		block.fogAttenuation	= attenuation;
		block.fogAmbient		= ambient;

		Vulkan::UniformSlice slice = context.AppendUniforms(&block, sizeof(block));
		if (!slice.valid) {
			return;
		}

		VkDescriptorSet set = context.AllocateDescriptorSet(m_program.setLayout);
		if (set == VK_NULL_HANDLE) {
			return;
		}

		context.UpdateDescriptorBuffer(set, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, slice.buffer, sizeof(block));
		context.BindDescriptorSet(m_program.pipelineLayout, set, 1, &slice.offset);
		context.Draw(m_program.pipeline, 0, 4);
	}

	void R3DScrollFog::AllocResources()
	{
		VulkanContext &context = VulkanContext::Get();

		m_program.vertexShader		= VulkanShader::CreateModule(context, R3DScrollFog_vert_spv, sizeof(R3DScrollFog_vert_spv));
		m_program.fragmentShader	= VulkanShader::CreateModule(context, R3DScrollFog_frag_spv, sizeof(R3DScrollFog_frag_spv));

		const Vulkan::DescriptorBinding bindings[] = {
			{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_FRAGMENT_BIT, 1 }
		};
		m_program.setLayout			= context.CreateDescriptorSetLayout(bindings, 1);
		m_program.pipelineLayout	= VulkanShader::CreateLayout(context, &m_program.setLayout, 1, 0, 0);
	}

	bool R3DScrollFog::CreatePipeline(Vulkan::RenderTarget* target)
	{
		VulkanContext &context = VulkanContext::Get();

		if (target == nullptr || m_program.vertexShader == VK_NULL_HANDLE) {
			return false;
		}

		VulkanShader::DestroyPipeline(context, m_program.pipeline);
		m_program.pipeline = VK_NULL_HANDLE;

		Vulkan::PipelineDesc desc;
		desc.vertexShader	= m_program.vertexShader;
		desc.fragmentShader	= m_program.fragmentShader;
		desc.layout			= m_program.pipelineLayout;
		desc.renderPass		= target->pass;
		desc.colourAttachmentCount = (uint32_t)target->colour.size();
		for (uint32_t i = 0; i < desc.colourAttachmentCount && i < std::size(desc.writeMask); i++) {
			desc.writeMask[i] = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		}
		desc.blend			= false;	// blending is OFF here: CRender2D::DrawSurface disables it before the 3D frame starts, and nothing turns it back on until R3DFrameBuffers::Draw
		desc.depthTest		= false;	// glDisable(GL_DEPTH_TEST)
		desc.depthWrite		= false;	// glDepthMask(GL_FALSE)

		m_program.pipeline = VulkanShader::CreatePipeline(context, desc);
		if (!m_program.Valid()) {
			ErrorLog("R3DScrollFog: could not create its pipeline.\n");
			return false;
		}

		return true;
	}

	void R3DScrollFog::DeallocResources()
	{
		m_program.Destroy(VulkanContext::Get());
	}

}
