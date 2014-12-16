/*************************************************************************/ /*!
@File
@Title          RGX heap definitions
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Strictly Confidential.
*/ /**************************************************************************/

#if !defined(__RGX_HEAPS_H__)
#define __RGX_HEAPS_H__

#include "km/rgxdefs_km.h"

/* RGX Heap IDs, note: not all heaps are available to clients */
/* N.B.  Old heap identifiers are deprecated now that the old memory
   management is. New heap identifiers should be suitably renamed */
#define RGX_UNDEFINED_HEAP_ID					(~0LU)          /*!< RGX Undefined Heap ID */
#define RGX_GENERAL_HEAP_ID						0               /*!< RGX General Heap ID */
#define RGX_PDSCODEDATA_HEAP_ID					1               /*!< RGX PDS Code/Data Heap ID */
//#define RGX_3DPARAMETERS_HEAP_ID				2               /*!< RGX 3D Parameters Heap ID */
#define RGX_USCCODE_HEAP_ID						2               /*!< RGX USC Code Heap ID */
#define RGX_FIRMWARE_HEAP_ID					3               /*!< RGX Firmware Heap ID */
#define RGX_TQ3DPARAMETERS_HEAP_ID				4               /*!< RGX Firmware Heap ID */
#define RGX_BIF_TILING_HEAP_1_ID				5 				/*!< RGX BIF Tiling Heap 1 ID */
#define RGX_BIF_TILING_HEAP_2_ID				6 				/*!< RGX BIF Tiling Heap 2 ID */
#define RGX_BIF_TILING_HEAP_3_ID				7 				/*!< RGX BIF Tiling Heap 3 ID */
#define RGX_BIF_TILING_HEAP_4_ID				8 				/*!< RGX BIF Tiling Heap 4 ID */
#define RGX_HWBRN37200_HEAP_ID					9				/*!< RGX HWBRN37200 */
#define RGX_DOPPLER_HEAP_ID						10				/*!< Doppler Heap ID */
#define RGX_DOPPLER_OVERFLOW_HEAP_ID			11				/*!< Doppler Overflow Heap ID */

/* FIXME: work out what this ought to be.  In the old days it was
   typically bigger than it needed to be.  Is the correct thing
   "max + 1" ?? */
#define RGX_MAX_HEAP_ID     	(RGX_DOPPLER_OVERFLOW_HEAP_ID + 1)		/*!< Max Valid Heap ID */

/*
  Identify heaps by their names
*/
#define RGX_GENERAL_HEAP_IDENT 			"General"               /*!< RGX General Heap Identifier */
#define RGX_PDSCODEDATA_HEAP_IDENT 		"PDS Code and Data"     /*!< RGX PDS Code/Data Heap Identifier */
#define RGX_USCCODE_HEAP_IDENT			"USC Code"              /*!< RGX USC Code Heap Identifier */
#define RGX_TQ3DPARAMETERS_HEAP_IDENT	"TQ3DParameters"        /*!< RGX TQ 3D Parameters Heap Identifier */
#define RGX_BIF_TILING_HEAP_1_IDENT	    "BIF Tiling Heap l"	    /*!< RGX BIF Tiling Heap 1 identifier */
#define RGX_BIF_TILING_HEAP_2_IDENT	    "BIF Tiling Heap 2"	    /*!< RGX BIF Tiling Heap 2 identifier */
#define RGX_BIF_TILING_HEAP_3_IDENT	    "BIF Tiling Heap 3"	    /*!< RGX BIF Tiling Heap 3 identifier */
#define RGX_BIF_TILING_HEAP_4_IDENT	    "BIF Tiling Heap 4"	    /*!< RGX BIF Tiling Heap 4 identifier */
#define RGX_DOPPLER_HEAP_IDENT			"Doppler"				/*!< Doppler Heap Identifier */
#define RGX_DOPPLER_OVERFLOW_HEAP_IDENT	"Doppler Overflow"				/*!< Doppler Heap Identifier */

#endif /* __RGX_HEAPS_H__ */

