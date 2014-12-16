/*************************************************************************/ /*!
@File           driverlock.h
@Title          Main driver lock
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    The main driver lock, held in most places in
                the driver.
@License        Strictly Confidential.
*/ /**************************************************************************/
#ifndef __DRIVERLOCK_H__
#define __DRIVERLOCK_H__

/*
 * Main driver lock, used to ensure driver code is single threaded.
 * There are some places where this lock must not be taken, such as
 * in the mmap related deriver entry points.
 */
extern struct mutex gPVRSRVLock;

#endif /* __DRIVERLOCK_H__ */
/*****************************************************************************
 End of file (driverlock.h)
*****************************************************************************/
