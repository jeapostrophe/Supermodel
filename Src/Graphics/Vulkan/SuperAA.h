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
 * Src/Graphics/Vulkan/SuperAA.h
 *
 * The Vulkan mirror of Src/Graphics/SuperAA.h. Same class, same public members,
 * same "this is a no-op unless aa > 1 or CRT colours are on" rule.
 *
 * GetTargetID() returns an AATarget -- under Vulkan a RenderTarget* rather than
 * a GLuint -- and nullptr, like GL's 0, means "there is no AA target, draw
 * straight at the screen".
 */

#pragma once

#include "Supermodel.h"
#include "Graphics/IRender3D.h"
#include "FBO.h"
#include "VulkanShader.h"

class SuperAA
{
public:
	SuperAA(int aaValue, CRTcolor CRTcolors);
	~SuperAA();

	void Init(int width, int height);		// width & height are real window dimensions
	void SetOutputSize(int width, int height);
	void Draw();							// no-op if AA is 1 and CRTcolors 0: everything already drew at the screen

	AATarget GetTargetID();

private:
	bool Enabled() const { return (m_aa > 1) || (m_crtcolors != CRTcolor::None); }

	FBO					m_fbo;
	const int			m_aa;
	const CRTcolor		m_crtcolors;
	int					m_width			= 0;
	int					m_height		= 0;
	int					m_outputWidth	= 0;
	int					m_outputHeight	= 0;

	Vulkan::ShaderProgram	m_program;
};
