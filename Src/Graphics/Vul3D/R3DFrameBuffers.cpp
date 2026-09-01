#include "R3DFrameBuffers.h"

#include <iterator>

#include "Shaders/R3DFrameBuffersBase.vert.spv.h"
#include "Shaders/R3DFrameBuffersBase.frag.spv.h"
#include "Shaders/R3DFrameBuffersTrans.vert.spv.h"
#include "Shaders/R3DFrameBuffersTrans.frag.spv.h"

namespace Vul3D {

using Vulkan::VulkanContext;
using Vulkan::VulkanShader;

R3DFrameBuffers::R3DFrameBuffers()
{
	m_target = nullptr;
	m_dest = nullptr;
	m_width = 0;
	m_height = 0;

	AllocShaderTrans();
	AllocShaderBase();

	// No VAO: the four vertices come out of gl_VertexIndex, and Vulkan has no
	// vertex array object to hold "no state" in.
}

R3DFrameBuffers::~R3DFrameBuffers()
{
	DestroyFBO();
	m_shaderTrans.Destroy(VulkanContext::Get());
	m_shaderBase.Destroy(VulkanContext::Get());
}

Result R3DFrameBuffers::CreateFBO(int width, int height, Vulkan::RenderTarget* dest)
{
	VulkanContext &context = VulkanContext::Get();

	m_width = width;
	m_height = height;
	m_dest = dest != nullptr ? dest : context.Screen();

	// One target with three colour attachments (colour buffer, trans layer1,
	// trans layer2) and a D32S8 depth/stencil, which is what the GL code built
	// out of three textures plus a renderbuffer.
	m_target = context.CreateTarget((uint32_t)width, (uint32_t)height, 3, true);

	// The two composite pipelines are built here rather than in
	// AllocShader{Base,Trans}, because a Vulkan pipeline needs the render pass
	// it will draw into and that is not known until now.
	if (m_target != nullptr && m_dest != nullptr) {

		Vulkan::PipelineDesc desc;
		desc.layout			= m_shaderBase.pipelineLayout;
		desc.renderPass		= m_dest->pass;
		desc.colourAttachmentCount = (uint32_t)m_dest->colour.size();
		desc.blend			= true;		// glEnable(GL_BLEND) with SRC_ALPHA / ONE_MINUS_SRC_ALPHA
		desc.depthTest		= false;	// glDisable(GL_DEPTH_TEST)

		desc.vertexShader	= m_shaderBase.vertexShader;
		desc.fragmentShader	= m_shaderBase.fragmentShader;
		m_shaderBase.pipeline = VulkanShader::CreatePipeline(context, desc);

		desc.layout			= m_shaderTrans.pipelineLayout;
		desc.vertexShader	= m_shaderTrans.vertexShader;
		desc.fragmentShader	= m_shaderTrans.fragmentShader;
		m_shaderTrans.pipeline = VulkanShader::CreatePipeline(context, desc);
	}

	const bool fboOkay = (m_target != nullptr) && m_shaderBase.Valid() && m_shaderTrans.Valid();

	BindSupermodelDefaultFramebuffer();	// created R3DFrameBuffers now disable it

	return ((CreateFBODepthCopy(width, height) == Result::OKAY) && fboOkay) ? Result::OKAY : Result::FAIL;
}

Result R3DFrameBuffers::CreateFBODepthCopy(int width, int height)
{
	// The GL original made a whole second framebuffer for this, but the only
	// thing ever attached to it was a depth/stencil renderbuffer and the only
	// thing ever done with it was a depth+stencil blit, so under Vulkan it is
	// one image and StoreDepth/RestoreDepth are vkCmdCopyImage.
	const bool okay = VulkanContext::Get().CreateImage(m_depthCopy, (uint32_t)width, (uint32_t)height, 1,
		VK_FORMAT_D32_SFLOAT_S8_UINT,
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);

	BindSupermodelDefaultFramebuffer();

	return okay ? Result::OKAY : Result::FAIL;
}

void R3DFrameBuffers::StoreDepth()
{
	if (m_target == nullptr) {
		return;
	}

	VulkanContext::Get().CopyImage(m_target->depth, m_depthCopy, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
}

void R3DFrameBuffers::RestoreDepth()
{
	if (m_target == nullptr) {
		return;
	}

	VulkanContext::Get().CopyImage(m_depthCopy, m_target->depth, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
}

void R3DFrameBuffers::DestroyFBO()
{
	VulkanContext &context = VulkanContext::Get();

	if (m_target) {
		BindSupermodelDefaultFramebuffer();
		context.DestroyTarget(m_target);
		m_target = nullptr;
	}

	context.DestroyImage(m_depthCopy);

	VulkanShader::DestroyPipeline(context, m_shaderBase.pipeline);
	m_shaderBase.pipeline = VK_NULL_HANDLE;
	VulkanShader::DestroyPipeline(context, m_shaderTrans.pipeline);
	m_shaderTrans.pipeline = VK_NULL_HANDLE;

	m_dest = nullptr;
	m_width = 0;
	m_height = 0;
}

void R3DFrameBuffers::BindTexture(Layer layer)
{
	// Nothing to bind: Draw() points descriptors at the three colour images.
	// Unreferenced -- as it is in the GL original, where SuperAA is the only
	// caller of anything in this class. Mirrored anyway so the two stay the
	// same class, exactly as Src/Graphics/Vulkan/FBO.h does.
	(void)layer;
}

void R3DFrameBuffers::SetFBO(Layer layer)
{
	// The three colour layers are one Vulkan render target, and glDrawBuffers
	// becomes the colour write mask of the pipeline R3DShader picks -- which it
	// takes from R3DShader::SetLayer, called immediately before every one of
	// these. So all this has left to do is bind the target.
	//
	// Unlike glBindFramebuffer this is NOT skipped when the layer is unchanged:
	// a depth copy or a line-of-sight readback ends the open render pass, and
	// this is where the next pass is opened.
	switch (layer)
	{
	case Layer::colour:
	case Layer::trans1:
	case Layer::trans2:
	{
		VulkanContext::Get().BindTarget(m_target);
		break;
	}
	case Layer::none:
	{
		BindSupermodelDefaultFramebuffer();
		break;
	}
	default:
	{
		break;
	}
	}
}

void R3DFrameBuffers::AllocShaderBase()
{
	VulkanContext &context = VulkanContext::Get();

	m_shaderBase.vertexShader	= VulkanShader::CreateModule(context, R3DFrameBuffersBase_vert_spv, sizeof(R3DFrameBuffersBase_vert_spv));
	m_shaderBase.fragmentShader	= VulkanShader::CreateModule(context, R3DFrameBuffersBase_frag_spv, sizeof(R3DFrameBuffersBase_frag_spv));

	const Vulkan::DescriptorBinding bindings[] = {
		{ 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1 }		// tex1
	};
	m_shaderBase.setLayout		= context.CreateDescriptorSetLayout(bindings, 1);
	m_shaderBase.pipelineLayout	= VulkanShader::CreateLayout(context, &m_shaderBase.setLayout, 1, 0, 0);
}

void R3DFrameBuffers::AllocShaderTrans()
{
	VulkanContext &context = VulkanContext::Get();

	m_shaderTrans.vertexShader		= VulkanShader::CreateModule(context, R3DFrameBuffersTrans_vert_spv, sizeof(R3DFrameBuffersTrans_vert_spv));
	m_shaderTrans.fragmentShader	= VulkanShader::CreateModule(context, R3DFrameBuffersTrans_frag_spv, sizeof(R3DFrameBuffersTrans_frag_spv));

	const Vulkan::DescriptorBinding bindings[] = {
		{ 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1 },		// tex1
		{ 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1 }		// tex2
	};
	m_shaderTrans.setLayout		= context.CreateDescriptorSetLayout(bindings, 2);
	m_shaderTrans.pipelineLayout = VulkanShader::CreateLayout(context, &m_shaderTrans.setLayout, 1, 0, 0);
}

void R3DFrameBuffers::Draw()
{
	VulkanContext &context = VulkanContext::Get();

	if (m_target == nullptr || !context.FrameOpen()) {
		return;
	}

	context.SetViewport	(0, 0, m_width, m_height);			// cover the entire screen
	// depth testing / writing, culling and blending are pipeline state here
#ifdef __linux__
	// Linux libretro keeps the render scissor active from the internal pass.
	// Disable it here so the final composite covers the whole framebuffer.
	context.SetScissorOff();
#endif

	// The three colour images are bound by descriptor rather than to texture
	// units; ending the MRT pass (which binding the destination target did)
	// already left them in SHADER_READ_ONLY_OPTIMAL.

	DrawBaseLayer		();
	DrawAlphaLayer		();
}

void R3DFrameBuffers::DrawBaseLayer()
{
	VulkanContext &context = VulkanContext::Get();

	context.DrawTexturedQuad(m_shaderBase, m_target->colour[0].view, context.Sampler(false));
}

void R3DFrameBuffers::DrawAlphaLayer()
{
	VulkanContext &context = VulkanContext::Get();

	const VkImageView views[] = {
		m_target->colour[1].view,		// tex1, what GL bound to texture unit 1
		m_target->colour[2].view		// tex2, texture unit 2
	};

	context.DrawTexturedQuad(m_shaderTrans, views, (uint32_t)std::size(views), context.Sampler(false));
}

}
