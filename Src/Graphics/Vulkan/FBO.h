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
 * Src/Graphics/Vulkan/FBO.h
 *
 * The Vulkan mirror of Src/Graphics/FBO.h: a single-colour-attachment offscreen
 * target. Same members and same names; only the two return types change, from a
 * GLuint framebuffer name and a GLuint texture name to a RenderTarget* and the
 * colour Image itself, because in Vulkan a shader reads an image through a
 * descriptor rather than through a bound name.
 *
 * BindTexture, Set and Disable are unreferenced -- in the GL original too, where
 * SuperAA is the only user and calls only Create/Destroy/GetFBOID/GetTextureID.
 * They are mirrored anyway so the two classes stay the same class.
 */

#ifndef INCLUDED_VULKAN_FBO_H
#define INCLUDED_VULKAN_FBO_H

#include "VulkanContext.h"

class FBO
{
public:

	FBO() = default;
	~FBO() = default;

	bool	Create(int width, int height);
	void	Destroy();
	void	BindTexture();				// nothing to bind: see the note above
	void	Set();
	void	Disable();

	Vulkan::RenderTarget	*GetFBOID()		{ return m_target; }
	Vulkan::Image			*GetTextureID();

private:

	Vulkan::RenderTarget *m_target = nullptr;
};

#endif	// INCLUDED_VULKAN_FBO_H
