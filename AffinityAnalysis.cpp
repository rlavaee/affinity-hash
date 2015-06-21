#include "AffinityAnalysis.hpp"

Layout::Layout(Analysis& a) : analysis(a) {}

std::vector<layout_t> Layout::find_affinity_layout() {
  layout_map.clear();

  std::vector<affinity_pair_t> all_affinity_pairs = analysis.get_affinity_pairs();

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
        rLayoutPair.first->second->merge(layout_map, lLayoutPair.first->second);
      else
        lLayoutPair.first->second->merge(layout_map, rLayoutPair.first->second);
    }
  }

  return getLayouts();
}

std::vector<layout_t> Layout::getLayouts() {
  std::vector<layout_t> layouts;

  for (auto layout_pair : layout_map)
    if (!layout_pair.second->dumped) {
      layouts.push_back(*layout_pair.second);
      // layout_os << *layout_pair.second;
      layout_pair.second->dumped = true;
    }

  return layouts;
}


/** AFFINITY public **/
std::ostream &Analysis::err(std::cerr);

Analysis::Analysis() {
  srand(time(NULL));

  // get prepared for the first analysis_set_sampling round
  analysis_set_sampling = true;
  analysis_count_down = analysis_sampling_time;

  const char* e;
  if ((e = getenv("ST_HASH_DEBUG")) && atoi(e) == 1)
    DEBUG = true;

  if ((e = getenv("ST_HASH_PRINT")) && atoi(e) == 1)
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

void Analysis::trace_hash_access(entry_index_t entry_index, bool analysis_bit) {
  timestamp++;

  // Count down and transit into the next stage if count drops to zero.
  if (!analysis_count_down--) {
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
      // analysis_set = entry_set_t(analysis_vec.begin(),analysis_vec.end());
      for (const auto& e : analysis_vec)
        analysis_set.insert(e);
        //table_ptr->entries[e.entry_index].set_analysis_bit(1);

      analysis_count_down = analysis_stage_time;
    } else {
      if (DEBUG) {
        err << "Analysis stage finishes now!\n";
        err.flush();
      }

      // Clear the exluded entry set and the analysis entry set
      analysis_vec.clear();
      analysis_set.clear();
      window_list.clear();
      timestamp_map.clear();

      //for (const auto& e : analysis_vec)
      //  table_ptr->entries[e.entry_index].set_analysis_bit(0);

      // Get ready for the next analysis_set_sampling round
      analysis_set_sampling = true;
      analysis_count_down = analysis_sampling_time;

      // TODO: Change analysis_set to new ids. and manage reordering stages.
    }

  } else {
    entry_t entry(entry_index);

    if (analysis_set_sampling) {
      // Fill in the analysis vector first
      if (analysis_vec.size() < analysis_set_size) {
        analysis_vec.push_back(entry);
      } else {
        // Reservoir sampling
        int r = rand() % (analysis_sampling_time - analysis_count_down);
        if (r < analysis_set_size)
          analysis_vec[r] = entry;
      }
    } else {
      if (DEBUG) dump_window_list(err, window_list);

      add_compress_update(entry, (analysis_set.find(entry) != analysis_set.end()));
    }
  }
}

void Analysis::remove_entry(entry_index_t entry_index) {
  // NOTE: Nothing seems to be done about this set.
  remove_set.insert(entry_t(entry_index));

  // Remove from all analysis structures
  // NOTE: the index from the interior affinity_map
  // is removed lazily in the generate_all_affinity_pairs().
  std::remove_if(analysis_vec.begin(), analysis_vec.end(),
    [&](entry_t &e) { return e.entry_index == entry_index; });

  // std::remove_if(window_list)
}


/** AFFINITY private **/

std::vector<affinity_pair_t<entry_t>> Analysis::get_affinity_pairs() {
  std::vector<affinity_pair_t> all_affinity_pairs;

  // Decay any affinities whose elements haven't been called
  // during the last generation phase.
  for_each(decay_map.begin(), decay_map.end(),
    [](decay_map_t<entry_t>::value_type &v) {
      std::pair<bool, uint32_t> &e = v.second;
      if (!e.first) e.second -= 1;

      // All affinities should decay if they aren't accessed
      // before get_affinity_pairs() is called again.
      e.first = false;
  });

  for (auto& all_wcount_pair : affinity_map) {
    auto le = all_wcount_pair.first;
    auto& all_wcount = all_wcount_pair.second;
    for (auto& wcount_pair : all_wcount.wcount_map) {
      auto re = wcount_pair.first;
      uint32_t affinity = wcount_pair.second.get_affinity() >> decay_map[re].second;

      // Remove an affinity pair if it has decayed entirely.
      if (affinity == 0) {
        all_wcount.wcount_map.erase(re);

      // Otherwise add it to all pairs.
      } else {
        all_affinity_pairs.emplace_back(le, re, affinity);
      }
    }
  }

  return all_affinity_pairs;
}

void Analysis::update_affinity(const entry_vec_t<entry_t>& entry_vec /* owners */, const entry_t& entry, int fpdist_ind) {
  decay_map[entry] = std::make_pair(true, 0);

  for (const auto& a_entry : entry_vec) {
    wcount_t& ref = affinity_map[a_entry].wcount_map[entry];
    ref.common_windows.at(fpdist_ind)++;
    ref.all_windows++;
  }
}

void Analysis::add_compress_update(const entry_t& entry, bool analysis) {
  typename window_list_t<entry_t>::iterator window_it = window_list.begin();

  if (analysis) {
    // NOTE: this will call the constructor of window_t
    window_list.emplace_front(entry, timestamp);  // remember to set the capacity as well
    auto res = affinity_map.emplace(entry, 0);
    res.first->second.potential_windows++;
  }

  if (window_it == window_list.end())
    return;

  auto res = timestamp_map.emplace(entry, timestamp);

  int entry_ts = -1;  // TODO: Ask why this is an int not timestamp_t.
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

    if (prev_window_it->start <= entry_ts)  // nothing to be done after this.
      return;
  }
}
