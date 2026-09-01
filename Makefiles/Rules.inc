##
## Supermodel
## A Sega Model 3 Arcade Emulator.
## Copyright 2003-2026 The Supermodel Team
##
## This file is part of Supermodel.
##
## Supermodel is free software: you can redistribute it and/or modify it under
## the terms of the GNU General Public License as published by the Free
## Software Foundation, either version 3 of the License, or (at your option)
## any later version.
##
## Supermodel is distributed in the hope that it will be useful, but WITHOUT
## ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
## FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
## more details.
##
## You should have received a copy of the GNU General Public License along
## with Supermodel.  If not, see <http://www.gnu.org/licenses/>.
##

#
# Rules.inc
#
# Build rules, included by all (platform-specific) top-level Makefiles.
#


###############################################################################
# Output Locations
###############################################################################

OBJ_DIR = obj$(strip $(BITS))
BIN_DIR = bin$(strip $(BITS))
LIB_DIR = lib$(strip $(BITS))

SUPERMODEL_BINARY = supermodel$(PLATFORM_BINARY_EXTENSION)
DISASSEMBLER_BINARY = ppcd$(PLATFORM_BINARY_EXTENSION)
SUPERMODEL_DYLIB = supermodel_libretro$(PLATFORM_DYLIB_EXTENSION)


###############################################################################
# Compiler Flags
###############################################################################

#
# Construct Supermodel build options
#
SUPERMODEL_BUILD_FLAGS =

# EXTRA_DEBUG enables render state analyser and additional debugging code by
# defining DEBUG
ifeq ($(strip $(EXTRA_DEBUG)),1)
	SUPERMODEL_BUILD_FLAGS += -DDEBUG
endif

# If built-in debugger enabled, need to define SUPERMODEL_DEBUGGER
ifeq ($(strip $(ENABLE_DEBUGGER)),1)
	SUPERMODEL_BUILD_FLAGS += -DSUPERMODEL_DEBUGGER
endif

# If libretro enabled, need to define SUPERMODEL_LIBRETRO
ifeq ($(strip $(LIBRETRO)),1)
	SUPERMODEL_BUILD_FLAGS += -DSUPERMODEL_LIBRETRO
	SUPERMODEL_BUILD_FLAGS += -DGLEW_LIBRETRO
endif

#
# RENDER_BACKEND: the graphics API the renderers are built against.
#
#   opengl  (default) -- the historical path: GLEW, Src/Graphics/{Shader,FBO,
#                        Render2D,SuperAA}.cpp and Src/Graphics/New3D.
#   vulkan            -- Src/Graphics/Vulkan (the foundation + the 2D layer,
#                        SuperAA and FBO mirrored onto it) and Src/Graphics/Vul3D
#                        (the Real3D renderer). See Src/Graphics/Vul3D/README.md.
#
# The two are mutually exclusive and chosen at build time: `vulkan` defines
# SUPERMODEL_VULKAN, which is what the two-line switches at the top of
# Src/Graphics/{Render2D,SuperAA,FBO}.h and the branches in
# Src/OSD/SDL/LibRetro.cpp key off.
#
# NOTHING LINKS A VULKAN LIBRARY. Every entry point is fetched through the
# libretro frontend's get_instance_proc_addr, which is why VK_NO_PROTOTYPES is
# defined project-wide here: the vendored Khronos headers in Src/Pkgs/vulkan
# then declare types and no functions, so a stray direct call does not compile
# -- rather than compiling and quietly acquiring a dependency on a loader.
#
# The Vulkan path only exists inside the libretro core: it renders into an image
# it hands to the frontend, and there is no window, no swapchain and no SDL
# desktop UI on it. A non-libretro build therefore errors out rather than
# producing a binary with no way to show a frame.
#
RENDER_BACKEND ?= opengl

ifeq ($(strip $(RENDER_BACKEND)),vulkan)
	ifneq ($(strip $(LIBRETRO)),1)
$(error RENDER_BACKEND=vulkan builds the libretro core only. Pass LIBRETRO=1.)
	endif
	SUPERMODEL_BUILD_FLAGS += -DSUPERMODEL_VULKAN
	SUPERMODEL_BUILD_FLAGS += -DVK_NO_PROTOTYPES
