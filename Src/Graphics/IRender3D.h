#ifndef INCLUDED_IRENDER3D_H
#define INCLUDED_IRENDER3D_H

#include <cstdint>
#include "Types.h"

/*
 * AATarget:
 *
 * What SuperAA::GetTargetID() hands to a renderer's Init() to say "draw here
 * instead of the default framebuffer", and what a renderer stores until it is
 * ready to bind it. Under OpenGL that is a framebuffer name, and 0 means the
 * default framebuffer; under Vulkan it is the render target itself, and
 * nullptr means the screen target. Both spellings are a value the renderer only
 * ever tests for truth and passes back, so the seam is this typedef and the
 * `unsigned` spelling below is deliberately GLuint's underlying type -- nothing
 * here may include a GL header.
 */
#ifdef SUPERMODEL_VULKAN
namespace Vulkan { class RenderTarget; }
typedef Vulkan::RenderTarget *AATarget;
#else
typedef unsigned AATarget;
#endif

/*
 * IRender3D:
 *
 * Interface (abstract base class) for Real3D rendering engine.
 */
class IRender3D
{
public:
  virtual void RenderFrame(void) = 0;
  virtual void BeginFrame(void) = 0;
  virtual void EndFrame(void) = 0;
  virtual void UploadTextures(unsigned level, unsigned x, unsigned y, unsigned width, unsigned height) = 0;
  virtual void AttachMemory(const uint32_t *cullingRAMLoPtr, const uint32_t *cullingRAMHiPtr, const uint32_t *polyRAMPtr, const uint32_t *vromPtr, const uint16_t *textureRAMPtr) = 0;
  virtual void SetStepping(int stepping) = 0;
  virtual Result Init(unsigned xOffset, unsigned yOffset, unsigned xRes, unsigned yRes, unsigned totalXRes, unsigned totalYRes, AATarget aaTarget) = 0;
  virtual void SetSunClamp(bool enable) = 0;
  virtual void SetBlockCulling(bool enable) = 0;
  virtual float GetLosValue(int layer) = 0;

  virtual ~IRender3D()
  {
  }
};

#endif  // INCLUDED_IRENDER3D_H
