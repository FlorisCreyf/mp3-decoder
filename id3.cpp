/*
 * Author: Floris Creyf
 * Date: May 2015
 * ID3 contains meta data irrelevant to the decoder. The header contains an
 * offset used to determine the location of the first MP3 header.
 * | Header | Additional header (optional) | Meta Data | Footer (optional) |
 */

#include <regex>
#include "id3.h"

id3::id3(unsigned char *buffer)
{
	this->buffer = buffer;
	
	if (buffer[0] == 'I' && buffer[1] == 'D' && buffer[2] == '3') {
		set_version(buffer[3], buffer[4]);
		set_flags(buffer[5]);
		set_offset(char_to_int(&buffer[6]));
		set_extended_header_size(char_to_int(&buffer[10]));
		set_fields(&buffer[10 + extended_header_size]);
	}
}

id3::id3(const id3& orig)
{
	this->start = orig.start;
	this->version = orig.version;
	this->offset = orig.offset;
	for (int i = 0; i < 4; i++)
		this->id3_flags[i] = orig.id3_flags[i];
	this->extended_header_size = orig.extended_header_size;
	for (int i = 0; i < 2; i++)
		this->id3_frames[i] = orig.id3_frames[i];
}

id3::~id3()
{
}

int id3::char_to_int(unsigned char *buffer)
{
	unsigned offset = 0x00;
	for (int i = 0; i < start + 4; i++)
		offset = (offset << 7) + buffer[i];
	return offset;
}

void id3::set_version(unsigned char version, unsigned char revision)
{
	char v[6];
	sprintf(v, "2.%u.%u", version, revision);
	this->version = (string)v;
}

string id3::get_id3_version()
{
	return this->version;
}

void id3::set_offset(int offset)
{
	this->offset = offset;
}

int id3::get_id3_offset()
{
	return this->offset;
}

void id3::set_flags(unsigned char flags)
{
	/* These flags must be unset for the ID3 header to be valid. */
	for (int bit_num = 0; bit_num < 4; bit_num++)
		if (flags >> bit_num & 1)
			delete this;

	/* The position of each flag is associated with a FLAG_XXX macro. */
	for (int bit_num = 4; bit_num < 8; bit_num++) {
		if (flags >> bit_num & 1)
			this->id3_flags[bit_num - 4] = true;
		else
			this->id3_flags[bit_num - 4] = false;
	}
}

bool *id3::get_id3_flags()
{
	return this->id3_flags;
}

void id3::set_extended_header_size(int size)
{
	if (this->id3_flags[ID3_FLAG_EXTENDED_HEADER] == 1)
		this->extended_header_size = size;
	else
		this->extended_header_size = 0;
}

int id3::get_id3_extended_header_size()
{
	return this->extended_header_size;
}

void id3::set_fields(unsigned char *buffer)
{
	int footer_size = id3_flags[ID3_FLAG_FOOTER_PRESENT] * 10;
	int size = offset - extended_header_size - footer_size;
	int i = 0;

	std::regex re("[A-Z0-9]");

	while (!std::regex_match((string){(char)buffer[i]}, re) && i < size) {
		string id = "";
		string content = "";
		int field_size = 0;

		for (int j = i; j < i + 4; j++)
			id += buffer[j];
		this->id3_frames[0].push_back(id);

		i += 4;
		field_size = char_to_int(&buffer[i]);

		i += 6;
		for (int j = i; j < field_size + i; j++)
			content += buffer[j];
		this->id3_frames[1].push_back(content);

		i += field_size;
	}
}

vector<string> *id3::get_id3_fields()
{
	return this->id3_frames;
}

unsigned int id3::get_id3_fields_length()
{
	return this->id3_frames[1].size();
}
