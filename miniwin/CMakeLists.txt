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

  # DirectSound
  src/dsound/dsound.cpp

  # DirectInput
  src/dinput/dinput.cpp
)

target_compile_definitions(miniwin PUBLIC MINIWIN)

target_include_directories(miniwin
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include/miniwin"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/internal"
)

# POSIX shims for MSVC-specific CRT headers (<io.h>); real Windows provides these natively.
if (NOT WIN32)
  target_include_directories(miniwin PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/shims")
endif()

target_link_libraries(miniwin PRIVATE SDL3::SDL3 miniaudio)
