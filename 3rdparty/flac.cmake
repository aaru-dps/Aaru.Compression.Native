# 3.1 is OK for most parts. However:
# 3.3 is needed in src/libFLAC
# 3.5 is needed in src/libFLAC/ia32
# 3.9 is needed in 'doc' because of doxygen_add_docs()
#cmake_minimum_required(VERSION 3.5)

#if(NOT (CMAKE_BUILD_TYPE OR CMAKE_CONFIGURATION_TYPES OR DEFINED ENV{CFLAGS} OR DEFINED ENV{CXXFLAGS}))
#    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build, options are: None Debug Release RelWithDebInfo")
#endif()

set(FLAC_VERSION 1.4.3) # HOMEPAGE_URL "https://www.xiph.org/flac/")

message(STATUS "FLAC VERSION: ${FLAC_VERSION}")
set(BUILD_SHARED_LIBS BOOL OFF)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/flac/cmake")

set(VERSION ${FLAC_VERSION})

set(OLD_PROJECT_SOURCE_DIR "${PROJECT_SOURCE_DIR}")
set(PROJECT_SOURCE_DIR "${PROJECT_SOURCE_DIR}/3rdparty/flac/")

#find_package(Iconv)
#set(HAVE_ICONV ${Iconv_FOUND})

#if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
#    set(CMAKE_C_FLAGS_RELEASE "-O3 -funroll-loops ${CMAKE_C_FLAGS_RELEASE}")
#    set(CMAKE_C_FLAGS_RELEASE "-DNDEBUG ${CMAKE_C_FLAGS_RELEASE}")
#endif()
#if(MSVC)
#    set(CMAKE_C_FLAGS_RELEASE "/O2 /Ob2 /Oi /Ot /Oy ${CMAKE_C_FLAGS_RELEASE}")
#endif()

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
check_include_file("stdbool.h" HAVE_STDBOOL_H)
check_include_file("arm_neon.h" FLAC__HAS_NEONINTRIN)
check_include_file("semaphore.h" HAVE_SEMAPHORE_H)

if (NOT HAVE_STDINT_H OR NOT HAVE_STDBOOL_H)
    message(SEND_ERROR "Header stdint.h and/or stdbool.h not found")
endif ()

if (MSVC)
    check_include_file("intrin.h" FLAC__HAS_X86INTRIN)
else ()
    check_include_file("x86intrin.h" FLAC__HAS_X86INTRIN)
endif ()

#check_function_exists(fseeko HAVE_FSEEKO)

check_c_source_compiles("int main() { return __builtin_bswap16 (0) ; }" HAVE_BSWAP16)
check_c_source_compiles("int main() { return __builtin_bswap32 (0) ; }" HAVE_BSWAP32)

