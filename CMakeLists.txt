cmake_minimum_required(VERSION 3.15)

# This file is part of the Aaru Data Preservation Suite.
# Copyright (c) 2019-2026 Natalia Portillo.
#
# This library is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 2.1 of the
# License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, see <http://www.gnu.org/licenses/>.

if(APPLE)
  # Too early cmake has not yet set it
  if((NOT DEFINED CMAKE_SYSTEM_PROCESSOR) AND (NOT DEFINED AARU_MACOS_TARGET_ARCH))
    execute_process(COMMAND uname -m OUTPUT_VARIABLE AARU_MACOS_TARGET_ARCH OUTPUT_STRIP_TRAILING_WHITESPACE)
  endif()

  if(NOT DEFINED AARU_MACOS_TARGET_ARCH)
    if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64")
      set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "Build architectures for macOS" FORCE)
    elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm64")
      set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Build architectures for macOS" FORCE)
    else()
      message(FATAL_ERROR "Unknown system processor ${CMAKE_SYSTEM_PROCESSOR} for macOS")
    endif()
  elseif(AARU_MACOS_TARGET_ARCH STREQUAL "x86_64")
    set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "Build architectures for macOS" FORCE)
  elseif(AARU_MACOS_TARGET_ARCH STREQUAL "arm64")
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Build architectures for macOS" FORCE)
  else()
    message(FATAL_ERROR "Unknown Aaru target architecture ${AARU_MACOS_TARGET_ARCH} for macOS")
  endif()

  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum OS X deployment version")
endif(APPLE)

project("Aaru.Compression.Native" C)

if("${CMAKE_C_COMPILER_ID}" MATCHES "MSVC" AND "${CMAKE_C_COMPILER_ARCHITECTURE_ID}" MATCHES "ARMV7")
  set(CMAKE_C_STANDARD 11)
else()
  set(CMAKE_C_STANDARD 99)
endif()

if("${CMAKE_C_PLATFORM_ID}" MATCHES "MinGW")
  if("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "aarch64" OR "${CMAKE_SYSTEM_PROCESSOR}" MATCHES "arm")
    set(WIN32 TRUE)
  endif()

  add_link_options(-static-libgcc)
endif()

if(DEFINED AARU_MACOS_TARGET_ARCH)
  message("Requested target architecture: ${AARU_MACOS_TARGET_ARCH}")
endif()
message("Detected system processor: ${CMAKE_SYSTEM_PROCESSOR}")
message("Detected vs platform name: ${CMAKE_C_COMPILER_ARCHITECTURE_ID}")
message("Detected compiler: ${CMAKE_C_COMPILER_ID}")
message("Detected build type: ${CMAKE_BUILD_TYPE}")
message("Detected platform: ${CMAKE_C_PLATFORM_ID}")
message("Size of (void*): ${CMAKE_SIZEOF_VOID_P}")

# Check if target is 64-bit
if("${CMAKE_SIZEOF_VOID_P}" MATCHES "8" OR "${CMAKE_C_COMPILER_ARCHITECTURE_ID}" MATCHES "x64" OR "${CMAKE_SYSTEM_PROCESSOR}" MATCHES "x86_64" OR "${CMAKE_SYSTEM_PROCESSOR}" MATCHES "AMD64" OR "${CMAKE_SYSTEM_PROCESSOR}" MATCHES "aarch64")
  set(ARCHITECTURE_IS_64BIT TRUE)
endif()

if("${CMAKE_BUILD_TYPE}" MATCHES "Release" OR "${CMAKE_BUILD_TYPE}" MATCHES "RelWithDebInfo" OR "${CMAKE_BUILD_TYPE}" MATCHES "MinSizeRel")
  if("${CMAKE_BUILD_TYPE}" MATCHES "Release")
    add_compile_definitions(NDEBUG)
  endif()

  if("${CMAKE_C_COMPILER_ID}" MATCHES "MSVC")
    add_compile_options("/O2 /Ob2 /Oi /Ot /Oy /Og /fp:fast")
    if(${CMAKE_C_COMPILER_ARCHITECTURE_ID} MATCHES "X86")
      add_compile_options("/arch:SSE2")
    elseif(${CMAKE_C_COMPILER_ARCHITECTURE_ID} MATCHES "ARM")
      add_compile_options("/arch:VFPv4")
    endif()
  else()
    add_compile_options(-O3 -ffast-math)

    if("${CMAKE_C_COMPILER_ID}" MATCHES "AppleClang")
      if(${AARU_MACOS_TARGET_ARCH} MATCHES "x86_64")
        add_compile_options(-march=ivybridge -mtune=westmere -msse4.2)
      elseif(${AARU_MACOS_TARGET_ARCH} MATCHES "arm64")
        add_compile_options(-mcpu=apple-m1 -mtune=apple-m1)
      endif()
    endif()

    if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64" OR ${CMAKE_SYSTEM_PROCESSOR} MATCHES "i686" OR ${CMAKE_SYSTEM_PROCESSOR} MATCHES "AMD64")
      if(NOT "${CMAKE_C_COMPILER_ID}" MATCHES "AppleClang")
        add_compile_options(-march=westmere -mtune=skylake -mfpmath=sse)
        add_compile_options(-msse4.2)
      endif()

      if(NOT "${CMAKE_C_PLATFORM_ID}" MATCHES "MinGW")
        add_compile_options(-flto)
      endif()
    elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "aarch64")
      if(NOT "${CMAKE_C_COMPILER_ID}" MATCHES "AppleClang")
        add_compile_options(-mtune=cortex-a53)
      endif()

      if(NOT "${CMAKE_C_PLATFORM_ID}" MATCHES "MinGW")
        add_compile_options(-flto)
      endif()
    elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "armv7l" OR ${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm" AND NOT "${CMAKE_C_COMPILER_ID}" MATCHES "AppleClang")
      if("${CMAKE_C_COMPILER_ID}" MATCHES "Clang")
        add_compile_options(-mtune=cortex-a53 -mcpu=cortex-a7)
      else()
        add_compile_options(-mtune=cortex-a53 -march=armv7-a+neon-vfpv4)
      endif()
    elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "mips")
      if(NOT "${CMAKE_C_PLATFORM_ID}" MATCHES "MinGW")
        add_compile_options(-flto)
      endif()
    endif()
  endif()
