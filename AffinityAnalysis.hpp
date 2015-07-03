#ifndef AFFINITY_ANAYLSIS_HPP
#define AFFINITY_ANAYLSIS_HPP

#include <unordered_set>
#include <functional>
#include <cstdint>
#include <algorithm>
#include <unordered_map>
#include <vector>
//#include <numeric>
#include <deque>
#include <cmath>
#include <array>
#include <map>

#include <iostream>

/** Globals **/
static const uint16_t max_fpdist(1 << 10);
static const uint16_t max_fpdist_ind(10);
static const uint16_t analysis_set_size(10);

static const uint16_t analysis_reordering_period(1 << (6+4));
static const uint16_t analysis_staging_period(1 << 12);
static const uint16_t analysis_sampling_period(1 << 8);

/** Types **/
using entry_t  = ptrdiff_t;
using wsize_t  = uint16_t;   // bits(wsize_t)  >= log(max_fpdist)
using avalue_t = uint32_t;   // bits(avalue_t) >= log(reorder_period * sampling_period) - 2

using entry_index_t = ptrdiff_t;
using entry_pair_t  = std::pair<entry_t, entry_t>;
using window_hist_t = std::array<avalue_t, max_fpdist_ind>; // CHeck!

template <class T> using entry_set_t = std::unordered_set<T>;
template <class T> using entry_vec_t = std::vector<T>;
template <class T> using decay_map_t = std::unordered_map<T, std::pair<bool, uint32_t>>;

/*
  Struct: affinity_pair_t
*/
struct affinity_pair_t {
  entry_t lentry, rentry;
  avalue_t affinity;

  affinity_pair_t(entry_t le, entry_t re, uint32_t affinity): lentry(le),
    rentry(re), affinity(affinity) {}

  bool operator < (const affinity_pair_t affinity_pair) const {
    return (affinity > affinity_pair.affinity);
  }

  friend std::ostream& operator << (std::ostream& out, const affinity_pair_t affinity_pair) {
    out << "("<<"[" << affinity_pair.lentry << "," << affinity_pair.rentry << "]: " << affinity_pair.affinity << ")";
    return out;
  }
};

/*
  Struct: window_t

*/
struct window_t {
  wsize_t length;
};

/*
  Struct: layout_t

*/
struct layout_t: std::deque<entry_t> {
  bool dumped = false;

  layout_t(const entry_t& fentry): std::deque<entry_t>() {
    this->push_back(fentry);
  }

  void merge(std::unordered_map<entry_t, layout_t*>& layout_map, layout_t* layout) {
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

/*
  Ensures that <entry_pair_t> acts like an unordered pair
  in associative containers.
*/
namespace std {
template <> struct hash<entry_pair_t> {
  inline size_t operator()(const entry_pair_t& v) const {
    std::hash<entry_t> hash_fn;
    return hash_fn(v.first) ^ hash_fn(v.second);
  }
};

template <> struct equal_to<entry_pair_t> {
  bool operator()(const entry_pair_t& lhs, const entry_pair_t& rhs) const {
    return ((lhs.first == rhs.first && lhs.second == rhs.second)
            || (lhs.first == rhs.second && lhs.second == rhs.first)) ? true : false;
  }
};
}

class Analysis {
 public:
  Analysis();

  void trace_hash_access(entry_index_t entry_index);

  void remove_entry(entry_index_t entry_index);

  std::vector<layout_t> getLayouts();

 private:
  // Verbosity information
  static std::ostream& err;
  bool DEBUG = false;
  bool PRINT = false;

  // Staging information
  enum stage_t { SAMPLE_STAGE, TRACE_STAGE };
  stage_t current_stage;

  typedef void (Analysis::*fptr)(entry_index_t);
  fptr current_stage_fn;

  uint32_t analysis_count_down;
  uint32_t trace_stage_count;

  // Sampling information
  entry_vec_t<entry_t> analysis_vec;
  entry_set_t<entry_t> analysis_set;
  entry_set_t<entry_t> remove_set;

  // Affinity information
  std::map<entry_t, window_t> window_list;
  std::unordered_map<entry_t, window_hist_t> singlFreq;
  std::unordered_map<entry_pair_t, window_hist_t> jointFreq;

  // Layout information
  std::unordered_map<entry_t, layout_t*> layout_map;

  std::vector<affinity_pair_t> get_affinity_pairs();

  void reorder_stage();
  void trace_stage(entry_index_t entry_index);
  void sample_stage(entry_index_t entry_index);
  void transition_stage();

  void add_compress_update(const entry_t& entry, bool analysis);
};

#endif
