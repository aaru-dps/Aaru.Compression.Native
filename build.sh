#!/usr/bin/env bash

# This file is part of the Aaru Data Preservation Suite.
# Copyright (c) 2019-2026 Natalia Portillo.
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

OS_NAME=$(uname)
export OCI_EXE=docker

mkdir -p docker

## Android (ARM)
# Detected system processor: armv7-a
rm -rf build/android-arm
mkdir -p build/android-arm
mkdir -p runtimes/android-arm/native
docker run --rm dockcross/android-arm > docker/dockcross-android-arm
chmod +x docker/dockcross-android-arm
docker/dockcross-android-arm bash -c "cd build/android-arm && cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release -DAARU_BUILD_PACKAGE=1 ../.."
docker/dockcross-android-arm bash -c "cd build/android-arm && make Aaru.Compression.Native"
docker/dockcross-android-arm /usr/arm-linux-androideabi/bin/llvm-strip -s -w -K "AARU*" build/android-arm/libAaru.Compression.Native.so
mv build/android-arm/libAaru.Compression.Native.so runtimes/android-arm/native/

## Android (ARM64)
# Detected system processor: aarch64
rm -rf build/android-arm64
mkdir -p build/android-arm64
mkdir -p runtimes/android-arm64/native
docker run --rm dockcross/android-arm64 > docker/dockcross-android-arm64
chmod +x docker/dockcross-android-arm64
docker/dockcross-android-arm64 bash -c "cd build/android-arm64 && cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release -DAARU_BUILD_PACKAGE=1 ../.."
sed -e 's/\-fuse-ld=gold//g' ./build/android-arm64/CMakeFiles/Aaru.Compression.Native.dir/link.txt > link.txt
mv link.txt ./build/android-arm64/CMakeFiles/Aaru.Compression.Native.dir/link.txt
docker/dockcross-android-arm64 bash -c "cd build/android-arm64 && make Aaru.Compression.Native"
docker/dockcross-android-arm64 /usr/aarch64-linux-android/bin/llvm-strip -s -w -K "AARU*" build/android-arm64/libAaru.Compression.Native.so
mv build/android-arm64/libAaru.Compression.Native.so runtimes/android-arm64/native/

## Android (amd64)
# Detected system processor: x86_64
rm -rf build/android-x64
mkdir -p build/android-x64
mkdir -p runtimes/android-x64/native
docker run --rm dockcross/android-x86_64 > docker/dockcross-android-x64
chmod +x docker/dockcross-android-x64
docker/dockcross-android-x64 bash -c "cd build/android-x64 && cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release -DAARU_BUILD_PACKAGE=1 ../.."
sed -e 's/\-fuse-ld=gold//g' ./build/android-x64/CMakeFiles/Aaru.Compression.Native.dir/link.txt > link.txt
mv link.txt ./build/android-x64/CMakeFiles/Aaru.Compression.Native.dir/link.txt
docker/dockcross-android-x64 bash -c "cd build/android-x64 && make Aaru.Compression.Native"
docker/dockcross-android-x64 /usr/x86_64-linux-android/bin/llvm-strip -s -w -K "AARU*" build/android-x64/libAaru.Compression.Native.so
mv build/android-x64/libAaru.Compression.Native.so runtimes/android-x64/native/

## Android (x86)
# Detected system processor: i686
rm -rf build/android-x86
mkdir -p build/android-x86
mkdir -p runtimes/android-x86/native
docker run --rm dockcross/android-x86 > docker/dockcross-android-x86
chmod +x docker/dockcross-android-x86
docker/dockcross-android-x86 bash -c "cd build/android-x86 && cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release -DAARU_BUILD_PACKAGE=1 ../.."
sed -e 's/\-fuse-ld=gold//g' ./build/android-x86/CMakeFiles/Aaru.Compression.Native.dir/link.txt > link.txt
mv link.txt ./build/android-x86/CMakeFiles/Aaru.Compression.Native.dir/link.txt
docker/dockcross-android-x86 bash -c "cd build/android-x86 && make Aaru.Compression.Native"
docker/dockcross-android-x86 /usr/i686-linux-android/bin/llvm-strip -s -w -K "AARU*" build/android-x86/libAaru.Compression.Native.so
mv build/android-x86/libAaru.Compression.Native.so runtimes/android-x86/native/

