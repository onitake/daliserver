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

#include <sys/types.h>

struct List;
typedef struct List *ListPtr;
struct ListNode;
typedef struct ListNode *ListNodePtr;
typedef void (*ListDataFreeFunc)(void *);
typedef int (*ListFindNodeFunc)(void *, void *);

ListPtr list_new(ListDataFreeFunc func);
void list_free(ListPtr list);
size_t list_length(ListPtr list);
ListNodePtr list_enqueue(ListPtr list, void *data);
void *list_dequeue(ListPtr list);
void *list_remove(ListPtr list, ListNodePtr node);
void *list_data(ListNodePtr node);
ListNodePtr list_find(ListPtr list, ListFindNodeFunc func, void *arg);
void list_lock(ListPtr list);
void list_unlock(ListPtr list);
ListNodePtr list_first(ListPtr list);
ListNodePtr list_next(ListNodePtr node);

#endif /*_LIST_H*/
