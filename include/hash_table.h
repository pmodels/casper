/*
 * hash_table.h
 *  <FILE_DESC>
 * 	
 *  Author: Min Si
 */

#ifndef HASH_TABLE_H_
#define HASH_TABLE_H_

typedef unsigned long ht_key_t;

struct entry_s {
    ht_key_t key;
    void *value;
    struct entry_s *next;
};

typedef struct entry_s entry_t;

struct hashtable_s {
    int size;
    struct entry_s **table;
};

typedef struct hashtable_s hashtable_t;

extern hashtable_t *ht_create(int size);
extern int ht_set(hashtable_t * hashtable, ht_key_t key, void *value);
extern void *ht_get(hashtable_t * hashtable, ht_key_t key);
extern void ht_destroy(hashtable_t * hashtable);

#endif /* HASH_TABLE_H_ */
