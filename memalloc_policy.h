#ifndef _MEM_ALLOC_POLICY_H
#define _MEM_ALLOC_POLICY_H

#include <stdlib.h>
#include <stdint.h>
#include <list>
#include <unordered_map>
#include "fast_sync_cls.hpp"

/*
	内存分配器实现
	（内存分配策略基于best-fit方法。）
*/
class MemAllocPolicy {
	MemAllocPolicy(const MemAllocPolicy&);
	struct ContiMem {
		size_t offset;
		size_t length;
	};
	template<class K,class V>
	using map_t = std::unordered_map<K,V>;

public:
	MemAllocPolicy():used_mem_(0) {}

	explicit MemAllocPolicy(size_t s) {
		init_rst(s);
	}
	void init_rst(size_t total_sz);

	uintptr_t allocm(size_t sz);
	int freem(uintptr_t pm);

	size_t used_mem() const {
		return used_mem_;
	}
private:
	map_t<uintptr_t,size_t> amSet;
	std::list<ContiMem> freelst_;
	CFutex ftx_fl;// , ftx_am;
	size_t used_mem_;
};


#endif