/*
 * The Xing header acts as an index to the MP3 file. It's contained within the first
 * MP3 frame. (Optional.)
 * | ID | Flags | Number of frames (optional) | Bytes in file (optional) |
 * | TOC (optional) | Quality (optional) |
 */

#ifndef XING_H
#define XING_H

#define XING_EXT_FRAME_FIELD 0
#define XING_EXT_BYTE_FIELD  1
#define XING_EXT_TOC         2
#define XING_EXT_QUALITY     3

class xing {
private:
	unsigned int start;
	unsigned char field_num = 0;

	/* Values:
	 * 0 -> Frame field.
	 * 1 -> Byte field.
	 * 2 -> TOC (table of contents).
	 * 3 -> Quality indicator. */
	bool xing_extensions[4];
	int byte_quantity;
	int frame_quantity;
	unsigned char quality;
	bool test[4];
	/* char *TOC; */

	void set_xing_extensions(unsigned char *buffer, unsigned int offset);
	void set_byte_quantity(unsigned char *buffer);
	void set_frame_quantity(unsigned char *buffer);
	void set_quality(unsigned char *buffer);

public:
	xing(unsigned char *buffer, unsigned int offset);
	xing(const xing &orig);

	bool *get_xing_extensions();
	int get_byte_quantity();
	int get_frame_quantity();

	/** A rating of the Xing quality ranging from 0 (best) to 100 (worst). */
	unsigned char get_quality();
};

#endif	/* XING_H */


