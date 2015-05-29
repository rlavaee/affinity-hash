#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "hash_table.h"

/** Bin Retrieval */
#if AH_STORE_HASH
#define BIN_IDX_FROM_ENTRY(table, entry) ((entry)->hash % (table)->num_bins)
#else
#define BIN_IDX_FROM_ENTRY(table, entry) BIN_IDX_FROM_KEY((table), (entry)->key)
#endif
#define BIN_IDX_FROM_HASH(table, hash)   (hash) % (table)->num_bins
#define BIN_IDX_FROM_KEY(table, key)     hash_func(&key, sizeof(key_type)) % (table)->num_bins

#define BIN_FROM_ENTRY(table, entry) table->bins[BIN_IDX_FROM_ENTRY(table, entry)]
#define BIN_FROM_HASH(table, hash)   table->bins[BIN_IDX_FROM_HASH(table, hash)  ]
#define BIN_FROM_KEY(table, key)     table->bins[BIN_IDX_FROM_KEY(table, key)    ]
#define BIN_FROM_IDX(table, idx)     table->bins[idx                             ]

/** Tracing defines */
#if AH_TRACE
#define AH_TRACE_ACCESS(tbl, ent) \
    trace_hash_access((tbl), ((ent) - (tbl)->entries), ((ent)->analysis_bit)) 
#define AH_REMOVE_FROM_TRACE(tbl, ent)
#endif


void ah_expand_table(ah_table *table);

// Credit: Jenkins, Bob (September 1997). "Hash functions". Dr. Dobbs Journal.
hash_type hash_func(void *key, size_t len) {
    unsigned hash, i;
    unsigned char *arr = key;

    for(hash = i = 0; i < len; ++i) {
        hash += arr[i];
        hash += (hash << 10);
        hash += (hash >> 6);
    }

    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);

    return hash;
}

/** Bin list functions */
static inline bool ah_bin_insert(ah_table *table, ah_entry *entry, hash_type hash) {
    ah_entry **bin = &BIN_FROM_HASH(table, hash);

    entry->next = *bin;
    *bin = entry;

#if AH_TRACE
    AH_TRACE_ACCESS(table, entry);
#endif
    
    return true;
}

static inline val_type *ah_bin_locate(ah_table *table, key_type key, hash_type hash) {
    ah_entry *entry = BIN_FROM_HASH(table, hash);

    while(entry != NULL && entry->key != key) {
#if AH_TRACE
        AH_TRACE_ACCESS(table, entry);
#endif
        entry = entry->next;
    }

    if(entry != NULL) {
#if AH_TRACE
        AH_TRACE_ACCESS(table, entry);
#endif
        return entry->val;
    } else return NULL;
}

static inline bool ah_bin_delete(ah_table *table, key_type key, hash_type hash) {
    ah_entry *entry = BIN_FROM_HASH(table, hash), *prev_entry = entry;

    // Find correct entry
    while(entry != NULL && entry->key != key) {
#if AH_TRACE
        AH_TRACE_ACCESS(table, entry);
#endif
        prev_entry = entry;
        entry = entry->next;
    }
    
    // Fail if key wasn't found
    if(entry == NULL) return false;

#if AH_TRACE
    AH_REMOVE_FROM_TRACE(table, entry);
#endif

    // Remove entry from bin list
    if(entry == prev_entry) {
        *&BIN_FROM_HASH(table, hash) = entry->next;
    } else {
        prev_entry->next = entry->next;
    }

    // Insert deleted entry into free list
    entry->next = table->free_list;
    table->free_list = entry;
    
    return true;
}

/** Hash table functions */
ah_table *new_table() {
    size_t i;
    ah_table *table = malloc(sizeof(ah_table));

    table->total_capacity = (size_t)ceil((1.0 + AH_LOAD_FACTOR) * AH_INITIAL_SIZE);
    table->bins     = malloc(AH_INITIAL_SIZE * sizeof(ah_entry*));
    table->entries  = malloc(table->total_capacity * sizeof(ah_entry));

    if (table->entries == NULL || table->bins == NULL) {
        printf("Oh no, you couldn't malloc!  Try harder!\n");
        exit(-1);
    }

    table->num_entries = 0;
    table->last_entry  = 0;
    table->num_bins    = AH_INITIAL_SIZE;
    table->free_list   = NULL;

    // Initialize the new bins.
    for (i = 0; i < table->num_bins; i++)
        table->bins[i] = NULL;

#if AH_TRACE
     init_affinity_analysis();
#endif

    return table;
}


