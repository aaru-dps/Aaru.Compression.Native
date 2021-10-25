include(bzip2.cmake)

include(lzip.cmake)

include(lzfse.cmake)

include(lzma.cmake)

include(zstd.cmake)

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
