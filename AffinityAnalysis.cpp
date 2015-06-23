#include "AffinityAnalysis.hpp"

/*
  Class: Affinity

  Public member functions: <Analysis>, <remove_entry>,
  <getLayouts>, and <trace_hash_access>
*/

std::ostream& Analysis::err(std::cerr);

Analysis::Analysis() {
  srand(time(NULL));

  // get prepared for the first analysis_set_sampling round
  analysis_count_down = analysis_sampling_time;
  current_stage_fn = &Analysis::sample_stage;
  current_stage = SAMPLE_STAGE;

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
  trace_stage_count = 0;
}

/*
  Function: remove_entry

  Removes a <entry_t> element from the analysis entirely.

  Effects:
    <entry_index> is added to <delete_set> to allow
    for it's lazy deletion from <affinity_map>.

    <entry_index> is removed from <window_list>, <analysis_vec>,
    <analysis_set>, and <timestamp_map> if applicable. This doesn't
    need to happen simply ignore <entry_index> while generating
    pairings. Still remove form <analysis_set> though; maybe.
*/
void Analysis::remove_entry(entry_index_t entry_index) {
  remove_set.emplace(entry_index);
}

/*
  Function: modify_entry

  Changes the uid of an entry_t in the analysis from o to n.

  Effects:
    Modifies <analysis_vec>, <analysis_set>, <timestamp_map>,
    <decay_map>, and <window_list> changing <entry_mappings>[i].first
    to <entry_mappings>[i].second for all i.

void Analysis::modify_entries(std::vector<std::pair<entry_index_t, entry_index_t>> entry_mappings) {
  for(const auto &p : entry_mappings) {

  }
}
*/

std::vector<layout_t> Analysis::getLayouts() {
  std::vector<layout_t> layouts;

  for (auto layout_pair : layout_map)
    if (!layout_pair.second->dumped) {
      layouts.push_back(*layout_pair.second);
      // layout_os << *layout_pair.second;
      layout_pair.second->dumped = true;
    }

  return layouts;
}


void Analysis::trace_hash_access(entry_index_t entry_index) {
  timestamp++;

  // Switch to new stage if the current has ended.
  if (!analysis_count_down--)
    transition_stage();

  // Process entry according to current stage.
  (this->*current_stage_fn)(entry_index);
}

/*
  Class: Analysis

  Private member functions: <trace_stage>, <sample_stage>,
  <reorder_stage>, <transition_stage>, <update_affinity>,
  and <add_compress_update>
*/

/*
  Function: transition_stages

  Effects:
    Changes <current_stage> and <current_stage_fn> to
    the current stage.
*/
void Analysis::transition_stage() {
  switch (current_stage) {
    case SAMPLE_STAGE: { // -> TRACE_STAGE
      if (DEBUG) {
        err << "End of the sampling stage, analyzed entries are:\n";

        for (const auto& a_entry : analysis_vec)
          err << a_entry << "\n";
        err << "\n";
      }

      // For constant time lookup.
      for (const auto& e : analysis_vec)
        analysis_set.insert(e);

      analysis_count_down = analysis_stage_time;
      current_stage_fn = &Analysis::trace_stage;
      current_stage = TRACE_STAGE;
    } break;

    case TRACE_STAGE: { // -> SAMPLE_STAGE
      if (DEBUG) {
        err << "Analysis stage finishes now!\n";
        err.flush();
      }

      // Clear the exluded entry set and the analysis entry set
      analysis_vec.clear();
      analysis_set.clear();
      window_list.clear();
      timestamp_map.clear();

      // Enter a reorder stage every 256 trace stages
      if (trace_stage_count++ % 256 == 0)
        reorder_stage();

      // TODO: Change analysis_set to new ids. and manage reordering stages.
      analysis_count_down = analysis_sampling_time;
      current_stage_fn = &Analysis::sample_stage;
      current_stage = SAMPLE_STAGE;
    } break;
  }
}

/*
  Function: sample_stage

  Effects:
*/
void Analysis::sample_stage(entry_index_t entry_index) {
  entry_t entry(entry_index);

  // Fill in the analysis vector first
  if (analysis_vec.size() < analysis_set_size) {
    analysis_vec.push_back(entry);

  // Reservoir sampling
  } else {
    // TODO: Ask about increasing probablity as
    // analysis_sampling_time - analysis_count_down <= analysis_set_size
    int r = rand() % (analysis_sampling_time - analysis_count_down);
    if (r < analysis_set_size)
      analysis_vec[r] = entry;
  }
}

/*
  Function: trace_stage

  Effects:
*/
void Analysis::trace_stage(entry_index_t entry_index) {
  entry_t entry(entry_index);

  if (DEBUG) dump_window_list(err, window_list);

  add_compress_update(entry, (analysis_set.find(entry) != analysis_set.end()));
}

/*
  Function: reorder_stage

  A reorder stage generates and caches a layout based on
  affinities generated during the trace stage. During this
  stage pair-wise affinities of objects are also decreased
  if an object in the pair hasn't been accessed since the
  last reorder stage via <decay_map>. Beyond this

  Effects:
    Removes <delete_set> elements from <affinity_map> then
    clears <delete_set>.

    Removes <entry_t> elements from <afffinity_map> based on
    their decay due to <decay_map>.

    Removes unused entires from <decay_map>.

    Generates <layout_map>.
*/
void Analysis::reorder_stage() {
  layout_map.clear();

  std::vector<affinity_pair_t> all_affinity_pairs = get_affinity_pairs();

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
}

std::vector<affinity_pair_t<entry_t>> Analysis::get_affinity_pairs() {
  std::vector<affinity_pair_t> all_affinity_pairs;
  std::unordered_set<entry_t> touched;

  // Decay any affinities whose elements haven't been
  // called between the current and last reorder stage.
  for (auto& v : decay_map) {
    std::pair<bool, uint32_t>& e = v.second;
    if (!e.first) e.second += 1;
    e.first = false;

    touched.insert(v.first);
  }

  // Remove unused elements from decay_map.
  for (auto it = decay_map.begin(); it != decay_map.end();) {
    if (touched.find(it->first) == touched.end())
      it = decay_map.erase(it);
    else
      ++it;
  }

  for (auto& all_wcount_pair : affinity_map) {
    auto le = all_wcount_pair.first;

    if (remove_set.find(le) != remove_set.end()) {
      affinity_map.erase(le);
      continue;
    }

    auto& all_wcount = all_wcount_pair.second;
    for (auto& wcount_pair : all_wcount.wcount_map) {
      auto re = wcount_pair.first;
      uint32_t affinity = wcount_pair.second.get_affinity() >> decay_map[re].second;

      // Remove an affinity pair if it has decayed entirely or it's been removed.
      if (affinity == 0 || remove_set.find(re) != remove_set.end()) {
        all_wcount.wcount_map.erase(re);

      // Otherwise add it to all pairs.
      } else {
        all_affinity_pairs.emplace_back(le, re, affinity);
      }
    }
  }

  // All entry_t's have been removed.
  remove_set.clear();

  return all_affinity_pairs;
}

void Analysis::update_affinity(const entry_vec_t<entry_t>& entry_vec, const entry_t& entry, int fpdist_ind) {
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
