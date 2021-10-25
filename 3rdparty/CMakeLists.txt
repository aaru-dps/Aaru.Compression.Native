cmake_policy(SET CMP0077 NEW)

include(bzip2.cmake)

include(lzip.cmake)

include(lzfse.cmake)

#
#set(LZFSE_BUNDLE_MODE ON)
#add_subdirectory(lzfse)
#
#set(ZSTD_BUILD_PROGRAMS OFF)
#set(ZSTD_BUILD_SHARED OFF)
#set(ZSTD_MULTITHREAD_SUPPORT OFF) # Not really sure if it works properly
#set(ZSTD_LEGACY_SUPPORT ON) # Unsure if may ever be needed, just in case
#add_subdirectory(zstd-1.5.0/build/cmake)
#
#include(lzma.cmake)
#
#cmake_policy(SET CMP0077 NEW)
#set(BUILD_CXXLIBS OFF)
#set(BUILD_PROGRAMS OFF)
#set(BUILD_EXAMPLES OFF)
#set(BUILD_DOCS OFF)
#set(INSTALL_MANPAGES OFF)
#set(INSTALL_PKGCONFIG_MODULES OFF)
#set(INSTALL_CMAKE_CONFIG_MODULE OFF)
#set(WITH_OGG OFF)
#set(WITH_OGG OFF)
#set(BUILD_TESTING OFF)
#set(BUILD_SHARED_LIBS OFF)
#if("${CMAKE_C_PLATFORM_ID}" MATCHES "MinGW" AND ("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "aarch64" OR "${CMAKE_SYSTEM_PROCESSOR}" MATCHES "arm"))
#    set(WITH_STACK_PROTECTOR OFF)
#endif()
#add_subdirectory(flac)
