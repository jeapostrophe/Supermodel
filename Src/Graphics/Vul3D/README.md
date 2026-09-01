# Vul3D — a Vulkan render path for Supermodel's libretro core

Design brief, updated with what unit A measured while building it. Where the
brief and the code disagree the code wins, and the paragraphs marked
**"Correction"** are the places that happened.

Unit A is landed: the vendored headers, the `RENDER_BACKEND` seam, the
`VulkanContext`/`VulkanShader` foundation, the 2D layer, SuperAA, the pointer
overlay and the whole libretro wiring.

Unit B is landed: the 3D renderer itself, a per-file, per-function translation
of `Src/Graphics/New3D/`. Its review artefact is the diff against the original --
`diff -u Src/Graphics/New3D/New3D.cpp Src/Graphics/Vul3D/Vul3D.cpp` and the five
other pairs -- in which every hunk is a GL-to-Vulkan edit or a commented
insertion. (Normalise line endings first: the New3D sources are CRLF and the new
files LF, so an unnormalised diff says nothing.)

## Why

The libretro core renders through desktop OpenGL 4.1 (`RETRO_HW_CONTEXT_OPENGL_CORE`).
iOS has no desktop GL, its GLES stops at 3.0, and New3D's shaders need edits to
fit GLES (`textureBank[texturePage]` indexing, `precision`). The station that
motivates this port (retro-trainer on an iPad, `PLAN-ipad SM-04`) already serves
libretro's **Vulkan** interface to every other hardware core through MoltenVK,
on macOS and on iOS alike. A Vulkan path gives Supermodel one hardware render
path on both.

Measured facts the design leans on (see the two 2026-08-30 research notes):
- Only ~300 of New3D's 6511 lines touch GL. `Mat4`, `Vec`, `Model`,
  `PolyHeader`, `R3DData`, `R3DFloat`, `Plane` contain **zero** GL and are
  reused by `#include` — not copied, not abstracted.
- New3D's projection is reverse-Z with a constant clip z (`Mat4::FrustumRZ`
  sets `m[10]=0`, `m[14]=near`) and the fragment shader writes
  `gl_FragDepth = projMat[3][2] * gl_FragCoord.w` explicitly. Under Vulkan's
  `0 ≤ z ≤ w` clip rule that is the **same** clip volume and the same depth
  value, so no projection change and no depth remap is needed.
- Pipeline state space is tiny: depth compare GEQUAL (colour pass) or GREATER
  (transparency passes), depth write on, no blend in the mesh passes, no cull
  (back-face culling is emulated in the shader), two stencil configurations.
- At libretro defaults the quad path (geometry shader) is off; Metal has no
  geometry stage, so Vul3D does not implement `QuadRendering` at all.

## Coordinate convention — the one rule everything else follows

**Render exactly as GL does, into images whose row 0 is GL's bottom row, and
flip once at hand-off.** Every viewport, scissor, `gl_FragCoord.y` computation,
`Render2D`'s "flip upside down" texcoords and `TranslateLosPosition` stay
verbatim. Vulkan's viewport transform maps NDC y=-1 to framebuffer row 0, which
is exactly GL's mapping if row 0 is called "bottom". The only place the two
conventions meet is the final `vkCmdBlitImage` from the *screen* image into the
*presentation* image, which uses inverted source y offsets
(`srcOffsets[0].y = height, srcOffsets[1].y = 0`). RetroArch and retro-trainer
both take a Vulkan core's image as top-down (`bottom_left_origin` is not
consulted on the Vulkan path in either frontend).

Viewports are positive-height; no `VK_KHR_maintenance1` negative viewport is
used. Front-face winding is irrelevant (cull mode NONE everywhere).

## Layout

