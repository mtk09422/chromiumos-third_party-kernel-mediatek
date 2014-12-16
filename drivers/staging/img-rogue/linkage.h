/*************************************************************************/ /*!
@File
@Title          Linux specific Services code internal interfaces
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Interfaces between various parts of the Linux specific
                Services code, that don't have any other obvious
                header file to go into.
@License        Strictly Confidential.
*/ /**************************************************************************/

#if !defined(__LINKAGE_H__)
#define __LINKAGE_H__

#if !defined(SUPPORT_DRM)
long PVRSRV_BridgeDispatchKM(struct file *file, unsigned int cmd, unsigned long arg);

#if defined(CONFIG_COMPAT)
long PVRSRV_BridgeCompatDispatchKM(struct file *file, unsigned int cmd, unsigned long arg);
#endif
#endif

void PVRDPFInit(void);
PVRSRV_ERROR PVROSFuncInit(void);
void PVROSFuncDeInit(void);

int PVRDebugCreateDebugFSEntries(void);
void PVRDebugRemoveDebugFSEntries(void);

#endif /* !defined(__LINKAGE_H__) */
