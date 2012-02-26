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

struct ListNode {
	struct ListNode *prev;
	struct ListNode *next;
	void *data;
};

struct List {
	struct ListNode *head;
	struct ListNode *tail;
	ListDataFreeFunc free;
	pthread_mutex_t mutex;
	size_t length;
};

ListPtr list_new(ListDataFreeFunc func) {
	ListPtr list = malloc(sizeof(struct List));
	if (list) {
		pthread_mutexattr_t mattr;
		pthread_mutexattr_init(&mattr);
		pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_ERRORCHECK);
		if (pthread_mutex_init(&list->mutex, &mattr) != 0) {
			free(list);
			list = NULL;
		}
		pthread_mutexattr_destroy(&mattr);
		list->head = NULL;
		list->tail = NULL;
		list->free = func;
		list->length = 0;
	}
	return list;
}

ListNodePtr list_enqueue(ListPtr list, void *data) {
	if (list) {
		struct ListNode *node = malloc(sizeof(struct ListNode));
		node->next = NULL;
		node->data = data;
		if (pthread_mutex_lock(&list->mutex)) {
			perror("Can't lock list mutex");
		}
		node->prev = list->tail;
		if (list->tail) {
			list->tail->next = node;
		}
		list->tail = node;
		if (!list->head) {
			list->head = node;
		}
		list->length++;
		if (pthread_mutex_unlock(&list->mutex)) {
			perror("Can't unlock list mutex");
		}
		return node;
	}
	return NULL;
}

void *list_dequeue(ListPtr list) {
	void *data = NULL;
	if (list) {
		struct ListNode *temp = NULL;
		list_lock(list);
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
		list_unlock(list);
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
				}
			}
		}
		if (pthread_mutex_destroy(&list->mutex)) {
			perror("Can't destroy list mutex");
		}
		free(list);
	}
}

size_t list_length(ListPtr list) {
	if (list) {
		return list->length;
	}
	return 0;
}

void *list_remove(ListPtr list, ListNodePtr node) {
	if (list && node) {
		list_lock(list);
		if (list->head == node) {
			list->head = node->next;
		}
		if (list->tail == node) {
			list->tail = node->prev;
		}
		if (node->next) {
			node->next->prev = node->prev;
		}
		if (node->prev) {
			node->prev->next = node->next;
		}
		void *data = node->data;
		free(node);
		list->length--;
		list_unlock(list);
		return data;
	}
	return NULL;
}

void *list_data(ListNodePtr node) {
	if (node) {
		return node->data;
	}
	return NULL;
}

ListNodePtr list_find(ListPtr list, ListFindNodeFunc func, void *arg) {
	if (list) {
		list_lock(list);
		ListNodePtr node = list->head;
		while (node && !func(node->data, arg)) {
			node = node->next;
		}
		list_unlock(list);
		return node;
	}
	return NULL;
}

void list_lock(ListPtr list) {
	if (list) {
		if (pthread_mutex_lock(&list->mutex)) {
			perror("Can't lock list mutex");
		}
	}
}

void list_unlock(ListPtr list) {
	if (list) {
		if (pthread_mutex_unlock(&list->mutex)) {
			perror("Can't unlock list mutex");
		}
	}
}

ListNodePtr list_first(ListPtr list) {
	if (list) {
		return list->head;
	}
	return NULL;
}

ListNodePtr list_next(ListNodePtr node) {
	if (node) {
		return node->next;
	}
	return NULL;
}

int list_equal(void *a, void *b) {
	return a == b;
}