endif()

# Ensure all static libraries are built with PIC so they can be linked into the shared library
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_subdirectory(3rdparty)

add_library("Aaru.Compression.Native" SHARED library.c apple_rle.c apple_rle.h apple_lzh.c apple_lzh.h adc.c adc.h lzip.c flac.c flac.h
            zoo/lzd.c zoo/lzd.h zoo/lzh.c zoo/decode.c zoo/huf.c zoo/io.c zoo/lh5.c zoo/lh5.h zoo/lzh.h zoo/ar.h zoo/maketbl.c
            arc/pack.c arc/squeeze.c arc/crunch.c arc/lzw.c
            pak/crush.c pak/distill.c pak/bitstream.c pak/bitstream.h pak/lzw.c pak/lzw.h pak/prefixcode.c
            pak/prefixcode.h
            ha/acoder.c
            ha/acoder.h
            ha/asc.c
            ha/asc.h
            ha/decompress.c
            ha/hsc.c
            ha/hsc.h
            ha/internal.h
            ha/swdict.c
            ha/swdict.h
            lha/bitio.c
            lha/bitio.h
            lha/lzss.c
            lha/lzss.h
            lha/huffman.c
            lha/huffman.h
            lha/larc.c
            lha/larc.h
            lha/lh1.c
            lha/lh1.h
            lha/lh_static.c
            lha/lh_static.h
            lha/lh_old.c
            lha/lh_old.h
            lha/pmarc1.c
            lha/pmarc1.h
            ace/ace.c
            ace/ace.h
            ace/ace_internal.h
            ace/bitstream.c
            ace/huffman.c
            ace/lz77.c
            ace/sound.c
            ace/pic.c
            ace/v1.c
            ace/v2.c
            arj/arj.c
            arj/arj.h
            arj/arj_fastest.c
            arjz/arjz.c
            arjz/arjz.h
            zip/shrink.c
            zip/shrink.h
            zip/reduce.c
            zip/reduce.h
            zip/implode.c
            zip/implode.h
            zip/deflate64.c
            zip/deflate64.h
            zip/zip.c
            zip/zip.h
            ppmd/RangeCoder.c
            ppmd/RangeCoder.h
            ppmd/Context.c
            ppmd/Context.h
            ppmd/SubAllocator.h
            ppmd/SubAllocatorVariantI.c
            ppmd/SubAllocatorVariantI.h
            ppmd/VariantI.c
            ppmd/VariantI.h
            wavpack/common_utils.c
            wavpack/decorr_utils.c
            wavpack/entropy_utils.c
            wavpack/open_legacy.c
            wavpack/open_utils.c
            wavpack/read_words.c
            wavpack/tags.c
            wavpack/unpack.c
            wavpack/unpack_floats.c
            wavpack/unpack_seek.c
            wavpack/unpack_utils.c
            wavpack/wavpack.h
            wavpack/wavpack_local.h
            wavpack/wavpack_version.h
            winzipjpeg/ArithmeticDecoder.c
            winzipjpeg/ArithmeticDecoder.h
            winzipjpeg/Decompressor.c
            winzipjpeg/Decompressor.h
            winzipjpeg/JPEG.c
            winzipjpeg/JPEG.h
            winzipjpeg/InputStream.h
            winzipjpeg/LZMA.h
            rar/bitstream.c
            rar/bitstream.h
            rar/huffman.c
            rar/huffman.h
            rar/lzss.c
            rar/lzss.h
            rar/vm.c
            rar/vm.h
            rar/filters.c
            rar/filters.h
            rar/rar.h
            rar/rar15.c
            rar/rar20.c
            rar/rar30.c
            rar/rar50.c
            ppmd/SubAllocatorVariantH.c
            ppmd/SubAllocatorVariantH.h
            ppmd/VariantH.c
            ppmd/VariantH.h
            ppmd/VariantG.c
            ppmd/VariantG.h
            ppmd/SubAllocatorVariantG.c
            ppmd/SubAllocatorVariantG.h
            ppmd/SubAllocatorBrimstone.c
            ppmd/SubAllocatorBrimstone.h
            cpt/cpt.c
            cpt/cpt.h
            dd/dd.c
            dd/dd.h
            stuffit/stuffit.h
            stuffit/stuffit_internal.h
            stuffit/bwt.c
            stuffit/bwt.h
            stuffit/rangecoder.c
            stuffit/rangecoder.h
            stuffit/rle90.c
            stuffit/compress.c
            stuffit/huffman.c
            stuffit/lzah.c
            stuffit/mw.c
            stuffit/method13.c
            stuffit/method13_tables.h
            stuffit/method14.c
            stuffit/shrinkwrap.c
            stuffit/arsenic.c
            stuffit/brimstone.c
            stuffit/cyanide.c
            stuffit/darkhorse.c
            stuffit/deflate_sitx.c
            stuffit/iron.c
            stuffit/blend.c
            stuffit/x86.c
            stuffit/english.c
            stuffit/english_dict.c)

