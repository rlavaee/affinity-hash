#ifndef TRACER_HPP
#define TRACER_HPP

#include <iostream>
#include <algorithm>
#include <unordered_set>
#include <vector>

#include "AffinityAnalysis.hpp"

template<typename T, typename D>
class Tracer {
 public:
  Tracer(T* t) : root(t), analyzer() {}

  void record(D* data) {
    analyzer.trace_hash_access(data - root->entries);
  }

  void remove(D* data) {
    analyzer.remove_entry(root, data - root->entries);
  }

  std::vector<std::vector<entry_index_t>> non_linear_results() {
    auto i_layout = analyzer.getLayouts();
    auto f_layout = std::vector<std::vector<entry_index_t>>(i_layout.size());
    unsigned i = 0;

    for (auto& ls : i_layout) {
      for (auto& e : ls)
        f_layout[i].push_back(e);
      ++i;
    }

    return f_layout;
  }

  std::vector<entry_index_t> results() {
    auto i_layout = analyzer.getLayouts();
    auto f_layout = std::vector<entry_index_t>();

    // Randomly order layouts.
    std::random_shuffle(i_layout.begin(), i_layout.end());

    // Flatten randomly ordered layouts.
    for (auto& ls : i_layout)
      for (auto& e : ls)
        f_layout.push_back(e);

    return f_layout;
  }

 private:
  T const* const root;
  Analysis analyzer;
};

#endif
