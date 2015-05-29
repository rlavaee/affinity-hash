#ifndef HASHTRACING_H
#define HASHTRACING_H

#include <list>
#include <unordered_set>
#include <stdint.h>
#include <algorithm>
#include <assert.h>
#include <fstream>
#include <iomanip>
#include <unordered_map>
#include <vector>

/*
#ifdef NOT_RUBY
#include "regint.h"
#include "st.h"
#else
#include "ruby/ruby.h"
#endif
*/
#include "../hash_table.h"

// typedef std::pair<hash_object,unsigned> ah_map_id;

typedef uint16_t wsize_t;
wsize_t max_fpdist;
wsize_t max_fpdist_ind;
uint16_t analysis_set_size;

uint32_t timestamp = 0;

//affinity representation of a hash table entry
struct hash_t {
    ah_table *table_ptr;

    ptrdiff_t entry_index;

    // hash_object hash_val;

    //unsigned offset;

    hash_t() {};

    hash_t(ah_table *a, ptrdiff_t i): table_ptr(a), entry_index(i) {};

    hash_t(const hash_t &hentry): table_ptr(hentry.table_ptr), entry_index(hentry.entry_index) {}; 

    bool operator == (const hash_t &rhs) const {
        return table_ptr==rhs.table_ptr && entry_index == rhs.entry_index;
        //return table_ptr==rhs.table_ptr && hash_val==rhs.hash_val && offset == rhs.offset;
    }

    bool operator < (const hash_t &rhs) const {
        
        if(table_ptr < rhs.table_ptr)
            return true;
        else if(table_ptr > rhs.table_ptr)
            return false;

        if(entry_index < rhs.entry_index)
            return true;
        else
            return false;
        /*
        if(hash_val < rhs.hash_val)
            return true;
        else if(hash_val > rhs.hash_val)
            return false;

        return (offset < rhs.offset);
        */
    }

    friend std::ostream& operator << (std::ostream& out, const hash_t& obj) {
        out << "(" << std::setbase(16) << obj.table_ptr << ", " << obj.entry_index << ")" << std::setbase(10);
        return out;
    }
};

struct hash_t_hash {
    size_t operator()(hash_t const& hentry) const
    {
        size_t const h1 ( std::hash<ah_table*>()(hentry.table_ptr) );
        size_t const h2 ( std::hash<ptrdiff_t>()(hentry.entry_index) );
        //size_t const h3 ( std::hash<unsigned>()(hentry.offset) );
        //return h1 ^ (h3 ^ ((h2 << 1) << 1));
        
        return h1 ^ (h2 << 1);
    }
};

typedef std::unordered_map<hash_t, uint32_t, hash_t_hash> timestamp_map_t;

typedef std::unordered_set<hash_t, hash_t_hash> entry_set_t;
typedef std::vector <hash_t> entry_vec_t;
typedef std::list<hash_t> entry_list_t;

typedef std::pair<hash_t,uint32_t> hist_pair_t;
typedef std::vector< hist_pair_t > hist_vec_t;

struct wcount_t {

    uint32_t excluded_windows;

    std::vector<uint32_t> common_windows;

    uint32_t all_windows;

    wcount_t(): excluded_windows(0), all_windows(0) {
        common_windows = std::vector<uint32_t>(max_fpdist_ind+1);
    }

    wcount_t& operator+=(const wcount_t& rhs) {
        this->excluded_windows+=rhs.excluded_windows;
        std::transform(rhs.common_windows.begin(),rhs.common_windows.end(), this->common_windows.begin(),this->common_windows.begin(),std::plus<uint32_t>());
        this->all_windows += rhs.all_windows;
        return *this;
    }

    friend std::ostream& operator << (std::ostream& out, const wcount_t& obj ) {
        out << "excluded: " << obj.excluded_windows;
        out << "\nsum of common: " << obj.all_windows;
        out << "\ncommon:\n";
        out << "size: "<<obj.common_windows.size() << "\n";

        uint32_t check_sum=0;
        for(const auto& count : obj.common_windows) {
            out << count << "\t";
            check_sum+=count;
        }

        assert(obj.all_windows==check_sum);


        out << "\n----------------------------\n";

        return out;
    }

};

