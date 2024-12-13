/* 
 * File:   fast_sync_utils.cpp
 * Author: ltmit
 * 
 */

#include "fast_sync_utils.h"
#include "common_interface.h"
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <limits.h>
#ifdef __linux
#include <sched.h>
#include <unistd.h>
#include <time.h>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <errno.h>


static inline int _futex(void *uaddr, int op, int val, const struct timespec *timeout,
                 void *uaddr2, int val3)
{
	return (int)syscall(SYS_futex,uaddr,op,val,timeout,uaddr2,val3);
}

int futex_wait(void* addr,int val,struct timespec* tmo) {
	return _futex(addr,FUTEX_WAIT_PRIVATE,val,tmo,0,0);
}

int futex_wake(void* addr,int nwake) {
	return _futex(addr,FUTEX_WAKE_PRIVATE,nwake,NULL,0,0);
}


int futex_glb_wait(void* addr,int val,struct timespec* tmo) {
	return _futex(addr,FUTEX_WAIT,val,tmo,0,0);
}

int futex_glb_wake(void* addr,int nwake) {
	return _futex(addr,FUTEX_WAKE,nwake,NULL,0,0);
}

#else/////end of linux, start of windows!
#include <windows.h>
#include <winnt.h>

inline static void sched_yield() {  SwitchToThread();  }

ULONG (__stdcall *NtDelayExecution)(
		IN BOOLEAN              Alertable,
		IN PLARGE_INTEGER       DelayInterval	)=0;

//LONG (__stdcall* NtCreateKeyedEvent)(
//	OUT PHANDLE             KeyedEventHandle,
//	IN ACCESS_MASK          DesiredAccess,
//	IN PVOID   ObjectAttributes OPTIONAL,
//	IN ULONG                Reserved ) =0;

//WINBASEAPI	LONG NTAPI NtOpenKeyedEvent(
//	OUT PHANDLE             KeyedEventHandle,
//	IN ACCESS_MASK          DesiredAccess,
//	IN PVOID   ObjectAttributes OPTIONAL );
//
LONG (__stdcall* NtReleaseKeyedEvent)(
	IN HANDLE               KeyedEventHandle,
	IN PVOID                Key,
	IN BOOLEAN              Alertable,
	IN PLARGE_INTEGER       Timeout OPTIONAL )=0;

LONG (__stdcall* NtWaitForKeyedEvent)(
	IN HANDLE               KeyedEventHandle,
	IN PVOID                Key,
	IN BOOLEAN              Alertable,
	IN PLARGE_INTEGER       Timeout OPTIONAL )=0;

template<class T>
inline static void _cpp_set_ptr(void* p,T* &dst) {
	dst=(T*)p;
}

class _nt_loader {
public:
	_nt_loader() {
		HINSTANCE h=(HINSTANCE)GetModuleHandleA("ntdll.dll");
		assert(h);
		_cpp_set_ptr( (void*)GetProcAddress(h,"NtDelayExecution"), NtDelayExecution );
		_cpp_set_ptr( (void*)GetProcAddress(h,"NtReleaseKeyedEvent"), NtReleaseKeyedEvent );
		_cpp_set_ptr( (void*)GetProcAddress(h,"NtWaitForKeyedEvent"), NtWaitForKeyedEvent );
		assert(NtWaitForKeyedEvent && NtReleaseKeyedEvent && NtDelayExecution);
	}
}__object__;

#endif//////////////////////////////////

#ifdef __GNUC__	//gcc|g++

template<class T>
inline void _automic_or(volatile T* opr, T addend){
	__sync_fetch_and_or(opr,addend);
}

template<class T>
inline void _automic_and(volatile T* opr, T addend){
	__sync_fetch_and_and(opr,addend);
}

inline void _mem_fence() {
	__sync_synchronize();
}

inline void _machine_pause(int delay) {
	for(register int i=0;i<delay;++i) {
#ifdef __aarch64__
        __asm__ __volatile__("yield");
#else
        __asm__ __volatile__("pause");
#endif
    }
}

#else //MSVC?


template<class T>
inline void _automic_or(volatile T* opr, T addend){
	//__sync_fetch_and_or(opr,addend);
	_InterlockedOr((volatile long*)opr,(long)addend);
}

template<class T>
inline void _automic_and(volatile T* opr, T addend){
	//__sync_fetch_and_and(opr,addend);
	_InterlockedAnd((volatile long*)opr,(long)addend);
}

inline void _mem_fence() {
	//__sync_synchronize();
	_ReadWriteBarrier();
}

inline void _machine_pause(int delay) {
	for(register int i=0;i<delay;++i) {
		//__asm__ __volatile__("pause");
		_mm_pause();
	}
}

inline int __sync_fetch_and_add(volatile int* ptr,int delt) {
	return _InterlockedExchangeAdd((volatile long*)ptr,delt);
};
#endif

class automic_backoff: no_copy{
	static const uint32_t LOOPS_BEFORE_YIELD = 16;
	static const uint32_t LOOPS_BEFORE_SLEEP = 32;
	uint32_t count;
public:
	automic_backoff() : count(1) {}
	
