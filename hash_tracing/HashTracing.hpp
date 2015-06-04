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

/*
#ifdef NOT_RUBY
#include "regint.h"
#include "st.h"
#else
#include "ruby/ruby.h"
#endif
*/
#include "../hash_table.h"


typedef std::pair<ptrdiff_t,unsigned> ah_map_id;

typedef uint16_t wsize_t;
wsize_t max_fpdist;
wsize_t max_fpdist_ind;
uint16_t analysis_set_size;

uint32_t timestamp = 0;

//affinity representation of a hash table entry
struct hash_t
{
  ah_table *table_ptr;

  ptrdiff_t entry_index;

  hash_t() {}

  hash_t(ah_table *a, int b): table_ptr(a), entry_index(b) {}

  hash_t(const hash_t &hentry): table_ptr(hentry.table_ptr), entry_index(hentry.entry_index) {}

  bool operator == (const hash_t &rhs) const
  {
    return table_ptr==rhs.table_ptr && entry_index==rhs.entry_index;
  }

  bool operator < (const hash_t &rhs) const
  {
    if(table_ptr < rhs.table_ptr)
      return true;
    else if(table_ptr > rhs.table_ptr)
      return false;

    if(entry_index < rhs.entry_index)
      return true;

    return false;

  }

  friend std::ostream& operator << (std::ostream& out, const hash_t& obj)
  {
    out << "(" << std::setbase(16) << obj.table_ptr << ", " << obj.entry_index << ")" << std::setbase(10);
    return out;
  }
};

namespace std {
  template <> struct hash<hash_t>
  {
    size_t              
      operator()(const hash_t& __val) const noexcept
      {
	size_t const h1 ( std::hash<ah_table*>()(__val.table_ptr) );
	size_t const h2 ( std::hash<ptrdiff_t>()(__val.entry_index) );
	return h1 ^ ((h2 << 1) << 1);
      }
  };

  template <> struct hash<const hash_t>
  {
    size_t              
      operator()(const hash_t& __val) const noexcept
      {
	size_t const h1 ( std::hash<ah_table*>()(__val.table_ptr) );
	size_t const h2 ( std::hash<ptrdiff_t>()(__val.entry_index) );
	return h1 ^ ((h2 << 1) << 1);
      }
  };

}




typedef std::unordered_map<const hash_t, uint32_t> timestamp_map_t;

typedef std::unordered_set<const hash_t> entry_set_t;
typedef std::vector <hash_t> entry_vec_t;
typedef std::list<hash_t> entry_list_t;

typedef std::pair<hash_t,uint32_t> hist_pair_t;
typedef std::vector< hist_pair_t > hist_vec_t;

struct wcount_t
{

  uint32_t excluded_windows;

  std::vector<uint32_t> common_windows;

  uint32_t all_windows;

  wcount_t(): excluded_windows(0), all_windows(0)
  {
    common_windows = std::vector<uint32_t>(max_fpdist_ind+1);
  }

  wcount_t& operator+=(const wcount_t& rhs)
  {
    this->excluded_windows+=rhs.excluded_windows;
    std::transform(rhs.common_windows.begin(),rhs.common_windows.end(), this->common_windows.begin(),this->common_windows.begin(),std::plus<uint32_t>());
    this->all_windows += rhs.all_windows;
    return *this;
  }

  friend std::ostream& operator << (std::ostream& out, const wcount_t& obj )
  {
    out << "excluded: " << obj.excluded_windows;
    out << "\nsum of common: " << obj.all_windows;
    out << "\ncommon:\n";
    out << "size: "<<obj.common_windows.size() << "\n";

    uint32_t check_sum=0;
    for(const auto& count : obj.common_windows)
    {
      out << count << "\t";
      check_sum+=count;
    }

    assert(obj.all_windows==check_sum);


    out << "\n----------------------------\n";

    return out;
  }

  uint32_t get_affinity() const{
    return std::accumulate(common_windows.begin(),common_windows.end(),0);
  }

};

typedef std::unordered_map <const hash_t, wcount_t> wcount_map_t;



struct all_wcount_t
{
  uint32_t potential_windows;
  wcount_map_t wcount_map;
  all_wcount_t(uint32_t _pw): potential_windows(_pw) {}
  all_wcount_t(): potential_windows(0)
  {
    wcount_map = wcount_map_t();
  }

  friend std::ostream& operator << (std::ostream& out, const all_wcount_t& obj)
  {
    out << "potential windows: " << obj.potential_windows;
    out << "\nwindow count map:\n";

    for(const auto& wcount_pair : obj.wcount_map)
    {
      out << wcount_pair.first << "\n";
      out << wcount_pair.second << "\n";
    }
    return out;
  }



};

typedef std::unordered_map <const hash_t, all_wcount_t> all_wcount_map_t;

