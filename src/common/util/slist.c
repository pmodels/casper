/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2015 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "csp_util.h"
#include "slist.h"

/* Initialize a sorted list with user specified order and compare function.
 * Elements will be inserted in the specified order, and always removed from the
 * head of list.
 * - ASC: elements are stored from small to large, and removed from the smallest one.
 * - DESC: elements are stored from large to small, and removed from the largest one.
 * - NONE: elements are stored at the inserting order, and removed from the head
 *         (equal to FIFO). */
void CSP_slist_init(CSP_slist_order_t order, CSP_slist_cmp_fnc_t cmp_fnc, CSP_slist_t * slist)
{
    slist->head = slist->tail = NULL;
    slist->count = 0;
    slist->order = order;
    slist->cmp_fnc = cmp_fnc;
}

/* Remove one element from the head of sorted list.
 * Returns the user buffer of the removed element, and the element object is freed.
 * - ASC: the smallest element is removed.
 * - DESC: the largest element is removed.
 * - NONE: the first inserted element is removed (FIFO). */
void *CSP_slist_dequeue(CSP_slist_t * slist)
{
    CSP_slist_elem_t *elem = NULL;
    void *ubuf = NULL;

    /* empty */
    if (slist->head == NULL)
        return NULL;

    elem = slist->head;
    slist->head = elem->next;
    slist->count--;

    /* removing the last one */
    if (elem == slist->tail) {
        slist->tail = NULL;
        CSP_ASSERT(slist->head == NULL);
        CSP_ASSERT(slist->count == 0);
    }

    ubuf = elem->ubuf;
    free(elem);

    return ubuf;
}

/* Remove the element containing the specified object (search_ubuf) from the sorted list.
 * Returns the user buffer of the removed element, and the element object is freed. */
void *CSP_slist_remove(void *search_ubuf, CSP_slist_t * slist)
{
    CSP_slist_elem_t *e = slist->head, *pe = NULL;
    void *ubuf = NULL;

    while (e) {
        if (slist->cmp_fnc(e->ubuf, search_ubuf) == 0)
            break;
        pe = e;
        e = e->next;
    }

    if (e) {
        if (pe) {
            pe->next = e->next;
        }
        if (e == slist->head) {
            slist->head = e->next;
            CSP_ASSERT(pe == NULL);
        }
        if (e == slist->tail) {
            slist->tail = pe;
        }

        slist->count--;
        ubuf = e->ubuf;
        free(e);
    }

    return ubuf;
}

/* Destroy a sorted list.
 * All existing elements will be freed. However, the ubuf object of each
 * element must still be freed by caller. */
void CSP_slist_destroy(CSP_slist_t * slist)
{

    while (CSP_slist_count(slist) > 0)
        CSP_slist_dequeue(slist);

    slist->head = slist->tail = NULL;
    slist->count = 0;
    slist->order = CSPG_SLIST_ORDER_NONE;
    slist->cmp_fnc = NULL;
}

/* Insert one element into the sorted list.
 * Returns 0 if succeed, otherwise -1.
 * - ASC: inserted in front of the first larger element.
 * - DESC: inserted in front of the first smaller element.
 * - NONE: inserted at the end of list.
 * Note that, for elements that have the same value, they are always stored in
 * FIFO order. */
int CSP_slist_insert(void *ubuf, CSP_slist_t * slist)
{
    CSP_slist_elem_t *e = slist->head, *pe = NULL, *new_e = NULL;

    switch (slist->order) {
    case CSPG_SLIST_ORDER_ASC:
        /* insert in ascending order */
        while (e) {
            /* find the first element larger than new one */
            if (slist->cmp_fnc(e->ubuf, ubuf) > 0)
                break;
            pe = e;
            e = e->next;
        }
        break;
    case CSPG_SLIST_ORDER_DESC:
        /* insert in descending order */
        while (e) {
            /* find the first element smaller than new one */
            if (slist->cmp_fnc(e->ubuf, ubuf) < 0)
                break;
            pe = e;
            e = e->next;
        }
        break;
    case CSPG_SLIST_ORDER_NONE:
        /* insert at end */
        pe = slist->tail;
        e = NULL;
        break;
    }

    /* create new element */
    new_e = CSP_calloc(1, sizeof(CSP_slist_elem_t));
    if (new_e == NULL)
        return -1;

    new_e->ubuf = ubuf;
    new_e->next = NULL;

    /* insert at head */
    if (pe == NULL) {
        CSP_ASSERT(e == slist->head);
        slist->head = new_e;
        new_e->next = e;
    }
    /* insert after previous element */
    else {
        pe->next = new_e;
        new_e->next = e;
    }

    /* inserted at the end, update tail */
    if (e == NULL) {
        slist->tail = new_e;
    }

    slist->count++;
    return 0;
}
