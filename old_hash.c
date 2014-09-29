/* Brandon's Great Idea - replace "next" pointers with relative skips. */

#include <stdlib.h>
#include <assert.h>

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
#define BIN_FROM_IDX(tablep, idx)       tablep->binspp[idx                                ]

struct st_entry {
  long hash;
  object key;
  object *valuep;
  struct st_entry *nextp; // Singly linked list for bin
};

struct st_table {
  size_t num_bins;
  size_t num_entries;
  struct st_entry *entriesp;
  struct st_entry **binspp;
};

struct st_table* new_tablep() {
  struct st_table *tablep = malloc(sizeof(struct st_table));
  size_t i;

  tablep->entriesp = malloc(INITIAL_SIZE * sizeof(struct st_entry));
  tablep->binspp = calloc(INITIAL_SIZE, sizeof(struct st_entry*));

  tablep->num_entries = 0;
  tablep->num_bins = INITIAL_SIZE;

  /* NULLify the new bins. */
  if (NULL != 0) {
    for (i = 0; i < tablep->num_bins; i++) {
      tablep->binspp[i] = NULL;
    }
  }

  return tablep;
}

long hash_func(object key) {
  return key;
}

/** List Management */
// /* Add entryp to the table's list. */
// void add_to_table_list(struct st_table *tablep, struct st_entry *entryp) {
//   if (tablep->headp == NULL) {
//     tablep->headp = tablep->tailp = entryp;
//     entryp->backp = entryp->forep = NULL;
//   } else {
//     tablep->tailp->forep = entryp;
//     entryp->backp = tablep->tailp;
//     tablep->tailp = entryp;
//   }
// } 

// /* Remove from table's list, but leave data there. */
// void del_from_table_list(struct st_table *tablep, struct st_entry *entryp) {
//   // Can optimize these conditionals.
//   if (entryp->backp != NULL)
//     entryp->backp->forep = entryp->forep;
//   if (entryp->forep != NULL)
//     entryp->forep->backp = entryp->backp;
//   if (entryp == tablep->headp)
//     tablep->headp = entryp->forep; //Should be NULL
//   if (entryp == tablep->tailp)
//     tablep->tailp = entryp->nextp; //Should be NULL
// }

/* Add entryp to the appropriate bin's list.
   Memory must be zeroed PRIOR to calling this! */
void add_to_bin_list(struct st_table *tablep, struct st_entry *entryp) {
  entryp->nextp = BIN_FROM_ENTRYP(tablep, entryp);
  BIN_FROM_ENTRYP(tablep, entryp) = entryp;
}

/* Remove entryp from the indicated bin's list. */
struct st_entry * del_from_bin_list(struct st_table *tablep, long bin_idx, struct st_entry *entryp) {
  struct st_entry *next_entryp = entryp->nextp;
  struct st_entry *entryp_moving = BIN_FROM_IDX(tablep, bin_idx);

  if (next_entryp < 0x10000 && next_entryp != NULL) {
    ; //breakpoint
  }

  if (entryp_moving == NULL){
    printf("Problem 1: see code\n");
    exit(0);
  }

  if (entryp_moving == entryp) {
    BIN_FROM_IDX(tablep, bin_idx) = entryp->nextp;
    if (next_entryp < 0x10000 && next_entryp != NULL) {
      ; //breakpoint
    }
    return next_entryp;
  }

  while (entryp_moving->nextp != NULL) {
    if (entryp_moving->nextp == entryp) {
      entryp_moving->nextp = entryp->nextp;
      return next_entryp;
    }
    entryp_moving = entryp_moving->nextp;
  }

  assert(0);
}

void free_table(struct st_table *tablep) {
  free(tablep->entriesp);
  tablep->entriesp = NULL;
  free(tablep->binspp);
  tablep->binspp = NULL;
  //  free(tablep); // causes segfault. idk why.
}

