/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2015 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */
#ifndef SLIST_H_INCLUDED
#define SLIST_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>

typedef enum {
    CSPG_SLIST_ORDER_NONE,
    CSPG_SLIST_ORDER_ASC,
    CSPG_SLIST_ORDER_DESC
} CSP_slist_order_t;

/* User specified function comparing the value of two user buffers.
 * It is used when inserting a new element into the sorted list.
 * It returns the difference between ubuf1 and ubuf2 as follows:
 *  0: ubuf1 is equal to ubuf2;
 * >0: ubuf1 is larger than ubuf2;
 * <0: ubuf1 is smaller than ubuf2.*/
typedef int (*CSP_slist_cmp_fnc_t) (void *ubuf1, void *ubuf2);

typedef struct CSP_slist_elem {
    void *ubuf;
    struct CSP_slist_elem *next;
} CSP_slist_elem_t;

typedef struct CSP_slist {
    CSP_slist_elem_t *head;
    CSP_slist_elem_t *tail;
    int count;
    CSP_slist_order_t order;
    CSP_slist_cmp_fnc_t cmp_fnc;
} CSP_slist_t;

static inline int CSP_slist_count(CSP_slist_t * list)
{
    return list->count;
}

extern void CSP_slist_init(CSP_slist_order_t order, CSP_slist_cmp_fnc_t cmp_fnc,
                           CSP_slist_t * slist);
extern void CSP_slist_destroy(CSP_slist_t * slist);
extern void *CSP_slist_dequeue(CSP_slist_t * slist);
extern void *CSP_slist_remove(void *search_ubuf, CSP_slist_t * slist);
extern int CSP_slist_insert(void *buf, CSP_slist_t * slist);

#endif /* SLIST_H_INCLUDED */
