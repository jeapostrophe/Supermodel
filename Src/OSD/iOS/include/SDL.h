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
 * SDL.h (iOS shim)
 *
 * NOT SDL. This is a stand-in for the handful of SDL2 declarations that survive
 * into the iOS libretro build, reached only via Src/OSD/SDL/SDLIncludes.h. It
 * exists so that no Supermodel source file has to be edited to drop SDL, and it
 * is deliberately tiny: if a translation unit needs more of SDL than is
 * declared here, that translation unit does not belong in the iOS build and
 * should be added to EXCLUDE_SRC_FILES in Makefiles/Makefile.iOS.
 *
 * The surviving users are:
 *   Src/OSD/SDL/Thread.cpp   -- SDL threads/mutexes/semaphores/condvars, which
 *                               Src/OSD/iOS/iOSStubs.cpp implements over
 *                               pthreads (a real implementation, not a stub:
 *                               CThread is used by Model3, SCSP and
 *                               InputSystem).
 *   Src/Model3/Model3.cpp    -- SDL_ShowSimpleMessageBox on unreachable
 *   Src/Network/NetBoard.cpp    address-decode error paths.
 */

#ifndef INCLUDED_SUPERMODEL_IOS_SDL_SHIM_H
#define INCLUDED_SUPERMODEL_IOS_SDL_SHIM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t  Uint8;
typedef int8_t   Sint8;
typedef uint16_t Uint16;
typedef int16_t  Sint16;
typedef uint32_t Uint32;
typedef int32_t  Sint32;
typedef uint64_t Uint64;
typedef int64_t  Sint64;

/* Opaque handles. */
typedef struct SDL_Thread SDL_Thread;
typedef struct SDL_mutex  SDL_mutex;
typedef struct SDL_cond   SDL_cond;
typedef struct SDL_sem    SDL_sem;
typedef struct SDL_Window SDL_Window;

typedef unsigned long SDL_threadID;

typedef int (*SDL_ThreadFunction)(void *data);

/* Timing */
extern Uint32      SDL_GetTicks(void);
extern void        SDL_Delay(Uint32 ms);
extern const char *SDL_GetError(void);

/* Threads */
extern SDL_Thread  *SDL_CreateThread(SDL_ThreadFunction fn, const char *name, void *data);
extern void         SDL_WaitThread(SDL_Thread *thread, int *status);
extern SDL_threadID SDL_GetThreadID(SDL_Thread *thread);

/* Mutexes */
extern SDL_mutex *SDL_CreateMutex(void);
extern void       SDL_DestroyMutex(SDL_mutex *mutex);
extern int        SDL_mutexP(SDL_mutex *mutex);   /* lock   */
extern int        SDL_mutexV(SDL_mutex *mutex);   /* unlock */

/* Semaphores */
extern SDL_sem *SDL_CreateSemaphore(Uint32 initial_value);
extern void     SDL_DestroySemaphore(SDL_sem *sem);
extern int      SDL_SemWait(SDL_sem *sem);
extern int      SDL_SemPost(SDL_sem *sem);
extern Uint32   SDL_SemValue(SDL_sem *sem);

/* Condition variables */
extern SDL_cond *SDL_CreateCond(void);
extern void      SDL_DestroyCond(SDL_cond *cond);
extern int       SDL_CondWait(SDL_cond *cond, SDL_mutex *mutex);
extern int       SDL_CondSignal(SDL_cond *cond);
extern int       SDL_CondBroadcast(SDL_cond *cond);

/* Message boxes -- there is no such thing on a libretro core; the shim logs. */
#define SDL_MESSAGEBOX_ERROR       0x00000010u
#define SDL_MESSAGEBOX_WARNING     0x00000020u
#define SDL_MESSAGEBOX_INFORMATION 0x00000040u

extern int SDL_ShowSimpleMessageBox(Uint32 flags, const char *title,
                                    const char *message, SDL_Window *window);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDED_SUPERMODEL_IOS_SDL_SHIM_H */
