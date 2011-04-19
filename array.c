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

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "array.h"

struct Array {
	size_t elemsize;
	size_t allocated;
	size_t length;
	void *data;
};

ArrayPtr array_new(size_t elemsize) {
	struct Array *ret = malloc(sizeof(struct Array));
	if (ret) {
		ret->elemsize = elemsize;
		ret->allocated = 0;
		ret->length = 0;
		ret->data = NULL;
	}
	return ret;
}

void array_free(ArrayPtr array) {
	if (array) {
		if (array->data) {
			free(array->data);
		}
		free(array);
	}
}

ssize_t array_length(ArrayPtr array) {
	if (array) {
		return array->length;
	}
	return -1;
}

void *array_get(ArrayPtr array, size_t index) {
	if (array && index < array->length) {
		uint8_t *ptr = (uint8_t *) array->data;
		return &ptr[array->elemsize * index];
	}
	return NULL;
}

ssize_t array_append(ArrayPtr array, void *data) {
	if (array && data) {
		if (!array->data) {
			array->allocated = 1;
			array->data = malloc(array->elemsize * array->allocated);
		}
		if (array->length + 1 > array->allocated) {
			array->allocated <<= 1;
			array->data = realloc(array->data, array->elemsize * array->allocated);
		}
		if (array->data) {
			uint8_t *ptr = (uint8_t *) array->data;
			memcpy(&ptr[array->elemsize * array->length], data, array->elemsize);
			return array->length++;
		}
	}
	return -1;
}

void array_remove(ArrayPtr array, size_t index) {
	if (array && index < array->length) {
		uint8_t *ptr = (uint8_t *) array->data;
		memcpy(&ptr[array->elemsize * index], &ptr[array->elemsize * (index + 1)], array->elemsize * (array->length - index));
		if (--array->length < 1) {
			free(array->data);
			array->allocated = 0;
			array->data = NULL;
		}
	}
}

ssize_t array_memsize(ArrayPtr array) {
	if (array) {
		return array->length * array->elemsize;
	}
	return -1;
}

void *const array_storage(ArrayPtr array) {
	if (array) {
		return array->data;
	}
	return NULL;
}

