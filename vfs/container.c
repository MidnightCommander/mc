/* Virtual File System
   Copyright (C) 1995 The Free Software Foundation
   
   Written by: 1995 Ching Hui (mr854307@cs.nthu.edu.tw)
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../src/mad.h"
#include "../src/util.h"
#include "container.h"


struct linklist *
linklist_init(void)
{
    struct linklist *head;
    
    head = xmalloc(sizeof(struct linklist), "struct linklist");
    if (head) {
        head->prev = head->next = head;
	head->data = NULL;
    }
    return head;
}

void
linklist_destroy(struct linklist *head, void (*destructor) (void *))
{
    struct linklist *p, *q;

    for (p = head->next; p != head; p = q) {
	if (p->data && destructor)
	    (*destructor) (p->data);
	q = p->next;
	free(p);
    }
    free(head);
}

int 
linklist_insert(struct linklist *head, void *data)
{
    struct linklist *p;

    p = xmalloc(sizeof(struct linklist), "struct linklist");
    if (p == NULL)
        return 0;
    p->data = data;
    p->prev = head->prev;
    p->next = head;
    head->prev->next = p;
    head->prev = p;
    return 1;
}

void
linklist_delete_all(struct linklist *head, void (*destructor) (void *))
{
    struct linklist *p, *q;

    for (p = head->next; p != head; p = q) {
	destructor(p->data);
	q = p->next;
	free(p);
    }
    head->next = head->prev = head;
    head->data = NULL;
}

int
linklist_delete(struct linklist *head, void *data)
{
    struct linklist *h = head->next;

    while (h != head) {
	if (h->data == data) {
	    h->prev->next = h->next;
	    h->next->prev = h->prev;
	    free(h);
	    return 1;
	}
	h = h->next;
    }
    return 0;
}


