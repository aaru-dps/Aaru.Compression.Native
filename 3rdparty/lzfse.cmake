cmake_minimum_required(VERSION 2.8.12)

project(lzfse C)
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

# Compiler flags
function(lzfse_add_compiler_flags target)
    set (flags ${ARGV})
    list (REMOVE_AT flags 0)

    foreach (FLAG ${flags})
        if(CMAKE_C_COMPILER_ID STREQUAL GNU)
            # Because https://gcc.gnu.org/wiki/FAQ#wnowarning
            string(REGEX REPLACE "\\-Wno\\-(.+)" "-W\\1" flag_to_test "${FLAG}")
        else()
            set (flag_to_test ${FLAG})
        endif()

        string(REGEX REPLACE "[^a-zA-Z0-9]+" "_" test_name "CFLAG_${flag_to_test}")

        check_c_compiler_flag("${flag_to_test}" "${test_name}")
        if(${${test_name}})
            set_property(TARGET "${target}" APPEND_STRING PROPERTY COMPILE_FLAGS " ${FLAG}")
        endif()
    endforeach()
endfunction()

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

list(TRANSFORM LZFSE_SOURCES PREPEND "lzfse/")

add_library(lzfse STATIC ${LZFSE_SOURCES})
lzfse_add_compiler_flags(lzfse -Wall -Wno-unknown-pragmas -Wno-unused-variable)


if(CMAKE_VERSION VERSION_LESS 3.1 OR CMAKE_C_COMPLIER_ID STREQUAL "Intel")
    lzfse_add_compiler_flags(lzfse -std=c99)
else()
    set_property(TARGET lzfse PROPERTY C_STANDARD 99)
endif()

set_target_properties(lzfse PROPERTIES
                      C_VISIBILITY_PRESET hidden)

if(NOT AARU_MUSL)
   set_property(TARGET lzfse APPEND PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

if(NOT "${CMAKE_C_PLATFORM_ID}" MATCHES "MinGW" OR (NOT ${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm" AND NOT ${CMAKE_SYSTEM_PROCESSOR} MATCHES "aarch64"))
    set_property(TARGET lzfse APPEND PROPERTY POSITION_INDEPENDENT_CODE TRUE)
endif()