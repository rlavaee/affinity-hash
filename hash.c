#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

/** Hashtable constants */
#define INITIAL_SIZE 11 // size will be same as num_bins
#define EXPAND_FACTOR 2

/** Types used */
typedef long key_type;
typedef long val_type;
typedef unsigned hash_type;

/** Bin Retrieval */
#define BIN_IDX_FROM_ENTRY(table, entry) (entry)->hash % (table)->num_bins
#define BIN_IDX_FROM_HASH(table, hash)   (hash) % (table)->num_bins
#define BIN_IDX_FROM_KEY(table, key)     hash_func(&key, sizeof(key_type)) % (table)->num_bins

#define BIN_FROM_ENTRY(table, entry) table->bins[BIN_IDX_FROM_ENTRY(table, entry)]
#define BIN_FROM_HASH(table, hash)   table->bins[BIN_IDX_FROM_HASH(table, hash)  ]
#define BIN_FROM_KEY(table, key)     table->bins[BIN_IDX_FROM_KEY(table, key)    ]
#define BIN_FROM_IDX(table, idx)     table->bins[idx                             ]

typedef struct ah_entry {
  hash_type hash;
  key_type key;
  val_type *value;
  struct ah_entry *next; // Singly linked list for bin
} ah_entry;

typedef struct ah_table {
  size_t num_bins;
  size_t num_entries;
  ah_entry *entries;
  ah_entry **bins;
} ah_table;

ah_table *new_table() {
  size_t i;
  
  ah_table *table = malloc(sizeof(ah_table));
  table->entries  = malloc(INITIAL_SIZE * sizeof(ah_entry));
  table->bins     = malloc(INITIAL_SIZE * sizeof(ah_entry*));

  if (table->entries == NULL || table->bins == NULL) {
    printf("Oh no, you couldn't malloc!  Try harder!\n");
    exit(-1);
  }

  table->num_entries = 0;
  table->num_bins = INITIAL_SIZE;

  // Initialize the new bins.
  if (NULL != 0)
    for (i = 0; i < table->num_bins; i++)
      table->bins[i] = NULL;

  return table;
}

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

/** List Management */
void bin_list_insert(ah_table *table, ah_entry *entry) {
  // Find proper bin.
  ah_entry **bin = &(BIN_FROM_ENTRY(table, entry));
  
  // Insert entry into the bin.
  entry->next = *bin;
  *bin = entry;
}

//void bin_list_remove(ah_table *table, ah_entry *entry) {}

/** Table Management */
void free_table(ah_table *table) {
  free(table->entries);
  free(table->bins);
  free(table);
}

void expand_table(ah_table *table) {
  // Assuming num_entries >= num_bins.
  int new_size = table->num_entries * EXPAND_FACTOR;
  size_t i;

  // Prepare to iterate all old bins.
  ah_entry *old_entries = table->entries;
  size_t old_bin_num = table->num_bins;
  ah_entry **old_bin = table->bins;
  ah_entry  *current_entry, *temp_entry;

  // Not realloc'ing bins since we need the old bins to locate entries.
  ah_entry **new_bins = calloc(new_size, sizeof(ah_entry*));
  table->entries = realloc(table->entries, new_size * sizeof(ah_entry));

  if (table->entries == NULL || new_bins == NULL) {
    printf("Space allocation for expanding failed\n");
    exit(-1);
  }

  // Initialize the new bins.
  if (NULL != 0)
    for (i = 0; i < new_size; i++)
      new_bins[i] = NULL;

  table->num_bins = new_size;
  table->bins = new_bins;

  // Reinsert all entries into new bin list.
  for (i=0; i < old_bin_num; i++) {
    if(old_bin[i] == NULL) continue;
    current_entry = &table->entries[(old_bin[i] - old_entries)];
    
    while(current_entry != &table->entries[((ah_entry *)NULL - old_entries)]) {
      temp_entry = &table->entries[(current_entry->next - old_entries)];
      bin_list_insert(table, current_entry);
      current_entry = temp_entry;
    }
  }

  // Free the old bins.
  free(old_bin);
}

val_type *search(ah_table *table, key_type key) {
  ah_entry *entry;
  for(entry = BIN_FROM_KEY(table, key); entry != NULL; entry = entry->next)
    if (entry->key == key) 
      return entry->value;
  
  return NULL;
}

void insert(ah_table *table, key_type key, val_type *value) {
  hash_type hashed_key = hash_func(&key, sizeof(key_type));
  
  if (table->num_entries >= table->num_bins) 
    expand_table(table);

  // Create a new entry.
  ah_entry *entry = &(table->entries[table->num_entries++]);
  entry->key = key;
  entry->hash = hashed_key;
  entry->value = value;
  entry->next = NULL;
    
  // Add entry to hashtable.
  bin_list_insert(table, entry);
}

void delete(ah_table *table, key_type key) {
    // Find key
    // Remove from list
    // Data for entry isn't cleared.
}

//ah_entry* affinity_rehash(long *hashesp, size_t len) {}


int main() {
    ah_table *table = new_table();

    long i;

    for(i=0; i < 4000; i++) {
        long *temp = malloc(sizeof(long));
        *temp = i;
        insert(table, i, temp);
        printf("%lu, ", *search(table, i));
    }

    free_table(table);
}

