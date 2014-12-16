/*************************************************************************/ /*!
@File
@Title          RGX PDump panic definitions header
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    RGX PDump panic definitions header
@License        Strictly Confidential.
*/ /**************************************************************************/

#if !defined (RGX_PDUMP_PANICS_H_)
#define RGX_PDUMP_PANICS_H_


/*! Unique device specific IMG_UINT16 panic IDs to identify the cause of a
 * RGX PDump panic in a PDump script. */
typedef enum
{
	RGX_PDUMP_PANIC_UNDEFINED = 0,

	/* These panics occur when test parameters and driver configuration
	 * enable features that require the firmware and host driver to
	 * communicate. Such features are not supported with off-line playback.
	 */
	RGX_PDUMP_PANIC_ZSBUFFER_BACKING         = 101, /*!< Requests ZSBuffer to be backed with physical pages */
	RGX_PDUMP_PANIC_ZSBUFFER_UNBACKING       = 102, /*!< Requests ZSBuffer to be unbacked */
	RGX_PDUMP_PANIC_FREELIST_GROW            = 103, /*!< Requests an on-demand freelist grow/shrink */
	RGX_PDUMP_PANIC_FREELISTS_RECONSTRUCTION = 104, /*!< Requests freelists reconstruction */
} RGX_PDUMP_PANIC;
 

#endif /* RGX_PDUMP_PANICS_H_ */


