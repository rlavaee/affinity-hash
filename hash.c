#include <stdlib.h>
#include <assert.h>
#include <stdint.h> // so that SIZE_MAX is available (in 2's complement it's fffffffff)

#define INITIAL_SIZE 8 // size will be same as num_bins
#define EXPAND_FACTOR 2
typedef long object_t;
typedef size_t offset_t; // 11111... is impossible offset

/* Bin Retrieval */
#define BIN_IDX_FROM_ENTRYP(tablep, entryp) (entryp)->hash % (tablep)->num_bins
#define BIN_IDX_FROM_HASH(tablep, hash)     (hash) % (tablep)->num_bins
#define BIN_IDX_FROM_KEY(tablep, key)       hash_func(key) % (tablep)->num_bins

#define BIN_FROM_ENTRYP(tablep, entryp) tablep->binspo[BIN_IDX_FROM_ENTRYP(tablep, entryp)]
#define BIN_FROM_HASH(tablep, hash)     tablep->binspo[BIN_IDX_FROM_HASH(tablep, hash)    ]
#define BIN_FROM_KEY(tablep, key)       tablep->binspo[BIN_IDX_FROM_KEY(tablep, key)      ]
#define BIN_FROM_IDX(tablep, idx)       tablep->binspo[idx                                ]

/* Address Calculation from Offset */
#define ADDRESS_OF(addr, offst) ((addr) + (offst))

struct st_entry {
  long hash;
  object_t key;
  object_t *valuep;
  offset_t nexto; // Singly linked list for bin
};

struct st_table {
  size_t num_bins;
  size_t num_entries;
  struct st_entry *entriesp;
  offset_t *binspo;
};

struct st_table* new_table() {
  struct st_table *tablep = malloc(sizeof(struct st_table));
  size_t i;

  tablep->entriesp = malloc(INITIAL_SIZE * sizeof(struct st_entry));
  tablep->binspo   = malloc(INITIAL_SIZE * sizeof(offset_t));

  if (!(tablep->entriesp && tablep->binspo)) {
    printf("Oh no, you couldn't malloc!  Try harder!\n");
    exit(-1);
  }

  tablep->num_entries = 0;
  tablep->num_bins = INITIAL_SIZE;

  // Initialize the new bins.
  for (i = 0; i < tablep->num_bins; i++) {
    tablep->binspo[i] = SIZE_MAX;
  }

  return tablep;
}

long hash_func(object_t key) {
  return key;
}

/** List Management */
void bin_list_insert(struct st_table *tablep, struct st_entry *entryp) {}
void bin_list_remove(struct st_table *tablep, struct st_entry *entryp) {}

/** Table Management */
void free_table(struct st_table *tablep) {
  free(tablep->entriesp);
  free(tablep->binspo);
  free(tablep);
}

void expand_table(struct st_table *tablep) {}

object_t *get(struct st_table *tablep, object_t key) {}
void set(struct st_table *tablep, object_t key, object_t *valuep) {}

void del(struct st_table *tablep, object_t key) {}

struct st_entry* affinity_rehash(long *hashesp, size_t len) {}






