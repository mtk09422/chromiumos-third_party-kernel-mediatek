/*************************************************************************/ /*!
@File
@Title          Device specific utility routines declarations
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Inline functions/structures specific to RGX
@License        Strictly Confidential.
*/ /**************************************************************************/

#include "device.h"
#include "rgxdevice.h"
#include "rgxdebug.h"
#include "pvrsrv.h"

/*!
******************************************************************************

 @Function	RGXRunScript

 @Description Execute the commands in the script

 @Input 

 @Return   PVRSRV_ERROR

******************************************************************************/
PVRSRV_ERROR RGXRunScript(PVRSRV_RGXDEV_INFO	*psDevInfo,
						 RGX_INIT_COMMAND	*psScript,
						 IMG_UINT32			ui32NumCommands,
						 IMG_UINT32				ui32PdumpFlags,
						 DUMPDEBUG_PRINTF_FUNC  *pfnDumpDebugPrintf);

/******************************************************************************
 End of file (rgxutils.h)
******************************************************************************/
