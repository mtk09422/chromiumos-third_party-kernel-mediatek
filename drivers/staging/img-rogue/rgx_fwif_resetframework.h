/*************************************************************************/ /*!
@File			rgx_fwif_resetframework.h
@Title         	Post-reset work-around framework FW interface
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Strictly Confidential.
*/ /**************************************************************************/

#if !defined(_RGX_FWIF_RESETFRAMEWORK_H)
#define _RGX_FWIF_RESETFRAMEWORK_H

#include "img_types.h"
#include "rgx_fwif_shared.h"

typedef struct _RGXFWIF_RF_REGISTERS_
{
	IMG_UINT64  uCDMReg_CDM_CTRL_STREAM_BASE;
} RGXFWIF_RF_REGISTERS;

#define RGXFWIF_RF_FLAG_ENABLE 0x00000001 /*!< enables the reset framework in the firmware */

typedef struct _RGXFWIF_RF_CMD_
{
	IMG_UINT32           ui32Flags;

	/* THIS MUST BE THE LAST MEMBER OF THE CONTAINING STRUCTURE */
	RGXFWIF_RF_REGISTERS RGXFW_ALIGN sFWRegisters;

} RGXFWIF_RF_CMD;

/* to opaquely allocate and copy in the kernel */
#define RGXFWIF_RF_CMD_SIZE  sizeof(RGXFWIF_RF_CMD)

#endif /* _RGX_FWIF_RESETFRAMEWORK_H */
