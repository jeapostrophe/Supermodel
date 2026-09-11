# Vul3D -- a Vulkan render path for Supermodel's libretro core

`Src/Graphics/Vulkan/` and `Src/Graphics/Vul3D/` are a per-file, per-function
translation of `Src/Graphics/` and `Src/Graphics/New3D/` onto Vulkan, selected
at build time by `RENDER_BACKEND=vulkan` and reachable only from the libretro
core. This file describes what is there and why it is shaped that way.

The reviewable artefact is the diff against the originals --
`diff -u Src/Graphics/New3D/New3D.cpp Src/Graphics/Vul3D/Vul3D.cpp` and the five
other pairs -- in which every hunk is a GL-to-Vulkan edit or a commented
insertion. Normalise line endings first: the New3D sources are CRLF and the new
files LF.

## Why

The libretro core renders through desktop OpenGL 4.1 (`RETRO_HW_CONTEXT_OPENGL_CORE`).
iOS has no desktop GL, its GLES stops at 3.0, and New3D's shaders need edits to
fit GLES (`textureBank[texturePage]` indexing, `precision`). The station that
motivates this port (retro-trainer on an iPad, `PLAN-ipad SM-04`) already serves
libretro's **Vulkan** interface to every other hardware core through MoltenVK,
on macOS and on iOS alike. A Vulkan path gives Supermodel one hardware render
path on both.

Facts the design leans on:
- Only ~300 of New3D's 6511 lines touch GL. `Mat4`, `Vec`, `Model`,
  `PolyHeader`, `R3DData`, `R3DFloat` and `Plane` contain **zero** GL and are
  reused by `#include` -- not copied, not abstracted.
- New3D's projection is reverse-Z with a constant clip z (`Mat4::FrustumRZ`
  sets `m[10]=0`, `m[14]=near`) and the fragment shader writes
  `gl_FragDepth = projMat[3][2] * gl_FragCoord.w` explicitly. Under Vulkan's
  `0 <= z <= w` clip rule that is the **same** clip volume and the same depth
  value, so no projection change and no depth remap is needed.
- Pipeline state space is tiny: depth compare GEQUAL (colour pass) or GREATER
  (transparency passes), depth write on, no blend in the mesh passes, no cull
  (back-face culling is emulated in the shader), two stencil configurations.
