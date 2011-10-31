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

#ifndef _LIST_H
#define _LIST_H

#include <stddef.h>

struct List;
typedef struct List *ListPtr;
struct ListNode;
typedef struct ListNode *ListNodePtr;

typedef void (*ListDataFreeFunc)(void *);
typedef int (*ListFindNodeFunc)(void *, void *);

// Creates a new empty double-linked list
// func is a type-specific destructor for the data to be put into the list
// Pass NULL if you don't need destruction when the list is free'd
ListPtr list_new(ListDataFreeFunc func);
// Destroys the list and all its contents (if a data destructor was passed in)
void list_free(ListPtr list);
// Returns the number of entries contained in the list
size_t list_length(ListPtr list);
// Appends a new entry to the beginning of the list
ListNodePtr list_enqueue(ListPtr list, void *data);
// Removes the last entry from the list and returns its data pointer
void *list_dequeue(ListPtr list);
// Removes an element from the list and returns its data pointer
void *list_remove(ListPtr list, ListNodePtr node);
// Gets the data pointer from a list element
void *list_data(ListNodePtr node);
// Finds the first element in the list that matches arg using matching function func
ListNodePtr list_find(ListPtr list, ListFindNodeFunc func, void *arg);
// Locks the list mutex
void list_lock(ListPtr list);
// Unlocks the list mutex
void list_unlock(ListPtr list);
// Returns the first element in the list
ListNodePtr list_first(ListPtr list);
// Returns the element following node in the list
ListNodePtr list_next(ListNodePtr node);

// Convenience compare function that checks if the data pointers point to the same location
int list_equal(void *, void *);

#endif /*_LIST_H*/
