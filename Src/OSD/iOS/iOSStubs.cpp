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
 * iOSStubs.cpp
 *
 * Backs the iOS SDL shim headers in Src/OSD/iOS/include/. Two very different
 * kinds of thing live here, and the difference matters:
 *
 *   1. THREADING IS A REAL IMPLEMENTATION, over pthreads. CThread, CMutex,
 *      CSemaphore and CCondVar (Src/OSD/Thread.h, implemented in
 *      Src/OSD/SDL/Thread.cpp) are used by CModel3, CSCSP and CInputSystem, so
 *      a failing stub would be a silent correctness bug. Semantics match the
 *      SDL2 functions Thread.cpp calls: 0 on success, -1 on failure.
 *
 *   2. SDL_net AND SDL_ShowSimpleMessageBox ARE INERT STUBS. The libretro core
 *      sets Network=false, so CSimNetBoard::Init returns before constructing
 *      any socket and none of the SDLNet_* entry points is reachable; they fail
 *      loudly (and log) rather than pretending to work. SDL_ShowSimpleMessageBox
 *      is only called from unreachable address-decode error paths in
 *      Src/Model3/Model3.cpp and Src/Network/NetBoard.cpp, and becomes a log
 *      line.
 *
 * Nothing here is graphics. The GL entry points the core still needs are
 * deliberately NOT stubbed -- see the GL seam in Makefiles/Makefile.iOS.
 */

#include "SDL.h"
#include "SDL_net.h"

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <new>
#include <pthread.h>
#include <string>
#include <unistd.h>

namespace
{
  thread_local std::string s_lastError;

  void SetLastError(const char *what)
  {
    s_lastError = what ? what : "";
  }
}

/******************************************************************************
 Timing
******************************************************************************/

extern "C" Uint32 SDL_GetTicks(void)
{
  using namespace std::chrono;
  static const steady_clock::time_point start = steady_clock::now();
  return (Uint32)duration_cast<milliseconds>(steady_clock::now() - start).count();
}

extern "C" void SDL_Delay(Uint32 ms)
{
  struct timespec ts;
  ts.tv_sec = (time_t)(ms / 1000u);
  ts.tv_nsec = (long)(ms % 1000u) * 1000000L;
  while (nanosleep(&ts, &ts) == -1 && errno == EINTR)
    ;
}

extern "C" const char *SDL_GetError(void)
{
  return s_lastError.c_str();
}

/******************************************************************************
 Threads
******************************************************************************/

struct SDL_Thread
{
  pthread_t handle;
  SDL_ThreadFunction fn;
  void *arg;
  int status;
};

static void *ThreadTrampoline(void *opaque)
{
  SDL_Thread *t = static_cast<SDL_Thread *>(opaque);
  t->status = t->fn(t->arg);
  return nullptr;
}

extern "C" SDL_Thread *SDL_CreateThread(SDL_ThreadFunction fn, const char *name, void *data)
{
  (void)name;   // pthread_setname_np can only name the *calling* thread on Apple
  SDL_Thread *t = new (std::nothrow) SDL_Thread;
  if (t == nullptr)
  {
    SetLastError("out of memory");
    return nullptr;
  }
  t->fn = fn;
  t->arg = data;
  t->status = 0;
  if (pthread_create(&t->handle, nullptr, ThreadTrampoline, t) != 0)
  {
    SetLastError(std::strerror(errno));
    delete t;
    return nullptr;
  }
  return t;
}

extern "C" void SDL_WaitThread(SDL_Thread *thread, int *status)
{
  if (thread == nullptr)
    return;
  pthread_join(thread->handle, nullptr);
  if (status != nullptr)
    *status = thread->status;
  delete thread;
}

extern "C" SDL_threadID SDL_GetThreadID(SDL_Thread *thread)
{
  uint64_t id = 0;
  if (thread == nullptr)
    pthread_threadid_np(nullptr, &id);
  else
    pthread_threadid_np(thread->handle, &id);
  return (SDL_threadID)id;
}