else ifneq ($(strip $(RENDER_BACKEND)),opengl)
$(error Unknown RENDER_BACKEND "$(RENDER_BACKEND)" (expected: opengl, vulkan))
endif

# DEBUG will build a debug build but does *not* enable extra debug logging and
# therefore does not modify SUPERMODEL_BUILD_FLAGS

#
# Compiler options
#
ARCH =
OPT = -O3
ifeq ($(strip $(DEBUG)),1)
	OPT = -g	# no optimization; debug mode
endif
WARN = -Wall
CSTD = -std=iso9899:2011
CXXSTD = -std=c++17
LDOPT = -s
ifeq ($(strip $(DEBUG)),1)
	LDOPT =
endif

#
# Construct the compiler (C and C++) and linker flags
#
#
# PLATFORM_ARCH is how a cross-compiling platform makefile (Makefile.iOS) gets
# -arch/-isysroot/-mios-version-min onto every compile. It cannot use ARCH for
# that: platform makefiles include this file, so the `ARCH =` a few lines up
# runs last and would wipe out anything they set. Native platforms leave
# PLATFORM_ARCH empty and nothing changes for them.
#
COMMON_CFLAGS = -c $(ARCH) $(PLATFORM_ARCH) $(OPT) $(WARN) $(addprefix -I,$(sort $(INCLUDE_DIRS))) -DGLEW_STATIC $(SUPERMODEL_BUILD_FLAGS)
ifeq ($(strip $(LIBRETRO)),1)
COMMON_CFLAGS += -fPIC
endif
CFLAGS = $(COMMON_CFLAGS) $(CSTD)
CXXFLAGS = $(PLATFORM_CXXFLAGS) $(COMMON_CFLAGS) $(CXXSTD)
ifeq ($(strip $(LIBRETRO)),1)
LDFLAGS = $(PLATFORM_DYLIB_FLAG) -o $(LIB_DIR)/$(SUPERMODEL_DYLIB) $(PLATFORM_LDFLAGS) $(LDOPT)
else
LDFLAGS = -o $(BIN_DIR)/$(SUPERMODEL_BINARY) $(PLATFORM_LDFLAGS) $(LDOPT)
endif
CFLAGS_GENERATE_DEPENDENCIES = $(filter-out -c,$(CFLAGS))	# remove -c because clang will warn
CXXFLAGS_GENERATE_DEPENDENCIES = $(filter-out -c,$(CXXFLAGS))


###############################################################################
# Source Files
###############################################################################

