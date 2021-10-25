cmake_minimum_required(VERSION 3.12)

project(bzip2
        VERSION 1.0.7
        DESCRIPTION "This Bzip2/libbz2 a program and library for lossless block-sorting data compression."
        LANGUAGES C)

set(LT_CURRENT  1)
set(LT_REVISION 7)
set(LT_AGE      0)

set(CMAKE_CURRENT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bzip2/")
set(PROJECT_SOURCE_DIR "${PROJECT_SOURCE_DIR}/bzip2/")
set(PROJECT_BINARY_DIR "${PROJECT_BINARY_DIR}/bzip2/")

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})
include(Version)
include(SymLink)

set(BZ_VERSION ${PROJECT_VERSION})
configure_file (
        ${PROJECT_SOURCE_DIR}/bz_version.h.in
        ${PROJECT_BINARY_DIR}/bz_version.h
)
include_directories(${PROJECT_BINARY_DIR})

message(STATUS "BZIP2 VERSION: ${BZ_VERSION}")

math(EXPR LT_SOVERSION "${LT_CURRENT} - ${LT_AGE}")
set(LT_VERSION "${LT_SOVERSION}.${LT_AGE}.${LT_REVISION}")
set(PACKAGE_VERSION ${PROJECT_VERSION})
HexVersion(PACKAGE_VERSION_NUM ${PROJECT_VERSION_MAJOR} ${PROJECT_VERSION_MINOR} ${PROJECT_VERSION_PATCH})

# Do not disable assertions based on CMAKE_BUILD_TYPE.
foreach(_build_type Release MinSizeRel RelWithDebInfo)
    foreach(_lang C)
        string(TOUPPER CMAKE_${_lang}_FLAGS_${_build_type} _var)
        string(REGEX REPLACE "(^|)[/-]D *NDEBUG($|)" " " ${_var} "${${_var}}")
    endforeach()
endforeach()

# Support the latest c++ standard available.
include(ExtractValidFlags)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Choose the build type" FORCE)

    # Include "None" as option to disable any additional (optimization) flags,
    # relying on just CMAKE_C_FLAGS and CMAKE_CXX_FLAGS (which are empty by
    # default). These strings are presented in cmake-gui.
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
                 None Debug Release MinSizeRel RelWithDebInfo)
endif()

include(GNUInstallDirs)

# For test scripts and documentation
find_package(Python3)

# Always use '-fPIC'/'-fPIE' option, except when using MingW to compile for WoA.
if("${CMAKE_C_PLATFORM_ID}" MATCHES "MinGW" AND ("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "aarch64" OR "${CMAKE_SYSTEM_PROCESSOR}" MATCHES "arm"))
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

# Checks for header files.
include(CheckIncludeFile)
check_include_file(arpa/inet.h    HAVE_ARPA_INET_H)
check_include_file(fcntl.h        HAVE_FCNTL_H)
check_include_file(inttypes.h     HAVE_INTTYPES_H)
check_include_file(limits.h       HAVE_LIMITS_H)
check_include_file(netdb.h        HAVE_NETDB_H)
check_include_file(netinet/in.h   HAVE_NETINET_IN_H)
check_include_file(pwd.h          HAVE_PWD_H)
check_include_file(sys/socket.h   HAVE_SYS_SOCKET_H)
check_include_file(sys/time.h     HAVE_SYS_TIME_H)
check_include_file(syslog.h       HAVE_SYSLOG_H)
check_include_file(time.h         HAVE_TIME_H)
check_include_file(unistd.h       HAVE_UNISTD_H)

include(CheckTypeSize)
# Checks for typedefs, structures, and compiler characteristics.
# AC_TYPE_SIZE_T
check_type_size("ssize_t" SIZEOF_SSIZE_T)
if(NOT SIZEOF_SSIZE_T)
    # ssize_t is a signed type in POSIX storing at least -1.
    # Set it to "int" to match the behavior of AC_TYPE_SSIZE_T (autotools).
    set(ssize_t int)
endif()

include(CheckStructHasMember)
check_struct_has_member("struct tm" tm_gmtoff time.h HAVE_STRUCT_TM_TM_GMTOFF)

# Checks for library functions.
include(CheckFunctionExists)
check_function_exists(_Exit     HAVE__EXIT)
check_function_exists(accept4   HAVE_ACCEPT4)
check_function_exists(mkostemp  HAVE_MKOSTEMP)

include(CheckSymbolExists)
# XXX does this correctly detect initgroups (un)availability on cygwin?
check_symbol_exists(initgroups grp.h HAVE_DECL_INITGROUPS)
if(NOT HAVE_DECL_INITGROUPS AND HAVE_UNISTD_H)
    # FreeBSD declares initgroups() in unistd.h
    check_symbol_exists(initgroups unistd.h HAVE_DECL_INITGROUPS2)
    if(HAVE_DECL_INITGROUPS2)
        set(HAVE_DECL_INITGROUPS 1)
    endif()
endif()

# The build targets.
#   In a larger project, the following would be in subdirectories and
#   These targets would be included with `add_subdirectory()`
#
set(BZ2_SOURCES
    blocksort.c
    huffman.c
    crctable.c
    randtable.c
    compress.c
    decompress.c
    bzlib.c)

list(TRANSFORM BZ2_SOURCES PREPEND "bzip2/")

#if(ENABLE_STATIC_LIB)
    # The libbz2 static library.
    add_library(bz2_static STATIC)
    target_sources(bz2_static
                   PRIVATE     ${BZ2_SOURCES}
                   PUBLIC      ${CMAKE_CURRENT_SOURCE_DIR}/bzlib_private.h
                   INTERFACE   ${CMAKE_CURRENT_SOURCE_DIR}/bzlib.h)
    set_target_properties(bz2_static PROPERTIES
                          COMPILE_FLAGS       "${WARNCFLAGS}"
                          VERSION             ${LT_VERSION}
                          SOVERSION           ${LT_SOVERSION}
                          ARCHIVE_OUTPUT_NAME bz2_static)
    target_compile_definitions(bz2_static PUBLIC BZ2_STATICLIB)
#    install(TARGETS bz2_static DESTINATION ${CMAKE_INSTALL_LIBDIR})
#    install(FILES bzlib.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
#endif()
