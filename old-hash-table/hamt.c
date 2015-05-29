#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "xxhash.h"


/** Hashtable constants */
#define MAP_SIZE 32
#define HASH_SIZE 64
#define INDEX_SIZE 5
#define INDEX_MAP 0x1F

/** Bitset manipulation functions */
#define BITS_POP(bitmap)  __builtin_popcount(bitmap)
#define BITS_SET(bitmap, loc, val) ((bitmap) = ((bitmap) & ~(1 << (loc))) | (val << (loc)))
#define BITS_GET(bitmap, loc) (((bitmap) & (1 << (loc))) ? true : false)
#define BITS_CNT(bitmap, off) (((off) == MAP_SIZE) ? 0 : BITS_POP(((bitmap) >> (off))))

/** Functions to store data in pointers */
#define AMT_CHECK_MSb(ptr) (((long long)ptr) &  0x1)
#define AMT_SET_MSb(ptr)   (((long long)ptr) |  0x1)
#define AMT_CLEAR_MSb(ptr) (((long long)ptr) & ~0x1)

/** Types used */
typedef uint64_t key_object;
typedef uintptr_t val_object;
typedef uint64_t hash_object;
typedef uint32_t map_object;

/** Structures used */
typedef struct ah_map ah_map;

typedef union {
    struct { // ah_key_value
        key_object key;
        val_object *val;
    };
    struct { // ah_map_array
        map_object map;
        void *node; // Have the compiler warn when it's used.
    };
} ah_entry;

struct ah_map {
    bool analysis_bit;
    ah_entry array[0];
};

typedef ah_entry ah_table;

/** Functions to trace all accesses to map */
typedef struct ah_access{
    ah_table *table;
    hash_object hash;
    unsigned offset;
    ah_map *map;
    struct ah_access *next;
} ah_access;

ah_access *trace_start = NULL;
ah_access *trace_end = NULL;

void mark_access(ah_table *tbl, ah_map *map, hash_object hash_val, unsigned offset) {
    if(trace_start == NULL) {
        trace_start = malloc(sizeof(ah_access));
        trace_end = trace_start;
    }

    trace_end->next = malloc(sizeof(ah_access));
    trace_end = trace_end->next;

    trace_end->table = tbl;
    trace_end->hash = hash_val;
    trace_end->offset = offset;
    trace_end->map = map;
    trace_end->next = NULL;
}

#ifdef TRACE_MAP
#define TRACE_ACCESS(t, m, h, o) mark_access((t), (m), (h), (o))
#else
#define TRACE_ACCESS(t, m, h, o)
#endif

/** Structural minipulation functions */
// Resolves a collision by converting a key/val to a map/ptr.
static inline void ah_transmute(ah_entry *, hash_object, unsigned, unsigned);

// Calculate the index for a value ah_map's array.
static inline unsigned ah_compressed_index(ah_entry *,  unsigned);

// Calculate the index for a value ah_map's map.
static inline unsigned ah_uncompressed_index(hash_object, unsigned);

// Inserts entry into existing map.
static inline void ah_insert(ah_entry *, key_object, val_object *, hash_object, unsigned);

// Check to see if map has index.
static inline bool ah_map_has_index(ah_entry *, unsigned);

// Progress map search values to next state.
static inline void ah_entry_next(key_object *, hash_object *, unsigned *, unsigned *);

// Check whether current entry is a map or key/val.
static inline bool ah_entry_is_map(ah_entry *);

static hash_object ah_hash(void *, size_t, int);

static ah_map *ah_get_map(ah_entry *current_entry) {
    return (ah_map *)AMT_CLEAR_MSb(current_entry->node);
}

static void *ah_set_map(ah_map *current_map) {
    return (void *)AMT_SET_MSb(current_map);
}

/** Public hash map functions */
ah_table *init_table(void) {
    ah_table *new_table = malloc(sizeof(ah_table));

    new_table->map = 0;
    new_table->node = ah_set_map(malloc(sizeof(ah_map)));

    ah_get_map(new_table)->analysis_bit = false;

    return new_table;
}

void insert(ah_table *table, key_object key, val_object *val) {
    hash_object current_hash = ah_hash(&key, sizeof(key_object), 0);
    unsigned current_off = 0;
    unsigned current_lvl = 0;

    ah_entry *current_entry = table;

    for(;;) {
        if(ah_entry_is_map(current_entry)) {
            // Trace all accesses to internal map nodes (the if is temporary).
            if(current_off < 64) TRACE_ACCESS(table, ah_get_map(current_entry), current_hash, current_off);
            
            unsigned index = ah_uncompressed_index(current_hash, current_off);
            
            // Descend to the next map.
            if(ah_map_has_index(current_entry, index)) {
                current_entry = &ah_get_map(current_entry)->array[ah_compressed_index(current_entry, index)];
                ah_entry_next(&key, &current_hash, &current_lvl, &current_off);
            
            // Add entry to existing array.           
            } else {
                ah_insert(current_entry, key, val, current_hash, current_off);
                break;
            }

        // Convert a conflicting value to a map and try to insert
        // new entry into it next iteration.
        } else {
            if(current_entry->key != key) 
                ah_transmute(current_entry, current_hash, current_lvl, current_off);
            else break;
        }
    }
}