SRC_FILES = \
	Src/CPU/PowerPC/PPCDisasm.cpp \
	Src/BlockFile.cpp \
	Src/Pkgs/unzip.c \
	Src/Pkgs/ioapi.c \
	Src/Model3/93C46.cpp \
	Src/Model3/JTAG.cpp \
	Src/Pkgs/glew.c \
	Src/Graphics/Shader.cpp \
	Src/Model3/Real3D.cpp \
	Src/Graphics/New3D/GLSLShader.cpp \
	Src/Graphics/New3D/R3DFrameBuffers.cpp \
	Src/Graphics/New3D/New3D.cpp \
	Src/Graphics/New3D/Mat4.cpp \
	Src/Graphics/New3D/Model.cpp \
	Src/Graphics/New3D/PolyHeader.cpp \
	Src/Graphics/New3D/VBO.cpp \
	Src/Graphics/New3D/Vec.cpp \
	Src/Graphics/New3D/R3DShader.cpp \
	Src/Graphics/New3D/R3DFloat.cpp \
	Src/Graphics/New3D/R3DScrollFog.cpp \
	Src/Graphics/New3D/TextureBank.cpp \
	Src/Graphics/FBO.cpp \
	Src/Graphics/Render2D.cpp \
	Src/Graphics/SuperAA.cpp \
	Src/Model3/TileGen.cpp \
	Src/Model3/Model3.cpp \
	Src/CPU/PowerPC/ppc.cpp \
	Src/OSD/SDL/Main.cpp \
	Src/OSD/SDL/Audio.cpp \
	Src/OSD/SDL/Thread.cpp \
	Src/OSD/SDL/Gui.cpp \
	Src/Model3/SoundBoard.cpp \
	Src/Sound/SCSP.cpp \
	Src/Sound/SCSPDSP.cpp \
	Src/CPU/68K/68K.cpp \
	$(OBJ_DIR)/m68kcpu.c \
	$(OBJ_DIR)/m68kopnz.c \
	$(OBJ_DIR)/m68kopdm.c \
	$(OBJ_DIR)/m68kopac.c \
	$(OBJ_DIR)/m68kops.c \
	Src/Model3/DSB.cpp \
	Src/CPU/Z80/Z80.cpp \
	Src/Model3/IRQ.cpp \
	Src/Model3/53C810.cpp \
	Src/Model3/PCI.cpp \
	Src/Model3/RTC72421.cpp \
	Src/Model3/DriveBoard/DriveBoard.cpp \
	Src/Model3/DriveBoard/WheelBoard.cpp \
	Src/Model3/DriveBoard/JoystickBoard.cpp \
	Src/Model3/DriveBoard/SkiBoard.cpp \
	Src/Model3/DriveBoard/BillBoard.cpp \
	Src/Model3/DriveBoard/Z80CTC.cpp \
	Src/Model3/MPC10x.cpp \
	Src/Inputs/Input.cpp \
	Src/Inputs/Inputs.cpp \
	Src/Inputs/InputSource.cpp \
	Src/Inputs/InputSystem.cpp \
	Src/Inputs/InputTypes.cpp \
	Src/Inputs/MultiInputSource.cpp \
	Src/OSD/SDL/SDLInputSystem.cpp \
	Src/OSD/SDL/Crosshair.cpp \
	Src/OSD/Outputs.cpp \
	Src/Sound/MPEG/MpegAudio.cpp \
	Src/Model3/Crypto.cpp \
	Src/OSD/Logger.cpp \
	Src/Util/Format.cpp \
	Src/Util/NewConfig.cpp \
	Src/Util/ByteSwap.cpp \
	Src/Util/ConfigBuilders.cpp \
	Src/GameLoader.cpp \
	Src/Pkgs/tinyxml2.cpp \
	Src/Pkgs/imgui/imgui.cpp \
	Src/Pkgs/imgui/imgui_draw.cpp \
	Src/Pkgs/imgui/imgui_impl_sdl2.cpp \
	Src/Pkgs/imgui/imgui_impl_opengl3.cpp \
	Src/Pkgs/imgui/imgui_tables.cpp \
	Src/Pkgs/imgui/imgui_widgets.cpp \
	Src/ROMSet.cpp \
	Src/OSD/SDL/NetOutputs.cpp \
	Src/Network/TCPReceive.cpp \
	Src/Network/TCPSend.cpp \
	Src/Network/NetBoard.cpp \
	Src/Network/SimNetBoard.cpp \
	$(PLATFORM_SRC_FILES)

ifeq (,$(findstring -DSUPERMODEL_OSX,$(PLATFORM_CXXFLAGS)))
	# -DSUPERMODEL_OSX not present: we are not compiling for macOS and can
	# therefore include the legacy renderer
	SRC_FILES += \
		Src/Graphics/Legacy3D/Error.cpp \
		Src/Graphics/Legacy3D/Legacy3D.cpp \
		Src/Graphics/Legacy3D/Models.cpp \
		Src/Graphics/Legacy3D/TextureRefs.cpp
endif

ifeq ($(strip $(ENABLE_DEBUGGER)),1)
	SRC_FILES += \
		Src/Debugger/Debugger.cpp \
		Src/Debugger/ConsoleDebugger.cpp \
		Src/Debugger/SupermodelDebugger.cpp \
		Src/Debugger/CPUDebug.cpp \
		Src/Debugger/AddressTable.cpp \
		Src/Debugger/Breakpoint.cpp \
		Src/Debugger/CodeAnalyser.cpp \
		Src/Debugger/Exception.cpp \
		Src/Debugger/Interrupt.cpp \
		Src/Debugger/DebuggerIO.cpp \
		Src/Debugger/Label.cpp \
		Src/Debugger/Register.cpp \
		Src/Debugger/Watch.cpp \
		Src/Debugger/CPU/PPCDebug.cpp \
		Src/Debugger/CPU/68KDebug.cpp \
		Src/Debugger/CPU/Musashi68KDebug.cpp \
		Src/Debugger/CPU/Z80Debug.cpp
endif

ifeq ($(strip $(LIBRETRO)),1)
	SRC_FILES += \
		Src/OSD/SDL/LibRetro.cpp
endif

