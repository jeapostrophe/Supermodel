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

#include "SuperAA.h"

#include "Shaders/SuperAA.vert.spv.h"
#include "Shaders/SuperAA.frag.spv.h"

using Vulkan::VulkanContext;
using Vulkan::VulkanShader;

SuperAA::SuperAA(int aaValue, CRTcolor CRTcolors) :
	m_aa(aaValue),
	m_crtcolors(CRTcolors)
{
	// Nothing is created here: the pipeline needs the screen target's render
	// pass, which does not exist until Init.
}

SuperAA::~SuperAA()
{
	m_program.Destroy(VulkanContext::Get());
	m_fbo.Destroy();
}

void SuperAA::Init(int width, int height)
{
	if (!Enabled())
		return;

	VulkanContext &context = VulkanContext::Get();
	if (!context.IsReady() || context.Screen() == nullptr)
	{
		ErrorLog("SuperAA: no Vulkan screen target to draw into.\n");
		return;
	}

	if (!m_fbo.Create(width * m_aa, height * m_aa))
	{
		ErrorLog("SuperAA: could not create its supersampled render target.\n");
		return;
	}

	m_width			= width;
	m_height		= height;
	m_outputWidth	= width;
	m_outputHeight	= height;

	m_program.Destroy(context);

	m_program.vertexShader		= VulkanShader::CreateModule(context, SuperAA_vert_spv, sizeof(SuperAA_vert_spv));
	m_program.fragmentShader	= VulkanShader::CreateModule(context, SuperAA_frag_spv, sizeof(SuperAA_frag_spv));

	const Vulkan::DescriptorBinding bindings[] = {
		{ 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1 }
	};
	m_program.setLayout			= context.CreateDescriptorSetLayout(bindings, 1);
	m_program.pipelineLayout	= VulkanShader::CreateLayout(context, &m_program.setLayout, 1, 0, 0);

	// The two values the GL path pasted into the shader source as text.
	const int32_t specData[2] = { (int32_t)m_aa, (int32_t)m_crtcolors };
	VkSpecializationMapEntry entries[2] = {};
	entries[0].constantID	= 0;
	entries[0].offset		= 0;
	entries[0].size			= sizeof(int32_t);
	entries[1].constantID	= 1;
	entries[1].offset		= sizeof(int32_t);
	entries[1].size			= sizeof(int32_t);

	VkSpecializationInfo spec = {};
	spec.mapEntryCount	= 2;
	spec.pMapEntries	= entries;
	spec.dataSize		= sizeof(specData);
	spec.pData			= specData;

	Vulkan::PipelineDesc desc;
	desc.vertexShader	= m_program.vertexShader;
	desc.fragmentShader	= m_program.fragmentShader;
	desc.fragmentSpec	= &spec;
	desc.layout			= m_program.pipelineLayout;
	desc.renderPass		= context.Screen()->pass;		// SuperAA always draws at the screen
	desc.colourAttachmentCount = (uint32_t)context.Screen()->colour.size();
	desc.blend			= false;						// the GL path disables blending here
	desc.depthTest		= false;						// ...and the depth and stencil tests

	m_program.pipeline = VulkanShader::CreatePipeline(context, desc);
	if (!m_program.Valid())
		ErrorLog("SuperAA: could not create its pipeline.\n");
}

void SuperAA::SetOutputSize(int width, int height)
{
	m_outputWidth = width > 0 ? width : m_width;
	m_outputHeight = height > 0 ? height : m_height;
}

void SuperAA::Draw()
{
	if (!Enabled() || !m_program.Valid())
		return;

	VulkanContext &context = VulkanContext::Get();
	Vulkan::Image *source = m_fbo.GetTextureID();
	if (source == nullptr || !context.FrameOpen())
		return;

	// glDisable(GL_DEPTH_TEST | GL_STENCIL_TEST | GL_BLEND) are all pipeline
	// state here; only the scissor has to be said out loud.
	context.BindScreen();
	context.SetScissorOff();
	context.SetViewport(0, 0, m_outputWidth > 0 ? m_outputWidth : m_width, m_outputHeight > 0 ? m_outputHeight : m_height);

	context.DrawTexturedQuad(m_program, source->view, context.Sampler(true));
}

AATarget SuperAA::GetTargetID()
{
	return m_fbo.GetFBOID();	// nullptr if no render target, which is our screen target
}