val_object *search(ah_table *table, key_object key) {
    hash_object current_hash = ah_hash(&key, sizeof(key_object), 0);
    unsigned current_off = 0;
    unsigned current_lvl = 0;

    ah_entry *current_entry = table;

    for(;;) {
        if(ah_entry_is_map(current_entry)) {
            // Trace all accesses to internal map nodes (the if is temporary).
            if(current_off < 64) TRACE_ACCESS(table, ah_get_map(current_entry), current_hash, current_off);

            unsigned index = ah_uncompressed_index(current_hash, current_off);

            // Descend to the next map.
            if(ah_map_has_index(current_entry, index)) {
                current_entry = &ah_get_map(current_entry)->array[ah_compressed_index(current_entry, index)];
                ah_entry_next(&key, &current_hash, &current_lvl, &current_off);
            } else return NULL;

        } else return (current_entry->key == key) ? current_entry->val : NULL;
    }
}

// Finds the corresponding map of a location deteremined by hash + offset. 
// If a map doesn't exist at the location NULL is returned.
// Offset has to be: 0, INDEX_SIZE, 2*INDEX_SIZE,..., N*INDEX_SIZE <= HASH_SIZE.
ah_map *retrieve_map_for_hash_val(ah_table *table, hash_object hash_val, unsigned offset) {
    hash_object current_hash = hash_val;
    unsigned current_off = 0;
    unsigned current_lvl = 0;
    
    ah_entry *current_entry = table;

    while(current_off != offset) {
        if(ah_entry_is_map(current_entry)) {
            unsigned index = ah_uncompressed_index(current_hash, current_off);

            // Descend to the next map.
            if(ah_map_has_index(current_entry, index)) {
                current_entry = &ah_get_map(current_entry)->array[ah_compressed_index(current_entry, index)];
                
                current_lvl += 1;
                current_off += INDEX_SIZE;
            } else return NULL;
        } else return NULL;
    }

    return ah_get_map(current_entry);
}

//bool remove(ah_table *root, key_object key);

/** Internal management functions */
static inline bool ah_map_has_index(ah_entry *current_entry, unsigned uncompressed_index) {
    return BITS_GET(current_entry->map,uncompressed_index);
}

// Resolves a collision by converting a key/val to a map/ptr.
static inline void ah_transmute(ah_entry *entry, hash_object hash, unsigned lvl, unsigned off) {

    // Move the current entry to new map.
    ah_map *new_map = malloc(sizeof(ah_map) + sizeof(ah_entry));
    new_map->analysis_bit = false;

    memmove(&new_map->array[0], entry, sizeof(ah_entry));
    
    hash_object old_hash = ah_hash(&entry->key, sizeof(key_object), -1);
    
    // Clear and set entry values.
    entry->map = 0;
    BITS_SET(entry->map, ah_uncompressed_index(old_hash, off), 1);
    
    entry->node = ah_set_map(new_map);
}

// Inserts entry into existing map.
static inline void ah_insert(ah_entry *current_entry, 
                             key_object key,
                             val_object *val,
                             hash_object hash,
                             unsigned off) {
    unsigned array_size = BITS_CNT(current_entry->map, 0);
    unsigned uncompressed_index = ah_uncompressed_index(hash, off);
    unsigned compressed_index = ah_compressed_index(current_entry, uncompressed_index);

    // Allocate room for new entry.
    current_entry->node = ah_set_map(realloc(ah_get_map(current_entry), sizeof(ah_map) + sizeof(ah_entry)*(array_size+1)));
    ah_map *current_map = ah_get_map(current_entry);

    // Shift array elements past compressed_index over by one.
    if(array_size - compressed_index)
    	memmove(&current_map->array[compressed_index+1], &current_map->array[compressed_index], sizeof(ah_entry)*(array_size - compressed_index));

    // Insert the new entry.
    current_map->array[compressed_index] = (ah_entry){ key, val };

    // Set map entry bit in parent.
    BITS_SET(current_entry->map, uncompressed_index, 1);
}

// Calculate the index for a value ah_map's map.
static inline unsigned ah_uncompressed_index(hash_object hash, unsigned current_offset) {
    return (hash >> current_offset) & INDEX_MAP;
}

// Calculate the index for a value ah_map's array.
static inline unsigned ah_compressed_index(ah_entry *current_map, unsigned uncompressed_index) {
    return BITS_CNT(current_map->map, (uncompressed_index+1));
}

// Progress map search values to next state.
static inline void ah_entry_next(key_object *key, hash_object *hash, unsigned *lvl, unsigned *off) {
    *lvl += 1;
    *off += INDEX_SIZE;
    
    // Rehash if needed.
    if((*off + INDEX_SIZE) > HASH_SIZE) {
        *hash = ah_hash(key, sizeof(key_object), *lvl);
        *off = 0;
    }
}

// Check whether current entry is a map or key/val
static inline bool ah_entry_is_map(ah_entry *current_entry) {
    return AMT_CHECK_MSb(current_entry->node);
}

static hash_object ah_hash(void *key, size_t len, int level) {
    static int keep_lvl = 0;

    if (level != -1 && level != keep_lvl) keep_lvl = level;
    
    return XXH64(key, len, keep_lvl); 
}

unsigned long long counter[MAP_SIZE];
void count(ah_entry *current_entry) {

    // If entry is map, add its size to the counter array, 
    // and iterate through it's children
    if(ah_entry_is_map(current_entry)) {
        unsigned i, elements =  BITS_CNT(current_entry->map, 0);

        (counter[elements-1])++;
        
        // Iterate through all children
        for(i = 0; i < elements; i++)
            count(&ah_get_map(current_entry)->array[i]);
    }
}
