/*
 * Author: Floris Creyf
 * Date: May 2015
 * Unpacks and decodes frames/headers.
 */

#include <string.h>
#include "mp3.h"
#include "util.h"

#define PI    3.141592653589793
#define SQRT2 1.414213562373095

/**
 * Unpack the MP3 header.
 * @param buffer A pointer that points to the first byte of the frame header.
 */
void mp3::init_header_params(unsigned char *buffer)
{
	if (buffer[0] == 0xFF && buffer[1] >= 0xE0) {
		this->buffer = buffer;

		static bool init = true;
		if (init) {
			set_mpeg_version();
			set_layer(buffer[1]);
			set_crc();
			set_channel_mode(buffer);
			set_info();
			set_emphasis(buffer);
			set_mode_extension(buffer);
			set_sampling_rate();
			set_tables();

			mono = channel_mode == 3;
			init = false;
		}

		set_padding();
		set_bit_rate(buffer);
		set_frame_size();
	} else {
		if (buffer[0] != 0xFF)
			valid = false;
	}
}

/**
 * Unpack and decode the MP3 frame.
 * @param buffer A pointer that points to the first byte of the frame header.
 */
void mp3::init_frame_params(unsigned char *buffer)
{
	set_side_info(&buffer[4]);
	set_main_data(buffer);
	for (int gr = 0; gr < 2; gr++) {
		for (int ch = 0; ch < (mono ? 1 : 2); ch++)
			requantize(gr, ch);

		if (!mono && mode_extension[0] != 0)
			ms_stereo(gr);

		for (int ch = 0; ch < (mono ? 1 : 2); ch++) {
			reorder(gr, ch);
			alias_reduction(gr, ch);
			imdct(gr, ch);
			synth_filterbank(gr, ch);
		}
	}
}

mp3::mp3(unsigned char *buffer)
{
	if (buffer[0] == 0xFF && buffer[1] >= 0xE0) {
		valid = true;
		frame_size = 0;
		main_data_begin = 0;
		init_header_params(buffer);
	}
}

mp3::mp3(const mp3 &orig)
{
}

mp3::~mp3()
{
}

/** Check validity of the header and frame. */
bool mp3::is_valid()
{
	return valid;
}

/** Determine MPEG version. */
inline void mp3::set_mpeg_version()
{
	if ((buffer[1] & 0x10) == 0x10 && (buffer[1] & 0x08) == 0x08)
		mpeg_version = 1;
	else if ((buffer[1] & 0x10) == 0x10 && (buffer[1] & 0x08) != 0x08)
		mpeg_version = 2;
	else if ((buffer[1] & 0x10) != 0x10 && (buffer[1] & 0x08) == 0x08)
		mpeg_version = 0;
	else if ((buffer[1] & 0x10) != 0x10 && (buffer[1] & 0x08) != 0x08)
		mpeg_version = 2.5;
}

float mp3::get_mpeg_version()
{
	return mpeg_version;
}

/** Determine layer. */
inline void mp3::set_layer(unsigned char byte)
{
	byte = byte << 5;
	byte = byte >> 6;
	layer = 4 - byte;
}

unsigned mp3::get_layer()
{
	return layer;
}

/**
 * Cyclic redundancy check. If set, two bytes after the header information are
 * used up by the CRC.
 */
inline void mp3::set_crc()
{
	crc = buffer[1] & 0x01;
}

bool mp3::get_crc()
{
	return crc;
}

/**
 * For variable bit rate (VBR) files, this data has to be gathered constantly.
 */
inline void mp3::set_bit_rate(unsigned char *buffer)
{
	if (mpeg_version == 1) {
		if (layer == 1) {
			bit_rate = buffer[2] * 32;
		} else if (layer == 2) {
			const int rates[14] {32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384};
			bit_rate = rates[(buffer[2] >> 4) - 1] * 1000;
		} else if (layer == 3) {
			const int rates[14] {32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320};
			bit_rate = rates[(buffer[2] >> 4) - 1] * 1000;
		} else
			valid = false;
	} else {
		if (layer == 1) {
			const int rates[14] {32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256};
			bit_rate = rates[(buffer[2] >> 4) - 1] * 1000;
		} else if (layer < 4) {
			const int rates[14] {8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160};
			bit_rate = rates[(buffer[2] >> 4) - 1] * 1000;
		} else
			valid = false;
	}
}

