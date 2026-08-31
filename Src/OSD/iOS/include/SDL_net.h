/*
 * SDL_net.h (iOS shim)
 *
 * NOT SDL_net. Src/Network/{TCPSend,TCPReceive,NetBoard,SimNetBoard}.cpp are
 * kept in the iOS build so that Src/Model3/Model3.cpp needs no edits -- it
 * constructs a CSimNetBoard unconditionally. Nothing here is ever called: the
 * libretro core sets Network=false (Src/OSD/SDL/LibRetro.cpp), so
 * CSimNetBoard::Init leaves m_attached false and returns before any socket is
 * created, and CModel3's m_runNetBoard stays false. The implementations in
 * Src/OSD/iOS/iOSStubs.cpp therefore fail loudly rather than silently.
 */
#ifndef INCLUDED_SUPERMODEL_IOS_SDL_NET_SHIM_H
#define INCLUDED_SUPERMODEL_IOS_SDL_NET_SHIM_H

#include "SDL.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { Uint32 host; Uint16 port; } IPaddress;

typedef struct _TCPsocket        *TCPsocket;
typedef struct _SDLNet_SocketSet *SDLNet_SocketSet;
typedef struct _SDLNet_Socket    *SDLNet_GenericSocket;

extern int  SDLNet_Init(void);
extern void SDLNet_Quit(void);
extern const char *SDLNet_GetError(void);

extern int       SDLNet_ResolveHost(IPaddress *address, const char *host, Uint16 port);
extern TCPsocket SDLNet_TCP_Open(IPaddress *ip);
extern TCPsocket SDLNet_TCP_Accept(TCPsocket server);
extern int       SDLNet_TCP_Send(TCPsocket sock, const void *data, int len);
extern int       SDLNet_TCP_Recv(TCPsocket sock, void *data, int maxlen);
extern void      SDLNet_TCP_Close(TCPsocket sock);

extern SDLNet_SocketSet SDLNet_AllocSocketSet(int maxsockets);
extern int              SDLNet_AddSocket(SDLNet_SocketSet set, SDLNet_GenericSocket sock);
extern int              SDLNet_DelSocket(SDLNet_SocketSet set, SDLNet_GenericSocket sock);
extern void             SDLNet_FreeSocketSet(SDLNet_SocketSet set);
extern int              SDLNet_CheckSockets(SDLNet_SocketSet set, Uint32 timeout);

#ifdef __cplusplus
}
#endif

#endif