if (NOT "${CMAKE_C_PLATFORM_ID}" MATCHES "MinGW" OR (NOT ${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm" AND NOT ${CMAKE_SYSTEM_PROCESSOR} MATCHES "aarch64"))
    test_big_endian(CPU_IS_BIG_ENDIAN)
endif ()

check_c_compiler_flag(-mstackrealign HAVE_STACKREALIGN_FLAG)

if (CMAKE_SYSTEM_PROCESSOR STREQUAL "i686" AND HAVE_STACKREALIGN_FLAG)
    add_compile_options(-mstackrealign)
    add_compile_options($<$<OR:$<COMPILE_LANGUAGE:C>,$<COMPILE_LANGUAGE:CXX>>:-mstackrealign>)
endif ()

include_directories("3rdparty/flac/include")

include_directories("${CMAKE_CURRENT_BINARY_DIR}/3rdparty/flac")
add_definitions(-DHAVE_CONFIG_H)

if (MSVC)
    add_definitions(
            -D_CRT_SECURE_NO_WARNINGS
            -D_USE_MATH_DEFINES)
endif ()

option(WITH_ASM "Use any assembly optimization routines" ON)

check_include_file("cpuid.h" HAVE_CPUID_H)
check_include_file("sys/param.h" HAVE_SYS_PARAM_H)

set(CMAKE_REQUIRED_LIBRARIES m)
check_function_exists(lround HAVE_LROUND)

include(CheckCSourceCompiles)
include(CheckCPUArch)
include(CheckA64NEON)

check_cpu_arch_x64(FLAC__CPU_X86_64)
if (NOT FLAC__CPU_X86_64)
    check_cpu_arch_x86(FLAC__CPU_IA32)
endif ()

if (FLAC__CPU_X86_64 OR FLAC__CPU_IA32)
    set(FLAC__ALIGN_MALLOC_DATA 1)
    option(WITH_AVX "Enable AVX, AVX2 optimizations (with runtime detection, resulting binary does not require AVX2, so only necessary when a compiler doesn't know about AVX)" ON)
    if (WITH_AVX AND MSVC)
        set_source_files_properties(fixed_intrin_avx2.c lpc_intrin_avx2.c stream_encoder_intrin_avx2.c PROPERTIES COMPILE_FLAGS /arch:AVX2)
        set_source_files_properties(lpc_intrin_fma.c PROPERTIES COMPILE_FLAGS "/arch:AVX2 /fp:fast")
    endif ()
    if (WITH_AVX AND (CMAKE_C_COMPILER_ID MATCHES "Clang"))
        set_source_files_properties(lpc_intrin_fma.c PROPERTIES COMPILE_FLAGS "-ffast-math")
    endif ()
else ()
    if (FLAC__CPU_ARM64)
        check_a64neon(FLAC__HAS_A64NEONINTRIN)
    endif ()
endif ()

if (NOT WITH_ASM)
    add_definitions(-DFLAC__NO_ASM)
endif ()

if (HAVE_SEMAPHORE_H)
    set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
    set(THREADS_PREFER_PTHREAD_FLAG TRUE)
    find_package(Threads)
    if (CMAKE_USE_PTHREADS_INIT)
        set(HAVE_PTHREAD 1)
    endif ()
endif ()

include_directories("3rdparty/flac/src/libFLAC/include")

target_sources("Aaru.Compression.Native" PRIVATE
        3rdparty/flac/src/libFLAC/bitmath.c
        3rdparty/flac/src/libFLAC/bitreader.c
        3rdparty/flac/src/libFLAC/bitwriter.c
        3rdparty/flac/src/libFLAC/cpu.c
        3rdparty/flac/src/libFLAC/crc.c
        3rdparty/flac/src/libFLAC/fixed.c
        3rdparty/flac/src/libFLAC/fixed_intrin_sse2.c
        3rdparty/flac/src/libFLAC/fixed_intrin_ssse3.c
        3rdparty/flac/src/libFLAC/fixed_intrin_sse42.c
        3rdparty/flac/src/libFLAC/fixed_intrin_avx2.c
        3rdparty/flac/src/libFLAC/float.c
        3rdparty/flac/src/libFLAC/format.c
        3rdparty/flac/src/libFLAC/lpc.c
        3rdparty/flac/src/libFLAC/lpc_intrin_neon.c
        3rdparty/flac/src/libFLAC/lpc_intrin_sse2.c
        3rdparty/flac/src/libFLAC/lpc_intrin_sse41.c
        3rdparty/flac/src/libFLAC/lpc_intrin_avx2.c
        3rdparty/flac/src/libFLAC/lpc_intrin_fma.c
        3rdparty/flac/src/libFLAC/md5.c
        3rdparty/flac/src/libFLAC/memory.c
        3rdparty/flac/src/libFLAC/metadata_iterators.c
        3rdparty/flac/src/libFLAC/metadata_object.c
        3rdparty/flac/src/libFLAC/stream_decoder.c
        3rdparty/flac/src/libFLAC/stream_encoder.c
        3rdparty/flac/src/libFLAC/stream_encoder_intrin_sse2.c
        3rdparty/flac/src/libFLAC/stream_encoder_intrin_ssse3.c
        3rdparty/flac/src/libFLAC/stream_encoder_intrin_avx2.c
        3rdparty/flac/src/libFLAC/stream_encoder_framing.c
        3rdparty/flac/src/libFLAC/window.c
        $<$<BOOL:${WIN32}>:3rdparty/flac/include/share/win_utf8_io.h>
        $<$<BOOL:${WIN32}>:3rdparty/flac/src/share/win_utf8_io/win_utf8_io.c>
        $<$<BOOL:${OGG_FOUND}>:ogg_decoder_aspect.c>
        $<$<BOOL:${OGG_FOUND}>:ogg_encoder_aspect.c>
        $<$<BOOL:${OGG_FOUND}>:ogg_helper.c>
        $<$<BOOL:${OGG_FOUND}>:ogg_mapping.c>)

target_compile_definitions("Aaru.Compression.Native" PUBLIC FLAC__NO_DLL)
target_compile_definitions("Aaru.Compression.Native" PUBLIC FLAC__NO_FILEIO)

# Disable fortify source when not-release or when cross-building with MingW for WoA
if (CMAKE_BUILD_TYPE STREQUAL Debug OR CMAKE_BUILD_TYPE STREQUAL RelWithDebInfo OR "${CMAKE_C_PLATFORM_ID}" MATCHES "MinGW")
    set(DODEFINE_FORTIFY_SOURCE 0)
endif ()

set_property(TARGET "Aaru.Compression.Native" PROPERTY C_VISIBILITY_PRESET hidden)

if (ARCHITECTURE_IS_64BIT)
    set(ENABLE_64_BIT_WORDS 1)
endif ()

configure_file(3rdparty/flac/config.cmake.h.in 3rdparty/flac/config.h)

set(PROJECT_SOURCE_DIR "${OLD_PROJECT_SOURCE_DIR}")
