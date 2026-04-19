# Aaru.Compression.Native

This repository contains the Aaru.Compression.Native library for [Aaru](https://www.aaru.app).

The purpose of this library is to provide compression and de-compression algorithms for Aaru.

No archiver processing code should fall here, those go
in [Aaru.Compression](https://github.com/aaru-dps/Aaru/tree/devel/Aaru.Compression).

To build you just need Docker on Linux and run `build.sh`, that will generate a NuGet package for use with
Aaru.Compression.

Currently implemented algorithms are:

### General-purpose codecs
- Apple Data Compression (RLE with sliding dictionary created for Apple Disk Copy's NDIF)
- Apple LZH (LH1 variant used by Apple DART's "best" compression mode)
- Apple RLE (Run Length Encoding created for Apple DART)
- [BZIP2](https://gitlab.com/bzip2/bzip2.git)
- [FLAC](https://github.com/xiph/flac)
- [LZ4](https://github.com/lz4/lz4)
- [LZFSE](https://github.com/lzfse/lzfse)
- [LZIP](http://www.nongnu.org/lzip)
- [LZMA / LZMA2 / XZ](https://www.7-zip.org)
- [LZO](http://www.oberhumer.com/opensource/lzo/)
- [Zstandard](https://facebook.github.io/zstd)

### Archive decompressors
- ACE v1 (LZ77) and v2 (blocked with delta/EXE/sound/picture modes)
- ARC methods 3–9 (pack, squeeze, crunch, squash) and PAK methods 10–11 (crush, distill)
- ARJ methods 1–4 and ARJZ (standard LZH and extended DEFLATE)
- Compact Pro (RLE, LZH+RLE)
- DiskDoubler (ADn LZSS, DDn block Huffman LZ77, adaptive Huffman, Stac LZS, Compact Pro, LZW)
- HA (ASC and HSC)
- LHA (LH1–LH7, LArc LZS/LZ5, PMarc PM1/PM2)
- RAR 1.5 (custom LZ77 with fixed Huffman tables)
- RAR 2.0 (block Huffman LZ77 with optional multichannel audio)
- RAR 3.0 (Huffman LZ77 / PPMd Variant H with VM-based post-filters)
- RAR 5.0 (Huffman LZ77 with native Delta/E8/E8E9/ARM filters)
- StuffIt classic methods 1–3, 5, 8, 13–15 (RLE, LZW, Huffman, LZAH, MW, LZSS, Arsenic)
- StuffIt X methods 0–4, 6 (Brimstone/PPMd, Cyanide/BWT, Darkhorse/LZSS, Deflate, Blend, Iron/BWT)
- StuffIt X preprocessors (English dictionary, x86 executable transform)
- ZIP methods 1–6, 9, 96–98 (Shrink, Reduce, Implode, Deflate64, WinZip JPEG, WavPack, PPMd)
- Zoo (LZD, LH5)

Each of these algorithms have a corresponding license, that can be found in their corresponding folder.

The resulting output of `build.sh` falls under the LGPL 2.1 license as stated in the [LICENSE file](LICENSE).

Any new algorithm added should be under a license compatible with the LGPL 2.1 license to be accepted.

© 2021-2026 Natalia Portillo