- At libretro defaults the quad path (geometry shader) is off
  (`QuadRendering` is `false` in `LibRetro.cpp`'s runtime config); Metal has no
  geometry stage, so Vul3D does not implement `QuadRendering` at all.

## Coordinate convention -- the one rule everything else follows

**Render exactly as GL does, into images whose row 0 is GL's bottom row, and
flip once at hand-off.** Every viewport, scissor, `gl_FragCoord.y` computation,
`Shaders2D.vert`'s "flip upside down" texcoords and `TranslateLosPosition` stay
verbatim. Vulkan's viewport transform maps NDC y=-1 to framebuffer row 0, which
is exactly GL's mapping if row 0 is called "bottom". The only place the two
conventions meet is the final `vkCmdBlitImage` in `VulkanContext::End`, from the
*screen* image into the *presentation* image, which uses inverted source y
offsets (`srcOffsets[0].y = height`, `srcOffsets[1].y = 0`). RetroArch and
retro-trainer both take a Vulkan core's image as top-down; `bottom_left_origin`
is not consulted on the Vulkan path in either frontend.

Viewports are positive-height; no `VK_KHR_maintenance1` negative viewport is
used. Front-face winding is irrelevant (cull mode NONE everywhere).

## Layout

```
Src/Pkgs/vulkan/{vulkan.h,vulkan_core.h,vk_platform.h}   vendored Khronos headers (VK_NO_PROTOTYPES)
Src/Pkgs/vulkan/vk_video/*.h                             12 more, included unconditionally by vulkan_core.h
Src/Pkgs/libretro_vulkan.h                               libretro's Vulkan interface header
Src/Graphics/Vulkan/                                     the foundation + the non-3D GL users, mirrored
    VulkanContext.{h,cpp}   function table, device/queue, memory, per-frame command buffers,
                            render-target state machine, staging uploads, uniform ring
    VulkanShader.{h,cpp}    SPIR-V modules, descriptor-set layouts, pipeline building
                            (the GLSLShader analogue)
    FBO.{h,cpp}             mirror of Src/Graphics/FBO -- an offscreen colour target
    Render2D.{h,cpp}        mirror of Src/Graphics/Render2D -- class CRender2D
    SuperAA.{h,cpp}         mirror of Src/Graphics/SuperAA -- class SuperAA
    Shaders/                Shaders2D.*, SuperAA.*, PointerOverlay.* + generated *.spv.h (committed)
Src/Graphics/Vul3D/                                      mirror of New3D, GL-touching files only
    Vul3D.{h,cpp}           class Vul3D::CVul3D : IRender3D   (from New3D.cpp)
    R3DShader.{h,cpp}       pipelines + uniform blocks       (from New3D/R3DShader)
    R3DFrameBuffers.{h,cpp} 3-attachment MRT + depth copy    (from New3D/R3DFrameBuffers)
    R3DScrollFog.{h,cpp}
    TextureBank.{h,cpp}     R16_UINT 2048x1024
    VBO.{h,cpp}             one big vertex buffer, ROM verts then RAM verts
    Shaders/                R3DShaderTriangles.{vert,frag}, R3DShaderCommon.glsl, R3DBlocks.glsl,
                            R3DScrollFog.*, R3DFrameBuffersBase.*, R3DFrameBuffersTrans.*
                            + generated *.spv.h and R3DBlockOffsets.h
    README.md               this file
Scripts/build-spirv.sh                                    glslc over both Shaders/ dirs -> *.spv.h
```

`vulkan.h` is fourteen headers, not two: with no platform macro defined it
includes only `vk_platform.h` and `vulkan_core.h`, and `vulkan_core.h` then
includes twelve `vk_video/vulkan_video_codec_*.h` headers unconditionally for
the video-codec structs. All fourteen are vendored under `Src/Pkgs/vulkan/` so
that `#include "vk_video/..."` resolves relative to `vulkan_core.h` itself and
does not depend on the `-I` list.

Namespace `Vul3D` for the 3D renderer, `Vulkan` for the foundation. `FBO`,
`CRender2D` and `SuperAA` keep their global names, because the rest of the tree
names them. Data types come from New3D's GL-free headers
(`using New3D::Mesh; using New3D::Model; using New3D::Viewport; ...`).

## Selection: a compile-time backend

`RENDER_BACKEND ?= opengl` in `Makefiles/Rules.inc` (`opengl` | `vulkan`).
`vulkan`:
- adds `-DSUPERMODEL_VULKAN` and `-DVK_NO_PROTOTYPES`;
- removes from `SRC_FILES`: `Src/Pkgs/glew.c`, `Src/Graphics/Shader.cpp`,
  `Src/Graphics/FBO.cpp`, `Src/Graphics/Render2D.cpp`, `Src/Graphics/SuperAA.cpp`,
  the seven GL-touching `Src/Graphics/New3D/*.cpp`, all of Legacy3D,
  `Src/OSD/SDL/Crosshair.cpp`, `Gui.cpp`, `Main.cpp`, `Audio.cpp`, imgui -- and
  adds `Src/OSD/iOS/iOSOSD.cpp` if the platform has not already, since dropping
  `Main.cpp` and `Audio.cpp` takes `BeginFrameVideo`, `EndFrameVideo` and
  `SetAudioCallback` with them and that file is exactly the stubs for them (its
  `CCrosshair` half is `#ifndef SUPERMODEL_VULKAN`, so it does not drag
  `New3D.h` and GLEW into a build that has neither);
- adds `Src/Graphics/Vulkan/*.cpp` and `Src/Graphics/Vul3D/*.cpp`;
- drops `-framework OpenGL -framework GLUT` on macOS. **Nothing links a Vulkan
  library**: every entry point is fetched through the frontend's
  `get_instance_proc_addr`, so the build needs headers only, and
  `VK_NO_PROTOTYPES` makes a stray direct call fail to compile rather than
  quietly acquire a loader dependency;
- is libretro-only; `Rules.inc` errors out under `RENDER_BACKEND=vulkan` without
  `LIBRETRO=1`, because there is no window, no swapchain and no SDL desktop UI
  on this path.

`Makefile.iOS` defaults to `vulkan`; on iOS `opengl` is still accepted and still
fails to link, which is the honest outcome (`glClearDepth` and `glDrawBuffer`
are among the entry points the GL core links directly and are not in GLES 3.0).
`Makefile.OSX` defaults to `opengl`, so the existing build is unchanged.

Headers that Model3 includes by name get a two-line switch so the rest of the
tree compiles unchanged:
```cpp
// Src/Graphics/Render2D.h (same for SuperAA.h, FBO.h)
#ifdef SUPERMODEL_VULKAN
#include "Vulkan/Render2D.h"
#else
... existing GL class ...
#endif
```
Those switch headers reach their Vulkan neighbours through a path relative to
themselves (`"Vulkan/Render2D.h"`), which a quoted include resolves against the
including file's own directory. `Src/Graphics/IRender3D.h` includes no GL header
and must not start, so `AATarget` is `typedef unsigned` under OpenGL --
deliberately `GLuint`'s underlying type, so `CNew3D::Init`'s existing
`unsigned aaTarget` still overrides -- and `Vulkan::RenderTarget *` under
`SUPERMODEL_VULKAN`, where `nullptr` means the screen target.

### The VPATH hazard, in both directions

`Rules.inc` derives `INCLUDE_DIRS` from the source list and uses it as `VPATH`,
while `OBJ_FILES` keys on each source's *basename*. Two consequences, both of
which produce a wrong object file with no diagnostic and a link failure several
files later:

- **`Src/Graphics/` must not be an include directory under this backend.** It
  holds no translation unit here, and adding it back makes `Render2D.cpp`
  resolve to the GL file that was just excluded.
- **Five Vul3D basenames also exist in `Src/Graphics/New3D/`**, which stays in
  the build for its GL-free translation units. `INCLUDE_DIRS` is sorted, and
  `New3D` sorts before `Vul3D`, so `R3DShader.o` (and `VBO`, `TextureBank`,
  `R3DFrameBuffers`, `R3DScrollFog`) would be built from the OpenGL source.
  `VPATH_PRIORITY_DIRS` puts the two mirror directories in front of the sorted
  list.

Because the failure is silent, `Rules.inc` also gates it: for every entry of
`SRC_FILES` it derives what `VPATH` would actually resolve that basename to and
`$(error)`s naming any that would resolve elsewhere. Sources that do not exist
at parse time are skipped, because Musashi generates `m68kcpu.c` and friends
into `$(OBJ_DIR)` during the build.

`LibRetro.cpp` carries `#ifdef SUPERMODEL_VULKAN` branches in exactly these
places: `RequestHWContext`, `retro_context_reset`, `retro_context_destroy`,
`InitGLState` (-> `InitVulkanState`), `UpdateOutputGeometry`'s
default-framebuffer query, `BindCurrentFramebuffer`,
`BindSupermodelDefaultFramebuffer`, `ApplyGLGeometry`, `DestroyRenderers`, the
pointer overlay, `InitializeRenderers` (`new Vul3D::CVul3D`), `retro_run`'s
`glFlush` (-> end frame + present), and the crosshair (compiled out).

## The frontend contract (libretro Vulkan)

- `RequestHWContext`: `context_type = RETRO_HW_CONTEXT_VULKAN`,
  `version_major = VK_API_VERSION_1_0`, `cache_context = true`, then register a
  `retro_hw_render_context_negotiation_interface_vulkan` **version 2** with
  `VulkanGetApplicationInfo`, `VulkanCreateDevice` and `VulkanCreateDevice2`
  (`destroy_device` and `create_instance` null). A frontend that only knows v1
  must not reject the higher version -- it uses the entry points it recognizes,
  which is why `VulkanCreateDevice` stays.
  retro-trainer declines `GET_HW_RENDER_INTERFACE` unless a negotiation
  interface with a device hook was registered, and RetroArch also calls it, so
  it is not optional. A frontend that rejects the negotiation interface is not
  fatal here -- the core is loaded, and `retro_context_reset` says so with its
  own message rather than leaving `retro_init` half finished.
- `VulkanCreateDeviceCommon`, the body both entry points share: pick `gpu` if
  given, else the first physical device with a graphics queue; choose a queue
  family with `VK_QUEUE_GRAPHICS_BIT` (preferring one that also has
  `VK_QUEUE_COMPUTE_BIT`) that can also present to `surface` when a surface is
  passed; enable exactly the `required_device_extensions`, layers and
  `required_features` handed in (under v1 retro-trainer hands in wgpu's set,
  which includes `VK_KHR_portability_subset` on MoltenVK); fill every field of
  `retro_vulkan_context` (presentation queue = the same queue). Return `false`
  rather than abort on any failure.

  The one difference between the two is how the device is built.
  `VulkanCreateDevice` (v1) calls `vkCreateDevice` resolved through the
  frontend's `get_instance_proc_addr`. `VulkanCreateDevice2` (v2) instead calls
  the `create_device_wrapper` the frontend hands in -- the header requires it:
  "Rather than call vkCreateDevice directly, a core must call the CreateDevice
  wrapper provided" -- which is what lets the frontend merge its own extensions
  and `pNext` features into this core's `VkDeviceCreateInfo`. v2 passes no
  required extensions, layers or features at all, so `ppEnabledExtensionNames`
  and `ppEnabledLayerNames` are empty there and the merge is entirely the
  frontend's. `pEnabledFeatures` is the exception: it is never empty, because
  the core always puts its own `shaderSampledImageArrayDynamicIndexing` in it
  (below), so a frontend that wants a `VkPhysicalDeviceFeatures2` of its own in
  the `pNext` chain has to fold that flat struct into it rather than set both --
  Vulkan permits only one of the two -- which is what retro-trainer's wrapper
  does. It also lets a frontend that
  wants to *share* the device know it built it: retro-trainer adopts the
  resulting `VkDevice` for wgpu and imports the core's images instead of reading
  each frame back (`PLAN LAT-03`, `LAT-08`).

  One feature is added to the frontend's set rather than passed through:
  `shaderSampledImageArrayDynamicIndexing`. Vulkan 1.0 permits a non-constant
  index into a sampled-image array only when it was enabled at `vkCreateDevice`,
  and Vul3D's fragment shader indexes `usampler2D textureBank[2]` by the
  `texturePage` uniform. Only the core knows it needs this, so it is ORed into a
  copy of the struct after checking `vkGetPhysicalDeviceFeatures`. Without it,
  the symptom is a validation error at pipeline creation and wrong texels on
  every textured polygon wherever the rule is enforced; MoltenVK is likely to
  run it regardless, which makes it a trap rather than a crash. A device that
  lacks it gets a log line rather than a refusal, because the 2D layer is still
  worth having.
- `retro_context_reset`: `GET_HW_RENDER_INTERFACE` ->
  `retro_hw_render_interface_vulkan*` (check `interface_type` and
  `interface_version`), `DestroyRenderers`, `VulkanContext::Init(iface)`,
  `InitVulkanState`, then the usual `InitializeRenderers`. The
  `DestroyRenderers` is not tidiness: libretro allows a `context_reset` with no
  preceding `context_destroy`, and `VulkanContext::Init` self-heals by calling
  `Shutdown()`, which would otherwise leave the renderers holding pipelines and
  images made against a context that has gone away. The interface's
  `get_device_proc_addr` may be null -- retro-trainer leaves it null -- so
  `Init` falls back to the `vkGetDeviceProcAddr` it loads from the instance; a
  core that dereferenced that field would crash on this frontend.
- Per frame: `Begin(total_x_res, total_y_res)` from `BindCurrentFramebuffer` at
  the top of `retro_run`. It picks up `get_sync_index`, waits on the fence *the
  core itself* submitted with the frame that last used that slot, resets that
  slot's command pool, descriptor pool and rings, frees its oversized staging
  buffers, and begins its primary command buffer. The interface's own
  `wait_sync_index` is not called: the core's fence is the stronger guarantee,
  and a frontend may not hand back a sync index it is still using.

  The emulator then records uploads and draws, and `End()` ends any open render
  pass, blits *screen* -> *present[slot]* flipped, transitions present to
  `SHADER_READ_ONLY_OPTIMAL`, ends the command buffer, then
  `lock_queue` / `vkQueueSubmit(fence)` / `unlock_queue`, then `set_image` with
  `retro_vulkan_image{ image_view, image_layout = SHADER_READ_ONLY_OPTIMAL,
  create_info }`, `num_semaphores = 0` and `src_queue_family = queue_index`.
  `End()` returns whether an image was actually handed over, and `retro_run`
  passes `RETRO_HW_FRAME_BUFFER_VALID` only when it did; a frame that never
  reached `set_image` is announced with NULL, libretro's "dupe the last frame",
  because retro-trainer's `video_refresh` unwraps a missing image. The frontend
  keeps the whole `VkImageViewCreateInfo`, not just the view -- retro-trainer
  takes the `VkImage` to blit from out of `create_info.image` -- so it must be
  real and must outlive the `retro_video_refresh` call, and the presentation
  images carry `TRANSFER_DST | TRANSFER_SRC | SAMPLED` and `MUTABLE_FORMAT`.

  The slot count is the **highest set bit** of `get_sync_index_mask` plus one,
  not its popcount. The slots are indexed by `get_sync_index`, so for a sparse
  mask a popcount-sized vector would be too small and two in-flight frames would
  alias onto one slot. (retro-trainer's mask is 1 and RetroArch's is usually its
  swapchain depth, so both are dense and the two agree in practice -- but the
  code must be right for the mask that is actually returned.)
- `retro_context_destroy`: `vkDeviceWaitIdle`, destroy renderers, then
  `VulkanContext::Shutdown`. Never destroy the `VkDevice` -- the frontend owns
  it.

## The foundation (`VulkanContext`) -- a GL-shaped state machine over Vulkan

The translation is mechanical because the context exposes the handful of GL
verbs the renderers use:

| GL | VulkanContext |
|---|---|
| `glBindFramebuffer(target)` / `BindSupermodelDefaultFramebuffer` | `BindTarget(RenderTarget*)` / `BindScreen()` -- ends the open render pass and begins the new target's with `LOAD_OP_LOAD` on every attachment; `nullptr` is the screen |
| `glClear(bits)` | `ClearAttachments(colour, depth, stencil, rgba, depthValue, stencilValue)` via `vkCmdClearAttachments` over the current scissor rect (GL clears honour scissor) |
| `glViewport` / `glScissor` / `glDisable(GL_SCISSOR_TEST)` | `SetViewport`, `SetScissor`, `SetScissorOff` / `SetScissorOn` -- stored, re-emitted after every pipeline bind and pass begin (dynamic state) |
| `glBindBuffer(GL_ARRAY_BUFFER)` | `BindVertexBuffer(buffer, offset)` |
| the descriptor half of `glUseProgram` | `BindDescriptorSet(layout, set, dynamicCount, dynamicOffsets)` |
| `glDrawArrays(prim, first, count)` | `Draw(pipeline, first, count)` -- binds the pipeline if it changed, re-emits dynamic state, `vkCmdDraw` |
| a full-screen textured quad | `DrawTexturedQuad(program, view, sampler)`, or the `views, count` overload for a shader whose set binds more than one image |
| `glStencilFunc` / `glStencilMask`'s dynamic values | `SetStencil(reference, compareMask, writeMask)` |
| `glTexSubImage2D` + `glPixelStorei(GL_UNPACK_ROW_LENGTH)` | `UploadImage(image, level, x, y, w, h, rowLengthTexels, src, srcBytes)` -- copies through the frame's staging ring, ends any open pass first, leaves the image in `SHADER_READ_ONLY_OPTIMAL` |
| `glBufferSubData` | `UploadBuffer(buffer, offset, size, src)`, through the same ring |
| `glBlitFramebuffer(DEPTH\|STENCIL)` | `CopyImage(src, dst, aspects)` outside a pass |
| `glReadPixels` of one texel | `ReadbackToBuffer(image, aspect, x, y, dst, dstOffset)` -- records the copy; the result is read after the frame's fence, see `ProcessLos` |
| `glUniform*` | `AppendUniforms(data, size)` -> a `UniformSlice{buffer, offset, valid}` for a dynamic-offset UBO binding; `UniformRingBuffer()` names the buffer a set binds |

Resource creation (`CreateImage`, `CreateBuffer`, `CreateTarget`,
`CreateDescriptorSetLayout`, `AllocateDescriptorSet`, `UpdateDescriptorBuffer` /
`UpdateDescriptorImage` / `UpdateDescriptorImages`, `Sampler(bool linear)`,
`PushConstants`, `EndPass`, `TransitionImage`) rounds it out. Pipeline state
lives in `VulkanShader`: a `PipelineDesc` (shader modules, specialization,
layout, render pass, vertex layout, topology, colour write masks, blend, depth
test/write/compare, `StencilMode`) and a `ShaderProgram` holding the five objects
every renderer here creates together.

`RenderTarget` = N colour images (+ views) + optional depth/stencil image, a
`VkRenderPass` (all attachments `LOAD_OP_LOAD`/`STORE_OP_STORE`) and a
`VkFramebuffer`. Colour images are `R8G8B8A8_UNORM` with
`COLOR_ATTACHMENT | SAMPLED | TRANSFER_SRC | TRANSFER_DST`; depth/stencil is
`D32_SFLOAT_S8_UINT` (mirrors `GL_DEPTH32F_STENCIL8`) with
`DEPTH_STENCIL_ATTACHMENT | TRANSFER_SRC | TRANSFER_DST`. Images the shaders
sample are transitioned to `SHADER_READ_ONLY_OPTIMAL` when their pass ends and
back when bound as a target; that bookkeeping lives in `BindTarget` and the
`Image::layout` field, so renderers never write a barrier.

The *screen* target mirrors libretro's GL default framebuffer: one colour
attachment + D32S8, sized `total_x_res` x `total_y_res`. It is **not**
supersampled -- `InitVulkanState` and `Begin` both pass the un-supersampled
size. The supersampled target is SuperAA's own `FBO`, at the AA multiple of
both dimensions, and everything draws into that instead when it exists
(`SuperAA::GetTargetID`), exactly as the GL path does.

Memory: one `VkDeviceMemory` per image/buffer (there are ~15 objects total) and
a `FindMemoryType(typeBits, flags)` helper. The staging ring (8 MB) and uniform
ring (16 MB) are `HOST_VISIBLE | HOST_COHERENT`, persistently mapped, one of
each per frame slot. A staging request too big for its ring gets a buffer of its
own, freed when the slot comes round again -- a full RAM vertex upload is about
19 MB. A **uniform** request that does not fit does not: `AppendUniforms` says
so once per frame and returns an invalid slice. R3DShader binds its blocks
through a set that names the *ring*, so a dedicated buffer is unreachable
through it and the draw is lost either way; and since `RingAppend` does not
advance the head on that path, every later append in the frame would take it
too, turning one exhausted frame into thousands of
`vkCreateBuffer`/`vkAllocateMemory` pairs against MoltenVK's low
`maxMemoryAllocationCount`. `UNIFORM_RING_BYTES` is the number to raise.

Descriptors: one `VkDescriptorPool` per frame slot, reset in `Begin()`. Uniform
blocks are `UNIFORM_BUFFER_DYNAMIC`; the dynamic offset is passed at bind time,
so a per-draw change of a block costs an append and a `vkCmdBindDescriptorSets`
with new offsets, not a new set. The pool's `UNIFORM_BUFFER_DYNAMIC` count is
`DESCRIPTOR_SETS_PER_FRAME * 3`, because R3DShader's one set binds three of
them. Only `R3DShader` caches a set at all (see below); `DrawTexturedQuad` and
`R3DScrollFog::DrawScrollFog` allocate a fresh one per draw, which is a handful
per frame.

Function loading: `VulkanContext::Api` holds every `PFN_vk*` the port uses, in
one X-macro list so the declarations and the loader cannot drift apart. Instance
functions come from the frontend's `get_instance_proc_addr`, device functions
from `vkGetDeviceProcAddr`. No global function pointers, no `volk`.

## Vul3D -- what each file changes relative to New3D

- **`Vul3D.cpp`** -- `New3D.cpp` verbatim except the GL lines. Constructor: no
  VAO; the vertex layout is fixed in the pipeline (locations 0-6 = `inVertex`,
  `inNormal`, `inTexCoord`, `inColour`, `inFaceNormal`, `inFixedShade`,
  `inTextureNP`, formats matching `FVertex`). `RenderFrame`'s sequence is kept
  step for step: bind MRT target, clear colour, `DrawAmbientFog`,
  `DrawScrollFog`, then per priority x {normal, overlay}: bind the MRT colour
  layer, clear depth to **0.0** and stencil to 0, colour pass (GEQUAL),
  `ProcessLos`, trans1 (GREATER) with `StoreDepth` before, `RestoreDepth`,
  trans2, then `R3DFrameBuffers::Draw` onto the screen (or the SuperAA target).
  `m_primType` and the `QuadRendering` branch are gone; the constructor logs
  once if the config asks for quads.
- **`R3DShader`** -- same public API (`SetMeshUniforms`, `SetModelStates`,
  `SetViewportUniforms`, `SetLayer`, `DiscardAlpha`, `SetShader`, `Start`);
  `GetVertexAttribPos` is gone, because attribute locations are declared in the
  shader and baked into the pipeline. Uniforms become three std140 blocks:
  `ViewportBlock` (proj matrix, fog, lighting, spot, `hardwareStep`, `cota`),
  `ModelBlock` (`modelMat`, `modelScale`, `nodeAlpha`) and `MeshBlock` (every
  per-mesh scalar and flag, including `colourLayer`, `discardAlpha`,
  `texturePage`, `baseTexInfo`, `textureWrapMode`). Each setter writes the CPU
  struct and marks it dirty; the draw path appends dirty blocks to the uniform
  ring and rebinds offsets. New3D's dirty-check structure is preserved -- that
  is what keeps the per-mesh cost low.

  The stencil state New3D sets from `SetMeshUniforms` becomes: `noLosReturn` ->
  dynamic stencil reference (`noLosReturn << 7`), compare mask `0x80`, write
  mask `0x80`; `layered` -> `StencilMode::Layered`, which compares EQUAL to the
  reference under mask `0x7F` and increments-and-clamps, write mask `0x7F`.
  The pipeline key's stencil half is an `m_stencilMode` assigned alongside every
  `SetStencil`, **not** `m->layered`: New3D writes the line-of-sight
  configuration whenever `noLosReturn` changes *even for a layered mesh*, and
  restores the layered one only when `layered` itself changes, so a mesh really
  can be drawn layered with LOS stencil ops.

  Pipelines are cached by `{layer, stencil mode, depth compare}` -- three by two
  by two, twelve -- with dynamic viewport, scissor, stencil reference, compare
  mask and write mask. `PIPELINE_LAYERS`, `PIPELINE_STENCIL_MODES` and
  `PIPELINE_DEPTH_COMPARES` in `R3DShader.h` are the only spelling of that
  encoding: `CreatePipelines` loops over them, `Draw` indexes back through them
  and `m_pipelines` is sized from them. Colour write masks implement
  `glDrawBuffers`: the colour layer writes all three attachments, trans1 only
  attachment 1, trans2 only attachment 2. `glDepthFunc` becomes
  `SetDepthCompare`; it could have been folded into the layer, but keeping it a
  separate call keeps one GL line mapping to one Vulkan line and costs six
  pipelines nothing ever binds.

  The descriptor set is **replaced, never updated**: a set bound in a recording
  command buffer may not be written again, so `EnsureDescriptorSet` allocates a
  fresh one whenever the frame's uniform ring or a texture-bank view changes
  under it -- about eight per frame -- and the per-draw cost stays an append plus
  a rebind with three dynamic offsets. `SetShader` drops the cached set on
  purpose: `VulkanContext::Begin` resets the slot's descriptor pool, and with a
  single frame slot the ring's `VkBuffer` handle is identical from frame to
  frame, so the `m_setBuffer == ring` test would reuse a set the pool has freed.
- **`R3DFrameBuffers`** -- one `RenderTarget` with three colour attachments plus
  D32S8, and a second D32S8 image for the depth copy (the GL original made a
  whole second framebuffer for that but only ever attached a depth/stencil
  renderbuffer to it). `StoreDepth`/`RestoreDepth` -> `CopyImage` of the depth
  and stencil aspects; `Draw` -> two full-screen quads (base, then the
  transparency composite) with blending on, sampling the three colour images
  with `texelFetch`.

  `CreateFBO` takes the destination render target, because the two composite
  pipelines bake in the pass they draw into and the AA target (one colour
  attachment, no depth) is not compatible with the screen (one colour attachment
  plus D32S8). `SetFBO` does not record the layer for `R3DShader` -- the
  write-mask variant is picked by `SetLayer`, which `CVul3D::RenderFrame` calls
  immediately before every `SetFBO` with the same value -- so all it has left is
  the bind, and unlike `glBindFramebuffer` it does that bind every time:
  `StoreDepth`, `RestoreDepth` and `ProcessLos` all end the open render pass,
  and `SetFBO` is where the next one is opened.
- **`R3DScrollFog`** -- a full-screen quad pipeline writing all three
  attachments, depth test and depth write off, one UBO block. Blending is
  **off**: `Model3` renders the bottom tile layer and then the 3D frame, and
  `CRender2D::DrawSurface` ends with GL's blend disabled; nothing between it and
  `DrawScrollFog` re-enables it. With blending on, the fog would come out
  multiplied by `scrollFog` over the black the buffer was just cleared to.
- **`TextureBank`** -- `R16_UINT` 2048x1024. The mip chain is **12** levels
  while `GetNumberOfLevels()` returns 11: New3D's loop allocates one final 1x1
  level *after* the increment that produced the count it reports, and
  `UploadTextures` iterates the reported count, so the extra level exists and is
  never uploaded. Vulkan needs the real count at `vkCreateImage`, so the
  constructor allocates `m_numLevels + 1` and `GetNumberOfLevels` keeps
  returning New3D's number.

  `UploadTextures` copies the sub-rectangle from texture RAM through the staging
  ring in one `vkCmdCopyBufferToImage` with `bufferRowLength = 2048`, the width
  of texture RAM -- GL walked it one row at a time with a `glTexSubImage2D` per
  row rather than set `GL_UNPACK_ROW_LENGTH`; the single copy replaces the whole
  loop. Both banks share one descriptor binding as `usampler2D textureBank[2]`;
  indexing by the `texturePage` uniform is dynamically uniform, which is the
  GLSL half of what makes it legal (the device-feature half is in
  `VulkanCreateDeviceCommon`, above).

  `Bind()` transitions the image to `SHADER_READ_ONLY_OPTIMAL` and returns its
  view. That transition is load-bearing: nothing but `UploadImage` would move
  the image out of `VK_IMAGE_LAYOUT_UNDEFINED`, while `R3DShader`'s descriptor
  claims `SHADER_READ_ONLY_OPTIMAL` -- so every frame before the game's first
  texture DMA, and every frame after a context reset rebuilds the banks, would
  be a layout mismatch. It is a no-op once an upload has left the image in that
  layout. `CRender2D::PreRenderFrame` has the same shape (it skips the upload
  while a tile-gen draw buffer is null, and `DrawSurface` samples the texture
  anyway) and does the same thing. Both of `VulkanContext`'s samplers use
  `VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE`, which never matters here: the 3D
  shaders only `texelFetch`, so neither filter nor address mode is consulted.

  An upload asked for **between** frames is deferred, not dropped.
  `CReal3D::LoadState` uploads the whole sheet and runs from
  `retro_unserialize`, outside `retro_run`'s frame. GL did it there and then; a
  Vulkan upload is a command recorded into a command buffer that does not exist
  yet, so `CVul3D::UploadTextures` sets `m_uploadWholeSheet` and `RenderFrame`
  performs it before anything samples the banks. Without that, every savestate
  load would show the textures of the state that was loaded over.
- **`VBO`** -- a `DEVICE_LOCAL` `VkBuffer` of `sizeof(FVertex) * (MAX_RAM_VERTS +
  MAX_ROM_VERTS)` with `VERTEX_BUFFER | TRANSFER_DST`;
  `BufferSubData`/`AppendData` go through `UploadBuffer`.
- **`ProcessLos`** -- the `glReadPixels` of one depth/stencil texel becomes a
  recorded `ReadbackToBuffer` of that texel (depth and stencil aspects) into a
  per-priority slot of a per-frame-slot host-visible buffer. The result is
  consumed by `ResolveLos` at the start of the **next** `RenderFrame`, after
  that frame's fence wait and **before** the `m_losBack`/`m_losFront` swap. The
  value computation (`zVal = depth / NEAR_PLANE`, stencil bit 7 into the
  mantissa LSB) is copied verbatim. No mid-frame stall.

  Three things GL did implicitly are written out. A fence wait does not make the
  copy visible to the host, so `ReadbackToBuffer` ends with a `VkMemoryBarrier`
  from `TRANSFER_WRITE` to `HOST_READ` at `PIPELINE_STAGE_HOST_BIT`;
  `HOST_COHERENT` removes the need to *invalidate* the mapping, not the need for
  the dependency, and the layout transition's image barrier carries neither
  `HOST_READ` nor the host stage. Without it `ResolveLos` may read a stale
  value, and a gun game decides "hit geometry" or "hit sky" from it. A
  `glReadPixels` outside the framebuffer returns nothing and is not an error,
  while a `vkCmdCopyImageToBuffer` outside the image is not defined, so
  `ProcessLos` bounds-checks the translated position against the target. And the
  buffer is per frame *slot*: `Begin` guarantees that *this slot's* fence has
  signalled, not the previous frame's, so with N slots the value is N-1 frames
  older than GL's. With the mask retro-trainer returns (one slot) the timing is
  New3D's exactly.
- **Shaders** -- GLSL 450 for Vulkan: `gl_VertexID` -> `gl_VertexIndex`; loose
  `uniform`s -> the blocks above with `layout(std140, set=0, binding=N)`;
  samplers get `set`/`binding`; every vertex input, stage varying and fragment
  output gets an explicit `layout(location=N)`;
  `#include "R3DShaderCommon.glsl"` replaces New3D's two-string concatenation
  (glslc supports `#include`). Bodies are otherwise verbatim: `gl_FragDepth`,
  `texelFetch`, `discard`, `usampler2D`, the LOD math, all unchanged.

  `Shaders2D`'s runtime `#define UPSCALEMODE n` and SuperAA's `aa`/`CRTCOLORS`
  defines become `layout(constant_id = N)` specialization constants. A
  specialization constant cannot drive `#if`, so the `#if` chains those defines
  selected become `if` chains -- and where a chain selected a *constant* rather
  than a statement (SuperAA's `cgamma` and `colmatrix`), a function returning
  it. The compiler folds the whole chain at pipeline creation, so this costs
  what the `#define` variants cost and every number stays verbatim. `sampler` is
  a type keyword in Vulkan GLSL and cannot be a parameter name, so
  `Shaders2D.frag`'s `biquintic` and `bicubic` take `tex`.

  `vec3 lighting[2]` stays a `vec3` array in the GLSL. std140 pads every element
  of a `vec3` array out to sixteen bytes (`OpDecorate ... ArrayStride 16`), so it
  has exactly the layout `vec4 lighting[2]` would, and the shader body keeps
  `lighting[0]` and `lighting[1].x` verbatim, which a `vec4` would have broken.
  Only the C++ mirror pads, and the same goes for the loose `vec3` uniforms.
  Booleans likewise stay `bool` in the blocks: glslang lays a block `bool` out as
  a 32-bit value, so the whole `if(textureEnabled)` body is verbatim against
  `int32_t` on the C++ side.

### The std140 mirrors are checked against the SPIR-V

`R3DShader.h`'s `ViewportBlock`/`ModelBlock`/`MeshBlock` and `R3DScrollFog`'s
`ScrollFogBlock` mirror `Shaders/R3DBlocks.glsl` byte for byte. A hand-typed
`static_assert(offsetof(...) == 96)` would be a second copy of the layout that
passes exactly when both copies drift together, so the offsets are read back out
of the compiled SPIR-V instead: `Scripts/build-spirv.sh` compiles each Vul3D
shader a second time with `-g -O0` (the `-O` build strips member names), reads
every `OpMemberDecorate ... Offset` out of `spirv-dis`, and writes
`Shaders/R3DBlockOffsets.h`. `R3DShader.cpp` and `R3DScrollFog.cpp` assert each
mirror member against it, and the script errors if two shaders lay the same
block out differently or if fewer than the four blocks' worth came back.

Per-member offsets alone would miss the likelier edit: a member *appended* to a
block moves no existing offset, so every assert would pass while the C++ mirror
was silently short. The generator therefore also emits
`VUL3D_STD140_<Block>_LASTOFFSET`, and each mirror asserts its own last member
against it. Both gates are verified red on purpose -- inserting a member into
`R3DBlocks.glsl` fails naming `MeshBlock.fogIntensity`, appending one fails
naming "MeshBlock has a member this mirror does not".

## The 2D layer, SuperAA, pointer overlay

`CRender2D` (Vulkan): two `R8G8B8A8_UNORM` 496x384 sampled images, uploaded each
frame from the tile-gen buffers (the buffers are 512 texels wide and only the
first 496 of each row are visible, so `bufferRowLength` is 512), one
full-screen-quad pipeline with alpha blending and the same `Shaders2D` GLSL
(specialization constant for the upscale mode), targeting the screen or the
SuperAA target exactly where the GL code binds `m_aaTarget`. `Setup2D(true)`'s
clear is `SetScissorOff` / `ClearAttachments` / `SetScissorOn`.

`SuperAA`: a no-op unless `aa > 1` or CRT colours, exactly as in GL; otherwise
an `FBO` (a Vulkan `RenderTarget`) at the supersampled size, drawn to the screen
with its existing shader.

Pointer overlay (`LibRetro.cpp`): the same 24-vertex triangle list, a small
`HOST_VISIBLE` vertex buffer, one pipeline, push-constant `u_resolution` and
`u_color`.

## Sharing opportunities noted, deliberately not taken

Recorded so the upstream maintainer can decide, and so this port does not impose
an abstraction on New3D:
1. `Vul3D.cpp` duplicates `New3D.cpp`'s scene-graph walk
   (`DescendCullingNode`, `RenderViewport`, `CacheModel`, `DrawModel`, ...),
   which is most of both files. A `CReal3DSceneBuilder` producing
   `m_nodes`/`m_polyBufferRam` that both renderers consume would remove the
   copy.
2. The uniform *values* -- `R3DShader`'s per-mesh/model/viewport state -- are
   identical in both; a shared `R3DUniforms.h` (the three block structs) could
   feed both `glUniform*` calls and the Vulkan ring.
3. `IRender3D` already abstracts the 3D renderer; an `IRender2D` (and a
   compile-independent `SuperAA`) would let both back ends coexist in one binary
   and be chosen at runtime like `New3DEngine`. Today the choice is
   `RENDER_BACKEND` at build time.
4. `R3DFrameBuffers`' composite shaders, `R3DScrollFog`'s and `Shaders2D`'s
   full-screen quads are the same four vertices five times.

## What this port does not do

- Quads (`QuadRendering`) -- Metal has no geometry shader; the option is ignored
  with a log line.
- The crosshair (`CCrosshair`, SDL + GL, gun games only) -- compiled out under
  `SUPERMODEL_VULKAN`.
- The SDL desktop UI stays OpenGL.

## Building and testing

```
# macOS, libretro, Vulkan backend. BITS=-vk sends objects and the artefact to
# obj-vk/ and lib-vk/, so the GL and Vulkan builds of one checkout never clobber
# each other: their sources differ but their basenames do not.
make -f Makefiles/Makefile.OSX LIBRETRO=1 RENDER_BACKEND=vulkan BITS=-vk -j12 lib-vk/supermodel_libretro.dylib
# macOS, libretro, the unchanged OpenGL backend -> lib/supermodel_libretro.dylib
make -f Makefiles/Makefile.OSX LIBRETRO=1 -j12 lib/supermodel_libretro.dylib
# iOS (RENDER_BACKEND defaults to vulkan there) -> lib-ios/supermodel_libretro.dylib
make -f Makefiles/Makefile.iOS LIBRETRO=1 -j12
# regenerate SPIR-V headers (needs glslc; the generated headers are committed)
VULKAN_SDK=~/VulkanSDK/1.4.350.1/macOS ./Scripts/build-spirv.sh
```

`./build-libretro-macos.sh` wraps the first two: it picks `BITS` from
`RENDER_BACKEND`, makes the SDL framework symlinks described below, and builds.

The check that "nothing links a Vulkan library" still holds is one command per
artefact, and both must print `0`:
```
nm -u lib-vk/supermodel_libretro.dylib  | grep -c "_gl\|_vk"
nm -u lib-ios/supermodel_libretro.dylib | grep -c "_gl\|_vk"
```

Run through retro-trainer on the Mac (`./make.sh spikeofe` in retro-trainer with
the core symlinked into `config/cores/`), which drives MoltenVK -- the same layer
the iPad uses -- with `WGPU_VALIDATION=1` turning on the Khronos validation
layer. The macOS Vulkan core still links SDL2 and SDL2_net (threads and the
netboard go through SDL on macOS; only iOS uses the pthread shim in
`Src/OSD/iOS/`), so the two frameworks must sit beside the dylib for
`@loader_path` resolution; `build-libretro-macos.sh` makes those symlinks. A
missing one shows up as `dlopen ... Library not loaded: @rpath/SDL2.framework`
in the frontend, before the core says anything.

**Measured 2026-09-01 on an M4 Max, retro-trainer + MoltenVK, validation on:**
the core negotiates a device (`Vulkan: Apple M4 Max (API 1.3.334), queue family
0`, `sync index mask 0x1, 1 frame slots`), builds the screen target and one
presentation image at 496x384, and runs `spikeofe` with **no validation message
from the core**. The two that remain in that log
(`VUID-vkAcquireNextImageKHR-fence-10066`, `VUID-StandaloneSpirv-None-10684`)
fire before the core is loaded and come from the frontend's own swapchain and
shaders. One core message was found and fixed in that run: a view on the
transfer-only depth-copy image (`VUID-VkImageViewCreateInfo-image-04441`).