## Linux (ARMv7-A)
# Detected system processor: arm
rm -rf build/linux-arm
mkdir -p build/linux-arm
mkdir -p runtimes/linux-arm/native
docker run --rm dockcross/linux-armv7a-lts > docker/dockcross-linux-arm
chmod +x docker/dockcross-linux-arm
docker/dockcross-linux-arm bash -c "cd build/linux-arm && cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release -DAARU_BUILD_PACKAGE=1 ../.."
docker/dockcross-linux-arm bash -c "cd build/linux-arm && make Aaru.Compression.Native"
docker/dockcross-linux-arm arm-cortexa8_neon-linux-gnueabihf-strip -s -w -K "AARU*" build/linux-arm/libAaru.Compression.Native.so
mv build/linux-arm/libAaru.Compression.Native.so runtimes/linux-arm/native/

## Linux (ARM64)
# Detected system processor: aarch64
rm -rf build/linux-arm64
mkdir -p build/linux-arm64
mkdir -p runtimes/linux-arm64/native
docker run --rm dockcross/linux-arm64-lts > docker/dockcross-linux-arm64
chmod +x docker/dockcross-linux-arm64
docker/dockcross-linux-arm64 bash -c "cd build/linux-arm64 && cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release -DAARU_BUILD_PACKAGE=1 ../.."
docker/dockcross-linux-arm64 bash -c "cd build/linux-arm64 && make Aaru.Compression.Native"
docker/dockcross-linux-arm64 aarch64-unknown-linux-gnu-strip -s -w -K "AARU*" build/linux-arm64/libAaru.Compression.Native.so
mv build/linux-arm64/libAaru.Compression.Native.so runtimes/linux-arm64/native/

## Linux (MIPS64)
# Detected system processor: mips
rm -rf build/linux-mips64
mkdir -p build/linux-mips64
mkdir -p runtimes/linux-mips64/native
docker run --rm dockcross/linux-mips > docker/dockcross-linux-mips64
chmod +x docker/dockcross-linux-mips64
docker/dockcross-linux-mips64 bash -c "cd build/linux-mips64 && cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release -DAARU_BUILD_PACKAGE=1 ../.."
docker/dockcross-linux-mips64 bash -c "cd build/linux-mips64 && make Aaru.Compression.Native"
docker/dockcross-linux-mips64 mips-unknown-linux-gnu-strip -s -w -K "AARU*" build/linux-mips64/libAaru.Compression.Native.so
mv build/linux-mips64/libAaru.Compression.Native.so runtimes/linux-mips64/native/

## Linux (PPC64LE)
# Detected system processor: ppc64le
rm -rf build/linux-ppc64le
mkdir -p build/linux-ppc64le
mkdir -p runtimes/linux-ppc64le/native
docker run --rm dockcross/linux-ppc64le > docker/dockcross-linux-ppc64le
chmod +x docker/dockcross-linux-ppc64le
docker/dockcross-linux-ppc64le bash -c "cd build/linux-ppc64le && cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release -DAARU_BUILD_PACKAGE=1 ../.."
docker/dockcross-linux-ppc64le bash -c "cd build/linux-ppc64le && make Aaru.Compression.Native"
docker/dockcross-linux-ppc64le powerpc64le-unknown-linux-gnu-strip -s -w -K "AARU*" build/linux-ppc64le/libAaru.Compression.Native.so
mv build/linux-ppc64le/libAaru.Compression.Native.so runtimes/linux-ppc64le/native/

