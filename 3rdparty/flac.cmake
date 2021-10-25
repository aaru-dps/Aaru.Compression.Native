# 3.1 is OK for most parts. However:
# 3.3 is needed in src/libFLAC
# 3.5 is needed in src/libFLAC/ia32
# 3.9 is needed in 'doc' because of doxygen_add_docs()
cmake_minimum_required(VERSION 3.5)

if(NOT (CMAKE_BUILD_TYPE OR CMAKE_CONFIGURATION_TYPES OR DEFINED ENV{CFLAGS} OR DEFINED ENV{CXXFLAGS}))
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build, options are: None Debug Release RelWithDebInfo")
endif()

project(FLAC VERSION 1.3.3) # HOMEPAGE_URL "https://www.xiph.org/flac/")

message(STATUS "FLAC VERSION: ${PROJECT_VERSION}")
set(BUILD_SHARED_LIBS BOOL OFF)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/flac/cmake")

set(VERSION ${PROJECT_VERSION})

set(PROJECT_SOURCE_DIR "${PROJECT_SOURCE_DIR}/flac/")

find_package(Iconv)
set(HAVE_ICONV ${Iconv_FOUND})

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -funroll-loops")
endif()

include(CMakePackageConfigHelpers)
include(CPack)
include(CTest)
include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)
include(CheckSymbolExists)
include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckCSourceCompiles)
include(CheckCXXSourceCompiles)
include(UseSystemExtensions)
include(TestBigEndian)

check_include_file("byteswap.h" HAVE_BYTESWAP_H)
check_include_file("inttypes.h" HAVE_INTTYPES_H)
check_include_file("stdint.h" HAVE_STDINT_H)
if(MSVC)
    check_include_file("intrin.h" FLAC__HAS_X86INTRIN)
else()
    check_include_file("x86intrin.h" FLAC__HAS_X86INTRIN)
endif()

check_function_exists(fseeko HAVE_FSEEKO)

check_c_source_compiles("int main() { return __builtin_bswap16 (0) ; }" HAVE_BSWAP16)
check_c_source_compiles("int main() { return __builtin_bswap32 (0) ; }" HAVE_BSWAP32)

if(NOT "${CMAKE_C_PLATFORM_ID}" MATCHES "MinGW" OR (NOT ${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm" AND NOT ${CMAKE_SYSTEM_PROCESSOR} MATCHES "aarch64"))
    test_big_endian(CPU_IS_BIG_ENDIAN)
endif()

check_c_compiler_flag(-mstackrealign HAVE_STACKREALIGN_FLAG)

include_directories("flac/include")

include_directories("${CMAKE_CURRENT_BINARY_DIR}/flac")
add_definitions(-DHAVE_CONFIG_H)

if(MSVC)
    add_definitions(
            -D_CRT_SECURE_NO_WARNINGS
            -D_USE_MATH_DEFINES)
endif()

option(WITH_ASM "Use any assembly optimization routines" ON)

check_include_file("cpuid.h" HAVE_CPUID_H)
check_include_file("sys/param.h" HAVE_SYS_PARAM_H)

set(CMAKE_REQUIRED_LIBRARIES m)
check_function_exists(lround HAVE_LROUND)

include(CheckCSourceCompiles)
include(CheckCPUArch)

check_cpu_arch_x64(FLAC__CPU_X86_64)
if(NOT FLAC__CPU_X86_64)
    check_cpu_arch_x86(FLAC__CPU_IA32)
endif()

if(FLAC__CPU_X86_64 OR FLAC__CPU_IA32)
    set(FLAC__ALIGN_MALLOC_DATA 1)
    option(WITH_AVX "Enable AVX, AVX2 optimizations" ON)
endif()

include(CheckLanguage)
check_language(ASM_NASM)
if(CMAKE_ASM_NASM_COMPILER)
    enable_language(ASM_NASM)
    add_definitions(-DFLAC__HAS_NASM)
endif()

if(NOT WITH_ASM)
    add_definitions(-DFLAC__NO_ASM)
endif()

if(FLAC__CPU_IA32)
    if(WITH_ASM AND CMAKE_ASM_NASM_COMPILER)
        add_subdirectory(ia32)
    endif()

    option(WITH_SSE "Enable SSE2 optimizations" ON)
    check_c_compiler_flag(-msse2 HAVE_MSSE2_FLAG)
    if(WITH_SSE)
        add_compile_options(
                $<$<BOOL:${HAVE_MSSE2_FLAG}>:-msse2>
                $<$<BOOL:${MSVC}>:/arch:SSE2>)
    endif()
endif()

include_directories("flac/src/libFLAC/include")

add_library(FLAC STATIC
            flac/src/libFLAC/bitmath.c
            flac/src/libFLAC/bitreader.c
            flac/src/libFLAC/bitwriter.c
            flac/src/libFLAC/cpu.c
            flac/src/libFLAC/crc.c
            flac/src/libFLAC/fixed.c
            flac/src/libFLAC/fixed_intrin_sse2.c
            flac/src/libFLAC/fixed_intrin_ssse3.c
            flac/src/libFLAC/float.c
            flac/src/libFLAC/format.c
            flac/src/libFLAC/lpc.c
            flac/src/libFLAC/lpc_intrin_sse.c
            flac/src/libFLAC/lpc_intrin_sse2.c
            flac/src/libFLAC/lpc_intrin_sse41.c
            flac/src/libFLAC/lpc_intrin_avx2.c
            flac/src/libFLAC/lpc_intrin_vsx.c
            flac/src/libFLAC/md5.c
            flac/src/libFLAC/memory.c
            flac/src/libFLAC/metadata_iterators.c
            flac/src/libFLAC/metadata_object.c
            flac/src/libFLAC/stream_decoder.c
            flac/src/libFLAC/stream_encoder.c
            flac/src/libFLAC/stream_encoder_intrin_sse2.c
            flac/src/libFLAC/stream_encoder_intrin_ssse3.c
            flac/src/libFLAC/stream_encoder_intrin_avx2.c
            flac/src/libFLAC/stream_encoder_framing.c
            flac/src/libFLAC/window.c
            $<$<BOOL:${WIN32}>:flac/include/share/windows_unicode_filenames.h>
            $<$<BOOL:${WIN32}>:flac/src/libFLAC/windows_unicode_filenames.c>
            $<$<BOOL:${OGG_FOUND}>:ogg_decoder_aspect.c>
            $<$<BOOL:${OGG_FOUND}>:ogg_encoder_aspect.c>
            $<$<BOOL:${OGG_FOUND}>:ogg_helper.c>
            $<$<BOOL:${OGG_FOUND}>:ogg_mapping.c>)

target_compile_definitions(FLAC PUBLIC FLAC__NO_DLL)

# Disable fortify source when not-release or when cross-building with MingW for WoA
if(CMAKE_BUILD_TYPE STREQUAL Debug OR CMAKE_BUILD_TYPE STREQUAL RelWithDebInfo OR "${CMAKE_C_PLATFORM_ID}" MATCHES "MinGW")
    set(DODEFINE_FORTIFY_SOURCE 0)
endif()

configure_file(flac/config.cmake.h.in flac/config.h)