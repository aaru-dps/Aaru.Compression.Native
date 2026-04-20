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

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/ace_v1_lz77.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/ace_v2_blocked.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/arj_m1.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/arj_m2.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/arj_m3.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/arj_m4.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/arjz_default.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/arjz_v55_new.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/zip_shrink.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/zip_implode.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/zip_deflate64.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/zip_ppmd.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/rar15_m5.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/rar20_default.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/rar30_default.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/rar50_default.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/rar50_m3.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/rar50_m4.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/rar50_m5.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/cpt_rle.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/cpt_lzh_rle.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/dd_ad1.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/dd_ad2.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/dd_dd1.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/dd_dd2.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/dd_dd3.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/stuffit_compress.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/stuffit_method13.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/stuffit_method13_v5.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/stuffit_arsenic.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/stuffit_shrinkwrap.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/stuffitx_brimstone.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/stuffitx_cyanide.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/stuffitx_darkhorse.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/stuffitx_deflate.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/stuffitx_blend.bin
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/mf2dd_hfs_fast.dart.lz
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/mf2dd_hfs_best.dart.lz
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/mf1dd_hfs_fast.dart.lz
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/mf1dd_hfs_best.dart.lz
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/mf2dd_mfs_fast.dart.lz
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/mf2dd_mfs_best.dart.lz
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/mf1dd_mfs_fast.dart.lz
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/mf1dd_mfs_best.dart.lz
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

# 'Google_Tests_run' is the target name
# 'test1.cpp tests2.cpp' are source files with tests
add_executable(tests_run apple_rle.cpp crc32.c crc32.h adc.cpp bzip2.cpp lzip.cpp lzfse.cpp zstd.cpp lzma.cpp flac.cpp lz4.cpp
               zoo/lzd.cpp arc/pack.cpp lh5.cpp arc/squeeze.cpp arc/crunch.cpp arc/squash.cpp pak/crush.cpp
               pak/distill.cpp ha.cpp xz.cpp
               lha/lh_static.cpp lha/lh1.cpp lha/larc.cpp lha/lh_old.cpp
               ace/ace.cpp
               arj/arj.cpp
               arjz/arjz.cpp
               zip/zip.cpp
               rar/rar.cpp
               cpt/cpt.cpp
               dd/dd.cpp
               stuffit/stuffit.cpp
               dart/dart.cpp)
target_link_libraries(tests_run gtest gtest_main "Aaru.Compression.Native")
