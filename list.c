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

#include "list.h"
#include <stdlib.h>
#include <pthread.h>

struct Node {
	struct Node *prev;
	struct Node *next;
	void *data;
};

struct List {
	struct Node *head;
	struct Node *tail;
	ListDataFreeFunc free;
	pthread_mutex_t mutex;
	size_t length;
};

ListPtr list_new(ListDataFreeFunc func) {
	ListPtr list = malloc(sizeof(struct List));
	if (list) {
		if (pthread_mutex_init(&list->mutex, NULL) != 0) {
			free(list);
			list = NULL;
		}
		list->head = NULL;
		list->tail = NULL;
		list->free = func;
		list->length = 0;
	}
	return list;
}

void list_enqueue(ListPtr list, void *data) {
	if (list) {
		struct Node *node = malloc(sizeof(struct Node));
		node->next = NULL;
		node->data = data;
		pthread_mutex_lock(&list->mutex);
		node->prev = list->tail;
		if (list->tail) {
			list->tail->next = node;
		}
		list->tail = node;
		if (!list->head) {
			list->head = node;
		}
		list->length++;
		pthread_mutex_unlock(&list->mutex);
	}
}

void *list_dequeue(ListPtr list) {
	void *data = NULL;
	if (list) {
		struct Node *temp = NULL;
		pthread_mutex_lock(&list->mutex);
		if (list->head) {
			data = list->head->data;
			if (list->head->next) {
				list->head->next->prev = NULL;
			}
			temp = list->head;
			list->head = list->head->next;
			if (list->tail == temp) {
				list->tail = NULL;
			}
			list->length--;
		}
		pthread_mutex_unlock(&list->mutex);
		free(temp);
	}
	return data;
}

void list_free(ListPtr list) {
	if (list) {
		while (list->head) {
			void *data = list_dequeue(list);
			if (data) {
				if (list->free) {
					list->free(data);
				} else {
					free(data);
				}
			}
		}
		pthread_mutex_destroy(&list->mutex);
		free(list);
	}
}

size_t list_length(ListPtr list) {
	if (list) {
		return list->length;
	}
	return 0;
}
