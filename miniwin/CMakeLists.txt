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

# GLES3 is the only render backend for the SDL2 port.
list(APPEND GRAPHICS_BACKENDS USE_OPENGLES3)

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

target_link_libraries(miniwin PRIVATE SDL2::SDL2 miniaudio)
