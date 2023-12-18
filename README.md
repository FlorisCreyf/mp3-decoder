# MP3 Decoder

An unoptimized floating point decoder for MPEG-1 Layer 3.

## Status

1. Reads most of the content in ID3v2 tags.
2. Ignores XING or INFO tags.
3. Ignores ID3v1 entirely.

## Summary

Raw digital audio is stored within a pulse code modulation (PCM) stream. The problem with PCM is that it takes up a lot of memory and can pose an inconvenience especially when streaming audio over the internet, TV, or radio. But we can process the signal so that it takes up less space.

<table>
  <tr>
    <td>ID3</td>
    <td colspan=5>MP3</td></tr>
  <tr>
    <td>Meta Data</td>
    <td>Header</td>
    <td>Side Information</td>
    <td>Main Data</td>
    <td>Header</td>
    <td>...</td>
  </tr>
</table>

### The Decoder

There are two ways a file can be compressed: lossless, and lossy. Compressing a file in a lossless manner means that when the file is reconstructed we end up exactly with what we started with. Lossy compression ideally removes information we have no use for. Because we can only hear between a certain range of frequencies and decibels, we can remove samples that fall outside of the hearable range. It is also possible for a frequency to mask another frequency causing the second frequency to become inaudible.

MP3 and AAC encoders combine both principles so that we can store more music and stream those files at a faster rate. The decoder is responsible for undoing the encoder's compression when the file is about to be played.

The decoder is responsible for:

- Deriving PCM data from the bitstream.
- Handing the data over to the operating system (or music player).

### ID3 Metadata

