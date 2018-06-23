/*
 * Author: Floris Creyf
 * Date: May 2015
 * A simplistic MPEG-1 layer 3 decoder.
 */

#include <fstream>
#include <stdio.h>
#include <alsa/asoundlib.h> /* dnf install alsa-lib-devel */ /* apt install libasound2-dev */
#include <vector>
#include "id3.h"
#include "mp3.h"
#include "xing.h"

#define ALSA_PCM_NEW_HW_PARAMS_API

/**
 * Start decoding the MP3 and let ALSA hand the PCM stream over to a driver.
 * @param mp3_decode
 * @param buffer A buffer containing the MP3 bit stream.
 * @param offset An offset to a MP3 frame header.
 */
inline void stream(mp3 &decoder, std::vector<unsigned char> &buffer, unsigned offset)
{
	unsigned sampeling_rate = decoder.get_sampling_rate();
	unsigned channels = decoder.get_channel_mode() == mp3::Mono ? 1 : 2;
	snd_pcm_t *handle;
	snd_pcm_hw_params_t *hw = NULL;
	snd_pcm_uframes_t frames = 128;

	if (snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0)
		exit(1);

	snd_pcm_hw_params_alloca(&hw);
	snd_pcm_hw_params_any(handle, hw);

	if (snd_pcm_hw_params_set_access(handle, hw, SND_PCM_ACCESS_RW_INTERLEAVED) < 0)
		exit(1);
	if (snd_pcm_hw_params_set_format(handle, hw, SND_PCM_FORMAT_FLOAT_LE) < 0)
		exit(1);
	if (snd_pcm_hw_params_set_channels(handle, hw, channels) < 0)
		exit(1);
	if (snd_pcm_hw_params_set_rate_near(handle, hw, &sampeling_rate, NULL) < 0)
		exit(1);
	if (snd_pcm_hw_params_set_period_size_near(handle, hw, &frames, NULL) < 0)
		exit(1);
	if (snd_pcm_hw_params(handle, hw) < 0)
		exit(1);
	if (snd_pcm_hw_params_get_period_size(hw, &frames, NULL) < 0)
		exit(1);
	if (snd_pcm_hw_params_get_period_time(hw, &sampeling_rate, NULL) < 0)
		exit(1);

	/* Start decoding. */
	while (decoder.is_valid() && buffer.size() > offset + decoder.get_header_size()) {
		decoder.init_header_params(&buffer[offset]);
		if (decoder.is_valid()) {
			decoder.init_frame_params(&buffer[offset]);
			offset += decoder.get_frame_size();
		}

		int e = snd_pcm_writei(handle, decoder.get_samples(), 1152);
		if (e == -EPIPE)
			snd_pcm_recover(handle, e, 0);
	}

	snd_pcm_drain(handle);
	snd_pcm_close(handle);
}

std::vector<unsigned char> get_file(const char *dir)
{
	std::ifstream file(dir, std::ios::in | std::ios::binary | std::ios::ate);
	std::vector<unsigned char> buffer(file.tellg());
	file.seekg(0, std::ios::beg);
	file.read((char *)buffer.data(), buffer.size());
	file.close();
	return std::move(buffer);
}

std::vector<id3> get_id3_tags(std::vector<unsigned char> &buffer, unsigned &offset)
{
	std::vector<id3> tags;
	int i = 0;
	bool valid = true;

	while (valid) {
		id3 tag(&buffer[offset]);
		if (valid = tag.is_valid()) {
			tags.push_back(tag);
			offset += tags[i++].get_id3_offset() + 10;
		}
	}

	return tags;
}

int main(int argc, char **argv)
{
	if (argc > 2) {
		printf("Unexpected number of arguments.\n");
		return -1;
	} else if (argc == 1) {
		printf("No directory specified.\n");
		return -1;
	}

	try {
		std::vector<unsigned char> buffer = get_file(argv[1]);
		unsigned offset = 0;
		std::vector<id3> tags = get_id3_tags(buffer, offset);
		mp3 decoder(&buffer[offset]);
		stream(decoder, buffer, offset);
	} catch (std::bad_alloc) {
		printf("File does not exist.\n");
		return -1;
	}

	return 0;
}
