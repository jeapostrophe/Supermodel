/*
 * SDL_audio.h (iOS shim)
 *
 * Intentionally empty. Src/OSD/SDL/SDLIncludes.h includes it unconditionally,
 * but the only translation unit that used SDL audio -- Src/OSD/SDL/Audio.cpp --
 * is excluded from the iOS build (the libretro core hands PCM to the frontend
 * through retro_audio_sample_batch instead). See Makefiles/Makefile.iOS.
 */
#ifndef INCLUDED_SUPERMODEL_IOS_SDL_AUDIO_SHIM_H
#define INCLUDED_SUPERMODEL_IOS_SDL_AUDIO_SHIM_H
#include "SDL.h"
#endif
