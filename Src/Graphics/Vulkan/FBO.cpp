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

#include "FBO.h"

#include "Supermodel.h"

bool FBO::Create(int width, int height)
{
	Destroy();

	// Colour only, exactly like the GL original: nothing that draws into this
	// target uses depth (Render2D disables the depth test, and the 3D renderer
	// composites its own depth-tested passes onto it).
	m_target = Vulkan::VulkanContext::Get().CreateTarget((uint32_t)width, (uint32_t)height, 1, false);
	return m_target != nullptr;
}

void FBO::Destroy()
{
	if (m_target != nullptr)
	{
		Vulkan::VulkanContext::Get().DestroyTarget(m_target);
		m_target = nullptr;
	}
}

void FBO::BindTexture()
{
	// Nothing to do: Vulkan reads this target's image through a descriptor set
	// written at draw time, not through a bound texture name.
}

void FBO::Set()
{
	Vulkan::VulkanContext::Get().BindTarget(m_target);
}

void FBO::Disable()
{
	BindSupermodelDefaultFramebuffer();
}

Vulkan::Image *FBO::GetTextureID()
{
	if (m_target == nullptr || m_target->colour.empty())
		return nullptr;
	return &m_target->colour[0];
}
