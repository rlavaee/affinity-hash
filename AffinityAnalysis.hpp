#ifndef AFFINITY_ANAYLSIS_HPP
#define AFFINITY_ANAYLSIS_HPP

#include <list>
#include <unordered_set>
#include <stdint.h>
#include <algorithm>
#include <assert.h>
#include <fstream>
#include <iomanip>
#include <unordered_map>
#include <vector>
#include <numeric>
#include <deque>

#include <iostream>

/** Types **/
using entry_index_t = ptrdiff_t;
using wsize_t       = uint16_t;

template <class T> using timestamp_map_t = std::unordered_map<const T, uint32_t>;
template <class T> using entry_set_t = std::unordered_set<const T>;
template <class T> using entry_vec_t = std::vector<T>;
template <class T> using entry_list_t = std::list<T>;

/** Globals **/
wsize_t max_fpdist;
wsize_t max_fpdist_ind;
uint16_t analysis_set_size;

// Affinity representation of a hash table entry.
template<typename T>
struct entry_t {
//  T* table_ptr;
  entry_index_t entry_index;

  entry_t() {}
  entry_t(/* T* a, */ entry_index_t b): /* table_ptr(a), */ entry_index(b) {}
  entry_t(const entry_t& hentry): /* table_ptr(hentry.table_ptr), */ entry_index(hentry.entry_index) {}

  bool operator == (const entry_t& rhs) const {
    return /*table_ptr == rhs.table_ptr && */ entry_index == rhs.entry_index;
  }
/*
  bool operator < (const entry_t& rhs) const {
    if (table_ptr < rhs.table_ptr) return true;
    else if (table_ptr > rhs.table_ptr) return false;
    else if (entry_index < rhs.entry_index) return true;
    else return false;
  }
*/
  friend std::ostream& operator << (std::ostream& out, const entry_t& obj) {
    out << "(" << std::setbase(16) /* << obj.table_ptr << ", " */ << obj.entry_index << ")" << std::setbase(10);
    return out;
  }
};

namespace std {
template <class T> struct hash<entry_t<T>> {
  size_t operator()(const entry_t<T>& __val) const noexcept {
    // size_t const h1 ( std::hash<T*>()(__val.table_ptr) );
    size_t const h2 ( std::hash<entry_index_t>()(__val.entry_index) );
    //return h1 ^ ((h2 << 1) << 1);
    return h2;
  }
};

template <class T> struct hash<const entry_t<T>> {
  size_t operator()(const entry_t<T>& __val) const noexcept {
    // size_t const h1 ( std::hash<T*>()(__val.table_ptr) );
    size_t const h2 ( std::hash<entry_index_t>()(__val.entry_index) );
    // return h1 ^ ((h2 << 1) << 1);
    return h2;
  }
};
}

struct wcount_t {
  uint32_t excluded_windows;
  std::vector<uint32_t> common_windows; // Histogram of window length.
  uint32_t all_windows;

  wcount_t(): excluded_windows(0), all_windows(0) {
    common_windows = std::vector<uint32_t>(max_fpdist_ind + 1); // G
  }

  wcount_t& operator+=(const wcount_t& rhs) {
    this->excluded_windows += rhs.excluded_windows;

    std::transform(rhs.common_windows.begin(), rhs.common_windows.end(),
                   this->common_windows.begin(), this->common_windows.begin(), std::plus<uint32_t>());

    this->all_windows += rhs.all_windows;

    return *this;
  }

  friend std::ostream& operator << (std::ostream& out, const wcount_t& obj) {
    out << "excluded: " << obj.excluded_windows;
    out << "\nsum of common: " << obj.all_windows;
    out << "\ncommon:\n";
    out << "size: " << obj.common_windows.size() << "\n";

    uint32_t check_sum = 0;
    for (const auto& count : obj.common_windows) {
      out << count << "\t";
      check_sum += count;
    }

    assert(obj.all_windows == check_sum);

    out << "\n----------------------------\n";

    return out;
  }

  uint32_t get_affinity() const {
    // TODO: Weight window sizes.
    return std::accumulate(common_windows.begin(), common_windows.end(), 0);
  }

};

template <class T> using wcount_map_t = std::unordered_map<const T, wcount_t>;

template <class T>
struct all_wcount_t {
  uint32_t potential_windows;
  wcount_map_t<T> wcount_map;

  all_wcount_t(uint32_t _pw): potential_windows(_pw) {}
  all_wcount_t(): potential_windows(0) {
    wcount_map = wcount_map_t<T>();
  }

