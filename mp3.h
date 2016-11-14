/*
 * Author: Floris Creyf
 * Date: May 2015
 * Unpacks and decodes frames/headers.
 */

#include <math.h>
#include "tables.h"

#ifndef MP3_H
#define MP3_H

#define MP3_INFO_PRIVATE   0
#define MP3_INFO_COPYRIGHT 1
#define MP3_INFO_ORIGINAL  2

class mp3 {
public:
	mp3(unsigned char *buffer);
 	void init_header_params(unsigned char *buffer);
	void init_frame_params(unsigned char *buffer);
	
private:
	unsigned char *buffer;
	bool valid;
	
public:
	bool is_valid();
	
private: /* Header */
	float mpeg_version;
	unsigned layer;
	bool crc;
	unsigned bit_rate;
	unsigned sampling_rate;
	bool padding;
	unsigned channel_mode;
	bool mono;
	unsigned mode_extension[2];
	unsigned emphasis;
	bool info[3];
	struct {
		const unsigned *long_win;
		const unsigned *short_win;
	} band_index;
	struct {
		const unsigned *long_win;
		const unsigned *short_win;
	} band_width;
	
	void set_mpeg_version();
	void set_layer(unsigned char byte);
	void set_crc();
	void set_bit_rate(unsigned char *buffer);
	void set_sampling_rate();
	void set_padding();
	void set_channel_mode(unsigned char *buffer);
	void set_mode_extension(unsigned char *buffer);
	void set_emphasis(unsigned char *buffer);
	void set_info();
	void set_tables();
	
public:
	float get_mpeg_version();
	unsigned get_layer();
	bool get_crc();
	unsigned get_bit_rate();
	unsigned get_sampling_rate();
	bool get_padding();
	
	/**
	 * Values:
	 * 0 -> Stereo
	 * 1 -> Joint stereo (this option requires use of mode_extension)
	 * 2 -> Dual channel
	 * 3 -> Single channel
	 */
	unsigned get_channel_mode();
	unsigned *get_mode_extension();
	
	/**
	 * Values:
	 * 0 -> None
	 * 1 -> 50/15 MS
	 * 2 -> Reserved
	 * 3 -> CCIT J.17
	 */
	unsigned get_emphasis();
	bool *get_info();
	
private: /* Frame */
	int prev_frame_size;
	int frame_size;
	
	int main_data_begin;
	bool scfsi[2][4];
	
	/* Allocate space for two granules and two channels. */
	int part2_3_length[2][2];
	int part2_length[2][2];
	int big_value[2][2];
	int global_gain[2][2];
	int scalefac_compress[2][2];
	int slen1[2][2];
	int slen2[2][2];
	bool window_switching[2][2];
	int block_type[2][2];
	bool mixed_block_flag[2][2];
	int switch_point_l[2][2];
	int switch_point_s[2][2];
	int table_select[2][2][3];
	int subblock_gain[2][2][3];
	int region0_count[2][2];
	int region1_count[2][2];
	int preflag[2][2];
	int scalefac_scale[2][2];
	int count1table_select[2][2];
	
	int scalefactor[2][2][39];
	int scalefac_l[2][2][22];
	int scalefac_s[2][2][3][13];
	
	float samples[2][2][576];
	float pcm[576 * 4];
	
	void set_frame_size();
	void set_side_info(unsigned char *buffer);
	void set_main_data(unsigned char *buffer);
	void unpack_scalefac(unsigned char *bit_stream, int gr, int ch, int &bit);
	void unpack_samples(unsigned char *bit_stream, int gr, int ch, int bit, int max_bit);
	void requantize(int gr, int ch);
	void ms_stereo(int gr);
	void reorder(int gr, int ch);
	void alias_reduction(int gr, int ch);
	void imdct(int gr, int ch);
	void frequency_inversion(int gr, int ch);
	void synth_filterbank(int gr, int ch);
	void interleave();
	
public:
	float *get_samples();
	unsigned get_frame_size();
};

#endif	/* MP3_H */

