# ################################################################
# Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# ################################################################

cmake_minimum_required(VERSION 2.8.12 FATAL_ERROR)

# As of 2018-12-26 ZSTD has been validated to build with cmake version 3.13.2 new policies.
# Set and use the newest cmake policies that are validated to work
set(ZSTD_MAX_VALIDATED_CMAKE_MAJOR_VERSION "3")
set(ZSTD_MAX_VALIDATED_CMAKE_MINOR_VERSION "13") #Policies never changed at PATCH level
if("${CMAKE_MAJOR_VERSION}" LESS 3)
    set(ZSTD_CMAKE_POLICY_VERSION "${CMAKE_VERSION}")
elseif( "${ZSTD_MAX_VALIDATED_CMAKE_MAJOR_VERSION}" EQUAL "${CMAKE_MAJOR_VERSION}" AND
        "${ZSTD_MAX_VALIDATED_CMAKE_MINOR_VERSION}" GREATER "${CMAKE_MINOR_VERSION}")
    set(ZSTD_CMAKE_POLICY_VERSION "${CMAKE_VERSION}")
else()
    set(ZSTD_CMAKE_POLICY_VERSION "${ZSTD_MAX_VALIDATED_CMAKE_MAJOR_VERSION}.${ZSTD_MAX_VALIDATED_CMAKE_MINOR_VERSION}.0")
