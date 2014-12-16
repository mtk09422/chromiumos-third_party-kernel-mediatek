/*************************************************************************/ /*!
@File
@Title          Functions for BVNC manipulating

@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Utility functions used internally by device memory management
                code.
@License        Strictly Confidential.
*/ /**************************************************************************/

#if !defined (__RGX_COMPAT_BVNC_H__)
#define __RGX_COMPAT_BVNC_H__

#include "img_types.h"

/******************************************************************************
 * RGX Version packed into 24-bit (BNC) and string (V) to be used by Compatibility Check
 *****************************************************************************/

#define RGX_BVNC_PACK_MASK_B 0x00FF0000
#define RGX_BVNC_PACK_MASK_N 0x0000FF00
#define RGX_BVNC_PACK_MASK_C 0x000000FF

#define RGX_BVNC_PACKED_EXTR_B(BVNC) (((BVNC).ui32BNC >> 16) & 0xFF)
#define RGX_BVNC_PACKED_EXTR_V(BVNC) ((BVNC).aszV)
#define RGX_BVNC_PACKED_EXTR_N(BVNC) (((BVNC).ui32BNC >> 8) & 0xFF)
#define RGX_BVNC_PACKED_EXTR_C(BVNC) (((BVNC).ui32BNC >> 0) & 0xFF)

#define RGX_BVNC_EQUAL(L,R,all,version,lenmax,bnc,v) do {													\
										(lenmax) = IMG_FALSE;												\
										(bnc) = IMG_FALSE;													\
										(v) = IMG_FALSE;													\
										(version) = ((L).ui32LayoutVersion == (R).ui32LayoutVersion);		\
										if (version)														\
										{																	\
											(lenmax) = ((L).ui32VLenMax == (R).ui32VLenMax);				\
										}																	\
										if (lenmax)															\
										{																	\
											(bnc) = ((L).ui32BNC == (R).ui32BNC);							\
										}																	\
										if (bnc)															\
										{																	\
											(L).aszV[(L).ui32VLenMax] = '\0';								\
											(R).aszV[(R).ui32VLenMax] = '\0';								\
											(v) = (OSStringCompare((L).aszV, (R).aszV)==0);					\
										}																	\
										(all) = (version) && (lenmax) && (bnc) && (v);						\
									} while (0)

IMG_VOID rgx_bvnc_packed(IMG_UINT32 *pui32OutBNC, IMG_CHAR *pszOutV, IMG_UINT32 ui32OutVMaxLen,
							IMG_UINT32 ui32B, IMG_CHAR *pszV, IMG_UINT32 ui32N, IMG_UINT32 ui32C);
IMG_VOID rgx_bvnc_pack_hw(IMG_UINT32 *pui32OutBNC, IMG_CHAR *pszOutV, IMG_UINT32 ui32OutVMaxLen,
							IMG_UINT32 ui32B, IMG_CHAR *pszFwV, IMG_UINT32 ui32V, IMG_UINT32 ui32N, IMG_UINT32 ui32C);

#endif /*  __RGX_COMPAT_BVNC_H__ */

/******************************************************************************
 End of file (rgx_compat_bvnc.h)
******************************************************************************/