bool ah_insert(ah_table *table, key_type key, val_type* val) {
    hash_type hashed_key = hash_func(&key, sizeof(key_type));

    // (unsigned)ceil((1 + AH_LOAD_FACTOR) * table->num_bins)) {
    //if(((float)table->num_bins / (float)table->num_entries) < AH_LOAD_FACTOR)
    //     ah_expand_table(table);
  
    // If the load factor goes above AH_LOAD_FACTOR resize table
    if (table->free_list == NULL && table->last_entry == table->total_capacity) {
        ah_expand_table(table);
    }

    ah_entry *entry;
    if(table->free_list != NULL) {
        entry = table->free_list;
        table->free_list = table->free_list->next;
    } else {
        entry = &table->entries[table->last_entry];
        table->last_entry++;
    }

#if AH_STORE_HASH
    entry->hash = hashed_key;
#endif
    entry->key = key;
    entry->val = val;

    table->num_entries++;

    return ah_bin_insert(table, entry, hashed_key);
}

val_type *ah_search(ah_table *table, key_type key) {
    hash_type hashed_key = hash_func(&key, sizeof(key_type));

    return ah_bin_locate(table, key, hashed_key);
}

bool ah_delete(ah_table *table, key_type key) {
    hash_type hashed_key = hash_func(&key, sizeof(key_type));

    bool ret_val = ah_bin_delete(table, key, hashed_key);

    if(ret_val) table->num_entries--;

    return ret_val;
}

/** Table management functions */
void free_table(ah_table *table) {
#if AH_TRACE
    remove_table_analysis(table);
#endif
    
    free(table->entries);
    free(table->bins);
    free(table);
}

void ah_expand_table(ah_table *table) {
    // Assuming num_entries >= num_bins.
    size_t new_size = AH_EXPAND_FACTOR * table->num_bins;
    size_t i;

    // Prepare to iterate all old bins.
    size_t old_bin_num = table->num_bins, old_free_list;
    ah_entry *old_entries = table->entries;
    ah_entry **old_bins = table->bins;

    if (table->free_list != NULL) old_free_list = table->free_list - old_entries;

    // Not realloc'ing bins since we need the old bins to locate entries.
    table->bins           = malloc(new_size * sizeof(ah_entry*));
    table->num_bins       = new_size;
    table->total_capacity = (size_t)ceil((1.0 + AH_LOAD_FACTOR) * new_size);
    table->entries        = realloc(table->entries, table->total_capacity * sizeof(ah_entry));
   
    if (table->free_list != NULL) table->free_list = &table->entries[old_free_list];

    if (table->entries == NULL || table->bins == NULL) {
        printf("Space allocation for expanding failed\n");
        exit(-1);
    }

    // Initialize the new bins.
    for (i = 0; i < new_size; i++)
        table->bins[i] = NULL;

    ah_entry  *current_entry, *temp_entry;

    // Reinsert all entries into new bin list.
    for (i=0; i < old_bin_num; i++) {
        if(old_bins[i] == NULL) continue;
        current_entry = &table->entries[old_bins[i] - old_entries];

        // Iterate through each bin and move entries to new array.
        while(current_entry != NULL) {
            temp_entry = (current_entry->next != NULL) ? &table->entries[current_entry->next - old_entries]
                                                       : NULL;
#if AH_STORE_HASH
            ah_bin_insert(table, current_entry, current_entry->hash);
#else
            ah_bin_insert(table, current_entry, hash_func(&current_entry->key, sizeof(key_type)));
#endif
            current_entry = temp_entry;
        }
    }
    
    // Free the old bins.
    free(old_bins);
    
    // Reorder data elements
    printf("Finished expanding! Bins: %lu Total Capacity: %lu\n", table->num_bins, table->total_capacity);
}


int main() {
    printf("TEST\n");
    long a = 3;

    ah_table *table = new_table();

    long i;

    for(i = 1; i < 40000000; i++)
        ah_insert(table, i, &a);

    for(i = 1; i < 40000000; i++) {
        long *b = ah_search(table, i);
        if(!b) printf("Idx: %d Ele: %lu\n", i, 0); 
    }

    printf("table ptr: %llx\n", table);

    free_table(table);
}
