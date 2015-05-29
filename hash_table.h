#ifndef AH_HASH_TABLE_H
#define AH_HASH_TABLE_H

#include <stdbool.h>
#include <stddef.h>

/** Global defines */
#define AH_LOAD_FACTOR 0.15
#define AH_EXPAND_FACTOR 15
#define AH_INITIAL_SIZE  3
#define AH_STORE_HASH true
#define AH_TRACE true

/** Types used */
typedef long key_type;
typedef long val_type;
typedef unsigned hash_type;

typedef struct ah_entry {
#if AH_STORE_HASH
    hash_type hash;
#endif
#if AH_TRACE
    bool analysis_bit;
#endif
    key_type  key;
    val_type *val;
    struct ah_entry *next;
} ah_entry;

typedef struct ah_table {
    size_t num_bins;
    size_t num_entries; // NOT USED
    size_t total_capacity;

    ah_entry *entries;
    ah_entry **bins;

    ah_entry *free_list;
    size_t    last_entry;
} ah_table;

typedef struct ah_reorder_list {
    size_t size;
    size_t *order;
} ah_reorder_list;

#if AH_TRACE
//#include "hash_tracing/HashTracing.hpp"
void ah_set_analysis_bit(ah_table *table, ptrdiff_t entry_index, bool val) {
    table->entries[entry_index].analysis_bit = val;
}
#endif

ah_table *new_table();
bool ah_insert(ah_table *table, key_type key, val_type* val);
bool ah_delete(ah_table *table, key_type key);
val_type *ah_search(ah_table *table, key_type key);

#endif
