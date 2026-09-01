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
 * Src/Graphics/Vulkan/Render2D.h
 *
 * The Vulkan mirror of Src/Graphics/Render2D.h: same class, same public
 * members, same meanings -- the long description of the tile generator
 * hardware lives in the GL header and .cpp and is not repeated here.
 *
 * The only signature that differs is Init's aaTarget, which is an AATarget
 * (a RenderTarget*, nullptr meaning "the screen") rather than a GLuint
 * framebuffer name. See Src/Graphics/IRender3D.h.
 */

#ifndef INCLUDED_VULKAN_RENDER2D_H
#define INCLUDED_VULKAN_RENDER2D_H

#include <memory>

#include "Supermodel.h"
#include "Util/NewConfig.h"
#include "Graphics/IRender3D.h"
#include "Model3/TileGenBuffer.h"
#include "VulkanContext.h"
#include "VulkanShader.h"

class CRender2D
{
public:
	void BeginFrame(void);
	void PreRenderFrame(void);
	void RenderFrameBottom(void);
	void RenderFrameTop(void);
	void EndFrame(void);

	void WriteVRAM(unsigned addr, uint32_t data);
	void AttachRegisters(const uint32_t* regPtr);
	void AttachPalette(const uint32_t* palPtr[2]);
	void AttachVRAM(const uint8_t* vramPtr);
	void AttachDrawBuffers(std::shared_ptr<TileGenBuffer> bottom, std::shared_ptr<TileGenBuffer> top);

	Result Init(unsigned xOffset, unsigned yOffset, unsigned xRes, unsigned yRes, unsigned totalXRes, unsigned totalYRes, AATarget aaTarget, UpscaleMode upscaleMode);

	CRender2D(const Util::Config::Node& config);
	~CRender2D(void);

private:

	void	Setup2D		(bool isBottom);
	void	DrawSurface	(Vulkan::Image &texture);
	void	Destroy		(void);

	// Run-time configuration
	const Util::Config::Node& m_config;

	unsigned  m_xPixels = 496;  // display surface resolution
	unsigned  m_yPixels = 384;  // ...
	unsigned  m_xOffset = 0;    // offset
	unsigned  m_yOffset = 0;
	unsigned  m_totalXPixels = 0;   // total display surface resolution
	unsigned  m_totalYPixels = 0;
	unsigned  m_correction = 0;
	AATarget  m_aaTarget = nullptr;
	UpscaleMode m_upscaleMode = UpscaleMode::Biquintic;

	Vulkan::Image			m_textures[2];
	Vulkan::ShaderProgram	m_program;

	std::shared_ptr<TileGenBuffer> m_drawBuffers[2];
};

#endif  // INCLUDED_VULKAN_RENDER2D_H
