cmake_minimum_required(VERSION 3.25...4.2 FATAL_ERROR)

project(racers-portable LANGUAGES CXX C VERSION 0.1)

if (EMSCRIPTEN)
  add_compile_options(-pthread -gsource-map)
  add_link_options(
    -sUSE_WEBGL2=1
    -sMIN_WEBGL_VERSION=2
    -sALLOW_MEMORY_GROWTH=1
    -sINITIAL_MEMORY=128mb
    -sMAXIMUM_MEMORY=2gb
    -sUSE_PTHREADS=1
    -sPROXY_TO_PTHREAD=1
    -sOFFSCREENCANVAS_SUPPORT=1
    -sPTHREAD_POOL_SIZE_STRICT=0
    -sFORCE_FILESYSTEM=1
    -sWASMFS=1
    -sEXIT_RUNTIME=1
    -sABORT_ON_WASM_EXCEPTIONS=1
    -gsource-map
  )
  set(SDL_PTHREADS ON CACHE BOOL "Enable SDL pthreads" FORCE)
endif()

include(CheckCXXSourceCompiles)
include(CMakeDependentOption)
include(FetchContent)
include(GenerateExportHeader)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# The decomp sources predate type-based alias analysis and rely on MSVC 6.0
# semantics — e.g. GolList's overlapped head/sentinel pseudo-links alias
# GolListLink and GolList fields onto the same memory. TBAA-driven reordering
# corrupts those structures at -O1 and above, so build everything the way the
# original compiler understood the code.
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  add_compile_options(-fno-strict-aliasing)
endif()

if (MINGW)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static -static-libgcc -static-libstdc++")
endif()

option(RACERS_BUILD_APP "Build LEGORacers application" ON)
option(RACERS_ASAN "Enable Address Sanitizer" OFF)
option(RACERS_UBSAN "Enable Undefined Behavior Sanitizer" OFF)
option(RACERS_WERROR "Treat warnings as errors" OFF)
option(ENABLE_CLANG_TIDY "Enable clang-tidy")
option(DOWNLOAD_DEPENDENCIES "Download dependencies" ON)
set(RACERS_EMSCRIPTEN_HOST "" CACHE STRING "Host URL for Emscripten data streaming (e.g., https://example.com)")
cmake_dependent_option(BUILD_SHARED_LIBS "Build goldp as a shared library" ON "NOT EMSCRIPTEN" OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" CACHE PATH "Directory where to put executables and dll")

if (NOT DEFINED CMAKE_POSITION_INDEPENDENT_CODE)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

if (ENABLE_CLANG_TIDY)
  find_program(CLANG_TIDY_BIN NAMES "clang-tidy")
  set(CMAKE_C_CLANG_TIDY "${CLANG_TIDY_BIN}")
  set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_BIN}")
endif()

if (RACERS_ASAN)
  add_compile_options(-fsanitize=address)
  add_link_options(-fsanitize=address)
endif()

if (RACERS_UBSAN)
  add_compile_options(-fsanitize=undefined)
  add_link_options(-fsanitize=undefined)
endif()

if (RACERS_WERROR)
  if (MSVC)
    add_compile_options(/WX)
  else()
    add_compile_options(-Werror)
  endif()
endif()