```
Src/Pkgs/vulkan/{vulkan.h,vulkan_core.h,vk_platform.h}   vendored Khronos headers (VK_NO_PROTOTYPES)
Src/Pkgs/vulkan/vk_video/*.h                             12 more, see the correction below
Src/Pkgs/libretro_vulkan.h                               libretro's Vulkan interface header
Src/Graphics/Vulkan/                                     the foundation + the non-3D GL users, mirrored
    VulkanContext.{h,cpp}   function table, device/queue, memory, per-frame command buffers,
                            render-target state machine, staging uploads, uniform ring
    VulkanShader.{h,cpp}    SPIR-V modules, descriptor-set layouts, pipeline building
                            (the GLSLShader analogue)
    FBO.{h,cpp}             mirror of Src/Graphics/FBO — an offscreen colour target
    Render2D.{h,cpp}        mirror of Src/Graphics/Render2D — class CRender2D
    SuperAA.{h,cpp}         mirror of Src/Graphics/SuperAA — class SuperAA
    Shaders/                GLSL 450 sources + generated *.spv.h (committed)
Src/Graphics/Vul3D/                                      mirror of New3D, GL-touching files only
    Vul3D.{h,cpp}           class Vul3D::CVul3D : IRender3D   (from New3D.cpp)
    R3DShader.{h,cpp}       pipelines + uniform blocks       (from New3D/R3DShader)
    R3DFrameBuffers.{h,cpp} 3-attachment MRT + depth copy    (from New3D/R3DFrameBuffers)
    R3DScrollFog.{h,cpp}
    TextureBank.{h,cpp}     R16_UINT 2048x1024, 12 mip levels
    VBO.{h,cpp}             one big vertex buffer, ROM verts then RAM verts
    Shaders/                R3DShaderTriangles.{vert,frag}, R3DShaderCommon.glsl, R3DBlocks.glsl,
                            R3DScrollFog.*, R3DFrameBuffersBase.*, R3DFrameBuffersTrans.*
                            + generated *.spv.h and R3DBlockOffsets.h
    README.md               this file
Scripts/build-spirv.sh                                    glslc over both Shaders/ dirs → *.spv.h
```

Namespace `Vul3D` for the 3D renderer, `Vulkan` for the foundation. `FBO`,
`CRender2D` and `SuperAA` keep their global names, because the rest of the tree
names them. Data types come from New3D's GL-free headers
(`using New3D::Mesh; using New3D::Model; using New3D::Viewport; …`).

**Correction — `vulkan.h` is not two headers, it is fourteen.** With no platform
macro defined `vulkan.h` includes only `vk_platform.h` and `vulkan_core.h`, which
is what the brief said; but `vulkan_core.h` then includes twelve
`vk_video/vulkan_video_codec_*.h` headers unconditionally, for the video-codec
structs. All fourteen are vendored, under `Src/Pkgs/vulkan/` so that
`#include "vk_video/…"` resolves relative to `vulkan_core.h` itself and does not
depend on the `-I` list.

**Correction — `Src/Graphics/` must NOT become an include directory.**
`Rules.inc` derives `INCLUDE_DIRS` from the source list and then uses it as
`VPATH`, while `OBJ_FILES` keys on each source's *basename*. Adding
`Src/Graphics/` back (it holds no translation unit under this backend) makes
`Render2D.cpp` resolve to the GL file that was just excluded, and the build
compiles the wrong one with no diagnostic — measured, three files deep, before
the link failed on missing GL symbols. Nothing needs it: the switch headers
reach their Vulkan neighbours through a path relative to themselves
(`"Vulkan/Render2D.h"`), which a quoted include resolves against the including
file's own directory.

## Selection: a compile-time backend

`RENDER_BACKEND ?= opengl` in the makefiles (`opengl` | `vulkan`). `vulkan`:
- adds `-DSUPERMODEL_VULKAN`;
- removes from `SRC_FILES`: `Src/Pkgs/glew.c`, `Src/Graphics/Shader.cpp`,
  `Src/Graphics/FBO.cpp`, `Src/Graphics/Render2D.cpp`, `Src/Graphics/SuperAA.cpp`,
  the seven GL-touching `Src/Graphics/New3D/*.cpp`, all of Legacy3D,
  `Src/OSD/SDL/Crosshair.cpp`, `Gui.cpp`, `Main.cpp`, `Audio.cpp`, imgui — and
  adds `Src/OSD/iOS/iOSOSD.cpp` on *every* platform, since dropping `Main.cpp`
  and `Audio.cpp` takes `BeginFrameVideo`, `EndFrameVideo` and
  `SetAudioCallback` with them and that file is exactly the set of stubs for
  them (its `CCrosshair` half is `#ifndef SUPERMODEL_VULKAN`, so it no longer
  drags `New3D.h` and GLEW into a build that has neither);
- adds `Src/Graphics/Vulkan/*.cpp` and `Src/Graphics/Vul3D/*.cpp`;
- drops `-framework OpenGL -framework GLUT` on macOS. **Nothing links a Vulkan
  library**: every entry point is fetched through the frontend's
  `get_instance_proc_addr`, so the build needs headers only.