#
# RENDER_BACKEND=vulkan: swap the graphics translation units.
#
# Everything removed here either calls OpenGL directly or exists only to serve
# the SDL desktop UI, which stays OpenGL (Main.cpp, Gui.cpp, imgui). Removing
# Main.cpp/Audio.cpp takes BeginFrameVideo, EndFrameVideo and SetAudioCallback
# with them -- exactly the set Src/OSD/iOS/iOSOSD.cpp was written to supply --
# so it is added here if the platform has not already added it.
#
# Kept from Src/Graphics/New3D: Mat4, Model, PolyHeader, Vec and R3DFloat, which
# contain no GL and which Src/Graphics/Vul3D reuses by #include.
#
ifeq ($(strip $(RENDER_BACKEND)),vulkan)
	EXCLUDE_SRC_FILES += \
		Src/Pkgs/glew.c \
		Src/Graphics/Shader.cpp \
		Src/Graphics/FBO.cpp \
		Src/Graphics/Render2D.cpp \
		Src/Graphics/SuperAA.cpp \
		Src/Graphics/New3D/GLSLShader.cpp \
		Src/Graphics/New3D/New3D.cpp \
		Src/Graphics/New3D/R3DShader.cpp \
		Src/Graphics/New3D/R3DFrameBuffers.cpp \
		Src/Graphics/New3D/R3DScrollFog.cpp \
		Src/Graphics/New3D/TextureBank.cpp \
		Src/Graphics/New3D/VBO.cpp \
		Src/Graphics/Legacy3D/Error.cpp \
		Src/Graphics/Legacy3D/Legacy3D.cpp \
		Src/Graphics/Legacy3D/Models.cpp \
		Src/Graphics/Legacy3D/TextureRefs.cpp \
		Src/OSD/SDL/Main.cpp \
		Src/OSD/SDL/Audio.cpp \
		Src/OSD/SDL/Gui.cpp \
		Src/OSD/SDL/Crosshair.cpp \
		Src/Pkgs/imgui/imgui.cpp \
		Src/Pkgs/imgui/imgui_draw.cpp \
		Src/Pkgs/imgui/imgui_impl_sdl2.cpp \
		Src/Pkgs/imgui/imgui_impl_opengl3.cpp \
		Src/Pkgs/imgui/imgui_tables.cpp \
		Src/Pkgs/imgui/imgui_widgets.cpp

	SRC_FILES += \
		Src/Graphics/Vulkan/VulkanContext.cpp \
		Src/Graphics/Vulkan/VulkanShader.cpp \
		Src/Graphics/Vulkan/FBO.cpp \
		Src/Graphics/Vulkan/Render2D.cpp \
		Src/Graphics/Vulkan/SuperAA.cpp \
		Src/Graphics/Vul3D/Vul3D.cpp

	ifeq (,$(filter Src/OSD/iOS/iOSOSD.cpp,$(SRC_FILES)))
		SRC_FILES += Src/OSD/iOS/iOSOSD.cpp
	endif
endif

#
# Per-platform source exclusions.
#
# A platform makefile may set EXCLUDE_SRC_FILES to drop translation units that
# cannot be built (or are simply unreachable) for that target -- e.g. the SDL
# OSD, imgui and the SDL2_net netboard on iOS, none of which the libretro entry
# points can reach. Paths must match the SRC_FILES spelling exactly.
#
SRC_FILES := $(filter-out $(EXCLUDE_SRC_FILES),$(SRC_FILES))

#
# Sorted-path compile order
#
OBJ_FILES = $(foreach file,$(SRC_FILES),$(OBJ_DIR)/$(basename $(notdir $(file))).o)

#
# Deduce include directories from the source file list. The sort function
# removes duplicates and is used to construct a set.
#
# This is also VPATH, and OBJ_FILES keys on each source's BASENAME -- so a
# directory here that holds a same-named source shadows the one in SRC_FILES.
# Do NOT add Src/Graphics/ back under RENDER_BACKEND=vulkan: it would make
# "Render2D.cpp" resolve to the excluded GL file rather than to
# Src/Graphics/Vulkan/Render2D.cpp, with no diagnostic.
#
INCLUDE_DIRS = $(sort $(foreach file,$(SRC_FILES),$(dir	$(file))))


###############################################################################
# Targets
###############################################################################

