#include <unordered_map>
#include <unordered_set>
#include <list>
#include <tuple>
#include <assert.h>
#include <random>
#include "cache.h"

using namespace std;

typedef tuple<long, long> object_t; // objectid, size
typedef list<object_t>::iterator list_iterator_t;

/*
  LRU: Least Recently Used eviction
*/

class LRUCache: public Cache {
public:
  // construct and destroy LRU
  LRUCache() {}
  ~LRUCache(){}
  virtual void I_am() { cout << "I am lru\n"; }

  // normal cache functions
  virtual bool lookup (const long cur_req) const {
    return(cache_map.count(cur_req)>0);
  }
  virtual void evict (const long cur_req) {
    if(lookup(cur_req)) {
      list_iterator_t lit = cache_map[cur_req];
      cache_map.erase(cur_req);
      current_size -= get<1>(*lit);
      cache_list.erase(lit);
    }
  }
  virtual bool request(const long cur_req, const long long size) {
    unordered_map<long, list_iterator_t>::const_iterator it;
    it = cache_map.find(cur_req);
    if(it != cache_map.end())
      {
	if (size == get<1>(*(it->second))) {
	  // hit and consistent size
	  LOG("h",0,cur_req,size);
	  hit(it, size);
	  return(true);
	}
	else { // inconsistent size -> treat as miss and delete inconsistent entry
	  evict(cur_req);
	}
      }
    miss(cur_req, size);
    return(false);
  }
  
protected:
  // list for recency order
  list<object_t> cache_list;
  // map to find objects in list
  unordered_map<long, list_iterator_t> cache_map;

  // main functionality: deal with hit and miss
  virtual void hit(unordered_map<long, list_iterator_t>::const_iterator it, long size) {
    cache_list.splice(cache_list.begin(), cache_list, it->second);
    Cache::hit(size);
  }

  virtual void miss(const long cur_req, const long size) {
    // object feasible to store?
    if(size >= cache_size) {
      LOG("L",0,size,cache_size);
      return;
    }
    list_iterator_t lit;  
    // check eviction needed
    while (current_size + size > cache_size) {
      // evict least popular (i.e. last element)
      lit = cache_list.end();
      lit--;
      long esize = get<1>(*lit);
      LOG("e",current_size,get<0>(*lit),esize);
      current_size -= esize;
      cache_map.erase(get<0>(*lit));
      cache_list.erase(lit);
    }
    // admit new object
    cache_list.push_front(object_t(cur_req, size)); 
    cache_map[cur_req] = cache_list.begin();
    current_size += size;
    LOG("a",current_size,cur_req,size);
  }
};
static Factory<LRUCache> factoryLRU("LRU");




/*
  FIFO: First-In First-Out eviction
*/

class FIFOCache: public LRUCache {
public:
  FIFOCache() {}
  ~FIFOCache(){}
  virtual void I_am() { cout << "I am fifo\n"; }

protected:
  virtual void hit(unordered_map<long, list_iterator_t>::const_iterator it, long size) {
    Cache::hit(size);
  }
};
static Factory<FIFOCache> factoryFIFO("FIFO");




/*
  S2LRU
*/

class S2LRUCache: public LRUCache {
protected:
  LRUCache previous;

  virtual void miss(const long cur_req, const long size) {
    // object feasible to store?
    if(size >= cache_size) {
      LOG("error",0,size,cache_size);
      return;
    }
    list_iterator_t lit;  
    // check eviction needed
    while (current_size + size > cache_size) {
      // evict least popular (i.e. last element)
      lit = cache_list.end();
      lit--;
      long esize = get<1>(*lit);
      LOG("e",current_size,get<0>(*lit),esize);
      previous.request(get<0>(*lit), esize); // move to front of previous cache
      current_size -= esize;
      cache_map.erase(get<0>(*lit));
      cache_list.erase(lit);
    }
    // admit new object
    cache_list.push_front(object_t(cur_req, size)); 
    cache_map[cur_req] = cache_list.begin();
    current_size += size;
    LOG("a",current_size,cur_req,size);
  }

public:
  S2LRUCache(): previous() {}
  ~S2LRUCache(){}
  virtual void setPar(int count, ...) {
    va_list args;
    assert(count==1);
    va_start(args, count);
    xx=va_arg(args, int); }


  void evict (const long cur_req) {
    previous.evict(cur_req);
    LRUCache::evict(cur_req);
  }

  bool lookup2nd (const long cur_req) {
    return (LRUCache::lookup(cur_req));
  }

  bool request (const long cur_req, const long size) {
    if(previous.lookup(cur_req)) { // found in previous layer
      previous.evict(cur_req); //delete in previous
      miss(cur_req, size); //admit to current
      LOG("h",101,cur_req,size);
      return(LRUCache::request(cur_req, size)); //hit in current
    } else if(LRUCache::lookup(cur_req)) {// found in current
      LOG("h",102,cur_req,size);
      return(LRUCache::request(cur_req, size)); //hit in current
    }
    if(previous.request(cur_req, size)) { //request in all prev. layers
      Cache::hit(size); // hidden hit
      return(true);
    }
    return(false);
  }
};
static Factory<S2LRUCache> factoryS2LRU("S2LRU");

