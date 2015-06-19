#ifndef TRACER_HPP
#define TRACER_HPP

#include <iostream>
#include <algorithm>
#include <unordered_set>
#include <vector>

#include "AffinityAnalysis.hpp"

//affinity representation of a hash table entry
template<typename T, typename D>
class Tracer {
 public:
  Tracer(T* t) : root(t), analyzer(t), layout(analyzer) {}
  ~Tracer() {}

  void record(D* data) {
    analyzer.trace_hash_access(data - root->entries, data->get_analysis_bit());
  }

  void remove(D* data) {
    analyzer.remove_entry(root, data - root->entries);
  }

  std::vector<std::vector<entry_index_t>> non_linear_results() {
    auto i_layout = layout.find_affinity_layout();
    auto f_layout = std::vector<std::vector<entry_index_t>>(i_layout.size());
    unsigned i = 0;

    std::for_each(i_layout.begin(), i_layout.end(),
    [&](typename Layout<T, D>::layout_t& ls) {
      std::for_each(ls.begin(), ls.end(),
      [&] (entry_t<T>& e) {
        f_layout[i].push_back(e.entry_index);
      });
      ++i;
    });

    return f_layout;
  }

  std::vector<entry_index_t> results() {
    auto i_layout = layout.find_affinity_layout();
    auto f_layout = std::vector<entry_index_t>();

    // Randomly order layouts.
    std::random_shuffle(i_layout.begin(), i_layout.end());

    // Flatten randomly ordered layouts.
    std::for_each(i_layout.begin(), i_layout.end(),
    [&](typename Layout<T, D>::layout_t& ls) {
      std::for_each(ls.begin(), ls.end(),
      [&] (entry_t<T>& e) {
        f_layout.push_back(e.entry_index);
      });
    });

    return f_layout;
  }

 private:
  T const* const root;
  Analysis<T, D> analyzer;
  Layout<T, D> layout;
};

#endif
