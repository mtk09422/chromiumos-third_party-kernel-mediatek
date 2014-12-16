/*************************************************************************/ /*!
@File
@Title          Common Display Class header
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Defines DC specific structures which are shared within services
                only
@License        Strictly Confidential.
*/ /**************************************************************************/
#include "img_types.h"
#include "services.h"

#ifndef _DC_COMMON_H_
#define _DC_COMMON_H_

typedef struct _DC_FBC_CREATE_INFO_
{
	IMG_UINT32		ui32FBCWidth;	/*!< Pixel width that the FBC module is working on */
	IMG_UINT32		ui32FBCHeight;	/*!< Pixel height that the FBC module is working on */
	IMG_UINT32		ui32FBCStride;	/*!< Pixel stride that the FBC module is working on */
	IMG_UINT32		ui32Size;		/*!< Size of the buffer to create */
} DC_FBC_CREATE_INFO;

typedef struct _DC_CREATE_INFO_
{
	union {
		DC_FBC_CREATE_INFO sFBC;
	} u;
} DC_CREATE_INFO;

typedef struct _DC_BUFFER_CREATE_INFO_
{
	PVRSRV_SURFACE_INFO   	sSurface;	/*!< Surface properies, specificed by user */
	IMG_UINT32            	ui32BPP;	/*!< Bits per pixel */
	union {
		DC_FBC_CREATE_INFO 	sFBC;		/*!< Frame buffer compressed specific data */
	} u;
} DC_BUFFER_CREATE_INFO;

#endif /* _DC_COMMON_H_ */
