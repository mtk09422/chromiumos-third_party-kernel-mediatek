/*************************************************************************/ /*!
@File
@Title          RGX Common Types and Defines Header
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description	Common types and definitions for RGX software
@License        Strictly Confidential.
*/ /**************************************************************************/
#ifndef RGX_COMMON_H_
#define RGX_COMMON_H_

#if defined (__cplusplus)
extern "C" {
#endif

#include "img_defs.h"

/* Included to get the BVNC_KM_N defined and other feature defs */
#include "km/rgxdefs_km.h"

/*! This macro represents a mask of LSBs that must be zero on data structure
 * sizes and offsets to ensure they are 8-byte granular on types shared between
 * the FW and host driver */
#define RGX_FW_ALIGNMENT_LSB (7)

/*! Macro to test structure size alignment */
#define RGX_FW_STRUCT_SIZE_ASSERT(_a)	\
	BLD_ASSERT((sizeof(_a)&RGX_FW_ALIGNMENT_LSB)==0, _a##struct_size)

/*! Macro to test structure member alignment */
#define RGX_FW_STRUCT_OFFSET_ASSERT(_a, _b)	\
	BLD_ASSERT((offsetof(_a, _b)&RGX_FW_ALIGNMENT_LSB)==0, _a##struct_offset)


/*! The number of performance counters in each layout block */
#if defined(RGX_FEATURE_CLUSTER_GROUPING)
#define RGX_HWPERF_CNTRS_IN_BLK 6
#define RGX_HWPERF_CNTRS_IN_BLK_MIN 4
#else
#define RGX_HWPERF_CNTRS_IN_BLK 4
#define RGX_HWPERF_CNTRS_IN_BLK_MIN 4
#endif


/*! The master definition for data masters known to the firmware of RGX.
 * The DM in a V1 HWPerf packet uses this definition. */
typedef enum _RGXFWIF_DM_
{
	RGXFWIF_DM_GP			= 0,
	RGXFWIF_DM_2D			= 1,
	RGXFWIF_DM_TA			= 2,
	RGXFWIF_DM_3D			= 3,
	RGXFWIF_DM_CDM			= 4,
#if defined(RGX_FEATURE_RAY_TRACING)
	RGXFWIF_DM_RTU			= 5,
	RGXFWIF_DM_SHG			= 6,
#endif
	RGXFWIF_DM_LAST,

	RGXFWIF_DM_FORCE_I32  = 0x7fffffff   /*!< Force enum to be at least 32-bits wide */
} RGXFWIF_DM;

#if defined(RGX_FEATURE_RAY_TRACING)
#define RGXFWIF_DM_MAX_MTS 8
#else
#define RGXFWIF_DM_MAX_MTS 6
#endif

#if defined(RGX_FEATURE_RAY_TRACING)
/* Maximum number of DM in use: GP, 2D, TA, 3D, CDM, SHG, RTU */
#define RGXFWIF_DM_MAX			(7)
#else
#define RGXFWIF_DM_MAX			(5)
#endif

/* Min/Max number of HW DMs (all but GP) */
#if defined(RGX_FEATURE_TLA)
#define RGXFWIF_HWDM_MIN		(1)
#else
#define RGXFWIF_HWDM_MIN		(2)
#endif
#define RGXFWIF_HWDM_MAX		(RGXFWIF_DM_MAX)

/*!
 ******************************************************************************
 * RGXFW Compiler alignment definitions
 *****************************************************************************/
#if defined(__GNUC__)
#define RGXFW_ALIGN			__attribute__ ((aligned (8)))
#elif defined(_MSC_VER)
#define RGXFW_ALIGN			__declspec(align(8))
#pragma warning (disable : 4324)
#else
#error "Align MACROS need to be defined for this compiler"
#endif

/*!
 ******************************************************************************
 * Force 8-byte alignment for structures allocated uncached.
 *****************************************************************************/
#define UNCACHED_ALIGN      RGXFW_ALIGN

#if defined (__cplusplus)
}
#endif

#endif /* RGX_COMMON_H_ */

/******************************************************************************
 End of file
******************************************************************************/

