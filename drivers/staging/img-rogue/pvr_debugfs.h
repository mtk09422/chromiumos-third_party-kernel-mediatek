/*************************************************************************/ /*!
@File
@Title          Functions for creating debugfs directories and entries.
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Strictly Confidential.
*/ /**************************************************************************/

#if !defined(__PVR_DEBUGFS_H__)
#define __PVR_DEBUGFS_H__

#include <linux/debugfs.h>
#include <linux/seq_file.h>

#include "img_types.h"
#include "osfunc.h"

typedef ssize_t (PVRSRV_ENTRY_WRITE_FUNC)(const char __user *pszBuffer,
					  size_t uiCount,
					  loff_t uiPosition,
					  void *pvData);


int PVRDebugFSInit(void);
void PVRDebugFSDeInit(void);

int PVRDebugFSCreateEntryDir(IMG_CHAR *pszName,
			     struct dentry *psParentDir,
			     struct dentry **ppsDir);
void PVRDebugFSRemoveEntryDir(struct dentry *psDir);

int PVRDebugFSCreateEntry(const char *pszName,
			  void *pvDir,
			  struct seq_operations *psReadOps,
			  PVRSRV_ENTRY_WRITE_FUNC *pfnWrite,
			  void *pvData,
			  struct dentry **ppsEntry);
void PVRDebugFSRemoveEntry(struct dentry *psEntry);

void *PVRDebugFSCreateStatisticEntry(const char *pszName,
				     void *pvDir,
				     OS_STATS_PRINT_FUNC *pfnStatsPrint,
				     void *pvData);
void PVRDebugFSRemoveStatisticEntry(void *pvStatEntry);

#endif /* !defined(__PVR_DEBUGFS_H__) */
