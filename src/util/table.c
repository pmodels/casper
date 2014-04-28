#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include "asp.h"

#ifdef TABLE_DEBUG
#define debug_printf(str, ...) do{ \
    fprintf(stdout, "[ASP][N-%d]"str, MPIASP_MY_NODE_ID, ## __VA_ARGS__); fflush(stdout); \
    } while(0)
#else
#define debug_printf(str...) {}
#endif

#define error_printf(str, ...) do{ \
    fprintf(stderr, "[ASP][N-%d]"str, MPIASP_MY_NODE_ID, ## __VA_ARGS__); fflush(stdout); \
    } while(0)

static void _free_key(gpointer key) {
    free(key);
}

static int cmd_table_max = 1;
static GHashTable **intern_tables = NULL;

int table_init() {
    int i = 0;

    intern_tables = calloc(cmd_table_max, sizeof(GHashTable *));
    if (intern_tables == NULL) {
        error_printf("allocate for tables failed\n");
        return -1;
    }

    /**
     * Free keys automatically, but don't free values
     */
    for (i = 0; i < cmd_table_max; i++) {
        intern_tables[i] = g_hash_table_new_full(g_int64_hash, g_int64_equal,
                _free_key, NULL);
        if (intern_tables[i] == NULL)
            goto err;
    }

    return 0;

    err:

    for (i = 0; i < cmd_table_max; i++) {
        if (intern_tables[i] != NULL)
            g_hash_table_destroy(intern_tables[i]);
    }
    free(intern_tables);

    return -1;
}

void table_destroy() {
    int i;

    if (intern_tables == NULL)
        return;

    /**
     * Keys are destroyed by registered free function
     */
    for (i = 0; i < cmd_table_max; i++) {
        if (intern_tables[i] != NULL)
            g_hash_table_destroy(intern_tables[i]);

        intern_tables[i] = NULL;
    }

    free(intern_tables);
}

int table_insert_with_key(int type, void *obj, unsigned long long key) {
    unsigned long long *newkey;

    if (type < 0 || type >= cmd_table_max) {
        error_printf("wrong table type %d\n", type);
        return 0;
    }
    if (intern_tables == NULL) {
        error_printf("uninitialized main tables\n");
        return 0;
    }
    if (intern_tables[type] == NULL) {
        error_printf("uninitialized table for type %d\n", type);
        return 0;
    }

    if (g_hash_table_lookup(intern_tables[type], &key) == NULL) {
        newkey = malloc(sizeof(unsigned long long));
        *newkey = key;

        debug_printf("new key %llx for table[%d]\n", *newkey, type);
        g_hash_table_insert(intern_tables[type], newkey, obj);
    } else {
        error_printf("key %llx already exists in table[%d]\n", key, type);
        return 1;
    }

    return 0;
}

unsigned long long table_insert(int type, void *obj) {
    unsigned long long *newkey;
    unsigned long long key;

    if (type < 0 || type >= cmd_table_max) {
        error_printf("wrong table type %d\n", type);
        return 0;
    }
    if (intern_tables == NULL) {
        error_printf("uninitialized main tables\n");
        return 0;
    }
    if (intern_tables[type] == NULL) {
        error_printf("uninitialized table for type %d\n", type);
        return 0;
    }

    key = (unsigned long long) obj;

    if (g_hash_table_lookup(intern_tables[type], &key) == NULL) {
        newkey = malloc(sizeof(unsigned long long));
        *newkey = key;

        debug_printf("new key %llx for table[%d]\n", *newkey, type);
        g_hash_table_insert(intern_tables[type], newkey, obj);
    } else {
        debug_printf("key %llx already exists in table[%d]\n", key, type);
    }

    return key;
}

void *table_find(int type, unsigned long long key) {
    if (type < 0 || type >= cmd_table_max) {
        error_printf("wrong table type %d\n", type);
        return NULL;
    }
    if (intern_tables == NULL) {
        error_printf("uninitialized main tables\n");
        return NULL;
    }
    if (intern_tables[type] == NULL) {
        error_printf("uninitialized table for type %d\n", type);
        return NULL;
    }

    return g_hash_table_lookup(intern_tables[type], &key);
}

int table_remove(int type, unsigned long long key) {
    if (type < 0 || type >= cmd_table_max) {
        error_printf("wrong table type %d\n", type);
        return -1;
    }
    if (intern_tables == NULL) {
        error_printf("uninitialized main tables\n");
        return -1;
    }
    if (intern_tables[type] == NULL) {
        error_printf("uninitialized table for type %d\n", type);
        return -1;
    }

    /**
     * glib return TRUE: 1 | FALSE :0
     * this return TRUE: 0 | FALSE :-1
     */
    return (int) g_hash_table_remove(intern_tables[type], &key) == 0 ? -1 : 0;
}

int table_remove_all(int type, int _destroy_func(void *obj)) {
    int ret = 0;
    GHashTableIter iter;
    gpointer key, value;

    if (type < 0 || type >= cmd_table_max) {
        error_printf("wrong table type %d\n", type);
        return -1;
    }
    if (intern_tables == NULL) {
        error_printf("uninitialized main tables\n");
        return -1;
    }
    if (intern_tables[type] == NULL) {
        error_printf("uninitialized table for type %d\n", type);
        return -1;
    }

    if (_destroy_func != NULL) {
        g_hash_table_iter_init(&iter, intern_tables[type]);
        while (g_hash_table_iter_next(&iter, &key, &value)) {
            ret |= _destroy_func(value);
        }
    }

    g_hash_table_remove_all(intern_tables[type]);
    return ret;
}