unsigned mp3::get_bit_rate()
{
	return bit_rate;
}

/**
 * Sampling rate.
 */
inline void mp3::set_sampling_rate()
{
	int rates[3][3] {44100, 48000, 32000, 22050, 24000, 16000, 11025, 12000, 8000};

	for (int version = 1; version <= 3; version++)
		if (mpeg_version == version) {
			if ((buffer[2] & 0x08) != 0x08 && (buffer[2] & 0x04) != 0x04) {
				sampling_rate = rates[version - 1][0];
				break;
			} else if ((buffer[2] & 0x08) != 0x08 && (buffer[2] & 0x04) == 0x04) {
				sampling_rate = rates[version - 1][1];
				break;
			} else if ((buffer[2] & 0x08) == 0x08 && (buffer[2] & 0x04) != 0x04) {
				sampling_rate = rates[version - 1][2];
				break;
			}
		}
}

unsigned mp3::get_sampling_rate()
{
	return sampling_rate;
}

/**
 * During the decoding process different tables are used depending on the
 * sampling rate.
 */
inline void mp3::set_tables()
{
	switch (sampling_rate) {
		case 32000:
			band_index.short_win = t_band_index.short_32;
			band_index.long_win = t_band_index.long_32;
			break;
		case 44100:
			band_index.short_win = &t_band_index.short_44[0];
			band_index.long_win = &t_band_index.long_44[0];
			break;
		case 48000:
			band_index.short_win = t_band_index.short_48;
			band_index.long_win = t_band_index.long_48;
			break;
	}
}

/**
 * If set, the frame size is 1 byte larger.
 */
inline void mp3::set_padding()
{
	padding = buffer[2] & 0x02;
}

bool mp3::get_padding()
{
	return padding;
}

/**
 * 0 -> Stereo
 * 1 -> Joint stereo (this option requires use of mode_extension)
 * 2 -> Dual channel
 * 3 -> Single channel
 */
inline void mp3::set_channel_mode(unsigned char *buffer)
{
	channel_mode = buffer[3] >> 6;
}

unsigned mp3::get_channel_mode()
{
	return channel_mode;
}

/** Applies only to joint stereo. */
inline void mp3::set_mode_extension(unsigned char *buffer)
{
	if (layer == 1 || layer == 2) {
		/* This is different from layer 3.
		 * A range of bands is determined instead. */
		unsigned char index = buffer[3] << 2;
		index = index >> 6;
		mode_extension[0] = 4 + index * 4;
		mode_extension[1] = 31;
	} else {
		mode_extension[0] = buffer[3] & 0x20;
		mode_extension[1] = buffer[3] & 0x10;
	}
}

unsigned *mp3::get_mode_extension()
{
	return mode_extension;
}

/** Although rarely used, there is no method for emphasis. */
inline void mp3::set_emphasis(unsigned char *buffer)
{
	emphasis = buffer[3] << 6;
	emphasis = emphasis >> 6;
}

unsigned mp3::get_emphasis()
{
	return emphasis;
}

/** Additional information (not important). */
inline void mp3::set_info()
{
	info[0] = buffer[2] & 0x01;
	info[1] = buffer[3] & 0x08;
	info[2] = buffer[3] & 0x04;
}

bool *mp3::get_info()
{
	return info;
}

/** Determine the frame size. */
inline void mp3::set_frame_size()
{
	unsigned int samples_per_frame;
	switch (layer) {
		case 3:
			if (mpeg_version == 1) samples_per_frame = 1152;
			else samples_per_frame = 576;
			break;
		case 2:
			samples_per_frame = 1152;
			break;
		case 1:
			samples_per_frame = 384;
			break;
	}
	prev_frame_size = frame_size;
	frame_size = (samples_per_frame / 8 * bit_rate / sampling_rate);
	if (padding == 1)
		frame_size += 1;
}

