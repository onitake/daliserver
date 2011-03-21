/* Copyright (c) 2011, onitake <onitake@gmail.com>
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *    1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 * 
 *    2. Redistributions in binary form must reproduce the above copyright notice, this list
 *       of conditions and the following disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "pack.h"
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/param.h>
#include <sys/errno.h>

#if defined(__BYTE_ORDER)
# if __BYTE_ORDER == __LITTLE_ENDIAN
#  define SYSTEM_LITTLE 1
# elif __BYTE_ORDER == __BIG_ENDIAN
#  define SYSTEM_LITTLE 0
# endif
#elif defined(BYTE_ORDER)
# if BYTE_ORDER == LITTLE_ENDIAN
#  define SYSTEM_LITTLE 1
# elif BYTE_ORDER == BIG_ENDIAN
#  define SYSTEM_LITTLE 0
# endif
#endif /*__BYTE_ORDER*/

#if !defined(SYSTEM_LITTLE)
# error Unknown byte order!
#endif

enum {
	PACK_BYTE = 'c',
	PACK_UBYTE = 'C',
	PACK_SHORT = 's',
	PACK_USHORT = 'S',
	PACK_INT = 'i',
	PACK_UINT = 'I',
	PACK_LONG = 'l',
	PACK_ULONG = 'L',
	PACK_FLOAT = 'f',
	PACK_DOUBLE = 'd',
	PACK_SKIP = ' ',
	PACK_LITTLE = '<',
	PACK_BIG = '>',
	PACK_SYS = '=',
};

static size_t pack_length(char *format) {
	size_t length = 0;
	char *fmt;
	for (fmt = format; *fmt; fmt++) {
		switch (*fmt) {
			case PACK_BYTE:
			case PACK_UBYTE:
			case PACK_SKIP:
				length++;
				break;
			case PACK_SHORT:
			case PACK_USHORT:
				length += 2;
				break;
			case PACK_INT:
			case PACK_UINT:
			case PACK_FLOAT:
				length += 4;
				break;
			case PACK_LONG:
			case PACK_ULONG:
			case PACK_DOUBLE:
				length += 8;
				break;
		}
	}
	return length;
}

char *pack(char *format, char *data, size_t *size, ...) {
	size_t length = pack_length(format);
	if (data && (!size || (length > *size))) {
		errno = EINVAL;
		return NULL;
	}
	char *ret;
	if (!data) {
		ret = malloc(length);
		if (!ret) {
			errno = ENOMEM;
			return NULL;
		}
	} else {
		ret = data;
	}
	
	int little = SYSTEM_LITTLE;
	va_list args;
	va_start(args, size);
	
	char *out = ret;
	char *fmt;
	for (fmt = format; *fmt; fmt++) {
		switch (*fmt) {
			case PACK_SKIP: {
				out++;
			} break;
			case PACK_BYTE:
			case PACK_UBYTE: {
				unsigned int value = va_arg(args, unsigned int);
				out[0] = value & 0xff;
				out++;
			} break;
			case PACK_SHORT:
			case PACK_USHORT: {
				unsigned int value = va_arg(args, unsigned int);
				if (little) {
					out[0] = value & 0xff;
					out[1] = (value >> 8) & 0xff;
				} else {
					out[0] = (value >> 8) & 0xff;
					out[1] = value & 0xff;
				}
				out += 2;
			} break;
			case PACK_INT:
			case PACK_UINT: {
				uint32_t value = va_arg(args, uint32_t);
				if (little) {
					out[0] = value & 0xff;
					out[1] = (value >> 8) & 0xff;
					out[2] = (value >> 16) & 0xff;
					out[3] = (value >> 24) & 0xff;
				} else {
					out[0] = (value >> 24) & 0xff;
					out[1] = (value >> 16) & 0xff;
					out[2] = (value >> 8) & 0xff;
					out[3] = value & 0xff;
				}
				out += 4;
			} break;
			case PACK_FLOAT: {
				float value = (float) va_arg(args, double);
				uint32_t cast = *(uint32_t *) &value;
				if (little) {
					out[0] = cast & 0xff;
					out[1] = (cast >> 8) & 0xff;
					out[2] = (cast >> 16) & 0xff;
					out[3] = (cast >> 24) & 0xff;
				} else {
					out[0] = (cast >> 24) & 0xff;
					out[1] = (cast >> 16) & 0xff;
					out[2] = (cast >> 8) & 0xff;
					out[3] = cast & 0xff;
				}
				out += 4;
			} break;
			case PACK_LONG:
			case PACK_ULONG:
			case PACK_DOUBLE: {
				uint64_t value = va_arg(args, uint64_t);
				if (little) {
					out[0] = value & 0xff;
					out[1] = (value >> 8) & 0xff;
					out[2] = (value >> 16) & 0xff;
					out[3] = (value >> 24) & 0xff;
					out[4] = (value >> 32) & 0xff;
					out[5] = (value >> 40) & 0xff;
					out[6] = (value >> 48) & 0xff;
					out[7] = (value >> 56) & 0xff;
				} else {
					out[0] = (value >> 56) & 0xff;
					out[1] = (value >> 48) & 0xff;
					out[2] = (value >> 40) & 0xff;
					out[3] = (value >> 32) & 0xff;
					out[4] = (value >> 24) & 0xff;
					out[5] = (value >> 16) & 0xff;
					out[6] = (value >> 8) & 0xff;
					out[7] = value & 0xff;
				}
				out += 8;
			} break;
			case PACK_LITTLE: {
				little = 1;
			} break;
			case PACK_BIG: {
				little = 0;
			} break;
			case PACK_SYS: {
				little = SYSTEM_LITTLE;
			} break;
		}
	}
	
	va_end(args);
	if (size) {
		*size = length;
	}
	return ret;
}

