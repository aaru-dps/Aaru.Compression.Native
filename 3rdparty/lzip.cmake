project(lzlib C)

set("LZIP_DIRECTORY" "lzlib-1.12")

message(STATUS "LZIP VERSION: 1.12")

add_library(lzlib STATIC ${LZIP_DIRECTORY}/lzlib.c)