unsigned mp3::get_frame_size()
{
	return frame_size;
}

/**
 * The side information contains information on how to decode the main_data.
 * @param buffer A pointer to the first byte of the side info.
 */
inline void mp3::set_side_info(unsigned char *buffer)
{
	int count = 0;

	/* Number of bytes the main data ends before the next frame header. */
	prev_main_data_begin = main_data_begin;
	main_data_begin = (int)get_bits_inc(buffer, &count, 9);

	/* Skip private bits. Not necessary. */
	count += mono ? 5 : 3;

	for (int ch = 0; ch < (mono ? 1 : 2); ch++)
		for (int scfsi_band = 0; scfsi_band < 4; scfsi_band++)
			/* - Scale factor selection information.
			 * - If scfsi[scfsi_band] == 1 then scale factors for the first
			 *   granule are also used for the second granule.
			 * - scfsi_band indicates what group of scaling factors are reused. */
			scfsi[ch][scfsi_band] = (int)get_bits_inc(buffer, &count, 1);

	for (int gr = 0; gr < 2; gr++)
		for (int ch = 0; ch < (mono ? 1 : 2); ch++) {
			/* Length of the scaling factors and main data in bits. */
			part2_3_length[gr][ch] = (int)get_bits_inc(buffer, &count, 12);
			/* Number of values in each big_region. */
			big_value[gr][ch] = (int)get_bits_inc(buffer, &count, 9);
			/* Quantizer step size. */
			global_gain[gr][ch] = (int)get_bits_inc(buffer, &count, 8);
			/* Used to determine the values of slen1 and slen2. */
			scalefac_compress[gr][ch] = (int)get_bits_inc(buffer, &count, 4);
			/* Number of bits given to a range of scale factors.
			 * - Normal blocks: slen1 0 - 10, slen2 11 - 20
			 * - Short blocks && mixed_block_flag == 1: slen1 0 - 5, slen2 6-11
			 * - Short blocks && mixed_block_flag == 0: */
			slen1[gr][ch] = slen[scalefac_compress[gr][ch]][0];
			slen2[gr][ch] = slen[scalefac_compress[gr][ch]][1];
			/* If set, a not normal window is used. */
			window_switching[gr][ch] = get_bits_inc(buffer, &count, 1) == 1;

			if (window_switching[gr][ch]) {
				/* The window type for the granule.
				 * 0: reserved (long block)
				 * 1: start block (long block)
				 * 2: 3 short windows (short block)
				 * 3: end block (long block) */
				block_type[gr][ch] = (int)get_bits_inc(buffer, &count, 2);
				/* Number of scale factor bands before window switching. */
				mixed_block_flag[gr][ch] = get_bits_inc(buffer, &count, 1) == 1;
				if (mixed_block_flag[gr][ch]) {
					switch_point_l[gr][ch] = 8;
					switch_point_s[gr][ch] = 3;
				} else {
					switch_point_l[gr][ch] = 0;
					switch_point_s[gr][ch] = 0;
				}

				/* These are set by default if window_switching. */
				region0_count[gr][ch] = block_type[gr][ch] == 2 ? 8 : 7;
				region1_count[gr][ch] = -1; /* No third region. */

				for (int region = 0; region < 2; region++)
					/* Huffman table number for a big region. */
					table_select[gr][ch][region] = (int)get_bits_inc(buffer, &count, 5);
				for (int window = 0; window < 3; window++)
					subblock_gain[gr][ch][window] = (int)get_bits_inc(buffer, &count, 3);
			} else {
				/* Set by default if !window_switching. */
				block_type[gr][ch] = 0;
				mixed_block_flag[gr][ch] = false;

				for (int region = 0; region < 3; region++)
					table_select[gr][ch][region] = (int)get_bits_inc(buffer, &count, 5);

				/* Number of scale factor bands in the first big value region. */
				region0_count[gr][ch] = (int)get_bits_inc(buffer, &count, 4);
				/* Number of scale factor bands in the third big value region. */
				region1_count[gr][ch] = (int)get_bits_inc(buffer, &count, 3);
				/* # scale factor bands is 12*3 = 36 */
			}

			/* If set, add values from a table to the scaling factors. */
			preflag[gr][ch] = (int)get_bits_inc(buffer, &count, 1);
			/* Determines the step size. */
			scalefac_scale[gr][ch] = (int)get_bits_inc(buffer, &count, 1) == 0 ? SQRT2 : 2;
			/* Table that determines which count1 table is used. */
			count1table_select[gr][ch] = (int)get_bits_inc(buffer, &count, 1);
		}
}