/* Expand bins array, expand entries allocation, and rehash. */
void expand_table(struct st_table *tablep){
  int i;
  struct st_entry *entryp_moving, *next_entryp, *tmp1, **tmp2;
  struct st_entry *old_entriesp, *entryp;
  long entriesp_diff;
  size_t old_num_bins, new_num_bins;

  /* Note old and new number of entries. */
  old_num_bins = tablep->num_bins;
  new_num_bins = tablep->num_bins * EXPAND_FACTOR;
  tablep->num_bins = new_num_bins;

  /* Realloc the entries (see note at bottom about when realloc
     fails). Note old and new entriesp location, and difference. */
  old_entriesp = tablep->entriesp;

  // tmp1 = realloc(tablep->entriesp, new_num_bins * sizeof(struct st_entry));
  tablep->entriesp = realloc(tablep->entriesp, new_num_bins * sizeof(struct st_entry));
  // if (tmp1 != NULL) {
  //   tablep->entriesp = tmp1;
  // } else { 
  //   printf("realloc tablep->entriesp failed. hash table is too big!\n");
  //   exit(0);
  // }

  entriesp_diff = tablep->entriesp - old_entriesp;

  /* Update pointers. */
  for (i = 0; i < old_num_bins; i++) {
    if (tablep->binspp[i] != NULL) {
      tablep->binspp[i] += entriesp_diff;

      entryp_moving = tablep->binspp[i];
      int jake = 0;
      while (entryp_moving->nextp != NULL) {
        if (entryp_moving->nextp < 0x10000 && next_entryp != NULL) {
          ; //breakpoint
        }
        jake++;
        entryp_moving->nextp += entriesp_diff;
        entryp_moving = entryp_moving->nextp;
      }
    }
  }

  /* Realloc for binspp. */
  // tmp2 = realloc(tablep->binspp, new_num_bins * sizeof(struct st_entry*));
  tablep->binspp = realloc(tablep->binspp, new_num_bins * sizeof(struct st_entry*));
  // if (tmp2 != NULL) {
  //   tablep->binspp = tmp2;
  // } else {
  //   printf("realloc tablep->binspp failed. hash table is too big!\n");
  //   exit(0);
  // }

  /* NULLify the new bins. */
  for (i = old_num_bins; i < new_num_bins; i++) {
    tablep->binspp[i] = NULL;
  }

  /* For each bin, for each entry, move to appropriate bin
     if not already in it. */
  for (i = 0; i < old_num_bins; i++) {
    entryp = tablep->binspp[i];

    while (entryp != NULL) {
      next_entryp = entryp->nextp;
      if (BIN_IDX_FROM_ENTRYP(tablep, entryp) != i) {
        next_entryp = del_from_bin_list(tablep, i, entryp);
        add_to_bin_list(tablep, entryp);
      }
      if (next_entryp < 0x10000 && next_entryp != NULL) {
        ; //breakpoint
      }
      entryp = next_entryp; //entryp->nextp;
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
  if(tablep->num_entries >= tablep->num_bins) {
    expand_table(tablep);
  }

  // Or could find empty slot somehow, but that's harder.
  struct st_entry *entryp = &(tablep->entriesp[tablep->num_entries++]);
  entryp->key = key;
  entryp->hash = hash_func(key);
  entryp->valuep = valuep;

  // add_to_table_list(tablep, entryp);
  add_to_bin_list(tablep, entryp);
}

void delete(struct st_table *tablep, object key) {
  struct st_entry *entryp = BIN_FROM_KEY(tablep, key);

  // del_from_table_list(tablep, entryp);
  del_from_bin_list(tablep, BIN_IDX_FROM_ENTRYP(tablep, entryp), entryp);

  // NEED TO ZERO THE MEMORY?!?!?!

  // tablep->num_entries--;

  return;
}

struct st_entry* affinity_rehash(long *hashesp, size_t len) {
  // Rahman gives me a list of hashes, and I make those bins
  // consecutive in a newly malloc'd space.
  return NULL;
}














/* Code Graveyard */
/* List Management */
// Need to include edge cases.
// #define ADD_TO_TABLE_LIST(tablep, entryp) do { \
//   if (tablep->headp == NULL) {                 \
//     tablep->headp = entryp;                    \
//     entryp->backp = entryp->forep = NULL;      \
//   } else {                                     \
//     (tablep)->tailp->forep = (entryp);         \
//     (entryp)->backp = (tablep)->tailp;         \
//     (tablep)->tailp = (entryp);                \
//   }                                            \
// } while(0)

/* Removes from list, but leaves data there. */
// #define DEL_FROM_TABLE_LIST(tablep, entryp) do {                         \
//   if ((entryp)->backp != NULL) { (entryp)->backp->forep = (entryp)->forep; } \
//   if ((entryp)->forep != NULL) { (entryp)->forep->backp = (entryp)->backp; } \
// } while(0)

/* Memory must be zeroed PRIOR to calling this! */
// #define ADD_TO_BIN_LIST(tablep, entryp) do {         \
//   (entryp)->nextp = BIN_FROM_ENTRYP(tablep, entryp); \
//   BIN_FROM_ENTRYP(tablep, entryp) = entryp;          \
// } while(0)

// #define DEL_FROM_BIN_LIST(tablep, bin_idx, entryp) del_from_bin_list(tablep, bin_idx, entryp)

// void delete(struct st_table *tablep, object key) {
//   struct st_entry *entryp = BIN_FROM_KEY(tablep, key);

//   while(entryp->nextp->key != key && entryp->nextp->key != NULL) {
//     entryp = entryp->nextp;
//   }
//   if (entryp->nextp != NULL) {
//     entryp->nextp = entryp->nextp->nextp;
//   }
//   return;
// }

/* Remove entryp from the indicated bin's list. */
/*
void bad_del_from_bin_list(struct st_table *tablep, long bin_idx, struct st_entry *entryp) {
  struct st_entry *entryp_moving = BIN_FROM_IDX(tablep, bin_idx);

  if (entryp_moving == entryp) {
    BIN_FROM_IDX(tablep, bin_idx) = entryp->nextp;
  } else {
    while (entryp_moving != NULL && entryp_moving->nextp != entryp) {
      entryp_moving = entryp_moving->nextp;
    }
    if (entryp != NULL && entryp->nextp != NULL) {
      entryp->nextp = entryp->nextp->nextp;
    }
  }
}
*/

/* Note about realloc - when it fails. From http://stackoverflow.com/questions/1986538/how-to-handle-realloc-when-it-fails-due-to-memory */

// tmp = realloc(orig, newsize);
// if (tmp == NULL) {
//   // could not realloc, but orig still valid
// } else {
//   orig = tmp;
// }

    // right inside while loop for traversing table's entry list
    //    if ((long)entryp->nextp < 0x100000000 &&
    //        entryp->nextp != NULL) {
    //      ;//breakpoint here
      //    }
    //    if (entryp->nextp != NULL &&
    //        (entryp->nextp + entriesp_diff)->nextp != NULL &&
    //        (long)(entryp->nextp + entriesp_diff)->nextp < 0x100000000) {
    //      ;//breakpoint here
    //    }
