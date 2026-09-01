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

#ifndef INCLUDED_VUL3D_R3DSCROLLFOG_H
#define INCLUDED_VUL3D_R3DSCROLLFOG_H

#include "Graphics/Vulkan/VulkanContext.h"
#include "Graphics/Vulkan/VulkanShader.h"

namespace Vul3D {

	class R3DScrollFog
	{
	public:

		R3DScrollFog();
		~R3DScrollFog();

		// The pipeline needs the render pass it draws into, which does not
		// exist until R3DFrameBuffers has made its MRT target -- so the
		// constructor cannot build it and CVul3D::Init calls this instead.
		bool CreatePipeline(Vulkan::RenderTarget* target);

		void DrawScrollFog(float rbga[4], float attenuation, float ambient, float spotRGB[3], float spotEllipse[4]);

		// std140 mirror of R3DScrollFog.frag's ScrollFogBlock. Public only so
		// that the .cpp's static_asserts can take offsetof of its members;
		// nothing outside builds one.
		struct ScrollFogBlock
		{
			float	fogColour[4];
			float	spotEllipse[4];
			float	spotFogColor[3];
			float	fogAttenuation;
			float	fogAmbient;
			float	pad[3];
		};

	private:

		void AllocResources();
		void DeallocResources();

		Vulkan::ShaderProgram m_program;
	};

}

#endif
