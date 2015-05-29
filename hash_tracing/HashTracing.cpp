#include "HashTracing.hpp"
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <unistd.h>
#include <new>
#include <string.h>
#include <vector>
#include <iostream>


/*
 * Debugging
 */

std::ofstream err("debug.out");
bool debug = false;
bool print = false;
bool layout = true;


/*
 * Analysis data structures
 */
bool analysis_set_sampling;
uint32_t count_down;
const uint32_t analysis_sampling_time = 1 << 8;
const uint32_t analysis_stage_time = 1 << 12;
entry_vec_t analysis_vec;
entry_set_t analysis_set;
entry_set_t excluded_set;
entry_set_t global_remove_set;



/*
 * Affinity data structures
 */
all_wcount_map_t affinity_map;
all_wcount_map_t stage_affinity_map;


/*
 * Trace list data structures
 */
wsize_t window_list_size;
window_list_t window_list;
window_iterator_map_t window_iterator_map;
entry_iterator_map_t entry_iterator_map;

timestamp_map_t timestamp_map;
const int sampleMask = 0x0;


/*
 * Iterators
 */
window_list_t::iterator window_list_it;
entry_list_t::iterator entry_list_it;
window_list_t::iterator window_it;
window_iterator_map_t::iterator window_found;

/*
 * Saves the affinity data into a file
 */
void save_affinity_into_file(const char * affinity_file_path)
{
  std::ofstream aff_file(affinity_file_path);
  aff_file << "max window size: "<< max_fpdist << "\n";
  aff_file << affinity_map;
}





void affinity_at_exit_handler()
{
  if(print)
    save_affinity_into_file("graph.babc");

  if(layout)
    find_affinity_layout();
}



void dump_window_list (std::ostream &out, window_list_t &wlist)
{

  out << "trace list:------------------------------------------\n" ;
  out << "size: " << window_list_size << "\n";
  out << wlist ;
  out << "---------------------------------------------\n";
}



extern "C" void init_affinity_analysis()
{
  window_list_size=0;
  srand(time(NULL));

  // get prepared for the first analysis_set_sampling round
  analysis_set_sampling = true;
  count_down = analysis_sampling_time;
  atexit(affinity_at_exit_handler);

  const char *e = getenv("ST_HASH_DEBUG");

  if(e && *e)   /*Set and not empty*/
  {
    debug = true;
  }

  e = getenv("ST_HASH_PRINT");

  if(e && *e)   /*Set and not empty*/
  {
    print = true;
  }

  if((e = getenv("ST_HASH_ANALYSIS_SIZE")))
    analysis_set_size = atoi(e);
  else /* default value; just to make sure it will be working for more than 1*/
    analysis_set_size = 2;

  if((e = getenv("ST_HASH_MAX_FPDIST_IND")))
    max_fpdist_ind = atoi(e);
  else /* default value; just something non-trivial */
    max_fpdist_ind = 10;

  max_fpdist = 1 << max_fpdist_ind;


}
/*
   void compress_window_list(){
   window_list_t::iterator window_it = window_list.begin();

   wsize_t fpdist = 1;

   while(window_it !=  window_list.end()){

   if(fpdist > max_fpdist){
   window_it = window_list.erase(window_it);
   continue;
   }

   window_list_t::iterator prev_window_it = window_it;
   window_it++;

   while((window_it != window_list.end()) && (prev_window_it->capacity >= prev_window_it->wsize + window_it->wsize)){
   fpdist += window_it->wsize;
   prev_window_it->wsize += window_it->wsize;
   prev_window_it->merge_owners(*window_it);
   prev_window_it->start = window_it->start;

   window_it = window_list.erase(window_it);
   }
   }
   }
   */

void update_affinity(const entry_vec_t& entry_vec, const hash_t& entry, int fpdist_ind)
{
  for(const auto &a_entry : entry_vec)
  {
    wcount_t& ref = affinity_map[a_entry].wcount_map[entry];
    ref.common_windows.at(fpdist_ind)++;
    ref.all_windows++;
  }
}



