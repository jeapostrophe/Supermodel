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
 * iOSOSD.cpp
 *
 * The OSD entry points that Src/OSD/SDL/{Main,Audio,Crosshair}.cpp provide on
 * the desktop, supplied here for the iOS libretro build, which excludes those
 * three translation units (they need SDL2, GLUT and Cocoa).
 *
 * Each definition below is justified against the *libretro* code path, not the
 * desktop one:
 *
 *   BeginFrameVideo / EndFrameVideo -- called by CModel3::RenderFrame. Main.cpp
 *     returns true and then calls SDL_GL_SwapWindow(s_window). In a libretro
 *     core there is no SDL window (s_window is null there) and presentation is
 *     the frontend's job via retro_video_refresh, so the correct iOS bodies are
 *     "true" and "nothing".
 *
 *   SetAudioCallback -- called only from CModel3::StartThreads /
 *     CModel3::StopThreads, which run only when the MultiThreaded config option
 *     is set. Src/OSD/SDL/LibRetro.cpp sets MultiThreaded and GPUMultiThreaded
 *     to false, so this is link-required and call-unreachable. It is a no-op
 *     rather than an error so that a future multithreaded libretro build fails
 *     by going silent rather than by aborting -- but if MultiThreaded is ever
 *     turned on for iOS, THIS is the function that must grow a real body.
 *
 *   CCrosshair -- LibRetro.cpp constructs one only inside InitializeCrosshair,
 *     which returns early unless IsGunGame() -- i.e. unless the loaded game
 *     declares INPUT_GUN1/2 or INPUT_ANALOG_GUN1/2. The real implementation
 *     needs SDL_LoadBMP and SDL_GetDisplayDPI. So on iOS the class exists to
 *     satisfy the linker and Init() fails: a gun game gets no native crosshair
 *     (LibRetro.cpp logs "Crosshair initialization failed" and continues with
 *     its own pointer overlay, which is separate code and unaffected).
 *     Spikeout: Final Edition (spikeofe), the game this port exists for, is not
 *     a gun game -- Config/Games.xml gives it common/joystick1/spikeout inputs
 *     -- so it never reaches this code at all.
 */

#include "Supermodel.h"
#include "OSD/Audio.h"
#include "OSD/Video.h"
#ifndef SUPERMODEL_VULKAN
#include "OSD/SDL/Crosshair.h"
#endif

/******************************************************************************
 OSD/Video.h
******************************************************************************/

bool BeginFrameVideo()
{
  return true;
}

void EndFrameVideo()
{
  // Nothing: the libretro frontend presents the frame.
}

/******************************************************************************
 OSD/Audio.h
******************************************************************************/

void SetAudioCallback(AudioCallbackFPtr newCallback, void *newData)
{
  (void)newCallback;
  (void)newData;
}

/******************************************************************************
 OSD/SDL/Crosshair.h

 Only under RENDER_BACKEND=opengl. The Vulkan backend compiles the crosshair out
 of Src/OSD/SDL/LibRetro.cpp entirely, so nothing names CCrosshair -- and
 defining it here would drag Crosshair.h's #include of New3D.h, and with it GLEW,
 into a build that has neither.
******************************************************************************/

#ifndef SUPERMODEL_VULKAN

CCrosshair::CCrosshair(const Util::Config::Node &config)
  : m_config(config),
    m_vertexShader(nullptr),
    m_fragmentShader(nullptr)
{
}

CCrosshair::~CCrosshair()
{
}

Result CCrosshair::Init()
{
  ErrorLog("The iOS build has no native crosshair (it needs SDL2).\n");
  return Result::FAIL;
}

void CCrosshair::Update(uint32_t currentInputs, CInputs *inputs, unsigned int xOffset,
                        unsigned int yOffset, unsigned int xRes, unsigned int yRes)
{
  (void)currentInputs;
  (void)inputs;
  (void)xOffset;
  (void)yOffset;
  (void)xRes;
  (void)yRes;
}

#endif	// !SUPERMODEL_VULKAN