	void pause() {
		if(count<=LOOPS_BEFORE_YIELD) {
			_machine_pause(count);	count<<=1;
		}else if(count <= LOOPS_BEFORE_SLEEP) {
			sched_yield();	
			++count;
		}else {
			//usleep(5);
			#ifdef __linux
			struct timespec tmp={0,50};
			nanosleep(&tmp,NULL);
			#else
			LARGE_INTEGER li;	li.QuadPart=-32;
			NtDelayExecution(FALSE,&li);
			#endif
		}
	}
	
	void reset() { count=1;}
};

////////////////////fast_spin///////////////////////////////
bool fast_spin_trylock(fast_spin_rw* plock) {
	register state_t r = CAS(&plock->state,(state_t)1,(state_t)0);
	//__sync_lock_test_and_set(&plock->state,1);
	//_mem_fence();
	return r==0;
}

void fast_spin_lock(fast_spin_rw* plock) {
	for(automic_backoff bo;!fast_spin_trylock(plock);bo.pause()) ;
}

void fast_spin_unlock(fast_spin_rw* plock) {
	//_mem_fence();
	*const_cast<state_t volatile*>(&plock->state)=0;
}

///////////////////////////////////////////////////

static const state_t WRITER = 1;
static const state_t WRITER_PENDING = 2;
static const state_t READERS = ~(WRITER | WRITER_PENDING);
static const state_t ONE_READER = 4;
static const state_t BUSY = WRITER | READERS;

void fast_spin_rw_init(fast_spin_rw* prw)
{
	prw->state=0;
}

void fast_spin_rw_rdlock(fast_spin_rw* prw)
{
	volatile state_t& state= prw->state;
	for( automic_backoff backoff;;backoff.pause() ){
		state_t s = const_cast<volatile state_t&>(state); // ensure reloading
		if( !(s & (WRITER|WRITER_PENDING)) ) { // no writer or write requests
			state_t t = (state_t)__sync_fetch_and_add( &state, (state_t) ONE_READER );
			if( !( t&WRITER )) 
				break; // successfully stored increased number of readers
			// writer got there first, undo the increment
			__sync_fetch_and_add( &state, -(state_t)ONE_READER );
		}
	}
}

void fast_spin_rw_runlock(fast_spin_rw* prw)
{
	__sync_fetch_and_add( &prw->state,-(state_t)ONE_READER);
}

bool fast_spin_rw_tryrdlock(fast_spin_rw* prw)
{
	state_t s = prw->state;
	if( !(s & (WRITER|WRITER_PENDING)) ) { // no writers
		state_t t = (state_t)__sync_fetch_and_add( &prw->state, (state_t) ONE_READER );
		if( !( t&WRITER )) {  // got the lock
			//ITT_NOTIFY(sync_acquired, this);
			return true; // successfully stored increased number of readers
		}
		// writer got there first, undo the increment
		__sync_fetch_and_add( &prw->state, -(state_t)ONE_READER );
	}
	return false;
}


bool fast_spin_rw_trywrlock(fast_spin_rw* prw)
{
	state_t s = prw->state;
	if( !(s & BUSY) ) // no readers, no writers; mask is 1..1101
		if( CAS(&prw->state, WRITER, s)==s ) {
			//ITT_NOTIFY(sync_acquired, this);
			return true; // successfully stored writer flag
		}
		return false;
}


void fast_spin_rw_wrlock(fast_spin_rw* prw)
{
	//ITT_NOTIFY(sync_prepare, this);
	for( automic_backoff backoff;;backoff.pause() ){
		state_t s = const_cast<volatile state_t&>(prw->state); // ensure reloading
		if( !(s & BUSY) ) { // no readers, no writers
			if( CAS(&prw->state, WRITER, s)==s )
				break; // successfully stored writer flag
			backoff.reset(); // we could be very close to complete op.
		} else if( !(s & WRITER_PENDING) ) { // no pending writers
			_automic_or(&prw->state, WRITER_PENDING);
		}
	}
}


void fast_spin_rw_wunlock(fast_spin_rw* prw)
{
	_automic_and( &prw->state, READERS );
}


///=---------------------------------------------=///


void fast_mutex_init(fast_mutex* m) {
	m->u=0;
}

#ifdef __linux
void fast_mutex_lock(fast_mutex* m) {
	for(int i=1;i<32;++i) {
		if(!_atomic_xchg(& m->b.locked,(unsigned char)1))
			return ;
		sched_yield();	
	}
	while(_atomic_xchg(& m->u,257u) & 1) {
		futex_wait(m,257);
	}
}

void fast_mutex_unlock(fast_mutex* m) {
	if(m->u==1 && _cmpxchg(&m->u,1u,0u)==1)
		return ;
	m->b.locked=0;
	_mem_fence();
	for(int i=1;i<33;i<<=1) {
		if(m->b.locked) return;
		_machine_pause(i);
	}
	m->b.contended=0;
	futex_wake(m,1);
}

