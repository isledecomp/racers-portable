add_library(miniwin STATIC
  # Core Win32
  src/windows/windows.cpp
  src/windows/handles.cpp
  src/windows/files.cpp
  src/windows/registry.cpp
  src/windows/mmsystem.cpp
  src/windows/events.cpp
  src/windows/guids.cpp

  # DirectDraw
  src/ddraw/ddraw.cpp
  src/ddraw/ddsurface.cpp
  src/ddraw/ddpalette.cpp

  # Direct3D (immediate mode)
  src/d3d/d3d.cpp
  src/d3d/d3ddevice.cpp

  # Render backend selection/dispatch (always compiled)
  src/d3d/backends/backends.cpp

  # DirectSound
  src/dsound/dsound.cpp

  # DirectInput
  src/dinput/dinput.cpp
)

# Render backends are selected at build time per platform, mirroring isle-portable:
# each entry becomes a USE_* compile definition, and the matching source is compiled
# only when its entry is present.
list(APPEND GRAPHICS_BACKENDS USE_SDL_GPU)

# Desktop OpenGL 3.3 core. Reached through SDL_GL_GetProcAddress, so no OpenGL library
# is linked (SDL loads it) — unlike isle's OpenGL 1.x backend. Not available on the web
# (WebGL/GLES only) or typical console/handheld targets.
if(NOT (EMSCRIPTEN OR ANDROID OR IOS OR VITA OR NINTENDO_SWITCH OR NINTENDO_3DS OR WINDOWS_STORE))
  list(APPEND GRAPHICS_BACKENDS USE_OPENGL3)
endif()

# OpenGL ES 3 / WebGL2. Also loaded through SDL_GL_GetProcAddress (no GLES library is
# linked; SDL supplies GLES/ANGLE natively and WebGL2 under Emscripten). This is the web
# target's only backend and an optional desktop/mobile one; excluded only where SDL has
# no GL/GLES at all.
if(NOT (VITA OR NINTENDO_SWITCH OR NINTENDO_3DS OR WINDOWS_STORE))
  list(APPEND GRAPHICS_BACKENDS USE_OPENGLES3)
endif()

# SDL_GPU has no web backend; drop it there (the GLES3 backend covers web).
if(EMSCRIPTEN)
  list(REMOVE_ITEM GRAPHICS_BACKENDS USE_SDL_GPU)
endif()

if(USE_SDL_GPU IN_LIST GRAPHICS_BACKENDS)
  target_sources(miniwin PRIVATE src/d3d/backends/sdlgpu/renderer.cpp)
endif()

if(USE_OPENGL3 IN_LIST GRAPHICS_BACKENDS)
  target_sources(miniwin PRIVATE src/d3d/backends/opengl3/renderer.cpp)
endif()

if(USE_OPENGLES3 IN_LIST GRAPHICS_BACKENDS)
  target_sources(miniwin PRIVATE src/d3d/backends/opengles3/renderer.cpp)
endif()

target_compile_definitions(miniwin PUBLIC MINIWIN ${GRAPHICS_BACKENDS})

target_include_directories(miniwin
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include/miniwin"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/internal"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/d3d/backends"
)

# POSIX shims for MSVC-specific CRT headers (<io.h>); real Windows provides these natively.
if (NOT WIN32)
  target_include_directories(miniwin PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/shims")
endif()

target_link_libraries(miniwin PRIVATE SDL3::SDL3 miniaudio)

# Offline SDL_GPU shader (re)generation — developer/CI only. The committed header
# src/d3d/backends/sdlgpu/shaders/generated/miniwin_sdlgpu_shaders.h is what the build
# compiles; this target refreshes it from the GLSL sources when glslang + SDL_shadercross
# are on PATH. DXIL (D3D12) needs a DXC-enabled shadercross, which has no macOS prebuilt,
# so it is filled in by a Linux CI job; SPIR-V + MSL are produced everywhere.
if(USE_SDL_GPU IN_LIST GRAPHICS_BACKENDS)
  find_program(RACERS_GLSLANG_BIN NAMES glslangValidator glslang)
  find_program(RACERS_SHADERCROSS_BIN NAMES shadercross)
  if(RACERS_GLSLANG_BIN AND RACERS_SHADERCROSS_BIN)
    set(_sdlgpu_shaders "${CMAKE_CURRENT_SOURCE_DIR}/src/d3d/backends/sdlgpu/shaders")
    add_custom_target(sdlgpu-shaders
      COMMAND python3 "${_sdlgpu_shaders}/gen_shaders.py"
        "${_sdlgpu_shaders}/src"
        "${_sdlgpu_shaders}/generated/miniwin_sdlgpu_shaders.h"
        "${RACERS_GLSLANG_BIN}" "${RACERS_SHADERCROSS_BIN}"
      COMMENT "Regenerating SDL_GPU shader bytecode header"
      VERBATIM
    )
  endif()
endif()