int unpack(char *format, char *data, size_t *size, ...) {
	size_t length = pack_length(format);
	if (data && (!size || (length > *size))) {
		errno = EINVAL;
		return -1;
	}

	int little = SYSTEM_LITTLE;
	va_list args;
	va_start(args, size);
	
	unsigned char *in = (unsigned char *) data;
	char *fmt;
	for (fmt = format; *fmt; fmt++) {
		switch (*fmt) {
			case PACK_SKIP: {
				in++;
			} break;
			case PACK_BYTE: {
				int *value = va_arg(args, int *);
				*value = (int) in[0];
				in++;
			} break;
			case PACK_UBYTE: {
				unsigned int *value = va_arg(args, unsigned int *);
				*value = in[0];
				in++;
			} break;
			case PACK_SHORT: {
				uint16_t value;
				if (little) {
					value = in[0] | (in[1] << 8);
				} else {
					value = in[1] | (in[0] << 8);
				}
				int *out = va_arg(args, int *);
				*out = (int16_t) value;
				in += 2;
			} break;
			case PACK_USHORT: {
				uint16_t value;
				if (little) {
					value = in[0] | (in[1] << 8);
				} else {
					value = in[1] | (in[0] << 8);
				}
				unsigned int *out = va_arg(args, unsigned int *);
				*out = value;
				in += 2;
			} break;
			case PACK_INT: {
				uint32_t value;
				if (little) {
					value = in[0] | (in[1] << 8) | (in[2] << 16) | (in[3] << 24);
				} else {
					value = in[3] | (in[2] << 8) | (in[1] << 16) | (in[0] << 24);
				}
				int32_t *out = va_arg(args, int32_t *);
				*out = (int32_t) value;
				in += 4;
			} break;
			case PACK_UINT:
			case PACK_FLOAT: {
				uint32_t value;
				if (little) {
					value = in[0] | (in[1] << 8) | (in[2] << 16) | (in[3] << 24);
				} else {
					value = in[3] | (in[2] << 8) | (in[1] << 16) | (in[0] << 24);
				}
				uint32_t *out = va_arg(args, uint32_t *);
				*out = value;
				in += 4;
			} break;
			case PACK_LONG: {
				uint64_t value;
				if (little) {
					value = (uint64_t) in[0] | ((uint64_t) in[1] << 8) | ((uint64_t) in[2] << 16) | ((uint64_t) in[3] << 24) | ((uint64_t) in[4] << 32) | ((uint64_t) in[5] << 40) | ((uint64_t) in[6] << 48) | ((uint64_t) in[7] << 56);
				} else {
					value = (uint64_t) in[7] | ((uint64_t) in[6] << 8) | ((uint64_t) in[5] << 16) | ((uint64_t) in[4] << 24) | ((uint64_t) in[3] << 32) | ((uint64_t) in[2] << 40) | ((uint64_t) in[1] << 48) | ((uint64_t) in[0] << 56);
				}
				int64_t *out = va_arg(args, int64_t *);
				*out = (int64_t) value;
				in += 8;
			} break;
			case PACK_ULONG:
			case PACK_DOUBLE: {
				uint64_t value;
				if (little) {
					value = (uint64_t) in[0] | ((uint64_t) in[1] << 8) | ((uint64_t) in[2] << 16) | ((uint64_t) in[3] << 24) | ((uint64_t) in[4] << 32) | ((uint64_t) in[5] << 40) | ((uint64_t) in[6] << 48) | ((uint64_t) in[7] << 56);
				} else {
					value = (uint64_t) in[7] | ((uint64_t) in[6] << 8) | ((uint64_t) in[5] << 16) | ((uint64_t) in[4] << 24) | ((uint64_t) in[3] << 32) | ((uint64_t) in[2] << 40) | ((uint64_t) in[1] << 48) | ((uint64_t) in[0] << 56);
				}
				uint64_t *out = va_arg(args, uint64_t *);
				*out = value;
				in += 8;
			} break;
			case PACK_LITTLE: {
				little = 1;
			} break;
			case PACK_BIG: {
				little = 0;
			} break;
			case PACK_SYS: {
				little = SYSTEM_LITTLE;
			} break;
		}
	}
	
	va_end(args);
}
