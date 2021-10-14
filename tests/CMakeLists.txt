if("${CMAKE_C_PLATFORM_ID}" MATCHES "MinGW" OR "${AARU_BUILD_PACKAGE}" MATCHES 1)
    return()
endif()

# 'Google_test' is the subproject name
project(tests)

# 'lib' is the folder with Google Test sources
add_subdirectory(lib)
include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})

# 'Google_Tests_run' is the target name
# 'test1.cpp tests2.cpp' are source files with tests
add_executable(tests_run "")
target_link_libraries(tests_run gtest gtest_main "Aaru.Compression.Native")
