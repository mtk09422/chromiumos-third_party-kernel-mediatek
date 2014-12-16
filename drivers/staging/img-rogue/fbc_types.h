/*************************************************************************/ /*!
@File
@Title          Frame buffer compression definitions
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Strictly Confidential.
*/ /**************************************************************************/

#ifndef _FBC_TYPES_H_
#define	_FBC_TYPES_H_

/**
 * Types of framebuffer compression available.
 */
typedef enum _FB_COMPRESSION_
{
	FB_COMPRESSION_NONE,
	FB_COMPRESSION_DIRECT_8x8,
	FB_COMPRESSION_DIRECT_16x4,
	FB_COMPRESSION_DIRECT_32x2,
	FB_COMPRESSION_INDIRECT_8x8,
	FB_COMPRESSION_INDIRECT_16x4,
	FB_COMPRESSION_INDIRECT_4TILE_8x8,
	FB_COMPRESSION_INDIRECT_4TILE_16x4
} FB_COMPRESSION;

#endif	/* _FBC_TYPES_H_ */

/* EOF */