- is libretro-only (`LIBRETRO=1`); the SDL desktop binary errors out with a clear
  message under `RENDER_BACKEND=vulkan`.
`Makefile.iOS` replaces its `GL_BACKEND` seam with `RENDER_BACKEND`, default
`vulkan`. `Makefile.OSX` defaults to `opengl` so the existing build is
unchanged.

Headers that Model3 includes by name get a two-line switch so the rest of the
tree compiles unchanged:
```cpp
// Src/Graphics/Render2D.h (same for SuperAA.h, FBO.h)
#ifdef SUPERMODEL_VULKAN
#include "Vulkan/Render2D.h"
#else
… existing GL class …
#endif
```
**Correction — the `AATarget` typedef is `unsigned`, not `GLuint`.**
`Src/Graphics/IRender3D.h` includes no GL header and must not start; `GLuint`
*is* `unsigned int`, so `typedef unsigned AATarget;` is the same type and
`CNew3D::Init`'s existing `unsigned aaTarget` still overrides.

`LibRetro.cpp` carries `#ifdef SUPERMODEL_VULKAN` branches in exactly these
places: `RequestHWContext`, `retro_context_reset/destroy`, `InitGLState`
(→ `InitVulkanState`), `BindCurrentFramebuffer`, `BindSupermodelDefaultFramebuffer`,
`ApplyGLGeometry`, the pointer overlay, `InitializeRenderers` (`new Vul3D::CVul3D`),
`retro_run`'s `glFlush` (→ end frame + present), and the crosshair (compiled out).

## The frontend contract (libretro Vulkan)

- `RequestHWContext`: `context_type = RETRO_HW_CONTEXT_VULKAN`, `version_major =
  VK_API_VERSION_1_0`, `cache_context = true`, then register a
  `retro_hw_render_context_negotiation_interface_vulkan` **version 1** with
  `get_application_info` and `create_device` (`destroy_device = nullptr`).
  retro-trainer declines `GET_HW_RENDER_INTERFACE` unless a negotiation
  interface with a device hook was registered, and RetroArch also calls it, so it
  is not optional.
- `create_device` (v1 signature in `libretro_vulkan.h`): pick `gpu` if given,
  else the first physical device with a graphics queue; choose one queue family
  with `VK_QUEUE_GRAPHICS_BIT` that can also present to `surface` when a surface
  is passed; enable exactly the `required_device_extensions`, layers and
  `required_features` handed in (retro-trainer hands in wgpu's set, which includes
  `VK_KHR_portability_subset` on MoltenVK); fill every field of
  `retro_vulkan_context` (presentation queue = the same queue). Return `false`
  rather than abort on any failure.
- `retro_context_reset`: `GET_HW_RENDER_INTERFACE` → `retro_hw_render_interface_vulkan*`
  (check `interface_type`/`interface_version`), `VulkanContext::Init(iface)`,
  then the usual `InitializeRenderers`.
  **Correction — `get_device_proc_addr` may be null.** retro-trainer leaves it
  null (`src/vulkan.rs`, `get_device_proc_addr: None`), so `VulkanContext::Init`
  falls back to the `vkGetDeviceProcAddr` it loads from the instance. A core that
  dereferenced the interface field would crash on this frontend.
  **Correction — the frontend reads `create_info.image`, not just `image_view`.**
  retro-trainer's `vulkan_set_image` takes the `VkImage` to blit from out of
  `retro_vulkan_image::create_info.image`, so the whole `VkImageViewCreateInfo`
  must be real and must outlive the `retro_video_refresh` call, not merely be
  present.
- Per frame: `Begin()` at the top of `retro_run` (waits on the fence *the core
  itself* submitted with the frame that last used this sync index, then resets
  that slot's command pool, begins its primary command buffer, and resets its
  descriptor pool and rings). The interface's own `wait_sync_index` is not
  called: the core's fence is the stronger guarantee, and a frontend may not
  hand back a sync index it is still using. the emulator then records uploads and draws; `End()` ends any
  open render pass, blits *screen* → *present[slot]* flipped, transitions
  present to `SHADER_READ_ONLY_OPTIMAL`, ends the command buffer,
  `lock_queue` / `vkQueueSubmit(fence)` / `unlock_queue`, `set_image` with
  `retro_vulkan_image{ image_view, image_layout=SHADER_READ_ONLY_OPTIMAL,
  create_info }`, `num_semaphores = 0`, `src_queue_family = queue_index`; then
  `cb_video_refresh(RETRO_HW_FRAME_BUFFER_VALID, total_x_res, total_y_res, 0)`.
  **Correction — the slot count is the highest set bit of `get_sync_index_mask`
  plus one, not its popcount.** The slots are indexed by `get_sync_index`, so
  for a sparse mask a popcount-sized vector would be too small and two in-flight
  frames would alias onto one slot. (retro-trainer's mask is 1; RetroArch's is
  usually its swapchain depth, and both are dense, so the two agree in practice
  — but the code must be right for the mask that is actually returned.)