/**
 * Due to the Huffman bits' varying length the main_data isn't aligned with the
 * frames. Unpacks the scaling factors and quantized samples.
 * @param buffer - A buffer that points to the the first byte of the frame header.
 */
inline void mp3::set_main_data(unsigned char *buffer)
{
	/* header + side_information */
	int constant = mono ? 21 : 36;
	if (crc == 0)
		constant += 2;

	/* Let's put the main data in a separate buffer so that side info and header
	 * don't interfere. The main_data_begin may be larger than the previous
	 * and doesn't include the size of side info and headers. */
	unsigned char *main_data;
	if (main_data_begin > prev_frame_size) {
		int part2_len = prev_frame_size - constant;
		int part1_len = main_data_begin - part2_len;
                int part12_len = part1_len + part2_len;

		int buffer_length = part12_len + frame_size;
	 	int buffer_offset = main_data_begin + constant;

		main_data = new unsigned char[buffer_length];
		memcpy(main_data, buffer - buffer_offset, part1_len);
                memcpy(&main_data[part1_len], buffer - part2_len, part2_len);
		memcpy(&main_data[part12_len], buffer + constant, frame_size - constant);
	} else {
		int buffer_length = main_data_begin + frame_size;

		main_data = new unsigned char[buffer_length];
		memcpy(main_data, buffer - main_data_begin, main_data_begin);
		memcpy(&main_data[main_data_begin], buffer + constant, frame_size - constant);
	}

	int bit = 0;
	for (int gr = 0; gr < 2; gr++)
		for (int ch = 0; ch < (mono ? 1 : 2); ch++) {
			int max_bit = bit + part2_3_length[gr][ch];
			unpack_scalefac(main_data, gr, ch, bit);
			unpack_samples(main_data, gr, ch, bit, max_bit);
			bit = max_bit;
		}

	delete[] main_data;
}

/**
 * This will get the scale factor indices from the main data. slen1 and slen2
 * represent the size in bits of each scaling factor. There are a total of 21 scaling
 * factors for long windows and 12 for each short window.
 * @param main_data - Buffer solely containing the main_data - excluding the frame header and side info.
 * @param gr
 * @param ch
 */