  friend std::ostream& operator << (std::ostream& out, const all_wcount_t& obj) {
    out << "potential windows: " << obj.potential_windows;
    out << "\nwindow count map:\n";

    for (const auto& wcount_pair : obj.wcount_map) {
      out << wcount_pair.first << "\n";
      out << wcount_pair.second << "\n";
    }

    return out;
  }
};

template <class T> using all_wcount_map_t = std::unordered_map <const T, all_wcount_t<T>>;

template <class T>
std::ostream& operator << (std::ostream& out, const all_wcount_map_t<T>& m) {
  for (const auto& all_wcount_pair : m) {
    out << "************************\n";
    out << all_wcount_pair.first << "\n";
    out << all_wcount_pair.second << "\n";
  }

  return out;
}

template <class T>
struct window_t {
  entry_vec_t<T> owners;
  wsize_t wsize;
  wsize_t capacity; // NOT USED.
  uint32_t start;
  //entry_list_t partial_entry_list;

  window_t(): wsize(0) {
    owners.reserve(analysis_set_size);
  }

  window_t(const T& entry, uint32_t timestamp): wsize(1), capacity(1), start(timestamp) {
    owners.reserve(analysis_set_size);
    owners.push_back(entry);
  }

  ~window_t() {
    owners.clear();
  }

  void push_front(const T& entry) {
    wsize++;
  }

  void erase(const typename entry_list_t<T>::iterator& it) {
    //partial_entry_list.erase(it);
    wsize--;
  }

  bool empty() {
    return wsize == 0;
  }

  void merge_owners(const window_t& other) {
    typename entry_vec_t<T>::iterator eit_begin = owners.begin();
    typename entry_vec_t<T>::iterator eit_end = owners.end();

    for (const auto& e : other.owners) {
      if (std::find(eit_begin, eit_end, e) == eit_end)
        owners.push_back(e);
    }
  }

  friend std::ostream& operator << (std::ostream& out, const window_t& obj) {
    out << "\nowners: " ;
    for (const auto& entry : obj.owners)
      out << entry << " ";

    out << "\n:size: " << obj.wsize << "\n";
    return out;
  }

};

template <class T> using window_list_t = std::list<window_t<T>>;

template <class T>
std::ostream& operator << (std::ostream& out, const window_list_t<T>& wlist) {
  for (const auto& window : wlist) {
    out << window << "\n";
  }

  return out;
}

template<class T, class D>
class Layout;

template<class T, class D>
class Analysis {
  using entry_t = entry_t<T>;

 public:
  Analysis(T *tbl): table_ptr(tbl) {
  // : err("debug.out") {
    srand(time(NULL));

    // get prepared for the first analysis_set_sampling round
    analysis_set_sampling = true;
    count_down = analysis_sampling_time;

    const char* e;
    if ((e = getenv("ST_HASH_DEBUG")) && atoi(e) == 1) // Set and not empty
      DEBUG = true;

    if ((e = getenv("ST_HASH_PRINT")) && atoi(e) == 1) // Set and not empty
      PRINT = true;

    if ((e = getenv("ST_HASH_ANALYSIS_SAMPLE_SIZE")))
      analysis_set_size = atoi(e);
    else  // default value; just to make sure it will be working for more than 1
      analysis_set_size = 2;

    if ((e = getenv("ST_HASH_MAX_FPDIST_IND")))
      max_fpdist_ind = atoi(e);
    else // default value; just something non-trivial
      max_fpdist_ind = 2;

    max_fpdist = 1 << max_fpdist_ind;

    timestamp = 0;
  }

