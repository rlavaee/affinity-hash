#ifndef AH_UNORDERED_MAP_HPP
#define AH_UNORDERED_MAP_HPP

#include <stdexcept>
#include <utility>
#include <cstddef>

#include <cstdlib>
#include <vector>
#include <cmath>
#include <tuple>

#include <functional>
#include <algorithm>

#include <unordered_map>

#include "Tracer.hpp"

template<typename V, bool cache_hash>
struct ah_entry;

template<typename K, typename V,
         bool cache_hash = false,
         bool trace_accesses = false,
         typename Hash = std::hash<K>,
         typename Pred = std::equal_to<K>
        >
class AhMap {
  using ah_entry = ah_entry<std::pair<K, V>, cache_hash>;
  using ah_map   = AhMap<K, V, cache_hash, trace_accesses, Hash, Pred>;

  static constexpr unsigned AH_EXPAND_FACTOR = 11;
  static constexpr unsigned AH_REORDER_CHECK = 10000000;
  static constexpr unsigned AH_INITIAL_SIZE = 3;
  static constexpr float    AH_LOAD_FACTOR = 0.15;

 public:
  AhMap() : epoc(0), num_bins(AH_INITIAL_SIZE), num_entries(0),
    total_capacity((size_t)ceil((1.0 + AH_LOAD_FACTOR) * AH_INITIAL_SIZE)),
    free_list(nullptr), last_entry(0), trace(this) {

    entries = new ah_entry[total_capacity]();
    bins = new ah_entry *[AH_INITIAL_SIZE]();
  }

  ~AhMap() {
    delete[] entries;
    delete[] bins;
  }

  V& insert(const K& key, V value) {
    check_reorder();

    size_t hash = hash_fn(key);

    // Resize the table if needed.
    if (free_list == nullptr && last_entry == total_capacity)
      ah_expand_table();

    // Find a entry to store in.
    ah_entry* entry;
    if (free_list != nullptr) {
      entry = free_list;
      free_list = free_list->next;
    } else entry = &entries[last_entry++];

    // Write new entry and store insert to a bin.
    entry->value = std::make_pair(key, value);
    if (cache_hash) entry->write_hash(hash);

    ah_bin_insert(entry, hash);

    num_entries++;

    return std::get<1>(entry->value);
  }

  V& find(const K& key) {
    check_reorder();

    size_t hash = hash_fn(key);

    return ah_bin_locate(key, hash);
  }

  bool remove(K key) {
    return true;
  }

  V& operator[](const K& key) {
    return find(key);
  }

  ah_entry* entries;

 private:
  friend class Tracer<ah_map, ah_entry>;

  size_t epoc;

  size_t num_bins;
  size_t num_entries;
  size_t total_capacity;

  ah_entry** bins;

  ah_entry* free_list;
  size_t last_entry;

  Hash hash_fn;
  Pred equa_fn;

  Tracer<ah_map, ah_entry> trace;

  inline void check_reorder() {
    if (trace_accesses && epoc++ % AH_REORDER_CHECK == 0) {
      reorder();
    }
  }

  inline ah_entry** bin_from_hash(size_t hash) {
    return &bins[hash % num_bins];
  }

  inline void ah_bin_insert(ah_entry* entry, size_t hash) {
    ah_entry** bin = bin_from_hash(hash);
    entry->next = *bin;
    *bin = entry;

    if (trace_accesses) trace.record(entry);
  }

  inline V& ah_bin_locate(const K& key, size_t hash) {
    ah_entry* entry = *bin_from_hash(hash);

    while (entry != nullptr && !equa_fn(std::get<0>(entry->value), key)) {
      if (trace_accesses) trace.record(entry);
      entry = entry->next;
    }

    if (entry != nullptr) {
      return std::get<1>(entry->value);

    } else {
      return insert(key, V());

    }
  }

  inline void ah_bin_remove(ah_entry* entry) { /* ... */ }

  // Second should never be invalid.
  // If first is invalid then it's on the free list.
  inline void ah_bin_swap(ah_entry* first, ah_entry* second) {
    if (first == second) return;

    ah_entry **first_ls_start, **second_ls_start;

    if (!cache_hash) {
      first_ls_start  = bin_from_hash(hash_fn(std::get<0>(first->value)));
      second_ls_start = bin_from_hash(hash_fn(std::get<0>(second->value)));
    } else {
      first_ls_start  = bin_from_hash(first->read_hash());
      second_ls_start = bin_from_hash(second->read_hash());
    }

    ah_entry* first_ls = *first_ls_start;
    ah_entry* second_ls = *second_ls_start;

    bool first_head = false, second_head = false;

    // Search through both bins and find the previous entry.

    if (first_ls != first && first_ls != nullptr) {
      while (first_ls->next != first && first_ls->next != nullptr)
        first_ls = first_ls->next;

    } else {
      first_head = true;
    }

    if (second_ls != second && second_ls != nullptr) {
      while (second_ls->next != second && second_ls->next != nullptr)
        second_ls = second_ls->next;

    } else {
      second_head = true;
    }

    if (first_head) {
      *first_ls_start = second;
    } else {
      first_ls->next = second;
    }

    if (second_head) {
      *second_ls_start = first;
    } else {
      second_ls->next = first;
    }

    std::swap(*first, *second);
  }

