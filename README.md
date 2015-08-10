# MP3 Decoder

An unoptimized floating point decoder, specifically for MPEG-1 Layer 3.

## Status

1. Reads most of the stuff in ID3v2 tags.
2. Ignores XING or INFO tags.
3. Ignores ID3v1 entirely.
4. Final PCM stream might still have some errors. Could be ALSA setup.
5. Several TODOs remaining throughout the source.

I wrote a summary titled [MP3 Decoding in C++](http://www.fcreyf.com/11114/mp3-decoding-in-c++), although the source should be fairly straightforward. Feel free to make suggestions, fork, or whatever.

## Development

Once the first byte of the MP3 file is found, most of the complexity lies in understanding the ISO (e.g. the ISO never mentions scalefac_mult but used it in the dequantizer formula). Although there exist several open source decoders such as MAD and MPG123, they are fairly heavily optimized.

My debugging method is _break points_. Is the decoder outputting weird values such as "inf" and "NaN"? Move the break point back until there aren't "inf"s or "NaNs" anymore. Then notice that occasionally no Huffman values are matched to the bitstream sample. Why? Track down what values are missed, and figure that some of the Huffman values have extra bits attached.

Comparing values against values from other decoders is helpful as well.

## Licencing

This project is a form of speech. Actually converging the source into a working program might require the purchase of a patent license. A compiled version, though, wouldn't be very suitable in real world applications, considering that there are plenty of better options like MAD.