  void trace_hash_access(entry_index_t entry_index, bool analysis_bit) {
    int r = rand();
    if ((r & sampleMask) != 0)
      return;

    // is [0, 2^32] going to be an issue?
    timestamp++;

    // Count down and transit into the next stage if count drops to zero.
    if (!count_down--) {
      if (analysis_set_sampling) {
        if (DEBUG) {
          err << "End of the sampling stage, analyzed entries are:\n";

          for (const auto& a_entry : analysis_vec)
            err << a_entry << "\n";
          err << "\n";
        }

        analysis_set_sampling = false;
        // For up to few hundred elements, lookup in vector is faster than in red-black tree:
        //	http://cppwisdom.quora.com/std-map-and-std-set-considered-harmful
        //analysis_set = entry_set_t(analysis_vec.begin(),analysis_vec.end());
        for (const auto& e : analysis_vec)
          table_ptr->entries[e.entry_index].set_analysis_bit(1);

        count_down = analysis_stage_time;
      } else {
        if (DEBUG) {
          err << "Analysis stage finishes now!\n";
          err.flush();
        }
        // Clear the exluded entry set and the analysis entry set
        //analysis_set.clear();
        analysis_vec.clear();
        window_list.clear();
        timestamp_map.clear();

        for (const auto& e : analysis_vec)
          table_ptr->entries[e.entry_index].set_analysis_bit(0);

        //excluded_set.clear();

        // Get ready for the next analysis_set_sampling round
        analysis_set_sampling = true;
        count_down = analysis_sampling_time;
      }

    } else {
      // entry_t entry(tbl, entry_index);
      entry_t entry(entry_index);

      if (analysis_set_sampling) {
        // Fill in the analysis vector first
        if (analysis_vec.size() < analysis_set_size)
          analysis_vec.push_back(entry);
        else {
          // Reservoir sampling
          int r = rand() % (analysis_sampling_time - count_down);
          if (r < analysis_set_size)
            analysis_vec[r] = entry;
        }
      } else {
        if (DEBUG) dump_window_list(err, window_list);

        add_compress_update(entry, analysis_bit);
      }
    }
  }

  void remove_entry(T* tbl, entry_index_t entry_index) {
    // NOTE: Nothing seems to be done about this set.
    global_remove_set.insert(hash_t(tbl, entry_index));
  }

 private:
  friend class Layout<T, D>;

  // The hash table being traced.
  T const * const table_ptr;

  // Verbosity setting
  static std::ostream &err; // layout_os;
  bool DEBUG = false;
  bool PRINT = false;

  // Sampling settings
  uint32_t timestamp;
  bool analysis_set_sampling;
  uint32_t count_down;
  static constexpr uint32_t analysis_sampling_time = 1 << 8;
  static constexpr uint32_t analysis_stage_time = 1 << 12;

  entry_vec_t<entry_t> analysis_vec;
  entry_set_t<entry_t> global_remove_set;

  // Affinity data structure
  all_wcount_map_t<entry_t> affinity_map;

  // Trace list data structure
  window_list_t<entry_t> window_list;

  timestamp_map_t<entry_t> timestamp_map;
  static constexpr int sampleMask = 0x0;

  // Debug functions

  void dump_window_list (std::ostream& out, window_list_t<entry_t>& wlist) {
    out << "trace list:------------------------------------------\n" ;
    //out << "size: " << window_list_size << "\n";
    out << wlist ;
    out << "---------------------------------------------\n";
  }

  // Affinity analyzing functions

  void update_affinity(const entry_vec_t<entry_t>& entry_vec, const entry_t& entry, int fpdist_ind) {
    for (const auto& a_entry : entry_vec) {
      wcount_t& ref = affinity_map[a_entry].wcount_map[entry];
      ref.common_windows.at(fpdist_ind)++;
      ref.all_windows++;
    }
  }

  void add_compress_update(const entry_t& entry, bool analysis) {
    // NOTE:  A global variable with the same name exists.
    typename window_list_t<entry_t>::iterator window_it = window_list.begin();

    if (analysis) {
      /*IMPORTANT: this will call the constructor of window_t */
      window_list.emplace_front(entry, timestamp); /* remember to set the capacity as well */
      auto res = affinity_map.emplace(entry, 0);
      res.first->second.potential_windows++;
    }

    if (window_it == window_list.end())
      return;

    auto res = timestamp_map.emplace(entry, timestamp);

    int entry_ts = -1;
    if (!res.second) {
      entry_ts = res.first->second;
      res.first->second = timestamp;
    }

    if (window_it->start <= entry_ts)
      return;

    wsize_t fpdist;
    if (analysis) {
      fpdist = 1;
    } else {
      window_it->wsize++;
      fpdist = 0;
    }

    typename window_list_t<entry_t>::iterator prev_window_it;
    wsize_t exp_fpdist = 1;
    int exp_fpdist_ind = 0;

    while (true) {
      prev_window_it = window_it;
      window_it++;

      prev_window_it->capacity = fpdist + 1;

compress:
      if (window_it == window_list.end()) {
        fpdist += prev_window_it->wsize;

        if (fpdist > max_fpdist)
          return;

        while (fpdist > exp_fpdist) {
          exp_fpdist <<= 1;
          exp_fpdist_ind++;
        }
        update_affinity(prev_window_it->owners, entry, exp_fpdist_ind);
        return;
      }

      if (window_it->start <= entry_ts) {
        window_it->wsize--;
      }

      if (fpdist + 1 >= prev_window_it->wsize + window_it->wsize) { /* fpdist+1 = prev_window_it->capacity */
        prev_window_it->wsize += window_it->wsize;

        prev_window_it->merge_owners(*window_it);
        prev_window_it->start = window_it->start;

        /* may leak memory */
        window_it = window_list.erase(window_it);
        goto compress;
      } else {
        fpdist += prev_window_it->wsize;

        if (fpdist > max_fpdist)
          return;

        while (fpdist > exp_fpdist) {
          exp_fpdist <<= 1;
          exp_fpdist_ind++;
        }

        update_affinity(prev_window_it->owners, entry, exp_fpdist_ind);
      }

      if (prev_window_it->start <= entry_ts) // nothing to be done after this.
        return;
    }
  }
};

