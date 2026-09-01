/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2011-2012 Bart Trzynadlowski, Nik Henson
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
 * Src/Graphics/Vulkan/Render2D.cpp
 *
 * The Vulkan mirror of Src/Graphics/Render2D.cpp. The tile generator hardware
 * documentation at the top of the GL source applies unchanged and is not
 * duplicated here; what follows is a line-for-line port of its rendering.
 */

#include "Render2D.h"

#include "Shaders/Shaders2D.vert.spv.h"
#include "Shaders/Shaders2D.frag.spv.h"

using Vulkan::VulkanContext;
using Vulkan::VulkanShader;

// The tile generator's draw buffers are 512 texels wide but only the first 496
// of each row are visible -- glPixelStorei(GL_UNPACK_ROW_LENGTH, 512) in GL,
// VkBufferImageCopy::bufferRowLength here.
static const uint32_t	SURFACE_WIDTH		= 496;
static const uint32_t	SURFACE_HEIGHT		= 384;
static const uint32_t	SURFACE_ROW_TEXELS	= 512;

/******************************************************************************
 Frame Display Functions
******************************************************************************/

// Set up viewport and state for 2D rendering (blending is pipeline state here,
// so the GL glBlendFunc call has no counterpart; depth testing likewise).
void CRender2D::Setup2D(bool isBottom)
{
	VulkanContext &context = VulkanContext::Get();

	// Clear everything if requested or just overscan areas for wide screen mode
	if (isBottom)
	{
		context.BindTarget(m_aaTarget);			// set target if needed; nullptr is the screen

		static const float black[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		context.SetViewport	(0, 0, (int)m_totalXPixels, (int)m_totalYPixels);
		context.SetScissorOff();											// scissor is enabled to fix the 2d/3d miss match problem
		context.ClearAttachments(true, false, false, black, 0.0f, 0);		// we want to clear outside the scissored areas so must disable it
		context.SetScissorOn();

		if (m_aaTarget)
			BindSupermodelDefaultFramebuffer();	// restore target if needed
	}

	// Set up the viewport and orthogonal projection
	bool stretchBottom = m_config["WideBackground"].ValueAs<bool>() && isBottom;
	if (!stretchBottom)
	{
		context.SetViewport((int)m_xOffset - (int)m_correction, (int)m_yOffset + (int)m_correction, (int)m_xPixels, (int)m_yPixels); //Preserve aspect ratio of tile layer by constraining and centering viewport
	}
}

void CRender2D::DrawSurface(Vulkan::Image &texture)
{
	VulkanContext &context = VulkanContext::Get();

	if (!m_program.Valid() || !texture.Valid() || !context.FrameOpen())
		return;

	context.BindTarget(m_aaTarget);				// set target if needed; nullptr is the screen

	context.DrawTexturedQuad(m_program, texture.view, context.Sampler(m_upscaleMode != UpscaleMode::Nearest));

	if (m_aaTarget)
		BindSupermodelDefaultFramebuffer();		// restore target if needed
}

void CRender2D::BeginFrame(void)
{
}

void CRender2D::PreRenderFrame(void)
{
	VulkanContext &context = VulkanContext::Get();

	for (int i = 0; i < 2; i++) {
		if (!m_drawBuffers[i]) {
			// No draw buffer yet -- but DrawSurface will sample this texture
			// anyway, and its descriptor claims SHADER_READ_ONLY_OPTIMAL while
			// an image nothing has uploaded to is still UNDEFINED.
			context.TransitionImage(m_textures[i], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			continue;
		}
		context.UploadImage(m_textures[i], 0, 0, 0, SURFACE_WIDTH, SURFACE_HEIGHT, SURFACE_ROW_TEXELS,
			m_drawBuffers[i]->data, sizeof(m_drawBuffers[i]->data));
	}
}

void CRender2D::RenderFrameBottom(void)
{
	Setup2D(true);
	DrawSurface(m_textures[0]);
}

void CRender2D::RenderFrameTop(void)
{
	Setup2D(false);
	DrawSurface(m_textures[1]);
}

void CRender2D::EndFrame(void)
{
}


/******************************************************************************
 Emulation Callbacks
******************************************************************************/

// Deprecated
void CRender2D::WriteVRAM(unsigned addr, uint32_t data)
{
}


/******************************************************************************
 Configuration, Initialization, and Shutdown
******************************************************************************/

void CRender2D::AttachRegisters(const uint32_t* regPtr)
{
}

void CRender2D::AttachPalette(const uint32_t* palPtr[2])
{
}

void CRender2D::AttachVRAM(const uint8_t* vramPtr)
{
}

void CRender2D::AttachDrawBuffers(std::shared_ptr<TileGenBuffer> bottom, std::shared_ptr<TileGenBuffer> top)
{
	m_drawBuffers[0] = bottom;
	m_drawBuffers[1] = top;
}

Result CRender2D::Init(unsigned xOffset, unsigned yOffset, unsigned xRes, unsigned yRes, unsigned totalXRes, unsigned totalYRes, AATarget aaTarget, UpscaleMode upscaleMode)
{
	VulkanContext &context = VulkanContext::Get();
	if (!context.IsReady() || context.Screen() == nullptr)
	{
		ErrorLog("Render2D: no Vulkan screen target to draw into.\n");
		return Result::FAIL;
	}

	// Resolution
	m_xPixels		= xRes;
	m_yPixels		= yRes;
	m_xOffset		= xOffset;
	m_yOffset		= yOffset;
	m_totalXPixels	= totalXRes;
	m_totalYPixels	= totalYRes;
	m_correction	= (UINT32)(((yRes / 384.) * 2.) + 0.5);		// for some reason the 2d layer is 2 pixels off the 3D
	m_aaTarget		= aaTarget;
	m_upscaleMode	= (m_xPixels == 496 && m_yPixels == 384) ? UpscaleMode::Nearest : upscaleMode;

	Destroy();

	m_program.vertexShader		= VulkanShader::CreateModule(context, Shaders2D_vert_spv, sizeof(Shaders2D_vert_spv));
	m_program.fragmentShader	= VulkanShader::CreateModule(context, Shaders2D_frag_spv, sizeof(Shaders2D_frag_spv));

	const Vulkan::DescriptorBinding bindings[] = {
		{ 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1 }
	};
	m_program.setLayout			= context.CreateDescriptorSetLayout(bindings, 1);
	m_program.pipelineLayout	= VulkanShader::CreateLayout(context, &m_program.setLayout, 1, 0, 0);

	// What the GL path did with a "#define UPSCALEMODE n" pasted in front of
	// the fragment source.
	const int32_t upscale = (int32_t)m_upscaleMode;
	VkSpecializationMapEntry entry = {};
	entry.constantID	= 0;
	entry.offset		= 0;
	entry.size			= sizeof(int32_t);

	VkSpecializationInfo spec = {};
	spec.mapEntryCount	= 1;
	spec.pMapEntries	= &entry;
	spec.dataSize		= sizeof(upscale);
	spec.pData			= &upscale;

	Vulkan::RenderTarget *target = m_aaTarget != nullptr ? m_aaTarget : context.Screen();

	Vulkan::PipelineDesc desc;
	desc.vertexShader	= m_program.vertexShader;
	desc.fragmentShader	= m_program.fragmentShader;
	desc.fragmentSpec	= &spec;
	desc.layout			= m_program.pipelineLayout;
	desc.renderPass		= target->pass;
	desc.colourAttachmentCount = (uint32_t)target->colour.size();
	desc.blend			= true;		// alpha of 1.0 is opaque, 0 is transparent
	desc.depthTest		= false;	// Disable Z-buffering

	m_program.pipeline = VulkanShader::CreatePipeline(context, desc);
	if (!m_program.Valid())
	{
		ErrorLog("Render2D: could not create its pipeline.\n");
		return Result::FAIL;
	}

	// allocate storage
	for (auto& t : m_textures) {
		if (!context.CreateImage(t, SURFACE_WIDTH, SURFACE_HEIGHT, 1, VK_FORMAT_R8G8B8A8_UNORM,
				VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_ASPECT_COLOR_BIT))
		{
			ErrorLog("Render2D: could not create its tile surfaces.\n");
			return Result::FAIL;
		}
	}

	return Result::OKAY;
}

CRender2D::CRender2D(const Util::Config::Node& config)
	: m_config(config)
{
	// Nothing is created here: the pipeline needs a render pass, which does not
	// exist until Init knows which target it draws into.
}

CRender2D::~CRender2D(void)
{
	Destroy();
}

void CRender2D::Destroy(void)
{
	VulkanContext &context = VulkanContext::Get();

	for (auto& t : m_textures)
		context.DestroyImage(t);

	m_program.Destroy(context);
}
