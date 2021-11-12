set(LT_CURRENT  1)
set(LT_REVISION 7)
set(LT_AGE      0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/bzip2/cmake")
include(Version)
include(SymLink)

set(BZ_VERSION ${LT_CURRENT}.${LT_AGE}.${LT_REVISION})
configure_file (
        ${PROJECT_SOURCE_DIR}/3rdparty/bzip2/bz_version.h.in
        ${PROJECT_BINARY_DIR}/3rdparty/bzip2/bz_version.h
)
include_directories(${PROJECT_BINARY_DIR})

message(STATUS "BZIP2 VERSION: ${BZ_VERSION}")

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

# Always use '-fPIC'/'-fPIE' option, except when using MingW to compile for WoA.
if(NOT "${CMAKE_C_PLATFORM_ID}" MATCHES "MinGW" OR (NOT "${CMAKE_SYSTEM_PROCESSOR}" MATCHES "aarch64" AND NOT "${CMAKE_SYSTEM_PROCESSOR}" MATCHES "arm"))
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
if(NOT "${CMAKE_C_PLATFORM_ID}" MATCHES "MinGW" OR (NOT ${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm" AND NOT ${CMAKE_SYSTEM_PROCESSOR} MATCHES "aarch64"))
    check_type_size("ssize_t" SIZEOF_SSIZE_T)
endif()

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

list(TRANSFORM BZ2_SOURCES PREPEND "3rdparty/bzip2/")

add_definitions(-DBZ_DEBUG=0)

target_sources("Aaru.Compression.Native"
               PRIVATE     ${BZ2_SOURCES}
               PUBLIC      ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/bzip2/bzlib_private.h
               INTERFACE   ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/bzip2/bzlib.h)
target_compile_definitions("Aaru.Compression.Native" PUBLIC BZ2_STATICLIB)

set_property(TARGET "Aaru.Compression.Native" PROPERTY C_VISIBILITY_PRESET hidden)
target_compile_definitions("Aaru.Compression.Native" PUBLIC BZ_NO_STDIO)