#
# Default target: build Supermodel binary. Must be specified *before* the auto-
# generated dependencies because otherwise, make gets confused for some reason
# and thinks the default target is just one of the object files.
#
ifeq ($(strip $(LIBRETRO)),1)
all:	$(LIB_DIR)/$(SUPERMODEL_DYLIB)
else
all:	$(BIN_DIR)/$(DISASSEMBLER_BINARY) $(BIN_DIR)/$(SUPERMODEL_BINARY)
endif

#
# Supermodel3.com release targets: used only to create official builds stamped
# with the Git version number. Double-colon rules should force ordered
# execution. The automated build script relies on these -- don't modify their
# output!
#
.PHONY: clean
.PHONY: set_version
.PHONY: print_message
.PHONY: version
release:: set_version
release:: print_message
release:: clean
release:: all

#
# Github release flow (not yet used).
#
PKG_FILES += $(BIN_DIR)/$(SUPERMODEL_BINARY) Docs/README.txt Docs/LICENSE.txt Config Assets

pkg: pkg/$(PKG_TYPE)
	$(info Cleaning up            : $(PKG_STAGE_PATH))
	$(SILENT)cd pkg && rm -r $(notdir $(PKG_STAGE_PATH))

pkg/tgz: pkg/stage
	$(info Creating package       : $(PKG_PATH))
	$(SILENT)tar -C pkg -czf $(PKG_PATH) $(notdir $(PKG_STAGE_PATH))

pkg/zip: pkg/stage
	$(info Creating package       : $(PKG_STAGE_PATH).zip)
	$(SILENT)cd pkg && zip -r $(notdir $(PKG_PATH)) $(notdir $(PKG_STAGE_PATH))

pkg/stage:: all

pkg/stage:: pkg/path
	$(info Staging package        : $(PKG_STAGE_PATH))
	$(SILENT)rm -rf $(PKG_PATH) $(PKG_STAGE_PATH) ; mkdir -p $(PKG_STAGE_PATH)
	$(SILENT)tar -cf - $(PKG_FILES) | tar -C $(PKG_STAGE_PATH) -xf -
	$(SILENT)cd $(PKG_STAGE_PATH) && \
	  mkdir NVRAM Saves ROMs && \
	  echo "NVRAM files go here." > NVRAM/DIR.txt && \
	  echo "Save states go here." > Saves/DIR.txt && \
	  echo "Recommended (but not mandatory) location for ROM sets." > ROMs/DIR.txt && \
	  mv $(BIN_DIR)/$(SUPERMODEL_BINARY) . && rmdir $(BIN_DIR) && \
	  mv Docs//* . && rmdir Docs
	$(info Generating             : $(PKG_STAGE_PATH)/CHANGES.txt)
	$(SILENT)./Scripts/changelog.sh $(PKG_STAGE_PATH)/CHANGES.txt

pkg/path: set_version
	$(eval PKG_STAGE_PATH = pkg/supermodel-$(VERSION))
	$(eval PKG_PATH = pkg/supermodel-$(VERSION).$(PKG_TYPE))
	@echo $(PKG_PATH)

