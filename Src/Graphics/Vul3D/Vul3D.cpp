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

#include "Vul3D.h"

namespace Vul3D {

CVul3D::CVul3D(const Util::Config::Node &config, const std::string &gameName) :
	m_config(config),
	m_gameName(gameName)
{
	InfoLog("Vul3D: 3D rendering is not implemented yet (SM-04 unit B); the tile layers will render alone.\n");

	// Metal has no geometry stage, so this port does not implement quads at all
	// and the option is ignored rather than silently half-honoured.
	if (m_config["QuadRendering"].ValueAs<bool>())
		InfoLog("Vul3D: QuadRendering is not supported by the Vulkan renderer; ignoring it.\n");
}

CVul3D::~CVul3D(void)
{
}

Result CVul3D::Init(unsigned xOffset, unsigned yOffset, unsigned xRes, unsigned yRes, unsigned totalXRes, unsigned totalYRes, AATarget aaTarget)
{
	m_xOffset	= xOffset;
	m_yOffset	= yOffset;
	m_xRes		= xRes;
	m_yRes		= yRes;
	m_totalXRes	= totalXRes;
	m_totalYRes	= totalYRes;
	m_aaTarget	= aaTarget;
	return Result::OKAY;
}

void CVul3D::RenderFrame(void)
{
	// Unit B: the port of New3D::CNew3D::RenderFrame.
}

void CVul3D::BeginFrame(void)
{
}

void CVul3D::EndFrame(void)
{
}

void CVul3D::UploadTextures(unsigned level, unsigned x, unsigned y, unsigned width, unsigned height)
{
}

void CVul3D::AttachMemory(const uint32_t *cullingRAMLoPtr, const uint32_t *cullingRAMHiPtr, const uint32_t *polyRAMPtr, const uint32_t *vromPtr, const uint16_t *textureRAMPtr)
{
	m_cullingRAMLo	= cullingRAMLoPtr;
	m_cullingRAMHi	= cullingRAMHiPtr;
	m_polyRAM		= polyRAMPtr;
	m_vrom			= vromPtr;
	m_textureRAM	= textureRAMPtr;
}

void CVul3D::SetStepping(int stepping)
{
	m_step = stepping;
}

void CVul3D::SetSunClamp(bool enable)
{
	m_sunClamp = enable;
}

void CVul3D::SetBlockCulling(bool enable)
{
	m_blockCulling = enable;
}

float CVul3D::GetLosValue(int layer)
{
	return 0.0f;
}

} // namespace Vul3D
