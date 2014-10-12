#ifndef _OHMETACACHE_H_X_INCLUDED_
#define _OHMETACACHE_H_X_INCLUDED_

#include <string>
#include <unordered_map>

typedef std::unordered_map<std::string, std::string> mcache_type;

/** 
 * Saving and restoring the metadata cache to/from disk
 */
extern bool dmcacheSave(const std::string& fn, const mcache_type& cache);
extern bool dmcacheRestore(const std::string& fn, mcache_type& cache);

#endif /* _OHMETACACHE_H_X_INCLUDED_ */