void add_compress_update(const hash_t& entry, bool analysis)
{


  window_list_t::iterator window_it = window_list.begin();

  if(analysis)
  {
    window_list.emplace_front(entry); /* remember to set the capacity as well */
    auto res = affinity_map.emplace(entry,0);
    res.first->second.potential_windows++;
  }

  if(window_it == window_list.end())
    return;

  auto res = timestamp_map.emplace(entry,timestamp);

  int entry_ts = -1;
  if(!res.second)
  {
    entry_ts = res.first->second;
    res.first->second = timestamp;
  }

  if(window_it->start <= entry_ts)
    return;

  wsize_t fpdist;
  if(analysis)
  {
    fpdist = 1;
  }
  else
  {
    window_it->wsize++;
    fpdist = 0;
  }


  window_list_t::iterator prev_window_it;
  wsize_t exp_fpdist = 1;
  int exp_fpdist_ind = 0;

  while(true)
  {

    prev_window_it = window_it;
    window_it++;

    prev_window_it->capacity = fpdist+1;

compress:

    if(window_it == window_list.end())
    {
      fpdist += prev_window_it->wsize;

      if(fpdist > max_fpdist)
        return;

      while(fpdist > exp_fpdist)
      {
        exp_fpdist <<= 1;
        exp_fpdist_ind++;
      }
      update_affinity(prev_window_it->owners,entry,exp_fpdist_ind);
      return;
    }


    if(window_it->start <= entry_ts)
    {
      window_it->wsize--;
    }

    if(fpdist+1 >= prev_window_it->wsize + window_it->wsize)     /* fpdist+1 = prev_window_it->capacity */
    {
      prev_window_it->wsize += window_it->wsize;

      prev_window_it->merge_owners(*window_it);
      prev_window_it->start = window_it->start;

      /* may leak memory */
      window_it = window_list.erase(window_it);
      goto compress;
    }
    else
    {
      fpdist += prev_window_it->wsize;

      if(fpdist > max_fpdist)
        return;

      while(fpdist > exp_fpdist)
      {
        exp_fpdist <<= 1;
        exp_fpdist_ind++;
      }
      update_affinity(prev_window_it->owners,entry,exp_fpdist_ind);
    }

    if(prev_window_it->start <= entry_ts) /* nothing to be done after this */
      return;


  }
}


extern "C" void trace_hash_access(ah_table * tbl, ptrdiff_t entry_index, bool analysis_bit)
{

  /*
   * Count down and transit into the next stage if count drops to zero.
   */
// return;

  int r=rand();
  if((r & sampleMask)!=0)
    return;


  timestamp++;

  if(!count_down--)
  {

    if(analysis_set_sampling)
    {
      if(debug)
      {
        err << "End of the sampling stage, analyzed entries are:\n";
        for(const auto &a_entry : analysis_vec)
          err << a_entry << "\n";
        err << "\n";
      }
      analysis_set_sampling = false;
      /*
       * for up to few hundred elements, lookup in vector is faster than in red-black tree:
       * 	http://cppwisdom.quora.com/std-map-and-std-set-considered-harmful
       *  analysis_set = entry_set_t(analysis_vec.begin(),analysis_vec.end());
       */
      for (const auto &e: analysis_vec)
        ah_set_analysis_bit(e.table_ptr,e.entry_index,1);
      count_down = analysis_stage_time;
    }
    else
    {
      if(debug)
      {
        err << "Analysis stage finishes now!\n";
        err.flush();
      }

      /*
       * Clear the exluded entry set and the analysis entry set
       analysis_set.clear();
       */
      analysis_vec.clear();

      window_list.clear();
      timestamp_map.clear();

      for(const auto &e: analysis_vec)
        ah_set_analysis_bit(e.table_ptr,e.entry_index,0);

      /*excluded_set.clear();*/

      /*
       * Get ready for the next analysis_set_sampling round
       */
      analysis_set_sampling = true;
      count_down = analysis_sampling_time;
    }

  }
  else
  {



    hash_t entry(tbl,entry_index);

    if(analysis_set_sampling)
    {

      /*
       * Fill in the analysis vector first
       */
      if(analysis_vec.size() < analysis_set_size)
        analysis_vec.push_back(entry);
      else
      {
        /*
         * reservoir sampling
         */
        int r = rand() % (analysis_sampling_time - count_down);
        if(r < analysis_set_size)
          analysis_vec[r] = entry;
      }
    }
    else
    {
      if(debug)
        dump_window_list(err,window_list);

      add_compress_update(entry,analysis_bit);
    }
  }

}



void remove_table_analysis(ah_table * tbl)
{
  entry_vec_t::iterator it=analysis_vec.begin();
  while(it!=analysis_vec.end())
  {
    if(it->table_ptr==tbl)
      it=analysis_vec.erase(it);
    else
      it++;
  }
}

void remove_entry(ah_table * tbl, ptrdiff_t entry_index)
{
  global_remove_set.insert(hash_t(tbl,entry_index));
}

void find_affinity_layout(){
  std::vector<affinity_pair_t> all_affinity_pairs;

  for(const auto& all_wcount_pair: affinity_map){
    auto le = all_wcount_pair.first;
    auto& all_wcount = all_wcount_pair.second;
    for(const auto& wcount_pair: all_wcount.wcount_map){
      auto re = wcount_pair.first;
      uint32_t affinity = wcount_pair.second.get_affinity();
      all_affinity_pairs.push_back(affinity_pair_t(le,re,affinity));
    }
  }

  std::sort(all_affinity_pairs.begin(),all_affinity_pairs.end());
  for(const auto& affinity_pair: all_affinity_pairs)
    err << affinity_pair << "\n";
}
