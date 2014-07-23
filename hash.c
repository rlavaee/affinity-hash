#define INITIAL_SIZE 8 // size will be same as num_bins
#define EXPAND_FACTOR 2
typedef long object;

/* Bin Retrieval */
#define BIN_IDX_FROM_ENTRYP(tablep, entryp) hash_func((entryp)->(key)) % (tablep)->(num_bins)
#define BIN_IDX_FROM_HASH(tablep, hash)     (hash) % (tablep)->(num_bins)
#define BIN_IDX_FROM_KEY(tablep, key)       hash_func(key) % (tablep)->(num_bins)

#define BIN_FROM_ENTRYP(tablep, entryp) tablep->binspp[BIN_IDX_FROM_ENTRYP(entryp)]
#define BIN_FROM_HASH(tablep, hash)     tablep->binspp[BIN_IDX_FROM_HASH(hash)    ]
#define BIN_FROM_KEY(tablep, key)       tablep->binspp[BIN_IDX_FROM_KEY(key)      ]

/* List Management */
// Need to include edge cases.
#define ADD_TO_TABLE_LIST(tablep, entryp) { \
  if (tablep->headp == NULL) {              \
    tablep->headp = entryp;                 \
    entryp->backp = entryp->forep = NULL;   \
  } else {                                  \
    (tablep)->tailp->forep = (entryp);      \
    (entryp)->backp = (tablep)->tailp;      \
    (tablep)->tailp = (entryp);             \
  }                                         \
}

/* Removes from list, but leaves data there. */
#define DEL_FROM_TABLE_LIST(tablep, entryp) {                            \
  if ((entryp)->back != NULL) { (entryp)->back->fore = (entryp)->fore; } \
  if ((entryp)->fore != NULL) { (entryp)->fore->back = (entryp)->back; } \
}

/* Memory must be zeroed PRIOR to calling this! */
#define ADD_TO_BIN_LIST(tablep, entryp) {            \
  (entryp)->nextp = BIN_FROM_ENTRYP(tablep, entryp); \
  BIN_FROM_ENTRYP(tablep, entryp) = entryp;          \
}

#define DEL_FROM_BIN_LIST(tablep, entryp) {   \
  st_entry *entryp2 = tablep->binspp[bin_idx] \
}

struct st_entry {
  long hash;
  object key;
  object *valuep;
  st_entry *nextp; // Singly linked list for bin
  st_entry *backp; // Doubly linked list for table
  st_entry *forep; // Doubly linked list for table
};

struct st_table {
  size_t num_bins;
  size_t num_entries;

  st_entry *entriesp;
  st_entry **binspp;
  st_entry *headp; // Doubly linked list for table
  st_entry *tailp; // Doubly linked list for table
};

st_table * new_table() {
  st_table *tablep = malloc(sizeof(st_table));

  table->entriesp = malloc(INITIAL_SIZE * sizeof(st_entry));
  tablep->binspp = calloc(INITIAL_SIZE, sizeof(st_entry*));

  tablep->headp = tablep->tailp = NULL;
  return tablep;
}

long hash_func(object key) {
  return key;
}

void expand_table(st_table *tablep){
  // Need to change all the pointers too.  May be able to add
  // difference between old and new pointer to each :)
  table->entriesp = realloc(table->entriesp, table->num_entries * EXPAND_FACTOR);
  // Need to zero this?
  st_entry **new_binspp = calloc(table->num_bins * EXPAND_FACTOR, sizeof(st_entry*))
   // use: void * memcpy(void *restrict dst, const void *restrict src, size_t n)
  table->binspp = realloc(table->binspp, table->num_bins * EXPAND_FACTOR);
  table->num_bins *= EXPAND_FACTOR;
  return;
}

------------------

object * get(st_table *tablep, object key) {
  st_entry *entryp = BIN_FROM_KEY(tablep, key);
  while(entryp != NULL) {
    if (entryp->key == key) {
      return entryp->value;
    }
    entryp = entryp->nextp;
  }
  return NULL;
}

void set(st_table *tablep, object key, object *value) {
  if(tablep->num_entries >= tablep->num_allocated) 
    expand_table(tablep);

  st_entry *entryp = tablep->entriesp[table->num_entries++];
  entryp->value = value;

  ADD_TO_TABLE_LIST(tablep, entryp);
  ADD_TO_BIN_LIST(tablep, entryp);
}

void delete(st_table *table, object key) {
  st_entry *entryp = BIN_FROM_KEY(tablep, key);
  while(entryp->next->key != key && entryp->next->key != NULL) {
    entryp = entryp->next;
  }
  if entryp->next != NULL {
    entryp->next = entryp->next->next;
  }
  return;
}

st_entry * affinity_rehash(long *hashesp, size_t len) {
  // Rahman gives me a list of hashes, and I make those bins
  // consecutive in a newly malloc'd space.
  return NULL;
}












