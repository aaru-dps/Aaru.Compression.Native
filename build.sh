#!/usr/bin/env bash

# This file is part of the Aaru Data Preservation Suite.
# Copyright (c) 2019-2021 Natalia Portillo.
#
# This library is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 2.1 of the
# License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, see <http://www.gnu.org/licenses/>.

OS_NAME=`uname`

mkdir -p docker

## Android (ARM)
# Detected system processor: armv7-a
rm -f CMakeCache.txt
mkdir -p runtimes/android-arm/native
docker run --rm dockcross/android-arm >docker/dockcross-android-arm
chmod +x docker/dockcross-android-arm
docker/dockcross-android-arm cmake -DCMAKE_BUILD_TYPE=Release -DAARU_BUILD_PACKAGE=1 .
docker/dockcross-android-arm make Aaru.Compression.Native
docker/dockcross-android-arm /usr/arm-linux-androideabi/bin/llvm-strip -s -w -K "AARU*" libAaru.Compression.Native.so
mv libAaru.Compression.Native.so runtimes/android-arm/native/

## Android (ARM64)
# Detected system processor: aarch64
rm -f CMakeCache.txt
mkdir -p runtimes/android-arm64/native
docker run --rm dockcross/android-arm64 >docker/dockcross-android-arm64
chmod +x docker/dockcross-android-arm64
docker/dockcross-android-arm64 cmake -DCMAKE_BUILD_TYPE=Release -DAARU_BUILD_PACKAGE=1 .
docker/dockcross-android-arm64 make Aaru.Compression.Native
docker/dockcross-android-arm64 /usr/aarch64-linux-android/bin/llvm-strip -s -w -K "AARU*" libAaru.Compression.Native.so
mv libAaru.Compression.Native.so runtimes/android-arm64/native/

## Android (amd64)
# Detected system processor: x86_64
rm -f CMakeCache.txt
mkdir -p runtimes/android-x64/native
docker run --rm dockcross/android-x86_64 >docker/dockcross-android-x64
chmod +x docker/dockcross-android-x64
docker/dockcross-android-x64 cmake -DCMAKE_BUILD_TYPE=Release -DAARU_BUILD_PACKAGE=1 .
docker/dockcross-android-x64 make Aaru.Compression.Native
docker/dockcross-android-x64 /usr/x86_64-linux-android/bin/llvm-strip -s -w -K "AARU*" libAaru.Compression.Native.so
mv libAaru.Compression.Native.so runtimes/android-x64/native/

## Android (x86)
# Detected system processor: i686
rm -f CMakeCache.txt
mkdir -p runtimes/android-x86/native
docker run --rm dockcross/android-x86 >docker/dockcross-android-x86
chmod +x docker/dockcross-android-x86
docker/dockcross-android-x86 cmake -DCMAKE_BUILD_TYPE=Release -DAARU_BUILD_PACKAGE=1 .
docker/dockcross-android-x86 make Aaru.Compression.Native
docker/dockcross-android-x86 /usr/i686-linux-android/bin/llvm-strip -s -w -K "AARU*" libAaru.Compression.Native.so
mv libAaru.Compression.Native.so runtimes/android-x86/native/

## Linux (ARMv7-A)
# Detected system processor: arm
rm -f CMakeCache.txt
mkdir -p runtimes/linux-arm/native
docker run --rm dockcross/linux-armv7-lts >docker/dockcross-linux-arm
chmod +x docker/dockcross-linux-arm
docker/dockcross-linux-arm cmake -DCMAKE_BUILD_TYPE=Release -DAARU_BUILD_PACKAGE=1 .
docker/dockcross-linux-arm make Aaru.Compression.Native
docker/dockcross-linux-arm armv7-unknown-linux-gnueabi-strip -s -w -K "AARU*" libAaru.Compression.Native.so
mv libAaru.Compression.Native.so runtimes/linux-arm/native/

## Linux (ARM64)
# Detected system processor: aarch64
rm -f CMakeCache.txt
mkdir -p runtimes/linux-arm64/native
docker run --rm dockcross/linux-arm64-lts >docker/dockcross-linux-arm64
chmod +x docker/dockcross-linux-arm64
docker/dockcross-linux-arm64 cmake -DCMAKE_BUILD_TYPE=Release -DAARU_BUILD_PACKAGE=1 .
docker/dockcross-linux-arm64 make Aaru.Compression.Native
docker/dockcross-linux-arm64 aarch64-unknown-linux-gnu-strip -s -w -K "AARU*" libAaru.Compression.Native.so
mv libAaru.Compression.Native.so runtimes/linux-arm64/native/

