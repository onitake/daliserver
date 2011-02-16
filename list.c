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
typedef struct Node Node;

struct List {
	struct Node *head;
	struct Node *tail;
	ListDataFreeFunc free;
	pthread_mutex_t mutex;
};

List *list_new(ListDataFreeFunc func) {
	List *list = malloc(sizeof(List));
	if (list) {
		list->head = NULL;
		list->tail = NULL;
		list->free = func;
		if (pthread_mutex_init(&list->mutex, NULL) != 0) {
			free(list);
			list = NULL;
		}
	}
	return list;
}

void list_append(List *list, void *data) {
	if (list) {
		Node *node = malloc(sizeof(Node));
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
		pthread_mutex_unlock(&list->mutex);
	}
}

void *list_remove(List *list) {
	void *data = NULL;
	if (list) {
		Node *temp = NULL;
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
		}
		pthread_mutex_unlock(&list->mutex);
		free(temp);
	}
	return data;
}

void list_free(List *list) {
	if (list) {
		while (list->head) {
			void *data = list_remove(list);
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