## Linux (ARM), musl
# Detected system processor: arm
rm -rf build/linux-musl-arm
mkdir -p build/linux-musl-arm
mkdir -p runtimes/linux-musl-arm/native
docker run --rm dockcross/linux-armv7l-musl >docker/dockcross-linux-musl-arm
chmod +x docker/dockcross-linux-musl-arm
docker/dockcross-linux-musl-arm bash -c "cd build/linux-musl-arm && cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release -DAARU_BUILD_PACKAGE=1 -DAARU_MUSL=1 ../.."
docker/dockcross-linux-musl-arm bash -c "cd build/linux-musl-arm && make"
docker/dockcross-linux-musl-arm armv7l-linux-musleabihf-strip -s -w -K "AARU*" build/linux-musl-arm/libAaru.Compression.Native.so
mv build/linux-musl-arm/libAaru.Compression.Native.so runtimes/linux-musl-arm/native/

## Linux (ARM64), musl
# Detected system processor: aarch64
rm -rf build/linux-musl-arm64
mkdir -p build/linux-musl-arm64
mkdir -p runtimes/linux-musl-arm64/native
docker run --rm dockcross/linux-arm64-musl >docker/dockcross-linux-musl-arm64
chmod +x docker/dockcross-linux-musl-arm64
docker/dockcross-linux-musl-arm64 bash -c "cd build/linux-musl-arm64 && cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release -DAARU_BUILD_PACKAGE=1 -DAARU_MUSL=1 ../.."
docker/dockcross-linux-musl-arm64 bash -c "cd build/linux-musl-arm64 && make Aaru.Compression.Native"
docker/dockcross-linux-musl-arm64 aarch64-linux-musl-strip -s -w -K "AARU*" build/linux-musl-arm64/libAaru.Compression.Native.so
mv build/linux-musl-arm64/libAaru.Compression.Native.so runtimes/linux-musl-arm64/native/

## Linux (s390x)
# Detected system processor: s390x
rm -rf build/linux-s390x
mkdir -p build/linux-s390x
mkdir -p runtimes/linux-s390x/native
docker run --rm dockcross/linux-s390x > docker/dockcross-linux-s390x
chmod +x docker/dockcross-linux-s390x
docker/dockcross-linux-s390x bash -c "cd build/linux-s390x && cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release -DAARU_BUILD_PACKAGE=1 ../.."
docker/dockcross-linux-s390x bash -c "cd build/linux-s390x && make Aaru.Compression.Native"
docker/dockcross-linux-s390x s390x-ibm-linux-gnu-strip -s -w -K "AARU*" build/linux-s390x/libAaru.Compression.Native.so
mv build/linux-s390x/libAaru.Compression.Native.so runtimes/linux-s390x/native/

## Linux (amd64)
# Detected system processor: x86_64
rm -rf build/linux-x64
mkdir -p build/linux-x64
mkdir -p runtimes/linux-x64/native
docker run --rm dockcross/linux-x64 > docker/dockcross-linux-x64
chmod +x docker/dockcross-linux-x64
docker/dockcross-linux-x64 bash -c "cd build/linux-x64 && cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release -DAARU_BUILD_PACKAGE=1 ../.."
docker/dockcross-linux-x64 bash -c "cd build/linux-x64 && make Aaru.Compression.Native"
docker/dockcross-linux-x64 x86_64-linux-gnu-strip -s -w -K "AARU*" build/linux-x64/libAaru.Compression.Native.so
mv build/linux-x64/libAaru.Compression.Native.so runtimes/linux-x64/native/

## Linux (x86)
# Detected system processor: i686
rm -rf build/linux-x86
mkdir -p build/linux-x86
mkdir -p runtimes/linux-x86/native
docker run --rm dockcross/linux-x86 > docker/dockcross-linux-x86
chmod +x docker/dockcross-linux-x86
docker/dockcross-linux-x86 bash -c "cd build/linux-x86 && cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release -DAARU_BUILD_PACKAGE=1 ../.."
docker/dockcross-linux-x86 bash -c "cd build/linux-x86 && make Aaru.Compression.Native"
docker/dockcross-linux-x86 /usr/bin/x86_64-linux-gnu-strip -s -w -K "AARU*" build/linux-x86/libAaru.Compression.Native.so
mv build/linux-x86/libAaru.Compression.Native.so runtimes/linux-x86/native/

