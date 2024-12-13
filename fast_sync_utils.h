/* 
 * File:   fast_sync_utils.h
 * Author: ltmit
 *
 * Created on 2012-11-23, 10:21 a.m.
 */

#ifndef FAST_SYNC_UTILS_H
#define	FAST_SYNC_UTILS_H

#ifdef _MSC_VER // for MSVC
#define forceinline __forceinline
#elif defined __GNUC__ // for gcc on Linux/Apple OS X
#define forceinline __inline__ __attribute__((always_inline))
#else
#define forceinline
#endif

#ifdef __linux
int futex_wait(void* addr,int val,struct timespec* tmo=0);
int futex_wake(void* addr,int nwake);

int futex_glb_wait(void* addr,int val,struct timespec* tmo=0);
int futex_glb_wake(void* addr,int nwake);
#endif

#ifdef __GNUC__

template<class T>
static inline T CAS(volatile T* ptr,T val,T cmp) {
	return __sync_val_compare_and_swap(ptr,cmp,val);
}

template<class T>
static inline T _cmpxchg(volatile T* ptr,T cmp,T val) {
	return __sync_val_compare_and_swap(ptr,cmp,val);
}

#ifdef __aarch64__

template<class T>
static inline  T _atomic_xchg(volatile T *ptr, T value)
{
    return __atomic_exchange_n(ptr, value, __ATOMIC_SEQ_CST);
}

template<>
inline unsigned char _atomic_xchg(volatile unsigned char* ptr, unsigned char value) {
    return __atomic_exchange_n(ptr, value, __ATOMIC_SEQ_CST);
}

#else

template<class T>
static inline  T _atomic_xchg(volatile T *ptr, T value)            \
{                                                                                    \
T result;                                                                        \
__asm__ __volatile__("lock\nxchg" " %0,%1"                                     \
: "=r"(result), "=m"(*(volatile T*)ptr)                \
: "0"(value), "m"(*(volatile T*)ptr)                 \
: "memory");                                               \
return result;                                                                   \
}

template<>
inline unsigned char _atomic_xchg(volatile unsigned char* ptr, unsigned char value) {
    unsigned char result;                                                                        \
        __asm__ __volatile__("lock\nxchg" " %0,%1"                                     \
        : "=q"(result), "=m"(*ptr)                \
        : "0"(value), "m"(*ptr)                 \
        : "memory");                                               \
        return result;
}

#endif

#else //msvc?
#include <intrin.h>

template<class T>
static inline T CAS(volatile T* ptr,T val,T cmp) {
	return _InterlockedCompareExchange((volatile long*)ptr,val,cmp); //__sync_val_compare_and_swap(ptr,cmp,val);
}

template<class T>
static inline T _cmpxchg(volatile T* ptr,T cmp,T val) {
	return _InterlockedCompareExchange((volatile long*)ptr,val,cmp);//__sync_val_compare_and_swap(ptr,cmp,val);
}

#endif
//---------------------------------------------------------

typedef int state_t;

typedef struct _fast_spin_rw {
	volatile state_t state;	//need aligned!
}fast_spin_rw;

#define FAST_SPIN_RW_INITIALIZER	{0}

/*typedef struct _fast_mutex{
	volatile int waiters;
}fast_mutex;*/
typedef union {
	unsigned int volatile u;
	struct {
		unsigned char volatile locked,contended;
	}b;
	unsigned char volatile owned;
}fast_mutex;


//newly add by ltmit @2016-02-27
typedef struct {
	int volatile _res[2];
}fast_rwlock;


#ifdef __cplusplus
extern "C" {
#endif
//////this APIs used as spin_mutex
bool fast_spin_trylock(fast_spin_rw* plock);
void fast_spin_lock(fast_spin_rw* plock);
void fast_spin_unlock(fast_spin_rw* plock);

//bool fast_spin_timedlock(fast_spin_rw* plock,unsigned int timeout_ms);

///////use fast_spin_rw as a read-write lock
void fast_spin_rw_init(fast_spin_rw* prw);
void fast_spin_rw_rdlock(fast_spin_rw* prw);
void fast_spin_rw_runlock(fast_spin_rw* prw);
bool fast_spin_rw_tryrdlock(fast_spin_rw* prw);
void fast_spin_rw_wrlock(fast_spin_rw* prw);
void fast_spin_rw_wunlock(fast_spin_rw* prw);
bool fast_spin_rw_trywrlock(fast_spin_rw* prw);

///////
void fast_mutex_init(fast_mutex *m);

void fast_mutex_lock(fast_mutex *m);

bool fast_mutex_trylock(fast_mutex *m);

void fast_mutex_unlock(fast_mutex *m);

//////////////////////////////////
void fast_rwl_init(fast_rwlock* p);

void fast_rwl_rlock(fast_rwlock* p);

void fast_rwl_runlock(fast_rwlock* p);

void fast_rwl_wlock(fast_rwlock* p);

void fast_rwl_wunlock(fast_rwlock* p);

void wait_ftxp(void* p);

void wake_ftxp(void* p);

#ifdef __cplusplus
}
#endif
#endif	/* FAST_SYNC_UTILS_H */

