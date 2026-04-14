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

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/lzip.lz
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/lzfse.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/zstd.zst
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/lz4.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/lzma.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/flac.flac
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/audio.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/data.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/alice29.lzd
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/alice29.lh5
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/arcpack.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/arcsqueeze.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/arccrunchnr.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/arccrunch_dynamic.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/arcsquash.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/pak_crush.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/pak_distill.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/ha_asc.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/ha_hsc.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/xz.xz
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/larc_lz5.lzs
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/lha_lh6.lzh
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/lharc_lh1.lzh
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/pmarc_pm2.pma
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

# 'Google_Tests_run' is the target name
# 'test1.cpp tests2.cpp' are source files with tests
add_executable(tests_run apple_rle.cpp crc32.c crc32.h adc.cpp bzip2.cpp lzip.cpp lzfse.cpp zstd.cpp lzma.cpp flac.cpp lz4.cpp
               zoo/lzd.cpp arc/pack.cpp lh5.cpp arc/squeeze.cpp arc/crunch.cpp arc/squash.cpp pak/crush.cpp
               pak/distill.cpp ha.cpp xz.cpp
               lha/lh_static.cpp lha/lh1.cpp lha/larc.cpp lha/lh_old.cpp)
target_link_libraries(tests_run gtest gtest_main "Aaru.Compression.Native")
