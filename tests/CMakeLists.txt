if("${CMAKE_C_PLATFORM_ID}" MATCHES "MinGW" OR "${AARU_BUILD_PACKAGE}" MATCHES 1)
    return()
endif()

# 'Google_test' is the subproject name
project(tests)

# 'lib' is the folder with Google Test sources
add_subdirectory(lib)
include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/apple_rle.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/adc.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/bzip2.bz2
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

# 'Google_Tests_run' is the target name
# 'test1.cpp tests2.cpp' are source files with tests
add_executable(tests_run apple_rle.cpp crc32.c crc32.h adc.cpp bzip2.cpp)
target_link_libraries(tests_run gtest gtest_main "Aaru.Compression.Native")
