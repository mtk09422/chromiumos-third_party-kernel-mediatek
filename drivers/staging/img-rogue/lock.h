/*************************************************************************/ /*!
@File           lock.h
@Title          Locking interface
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Services internal locking interface
@License        Strictly Confidential.
*/ /**************************************************************************/

#ifndef _LOCK_H_
#define _LOCK_H_

/* In Linux kernel mode we are using the kernel mutex implementation directly
 * with macros. This allows us to use the kernel lockdep feature for lock
 * debugging. */
#include "lock_types.h"

#if defined(LINUX) && defined(__KERNEL__)

#include "allocmem.h"
#include <asm/atomic.h>

#define OSLockCreate(phLock, eLockType) ({ \
	PVRSRV_ERROR e = PVRSRV_ERROR_OUT_OF_MEMORY; \
	*(phLock) = OSAllocMem(sizeof(struct mutex)); \
	if (*(phLock)) { mutex_init(*(phLock)); e = PVRSRV_OK; }; \
	e;})
#define OSLockDestroy(hLock) ({mutex_destroy((hLock)); OSFreeMem((hLock)); PVRSRV_OK;})

#define OSLockAcquire(hLock) ({mutex_lock((hLock)); PVRSRV_OK;})
#define OSLockAcquireNested(hLock, subclass) ({mutex_lock_nested((hLock), (subclass)); PVRSRV_OK;})
#define OSLockRelease(hLock) ({mutex_unlock((hLock)); PVRSRV_OK;})

#define OSLockIsLocked(hLock) ({IMG_BOOL b = ((mutex_is_locked((hLock)) == 1) ? IMG_TRUE : IMG_FALSE); b;})
#define OSTryLockAcquire(hLock) ({IMG_BOOL b = ((mutex_trylock(hLock) == 1) ? IMG_TRUE : IMG_FALSE); b;})

#if defined(CONFIG_DEBUG_MUTEXES) || defined(CONFIG_SMP)
#define OSLockIsLockedByMe(hLock) ({IMG_BOOL b = ( ((mutex_is_locked(hLock) == 1) && (current == (hLock)->owner)) ? IMG_TRUE : IMG_FALSE); b;})
#endif

/* These _may_ be reordered or optimized away entirely by the compiler/hw */
#define OSAtomicRead(pCounter)	({IMG_INT rv = atomic_read(pCounter); rv;})
#define OSAtomicWrite(pCounter, i)	({ atomic_set(pCounter, i); })

/* The following atomic operations, in addition to being SMP-safe, also
   imply a memory barrier around the operation  */
#define OSAtomicIncrement(pCounter)	({IMG_INT rv = atomic_inc_return(pCounter); rv;})
#define OSAtomicDecrement(pCounter) ({IMG_INT rv = atomic_dec_return(pCounter); rv;})
#define OSAtomicCompareExchange(pCounter, oldv, newv) ({IMG_INT rv = atomic_cmpxchg(pCounter,oldv,newv); rv;})

#define OSAtomicAdd(pCounter, incr) ({IMG_INT rv = atomic_add_return(incr,pCounter); rv;})
#define OSAtomicAddUnless(pCounter, incr, test) ({IMG_INT rv = __atomic_add_unless(pCounter,incr,test); rv;})

#define OSAtomicSubtract(pCounter, incr) ({IMG_INT rv = atomic_add_return(-(incr),pCounter); rv;})
#define OSAtomicSubtractUnless(pCounter, incr, test) OSAtomicAddUnless(pCounter, -(incr), test)

#else /* defined(LINUX) && defined(__KERNEL__) */

#include "img_types.h"
#include "pvrsrv_error.h"

IMG_INTERNAL
PVRSRV_ERROR OSLockCreate(POS_LOCK *phLock, LOCK_TYPE eLockType);

IMG_INTERNAL
IMG_VOID OSLockDestroy(POS_LOCK hLock);

IMG_INTERNAL
IMG_VOID OSLockAcquire(POS_LOCK hLock);

/* Nested notation isn't used in UM or other OS's */
#define OSLockAcquireNested(hLock, subclass) OSLockAcquire((hLock))

IMG_INTERNAL
IMG_VOID OSLockRelease(POS_LOCK hLock);

IMG_INTERNAL
IMG_BOOL OSLockIsLocked(POS_LOCK hLock);

IMG_INTERNAL
IMG_BOOL OSLockIsLockedByMe(POS_LOCK hLock);

#if defined(LINUX)

/* Use GCC intrinsics (read/write semantics consistent with kernel-side implementation) */
#define OSAtomicRead(pCounter) ({IMG_INT rv =  *(volatile int *)&(pCounter)->counter; rv;}) 
#define OSAtomicWrite(pCounter, i) ({(pCounter)->counter = (IMG_INT) i;}) 
#define OSAtomicIncrement(pCounter) ({IMG_INT rv = __sync_add_and_fetch((&(pCounter)->counter), 1); rv;}) 
#define OSAtomicDecrement(pCounter) ({IMG_INT rv = __sync_sub_and_fetch((&(pCounter)->counter), 1); rv;}) 
#define OSAtomicCompareExchange(pCounter, oldv, newv)  \
	({IMG_INT rv = __sync_val_compare_and_swap((&(pCounter)->counter), oldv, newv); rv;})
	
#define OSAtomicAdd(pCounter, incr) ({IMG_INT rv = __sync_add_and_fetch((&(pCounter)->counter), incr); rv;}) 
#define OSAtomicAddUnless(pCounter, incr, test) ({ \
	int c; int old; \
	c = OSAtomicRead(pCounter); \
    while (1) { \
		if (c == (test)) break; \
		old = OSAtomicCompareExchange(pCounter, c, c+(incr)); \
		if (old == c) break; \
		c = old; \
	} c; })

#define OSAtomicSubtract(pCounter, incr) OSAtomicAdd(pCounter, -(incr))	
#define OSAtomicSubtractUnless(pCounter, incr, test) OSAtomicAddUnless(pCounter, -(incr), test)

#else

/* These _may_ be reordered or optimized away entirely by the compiler/hw */
IMG_INTERNAL
IMG_INT OSAtomicRead(ATOMIC_T *pCounter);

IMG_INTERNAL
IMG_VOID OSAtomicWrite(ATOMIC_T *pCounter, IMG_INT v);

/* For the following atomic operations, in addition to being SMP-safe, 
   _should_ also  have a memory barrier around each operation  */
IMG_INTERNAL
IMG_INT OSAtomicIncrement(ATOMIC_T *pCounter);

IMG_INTERNAL
IMG_INT OSAtomicDecrement(ATOMIC_T *pCounter);

IMG_INTERNAL
IMG_INT OSAtomicAdd(ATOMIC_T *pCounter, IMG_INT v);

IMG_INTERNAL
IMG_INT OSAtomicAddUnless(ATOMIC_T *pCounter, IMG_INT v, IMG_INT t);

IMG_INTERNAL
IMG_INT OSAtomicSubtract(ATOMIC_T *pCounter, IMG_INT v);

IMG_INTERNAL
IMG_INT OSAtomicSubtractUnless(ATOMIC_T *pCounter, IMG_INT v, IMG_INT t);

IMG_INTERNAL
IMG_INT OSAtomicCompareExchange(ATOMIC_T *pCounter, IMG_INT oldv, IMG_INT newv);

#endif /* defined(LINUX) */
#endif /* defined(LINUX) && defined(__KERNEL__) */

#endif	/* _LOCK_H_ */
