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

  void print_affinity_info() { }
/*
  void print_frequency_info() {
    std::unordered_map<entry_t, std::unordered_map<entry_t, window_hist_t>> hist;

    for (const auto& wcount_pair : analyzer.jointFreq) {
      const auto& entry_pair = wcount_pair.first;
      const auto& window_hist = wcount_pair.second;

      // Gather jointFreq information for sampled entries
      if (analyzer.analysis_set.find(entry_pair.first) != analyzer.analysis_set.end()) {
        for(wsize_t w_ind = 0; w_ind < max_fpdist_ind; w_ind += 1)
          hist[entry_pair.first][entry_pair.second][w_ind] += window_hist[w_ind];
      } else {
        for(wsize_t w_ind = 0; w_ind < max_fpdist_ind; w_ind += 1)
          hist[entry_pair.second][entry_pair.first][w_ind] += window_hist[w_ind];
      }
    }

    for (const auto& sampled_entry : hist) {
      const auto& first_entry = sampled_entry.first;
      const auto& freq_info = sampled_entry.second;

      std::cout << "\n――――――――――――――――――――――――――――――――\n";

      // Print single freq information for first_entry

      // One tab before entry name, two before jointFreq
      for (const auto &other_entry : freq_info) {
        const auto& second_entry = other_entry.first;
        const auto& entry_hist = other_entry.second;

        // Print joint freq information for <first_entry, second_entry>

      }

      std::cout << "\n――――――――――――――――――――――――――――――――\n";
    }
  }
*/
 private:
  T const* const root;
  Analysis analyzer;
};

#endif
