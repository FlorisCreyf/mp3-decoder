/*
 * Author: Floris Creyf
 * Date: May 2015
 * ID3 contains meta data irrelevant to the decoder. The header contains an
 * offset used to determine the location of the first MP3 header.
 * | Header | Additional header (optional) | Meta Data | Footer (optional) |
 */

#include <string>
#include <vector>
#include <stdio.h>
#include <iostream>

using std::cout;
using std::endl;
using std::string;
using std::vector;

#ifndef ID3_H
#define ID3_H

#define ID3_FLAG_FOOTER_PRESENT         0
#define ID3_FLAG_EXPERIMENTAL_INDICATOR 1
#define ID3_FLAG_EXTENDED_HEADER        2
#define ID3_FLAG_UNSYNCHRONISATION      3

class id3 {
private:
	unsigned char *buffer;
	unsigned int start;
	string version;
	unsigned int offset;
	bool id3_flags[4];
	unsigned int extended_header_size;
	vector<string> id3_frames[2];

	int char_to_int(unsigned char *buffer);

	void set_version(unsigned char version, unsigned char revision);
	void set_flags(unsigned char flags);
	void set_offset(int offset);
	void set_extended_header_size(int size);
	void set_fields(unsigned char *buffer);

public:
	id3(unsigned char *buffer);
	id3(const id3& orig);
	virtual ~id3();

	static id3 get_all();

	string get_id3_version();
	bool *get_id3_flags();
	int get_id3_offset();
	int get_id3_extended_header_size();
	vector<string> *get_id3_fields();
	unsigned int get_id3_fields_length();
};

#endif	/* ID3_H */