template<class T, class D>
std::ostream &Analysis<T,D>::err(std::cerr);

template<class T, class D>
class Layout {
  using entry_t = entry_t<T>;
  using layouts_t = Layout<T, D>;

 public:
  class layout_t: public std::deque<entry_t> {
   public:
    bool dumped = false;

    layout_t(const entry_t& fentry): std::deque<entry_t>() {
      this->push_back(fentry);
    }

    void merge(layouts_t* layouts, layout_t* layout) {
      for (const auto& entry : *layout) {
        this->push_back(entry);
        layouts->layout_map[entry] = this;
      }

      delete layout;
    }

    friend std::ostream& operator << (std::ostream& out, const layout_t& layout) {
      for (T entry : layout)
        out << entry << " ";

      out << "\n";
      return out;
    }
  };

  class affinity_pair_t {
   public:
    entry_t lentry, rentry;
    uint32_t affinity;

    affinity_pair_t(entry_t le, entry_t re, uint32_t affinity): lentry(le),
      rentry(re), affinity(affinity) {}

    bool operator < (const affinity_pair_t affinity_pair) const {
      return (affinity > affinity_pair.affinity);
    }

    friend std::ostream& operator << (std::ostream& out, const affinity_pair_t affinity_pair) {
      out << "[" << affinity_pair.lentry << "," << affinity_pair.rentry << "]:" << affinity_pair.affinity;
      return out;
    }
  };

  Layout(const Analysis<T, D>& a) : affinity_map(a.affinity_map) {}

  // Change this to std::dequeue<D>
  std::vector<layout_t> find_affinity_layout() {
    layout_map.clear();

    std::vector<affinity_pair_t> all_affinity_pairs;

    for (const auto& all_wcount_pair : affinity_map) {
      auto le = all_wcount_pair.first;
      auto& all_wcount = all_wcount_pair.second;
      for (const auto& wcount_pair : all_wcount.wcount_map) {
        auto re = wcount_pair.first;
        uint32_t affinity = wcount_pair.second.get_affinity();
        all_affinity_pairs.push_back(affinity_pair_t(le, re, affinity));
      }
    }

    std::sort(all_affinity_pairs.rbegin(), all_affinity_pairs.rend());

    for (const auto& affinity_pair : all_affinity_pairs) {
      auto lLayoutPair = layout_map.emplace(std::piecewise_construct,
                                            std::forward_as_tuple(affinity_pair.lentry),
                                            std::forward_as_tuple((layout_t*)NULL));

      if (lLayoutPair.second)
        lLayoutPair.first->second = new layout_t(affinity_pair.lentry);

      auto rLayoutPair = layout_map.emplace(std::piecewise_construct,
                                            std::forward_as_tuple(affinity_pair.rentry),
                                            std::forward_as_tuple((layout_t*)NULL));

      if (rLayoutPair.second)
        rLayoutPair.first->second = new layout_t(affinity_pair.rentry);

      if (rLayoutPair.first->second != lLayoutPair.first->second) {
        if (rLayoutPair.first->second->size() > lLayoutPair.first->second->size())
          rLayoutPair.first->second->merge(this, lLayoutPair.first->second);
        else
          lLayoutPair.first->second->merge(this, rLayoutPair.first->second);
      }
    }

    return getLayouts();
  }

  std::vector<layout_t> getLayouts() {
    std::vector<layout_t> layouts;

    for (auto layout_pair : layout_map)
      if (!layout_pair.second->dumped) {
        layouts.push_back(*layout_pair.second);
        //layout_os << *layout_pair.second;
        layout_pair.second->dumped = true;
      }

    return layouts;
  }

 private:
  const all_wcount_map_t<entry_t>& affinity_map;
  std::unordered_map<entry_t, layout_t*> layout_map;
};

#endif
