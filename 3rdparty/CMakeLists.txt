set(CMAKE_C_CLANG_TIDY)
set(CMAKE_CXX_CLANG_TIDY)

# miniaudio (vendored submodule, pinned to 0.11.25 like isle-portable)
add_library(miniaudio STATIC
  miniaudio/extras/miniaudio_split/miniaudio.c
)
target_include_directories(miniaudio PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/miniaudio/extras/miniaudio_split")

# Disable most features since we don't need them: miniaudio is used purely as an
# in-memory mixing engine; SDL3 owns the audio device.
target_compile_definitions(miniaudio PUBLIC
  MA_ENABLE_ONLY_SPECIFIC_BACKENDS
  MA_NO_DECODING
  MA_NO_ENCODING
  MA_NO_WAV
  MA_NO_FLAC
  MA_NO_MP3
  MA_NO_DEVICE_IO
  MA_NO_RESOURCE_MANAGER
  MA_NO_GENERATION
  MA_NO_THREADING
  MA_NO_RUNTIME_LINKING
)