/******************************************************************************
 Mutexes
******************************************************************************/

struct SDL_mutex
{
  pthread_mutex_t handle;
};

extern "C" SDL_mutex *SDL_CreateMutex(void)
{
  SDL_mutex *m = new (std::nothrow) SDL_mutex;
  if (m == nullptr)
  {
    SetLastError("out of memory");
    return nullptr;
  }
  // SDL2 mutexes are recursive.
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  int rc = pthread_mutex_init(&m->handle, &attr);
  pthread_mutexattr_destroy(&attr);
  if (rc != 0)
  {
    SetLastError(std::strerror(rc));
    delete m;
    return nullptr;
  }
  return m;
}

extern "C" void SDL_DestroyMutex(SDL_mutex *mutex)
{
  if (mutex == nullptr)
    return;
  pthread_mutex_destroy(&mutex->handle);
  delete mutex;
}

extern "C" int SDL_mutexP(SDL_mutex *mutex)
{
  if (mutex == nullptr)
    return -1;
  return pthread_mutex_lock(&mutex->handle) == 0 ? 0 : -1;
}

extern "C" int SDL_mutexV(SDL_mutex *mutex)
{
  if (mutex == nullptr)
    return -1;
  return pthread_mutex_unlock(&mutex->handle) == 0 ? 0 : -1;
}

/******************************************************************************
 Semaphores

 pthreads' unnamed sem_* are deprecated and non-functional on Apple platforms
 (sem_init returns ENOSYS), so this is a condition-variable counting semaphore.
******************************************************************************/

struct SDL_sem
{
  pthread_mutex_t lock;
  pthread_cond_t  cond;
  Uint32          count;
};

extern "C" SDL_sem *SDL_CreateSemaphore(Uint32 initial_value)
{
  SDL_sem *s = new (std::nothrow) SDL_sem;
  if (s == nullptr)
  {
    SetLastError("out of memory");
    return nullptr;
  }
  s->count = initial_value;
  if (pthread_mutex_init(&s->lock, nullptr) != 0 || pthread_cond_init(&s->cond, nullptr) != 0)
  {
    SetLastError("failed to create semaphore");
    delete s;
    return nullptr;
  }
  return s;
}

extern "C" void SDL_DestroySemaphore(SDL_sem *sem)
{
  if (sem == nullptr)
    return;
  pthread_cond_destroy(&sem->cond);
  pthread_mutex_destroy(&sem->lock);
  delete sem;
}

extern "C" int SDL_SemWait(SDL_sem *sem)
{
  if (sem == nullptr)
    return -1;
  pthread_mutex_lock(&sem->lock);
  while (sem->count == 0)
    pthread_cond_wait(&sem->cond, &sem->lock);
  --sem->count;
  pthread_mutex_unlock(&sem->lock);
  return 0;
}

extern "C" int SDL_SemPost(SDL_sem *sem)
{
  if (sem == nullptr)
    return -1;
  pthread_mutex_lock(&sem->lock);
  ++sem->count;
  pthread_cond_signal(&sem->cond);
  pthread_mutex_unlock(&sem->lock);
  return 0;
}

extern "C" Uint32 SDL_SemValue(SDL_sem *sem)
{
  if (sem == nullptr)
    return 0;
  pthread_mutex_lock(&sem->lock);
  Uint32 value = sem->count;
  pthread_mutex_unlock(&sem->lock);
  return value;
}

/******************************************************************************
 Condition variables
******************************************************************************/

struct SDL_cond
{
  pthread_cond_t handle;
};

extern "C" SDL_cond *SDL_CreateCond(void)
{
  SDL_cond *c = new (std::nothrow) SDL_cond;
  if (c == nullptr)
  {
    SetLastError("out of memory");
    return nullptr;
  }
  if (pthread_cond_init(&c->handle, nullptr) != 0)
  {
    SetLastError("failed to create condition variable");
    delete c;
    return nullptr;
  }
  return c;
}