inline void mp3::unpack_scalefac(unsigned char *main_data, int gr, int ch, int &bit)
{
	int sfb, window;
	int scalefactor_length[2] {
		slen[scalefac_compress[gr][ch]][0],
		slen[scalefac_compress[gr][ch]][1]};

        int scalefac[39] = {0};
        int i = 0;

	/* No scale factor transmission for short blocks. */
	if (block_type[gr][ch] == 2 && window_switching[gr][ch]) {
		if (mixed_block_flag[gr][ch] == 1) { /* Mixed blocks. */
			for (sfb = 0; sfb < 8; sfb++) {
				scalefac_l[gr][ch][sfb] = (int)get_bits_inc(main_data, &bit, scalefactor_length[0]);
				scalefac[i++] = scalefac_l[gr][ch][sfb];
			}
			for (sfb = 3; sfb < 6; sfb++)
				for (window = 0; window < 3; window++) {
					scalefac_s[gr][ch][window][sfb] = (int)get_bits_inc(main_data, &bit, scalefactor_length[0]);
					scalefac[i++] = scalefac_s[gr][ch][window][sfb];
				}
		} else { /* Short blocks. */
			for (sfb = 0; sfb < 6; sfb++)
				for (window = 0; window < 3; window++) {
					scalefac_s[gr][ch][window][sfb] = (int)get_bits_inc(main_data, &bit, scalefactor_length[0]);
					scalefac[i++] = scalefac_s[gr][ch][window][sfb];
				}
		}
		while (sfb++ < 12) {
			for (window = 0; window < 3; window++) {
				scalefac_s[gr][ch][window][sfb] = (int)get_bits_inc(main_data, &bit, scalefactor_length[1]);
				scalefac[i++] = scalefac_s[gr][ch][window][sfb];
			}
		}
		for (window = 0; window < 3; window++) {
			scalefac_s[gr][ch][window][12] = 0;
			scalefac[i++] = scalefac_s[gr][ch][window][sfb];
		}
	}

	/* Scale factors for long blocks. */
	else {
		if (gr == 0) {
			for (sfb = 0; sfb < 11; sfb++) {
				scalefac_l[gr][ch][sfb] = (int)get_bits_inc(main_data, &bit, scalefactor_length[0]);
				scalefac[i++] = scalefac_l[gr][ch][sfb];
			}
			for (; sfb < 21; sfb++) {
				scalefac_l[gr][ch][sfb] = (int)get_bits_inc(main_data, &bit, scalefactor_length[1]);
				scalefac[i++] = scalefac_l[gr][ch][sfb];
			}
		} else {

			/* Scale factors might be reused in the second granule.  */
			const int sb[5] = {6, 11, 16, 21};
			for (int i = 0; i < 2; i++)
				for (; sfb < sb[i]; sfb++) {
					if (scfsi[ch][i] == 0) {
						scalefac_l[gr][ch][sfb] = (int)get_bits_inc(main_data, &bit, scalefactor_length[0]);
						scalefac[i++] = scalefac_l[gr][ch][sfb];
					} else {
						scalefac_l[gr][ch][sfb] = scalefac_l[0][ch][sfb];
						scalefac[i++] = scalefac_l[gr][ch][sfb];
					}
				}
			for (int i = 2; i < 4; i++)
				for (sfb = sb[i]; sfb < sb[i]; sfb++) {
					if (scfsi[ch][i] == 0) {
						scalefac_l[gr][ch][sfb] = (int)get_bits_inc(main_data, &bit, scalefactor_length[1]);
						scalefac[i++] = scalefac_l[gr][ch][sfb];
					} else {
						scalefac_l[gr][ch][sfb] = scalefac_l[0][ch][sfb];
						scalefac[i++] = scalefac_l[gr][ch][sfb];
					}
				}
		}
		scalefac_l[gr][ch][21] = 0;
		scalefac[i++] = scalefac_l[gr][ch][sfb];
	}
}

/**
 * The Huffman bits (part3) will be unpacked. Four bytes are retrieved from the
 * bit stream, and are consecutively evaluated against values of the selected Huffman
 * tables.
 * | big_value | big_value | big_value | quadruple | zero |
 * Each hit gives two samples.
 * @param main_data Buffer solely containing the main_data excluding the frame header and side info.
 * @param gr
 * @param ch
 */
