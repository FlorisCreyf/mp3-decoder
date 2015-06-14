/*
 * Author: Floris Creyf
 * Date: May 2015
 * A simplistic MPEG-1 layer 3 decoder.
 */

#include <fstream>
#include <stdio.h>
#include <iostream>
#include <alsa/asoundlib.h> /* yum install alsa-lib-devel */
#include <vector>
#include "id3.h"
#include "mp3.h"
#include "xing.h"

#define ALSA_PCM_NEW_HW_PARAMS_API

using std::cout;
using std::endl;
using std::vector;
using std::ios;
using std::ifstream;

/**
 * Start decoding the MP3 and let ALSA hand the PCM stream over to a driver.
 * @param mp3_decode
 * @param buffer A buffer containing the MP3 bit stream.
 * @param offset An offset to the a MP3 frame header.
 */
inline void stream(mp3 &mp3_decode, unsigned char *buffer, unsigned offset)
{
	unsigned sampeling_rate = mp3_decode.get_sampling_rate();
	unsigned channels = mp3_decode.get_channel_mode() == 3 ? 1 : 2;
	snd_pcm_t *handle;
	snd_pcm_hw_params_t *hw = NULL;
	snd_pcm_uframes_t frames = 128;

	if (snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
		exit(1);
	}

	snd_pcm_hw_params_alloca(&hw);
	snd_pcm_hw_params_any(handle, hw);

	if (snd_pcm_hw_params_set_access(handle, hw, SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
		exit(1);
	}
	if (snd_pcm_hw_params_set_format(handle, hw, SND_PCM_FORMAT_FLOAT_LE) < 0) {
		exit(1);
	}
	if (snd_pcm_hw_params_set_channels(handle, hw, channels) < 0) {
		exit(1);
	}
	if (snd_pcm_hw_params_set_rate_near(handle, hw, &sampeling_rate, NULL) < 0) {
		exit(1);
	}
	if (snd_pcm_hw_params_set_period_size_near(handle, hw, &frames, NULL) < 0) {
		exit(1);
	}
	if (snd_pcm_hw_params(handle, hw) < 0) {
		exit(1);
	}
	if (snd_pcm_hw_params_get_period_size(hw, &frames, NULL) < 0) {
		exit(1);
	}
	if (snd_pcm_hw_params_get_period_time(hw, &sampeling_rate, NULL) < 0) {
		exit(1);
	}

	/* Start decoding. */
	while (mp3_decode.is_valid()) {
		mp3_decode.init_header_params(&buffer[offset]);
		if (mp3_decode.is_valid()) {
			mp3_decode.init_frame_params(&buffer[offset]);
			offset += mp3_decode.get_frame_size();
		}

		int e = snd_pcm_writei(handle, mp3_decode.get_samples(), 1152);
		if (e == -EPIPE) {
			snd_pcm_recover(handle, e, 0);
		}
	}

	snd_pcm_drain(handle);
	snd_pcm_close(handle);
}

unsigned char *get_file(const char *dir)
{
	ifstream file(dir, ios::in | ios::binary);
	file.seekg(0, file.end);
	int len = file.tellg();
	file.seekg(0, file.beg);
	char *buffer = new char[len];
	file.read(buffer, len);
	file.close();
	return reinterpret_cast<unsigned char *>(buffer);
}

int main(int argc, char **argv)
{
	/*if (argc > 2) {
		printf("Unexpected number of arguments.\n");
		return -1;
	} else if (argc == 1) {
		printf("No directory specified.\n");
		return -1;
	}*/
	
	try {
		unsigned char *buffer = get_file("/home/FlorisCreyf/NetBeansProjects/MP3Decoder/tests/h.mp3");

		unsigned offset = 0;
		int id3_num = 0;
		vector<id3> meta;
		
		while (buffer[offset] == 'I' && buffer[offset + 1] == 'D' && buffer[offset + 2] == '3') {
			id3 _meta(&buffer[offset]);
			meta.push_back(_meta);
			offset += meta[id3_num].get_id3_offset() + 10;
			id3_num++;
		}

		mp3 mp3_decode(&buffer[offset]);
		stream(mp3_decode, buffer, offset);

		delete buffer;
	} catch (std::bad_alloc) {
		printf("File does not exist.\n");
		return -1;
	}

	return 0;
}
