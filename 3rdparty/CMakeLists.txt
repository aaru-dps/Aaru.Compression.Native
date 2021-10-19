cmake_policy(SET CMP0077 NEW)

SET(ENABLE_LIB_ONLY ON)
SET(ENABLE_STATIC_LIB ON)

add_subdirectory(bzip2)
include(lzip.cmake)

set(LZFSE_BUNDLE_MODE ON)
add_subdirectory(lzfse)

set(ZSTD_BUILD_PROGRAMS OFF)
set(ZSTD_BUILD_SHARED OFF)
set(ZSTD_MULTITHREAD_SUPPORT OFF) # Not really sure if it works properly
set(ZSTD_LEGACY_SUPPORT ON) # Unsure if may ever be needed, just in case
add_subdirectory(zstd-1.5.0/build/cmake)