endif()
cmake_policy(VERSION ${ZSTD_CMAKE_POLICY_VERSION})

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/zstd-1.5.0/build/cmake/CMakeModules")
set(ZSTD_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/zstd-1.5.0")
set(LIBRARY_DIR ${ZSTD_SOURCE_DIR}/lib)

# Parse version
include(GetZstdLibraryVersion)
GetZstdLibraryVersion(${LIBRARY_DIR}/zstd.h zstd_VERSION_MAJOR zstd_VERSION_MINOR zstd_VERSION_PATCH)

if( CMAKE_MAJOR_VERSION LESS 3 )
    ## Provide cmake 3+ behavior for older versions of cmake
    project(zstd)
    set(PROJECT_VERSION_MAJOR ${zstd_VERSION_MAJOR})
    set(PROJECT_VERSION_MINOR ${zstd_VERSION_MINOR})
    set(PROJECT_VERSION_PATCH ${zstd_VERSION_PATCH})
    set(PROJECT_VERSION "${zstd_VERSION_MAJOR}.${zstd_VERSION_MINOR}.${zstd_VERSION_PATCH}")
    enable_language(C)   # Main library is in C
    enable_language(CXX) # Testing contributed code also utilizes CXX
else()
    project(zstd
            VERSION "${zstd_VERSION_MAJOR}.${zstd_VERSION_MINOR}.${zstd_VERSION_PATCH}"
            LANGUAGES C   # Main library is in C
            CXX # Testing contributed code also utilizes CXX
            )
endif()

message(STATUS "ZSTD VERSION: ${zstd_VERSION}")
set(zstd_HOMEPAGE_URL "http://www.zstd.net")
set(zstd_DESCRIPTION  "Zstandard is a real-time compression algorithm, providing high compression ratios.")

# Set a default build type if none was specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Release' as none was specified.")
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
    # Set the possible values of build type for cmake-gui
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

include(GNUInstallDirs)

#-----------------------------------------------------------------------------
# Add extra compilation flags
#-----------------------------------------------------------------------------
#include(AddZstdCompilationFlags)
include(CheckCXXCompilerFlag)
include(CheckCCompilerFlag)

function(EnableCompilerFlag _flag _C _CXX)
    string(REGEX REPLACE "\\+" "PLUS" varname "${_flag}")
    string(REGEX REPLACE "[^A-Za-z0-9]+" "_" varname "${varname}")
    string(REGEX REPLACE "^_+" "" varname "${varname}")
    string(TOUPPER "${varname}" varname)
    if (_C)
        CHECK_C_COMPILER_FLAG(${_flag} C_FLAG_${varname})
        if (C_FLAG_${varname})
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${_flag}" PARENT_SCOPE)
        endif ()
    endif ()
    if (_CXX)
        CHECK_CXX_COMPILER_FLAG(${_flag} CXX_FLAG_${varname})
        if (CXX_FLAG_${varname})
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${_flag}" PARENT_SCOPE)
        endif ()
    endif ()
endfunction()

macro(ADD_ZSTD_COMPILATION_FLAGS)
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" OR MINGW) #Not only UNIX but also WIN32 for MinGW
        #Set c++11 by default
        EnableCompilerFlag("-std=c++11" false true)
        #Set c99 by default
        EnableCompilerFlag("-std=c99" true false)
#        if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND MSVC)
#            # clang-cl normally maps -Wall to -Weverything.
#            EnableCompilerFlag("/clang:-Wall" true true)
#        else ()
#            EnableCompilerFlag("-Wall" true true)
#        endif ()
#        EnableCompilerFlag("-Wextra" true true)
#        EnableCompilerFlag("-Wundef" true true)
#        EnableCompilerFlag("-Wshadow" true true)
#        EnableCompilerFlag("-Wcast-align" true true)
#        EnableCompilerFlag("-Wcast-qual" true true)
#        EnableCompilerFlag("-Wstrict-prototypes" true false)
        # Enable asserts in Debug mode
        if (CMAKE_BUILD_TYPE MATCHES "Debug")
            EnableCompilerFlag("-DDEBUGLEVEL=1" true true)
        endif ()
    elseif (MSVC) # Add specific compilation flags for Windows Visual

        set(ACTIVATE_MULTITHREADED_COMPILATION "ON" CACHE BOOL "activate multi-threaded compilation (/MP flag)")
        if (CMAKE_GENERATOR MATCHES "Visual Studio" AND ACTIVATE_MULTITHREADED_COMPILATION)
            EnableCompilerFlag("/MP" true true)
        endif ()

        # UNICODE SUPPORT
        EnableCompilerFlag("/D_UNICODE" true true)
        EnableCompilerFlag("/DUNICODE" true true)
        # Enable asserts in Debug mode
        if (CMAKE_BUILD_TYPE MATCHES "Debug")
            EnableCompilerFlag("/DDEBUGLEVEL=1" true true)
        endif ()
    endif ()

    # Remove duplicates compilation flags
    foreach (flag_var CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
             CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO
             CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
             CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
        if( ${flag_var} )
            separate_arguments(${flag_var})
            string(REPLACE ";" " " ${flag_var} "${${flag_var}}")
        endif()
    endforeach ()

    if (MSVC AND ZSTD_USE_STATIC_RUNTIME)
        foreach (flag_var CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
                 CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO
                 CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
                 CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
            if ( ${flag_var} )
                string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
            endif()
        endforeach ()
    endif ()

endmacro()


ADD_ZSTD_COMPILATION_FLAGS()

# Always hide XXHash symbols
add_definitions(-DXXH_NAMESPACE=ZSTD_)

#-----------------------------------------------------------------------------
# Installation variables
#-----------------------------------------------------------------------------
#message(STATUS "CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
#message(STATUS "CMAKE_INSTALL_LIBDIR: ${CMAKE_INSTALL_LIBDIR}")

#-----------------------------------------------------------------------------
# Options
#-----------------------------------------------------------------------------

# Legacy support
option(ZSTD_LEGACY_SUPPORT "LEGACY SUPPORT" ON)

if (ZSTD_LEGACY_SUPPORT)
    message(STATUS "ZSTD_LEGACY_SUPPORT defined!")
    add_definitions(-DZSTD_LEGACY_SUPPORT=5)
else ()
    message(STATUS "ZSTD_LEGACY_SUPPORT not defined!")
    add_definitions(-DZSTD_LEGACY_SUPPORT=0)
endif ()

# Multi-threading support
option(ZSTD_MULTITHREAD_SUPPORT "MULTITHREADING SUPPORT" ON)

if (ZSTD_MULTITHREAD_SUPPORT)
    message(STATUS "ZSTD_MULTITHREAD_SUPPORT is enabled")
else ()
    message(STATUS "ZSTD_MULTITHREAD_SUPPORT is disabled")
endif ()

option(ZSTD_BUILD_PROGRAMS "BUILD PROGRAMS" ON)
option(ZSTD_BUILD_CONTRIB "BUILD CONTRIB" OFF)

# Respect the conventional CMake option for enabling tests if it was specified on the first configure
if (BUILD_TESTING)
    set(ZSTD_BUILD_TESTS_default ON)
else()
    set(ZSTD_BUILD_TESTS_default OFF)
endif()
option(ZSTD_BUILD_TESTS "BUILD TESTS" ${ZSTD_BUILD_TESTS_default})
if (MSVC)
    option(ZSTD_USE_STATIC_RUNTIME "LINK TO STATIC RUN-TIME LIBRARIES" OFF)
endif ()

#-----------------------------------------------------------------------------
# External dependencies
#-----------------------------------------------------------------------------
if (ZSTD_MULTITHREAD_SUPPORT AND UNIX)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    if(CMAKE_USE_PTHREADS_INIT)
        set(THREADS_LIBS "${CMAKE_THREAD_LIBS_INIT}")
    else()
        message(SEND_ERROR "ZSTD currently does not support thread libraries other than pthreads")
    endif()
endif ()

project(libzstd C)

set(CMAKE_INCLUDE_CURRENT_DIR TRUE)
option(ZSTD_BUILD_STATIC "BUILD STATIC LIBRARIES" ON)
option(ZSTD_BUILD_SHARED "BUILD SHARED LIBRARIES" ON)

if(NOT ZSTD_BUILD_SHARED AND NOT ZSTD_BUILD_STATIC)
    message(SEND_ERROR "You need to build at least one flavor of libzstd")
endif()

# Define library directory, where sources and header files are located
include_directories(${LIBRARY_DIR} ${LIBRARY_DIR}/common)

file(GLOB CommonSources ${LIBRARY_DIR}/common/*.c)
file(GLOB CompressSources ${LIBRARY_DIR}/compress/*.c)
file(GLOB DecompressSources ${LIBRARY_DIR}/decompress/*.c)
file(GLOB DictBuilderSources ${LIBRARY_DIR}/dictBuilder/*.c)

set(Sources
    ${CommonSources}
    ${CompressSources}
    ${DecompressSources}
    ${DictBuilderSources})

file(GLOB CommonHeaders ${LIBRARY_DIR}/common/*.h)
file(GLOB CompressHeaders ${LIBRARY_DIR}/compress/*.h)
file(GLOB DecompressHeaders ${LIBRARY_DIR}/decompress/*.h)
file(GLOB DictBuilderHeaders ${LIBRARY_DIR}/dictBuilder/*.h)

set(Headers
    ${LIBRARY_DIR}/zstd.h
    ${CommonHeaders}
    ${CompressHeaders}
    ${DecompressHeaders}
    ${DictBuilderHeaders})

if (ZSTD_LEGACY_SUPPORT)
    set(LIBRARY_LEGACY_DIR ${LIBRARY_DIR}/legacy)
    include_directories(${LIBRARY_LEGACY_DIR})

    set(Sources ${Sources}
        ${LIBRARY_LEGACY_DIR}/zstd_v01.c
        ${LIBRARY_LEGACY_DIR}/zstd_v02.c
        ${LIBRARY_LEGACY_DIR}/zstd_v03.c
        ${LIBRARY_LEGACY_DIR}/zstd_v04.c
        ${LIBRARY_LEGACY_DIR}/zstd_v05.c
        ${LIBRARY_LEGACY_DIR}/zstd_v06.c
        ${LIBRARY_LEGACY_DIR}/zstd_v07.c)

    set(Headers ${Headers}
        ${LIBRARY_LEGACY_DIR}/zstd_legacy.h
        ${LIBRARY_LEGACY_DIR}/zstd_v01.h
        ${LIBRARY_LEGACY_DIR}/zstd_v02.h
        ${LIBRARY_LEGACY_DIR}/zstd_v03.h
        ${LIBRARY_LEGACY_DIR}/zstd_v04.h
        ${LIBRARY_LEGACY_DIR}/zstd_v05.h
        ${LIBRARY_LEGACY_DIR}/zstd_v06.h
        ${LIBRARY_LEGACY_DIR}/zstd_v07.h)
endif ()

if (MSVC)
    set(MSVC_RESOURCE_DIR ${ZSTD_SOURCE_DIR}/build/VS2010/libzstd-dll)
    set(PlatformDependResources ${MSVC_RESOURCE_DIR}/libzstd-dll.rc)
endif ()

# Split project to static and shared libraries build
set(library_targets)
if (ZSTD_BUILD_SHARED)
    add_library(libzstd_shared SHARED ${Sources} ${Headers} ${PlatformDependResources})
    list(APPEND library_targets libzstd_shared)
    if (ZSTD_MULTITHREAD_SUPPORT)
        set_property(TARGET libzstd_shared APPEND PROPERTY COMPILE_DEFINITIONS "ZSTD_MULTITHREAD")
        if (UNIX)
            target_link_libraries(libzstd_shared ${THREADS_LIBS})
        endif ()
    endif()
endif ()
if (ZSTD_BUILD_STATIC)
    add_library(libzstd_static STATIC ${Sources} ${Headers})
    list(APPEND library_targets libzstd_static)
    if (ZSTD_MULTITHREAD_SUPPORT)
        set_property(TARGET libzstd_static APPEND PROPERTY COMPILE_DEFINITIONS "ZSTD_MULTITHREAD")
        if (UNIX)
            target_link_libraries(libzstd_static ${THREADS_LIBS})
        endif ()
    endif ()
endif ()

# Add specific compile definitions for MSVC project
if (MSVC)
    if (ZSTD_BUILD_SHARED)
        set_property(TARGET libzstd_shared APPEND PROPERTY COMPILE_DEFINITIONS "ZSTD_DLL_EXPORT=1;ZSTD_HEAPMODE=0;_CONSOLE;_CRT_SECURE_NO_WARNINGS")
    endif ()
    if (ZSTD_BUILD_STATIC)
        set_property(TARGET libzstd_static APPEND PROPERTY COMPILE_DEFINITIONS "ZSTD_HEAPMODE=0;_CRT_SECURE_NO_WARNINGS")
    endif ()
endif ()

# With MSVC static library needs to be renamed to avoid conflict with import library
if (MSVC)
    set(STATIC_LIBRARY_BASE_NAME zstd_static)
else ()
    set(STATIC_LIBRARY_BASE_NAME zstd)
endif ()

# Define static and shared library names
#if (ZSTD_BUILD_STATIC)
    set_target_properties(
            libzstd_static
            PROPERTIES
            POSITION_INDEPENDENT_CODE On
            OUTPUT_NAME ${STATIC_LIBRARY_BASE_NAME})
#endif ()