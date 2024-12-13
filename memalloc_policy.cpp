#include "memalloc_policy.h"
#include <assert.h>
#include <algorithm>

using namespace std;

#ifdef _DEBUG
#define VERIFY(f)          assert(f)
#else
#define VERIFY(f)          ((void)(f))
#endif

void MemAllocPolicy::init_rst(size_t total_sz)
{
	CFutex::scoped_lock _L(ftx_fl);
	freelst_.clear();
	freelst_.push_back({0,total_sz});
	//CFutex::scoped_lock _m(ftx_am);
	amSet.clear();
}

uintptr_t MemAllocPolicy::allocm(size_t sz)
{
	//find best-fit
	CFutex::scoped_lock _L(ftx_fl);
	auto best = freelst_.end();	size_t diff = SIZE_MAX;
	for (auto it=freelst_.begin(); it != freelst_.end(); ++it) {
		if(it->length<sz) continue;
		size_t d = it->length - sz;
		if (d < diff) {
			diff = d;	best = it;
			if(d==0) break;
		}
	}
	if (best == freelst_.end()) return -1;
	uintptr_t upm = best->offset;
	best->offset += sz;
	best->length -= sz;
	if (best->length == 0) {
		freelst_.erase(best);
	}
	VERIFY(amSet.insert({ upm, sz }).second);
	used_mem_ += sz;
	return upm;
}

int MemAllocPolicy::freem(uintptr_t pm)
{
	CFutex::scoped_lock _L(ftx_fl); 
	size_t sz = 0; {
		auto it = amSet.find(pm);
		if (it == amSet.end()) return -1;
		sz = it->second;
		amSet.erase(it);
	}
	auto it = find_if(freelst_.begin(), freelst_.end(), [pm](ContiMem& cm) {
		return cm.offset > pm;
	});
	if (it != freelst_.end()) {
		assert(pm + sz <= it->offset);
		if (pm + sz < it->offset) {
			freelst_.insert(it, { pm,sz });
		}
		else {
			assert(it->offset >= sz);
			it->offset -= sz;
			it->length += sz;
		}
		auto cur = it++;
		for (;it!=freelst_.end(); cur = it++) {
			if (cur->offset + cur->length < it->offset) break;
			assert(cur->offset + cur->length == it->offset);
			it->offset -= cur->length;
			it->length += cur->length;
			freelst_.erase(cur);
		}
		if (cur !=freelst_.end() && cur != freelst_.begin()) {
			for (auto next = cur--; ;) {
				if (cur->offset + cur->length == next->offset) {
					next->offset -= cur->length;
					next->length += cur->length;
					if (cur == freelst_.begin()) {
						freelst_.erase(cur);
						break;
					}
					else{
						next = freelst_.erase(cur--);
					}
				}
				else{
					break;
				}
			}
			
		}
	}
	else {
		freelst_.push_back({ pm,sz });
	}
	used_mem_ -= sz;
	return 0;
}