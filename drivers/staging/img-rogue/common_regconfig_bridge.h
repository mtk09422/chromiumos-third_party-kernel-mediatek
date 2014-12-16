/*************************************************************************/ /*!
@File
@Title          Common bridge header for regconfig
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Declares common defines and structures that are used by both
                the client and sever side of the bridge for regconfig
@License        Strictly Confidential.
*/ /**************************************************************************/

#ifndef COMMON_REGCONFIG_BRIDGE_H
#define COMMON_REGCONFIG_BRIDGE_H

#include "rgx_bridge.h"


#include "pvr_bridge_io.h"

#define PVRSRV_BRIDGE_REGCONFIG_CMD_FIRST			(PVRSRV_BRIDGE_REGCONFIG_START)
#define PVRSRV_BRIDGE_REGCONFIG_RGXSETREGCONFIGPI			PVRSRV_IOWR(PVRSRV_BRIDGE_REGCONFIG_CMD_FIRST+0)
#define PVRSRV_BRIDGE_REGCONFIG_RGXADDREGCONFIG			PVRSRV_IOWR(PVRSRV_BRIDGE_REGCONFIG_CMD_FIRST+1)
#define PVRSRV_BRIDGE_REGCONFIG_RGXCLEARREGCONFIG			PVRSRV_IOWR(PVRSRV_BRIDGE_REGCONFIG_CMD_FIRST+2)
#define PVRSRV_BRIDGE_REGCONFIG_RGXENABLEREGCONFIG			PVRSRV_IOWR(PVRSRV_BRIDGE_REGCONFIG_CMD_FIRST+3)
#define PVRSRV_BRIDGE_REGCONFIG_RGXDISABLEREGCONFIG			PVRSRV_IOWR(PVRSRV_BRIDGE_REGCONFIG_CMD_FIRST+4)
#define PVRSRV_BRIDGE_REGCONFIG_CMD_LAST			(PVRSRV_BRIDGE_REGCONFIG_CMD_FIRST+4)


/*******************************************
            RGXSetRegConfigPI          
 *******************************************/

/* Bridge in structure for RGXSetRegConfigPI */
typedef struct PVRSRV_BRIDGE_IN_RGXSETREGCONFIGPI_TAG
{
	IMG_HANDLE hDevNode;
	IMG_UINT8 ui8RegPowerIsland;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_RGXSETREGCONFIGPI;


/* Bridge out structure for RGXSetRegConfigPI */
typedef struct PVRSRV_BRIDGE_OUT_RGXSETREGCONFIGPI_TAG
{
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_RGXSETREGCONFIGPI;

/*******************************************
            RGXAddRegconfig          
 *******************************************/

/* Bridge in structure for RGXAddRegconfig */
typedef struct PVRSRV_BRIDGE_IN_RGXADDREGCONFIG_TAG
{
	IMG_HANDLE hDevNode;
	IMG_UINT32 ui32RegAddr;
	IMG_UINT64 ui64RegValue;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_RGXADDREGCONFIG;


/* Bridge out structure for RGXAddRegconfig */
typedef struct PVRSRV_BRIDGE_OUT_RGXADDREGCONFIG_TAG
{
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_RGXADDREGCONFIG;

/*******************************************
            RGXClearRegConfig          
 *******************************************/

/* Bridge in structure for RGXClearRegConfig */
typedef struct PVRSRV_BRIDGE_IN_RGXCLEARREGCONFIG_TAG
{
	IMG_HANDLE hDevNode;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_RGXCLEARREGCONFIG;


/* Bridge out structure for RGXClearRegConfig */
typedef struct PVRSRV_BRIDGE_OUT_RGXCLEARREGCONFIG_TAG
{
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_RGXCLEARREGCONFIG;

/*******************************************
            RGXEnableRegConfig          
 *******************************************/

/* Bridge in structure for RGXEnableRegConfig */
typedef struct PVRSRV_BRIDGE_IN_RGXENABLEREGCONFIG_TAG
{
	IMG_HANDLE hDevNode;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_RGXENABLEREGCONFIG;


/* Bridge out structure for RGXEnableRegConfig */
typedef struct PVRSRV_BRIDGE_OUT_RGXENABLEREGCONFIG_TAG
{
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_RGXENABLEREGCONFIG;

/*******************************************
            RGXDisableRegConfig          
 *******************************************/

/* Bridge in structure for RGXDisableRegConfig */
typedef struct PVRSRV_BRIDGE_IN_RGXDISABLEREGCONFIG_TAG
{
	IMG_HANDLE hDevNode;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_RGXDISABLEREGCONFIG;


/* Bridge out structure for RGXDisableRegConfig */
typedef struct PVRSRV_BRIDGE_OUT_RGXDISABLEREGCONFIG_TAG
{
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_RGXDISABLEREGCONFIG;

#endif /* COMMON_REGCONFIG_BRIDGE_H */
