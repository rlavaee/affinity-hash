#ifndef AFFINITY_ANAYLSIS_HPP
#define AFFINITY_ANAYLSIS_HPP

#include <list>
#include <unordered_set>
#include <functional>
#include <cstdint>
#include <algorithm>
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
using timestamp_t   = uint32_t;

template <class T> using timestamp_map_t = std::unordered_map<const T, timestamp_t>;
template <class T> using entry_set_t = std::unordered_set<const T>;
template <class T> using entry_vec_t = std::vector<T>;
template <class T> using entry_list_t = std::list<T>;
template <class T> using decay_map_t = std::unordered_map<T, std::pair<bool, uint32_t>>;

/** Globals **/
static wsize_t max_fpdist;
static wsize_t max_fpdist_ind;
static uint16_t analysis_set_size;

/*
  Struct: affinity_pair_t
*/
template<class T>
struct affinity_pair_t {
  T lentry, rentry;
  uint32_t affinity;

  affinity_pair_t(T le, T re, uint32_t affinity): lentry(le),
    rentry(re), affinity(affinity) {}

  bool operator < (const affinity_pair_t affinity_pair) const {
    return (affinity > affinity_pair.affinity);
  }

  friend std::ostream& operator << (std::ostream& out, const affinity_pair_t affinity_pair) {
    out << "[" << affinity_pair.lentry << "," << affinity_pair.rentry << "]:" << affinity_pair.affinity;
    return out;
  }
};



/*
  Struct: entry_t

  Stores information associated with a hashtable entry.
*/
struct entry_t {
  entry_index_t entry_index;

  entry_t() {}
  entry_t(entry_index_t b): entry_index(b) {}
  entry_t(const entry_t& hentry): entry_index(hentry.entry_index) {}

  bool operator == (const entry_t& rhs) const {
    return entry_index == rhs.entry_index;
  }

  friend std::ostream& operator << (std::ostream& out, const entry_t& obj) {
    out << "(" << std::setbase(16)  << obj.entry_index << ")" << std::setbase(10);
    return out;
  }
};

namespace std {
 template<> struct hash<entry_t> {
  size_t operator()(const entry_t& __val) const noexcept {
    size_t const h2 ( std::hash<entry_index_t>()(__val.entry_index) );
    return h2;
  }
};

 template<> struct hash<const entry_t> {
  size_t operator()(const entry_t& __val) const noexcept {
    size_t const h2 ( std::hash<entry_index_t>()(__val.entry_index) );
    return h2;
  }
};
}

/*
  Struct: wcount_t

  Stores the histogram of windows in which a pair of
  entry_t's appear in for a given trace.
*/
struct wcount_t {
  uint32_t excluded_windows;
  std::vector<uint32_t> common_windows;
  uint32_t all_windows;

  wcount_t(): excluded_windows(0), all_windows(0) {
    common_windows = std::vector<uint32_t>(max_fpdist_ind + 1);
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

    out << "\n----------------------------\n";

    return out;
  }

  uint32_t get_affinity() {
    uint32_t a = 0;
    for(uint32_t i = 0; i < common_windows.size(); ++i)
      a += ((common_windows.size() - i) * common_windows[i]);

    return a;
  }
};

template <class T> using wcount_map_t = std::unordered_map<const T, wcount_t>;

/*
  Struct: all_wcount_t

  Stores the histogram of windows in which a pair of
  entry_t's appear in for a given trace.
*/
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
  timestamp_t start;

  window_t(): wsize(0) {
    owners.reserve(analysis_set_size);
  }

  window_t(const T& entry, timestamp_t timestamp): wsize(1), capacity(1), start(timestamp) {
    owners.reserve(analysis_set_size);
    owners.push_back(entry);
  }

  ~window_t() {
    owners.clear();
  }

  void push_front(const T& entry) { wsize++; }
  void erase(const typename entry_list_t<T>::iterator& it) { wsize--; }
  bool empty() { return wsize == 0; }

  void merge_owners(const window_t& other) {
    typename entry_vec_t<T>::iterator eit_begin = owners.begin();
    typename entry_vec_t<T>::iterator eit_end = owners.end();

    for (const auto& e : other.owners) {
      if (std::find(eit_begin, eit_end, e) == eit_end)
        owners.push_back(e);
    }
  }

  friend std::ostream& operator << (std::ostream& out, const window_t& obj) {
    out << "\nowners: ";
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

class Analysis {
  using affinity_pair_t = affinity_pair_t<entry_t>;

 public:
  Analysis();

  // TODO: analysis_bit not needed. analysis_vec provides equiv. information.
  void trace_hash_access(entry_index_t entry_index);

  void remove_entry(entry_index_t entry_index);

  std::vector<affinity_pair_t> get_affinity_pairs();

 private:

  // The hash table being traced.

  // Verbosity setting
  static std::ostream &err;
  bool DEBUG = false;
  bool PRINT = false;

  // Sampling settings
  timestamp_t timestamp;  // TODO: is [0, 2^32] going to be an issue?
  bool analysis_set_sampling;

  uint32_t analysis_count_down;
  uint32_t reorder_count_down;

  static constexpr uint32_t analysis_sampling_time = 1 << 8;
  static constexpr uint32_t analysis_stage_time = 1 << 12;

  entry_vec_t<entry_t> analysis_vec;
  entry_set_t<entry_t> analysis_set;

  entry_set_t<entry_t> remove_set;

  // Affinity data structure
  all_wcount_map_t<entry_t> affinity_map;
  decay_map_t<entry_t> decay_map;

  // Trace list data structure
  window_list_t<entry_t> window_list;

  timestamp_map_t<entry_t> timestamp_map;

  // Debug functions
  void dump_window_list (std::ostream& out, window_list_t<entry_t>& wlist) {
    out << "trace list:------------------------------------------\n" ;
    //out << "size: " << window_list_size << "\n";
    out << wlist ;
    out << "---------------------------------------------\n";
  }

  // Affinity analyzing functions
  void update_affinity(const entry_vec_t<entry_t>& entry_vec, const entry_t& entry, int fpdist_ind);

  void add_compress_update(const entry_t& entry, bool analysis);
};

struct layout_t: std::deque<entry_t> {
  bool dumped = false;

  layout_t(const entry_t& fentry): std::deque<entry_t>() {
    this->push_back(fentry);
  }

  void merge(std::unordered_map<entry_t, layout_t*> &layout_map, layout_t* layout) {
    for (const auto& entry : *layout) {
      this->push_back(entry);
      layout_map[entry] = this;
    }

    delete layout;
  }

  friend std::ostream& operator << (std::ostream& out, const layout_t& layout) {
    for (auto entry : layout)
      out << entry << " ";

    out << "\n";
    return out;
  }
};

class Layout {
  using affinity_pair_t = affinity_pair_t<entry_t>;

public:
  Layout(Analysis& a);

  std::vector<layout_t> find_affinity_layout();

  std::vector<layout_t> getLayouts();

private:
  Analysis& analysis;
  std::unordered_map<entry_t, layout_t*> layout_map;
};
#endif