- `retro_context_destroy`: `vkDeviceWaitIdle`, destroy renderers, then the
  context's own objects. Never destroy the `VkDevice` — the frontend owns it.

## The foundation (`VulkanContext`) — a GL-shaped state machine over Vulkan

The translation is mechanical because the context exposes the handful of GL
verbs the renderers use:

| GL | VulkanContext |
|---|---|
| `glBindFramebuffer(target)` / `BindSupermodelDefaultFramebuffer` | `BindTarget(RenderTarget*)` — ends the open render pass (if any) and begins the new target's pass with `LOAD_OP_LOAD` on every attachment |
| `glClear(bits)` | `ClearAttachments(color?, depth?, stencil?, values)` via `vkCmdClearAttachments` over the current scissor rect (GL clears honour scissor) |
| `glViewport` / `glScissor` / `glDisable(GL_SCISSOR_TEST)` | `SetViewport`, `SetScissor`, `SetScissorOff` — stored, re-emitted after every pipeline bind and pass begin (dynamic state) |
| `glDrawArrays(prim, first, count)` | `Draw(pipeline, first, count)` — binds pipeline if changed, re-emits viewport/scissor, `vkCmdDraw` |
| texture / buffer uploads | `UploadImage(image, level, x, y, w, h, rowPitchBytes, src)`, `UploadBuffer(buffer, offset, size, src)` — copy through a per-frame staging ring; a copy ends any open render pass first and inserts the needed barriers |
| `glBlitFramebuffer(DEPTH\|STENCIL)` | `CopyImage(src, dst, aspects)` outside a pass |
| `glReadPixels` | `ReadbackToBuffer(image, aspect, x, y)` — records the copy; the result is read after the frame's fence, see ProcessLos |
| `glUniform*` | `UniformRing::Append(&block, size)` → `{VkBuffer, offset}` for a dynamic-offset UBO binding |

`RenderTarget` = N colour images (+ views) + optional depth/stencil image, a
`VkRenderPass` (all attachments `LOAD_OP_LOAD`/`STORE_OP_STORE`, final layouts
`COLOR_ATTACHMENT_OPTIMAL` / `DEPTH_STENCIL_ATTACHMENT_OPTIMAL`) and a
`VkFramebuffer`. Colour images are `R8G8B8A8_UNORM` with
`COLOR_ATTACHMENT | SAMPLED | TRANSFER_SRC | TRANSFER_DST`; depth/stencil is
`D32_SFLOAT_S8_UINT` (mirrors `GL_DEPTH32F_STENCIL8`) with
`DEPTH_STENCIL_ATTACHMENT | TRANSFER_SRC | TRANSFER_DST`. Images the shaders
sample get a layout transition to `SHADER_READ_ONLY_OPTIMAL` when their pass
ends and back when bound as a target; keep that bookkeeping inside
`RenderTarget`/`BindTarget` so renderers never see a barrier.

The *screen* target mirrors libretro's GL default framebuffer: one colour
attachment + D32S8, sized `total_x_res × total_y_res` (times supersampling when
SuperAA is on, exactly as the GL path does through `SuperAA::GetTargetID`).

Memory: one `VkDeviceMemory` per image/buffer (there are ~15 objects total);
a `FindMemoryType(typeBits, flags)` helper; staging and uniform rings are
`HOST_VISIBLE | HOST_COHERENT`, persistently mapped, one per frame slot.

Descriptors: one `VkDescriptorPool` per frame slot, reset in `Begin()`. Sets are
allocated per (pipeline layout, bound images) and cached for the frame. Uniform
blocks are `UNIFORM_BUFFER_DYNAMIC`; the dynamic offset is passed at bind time
so a per-draw change of a block costs an `Append` and a
`vkCmdBindDescriptorSets` with new offsets, not a new set.

Function loading: `VulkanContext` holds every `PFN_vk*` it uses, fetched with
`get_instance_proc_addr` (instance-level) and `vkGetDeviceProcAddr`
(device-level), with `VK_NO_PROTOTYPES` defined project-wide for the Vulkan
build. No global function pointers, no `volk`.

