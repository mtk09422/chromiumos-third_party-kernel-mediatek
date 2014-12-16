/*************************************************************************/ /*!
@File
@Title          Common bridge header for smm
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Declares common defines and structures that are used by both
                the client and sever side of the bridge for smm
@License        Strictly Confidential.
*/ /**************************************************************************/

#ifndef COMMON_SMM_BRIDGE_H
#define COMMON_SMM_BRIDGE_H



#include "pvr_bridge_io.h"

#define PVRSRV_BRIDGE_SMM_CMD_FIRST			(PVRSRV_BRIDGE_SMM_START)
#define PVRSRV_BRIDGE_SMM_PMRSECUREEXPORTPMR			PVRSRV_IOWR(PVRSRV_BRIDGE_SMM_CMD_FIRST+0)
#define PVRSRV_BRIDGE_SMM_PMRSECUREUNEXPORTPMR			PVRSRV_IOWR(PVRSRV_BRIDGE_SMM_CMD_FIRST+1)
#define PVRSRV_BRIDGE_SMM_PMRSECUREIMPORTPMR			PVRSRV_IOWR(PVRSRV_BRIDGE_SMM_CMD_FIRST+2)
#define PVRSRV_BRIDGE_SMM_CMD_LAST			(PVRSRV_BRIDGE_SMM_CMD_FIRST+2)


/*******************************************
            PMRSecureExportPMR          
 *******************************************/

/* Bridge in structure for PMRSecureExportPMR */
typedef struct PVRSRV_BRIDGE_IN_PMRSECUREEXPORTPMR_TAG
{
	IMG_HANDLE hPMR;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_PMRSECUREEXPORTPMR;


/* Bridge out structure for PMRSecureExportPMR */
typedef struct PVRSRV_BRIDGE_OUT_PMRSECUREEXPORTPMR_TAG
{
	IMG_SECURE_TYPE Export;
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_PMRSECUREEXPORTPMR;

/*******************************************
            PMRSecureUnexportPMR          
 *******************************************/

/* Bridge in structure for PMRSecureUnexportPMR */
typedef struct PVRSRV_BRIDGE_IN_PMRSECUREUNEXPORTPMR_TAG
{
	IMG_HANDLE hPMR;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_PMRSECUREUNEXPORTPMR;


/* Bridge out structure for PMRSecureUnexportPMR */
typedef struct PVRSRV_BRIDGE_OUT_PMRSECUREUNEXPORTPMR_TAG
{
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_PMRSECUREUNEXPORTPMR;

/*******************************************
            PMRSecureImportPMR          
 *******************************************/

/* Bridge in structure for PMRSecureImportPMR */
typedef struct PVRSRV_BRIDGE_IN_PMRSECUREIMPORTPMR_TAG
{
	IMG_SECURE_TYPE Export;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_PMRSECUREIMPORTPMR;


/* Bridge out structure for PMRSecureImportPMR */
typedef struct PVRSRV_BRIDGE_OUT_PMRSECUREIMPORTPMR_TAG
{
	IMG_HANDLE hPMR;
	IMG_DEVMEM_SIZE_T uiSize;
	IMG_DEVMEM_ALIGN_T sAlign;
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_PMRSECUREIMPORTPMR;

#endif /* COMMON_SMM_BRIDGE_H */
