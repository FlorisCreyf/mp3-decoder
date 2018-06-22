/*
 * The Xing header acts as an index to the MP3 file. It's contained within the first
 * MP3 frame. (Optional.)
 * | ID | Flags | Number of frames (optional) | Bytes in file (optional) |
 * | TOC (optional) | Quality (optional) |
 */

#include "xing.h"
#include "util.h"
#include <string>

xing::xing(unsigned char *buffer, unsigned int offset)
{
	std::string id;

	/* The position of the Xing header within the first MP3 frame is unknown. */
	while (true) {
		if (buffer[offset] == 'I' || buffer[offset] == 'X') {
			for (int byte = 0; byte < 4; byte++)
				id += buffer[offset + byte];

			if (id == "Info" || id == "Xing") {
				this->start = offset + 4;
				set_xing_extensions(buffer, offset);

				if (this->xing_extensions[1]) set_frame_quantity(buffer);
				if (this->xing_extensions[0]) set_byte_quantity(buffer);
				/* if (this->xing_extensions[3]) TODO: TOC */
				if (this->xing_extensions[2]) set_quality(buffer);
				break;
			}
		} else if (buffer[offset] == 0xFF && buffer[offset+1] >= 0xE0)
			break;

		offset++;
	}
}

xing::xing(const xing &orig)
{
	this->byte_quantity = orig.byte_quantity;
	this->frame_quantity = orig.frame_quantity;
	this->quality = orig.quality;
	for (int byte = 0; byte < 4; byte++)
		this->xing_extensions[byte] = orig.xing_extensions[byte];
}

void xing::set_xing_extensions(unsigned char* buffer, unsigned int offset)
{
	unsigned char flag_byte = buffer[this->start + 3];

	for (int bit_num = 0; bit_num < 4; bit_num++)
		if (flag_byte >> bit_num & 1)
			this->xing_extensions[bit_num] = true;
		else
			this->xing_extensions[bit_num] = false;

	this->start += 4;
}

const bool *xing::get_xing_extensions()
{
	return this->xing_extensions;
}

void xing::set_frame_quantity(unsigned char *buffer)
{
	this->frame_quantity = char_to_int(&buffer[this->start + this->field_num*4]);
	this->field_num++;
}

int xing::get_frame_quantity()
{
	return this->frame_quantity;
}

void xing::set_byte_quantity(unsigned char *buffer)
{
	this->byte_quantity = char_to_int(&buffer[this->start + this->field_num*4]);
	this->field_num++;
}

int xing::get_byte_quantity()
{
	return byte_quantity;
}

void xing::set_quality(unsigned char *buffer)
{
	unsigned char size = (this->xing_extensions[TOC] == true ? 100 : 0);
	this->quality = buffer[this->start + size + this->field_num*4 + 3];
}

unsigned char xing::get_quality()
{
	return this->quality;
}