## Vul3D — what each file changes relative to New3D

- **`Vul3D.cpp`** — `New3D.cpp` verbatim except the GL lines. Constructor:
  no VAO; vertex layout is fixed in the pipeline (locations 0–6 =
  inVertex, inNormal, inTexCoord, inColour, inFaceNormal, inFixedShade,
  inTextureNP, formats matching `FVertex`). `RenderFrame`'s sequence is kept
  step for step: bind MRT target, clear colour, `DrawAmbientFog`, `DrawScrollFog`,
  then per priority × {normal, overlay}: bind MRT colour layer, clear
  depth to **0.0** and stencil to 0, colour pass (GEQUAL), `ProcessLos`, trans1
  (GREATER) with `StoreDepth` before, `RestoreDepth`, trans2, then
  `R3DFrameBuffers::Draw` onto the screen (or the SuperAA target).
  `m_primType` and the `QuadRendering` branch are dropped (log once if the config
  asks for quads).
**Correction — five basenames collide with New3D's, and the build silently
picked the wrong ones.** `Rules.inc` sets `VPATH` from a *sorted* `INCLUDE_DIRS`,
and `Src/Graphics/New3D/` sorts before `Src/Graphics/Vul3D/` — so
`obj-vk/R3DShader.o` (and `VBO`, `TextureBank`, `R3DFrameBuffers`,
`R3DScrollFog`) built from the OpenGL source, with no diagnostic, until the link
failed. Measured with `make … -n -B obj-vk/R3DShader.o`, which prints
`Src/Graphics/New3D/R3DShader.cpp` with the fix removed and
`Src/Graphics/Vul3D/R3DShader.cpp` with it. The fix is `VPATH_PRIORITY_DIRS`,
which puts the two mirror directories in front of the sorted list — the same
hazard as the `Src/Graphics/` correction above, from the other direction.

**Correction — the C++ uniform mirrors are checked against the SPIR-V, not
against a hand-typed copy of the GLSL.** A `static_assert(offsetof(…) == 96)` is
a second copy of the layout, and it passes exactly when both copies drift
together. So `Scripts/build-spirv.sh` compiles each Vul3D shader a *second* time
with `-g -O0` (the `-O` build strips the member names), reads every
`OpMemberDecorate … Offset` out of `spirv-dis`, and writes
`Shaders/R3DBlockOffsets.h`; `R3DShader.cpp` and `R3DScrollFog.cpp` assert each
member of each mirror struct against it, and the script errors if two shaders lay
the same block out differently. Verified red on purpose: inserting one member
into `R3DBlocks.glsl` fails the build naming `MeshBlock.fogIntensity`.

**Correction — `vec3 lighting[2]` stays a `vec3` array in the GLSL.** std140 pads
every element of a `vec3` array out to sixteen bytes (measured:
`OpDecorate … ArrayStride 16`), so it has exactly the layout `vec4 lighting[2]`
would — and the shader body keeps `sunVector = lighting[0]` and `lighting[1].x`
verbatim, which a `vec4` would have broken. Only the *C++* mirror pads, and the
same goes for the loose `vec3` uniforms. Booleans likewise stay `bool` in the
blocks: glslang lays a block `bool` out as a 32-bit value, so the whole
`if(textureEnabled)` body is verbatim against `int32_t` on the C++ side.

