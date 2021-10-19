cmake_policy(SET CMP0077 NEW)

SET(ENABLE_LIB_ONLY ON)
SET(ENABLE_STATIC_LIB ON)

add_subdirectory(bzip2)
include(lzip.cmake)

set(LZFSE_BUNDLE_MODE ON)
add_subdirectory(lzfse)