ID3 (version 2) is a block of bytes wherein metadata is stored. This metadata is mostly irrelevant to the decoder, except for an offset that points to the end of the tag. Inconveniently, there might come subsequent ID3 tags after the first. A full documentation is available at [ID3.org](https://id3.org).

| Type       | Offset (bytes) | Description                                         |
| :--------- | :------------- | :-------------------------------------------------- |
| Identifier | 0 - 2          | Indicates the presence of ID3.                      |
| Version    | 3 - 4          | Version and revision of the tag.                    |
| Flags      | 5              | Four single-bit booleans.                           |
| Size       | 6 - 9          | Size of the ID3 tag excluding this ten byte header. |

### MP3 Header

The MP3 headers contain parameters such as the sampling rate, bit rate, number of channels, and version. With this data we can determine the size of the MP3 frame, which will also tell us where the next header is.

$$ \frac{\text{samples per frame}}{8} \times \frac{\text{bit rate}}{\text{sampling rate}} + \text{padding} $$

We can find the second header knowing the size of the first frame. Within the first frame there usually is an INFO or XING tag that can aid in navigating the file.

### Side Information

Each header is followed by side information and contains the data needed to find where the main data is located. The main data is not located after the side information due to the varying sizes of the Huffman encoded samples. The side information also includes additional values that will be used in the requantization formula to reconstruct the samples into real numbers.

### Main Data

The main data is divided into two granules. Depending on the channel mode defined in the header each granule can have up to two channels. A channel starts with scale factors followed by Huffman bits. From the Huffman bits, 576 frequency lines are extracted per channel. At the end of the main data there is user defined data called ancillary data.

The samples are scaled by scale factors in the requantization formula. Different scaling factors are applied to different groups, or subbands, of samples.

Located after the scale factors are the Huffman bits. This region is divided into three big value regions, a quadruples region, and a zero region. Depending on the side information, each region uses one of the many Huffman tables. The quadruples region containing higher pitches is much more tightly compressed and makes use of just two tables. The zero region contains omitted samples.

1. Big Value Region
2. Big Value Region
3. Big Value Region
4. Quadruples Region
5. Zero Region

It is probably necessary at this point to write a separate program that can embed the Huffman tables in C code. The Huffman tables in my source code should be parsed so that the Huffman bits are shifted to the right of each integer and so that the length of each Huffman value (in bits) is stored as well.

```c
unsigned bit_sample = get_bits(bitstream, bit, bit + 32);
for ( ... ) {
    int value = table.hcod[entry];
    int size = table.hlen[entry];

    if (value == bit_sample >> (32 - size)) {
        ...
    }
}
```

Each region is further subdivided into two block types:

- Long blocks: Higher frequency resolution.
- Short blocks: Lower frequency resolution. Short blocks are one-third the size of long blocks and are not aligned in order so they have to be reorded. This is assuming that the frequencies are closer together at certain points, therefore using fewer Huffman codes. Short blocks are used to counter pre-echo.

### Inverse Quantization

Quantization takes something that is continuous, or infinite, and turns it into a discrete value. The Huffman samples represent a discrete data set. The inverse quantization formula processes the scale factors and other data from the side information to construct real numbers (which are only theoretically continuous) from our original Huffman samples.

$$ S_i = \text{sign}(s_i) \times |s_i|^{\frac{4}{3}} \times 2^{\frac{a}{4}} \times 2^{-b} $$

The exponents for long blocks are:

```c
a = global_gain[gr][ch] - 210
b = (scalefac_scale[gr][ch] == 0 ? 0.5 : 1) * scalefactor[gr][ch][sb] + preflag[gr][ch] * pretab[sb]
```
The exponents for short blocks are:

```c
a = global_gain[gr][ch] - 210 - 8 · subblock_gain[gr][ch][window]
b = (scalefac_scale[gr][ch] == 0 ? 0.5 : 1) * scalefactor[gr][ch][sb][window]
```

### Reordering

Only short blocks need to be reordered. If the first few subbands are long blocks, then those bands should be excluded. During this phase, samples in each subband are mapped to blocks of 18 samples.

```c
// OUTPUT:
0, 1, 2, 3, 12, 13, 4, 5, 6, 7, 16, 17, 8, 9, 10 . . .
// TABLE:
// 0 - 17 | subband_short_32[0] (4)
0, 4, 8,    // 0,  6,  12,       0 + 0, 0 + 6, 0 + 12
1, 5, 9,    // 1,  7,  13,       1 + 0, 1 + 6, 1 + 12
2, 6, 10,   // 2,  8,  14,       2 + 0, 2 + 6, 2 + 12
3, 7, 11,   // 3,  9,  15,       3 + 0, 3 + 6, 3 + 12
// subband_short_32[1] (4)
12, 16, 20, // 4,  10, 16,       4 + 0, 4 + 6, 4 + 12
13, 17, 21, // 5,  11, 17,       5 + 0, 5 + 6, 6 + 12
// 18 - 35
14, 18, 22, // 18, 24, 30,       18 + 0, 18 + 6, 18 + 12
15, 19, 23, // 19, 25, 31,
// subband_short_32[2] (4)
24, 28, 32, // 20, 26, 32,
25, 29, 33, // 21, 27, 33,
26, 30, 34, // 22, 28, 34,
27, 31, 35, // 23, 29, 35,
// 36 - 53 | subband_short_32[3] (4)
36, 40, 44, // 36, 42, 48,
37, 41, 45, // 37, 43, 49,
38, 42, 46, // 38, 44, 50,
39, 43, 47, // 39, 45, 51,
// subband_short_32[4] (6)
48, 54, 60, // 40, 46, 52,
49, 55, 61, // 41, 47, 53,
// 54 - 71
50, 56, 62, // 54, 60, 66,
51, 57, 63, // 55, 61, 67,
52, 58, 64, // 56, 62, 68,
53, 59, 65, // 57, 63, 69
...
```

### Inverse Modified Discrete Cosine Transform (IMDCT)

Another layer of lossy compression is the DCT. The MDCT takes closely related samples and places them on a cosine function. For long blocks the MDCT reduces 32 samples to 18 samples, and for short blocks the MCDT reduces 12 samples to six.

$$ x_i = \sum_{k=0}^{\frac{n}{2}-1}{X_k \cos{\left(\frac{\pi}{2n}\left[ 2i + 1 + \frac{n}{2} \right]\left[ 2k + 1 \right]\right)}} $$

Variable $n$ is 12 for short blocks and 36 for long blocks. The IMDCT produces $x_0$ through $x_n - 1$. Once the cosine transform is complete the samples need to be windowed and overlapped.

### Fast Fourier Transform

The pulse code modulation stream, the input to the encoder, is in the time domain. The encoder converts these time domain samples to frequency domain samples through a Fast Fourier Transform. When decoding, the IMDCT converts the frequency domain samples into time domain samples.
hexdump

### Synthesis Filter Bank

The encoder takes a PCM stream and divides it into several bands that approximate critical bands. A critical band contains frequencies that sound similar and affect the same area of the basilar membrane in the cochlea. Bands are structured similar to critical bands so that artifacts due to quantization are masked in the same band.

Critical bands become larger as frequency increases, and the encoder's filter divides the frequency spectrum into equal sized bands. In other words, it is harder to discern between two higher frequency sounds than two lower frequency sounds. The decoder eventually takes care of reconstruction.

### References

- ISO/IEC 11172-3:1993 | Information technology -- Coding of moving pictures and associated audio for digital storage media at up to about 1,5 Mbit/s -- Part 3: Audio
- [id3.org/Developer%20Information](http://id3.org/Developer%20Information)
- [blog.bjrn.se/2008/10/lets-build-mp3-decoder.html](http://blog.bjrn.se/2008/10/lets-build-mp3-decoder.html)
- Oneplay MP3 decoder (source)
