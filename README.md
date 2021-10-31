# Aaru.Compression.Native

This repository contains the Aaru.Compression.Native library for [Aaru](https://www.aaru.app).

The purpose of this library is to provide compression and de-compression algorithms for Aaru.

No archiver processing code should fall here, those go in [Aaru.Compression](https://github.com/aaru-dps/Aaru/tree/devel/Aaru.Compression).

To build you just need Docker on Linux and run `build.sh`, that will generate a NuGet package for use with Aaru.Compression.

Currently implemented algorithms are:
- Apple Data Compression (RLE with sliding dictionary created for Apple Disk Copy's NDIF)
- Apple RLE (Run Length Encoding created for Apple DART)
- [BZIP2](https://gitlab.com/bzip2/bzip2.git)
- [FLAC](https://github.com/xiph/flac)
- [LZFSE](https://github.com/lzfse/lzfse)
- [LZIP](http://www.nongnu.org/lzip)
- [LZMA](https://www.7-zip.org)
- [Zstandard](https://facebook.github.io/zstd)

Each of these algorithms have a corresponding license, that can be found in their corresponding folder.

The resulting output of `build.sh` falls under the LGPL 2.1 license as stated in the [LICENSE file](LICENSE).

Any new algorithm added should be under a license compatible with the LGPL 2.1 license to be accepted.

© 2021 Natalia Portillo