#!/bin/sh -e
# Build the Supermodel libretro core on macOS (arm64), for use in retro-trainer.
#
# Two non-obvious requirements, learned the hard way:
#
#  1. Apple's /usr/bin/clang, NOT Homebrew LLVM. Homebrew libc++ emits a
#     reference to std::__hash_memory (from the libretro throttle map's
#     std::unordered_map) that the system libc++ this links against does not
#     export -> undefined symbol at link. Apple's toolchain is self-consistent.
#
#  2. The dylib resolves SDL2.framework / SDL2_net.framework via @loader_path
#     (its own dir), so both frameworks must sit next to it in lib/. They live
#     in Frameworks/; we symlink them into lib/ if missing.
#
# Usage:  ./build-libretro-macos.sh                          (OpenGL, lib/)
#         RENDER_BACKEND=vulkan ./build-libretro-macos.sh    (Vulkan, lib-vk/)
# Output: lib/supermodel_libretro.dylib or lib-vk/supermodel_libretro.dylib
#
# The Vulkan core (Src/Graphics/Vul3D/README.md) builds into its own object
# and library directories via BITS=-vk so the two backends never clobber each
# other's objects. It still links SDL2/SDL2_net on macOS -- threads and the
# netboard go through SDL there -- so it needs the same framework symlinks.

cd "$(dirname "$0")"

RENDER_BACKEND="${RENDER_BACKEND:-opengl}"
case "$RENDER_BACKEND" in
  opengl) BITS= ;;
  vulkan) BITS=-vk ;;
  *) echo "unknown RENDER_BACKEND '$RENDER_BACKEND' (opengl|vulkan)" >&2; exit 1 ;;
esac
LIB="lib$BITS"
mkdir -p "$LIB"

# SDL frameworks next to the dylib for runtime @loader_path resolution.
[ -e "$LIB/SDL2.framework" ]     || ln -sfn ../Frameworks/SDL2.framework     "$LIB/SDL2.framework"
[ -e "$LIB/SDL2_net.framework" ] || ln -sfn ../Frameworks/SDL2_net.framework "$LIB/SDL2_net.framework"

make -f Makefiles/Makefile.OSX LIBRETRO=1 RENDER_BACKEND="$RENDER_BACKEND" BITS="$BITS" \
     CC=/usr/bin/clang CXX=/usr/bin/clang LD=/usr/bin/clang \
     "$LIB/supermodel_libretro.dylib" -j"$(sysctl -n hw.ncpu)"

echo "Built: $(pwd)/$LIB/supermodel_libretro.dylib"
file "$LIB/supermodel_libretro.dylib"