typedef std::unordered_map <const hash_t, wcount_t, hash_t_hash > wcount_map_t;



struct all_wcount_t {
    uint32_t potential_windows;
    wcount_map_t wcount_map;
    all_wcount_t(uint32_t _pw): potential_windows(_pw) {}
    all_wcount_t(): potential_windows(0) {
        wcount_map = wcount_map_t();
    }

    friend std::ostream& operator << (std::ostream& out, const all_wcount_t& obj) {
        out << "potential windows: " << obj.potential_windows;
        out << "\nwindow count map:\n";

        for(const auto& wcount_pair : obj.wcount_map) {
            out << wcount_pair.first << "\n";
            out << wcount_pair.second << "\n";
        }
        return out;
    }
};

typedef std::unordered_map <const hash_t, all_wcount_t , hash_t_hash > all_wcount_map_t;

std::ostream& operator << (std::ostream& out, const all_wcount_map_t& m) {
    for (const auto& all_wcount_pair: m) {
        out << "************************\n";
        out << all_wcount_pair.first << "\n";
        out << all_wcount_pair.second << "\n";
    }
    return out;
}

struct window_t {
    entry_vec_t owners;
    wsize_t wsize;
    wsize_t capacity;
    uint32_t start;
    //entry_list_t partial_entry_list;

    window_t():wsize(0) {
        owners.reserve(analysis_set_size);
    }

    window_t(const hash_t& entry):wsize(1), capacity(1), start(timestamp) {
        owners.reserve(analysis_set_size);
        owners.push_back(entry);
    }

    ~window_t() {
        owners.clear();
    }

    void push_front(const hash_t& entry) {
        wsize++;
    }

    void erase(const entry_list_t::iterator& it) {
        //partial_entry_list.erase(it);
        wsize--;
    }

    bool empty() {
        return wsize == 0;
    }

    void merge_owners(const window_t& other) {
        entry_vec_t::iterator eit_begin = owners.begin();
        entry_vec_t::iterator eit_end = owners.end();
        for(const auto &e: other.owners) {
            if(std::find(eit_begin,eit_end,e)==eit_end)
                owners.push_back(e);
        }
    }


    friend std::ostream& operator << (std::ostream& out, const window_t& obj) {
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


std::ostream& operator << (std::ostream& out, const window_list_t& wlist) {
    for (const auto& window: wlist) {
        out << window << "\n";
    }
    return out;
}


struct wlist_it_pair_t {
    window_list_t::iterator window_it;
    entry_list_t::iterator entry_it;

    wlist_it_pair_t() {}

    wlist_it_pair_t(const window_list_t::iterator& wit, const entry_list_t::iterator& eit)
        :window_it (wit), entry_it (eit) {}
};

typedef std::unordered_map <const hash_t, window_list_t::iterator, hash_t_hash > window_iterator_map_t;
typedef std::unordered_map <const hash_t, entry_list_t::iterator, hash_t_hash > entry_iterator_map_t;


hist_pair_t get_hist_entry (const std::pair<const hash_t, wcount_t> &p) {
    return hist_pair_t (p.first,p.second.all_windows);
}

/*
 * We need to sort in descending order. Thus, we flip the order
 */
bool hist_pair_cmp (const hist_pair_t &e1, const hist_pair_t &e2) {
    return (e1.second > e2.second);
}

uint32_t hist_pair_add (const uint32_t psum, const hist_pair_t &e) {
    return psum+e.second;
}


void affinity_at_exit_handler();
extern "C" void init_affinity_analysis();
void update_stage_affinity(const hash_t&, const window_list_t::iterator&);
extern "C" void trace_hash_access(ah_table *, ptrdiff_t, bool);
extern "C" void remove_table_analysis(ah_table *);

#endif
