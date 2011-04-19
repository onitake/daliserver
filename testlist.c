#include <stdio.h>
#include <stdlib.h>
#include "list.h"

int main(int argc, char **argv) {
	ListPtr list = list_new(free);

	printf("Start: %ld\n", list_length(list));
	
	unsigned int i;
	for (i = 0; i < 10; i++) {
		list_enqueue(list, malloc(4));
	}

	printf("After adding 10 elements: %ld\n", list_length(list));

	for (i = 0; i < 5; i++) {
		free(list_dequeue(list));
	}
	
	printf("After cutting off 5 elements: %ld\n", list_length(list));

	ListNodePtr node = list_first(list);
	node = list_next(node);
	free(list_remove(list, node));
	node = list_first(list);
	node = list_next(node);
	node = list_next(node);
	free(list_remove(list, node));

	printf("After removing 2 elements: %ld\n", list_length(list));
	
	list_free(list);
	
	return 0;
}

