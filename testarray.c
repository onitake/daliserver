#include <stdio.h>
#include <stdlib.h>
#include "array.h"
#include "util.h"

int main(int argc, char **argv) {
	size_t size = 4;
	
	ArrayPtr array = array_new(size);

	printf("Length: %ld Memsize: %ld Storage: %p\n", array_length(array), array_memsize(array), array_storage(array));
	
	unsigned int i;
	for (i = 0; i < 10; i++) {
		unsigned char data[] = { i, i, i, i };
		array_append(array, data);
	}

	printf("Length: %ld Memsize: %ld Storage: %p\n", array_length(array), array_memsize(array), array_storage(array));
	for (i = 0; i < array_length(array); i++) {
		hexdump((uint8_t *) array_get(array, i), size);
	}
	
	for (i = 0; i < 5; i++) {
		array_remove(array, 0);
	}
	
	printf("Length: %ld Memsize: %ld Storage: %p\n", array_length(array), array_memsize(array), array_storage(array));
	for (i = 0; i < array_length(array); i++) {
		hexdump((uint8_t *) array_get(array, i), size);
	}

	array_free(array);
	
	return 0;
}

