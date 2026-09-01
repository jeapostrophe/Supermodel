#ifndef FBO_H
#define FBO_H

#include "Supermodel.h"
#include "Graphics/New3D/Model.h"
#include "Graphics/Vulkan/VulkanContext.h"
#include "Graphics/Vulkan/VulkanShader.h"

namespace Vul3D {

using New3D::Layer;

class R3DFrameBuffers {

public:
	R3DFrameBuffers();
	~R3DFrameBuffers();

	void	Draw();					// draw and composite the transparent layers

	// `dest` is where Draw() composites: the AA target, or the screen. The two
	// composite pipelines are built against its render pass, which is why it is
	// a parameter here and was nothing at all in GL.
	Result	CreateFBO(int width, int height, Vulkan::RenderTarget* dest);
	void	DestroyFBO();

	void	BindTexture(Layer layer);
	void	SetFBO(Layer layer);
	void	StoreDepth();
	void	RestoreDepth();

	// The MRT target, for the pipelines R3DShader builds against its render
	// pass, and its depth/stencil image, which CVul3D::ProcessLos reads back.
	Vulkan::RenderTarget*	GetTarget()		{ return m_target; }
	Vulkan::Image*			GetDepthImage()	{ return m_target != nullptr ? &m_target->depth : nullptr; }

private:

	Result	CreateFBODepthCopy(int width, int height);
	void	AllocShaderTrans();
	void	AllocShaderBase();

	void	DrawBaseLayer();
	void	DrawAlphaLayer();

	Vulkan::RenderTarget*	m_target;		// 3 colour attachments + D32S8, the glDrawBuffers MRT
	Vulkan::RenderTarget*	m_dest;			// where Draw() composites
	Vulkan::Image			m_depthCopy;	// the second FBO in the GL original, which only ever held a depth/stencil renderbuffer
	Layer m_lastLayer;
	int m_width;
	int m_height;

	// shaders
	Vulkan::ShaderProgram m_shaderBase;
	Vulkan::ShaderProgram m_shaderTrans;
};

}

#endif