inline void mp3::unpack_samples(unsigned char *main_data, int gr, int ch, int bit, int max_bit)
{
	int sample = 0;
	int table_num;
	const unsigned *table;

	/* Get the big value region boundaries. */
	int region0;
	int region1;
	if (window_switching[gr][ch] && block_type[gr][ch] == 2) {
		region0 = 36;
		region1 = 576;
	} else {
		region0 = band_index.long_win[region0_count[gr][ch] + 1];
		region1 = band_index.long_win[region0_count[gr][ch] + 1 + region1_count[gr][ch] + 1];
	}

	/* Get the samples in the big value region. Each entry in the Huffman tables
	 * yields two samples. */
	for (; sample < big_value[gr][ch] * 2; sample += 2) {
		if (sample < region0) {
			table_num = table_select[gr][ch][0];
			table = big_value_table[table_num];
		} else if (sample < region1) {
			table_num = table_select[gr][ch][1];
			table = big_value_table[table_num];
		} else {
			table_num = table_select[gr][ch][2];
			table = big_value_table[table_num];
		}

		if (table_num == 0) {
			samples[gr][ch][sample] = 0;
			continue;
		}

		bool repeat = true;
		unsigned bit_sample = get_bits(main_data, bit, bit + 32);

		/* Cycle through the Huffman table and find a matching bit pattern. */
		for (int row = 0; row < big_value_max[table_num] && repeat; row++)
			for (int col = 0; col < big_value_max[table_num]; col++) {
				int i = 2 * big_value_max[table_num] * row + 2 * col;
				unsigned value = table[i];
				unsigned size = table[i + 1];
				
				/* TODO Need to update tables so that we can simply write:
				 *      value == bit_sample >> (32 - size)  */
				if (value >> (32 - size) == bit_sample >> (32 - size)) {
					bit += size;

					int values[2] = {row, col};
					for (int i = 0; i < 2; i++) {

						/* linbits extends the sample's size if needed. */
						int linbit = 0;
						if (big_value_linbit[table_num] != 0 && values[i] == big_value_max[table_num] - 1)
							linbit = (int)get_bits_inc(main_data, &bit, big_value_linbit[table_num]);

						/* If the sample is negative or positive. */
						int sign = 1;
						if (values[i] > 0)
							sign = get_bits_inc(main_data, &bit, 1) ? -1 : 1;

						samples[gr][ch][sample + i] = (float)(sign * (values[i] + linbit));
					}

					repeat = false;
					break;
				}
			}
	}

	/* Quadruples region. */
	for (; bit < max_bit && sample + 4 < 576; sample += 4) {
		int values[4];

		/* Flip bits. */
		if (count1table_select[gr][ch] == 1) {
			unsigned bit_sample = !get_bits_inc(main_data, &bit, 4);
			values[0] = bit_sample & 0x08;
			values[1] = bit_sample & 0x04;
			values[2] = bit_sample & 0x02;
			values[1] = bit_sample & 0x01;
		} else {
			unsigned bit_sample = get_bits(main_data, bit, bit + 32);
			for (int entry = 0; entry < 16; entry++) {
				unsigned value = quad_table_1.hcod[entry];
				unsigned size = quad_table_1.hlen[entry];

				if (value >> (32 - size) == bit_sample >> (32 - size)) {
					bit += size;
					for (int i = 0; i < 4; i++)
						values[i] = (int)quad_table_1.value[entry][i];
				}
			}
		}

		/* Get the sign bit. */
		for (int i = 0; i < 4; i++)
			if (values[i] > 0 && get_bits_inc(main_data, &bit, 1) == 1)
				values[i] = -values[i];

		if (bit < max_bit)
			for (int i = 0; i < 4; i++)
				samples[gr][ch][sample + i] = values[i];
	}

	/* Fill remaining samples with zero. */
	for (; sample < 576; sample++)
		samples[gr][ch][sample] = 0;
}

/**
 * For short blocks there are a total of three windows. The original high frequency to
 * low frequency sample spectrum is divided into three segments, and is organized so
 * that there is a high to low, high to low, high to low pattern.
 * @param gr
 * @param ch
 */
inline void mp3::reorder(int gr, int ch)
{
	if (block_type[gr][ch] == 2) {
		char div_samples[192][3];

		for (int group = 0; group < 192; group++) {
			if (mixed_block_flag[gr][ch])
				group += 12;
			for (int sample = 0; sample < 3; sample++)
				div_samples[group][sample] = samples[gr][ch][3 * group + sample];
		}

		for (int sample = 0; sample < 3; sample++)
			for (int window = 0; window < 192; window++)
				samples[gr][ch][192 * sample + window] = div_samples[window][sample];
	}
}

/**
 * The left and right channels are added together to form the middle channel. The
 * difference between each channel is stored in the side channel.
 * @param gr
 */
inline void mp3::ms_stereo(int gr)
{
	for (int sample = 0; sample < 576; sample++) {
		int middle = samples[gr][0][sample];
		int side = samples[gr][1][sample];
		samples[gr][0][sample] = (middle - side) / SQRT2;
		samples[gr][1][sample] = (middle + side) / SQRT2;
	}
}

/**
 * The reduced samples are rescaled to their original scales and precisions.
 * @param gr
 * @param ch
 */
