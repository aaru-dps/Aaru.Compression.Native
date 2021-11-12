message(STATUS "LZFSE VERSION: Unknown")

include(CheckCCompilerFlag)

# If LZFSE is being bundled in another project, we don't want to
# install anything.  However, we want to let people override this, so
# we'll use the LZFSE_BUNDLE_MODE variable to let them do that; just
# set it to OFF in your project before you add_subdirectory(lzfse).
get_directory_property(LZFSE_PARENT_DIRECTORY PARENT_DIRECTORY)
if("${LZFSE_BUNDLE_MODE}" STREQUAL "")
    # Bundled mode hasn't been set one way or the other, set the default
    # depending on whether or not we are the top-level project.
    if(LZFSE_PARENT_DIRECTORY)
        set(LZFSE_BUNDLE_MODE ON)
    else()
        set(LZFSE_BUNDLE_MODE OFF)
    endif(LZFSE_PARENT_DIRECTORY)
endif()
mark_as_advanced(LZFSE_BUNDLE_MODE)

if (CMAKE_VERSION VERSION_GREATER 3.2)
    cmake_policy (SET CMP0063 NEW)
endif ()

if (CMAKE_VERSION VERSION_GREATER 3.9)
    cmake_policy (SET CMP0069 NEW)
endif ()

if (ENABLE_SANITIZER)
    set(CMAKE_C_FLAGS " ${CMAKE_C_FLAGS} -fsanitize=${ENABLE_SANITIZER}")
    set(CMAKE_CXX_FLAGS " ${CMAKE_CXX_FLAGS} -fsanitize=${ENABLE_SANITIZER}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=${ENABLE_SANITIZER}")
endif ()

set(LZFSE_SOURCES
    src/lzfse_decode.c
    src/lzfse_decode_base.c
    src/lzfse_encode.c
    src/lzfse_encode_base.c
    src/lzfse_fse.c
    src/lzvn_decode_base.c
    src/lzvn_encode_base.c)

list(TRANSFORM LZFSE_SOURCES PREPEND "3rdparty/lzfse/")

target_sources("Aaru.Compression.Native" PRIVATE ${LZFSE_SOURCES})


if(NOT AARU_MUSL AND (NOT ${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm"))
   set_property(TARGET "Aaru.Compression.Native" PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()