include(3rdparty/bzip2.cmake)
include(3rdparty/flac.cmake)
include(3rdparty/lz4.cmake)
include(3rdparty/lzfse.cmake)
include(3rdparty/lzip.cmake)
include(3rdparty/lzma.cmake)

macro(target_link_libraries_whole_archive target)
  if("${CMAKE_C_COMPILER_ID}" MATCHES "MSVC")
    foreach(arg IN LISTS ARGN)
      set_target_properties(
              ${target} PROPERTIES LINK_FLAGS "/WHOLEARCHIVE:${lib}"
      )
    endforeach()
  else()
    if("${CMAKE_C_COMPILER_ID}" MATCHES "AppleClang")
      set(LINK_FLAGS "-Wl,-all_load")
      set(UNDO_FLAGS "-Wl")
    else()
      set(LINK_FLAGS "-Wl,--whole-archive")
      set(UNDO_FLAGS "-Wl,--no-whole-archive")
    endif()
    target_link_libraries(${target} ${LINK_FLAGS} ${ARGN} ${UNDO_FLAGS})
  endif()
endmacro()

target_link_libraries_whole_archive("Aaru.Compression.Native" libzstd_static lz4_static lzo_static_lib m)

# Add LZO include directories for library.c to resolve <lzo/...> includes
target_include_directories("Aaru.Compression.Native" PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/lzo-2.10/include)

check_include_file("semaphore.h" HAVE_SEMAPHORE_H)

if(HAVE_SEMAPHORE_H)
  set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
  set(THREADS_PREFER_PTHREAD_FLAG TRUE)
  find_package(Threads)
  if(CMAKE_USE_PTHREADS_INIT)
    set(HAVE_PTHREAD 1)
  endif()
endif()

if(HAVE_PTHREAD)
  target_link_libraries_whole_archive("Aaru.Compression.Native" Threads::Threads)
endif()

add_subdirectory(tests)