std::ostream& operator << (std::ostream& out, const all_wcount_map_t& m)
{
  for (const auto& all_wcount_pair: m)
  {
    out << "************************\n";
    out << all_wcount_pair.first << "\n";
    out << all_wcount_pair.second << "\n";
  }
  return out;
}

struct window_t
{
  entry_vec_t owners;
  wsize_t wsize;
  wsize_t capacity;
  uint32_t start;
  //entry_list_t partial_entry_list;

  window_t():wsize(0)
  {
    owners.reserve(analysis_set_size);
  }

  window_t(const hash_t& entry):wsize(1), capacity(1), start(timestamp)
  {
    owners.reserve(analysis_set_size);
    owners.push_back(entry);
  }

  ~window_t()
  {
    owners.clear();
  }

  void push_front(const hash_t& entry)
  {
    wsize++;
  }


  void erase(const entry_list_t::iterator& it)
  {
    //partial_entry_list.erase(it);
    wsize--;
  }

  bool empty()
  {
    return wsize == 0;
  }

  void merge_owners(const window_t& other)
  {
    entry_vec_t::iterator eit_begin = owners.begin();
    entry_vec_t::iterator eit_end = owners.end();
    for(const auto &e: other.owners)
    {
      if(std::find(eit_begin,eit_end,e)==eit_end)
	owners.push_back(e);
    }
  }


  friend std::ostream& operator << (std::ostream& out, const window_t& obj)
  {
    out << "entries: ";
    /*for(const auto &entry: obj.partial_entry_list)
      out << entry << " ";
      */
    out << "\nowners: " ;

    for(const auto& entry: obj.owners)
      out << entry << " ";

    out << "\n:size: " << obj.wsize << "\n";
    return out;
  }

};

typedef std::list<window_t> window_list_t;


std::ostream& operator << (std::ostream& out, const window_list_t& wlist)
{
  for (const auto& window: wlist)
  {
    out << window << "\n";
  }
  return out;
}


struct wlist_it_pair_t
{
  window_list_t::iterator window_it;
  entry_list_t::iterator entry_it;

  wlist_it_pair_t() {}

  wlist_it_pair_t(const window_list_t::iterator& wit, const entry_list_t::iterator& eit)
    :window_it (wit), entry_it (eit) {}
};

typedef std::unordered_map <const hash_t, window_list_t::iterator> window_iterator_map_t;
typedef std::unordered_map <const hash_t, entry_list_t::iterator > entry_iterator_map_t;





hist_pair_t get_hist_entry (const std::pair<const hash_t, wcount_t> &p)
{
  return hist_pair_t (p.first,p.second.all_windows);
}

/*
 * We need to sort in descending order. Thus, we flip the order
 */
bool hist_pair_cmp (const hist_pair_t &e1, const hist_pair_t &e2)
{
  return (e1.second > e2.second);
}

uint32_t hist_pair_add (const uint32_t psum, const hist_pair_t &e)
{
  return psum+e.second;
}

struct affinity_pair_t {
  hash_t lentry, rentry;
  uint32_t affinity;

  affinity_pair_t(hash_t le, hash_t re, uint32_t affinity): lentry(le), rentry(re), affinity(affinity){}

  bool operator < (const affinity_pair_t affinity_pair) const {
    return (affinity > affinity_pair.affinity);
  }

  friend std::ostream& operator << (std::ostream& out, const affinity_pair_t affinity_pair)
  {
    out << "[" << affinity_pair.lentry << "," << affinity_pair.rentry << "]:" << affinity_pair.affinity;
    return out;
  }

};


template <class T> 
class Layout: public std::deque <T> {
  public:
    static std::unordered_map <T, Layout<T>*> LayoutMap;

    bool dumped = false;

    Layout(const T& fentry): std::deque<T>() {
      this->push_back(fentry);
    }

    void merge(Layout* layout){
      for(const auto& entry: *layout){
	this->push_back(entry);
	LayoutMap[entry]=this;
      }
      delete layout;
    }

    friend std::ostream& operator << (std::ostream& out, const Layout& layout){
      for(T entry: layout)
	out << entry << " ";

      out << "\n";
      return out;
    }

    static std::vector<Layout> getLayouts(){
      std::vector<Layout> layouts;
      for(auto layout_pair: LayoutMap)
	if(!layout_pair.second->dumped){
	  layouts.push_back(*layout_pair.second);
	  //layout_os << *layout_pair.second;
	  layout_pair.second->dumped = true;
	}

      return layouts;

    }

};

template <class T>  std::unordered_map <T, Layout<T> *> Layout<T>::LayoutMap;




extern std::vector<Layout<hash_t>> find_affinity_layout();
void affinity_at_exit_handler();
extern "C" void init_affinity_analysis();
void update_stage_affinity(const hash_t&, const window_list_t::iterator&);
extern "C" void trace_hash_access(ah_table *, ptrdiff_t, bool);
extern "C" void remove_table_analysis(ah_table *);
extern "C" void remove_entry(ah_table *, ptrdiff_t entry_index);
