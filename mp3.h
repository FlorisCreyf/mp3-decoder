/*
 * Author: Floris Creyf
 * Date: May 2015
 * Unpacks and decodes frames/headers.
 */

#ifndef MP3_H
#define MP3_H

#include <cmath>
#include <vector>
#include "tables.h"

class mp3 {
public:
	enum ChannelMode {
		Stereo = 0,
		JointStereo = 1,
		DualChannel = 2,
		Mono = 3
	};
	enum Emphasis {
		None = 0,
		MS5015 = 1,
		Reserved = 2,
		CCITJ17 = 3
	};

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
	ChannelMode channel_mode;
	int channels;
	unsigned mode_extension[2];
	Emphasis emphasis;
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
	ChannelMode get_channel_mode();
	unsigned *get_mode_extension();
	Emphasis get_emphasis();
	bool *get_info();

private: /* Frame */
	static const int num_prev_frames = 9;
	int prev_frame_size[9];
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

	int scalefac_l[2][2][22];
	int scalefac_s[2][2][3][13];

	float prev_samples[2][32][18];
	float fifo[2][1024];

	std::vector<unsigned char> main_data;
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
	unsigned get_header_size();
};

#endif	/* MP3_H */
