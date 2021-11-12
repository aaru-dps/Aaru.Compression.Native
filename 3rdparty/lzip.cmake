set("LZIP_DIRECTORY" "3rdparty/lzlib-1.12")

message(STATUS "LZIP VERSION: 1.12")

target_sources("Aaru.Compression.Native" PRIVATE ${LZIP_DIRECTORY}/lzlib.c)