bool fast_mutex_trylock(fast_mutex* m){
	return !_atomic_xchg(& m->b.locked,(unsigned char)1);
}

void wake_ftxp(void* p)
{
	if (__sync_fetch_and_add((int*)p, 1) == 0) {
		futex_wake(p, 1111);
	}
}

void wait_ftxp(void* p)
{
	futex_wait(p, 0, NULL);
}
#else	//windows!? ------------------------------

inline LONG _WaitForKeyedEvent(PVOID key,BOOLEAN bAlertable,PLARGE_INTEGER timeo)
{
	return NtWaitForKeyedEvent(NULL,key,bAlertable,timeo);
}

inline LONG _ReleaseKeyedEvent(PVOID key,BOOLEAN bAlertable,PLARGE_INTEGER timeo)
{
	return NtReleaseKeyedEvent(NULL,key,bAlertable,timeo);
}

/* Allows up to 2^23-1 waiters */
#define FAST_M_WAKE		256
#define FAST_M_WAIT		(FAST_M_WAKE<<1)

#ifdef __GNUC__

bool fast_mutex_trylock(fast_mutex* m){
	return _cmpxchg(&m->owned,(unsigned char)0, (unsigned char)1)==0;
}

void fast_mutex_lock(fast_mutex* m) {
	// Try to take lock if not owned 
	for (; _cmpxchg(&m->owned,(unsigned char)0, (unsigned char)1);sched_yield() )
	{
		register unsigned int waiters = m->u | 1;
		// Otherwise, say we are sleeping 
		if (CAS(&m->u, waiters + FAST_M_WAIT, waiters) == waiters) {
			// Sleep 
			_WaitForKeyedEvent(m,0,NULL);
			// No longer in the process of waking up 
			__sync_fetch_and_sub(&m->u, (unsigned int)FAST_M_WAKE);
		}
	}
}


#else //msvc?
void fast_mutex_lock(fast_mutex *m)
{
	//最低位是0，则testandset之后变为1。然后不执行while内部。
	// Try to take lock if not owned 
	while (m->owned || _interlockedbittestandset((volatile long*)&m->u, 0))
	{//能进来，说明刚才testbit时最低位是1。
		unsigned long waiters = m->u | 1;
		// Otherwise, say we are sleeping 
		if (_InterlockedCompareExchange((volatile long*)&m->u, waiters + FAST_M_WAIT, waiters) == waiters)
		{
			// Sleep 
			_WaitForKeyedEvent(m,0,NULL);
			// No longer in the process of waking up 
			_InterlockedExchangeAdd(&m->u, -FAST_M_WAKE);
		}
	}
}

bool fast_mutex_trylock(fast_mutex *m)
{
	if (!m->owned && !_interlockedbittestandset((volatile long*)&m->u, 0)) return true;

	return false;	//busy
}
#endif

void fast_mutex_unlock(fast_mutex* m) {
	m->owned=0;
	_mem_fence();
	// While we need to wake someone up 
	//for (automic_backoff bo;;bo.pause())
	for(;;)
	{
		register unsigned int waiters = m->u;
		if ((waiters < FAST_M_WAIT)||(waiters&1)||(waiters&FAST_M_WAKE)) 
			break;
		/*// Has someone else taken the lock?
		if (waiters & 1) return;
		// Someone else is waking up
		if (waiters & FAST_M_WAKE) return;
		// Try to decrease wake count*/ 
		if (CAS(&m->u, waiters - FAST_M_WAIT + FAST_M_WAKE, waiters) == waiters)
		{
			_ReleaseKeyedEvent(m,0,NULL);
			return;
		}
		sched_yield();
	}
}

void wait_ftxp(void* p)
{
	NtWaitForKeyedEvent(NULL, p, 0, NULL);
}

void wake_ftxp(void* p)
{
	NtReleaseKeyedEvent(NULL, p, 0, NULL);
}

#endif //------------------------------------------

void fast_rwl_init(fast_rwlock* p) {
	p->_res[0]=0;
	p->_res[1]=0;
}

void fast_rwl_rlock(fast_rwlock* p) {
#ifdef __GNUC__
	if(__sync_fetch_and_add(&p->_res[1],1)==0) 
#else //msvc
	if(_InterlockedIncrement((volatile long*)&p->_res[1])==1)
#endif
	{
		fast_mutex_lock((fast_mutex*)&p->_res[0]);
	}
}

void fast_rwl_runlock(fast_rwlock* p) {
#ifdef __GNUC__
	if(__sync_sub_and_fetch(&p->_res[1],1)==0) 
#else
	if(_InterlockedDecrement((volatile long*)&p->_res[1])==0)
#endif
	{
		fast_mutex_unlock((fast_mutex*)&p->_res[0]);
	}
}

void fast_rwl_wlock(fast_rwlock* p) {
	fast_mutex_lock((fast_mutex*)&p->_res[0]);
}

void fast_rwl_wunlock(fast_rwlock* p) {
	fast_mutex_unlock((fast_mutex*)&p->_res[0]);
}

////////////////////////////////////////