## Windows (ARM)
# Detected system processor: arm
rm -rf build/win-arm
mkdir -p build/win-arm
mkdir -p runtimes/win-arm/native
docker run --rm dockcross/windows-armv7 > docker/dockcross-win-arm
chmod +x docker/dockcross-win-arm
docker/dockcross-win-arm bash -c "cd build/win-arm && cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release -DAARU_BUILD_PACKAGE=1 ../.."
sed -e 's/\-soname,libAaru\.Compression\.Native\.so//g' ./build/win-arm/CMakeFiles/Aaru.Compression.Native.dir/link.txt > link.txt
mv link.txt ./build/win-arm/CMakeFiles/Aaru.Compression.Native.dir/link.txt
docker/dockcross-win-arm bash -c "cd build/win-arm && make Aaru.Compression.Native"
mv build/win-arm/libAaru.Compression.Native.dll runtimes/win-arm/native/

## Windows (ARM64)
# Detected system processor: aarch64
rm -rf build/win-arm64
mkdir -p build/win-arm64
mkdir -p runtimes/win-arm64/native
docker run --rm dockcross/windows-arm64 > docker/dockcross-win-arm64
chmod +x docker/dockcross-win-arm64
docker/dockcross-win-arm64 bash -c "cd build/win-arm64 && cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release -DAARU_BUILD_PACKAGE=1 ../.."
sed -e 's/\-soname,libAaru\.Compression\.Native\.so//g' ./build/win-arm64/CMakeFiles/Aaru.Compression.Native.dir/link.txt > link.txt
mv link.txt ./build/win-arm64/CMakeFiles/Aaru.Compression.Native.dir/link.txt
docker/dockcross-win-arm64 bash -c "cd build/win-arm64 && make Aaru.Compression.Native"
mv build/win-arm64/libAaru.Compression.Native.dll runtimes/win-arm64/native/

## Windows (AMD64)
# Detected system processor: x86_64
# TODO: Requires MSVCRT.DLL
rm -rf build/win-x64
mkdir -p build/win-x64
mkdir -p runtimes/win-x64/native
docker run --rm dockcross/windows-static-x64 > docker/dockcross-win-x64
chmod +x docker/dockcross-win-x64
docker/dockcross-win-x64 bash -c "cd build/win-x64 && cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release -DAARU_BUILD_PACKAGE=1 ../.."
docker/dockcross-win-x64 bash -c "cd build/win-x64 && make Aaru.Compression.Native"
mv build/win-x64/libAaru.Compression.Native.dll runtimes/win-x64/native/

## Windows (x86)
# Detected system processor: i686
# TODO: Requires MSVCRT.DLL
rm -rf build/win-x86
mkdir -p build/win-x86
mkdir -p runtimes/win-x86/native
docker run --rm dockcross/windows-static-x86 > docker/dockcross-win-x86
chmod +x docker/dockcross-win-x86
docker/dockcross-win-x86 bash -c "cd build/win-x86 && cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release -DAARU_BUILD_PACKAGE=1 ../.."
docker/dockcross-win-x86 bash -c "cd build/win-x86 && make Aaru.Compression.Native"
mv build/win-x86/libAaru.Compression.Native.dll runtimes/win-x86/native/

## Mac OS X (arm64 and x64)
if [[ ${OS_NAME} == Darwin ]]; then
    rm -rf build/osx-x64
    mkdir -p build/osx-x64
    cd build/osx-x64
    cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release -DAARU_BUILD_PACKAGE=1 -DAARU_MACOS_TARGET_ARCH=x86_64 ../..
    make Aaru.Compression.Native
    cd ../..
    mkdir -p runtimes/osx-x64/native
    mv build/osx-x64/libAaru.Compression.Native.dylib runtimes/osx-x64/native

    rm -rf build/osx-arm64
    mkdir -p build/osx-arm64
    cd build/osx-arm64
    cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release -DAARU_BUILD_PACKAGE=1 -DAARU_MACOS_TARGET_ARCH=arm64 ../..
    make Aaru.Compression.Native
    cd ../..
    mkdir -p runtimes/osx-arm64/native
    mv build/osx-arm64/libAaru.Compression.Native.dylib runtimes/osx-arm64/native
fi

# TODO: "linux-musl-x64"
# TODO: "linux-musl-x86"

nuget pack

