# MP3 Decoder

An unoptimized floating point decoder for MPEG-1 Layer 3.

## Status

1. Reads most of the content in ID3v2 tags.
2. Ignores XING or INFO tags.
3. Ignores ID3v1 entirely.

I wrote a summary titled [MP3 Decoding in C++](http://www.fcreyf.com/11114/mp3-decoding-in-c++), although the source should be fairly straightforward. Feel free to make suggestions, fork, or whatever.

## Development

Once the first byte of the MP3 file is found, most of the complexity lies in understanding the ISO. Although there exist several open source decoders like MAD and MPG123, they are fairly heavily optimized.

Determining correct output is straightforward. Either the PCM or configuration is wrong if the audio sounds distorted. The output from a decoder such as MPG123 can be used as a reference for debugging purposes.

## Licencing

This project is a form of speech. Actually converging the source into a working program might require the purchase of a patent license. A compiled version of this project wouldn't be very suitable in real world applications, considering that there are plenty of better and optimized options like MAD.