- **`R3DShader`** — same public API (`SetMeshUniforms`, `SetModelStates`,
  `SetViewportUniforms`, `SetLayer`, `DiscardAlpha`, `SetShader`, `Start`);
  `GetVertexAttribPos` is gone, because attribute locations are declared in the
  shader and baked into the pipeline.
  Uniforms become three std140 blocks: `ViewportBlock` (proj matrix, fog,
  lighting, spot, hardwareStep, cota), `ModelBlock` (modelMat, modelScale,
  nodeAlpha), `MeshBlock` (every per-mesh scalar/flag incl. `colourLayer`,
  `discardAlpha`, `texturePage`, `baseTexInfo`, `textureWrapMode`). Each setter
  writes the CPU struct and marks it dirty; the draw path appends dirty blocks
  to the uniform ring and rebinds offsets. The dirty-check structure of New3D is
  preserved (that is what keeps the per-mesh cost low). Booleans are `int32_t`;
  `vec3` arrays (`lighting[2]`) are stored as `vec4` in both C++ and GLSL.
  The stencil state that New3D sets from `SetMeshUniforms` becomes:
  `noLosReturn` → dynamic stencil reference (`noLosReturn << 7`), compare mask
  `0x80`, write mask `0x80`; `layered` → a pipeline variant with
  `stencilOp(KEEP, INCR_AND_CLAMP, INCR_AND_CLAMP)`, compare EQUAL to 0 under
  mask `0x7F`, write mask `0x7F`. Pipelines are cached by
  `{layer, stencil mode, depthCompare}` — twelve — with dynamic viewport,
  scissor, stencil reference / compare mask / write mask. Colour write masks
  implement `glDrawBuffers`: colour layer writes all three attachments, trans1
  only attachment 1, trans2 only attachment 2.
  **Correction — the stencil half of the key is a *mode*, not `layered`.**
  New3D's `SetMeshUniforms` writes the line-of-sight stencil configuration
  whenever `noLosReturn` changes **even for a layered mesh**, and restores the
  layered one only when `layered` itself changes — so a mesh really can be drawn
  layered with LOS stencil ops. A pipeline keyed on `m->layered` would not
  reproduce that; one keyed on an `m_stencilMode` assigned alongside every
  `SetStencil`, in the same two branches, does.
  **Correction — `glDepthFunc` becomes `R3DShader::SetDepthCompare`.** It could
  have been folded into the layer (the colour pass is the GEQUAL one and the two
  transparency passes the GREATER ones, with no exception in `RenderFrame`), but
  keeping it a separate call keeps one GL line mapping to one Vulkan line, and
  costs six pipelines nothing ever binds.
  **Correction — the descriptor set is replaced, never updated.** A set that has
  been bound in a recording command buffer may not be written again, so
  `EnsureDescriptorSet` allocates a fresh one whenever the frame's uniform ring
  or a texture-bank view changes under it — about eight per frame — and the
  per-draw cost stays an `Append` plus a rebind with three dynamic offsets.
  **Correction — the uniform ring is 16 MB, not 1.** `AppendUniforms` falls back
  to a dedicated buffer for a request that does not fit, which is fine for a
  texture upload but not here: R3DShader binds its blocks through a set that
  names the *ring*, so an overflow is a skipped draw. A mesh costs up to three
  appends, each rounded up to `minUniformBufferOffsetAlignment` (256 bytes on
  MoltenVK), across twelve passes.
- **`R3DFrameBuffers`** — one `RenderTarget` with three colour attachments +
  D32S8, and a second D32S8 image for the depth copy (the GL original made a
  whole second framebuffer for that, but only ever attached a depth/stencil
  renderbuffer to it). `StoreDepth/RestoreDepth` → `CopyImage` of depth+stencil
  aspects; `Draw` → two full-screen quads (base then trans composite) with
  blending on, sampling the three colour images with `texelFetch`.
  **Correction — `SetFBO` does not record the layer for `R3DShader`, and it may
  not skip an unchanged layer.** The write-mask variant is picked by
  `R3DShader::SetLayer`, which `CVul3D::RenderFrame` calls immediately before
  every `SetFBO` with the same value, so the two cannot diverge and `SetFBO` has
  only the bind left to do. But unlike `glBindFramebuffer` it must do that bind
  every time: `StoreDepth`, `RestoreDepth` and `ProcessLos` all end the open
  render pass, and `SetFBO` is where the next one is opened.
  **Correction — `CreateFBO` takes the destination render target.** The two
  composite pipelines bake in the pass they draw into, and the AA target (one
  colour attachment, no depth) is not compatible with the screen (one colour
  attachment plus D32S8), so which one it is has to be known at creation.
- **`R3DScrollFog`** — full-screen quad pipeline writing all three attachments,
  depth test and depth write off; one UBO block.
  **Correction — blending is OFF here, not on.** The brief said on; GL's state at
  that point in the frame has it disabled. `Model3` renders the bottom tile layer
  and then the 3D frame, and `CRender2D::DrawSurface` ends with
  `glDisable(GL_BLEND)`; nothing between it and `New3D::DrawScrollFog` re-enables
  it, and `DrawScrollFog`'s own trailing `glDisable(GL_BLEND)` is belt-and-braces,
  not evidence. With blending on the fog would come out multiplied by
  `scrollFog` over the black the buffer was just cleared to — a different image.
