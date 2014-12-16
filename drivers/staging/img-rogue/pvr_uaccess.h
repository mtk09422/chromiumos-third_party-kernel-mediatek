/*************************************************************************/ /*!
@File
@Title          Utility functions for user space access
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Strictly Confidential.
*/ /**************************************************************************/
#ifndef __PVR_UACCESS_H__
#define __PVR_UACCESS_H__

#include <linux/version.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,38))
#ifndef AUTOCONF_INCLUDED
#include <linux/config.h>
#endif
#endif

#include <asm/uaccess.h>

static inline unsigned long pvr_copy_to_user(void __user *pvTo, const void *pvFrom, unsigned long ulBytes)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,33))
    if (access_ok(VERIFY_WRITE, pvTo, ulBytes))
    {
	return __copy_to_user(pvTo, pvFrom, ulBytes);
    }
    return ulBytes;
#else
    return copy_to_user(pvTo, pvFrom, ulBytes);
#endif
}


#if defined(__KLOCWORK__)
	/* this part is only to tell Klocwork not to report false positive because
	   it doesn't understand that pvr_copy_from_user will initialise the memory
	   pointed to by pvTo */
#include <linux/string.h> /* get the memset prototype */
static inline unsigned long pvr_copy_from_user(void *pvTo, const void __user *pvFrom, unsigned long ulBytes)
{
	if (pvTo != NULL)
	{
		memset(pvTo, 0xAA, ulBytes);
		return 0;
	}
	return 1;
}
	
#else /* real implementation */

static inline unsigned long pvr_copy_from_user(void *pvTo, const void __user *pvFrom, unsigned long ulBytes)
{

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,33))
    /*
     * The compile time correctness checking introduced for copy_from_user in
     * Linux 2.6.33 isn't fully compatible with our usage of the function.
     */
    if (access_ok(VERIFY_READ, pvFrom, ulBytes))
    {
	return __copy_from_user(pvTo, pvFrom, ulBytes);
    }
    return ulBytes;
#else
    return copy_from_user(pvTo, pvFrom, ulBytes);
#endif
}
#endif /* klocworks */ 

#endif /* __PVR_UACCESS_H__ */