#
# Sets a VERSION variable based on the git sha.
#
set_version:
	$(eval VERSION = $(strip $(subst ',,0.3a-$(shell git show -s --format=%cd --date=format:%Y%m%d HEAD)-git-$(shell git rev-parse --short HEAD))))
	$(eval SUPERMODEL_BUILD_FLAGS += -DSUPERMODEL_VERSION=\"$(VERSION)\")

#
# Prints a message containing the version number to stdout for informational
# purposes only.
#
print_message: set_version
	$(info Building Supermodel Version $(VERSION))

#
# Print *only* the version directly.
#
version: set_version
	@echo $(VERSION)

#
# Supermodel binary
#
$(BIN_DIR)/$(SUPERMODEL_BINARY):	$(BIN_DIR) $(OBJ_FILES)
	$(info --------------------------------------------------------------------------------)
	$(info Linking Supermodel     : $(BIN_DIR)/$(SUPERMODEL_BINARY))
	$(SILENT)$(LD) $(OBJ_FILES) $(LDFLAGS)
	$(info --------------------------------------------------------------------------------)

#
# Disassembler binary (we assume $(LD) can also compile here, which is sloppy).
# $(PLATFORM_DIRS) is required on macOS, whose $(PLATFORM_LDFLAGS) pulls in
# Frameworks that must be downloaded.
#
$(BIN_DIR)/$(DISASSEMBLER_BINARY):  $(BIN_DIR) Src/CPU/PowerPC/PPCDisasm.cpp $(PLATFORM_DIRS)
	$(info --------------------------------------------------------------------------------)
	$(info Building Disassembler  : $(BIN_DIR)/$(DISASSEMBLER_BINARY))
	$(SILENT)$(LD) -o $(BIN_DIR)/$(DISASSEMBLER_BINARY) Src/CPU/PowerPC/PPCDisasm.cpp -DSTANDALONE $(OPT) $(WARN) $(addprefix -I,$(sort $(INCLUDE_DIRS))) $(CXXSTD) $(PLATFORM_LDFLAGS)
	$(info --------------------------------------------------------------------------------)

#
# Supermodel dylib
#
$(LIB_DIR)/$(SUPERMODEL_DYLIB):	$(LIB_DIR) $(OBJ_FILES)
	$(info --------------------------------------------------------------------------------)
	$(info Dynamic Linking Supermodel     : $(LIB_DIR)/$(SUPERMODEL_DYLIB))
	$(LD) $(OBJ_FILES) $(LDFLAGS)
	$(info --------------------------------------------------------------------------------)

#
# Directory to which binary will be written. Note that Supermodel is never
# packaged with this directory.
#
$(BIN_DIR):
	$(info Creating directory     : $(BIN_DIR))
	$(SILENT)mkdir $(BIN_DIR)

#
# Object directory
#
$(OBJ_DIR): | $(PLATFORM_DIRS)
	$(info Creating directory     : $(OBJ_DIR))
	$(SILENT)mkdir $(OBJ_DIR)

#
# Library directory
#
$(LIB_DIR): | $(PLATFORM_DIRS)
	$(info Creating directory     : $(LIB_DIR))
	$(SILENT)mkdir $(LIB_DIR)


###############################################################################
# Rules
###############################################################################

#
# Create list of auto-generated dependency files (which contain rules that make
# understands) and include them all.
#
AUTODEPS := $(patsubst %.o,%.d,$(OBJ_FILES))
-include $(AUTODEPS)

#
# VPATH is the search path for files. This trick allows a single %.cpp rule to
# be matched against files in sub-directories of Src/.
#
VPATH = $(INCLUDE_DIRS)

#
# Compilation rules that both auto-generate the dependency files and compile
# the source code. This technique is described in the reply by user "rr-" at:
# https://stackoverflow.com/questions/8025766/makefile-auto-dependency-generation
#
$(OBJ_DIR)/%.o:	%.cpp | $(OBJ_DIR)
	$(info Generating dependencies: $< -> $(OBJ_DIR)/$(*F).d)
	$(SILENT)$(CXX) -MM -MP -MT $(OBJ_DIR)/$(*F).o -MT $(OBJ_DIR)/$(*F).d $(CXXFLAGS_GENERATE_DEPENDENCIES) $< > $(OBJ_DIR)/$(*F).d
	$(info Compiling              : $< -> $@)
	$(SILENT)$(CXX) $(CXXFLAGS) $< -o $@

# Suppress all warnings for third-party packages that we cannot modify
$(OBJ_DIR)/glew.o: CFLAGS += -w

$(OBJ_DIR)/%.o:	%.c | $(OBJ_DIR)
	$(info Generating dependencies: $< -> $(OBJ_DIR)/$(*F).d)
	$(SILENT)$(CC) -MM -MP -MT $(OBJ_DIR)/$(*F).o -MT $(OBJ_DIR)/$(*F).d $(CFLAGS_GENERATE_DEPENDENCIES) $< > $(OBJ_DIR)/$(*F).d
	$(info Compiling              : $< -> $@)
	$(SILENT)$(CC) $(CFLAGS) $< -o $@

#
# Musashi 68K emulator
#
# All generated source files are emitted to the object directory. For MSVC,
# INLINE must be redefined as "static __inline", which is the syntax in C mode.
#
# The .exe suffix is absolutely required for native Windows non-MSYS builds!
#

MUSASHI_OUTFILE = $(OBJ_DIR)/m68kmake.exe # do not remove the .exe suffix!
MUSASHI_CFLAGS = -ISrc/CPU/68K/Musashi -I$(OBJ_DIR) -DINLINE="static inline" -Wno-unused-variable
MUSASHI_LDFLAGS = -o $(MUSASHI_OUTFILE) $(OBJ_DIR)/m68kmake.o -s

#
# m68kmake is a BUILD-TIME code generator: it must run on the build machine, so
# when cross-compiling it cannot be built with $(CC)/$(CFLAGS). Platform
# makefiles that cross-compile (Makefile.iOS) override HOST_CC/HOST_LD/
# HOST_CFLAGS; native builds inherit the target toolchain unchanged.
#
HOST_CC ?= $(CC)
HOST_LD ?= $(LD)
HOST_CFLAGS ?= $(CFLAGS)

$(MUSASHI_OUTFILE): Src/CPU/68K/Musashi/m68kmake.c Src/CPU/68K/Musashi/m68k_in.c | $(OBJ_DIR)
	$(info --------------------------------------------------------------------------------)
	$(info Compiling              : $< -> $(OBJ_DIR)/m68kmake.o)
	$(SILENT)$(HOST_CC) $< $(HOST_CFLAGS) -o $(OBJ_DIR)/m68kmake.o
	$(info Linking                : $(MUSASHI_OUTFILE))
	$(SILENT)$(HOST_LD) $(MUSASHI_LDFLAGS)

$(OBJ_DIR)/m68kops.h $(OBJ_DIR)/m68kops.c $(OBJ_DIR)/m68kopac.c $(OBJ_DIR)/m68kopdm.c $(OBJ_DIR)/m68kopnz.c:	$(MUSASHI_OUTFILE) Src/CPU/68K/Musashi/m68k_in.c Src/CPU/68K/Musashi/m68k.h Src/CPU/68K/Musashi/m68kconf.h | $(OBJ_DIR)
	$(info Generating 68K emulator: $@)
	@$(MUSASHI_OUTFILE) $(OBJ_DIR) Src/CPU/68K/Musashi/m68k_in.c

$(OBJ_DIR)/m68kcpu.o:	Src/CPU/68K/Musashi/m68kcpu.c $(OBJ_DIR)/m68kops.h Src/CPU/68K/Musashi/m68k.h Src/CPU/68K/Musashi/m68kconf.h | $(OBJ_DIR)
	$(info Compiling              : $< -> $@)
	@$(CC) $< $(CFLAGS) $(MUSASHI_CFLAGS) -o $(OBJ_DIR)/m68kcpu.o

$(OBJ_DIR)/m68kops.o:	$(OBJ_DIR)/m68kops.c $(OBJ_DIR)/m68kops.h Src/CPU/68K/Musashi/m68k.h Src/CPU/68K/Musashi/m68kconf.h $(MUSASHI_OUTFILE) | $(OBJ_DIR)
	$(info Compiling              : $< -> $@)
	@$(CC) $< $(CFLAGS) $(MUSASHI_CFLAGS) -o $@

$(OBJ_DIR)/m68kopac.o: $(OBJ_DIR)/m68kopac.c $(OBJ_DIR)/m68kops.h Src/CPU/68K/Musashi/m68k.h Src/CPU/68K/Musashi/m68kconf.h $(MUSASHI_OUTFILE) | $(OBJ_DIR)
	$(info Compiling              : $< -> $@)
	@$(CC) $< $(CFLAGS) $(MUSASHI_CFLAGS) -o $@

$(OBJ_DIR)/m68kopdm.o: $(OBJ_DIR)/m68kopdm.c $(OBJ_DIR)/m68kops.h Src/CPU/68K/Musashi/m68k.h Src/CPU/68K/Musashi/m68kconf.h $(MUSASHI_OUTFILE) | $(OBJ_DIR)
	$(info Compiling              : $< -> $@)
	@$(CC) $< $(CFLAGS) $(MUSASHI_CFLAGS) -o $@

$(OBJ_DIR)/m68kopnz.o: $(OBJ_DIR)/m68kopnz.c $(OBJ_DIR)/m68kops.h Src/CPU/68K/Musashi/m68k.h Src/CPU/68K/Musashi/m68kconf.h $(MUSASHI_OUTFILE) | $(OBJ_DIR)
	$(info Compiling              : $< -> $@)
	@$(CC) $< $(CFLAGS) $(MUSASHI_CFLAGS) -o $@