- **`TextureBank`** — `R16_UINT` 2048×1024 with 12 mip levels
  (`VkImage` mips halve exactly as New3D's loop allocates). `UploadTextures`
  copies the sub-rectangle from texture RAM through the staging ring in one
  `vkCmdCopyBufferToImage` (`bufferRowLength = width` after packing rows, or
  2048 if the staging copy keeps the source stride). Sampler: nearest, repeat
  (only `texelFetch` is used, so filtering never matters). Both banks share one
  descriptor binding as `usampler2D textureBank[2]`; indexing by the
  `texturePage` uniform is legal in Vulkan GLSL (dynamically uniform).
  **Correction — the copy carries the source stride; the rows are not packed.**
  `bufferRowLength = 2048`, which is the texture-RAM width GL walked one row at a
  time (a `glTexSubImage2D` per row) rather than set `GL_UNPACK_ROW_LENGTH` for.
  One `vkCmdCopyBufferToImage` replaces the whole loop.
  **Correction — the chain is 12 levels but `GetNumberOfLevels()` returns 11.**
  New3D's allocation loop allocates one final 1×1 level *after* the increment
  that produced the count it reports, and `CNew3D::UploadTextures` iterates the
  reported count — so the extra level exists and is never uploaded. Vulkan needs
  the real count at `vkCreateImage`, so the constructor allocates
  `m_numLevels + 1` and `GetNumberOfLevels` keeps returning New3D's number.
  **Correction — an upload asked for between frames is deferred, not dropped.**
  `CReal3D::LoadState` uploads the whole sheet, and it runs from
  `retro_unserialize`, outside `retro_run`'s frame. GL did it there and then; a
  Vulkan upload is a command recorded into a command buffer that does not exist
  yet, and `VulkanContext::UploadImage` correctly ignores it. So
  `CVul3D::UploadTextures` records the request and `RenderFrame` performs it
  before anything samples the banks — without that, every savestate load would
  show the textures of the state that was loaded over.
- **`VBO`** — a `DEVICE_LOCAL` `VkBuffer` of `sizeof(FVertex) * (MAX_RAM_VERTS +
  MAX_ROM_VERTS)`; `BufferSubData`/`AppendData` go through `UploadBuffer`.
- **`ProcessLos`** — the `glReadPixels` of one depth/stencil texel becomes a
  recorded `ReadbackToBuffer` of that texel (depth and stencil aspects) into a
  per-priority slot of a per-frame host-visible buffer. The result is consumed
  at the start of the **next** `RenderFrame`, after that frame's fence wait and
  **before** the `m_losBack`/`m_losFront` swap — which reproduces New3D's timing
  exactly, because the CPU only ever reads the *front* buffer that was written
  during the previous frame. The value computation (`zVal = depth / NEAR_PLANE`,
  stencil bit 7 into the mantissa LSB) is copied verbatim. No mid-frame stall.
  **Correction — the buffer is per frame SLOT, and with more than one slot the
  value is that many frames old.** What `VulkanContext::Begin` guarantees is that
  *this slot's* fence has signalled, not the previous frame's — so the readback
  legible at the top of a frame is the one recorded by the last frame that used
  the same slot. With the sync-index mask retro-trainer returns (one slot) that
  is the previous frame and the timing is New3D's exactly; with N slots the
  line-of-sight value a gun game reads is N-1 frames older than GL's.
  **Correction — the bounds test GL did silently is written out.** `glReadPixels`
  outside the framebuffer returns nothing and is not an error; a
  `vkCmdCopyImageToBuffer` outside the image is not defined, so `ProcessLos`
  checks the translated position against the target and skips it.
- **Shaders** — GLSL 450 for Vulkan: `gl_VertexID` → `gl_VertexIndex`; loose
  `uniform`s → the blocks above with `layout(std140, set=0, binding=N)`;
  samplers get `set`/`binding`; every vertex input, stage varying and fragment
  output gets an explicit `layout(location=N)`; `#include "R3DShaderCommon.glsl"`
  replaces New3D's two-string concatenation (glslc supports `#include`).
  Bodies are otherwise verbatim: `gl_FragDepth`, `texelFetch`, `discard`,
  `usampler2D`, the LOD math, all unchanged. `Shaders2D`'s runtime
  `#define UPSCALEMODE n` and SuperAA's `aa`/`CRTCOLORS` defines become
  specialization constants.
  **Correction — a specialization constant cannot drive `#if`.** The `#if` chains
  those defines selected become `if` chains — and where the chain selected a
  *constant* rather than a statement (SuperAA's `cgamma` and `colmatrix`), a
  function returning it. The compiler folds the whole chain at pipeline creation,
  so this costs what the `#define` variants cost, and every number stays verbatim.
  **Correction — `sampler` is a type keyword in Vulkan GLSL** and cannot be a
  parameter name, so `Shaders2D.frag`'s `biquintic`/`bicubic` take `tex`.

## The 2D layer, SuperAA, pointer overlay

`CRender2D` (Vulkan): two `R8G8B8A8_UNORM` 496×384 sampled images, uploaded
each frame from the tile-gen buffers (source stride 512 px → `bufferRowLength`
512), one full-screen-quad pipeline with alpha blending and the same
`Shaders2D` GLSL (specialization constant for the upscale mode), targets the
screen or the SuperAA target exactly where the GL code binds `m_aaTarget`.
`Setup2D(true)`'s scissor-off clear is `ClearAttachments` with scissor off.

`SuperAA`: a no-op unless `aa > 1` or CRT colours, exactly as in GL; otherwise
an `FBO` (Vulkan `RenderTarget`) at the supersampled size, drawn to the screen
with its existing shader.

Pointer overlay (`LibRetro.cpp`): the same 24-vertex triangle list, a small
`HOST_VISIBLE` vertex buffer, one pipeline, push-constant `u_resolution`/`u_color`.

## Sharing opportunities noted, deliberately not taken

Recorded so the upstream maintainer can decide, and so this port does not
impose an abstraction on New3D:
1. `Vul3D.cpp` duplicates ~1300 lines of `New3D.cpp`'s scene-graph walk
   (`DescendCullingNode`, `RenderViewport`, `CacheModel`, `DrawModel`, …).
   A `CReal3DSceneBuilder` producing `m_nodes`/`m_polyBufferRam` that both
   renderers consume would remove the copy.
2. The uniform *values* — `R3DShader`'s per-mesh/model/viewport state — are
   identical in both; a shared `R3DUniforms.h` (the three block structs) could
   feed both `glUniform*` calls and the Vulkan ring.
3. `IRender3D` already abstracts the 3D renderer; an `IRender2D` (and a
   compile-independent `SuperAA`) would let both back ends coexist in one
   binary and be chosen at runtime like `New3DEngine`. Today the choice is
   `RENDER_BACKEND` at build time.
4. `R3DFrameBuffers`' composite shaders, `R3DScrollFog`'s and `Shaders2D`'s
   full-screen quads are the same four vertices five times.

## What this port does not do

- Quads (`QuadRendering`) — Metal has no geometry shader; the option is ignored
  with a log line.
- The crosshair (`CCrosshair`, SDL + GL, gun games only) — compiled out under
  `SUPERMODEL_VULKAN`.
- The SDL desktop UI stays OpenGL.

## Building and testing

```
# macOS, libretro, Vulkan backend (the existing GL build is RENDER_BACKEND=opengl,
# the default). BITS=-vk sends objects and the artefact to obj-vk/ and lib-vk/, so
# the GL and Vulkan builds of one checkout never clobber each other: their sources
# differ but their basenames do not.
make -f Makefiles/Makefile.OSX LIBRETRO=1 RENDER_BACKEND=vulkan BITS=-vk -j12 lib-vk/supermodel_libretro.dylib
# macOS, libretro, the unchanged OpenGL backend -> lib/supermodel_libretro.dylib
make -f Makefiles/Makefile.OSX LIBRETRO=1 -j12 lib/supermodel_libretro.dylib
# iOS (RENDER_BACKEND defaults to vulkan there) -> lib-ios/supermodel_libretro.dylib
make -f Makefiles/Makefile.iOS LIBRETRO=1 -j12
# regenerate SPIR-V headers (needs glslc; the generated headers are committed)
VULKAN_SDK=~/VulkanSDK/1.4.350.1/macOS ./Scripts/build-spirv.sh
```

The check that "nothing links a Vulkan library" still holds is one command per
artefact, and both must print `0`:
```
nm -u lib-vk/supermodel_libretro.dylib  | grep -c "_gl\|_vk"
nm -u lib-ios/supermodel_libretro.dylib | grep -c "_gl\|_vk"
```
Run through retro-trainer on the Mac (`./make.sh spikeofe` in retro-trainer with
the core symlinked into `config/cores/`), which drives MoltenVK — the same layer
the iPad uses — with `WGPU_VALIDATION=1` turning on the Khronos validation
layer.