# --- SDL3 ---
if (DOWNLOAD_DEPENDENCIES)
  set(RACERS_SDL3_GIT_REPO "https://github.com/libsdl-org/SDL.git" CACHE STRING "Git repository for SDL3")
  set(RACERS_SDL3_GIT_TAG "release-3.4.8" CACHE STRING "Git tag for SDL3")
  set(SDL_TEST_LIBRARY OFF)
  set(SDL_TESTS OFF)
  set(SDL_EXAMPLES OFF)
  FetchContent_Declare(
    SDL3
    GIT_REPOSITORY "${RACERS_SDL3_GIT_REPO}"
    GIT_TAG "${RACERS_SDL3_GIT_TAG}"
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(SDL3)
else()
  find_package(SDL3 REQUIRED CONFIG)
endif()

add_subdirectory(3rdparty)
add_subdirectory(miniwin)

# --- Decompiled game code ---
#
# The decompilation project (git remote 'racers') is the source of truth for these
# directories; every source file participates in the build, so upstream file additions are
# picked up automatically by the globs below.
file(GLOB_RECURSE COMMON_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/common/src/*.cpp")
file(GLOB_RECURSE GOLDP_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/GolDP/src/*.cpp")
file(GLOB_RECURSE LEGORACERS_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/LEGORacers/src/*.cpp")

# The software rasterizer is hand-written x86 assembly and largely unimplemented in the
# decompilation; the port is hardware-accelerated only (for now) and never advertises a
# software device.
list(REMOVE_ITEM GOLDP_SOURCES
  "${CMAKE_SOURCE_DIR}/GolDP/src/render/golrasterizers1.cpp"
  "${CMAKE_SOURCE_DIR}/GolDP/src/render/golrasterizers2.cpp"
  "${CMAKE_SOURCE_DIR}/GolDP/src/render/golsoftwarerenderer.cpp"
)
list(APPEND GOLDP_SOURCES "${CMAKE_SOURCE_DIR}/GolDP/portable/golsoftwarerendererstub.cpp")

# COM GUID storage is provided by miniwin.
list(REMOVE_ITEM LEGORACERS_SOURCES "${CMAKE_SOURCE_DIR}/LEGORacers/src/app/guids.cpp")

# GolDP.dll ("Gol" renderer backend), linked directly instead of LoadLibrary'd.
add_library(goldp ${GOLDP_SOURCES} ${COMMON_SOURCES})
target_compile_definitions(goldp PRIVATE BUILDING_GOL)
target_include_directories(goldp PRIVATE
  "${PROJECT_SOURCE_DIR}/GolDP/include"
  "${PROJECT_SOURCE_DIR}/GolDP"
  "${PROJECT_SOURCE_DIR}/common/include"
  "${PROJECT_SOURCE_DIR}/util"
)
generate_export_header(goldp BASE_NAME GOLDP EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/goldp_export.h")
target_include_directories(goldp PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
set_property(TARGET goldp PROPERTY C_VISIBILITY_PRESET hidden)
set_property(TARGET goldp PROPERTY CXX_VISIBILITY_PRESET hidden)
target_link_libraries(goldp PRIVATE miniwin SDL3::SDL3)
if (WIN32 AND BUILD_SHARED_LIBS)
  target_sources(goldp PRIVATE GolDP/GolDP.def)
endif()
set_property(TARGET goldp PROPERTY OUTPUT_NAME "GolDP")

# LEGORacers.exe
if (RACERS_BUILD_APP)
  add_executable(legoracers WIN32 ${LEGORACERS_SOURCES} ${COMMON_SOURCES})
  target_compile_definitions(legoracers PRIVATE BUILDING_LEGORACERS DIRECTINPUT_VERSION=0x500)
  target_include_directories(legoracers PRIVATE
    "${PROJECT_SOURCE_DIR}/LEGORacers/include"
    "${PROJECT_SOURCE_DIR}/LEGORacers"
    "${PROJECT_SOURCE_DIR}/common/include"
    "${PROJECT_SOURCE_DIR}/util"
    "${PROJECT_SOURCE_DIR}/GolDP/include"
  )
  target_link_libraries(legoracers PRIVATE goldp miniwin indeo5 SDL3::SDL3)

  # The decomp headers use MSVC-isms (calling conventions, #pragma warning) that
  # compat.h neutralizes; not every header includes it, so inject it up front for
  # the decomp code. Scoped to these targets: C libraries must not see its C++
  # macros.
  if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(legoracers PRIVATE -include "${CMAKE_SOURCE_DIR}/util/compat.h")
    target_compile_options(goldp PRIVATE -include "${CMAKE_SOURCE_DIR}/util/compat.h")
  endif()
  if (WIN32)
    target_sources(legoracers PRIVATE LEGORacers/racers.rc)
    set_property(SOURCE LEGORacers/racers.rc APPEND PROPERTY OBJECT_DEPENDS "${PROJECT_SOURCE_DIR}/LEGORacers/resources/racers.ico")
  endif()
  set_property(TARGET legoracers PROPERTY OUTPUT_NAME "LEGORacers")
endif()

find_program(CLANGFORMAT_BIN NAMES clang-format)
if (EXISTS "${CLANGFORMAT_BIN}")
  execute_process(COMMAND "${CLANGFORMAT_BIN}" --version
    OUTPUT_VARIABLE "CLANGFORMAT_VERSION_OUTPUT"
    RESULT_VARIABLE "CLANGFORMAT_RESULT"
  )
  if (CLANGFORMAT_RESULT EQUAL 0 AND CLANGFORMAT_VERSION_OUTPUT MATCHES "version ([0-9\\.]+)")
    set(CLANGFORMAT_VERSION "${CMAKE_MATCH_1}")
    set(CLANGFORMAT_VERSION_REQUIRED "17.0")
    message(DEBUG "Found clang-format version ${CLANGFORMAT_VERSION} (needs ${CLANGFORMAT_VERSION_REQUIRED}")
    if (CLANGFORMAT_VERSION VERSION_GREATER_EQUAL "${CLANGFORMAT_VERSION_REQUIRED}")
      file(GLOB_RECURSE racers_sources CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/LEGORacers/*.cpp"
        "${PROJECT_SOURCE_DIR}/LEGORacers/*.h"
        "${PROJECT_SOURCE_DIR}/GolDP/*.cpp"
        "${PROJECT_SOURCE_DIR}/GolDP/*.h"
        "${PROJECT_SOURCE_DIR}/common/*.cpp"
        "${PROJECT_SOURCE_DIR}/common/*.h"
        "${PROJECT_SOURCE_DIR}/miniwin/src/d3d/backends/opengl3/*.h"
        "${PROJECT_SOURCE_DIR}/miniwin/src/d3d/backends/opengl3/*.cpp"
        "${PROJECT_SOURCE_DIR}/miniwin/src/d3d/*.h"
        "${PROJECT_SOURCE_DIR}/miniwin/src/d3d/*.cpp"
        "${PROJECT_SOURCE_DIR}/miniwin/src/ddraw/*.h"
        "${PROJECT_SOURCE_DIR}/miniwin/src/ddraw/*.cpp"
        "${PROJECT_SOURCE_DIR}/miniwin/src/dinput/*.cpp"
        "${PROJECT_SOURCE_DIR}/miniwin/src/dsound/*.cpp"
        "${PROJECT_SOURCE_DIR}/miniwin/src/internal/*.h"
        "${PROJECT_SOURCE_DIR}/miniwin/src/windows/*.cpp"
      )
      string(REPLACE ";" "\n" racers_sources_lines "${racers_sources}")
      file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/racers_sources.txt" "${racers_sources_lines}\n")
      add_custom_target(clang-format ${CLANGFORMAT_BIN} -i "--files=${CMAKE_CURRENT_BINARY_DIR}/racers_sources.txt")
    endif()
  endif()
endif()
