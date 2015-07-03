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
  analysis_count_down = analysis_sampling_period;
  current_stage_fn = &Analysis::sample_stage;
  current_stage = SAMPLE_STAGE;
  trace_stage_count = 0;

  const char* e;
  if ((e = getenv("ST_HASH_DEBUG")) && atoi(e) == 1)
    DEBUG = true;

  if ((e = getenv("ST_HASH_PRINT")) && atoi(e) == 1)
    PRINT = true;
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

      analysis_count_down = analysis_staging_period;
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

      // Enter a reorder stage every 256 trace stages
      // log(2 ^ 5 * 2 ^ 11) == 16
      if (trace_stage_count++ % analysis_reordering_period == 0) {
        reorder_stage();

        // Clear frequency information.
        singlFreq.clear();
        jointFreq.clear();
      }

      // TODO: Change analysis_set to new ids. and manage reordering stages.
      analysis_count_down = analysis_sampling_period;
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
    int r = rand() % (analysis_sampling_period - analysis_count_down);
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

std::vector<affinity_pair_t> Analysis::get_affinity_pairs() {
  std::vector<affinity_pair_t> all_affinity_pairs;
  all_affinity_pairs.reserve(jointFreq.size());

  for (const auto& wcount_pair : jointFreq) {
    const auto& entry_pair = wcount_pair.first;
    const auto& window_hist = wcount_pair.second;

    // Lazily remove datum.
    //if(remove_set.find(entry_pair.first) || remove_set.find(entry_pair.second)) {
    //  continue;
    //}

    avalue_t affinity = 0;

    for(wsize_t w_ind = 0; w_ind < max_fpdist_ind; w_ind += 1) {
      if(window_hist[w_ind] != 0) {
        if(std::max(singlFreq[entry_pair.first][w_ind], singlFreq[entry_pair.second][w_ind]) == 0)
          std::cout << "OVERFLOW!\n";
        affinity += ((max_fpdist_ind - w_ind) * window_hist[w_ind]) /
          std::max(singlFreq[entry_pair.first][w_ind], singlFreq[entry_pair.second][w_ind]);
      }
    }

    if(affinity > 0)
      all_affinity_pairs.emplace_back(entry_pair.first, entry_pair.second, affinity);
  }

  //remove_set.clear();

  return all_affinity_pairs;
}

inline wsize_t log2_ceil(const wsize_t in) {
  return ((sizeof(unsigned int) * 8) - __builtin_clz(in)) - 2;
}

void Analysis::add_compress_update(const entry_t& entry, bool analysis) {
  // Add or update window information for analyzed entry.
  if(analysis) {
    auto &window = window_list[entry];
    window.length = 1;
  }

  // decay_map[entry] = std::make_pair(true, 0);

  for(auto e = window_list.begin(); e != window_list.end();) {
    auto &owner  = e->first;
    auto &window = e->second;

    // Don't add to window that starts with same entry.
    if(owner == entry) {
      ++e;
      continue;
    }

    // Update window size.
    window.length += 1;
    wsize_t window_ind = log2_ceil(window.length);

    // Update frequency information.
    singlFreq[entry][window_ind] += 1;
    jointFreq[entry_pair_t(entry, owner)][window_ind] += 1;

    // If e's window exceeds the max remove element from window analysis.
    if(window.length == max_fpdist + 1) {
      e = window_list.erase(e);
    } else {
      ++e;
    }
  }
}