inline void mp3::requantize(int gr, int ch)
{
	float exp1, exp2;
	int window = 0;
	int sfb = 0;

	for (int sample = 0; sample < 576; sample++) {
		float scalefac_mult = (scalefac_scale[gr][ch] + 1.0) * 2.0;
		
		if (block_type[gr][ch] == 2 && window_switching[gr][ch]) {
			float scalefac;
			if (mixed_block_flag[gr][ch] == 1 && sfb < 8 && window == 0) {
				if (sample >= band_index.long_win[sfb])
					sfb++;
				scalefac = scalefac_l[gr][ch][sfb];
			} else {
				if (sample >= window * 192 + band_index.short_win[sfb]) {
					if (sfb == 12) {
						window++;
						sfb = 0;
					} else
						sfb++;
				}
				scalefac = scalefac_s[gr][ch][window][sfb];
			}

			exp1 = global_gain[gr][ch] - 210.0 - 8.0 * subblock_gain[gr][ch][window];
			exp2 = scalefac_mult * scalefac;
		} else {
			exp1 = global_gain[gr][ch] - 210.0;
			exp2 = scalefac_mult * scalefac_l[0][ch][sfb] + preflag[gr][ch] * pretab[sfb];

			if (sample >= band_index.long_win[sfb])
				sfb++;
		}

		float sign = samples[gr][ch][sample] < 0 ? -1.0f : 1.0f;
		float a = pow(abs(samples[gr][ch][sample]), 4.0 / 3.0);
		float b = pow(2.0, exp1 / 4.0);
		float c = pow(2.0, -exp2);

		samples[gr][ch][sample] = sign * a * b * c;
	}
}

/**
 * @param gr
 * @param ch
 */
inline void mp3::alias_reduction(int gr, int ch)
{
	static float cs[8] {
			.8574929257, .8817419973, .9496286491, .9833145925,
			.9955178161, .9991605582, .9998991952, .9999931551
	};
	static float ca[8] {
			-.5144957554, -.4717319686, -.3133774542, -.1819131996,
			-.0945741925, -.0409655829, -.0141985686, -.0036999747
	};

	float result[576];
	/* Not all samples are assigned values (i.e. 17 - 8 == 9 && 0 + 7 = 7). */
	for (int i = 0; i < 576; i++)
		result[i] = samples[gr][ch][i];

	int sb_max;
	if (window_switching[gr][ch] && block_type[gr][ch] == 2)
		sb_max = mixed_block_flag[gr][ch] ? 1 : 0;
	else
		sb_max = 32;

	for (int sb = 1; sb < sb_max; sb++)
		for (int sample = 0; sample < 8; sample++) {
			int offset1 = 18 * sb - sample - 1;
			int offset2 = 18 * sb + sample;
			float s1 = samples[gr][ch][offset1];
			float s2 = samples[gr][ch][offset2];
			result[offset1] = s1 * cs[sample] - s2 * ca[sample];
			result[offset2] = s2 * ca[sample] + s1 * cs[sample];
		}

	for (int i = 0; i < 576; i++)
		samples[gr][ch][i] = result[i];
}

/**
 * Inverted modified discrete cosine transformations (IMDCT) are applied to each
 * sample and are afterwards windowed to fit their window shape. As an addition, the
 * samples are overlapped.
 * @param gr
 * @param ch
 */
