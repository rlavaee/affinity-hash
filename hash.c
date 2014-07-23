#include <stdlib.h>

#define INITIAL_SIZE 8 // size will be same as num_bins
#define EXPAND_FACTOR 2
typedef long object;

/* Bin Retrieval */
#define BIN_IDX_FROM_ENTRYP(tablep, entryp) (entryp)->hash % (tablep)->num_bins
#define BIN_IDX_FROM_HASH(tablep, hash)     (hash) % (tablep)->num_bins
#define BIN_IDX_FROM_KEY(tablep, key)       hash_func(key) % (tablep)->num_bins

#define BIN_FROM_ENTRYP(tablep, entryp) tablep->binspp[BIN_IDX_FROM_ENTRYP(tablep, entryp)]
#define BIN_FROM_HASH(tablep, hash)     tablep->binspp[BIN_IDX_FROM_HASH(tablep, hash)    ]
#define BIN_FROM_KEY(tablep, key)       tablep->binspp[BIN_IDX_FROM_KEY(tablep, key)      ]
#define BIN_FROM_IDX(tablep, idx)       tablep->binspp[idx                        ]

/* List Management */
// Need to include edge cases.
#define ADD_TO_TABLE_LIST(tablep, entryp) do { \
  if (tablep->headp == NULL) {                 \
    tablep->headp = entryp;                    \
    entryp->backp = entryp->forep = NULL;      \
  } else {                                     \
    (tablep)->tailp->forep = (entryp);         \
    (entryp)->backp = (tablep)->tailp;         \
    (tablep)->tailp = (entryp);                \
  }                                            \
} while(0)

/* Removes from list, but leaves data there. */
#define DEL_FROM_TABLE_LIST(tablep, entryp) do {                            \
  if ((entryp)->back != NULL) { (entryp)->back->fore = (entryp)->fore; } \
  if ((entryp)->fore != NULL) { (entryp)->fore->back = (entryp)->back; } \
} while(0)

/* Memory must be zeroed PRIOR to calling this! */
#define ADD_TO_BIN_LIST(tablep, entryp) do {         \
  (entryp)->nextp = BIN_FROM_ENTRYP(tablep, entryp); \
  BIN_FROM_ENTRYP(tablep, entryp) = entryp;          \
} while(0)

#define DEL_FROM_BIN_LIST(tablep, bin_idx, entryp) do {   \
  /* Do Something. */ ; \
} while(0)

struct st_entry {
  long hash;
  object key;
  object *valuep;
  struct st_entry *nextp; // Singly linked list for bin
  struct st_entry *backp; // Doubly linked list for table
  struct st_entry *forep; // Doubly linked list for table
};

struct st_table {
  size_t num_bins;
  size_t num_entries;

  struct st_entry *entriesp;
  struct st_entry **binspp;
  struct st_entry *headp; // Doubly linked list for table
  struct st_entry *tailp; // Doubly linked list for table
};

struct st_table * new_table() {
  struct st_table *tablep = malloc(sizeof(struct st_table));

  tablep->entriesp = malloc(INITIAL_SIZE * sizeof(struct st_entry));
  tablep->binspp = calloc(INITIAL_SIZE, sizeof(struct st_entry*));

  tablep->headp = tablep->tailp = NULL;
  return tablep;
}

long hash_func(object key) {
  return key;
}

/* Expand bins array, expand entries allocation, and rehash. */
void expand_table(struct st_table *tablep){
  int i;
  struct st_entry *entryp;
  struct st_entry *old_entriesp, *new_entriesp;
  int entriesp_difference;
  size_t old_num_entries, new_num_entries;

  /* Note old and new number of entries. */
  old_num_entries = tablep->num_entries;
  new_num_entries = tablep->num_entries * EXPAND_FACTOR;
  tablep->num_entries = new_num_entries;

  /* Note old and new entriesp location, and difference. */
  old_entriesp = tablep->entriesp;
  tablep->entriesp = realloc(tablep->entriesp, new_num_entries);
  new_entriesp = tablep->entriesp;
  entriesp_difference = new_entriesp - old_entriesp;

  /* Traverse table's entry list and update all the pointers. */
  tablep->headp += entriesp_difference;
  tablep->tailp += entriesp_difference;
  entryp = tablep->headp;
  while (entryp != NULL) {
    entryp->nextp += entriesp_difference;
    entryp->backp += entriesp_difference;
    entryp->forep += entriesp_difference;
    entryp = entryp->forep;
  }

  /* Update pointers in binspp. */
  for (i = 0; i < old_num_entries; i++) {
    if (tablep->binspp[i] != NULL)
      tablep->binspp[i] += entriesp_difference;
  }

  /* Realloc for binspp. */
  tablep->binspp = realloc(tablep->binspp, new_num_entries);

  /* NULLify the new bins. */
  for (i = old_num_entries; i < new_num_entries; i++) {
    tablep->binspp[i] = NULL;
  }

  /* For each bin, for each entry, move to appropriate bin
     if not already in it. */
  for (i = 0; i < tablep->num_entries; i++) {
    entryp = tablep->binspp[i];
    while (entryp != NULL) {
      if (BIN_IDX_FROM_ENTRYP(tablep, entryp) != i) {
        DEL_FROM_BIN_LIST(tablep, i, entryp);
        ADD_TO_BIN_LIST(tablep, entryp);
      }
      entryp = entryp->nextp;
    }
  }
}

/*------------------*/

object* get(struct st_table *tablep, object key) {
  struct st_entry *entryp = BIN_FROM_KEY(tablep, key);
  while(entryp != NULL) {
    if (entryp->key == key) {
      return entryp->valuep;
    }
    entryp = entryp->nextp;
  }
  return NULL;
}

void set(struct st_table *tablep, object key, object *valuep) {
  if(tablep->num_entries >= tablep->num_entries) 
    expand_table(tablep);

  struct st_entry *entryp = &(tablep->entriesp[tablep->num_entries++]);
  entryp->valuep = valuep;

  ADD_TO_TABLE_LIST(tablep, entryp);
  ADD_TO_BIN_LIST(tablep, entryp);
}

void delete(struct st_table *tablep, object key) {
  struct st_entry *entryp = BIN_FROM_KEY(tablep, key);
  while(entryp->nextp->key != key && entryp->nextp->key != NULL) {
    entryp = entryp->nextp;
  }
  if (entryp->nextp != NULL) {
    entryp->nextp = entryp->nextp->nextp;
  }
  return;
}

struct st_entry* affinity_rehash(long *hashesp, size_t len) {
  // Rahman gives me a list of hashes, and I make those bins
  // consecutive in a newly malloc'd space.
  return NULL;
}












