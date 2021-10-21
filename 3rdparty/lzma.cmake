project(lzma C ASM)

set("LZMA_C_DIRECTORY" "lzma-21.03beta/C")
set("LZMA_ASM_DIRECTORY" "lzma-21.03beta/Asm")

add_library(lzma STATIC)

set_property(TARGET lzma PROPERTY POSITION_INDEPENDENT_CODE ON)

target_compile_definitions(lzma PUBLIC _REENTRANT)
target_compile_definitions(lzma PUBLIC _FILE_OFFSET_BITS)
target_compile_definitions(lzma PUBLIC _LARGEFILE_SOURCE)
target_compile_definitions(lzma PUBLIC _7ZIP_ST)

# All assembly for x86 and x64 disabled because it uses a custom, non GAS, non MASM, assembler

if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64" OR ${CMAKE_SYSTEM_PROCESSOR} MATCHES "AMD64")
    set(IS_X64 1)
    target_compile_definitions(lzma PUBLIC IS_X64)

#    if(NOT "${CMAKE_C_COMPILER_ID}" MATCHES "AppleClang")
#        set(USE_ASM 1)
#    endif()
elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "i686")
    set(IS_X86 1)
    target_compile_definitions(lzma PUBLIC IS_X86)
#    set(USE_ASM 1)
elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "aarch64")
    set(IS_ARM64 1)
    target_compile_definitions(lzma PUBLIC IS_ARM64)
    set(USE_ASM 1)
endif()

if("${CMAKE_C_COMPILER_ID}" MATCHES "AppleClang" OR "${CMAKE_C_COMPILER_ID}" MATCHES "Clang" )
    set(USE_CLANG 1)
    target_compile_definitions(lzma PUBLIC USE_CLANG)
endif()

target_compile_options(lzma PUBLIC -Wall)
#target_compile_options(lzma PUBLIC -Werror)

target_compile_definitions(lzma PUBLIC $<$<COMPILE_LANGUAGE:ASM>:-DABI_LINUX>)


if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64" OR ${CMAKE_SYSTEM_PROCESSOR} MATCHES "AMD64")
    target_compile_options(lzma PUBLIC $<$<COMPILE_LANGUAGE:ASM>:-elf64>)
else()
    target_compile_options(lzma PUBLIC $<$<COMPILE_LANGUAGE:ASM>:-elf>)
    target_compile_definitions(lzma PUBLIC $<$<COMPILE_LANGUAGE:ASM>:-DABI_CDECL>)
endif()

target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/7zAlloc.c)
target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/7zArcIn.c)
target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/7zBuf.c)
target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/7zBuf2.c)
target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/7zCrc.c)
target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/7zDec.c)
target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/7zFile.c)
target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/7zStream.c)
target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/Aes.c)
target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/Alloc.c)
target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/Bcj2.c)
target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/Bcj2Enc.c)
#target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/Blake2s.c)
target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/Bra.c)
target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/Bra86.c)
target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/BraIA64.c)
#target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/BwtSort.c)
target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/CpuArch.c)
target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/Delta.c)
target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/DllSecur.c)
#target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/HuffEnc.c)
target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/LzFind.c)
## ifdef MT_FILES
#target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/LzFindMt.c)
#
#target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/Threads.c)
## endif
#
target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/LzmaEnc.c)
target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/Lzma86Dec.c)
target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/Lzma86Enc.c)
target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/Lzma2Dec.c)
target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/Lzma2DecMt.c)
target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/Lzma2Enc.c)
target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/LzmaLib.c)
target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/MtCoder.c)
target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/MtDec.c)
target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/Ppmd7.c)
#target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/Ppmd7aDec.c)
target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/Ppmd7Dec.c)
target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/Ppmd7Enc.c)
#target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/Ppmd8.c)
#target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/Ppmd8Dec.c)
#target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/Ppmd8Enc.c)
#target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/Sha1.c)
target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/Sha256.c)
target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/Sort.c)
target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/Xz.c)
target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/XzCrc64.c)


#target_sources(lzma PRIVATE ${LZMA_ASM_DIRECTORY}/x86/7zCrcOpt.asm)

if(USE_ASM)
    if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64" OR ${CMAKE_SYSTEM_PROCESSOR} MATCHES "i686" OR ${CMAKE_SYSTEM_PROCESSOR} MATCHES "AMD64")
        set(USE_X86_ASM 1)
    endif()
endif()

if(USE_X86_ASM)
    target_sources(lzma PRIVATE ${LZMA_ASM_DIRECTORY}/x86/7zCrcOpt.asm)
    target_sources(lzma PRIVATE ${LZMA_ASM_DIRECTORY}/x86/XzCrc64Opt.asm)
    target_sources(lzma PRIVATE ${LZMA_ASM_DIRECTORY}/x86/AesOpt.asm)
#    target_sources(lzma PRIVATE ${LZMA_ASM_DIRECTORY}/x86/Sha1Opt.asm)
    target_sources(lzma PRIVATE ${LZMA_ASM_DIRECTORY}/x86/Sha256Opt.asm)
else()
    target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/7zCrcOpt.c)
    target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/XzCrc64Opt.c)
#    target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/Sha1Opt.c)
    target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/Sha256Opt.c)
    target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/AesOpt.c)
endif()

if(USE_LZMA_DEC_ASM)
    if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64" OR ${CMAKE_SYSTEM_PROCESSOR} MATCHES "AMD64")
        target_sources(lzma PRIVATE ${LZMA_ASM_DIRECTORY}/x86/LzmaDecOpt.asm)
    endif()

    if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "aarch64")
        target_sources(lzma PRIVATE ${LZMA_ASM_DIRECTORY}/arm64/LzmaDecOpt.S)
        target_sources(lzma PRIVATE ${LZMA_ASM_DIRECTORY}/arm64/7zAsm.S)
    endif()

    target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/LzmaDec.c)
else()
    target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/LzmaDec.c)
endif()

target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/XzDec.c)
target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/XzEnc.c)
target_sources(lzma PRIVATE ${LZMA_C_DIRECTORY}/XzIn.c)