inline void mp3::imdct(int gr, int ch)
{
	static bool init = true;
	static float sine_block[4][36];
	static float prev_samples[18];
	float sample_block[36];

	if (init) {
		int i;
		for (i = 0; i < 36; i++)
			sine_block[0][i] = sin(PI / 36.0 * (i + 0.5));
		for (i = 0; i < 18; i++)
			sine_block[1][i] = sin(PI / 36.0 * (i + 0.5));
		for (; i < 24; i++)
			sine_block[1][i] = 1.0;
		for (; i < 30; i++)
			sine_block[1][i] = sin(PI / 12.0 * (i - 18.0 + 0.5));
		for (; i < 36; i++)
			sine_block[1][i] = 0.0;
		for (i = 0; i < 12; i++)
			sine_block[2][i] = sin(PI / 36.0 * (i + 0.5));
		for (i = 0; i < 6; i++)
			sine_block[3][i] = 0.0;
		for (; i < 12; i++)
			sine_block[3][i] = sin(PI / 12.0 * (i - 6.0 + 0.5));
		for (; i < 18; i++)
			sine_block[3][i] = 1.0;
		for (; i < 36; i++)
			sine_block[3][i] = sin(PI / 36.0 * (i + 0.5));
	}

	const int n = block_type[gr][ch] == 2 ? 12 : 36;
	int sample = 0;

	for (int block = 0; block < 32; block++) {
		for (int i = 0; i < n; i++) {
			float xi = 0.0;
			for (int k = 0; k < n/2; k++) {
				float s = samples[gr][ch][18 * block + k];
				xi += s * cos(PI / (2 * n) * (2 * i + 1 + n / 2) * (2 * k + 1));
			}
				
			/* Windowing samples. */
			sample_block[i] = xi * sine_block[block_type[gr][ch]][i];
		}

		if (block_type[gr][ch] == 2 && !(block == 0 && window_switching[gr][ch])) {
			float temp_block[36];
			memcpy(temp_block, sample_block, 36 * 4);

			/* Overlap samples. */
			for (int i = 0; i < 36; i++) {
				int j = 0;
				for (; j < 6; j++)
					sample_block[i] = 0;
				for (; j < 12; j++)
					sample_block[i] = temp_block[i - 6];
				for (; j < 18; j++)
					sample_block[i] = temp_block[i - 6]  + temp_block[i - 12];
				for (; j < 24; j++)
					sample_block[i] = temp_block[i - 12] + temp_block[i - 18];
				for (; j < 30; j++)
					sample_block[i] = temp_block[i - 18];
				for (; j < 36; j++)
					sample_block[i] = 0;
			}
		}

		for (int i = 0; i < 18; i++) {
			samples[gr][ch][sample + i] += sample_block[i] + prev_samples[i];
			prev_samples[i] = sample_block[18 + i];
		}
		sample += 18;
	}
}

/**
 * @param gr
 * @param ch
 */
inline void mp3::frequency_inversion(int gr, int ch)
{
	for (int sb = 1; sb < 32; sb += 2)
		for (int sample = 1; sample < 18; sample += 2)
			samples[gr][ch][18 * sb + sample] *= -1.0;
}

/**
 * @param gr
 * @param ch
 */
inline void mp3::synth_filterbank(int gr, int ch)
{
	static float fifo[2][576];
	static float lookup[64][32];
	static bool init = true;

	if (init) {
		init = false;
		for (int i = 0; i < 64; i++)
			for (int j = 0; j < 32; j++)
				lookup[i][j] = cos(PI / 64.0 * (2.0 * j + 1.0) * (16.0 + j));
	}

	float S[32], U[512], sum;
	float temp_samples[576];

	for (int sb = 0; sb < 18; sb++) {
		for (int i = 0; i < 32; i++)
			S[i] = samples[gr][ch][i * 18 + sb];

		for (int i = 1023; i > 63; i--)
			fifo[ch][i] = fifo[ch][i - 64];

		for (int i = 0; i < 64; i++) {
			fifo[ch][i] = 0.0;
			for (int j = 0; j < 32; j++)
				fifo[ch][i] += S[j] * lookup[i][j];
		}

		for (int i = 0; i < 8; i++)
			for (int j = 0; j < 32; j++) {
				U[i * 64 + j] = fifo[ch][i * 128 + j];
				U[i * 64 + j + 32] = fifo[ch][i * 128 + j + 96];
			}

		for (int i = 0; i < 512; i++)
			U[i] *= synth_window[i];

		for (int i = 0; i < 32; i++) {
			sum = 0;
			for (int j = 0; j < 16; j++)
				sum += U[j * 32 + i];
			temp_samples[32 * sb + i] = sum;
		}
	}

	memcpy(samples[gr][ch], temp_samples, 576 * 4);
}

float *mp3::get_samples()
{
	return samples[0][0];
}