## Linux (MIPS64)
# Detected system processor: mips
rm -f CMakeCache.txt
mkdir -p runtimes/linux-mips64/native
docker run --rm dockcross/linux-mips >docker/dockcross-linux-mips64
chmod +x docker/dockcross-linux-mips64
docker/dockcross-linux-mips64 cmake -DCMAKE_BUILD_TYPE=Release -DAARU_BUILD_PACKAGE=1 .
docker/dockcross-linux-mips64 make Aaru.Compression.Native
docker/dockcross-linux-mips64 mips-unknown-linux-gnu-strip -s -w -K "AARU*" libAaru.Compression.Native.so
mv libAaru.Compression.Native.so runtimes/linux-mips64/native/

## Linux (ARM), musl
# Detected system processor: arm
rm -f CMakeCache.txt
mkdir -p runtimes/linux-musl-arm/native
docker run --rm dockcross/linux-armv7l-musl >docker/dockcross-linux-musl-arm
chmod +x docker/dockcross-linux-musl-arm
docker/dockcross-linux-musl-arm cmake -DCMAKE_BUILD_TYPE=Release -DAARU_BUILD_PACKAGE=1 -DAARU_MUSL=1 .
docker/dockcross-linux-musl-arm make Aaru.Compression.Native
docker/dockcross-linux-musl-arm armv7l-linux-musleabihf-strip -s -w -K "AARU*" libAaru.Compression.Native.so
mv libAaru.Compression.Native.so runtimes/linux-musl-arm/native/

## Linux (ARM64), musl
# Detected system processor: aarch64
rm -f CMakeCache.txt
mkdir -p runtimes/linux-musl-arm64/native
docker run --rm dockcross/linux-arm64-musl >docker/dockcross-linux-musl-arm64
chmod +x docker/dockcross-linux-musl-arm64
docker/dockcross-linux-musl-arm64 cmake -DCMAKE_BUILD_TYPE=Release -DAARU_BUILD_PACKAGE=1 -DAARU_MUSL=1 .
docker/dockcross-linux-musl-arm64 make Aaru.Compression.Native
docker/dockcross-linux-musl-arm64 aarch64-linux-musl-strip -s -w -K "AARU*" libAaru.Compression.Native.so
mv libAaru.Compression.Native.so runtimes/linux-musl-arm64/native/

## Linux (s390x)
# Detected system processor: s390x
rm -f CMakeCache.txt
mkdir -p runtimes/linux-s390x/native
docker run --rm dockcross/linux-s390x >docker/dockcross-linux-s390x
chmod +x docker/dockcross-linux-s390x
docker/dockcross-linux-s390x cmake -DCMAKE_BUILD_TYPE=Release -DAARU_BUILD_PACKAGE=1 .
docker/dockcross-linux-s390x make Aaru.Compression.Native
docker/dockcross-linux-s390x s390x-ibm-linux-gnu-strip -s -w -K "AARU*" libAaru.Compression.Native.so
mv libAaru.Compression.Native.so runtimes/linux-s390x/native/

## Linux (amd64)
# Detected system processor: x86_64
rm -f CMakeCache.txt
mkdir -p runtimes/linux-x64/native
docker run --rm dockcross/linux-x64 >docker/dockcross-linux-x64
chmod +x docker/dockcross-linux-x64
docker/dockcross-linux-x64 cmake -DCMAKE_BUILD_TYPE=Release -DAARU_BUILD_PACKAGE=1 .
docker/dockcross-linux-x64 make Aaru.Compression.Native
docker/dockcross-linux-x64 x86_64-linux-gnu-strip -s -w -K "AARU*" libAaru.Compression.Native.so
mv libAaru.Compression.Native.so runtimes/linux-x64/native/

## Linux (x86)
# Detected system processor: i686
rm -f CMakeCache.txt
mkdir -p runtimes/linux-x86/native
docker run --rm dockcross/linux-x86 > docker/dockcross-linux-x86
chmod +x docker/dockcross-linux-x86
docker/dockcross-linux-x86 cmake -DCMAKE_BUILD_TYPE=Release -DAARU_BUILD_PACKAGE=1 .
sed -e 's/CMAKE_C_COMPILER_AR\-NOTFOUND/\/usr\/i686-linux-gnu\/bin\/i686-linux-gnu-ar/g' ./3rdparty/CMakeFiles/lzfse.dir/link.txt > link.txt
mv link.txt ./3rdparty/CMakeFiles/lzfse.dir/link.txt
sed -e 's/CMAKE_C_COMPILER_RANLIB\-NOTFOUND/ranlib/g' ./3rdparty/CMakeFiles/lzfse.dir/link.txt > link.txt
mv link.txt ./3rdparty/CMakeFiles/lzfse.dir/link.txt
docker/dockcross-linux-x86 make Aaru.Compression.Native
docker/dockcross-linux-x86 /usr/bin/x86_64-linux-gnu-strip -s -w -K "AARU*" libAaru.Compression.Native.so
mv libAaru.Compression.Native.so runtimes/linux-x86/native/