  void reorder() {
    auto new_ordering = trace.results();

    // Check for duplicates.
    std::unordered_set<entry_t> dups;

    for (const auto e : new_ordering) {
      if (dups.find(e) != dups.end()) {
        std::cout << "Error! Duplicate!\n";
      }

      dups.insert(e);
    }

    // Check that all entries in new ordering are valid.
    for (const auto e : new_ordering) {
      auto bin = *bin_from_hash(hash_fn(std::get<0>(entries[e].value)));

      while (bin != nullptr && bin != &entries[e])
        bin = bin->next;

      if (bin != &entries[e]) {
        std::cout << "Error! Invalid entry!\n";
        exit(0);
      }
    }

    // r: current_position -> original_position
    // r_i: original_position -> current_position
    std::unordered_map<entry_t, entry_t> r, r_i;

    for (auto i = 0; i < new_ordering.size(); ++i) {

      // If this position has been changed.
      if (r.find(new_ordering[i]) != r.end()) {
        if (r[new_ordering[i]] < i) {
          std::cout << "INVARIANT!\n";
          exit(0);
        }

        ah_bin_swap(&entries[i], &entries[r[new_ordering[i]]]);

        // If i isn't the original value of the position
        if (r_i.find(i) != r_i.end()) {
          r[r_i[i]] = r[new_ordering[i]];
          r_i[r[new_ordering[i]]] = r_i[i];

        } else {
          r[i] = r[new_ordering[i]];
          r_i[r[new_ordering[i]]] = i;
        }

      // Otherwise this position contains the original.
      } else {
        if (new_ordering[i] < i) {
          std::cout << "INVARIANT!\n";
          exit(0);
        }

        ah_bin_swap(&entries[i], &entries[new_ordering[i]]);

        if (r_i.find(i) != r_i.end()) {
          r[r_i[i]] = new_ordering[i];
          r_i[new_ordering[i]] = r_i[i];

        } else {
          r[i] = new_ordering[i];
          r_i[new_ordering[i]] = i;
        }
      }
    }
  }

  void ah_expand_table() {
    // Assuming num_entries >= num_bins.
    size_t new_size = AH_EXPAND_FACTOR * num_bins;

    // Prepare to iterate all old bins.
    size_t old_bin_num = num_bins, old_free_list;
    size_t old_total_capacity = total_capacity;
    ah_entry* old_entries = entries;
    ah_entry** old_bins = bins;

    if (free_list != nullptr) old_free_list = free_list - old_entries;

    bins           = new ah_entry *[new_size]();
    num_bins       = new_size;
    total_capacity = (size_t)ceil((1.0 + AH_LOAD_FACTOR) * new_size);
    entries        = new ah_entry[total_capacity]();

    if (free_list != nullptr) free_list = &entries[old_free_list];

    ah_entry*  current_entry, *temp_entry;

    std::copy(old_entries, old_entries + old_total_capacity, entries);
    delete[] old_entries;

    // Reinsert all entries into new bin list.
    for (size_t i = 0; i < old_bin_num; i++) {
      if (old_bins[i] == nullptr) continue;
      current_entry = &entries[old_bins[i] - old_entries];

      // Iterate through each bin and move entries to new array.
      while (current_entry != nullptr) {
        temp_entry = (current_entry->next != nullptr) ?
                     &entries[current_entry->next - old_entries] : nullptr;

        if (cache_hash) ah_bin_insert(current_entry, current_entry->read_hash());
        else ah_bin_insert(current_entry, hash_fn(std::get<0>(current_entry->value)));

        current_entry = temp_entry;
      }
    }

    delete[] old_bins;
  }
};

template<typename V>
struct ah_entry<V, true> {
  V value;
  ah_entry* next;
  size_t hash;

  void write_hash(size_t hash) {
    this->hash = hash;
  }
  size_t read_hash() {
    return hash;
  }
};

template<typename V>
struct ah_entry<V, false> {
  V value;
  ah_entry* next;

  void write_hash(size_t hash) {
    throw std::logic_error("");
  }
  size_t read_hash() {
    throw std::logic_error("");
  }
};

#endif
