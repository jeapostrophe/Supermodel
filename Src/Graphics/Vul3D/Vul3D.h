// PLAN-ipad SM-04 unit B replaces this stub.
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
 * Vul3D.h
 *
 * The Vulkan Real3D renderer, mirroring Src/Graphics/New3D. THIS IS A STUB:
 * unit A built the foundation, the makefile seam, the 2D layer and the libretro
 * wiring around it, and unit B replaces the body with the port of New3D.cpp.
 * Everything here does nothing except remember the sizes Init was given.
 *
 * The stub uses none of the New3D includes below. They are here so that unit B
 * starts from an include set already known to compile, and to state the reuse
 * rule while the file is still small enough to read: Mat4, Vec, Model,
 * PolyHeader, R3DData, R3DFloat and Plane are reused by #include, not copied and
 * not abstracted, because they contain no GL. Their translation units stay in
 * the build under RENDER_BACKEND=vulkan for the same reason.
 *
 * Note what does and does not check that. Including them here only shows they
 * compile -- Src/Pkgs is on the include path, so <GL/glew.h> would still resolve
 * if one of them reached for it. The claim is checked at the LINK, by the
 * `nm -u ... | grep -c "_gl\|_vk"` gate in README.md: Mat4.o, Model.o,
 * PolyHeader.o, Vec.o and R3DFloat.o are all in that artefact, so a GL call in
 * any of them would appear there as an undefined symbol.
 */

#ifndef INCLUDED_VUL3D_VUL3D_H
#define INCLUDED_VUL3D_VUL3D_H

#include <string>

#include "Supermodel.h"
#include "Types.h"
#include "Util/NewConfig.h"
#include "Graphics/IRender3D.h"

// New3D's GL-free data types, reused verbatim.
#include "Graphics/New3D/Model.h"
#include "Graphics/New3D/Mat4.h"
#include "Graphics/New3D/Vec.h"
#include "Graphics/New3D/Plane.h"
#include "Graphics/New3D/PolyHeader.h"
#include "Graphics/New3D/R3DData.h"
#include "Graphics/New3D/R3DFloat.h"

#include "Graphics/Vulkan/VulkanContext.h"
#include "Graphics/Vulkan/VulkanShader.h"

namespace Vul3D {

class CVul3D final : public IRender3D
{
public:
	CVul3D(const Util::Config::Node &config, const std::string &gameName);
	~CVul3D(void);

	void	RenderFrame(void) override;
	void	BeginFrame(void) override;
	void	EndFrame(void) override;
	void	UploadTextures(unsigned level, unsigned x, unsigned y, unsigned width, unsigned height) override;
	void	AttachMemory(const uint32_t *cullingRAMLoPtr, const uint32_t *cullingRAMHiPtr, const uint32_t *polyRAMPtr, const uint32_t *vromPtr, const uint16_t *textureRAMPtr) override;
	void	SetStepping(int stepping) override;
	Result	Init(unsigned xOffset, unsigned yOffset, unsigned xRes, unsigned yRes, unsigned totalXRes, unsigned totalYRes, AATarget aaTarget) override;
	void	SetSunClamp(bool enable) override;
	void	SetBlockCulling(bool enable) override;
	float	GetLosValue(int layer) override;

private:
	const Util::Config::Node	&m_config;
	std::string					m_gameName;

	unsigned	m_xOffset		= 0;
	unsigned	m_yOffset		= 0;
	unsigned	m_xRes			= 0;
	unsigned	m_yRes			= 0;
	unsigned	m_totalXRes		= 0;
	unsigned	m_totalYRes		= 0;
	AATarget	m_aaTarget		= nullptr;

	int			m_step			= 0;
	bool		m_sunClamp		= true;
	bool		m_blockCulling	= true;

	const uint32_t	*m_cullingRAMLo	= nullptr;
	const uint32_t	*m_cullingRAMHi	= nullptr;
	const uint32_t	*m_polyRAM		= nullptr;
	const uint32_t	*m_vrom			= nullptr;
	const uint16_t	*m_textureRAM	= nullptr;
};

} // namespace Vul3D

#endif	// INCLUDED_VUL3D_VUL3D_H
