#ifndef _FAST_SYNC_CLS_H_
#define _FAST_SYNC_CLS_H_
#include "fast_sync_utils.h"
#include "common_interface.h"


class CSpinMutex:no_copy{
	fast_spin_rw spin;
public:
	CSpinMutex() { fast_spin_rw_init(&spin); }
	~CSpinMutex() { spin.state=0; }

	void Lock() { fast_spin_lock(&spin); }
	void Unlock() { fast_spin_unlock(&spin); }
	bool tryLock() { return fast_spin_trylock(&spin); }

	bool is_locked() const {
		return spin.state!=0;
	}
	
	class scoped_lock:no_copy{
		CSpinMutex & ftx;
		void operator=(const scoped_lock&) {}
	public:
		scoped_lock(CSpinMutex& ft): ftx(ft) {
			ftx.Lock();
		}
		~scoped_lock() {
			ftx.Unlock();
		}
	};
};



class CSpinRwLock :no_copy{
	fast_spin_rw rwl;
public:
	CSpinRwLock() { fast_spin_rw_init(&rwl); }
	~CSpinRwLock() {}

	void rLock() { fast_spin_rw_rdlock(&rwl); }
	void rUnlock() { fast_spin_rw_runlock(&rwl); }

	void wLock() {fast_spin_rw_wrlock(&rwl); }
	bool wTryLock() { return fast_spin_rw_trywrlock(&rwl);}
	void wUnlock() { fast_spin_rw_wunlock(&rwl); }

	//bool upgrade() { return fast_spin_rw_upgrade(&rwl); }
	//void downgrade()  { fast_spin_rw_downgrade(&rwl); }

	class scoped_rlock:no_copy{
		CSpinRwLock& rw;
		void operator=(const scoped_rlock&) {}
	public:
		scoped_rlock(CSpinRwLock& lock): rw(lock) {
			rw.rLock();
		}

		~scoped_rlock() {
			rw.rUnlock();
		}
	};

	class scoped_wlock:no_copy{
		CSpinRwLock& rw;
		void operator=(const scoped_wlock&) {}
	public:
		scoped_wlock(CSpinRwLock& lock): rw(lock) {
			rw.wLock();
		}

		~scoped_wlock() {
			rw.wUnlock();
		}
	};
};


class CFutex :no_copy{
	fast_mutex fmt;
public:
	CFutex() { fast_mutex_init(&fmt); }
	~CFutex() {} 
	
	void Lock() { fast_mutex_lock(&fmt); }
	bool tryLock() { return fast_mutex_trylock(&fmt);}
	void Unlock()  { fast_mutex_unlock(&fmt);}
	
	class scoped_lock :no_copy{
		CFutex& ft; 
	public:
		scoped_lock(CFutex& ftx):ft(ftx){
			ft.Lock();
		}
		~scoped_lock() {
			ft.Unlock();
		}
	};
};

//newly add by ltmit @ 2016-02-23
class CFtxRwlock:no_copy{
	fast_rwlock _dum;
public:
	CFtxRwlock() { fast_rwl_init(&_dum); }

	inline void rLock() {
		fast_rwl_rlock(&_dum);
	}
	inline void rUnlock() {
		fast_rwl_runlock(&_dum);
	}

	inline void wLock() {
		fast_rwl_wlock(&_dum);
	}

	inline void wUnlock() {
		fast_rwl_wunlock(&_dum);
	}

	class scoped_rlock:no_copy{
		CFtxRwlock& rw;
		void operator=(const scoped_rlock&) {}
	public:
		scoped_rlock(CFtxRwlock& lock): rw(lock) {
			rw.rLock();
		}

		~scoped_rlock() {
			rw.rUnlock();
		}
	};

	class scoped_wlock:no_copy{
		CFtxRwlock& rw;
		void operator=(const scoped_wlock&) {}
	public:
		scoped_wlock(CFtxRwlock& lock): rw(lock) {
			rw.wLock();
		}

		~scoped_wlock() {
			rw.wUnlock();
		}
	};
};
#endif
