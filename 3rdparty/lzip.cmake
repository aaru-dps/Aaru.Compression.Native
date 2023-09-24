set("LZIP_DIRECTORY" "3rdparty/lzlib")

message(STATUS "LZIP VERSION: 1.13")

target_sources("Aaru.Compression.Native" PRIVATE ${LZIP_DIRECTORY}/lzlib.c)
