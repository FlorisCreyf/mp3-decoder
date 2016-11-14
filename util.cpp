/*
 * Author: Floris Creyf
 * Date: May 2015
 */

#include "util.h"

unsigned get_bits(unsigned char *buffer, int start_bit, int end_bit)
{
	/* Decrement end_bit so that i < end_bit. */
	end_bit--;
	
	int start_byte = 0;
	int end_byte = 0;
	
	while (start_bit >= 8) {
		end_bit -= 8;
		start_bit -= 8;
		start_byte++;
		end_byte++;
	}
	while (end_bit >= 8) {
		end_bit -= 8;
		end_byte++;
	}
	
	/* Get the bits. */
	unsigned result = ((unsigned)buffer[start_byte] << (32 - (8 - start_bit))) >> (32 - (8 - start_bit));	
	
	if (start_byte != end_byte) {
		while (++start_byte != end_byte) {
			result <<= 8;
			result += buffer[start_byte];
		}
		result <<= end_bit + 1;
		result += buffer[end_byte] >> (7 - end_bit);
	} else if (end_bit != 7)
		result >>= (7 - end_bit);
	
	return result;
}

unsigned get_bits_inc(unsigned char *buffer, int *offset, int count)
{
	unsigned result = get_bits(buffer, *offset, *offset + count);
	*offset += count;
	return result;
}

int char_to_int(unsigned char *buffer)
{
	unsigned num = 0x00;
	for (int i = 0; i < 4; i++)
		num = (num << 7) + buffer[i];
	return num;
}

