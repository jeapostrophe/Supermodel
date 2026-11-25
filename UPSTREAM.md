# Upstreaming notes — libretro core → trzy

This fork's `master` is **mixerjdp's libretro core (PR #1 to this repo) rebased
onto current upstream trzy** (`upstream/master`, tip `24d2ffc`), plus a few
fixes. It is a clean, linear history on trzy's tip — upstream-shaped. **Nothing
is pushed yet.** (Uncommitted working note; not part of the tree.)

## Provenance & credit
- **mixerjdp's PR #1** provides the libretro core: OpenGL HW-render
  (`RETRO_HW_CONTEXT_OPENGL_CORE`), in-memory savestates, the libretro memory
  API, input profiles, core options, and a frontend log/perf layer.
- The rebase **preserved his authorship** — 25 commits on `master` carry
  `Author: mixerjdp` (only the committer changed). Verify:
  `git log --author=mixerjdp upstream/master..master`.
- Pre-mixerjdp state is tagged **`hack-v0`**; his original un-rebased branch is
  **`pr1-mixerjdp`**.
- *Optional courtesy:* to also get the GitHub **"Merged"** badge + notification
  on PR #1, merge it on GitHub **before** pushing this rebased `master` (the
  rebase changed the commit SHAs, so after a push GitHub would otherwise mark #1
  "closed" rather than "merged"). The authorship credit does not depend on this.

## Fixes layered on top of mixerjdp
- `fix(libretro): set Network config default` (`2b98a4d`) — trzy's netboard reads
  `config["Network"]`, which the self-contained libretro config didn't set; a
  Net-Board game (Spikeout: Final Edition) threw `Node "Network" does not exist`
  at load. Mirrors `Main.cpp`.
- `fix(libretro): core-profile crosshair shader` (`e02b128`) — the pointer-overlay
  shader was `#version 130`, rejected under macOS's required GL 4.1 core profile;
  bumped both stages to `#version 410 core` (the body was already core-compatible).
- `feat(libretro): expose 8 MB PPC main RAM as RETRO_MEMORY_SYSTEM_RAM`
  (`298f54e`) — for frontend memory inspection (cheats / rewind / analysis),
  alongside mixerjdp's SAVE_RAM (backup) / RTC (EEPROM). Byte-swap layout
  documented in `Src/Model3/Model3.h` (PPC byte at `ram[A^3]`, halfword `[A^2]`,
  word native-LE).
- `build-libretro-macos.sh` — captures the macOS build requirements.

## Rebase conflict resolutions (for reference)
- `New3D/R3DFrameBuffers.cpp` — kept mixerjdp's `BindSupermodelDefaultFramebuffer()`
  for `Layer::none` **and** trzy's new `default:` switch case.
- `Model3.cpp`/`Model3.h` — inserted mixerjdp's memory accessors into trzy's
  now-unguarded NetBoard layout; kept trzy's include structure + `#include <cstddef>`.
- Windows `.vcxproj` (toolset v145 vs v143) and `.github/workflows/build.yml` +
  `README.md` — took trzy's side (this fork builds via the Makefiles on macOS).

## macOS build
`./build-libretro-macos.sh` — two non-obvious requirements:
- **Apple `/usr/bin/clang`**, not Homebrew LLVM (Homebrew libc++ emits a
  reference to `std::__hash_memory` from the throttle map's `unordered_map`,
  unresolved against the system libc++).
- SDL2 / SDL2_net frameworks symlinked next to the dylib for `@loader_path`.

## Before opening the trzy PR
- [ ] Re-add mixerjdp's libretro CI (`.github/workflows/build.yml`) that the
      rebase dropped in favor of trzy's — reconcile the two `build.yml` properly.
- [ ] Decide PR shape (one branch vs. mixerjdp's PR + our follow-ups) — the
      commits are already granular and attributed.
- [ ] Address trzy's PR #282 refactor asks: expose `main()`/`CreateGLScreen`,
      enumerable config via env callback (core options partly do this), savestate
      out of SDL/Main (the in-memory path does), audio as an object not C callbacks.
- [ ] Confirm the GL-HW path is upstream-acceptable as-is (it's the standard
      `OPENGL_CORE` context; a Vulkan-only frontend can do its own readback
      without any core change).
- [ ] Flag the SYSTEM_RAM exposure in the PR as an addition beyond mixerjdp.
