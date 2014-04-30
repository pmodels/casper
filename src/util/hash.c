#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <hash_table.h>

static int ht_hash(hashtable_t *hashtable, ht_key_t key) {
    return key % hashtable->size;
}

/* Create a key-value pair. */
static entry_t *ht_newpair(ht_key_t key, void *value) {
    entry_t *newpair;

    if ((newpair = malloc(sizeof(entry_t))) == NULL) {
        return NULL;
    }
    memset(newpair, 0, sizeof(newpair));

    newpair->key = key;
    newpair->value = value;
    newpair->next = NULL;

    return newpair;
}

/* Create a new hashtable. */
hashtable_t *ht_create(int size) {

    hashtable_t *hashtable = NULL;
    int i;

    if (size < 1)
        return NULL;

    /* Allocate the table itself. */
    if ((hashtable = malloc(sizeof(hashtable_t))) == NULL) {
        return NULL;
    }

    /* Allocate pointers to the head nodes. */
    if ((hashtable->table = malloc(sizeof(entry_t *) * size)) == NULL) {
        return NULL;
    }
    for (i = 0; i < size; i++) {
        hashtable->table[i] = NULL;
    }

    hashtable->size = size;

    return hashtable;
}

/* Insert a key-value pair into a hash table. */
int ht_set(hashtable_t *hashtable, ht_key_t key, void *value) {
    int bin = 0;
    entry_t *newpair = NULL;
    entry_t *next = NULL;
    entry_t *last = NULL;

    bin = ht_hash(hashtable, key);

    next = hashtable->table[bin];

    while (next != NULL && next->key != key) {
        last = next;
        next = next->next;
    }

    /* If key already exists, return error*/
    if (next != NULL && next->key == key) {
        goto fn_fail;

    } else {
        newpair = ht_newpair(key, value);
        if (newpair == NULL)
            goto fn_fail;

        /* At start of the linked list */
        if (next == hashtable->table[bin]) {
            newpair->next = next;
            hashtable->table[bin] = newpair;

            /* At the end of the linked list */
        } else if (next == NULL) {
            last->next = newpair;

            /* In the middle of the list */
        } else {
            newpair->next = next;
            last->next = newpair;
        }
    }
    return 0;

    fn_fail:
    return -1;
}

/* Retrieve a key-value pair from a hash table. */
void *ht_get(hashtable_t *hashtable, ht_key_t key) {
    int bin = 0;
    entry_t *pair;

    bin = ht_hash(hashtable, key);

    /* Step through the bin, looking for our value. */
    pair = hashtable->table[bin];
    while (pair != NULL && pair->key != key) {
        pair = pair->next;
    }

    /* Did we actually find anything? */
    if (pair == NULL || pair->key != key) {
        return NULL;

    } else {
        return pair->value;
    }
}

void ht_destroy(hashtable_t *hashtable) {
    int bin = 0;
    entry_t *next = NULL;
    entry_t *current = NULL;

    if (hashtable == NULL)
        return;

    if (hashtable->table != NULL) {
        for (bin = 0; bin < hashtable->size; bin++) {
            next = hashtable->table[bin];

            while (next != NULL) {
                current = next;
                next = next->next;

                free(current);
            }
        }

        free(hashtable->table);
    }
    free(hashtable);
}