extern "C" void SDL_DestroyCond(SDL_cond *cond)
{
  if (cond == nullptr)
    return;
  pthread_cond_destroy(&cond->handle);
  delete cond;
}

extern "C" int SDL_CondWait(SDL_cond *cond, SDL_mutex *mutex)
{
  if (cond == nullptr || mutex == nullptr)
    return -1;
  return pthread_cond_wait(&cond->handle, &mutex->handle) == 0 ? 0 : -1;
}

extern "C" int SDL_CondSignal(SDL_cond *cond)
{
  if (cond == nullptr)
    return -1;
  return pthread_cond_signal(&cond->handle) == 0 ? 0 : -1;
}

extern "C" int SDL_CondBroadcast(SDL_cond *cond)
{
  if (cond == nullptr)
    return -1;
  return pthread_cond_broadcast(&cond->handle) == 0 ? 0 : -1;
}

/******************************************************************************
 Message boxes -- a libretro core has no window to attach one to.
******************************************************************************/

extern "C" int SDL_ShowSimpleMessageBox(Uint32 flags, const char *title,
                                        const char *message, SDL_Window *window)
{
  (void)flags;
  (void)window;
  std::fprintf(stderr, "Supermodel: %s: %s\n", title ? title : "(no title)",
               message ? message : "(no message)");
  return 0;
}

/******************************************************************************
 SDL_net -- unreachable in the libretro core (Network=false). Fail loudly.
******************************************************************************/

namespace
{
  int NetUnavailable(const char *fn)
  {
    SetLastError("SDL_net is not available in the iOS libretro build");
    std::fprintf(stderr, "Supermodel: %s called, but the iOS build has no netboard networking.\n", fn);
    return -1;
  }
}

extern "C" int  SDLNet_Init(void)  { return NetUnavailable("SDLNet_Init"); }
extern "C" void SDLNet_Quit(void)  { }
extern "C" const char *SDLNet_GetError(void) { return SDL_GetError(); }

extern "C" int SDLNet_ResolveHost(IPaddress *address, const char *host, Uint16 port)
{
  (void)host;
  if (address != nullptr)
  {
    address->host = 0;
    address->port = port;
  }
  return NetUnavailable("SDLNet_ResolveHost");
}

extern "C" TCPsocket SDLNet_TCP_Open(IPaddress *ip)
{
  (void)ip;
  NetUnavailable("SDLNet_TCP_Open");
  return nullptr;
}

extern "C" TCPsocket SDLNet_TCP_Accept(TCPsocket server)
{
  (void)server;
  return nullptr;
}

extern "C" int SDLNet_TCP_Send(TCPsocket sock, const void *data, int len)
{
  (void)sock; (void)data; (void)len;
  return NetUnavailable("SDLNet_TCP_Send");
}

extern "C" int SDLNet_TCP_Recv(TCPsocket sock, void *data, int maxlen)
{
  (void)sock; (void)data; (void)maxlen;
  return NetUnavailable("SDLNet_TCP_Recv");
}

extern "C" void SDLNet_TCP_Close(TCPsocket sock) { (void)sock; }

extern "C" SDLNet_SocketSet SDLNet_AllocSocketSet(int maxsockets)
{
  (void)maxsockets;
  NetUnavailable("SDLNet_AllocSocketSet");
  return nullptr;
}

extern "C" int SDLNet_AddSocket(SDLNet_SocketSet set, SDLNet_GenericSocket sock)
{
  (void)set; (void)sock;
  return NetUnavailable("SDLNet_AddSocket");
}

extern "C" int SDLNet_DelSocket(SDLNet_SocketSet set, SDLNet_GenericSocket sock)
{
  (void)set; (void)sock;
  return NetUnavailable("SDLNet_DelSocket");
}

extern "C" void SDLNet_FreeSocketSet(SDLNet_SocketSet set) { (void)set; }

extern "C" int SDLNet_CheckSockets(SDLNet_SocketSet set, Uint32 timeout)
{
  (void)set; (void)timeout;
  return -1;
}
