/*************************************************************************/ /*!
@File
@Title          RGX fw interface alignment checks
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Checks to avoid disalignment in RGX fw data structures shared with the host
@License        Strictly Confidential.
*/ /**************************************************************************/

#if !defined (__RGX_FWIF_ALIGNCHECKS_KM_H__)
#define __RGX_FWIF_ALIGNCHECKS_KM_H__

/* for the offsetof macro */
#include <stddef.h> 

/*!
 ******************************************************************************
 * Alignment checks array
 *****************************************************************************/

#define RGXFW_ALIGN_CHECKS_INIT_KM							\
		sizeof(RGXFWIF_INIT),								\
		offsetof(RGXFWIF_INIT, sFaultPhysAddr),			\
		offsetof(RGXFWIF_INIT, sPDSExecBase),				\
		offsetof(RGXFWIF_INIT, sUSCExecBase),				\
		offsetof(RGXFWIF_INIT, psKernelCCBCtl),				\
		offsetof(RGXFWIF_INIT, psKernelCCB),				\
		offsetof(RGXFWIF_INIT, psFirmwareCCBCtl),			\
		offsetof(RGXFWIF_INIT, psFirmwareCCB),				\
		offsetof(RGXFWIF_INIT, eDM),						\
		offsetof(RGXFWIF_INIT, asSigBufCtl),				\
		offsetof(RGXFWIF_INIT, psTraceBufCtl),				\
		offsetof(RGXFWIF_INIT, sRGXCompChecks),				\
															\
		/* RGXFWIF_FWRENDERCONTEXT checks */				\
		sizeof(RGXFWIF_FWRENDERCONTEXT),					\
		offsetof(RGXFWIF_FWRENDERCONTEXT, sTAContext),		\
		offsetof(RGXFWIF_FWRENDERCONTEXT, s3DContext),		\
															\
		sizeof(RGXFWIF_FWCOMMONCONTEXT),					\
		offsetof(RGXFWIF_FWCOMMONCONTEXT, psFWMemContext),	\
		offsetof(RGXFWIF_FWCOMMONCONTEXT, sRunNode),		\
		offsetof(RGXFWIF_FWCOMMONCONTEXT, psCCB),			\
		offsetof(RGXFWIF_FWCOMMONCONTEXT, ui64MCUFenceAddr)

#endif /*  __RGX_FWIF_ALIGNCHECKS_KM_H__ */

/******************************************************************************
 End of file (rgx_fwif_alignchecks_km.h)
******************************************************************************/