## Windows (ARM)
# Detected system processor: arm
rm -f CMakeCache.txt
mkdir -p runtimes/win-arm/native
docker run --rm dockcross/windows-armv7 > docker/dockcross-win-arm
chmod +x docker/dockcross-win-arm
docker/dockcross-win-arm cmake -DCMAKE_BUILD_TYPE=Release -DAARU_BUILD_PACKAGE=1 .
sed -e 's/CMAKE_C_COMPILER_AR\-NOTFOUND/\/usr\/xcc\/armv7-w64-mingw32-cross\/bin\/armv7-w64-mingw32-ar/g' ./3rdparty/CMakeFiles/lzfse.dir/link.txt > link.txt
mv link.txt ./3rdparty/CMakeFiles/lzfse.dir/link.txt
sed -e 's/CMAKE_C_COMPILER_RANLIB\-NOTFOUND/\/usr\/xcc\/armv7-w64-mingw32-cross\/bin\/armv7-w64-mingw32-ranlib/g' ./3rdparty/CMakeFiles/lzfse.dir/link.txt > link.txt
mv link.txt ./3rdparty/CMakeFiles/lzfse.dir/link.txt
sed -e 's/\-soname,libAaru\.Compression\.Native\.so//g' ./CMakeFiles/Aaru.Compression.Native.dir/link.txt > link.txt
mv link.txt ./CMakeFiles/Aaru.Compression.Native.dir/link.txt
docker/dockcross-win-arm make Aaru.Compression.Native
mv libAaru.Compression.Native.so runtimes/win-arm/native/libAaru.Compression.Native.dll

## Windows (ARM64)
# Detected system processor: aarch64
rm -f CMakeCache.txt
mkdir -p runtimes/win-arm64/native
docker run --rm dockcross/windows-arm64 > docker/dockcross-win-arm64
chmod +x docker/dockcross-win-arm64
docker/dockcross-win-arm64 cmake -DCMAKE_BUILD_TYPE=Release -DAARU_BUILD_PACKAGE=1 .
sed -e 's/CMAKE_C_COMPILER_AR\-NOTFOUND/\/usr\/xcc\/aarch64-w64-mingw32-cross\/bin\/aarch64-w64-mingw32-ar/g' ./3rdparty/CMakeFiles/lzfse.dir/link.txt > link.txt
mv link.txt ./3rdparty/CMakeFiles/lzfse.dir/link.txt
sed -e 's/CMAKE_C_COMPILER_RANLIB\-NOTFOUND/\/usr\/xcc\/aarch64-w64-mingw32-cross\/bin\/aarch64-w64-mingw32-ranlib/g' ./3rdparty/CMakeFiles/lzfse.dir/link.txt > link.txt
mv link.txt ./3rdparty/CMakeFiles/lzfse.dir/link.txt
sed -e 's/\-soname,libAaru\.Compression\.Native\.so//g' ./CMakeFiles/Aaru.Compression.Native.dir/link.txt > link.txt
mv link.txt ./CMakeFiles/Aaru.Compression.Native.dir/link.txt
docker/dockcross-win-arm64 make Aaru.Compression.Native
mv libAaru.Compression.Native.so runtimes/win-arm64/native/libAaru.Compression.Native.dll

## Windows (AMD64)
# Detected system processor: x86_64
# TODO: Requires MSVCRT.DLL
rm -f CMakeCache.txt
mkdir -p runtimes/win-x64/native
docker run --rm dockcross/windows-shared-x64 >docker/dockcross-win-x64
chmod +x docker/dockcross-win-x64
docker/dockcross-win-x64 cmake -DCMAKE_BUILD_TYPE=Release -DAARU_BUILD_PACKAGE=1 .
docker/dockcross-win-x64 make Aaru.Compression.Native
mv libAaru.Compression.Native.dll runtimes/win-x64/native/

## Windows (x86)
# Detected system processor: i686
# TODO: Requires MSVCRT.DLL
rm -f CMakeCache.txt
mkdir -p runtimes/win-x86/native
docker run --rm dockcross/windows-shared-x86 > docker/dockcross-win-x86
chmod +x docker/dockcross-win-x86
docker/dockcross-win-x86 cmake -DCMAKE_BUILD_TYPE=Release -DAARU_BUILD_PACKAGE=1 .
docker/dockcross-win-x86 make Aaru.Compression.Native
mv libAaru.Compression.Native.dll runtimes/win-x86/native/

## Mac OS X (arm64 and x64)
if [[ ${OS_NAME} == Darwin ]]; then
  rm -f CMakeCache.txt
  cmake -DCMAKE_BUILD_TYPE=Release -DAARU_BUILD_PACKAGE=1 . .
  make Aaru.Compression.Native
  mkdir -p runtimes/osx-arm64/native
  mkdir -p runtimes/osx-x64/native
  lipo libAaru.Compression.Native.dylib -thin arm64 -output runtimes/osx-arm64/native/libAaru.Compression.Native.dylib
  lipo libAaru.Compression.Native.dylib -thin x86_64 -output runtimes/osx-x64/native/libAaru.Compression.Native.dylib
fi

# TODO: "linux-musl-x64"
# TODO: "linux-musl-x86"

nuget pack
