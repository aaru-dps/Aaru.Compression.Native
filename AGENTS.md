# AGENTS.md

This document provides guidance for AI agents working with the Aaru.Compression.Native codebase.

## Project Overview

Aaru.Compression.Native is a C library providing compression and decompression algorithms for the [Aaru Data Preservation Suite](https://www.aaru.app). It builds native libraries for multiple platforms (Windows, Linux, macOS, Android) and architectures (x86, x64, ARM, ARM64, etc.) and packages them as a NuGet package for use with .NET applications.

## Repository Structure

```
├── library.c/h          # Main library exports and entry points
├── adc.c/h              # Apple Data Compression implementation
├── apple_rle.c/h        # Apple RLE implementation
├── flac.c/h             # FLAC wrapper
├── lzip.c               # LZIP wrapper
├── 3rdparty/            # Third-party compression libraries (submodules)
│   ├── bzip2/           # BZIP2 library
│   ├── flac/            # FLAC library
│   ├── lz4/             # LZ4 library
│   ├── lzfse/           # LZFSE library (Apple)
│   ├── lzlib/           # LZIP library
│   ├── lzma/            # LZMA/7-Zip SDK
│   └── zstd/            # Zstandard library
├── arc/                 # ARC archive compression methods
├── ha/                  # HA archive compression methods
├── pak/                 # PAK archive compression methods
├── zoo/                 # Zoo archive compression methods (LZD, LH5)
├── tests/               # Google Test based unit tests
├── docker/              # Docker scripts for cross-compilation
├── runtimes/            # Built native libraries per platform
├── CMakeLists.txt       # CMake build configuration
├── build.sh             # Cross-platform build script
└── Aaru.Compression.Native.nuspec  # NuGet package specification
```

## Build System

- **Build Tool**: CMake (minimum version 3.15)
- **Language**: C99 (C11 for MSVC ARM)
- **Build Script**: `build.sh` uses Docker (dockcross images) for cross-compilation

### Building Locally

For local development on macOS:
```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
```

For a full release build (requires Docker on Linux):
```bash
./build.sh
```

### Running Tests

Tests use Google Test framework:
```bash
cd build
make tests_run
./tests/tests_run
```

## Coding Conventions

### File Structure
- Each compression algorithm has its own `.c` and `.h` file pair
- Third-party libraries are in `3rdparty/` as git submodules
- Public API functions are declared in `library.h` and implemented where appropriate

### Naming Conventions
- Public API functions use `AARU_` prefix (e.g., `AARU_bzip2_decode_buffer`)
- Macros: `AARU_EXPORT`, `AARU_CALL`, `AARU_LOCAL`
- All public functions must use `AARU_EXPORT` and `AARU_CALL` for cross-platform compatibility

### Export Conventions
```c
AARU_EXPORT return_type AARU_CALL function_name(parameters);
```

### Function Patterns
Every exported buffer transform has the same shape:

```c
int32_t AARU_<algorithm>_decode_buffer(const uint8_t *src_buffer, size_t src_size,
                                       uint8_t *dst_buffer, size_t *dst_size, ...options);
int32_t AARU_<algorithm>_encode_buffer(const uint8_t *src_buffer, size_t src_size,
                                       uint8_t *dst_buffer, size_t *dst_size, ...options);
```

- On entry `*dst_size` is the capacity of `dst_buffer`; on success it holds the number of bytes written.
- The return value is `0` (`AARU_ERROR_NONE`) on success, non-zero on error — either an `AARU_ERROR_*`
  constant from `library.h` or the underlying library's own error code when it is propagated.
- The only exceptions are the streaming LZD context API (`CreateLZDContext`, `LZD_FeedNative`,
  `LZD_DrainNative`, `DestroyLZDContext`) and `AARU_get_acn_version`.

## Adding New Compression Algorithms

1. **License Check**: New algorithms must use a license compatible with LGPL 2.1
2. **Create Source Files**: Add `algorithm.c` and `algorithm.h` in the root or appropriate subdirectory
3. **Add to CMakeLists.txt**: Include source files in the `add_library` call
4. **Declare in library.h**: Add function prototypes with proper `AARU_EXPORT` and `AARU_CALL`
5. **Add Tests**: Create test file in `tests/` using Google Test
6. **Update Documentation**: Add to README.md algorithm list

## Testing Guidelines

- Each algorithm should have corresponding tests in `tests/`
- Test data files go in `tests/data/`
- Tests verify both compression and decompression where applicable
- Use CRC32 validation for data integrity checks

## Platform Support

The library targets:
- **Windows**: x86, x64, ARM, ARM64
- **Linux**: x86, x64, ARM, ARM64, MIPS64, PowerPC64, RISC-V, s390x, musl variants
- **macOS**: x64, ARM64 (minimum deployment target: 10.15)
- **Android**: x86, x64, ARM, ARM64

## Version Information

- Version is defined in `library.h` as `AARU_CHECKUMS_NATIVE_VERSION`
- Use `AARU_get_acn_version()` to retrieve the version at runtime

## Third-Party Dependencies

All third-party libraries are included as git submodules in `3rdparty/`:
- Managed via `.gitmodules`
- Each has its own `.cmake` file for build configuration
- Licenses are in their respective folders

## Important Notes

- Do not add archiver processing code here; those belong in [Aaru.Compression](https://github.com/aaru-dps/Aaru/tree/devel/Aaru.Compression)
- The `bz_internal_error` function stub is required when `BZ_NO_STDIO` is defined
- Symbol stripping in `build.sh` preserves only `AARU*` symbols for minimal binary size

