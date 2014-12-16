/*************************************************************************/ /*!
@File           lock_types.h
@Title          Locking types
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Locking specific enums, defines and structures
@License        Strictly Confidential.
*/ /**************************************************************************/

#ifndef _LOCK_TYPES_H_
#define _LOCK_TYPES_H_

/* In Linux kernel mode we are using the kernel mutex implementation directly
 * with macros. This allows us to use the kernel lockdep feature for lock
 * debugging. */
#if defined(LINUX) && defined(__KERNEL__)

#include <linux/types.h>
#include <linux/mutex.h>
/* The mutex is defined as a pointer to be compatible with the other code. This
 * isn't ideal and usually you wouldn't do that in kernel code. */
typedef struct mutex *POS_LOCK;
typedef atomic_t ATOMIC_T;

#else /* defined(LINUX) && defined(__KERNEL__) */

typedef struct _OS_LOCK_ *POS_LOCK;
#if defined(LINUX)
	typedef struct _OS_ATOMIC {IMG_INT counter;} ATOMIC_T;
#elif defined(__QNXNTO__)
	typedef struct _OS_ATOMIC {IMG_INT counter;} ATOMIC_T;
#elif defined(_WIN32)
	/*
	 * Dummy definition. WDDM doesn't use Services, but some headers
	 * still have to be shared. This is one such case.
	 */
	typedef struct _OS_ATOMIC {IMG_INT counter;} ATOMIC_T;
#else
	#error "Please type-define an atomic lock for this environment"
#endif

#endif /* defined(LINUX) && defined(__KERNEL__) */

typedef enum
{
	LOCK_TYPE_NONE 			= 0x00,

	LOCK_TYPE_MASK			= 0x0F,
	LOCK_TYPE_PASSIVE		= 0x01,		/* Passive level lock e.g. mutex, system may promote to dispatch */
	LOCK_TYPE_DISPATCH		= 0x02,		/* Dispatch level lock e.g. spin lock, may be used in ISR/MISR */

	LOCK_TYPE_INSIST_FLAG	= 0x80,		/* When set caller can guarantee lock not used in ISR/MISR */
	LOCK_TYPE_PASSIVE_ONLY	= LOCK_TYPE_INSIST_FLAG | LOCK_TYPE_PASSIVE

} LOCK_TYPE;
#endif	/* _LOCK_TYPES_H_ */
