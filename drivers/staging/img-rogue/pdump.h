/*************************************************************************/ /*!
@File
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Strictly Confidential.
*/ /**************************************************************************/
#ifndef _SERVICES_PDUMP_H_
#define _SERVICES_PDUMP_H_

#include "img_types.h"

typedef IMG_UINT32 PDUMP_FLAGS_T;


#define PDUMP_FLAGS_DEINIT		    0x20000000UL /*<! Output this entry to the de-initialisation section */

#define PDUMP_FLAGS_POWER		0x08000000UL /*<! Output this entry even when a power transition is ongoing */

#define PDUMP_FLAGS_CONTINUOUS		0x40000000UL /*<! Output this entry always regardless of framed capture range,
                                                      used by client applications being dumped. */
#define PDUMP_FLAGS_PERSISTENT		0x80000000UL /*<! Output this entry always regardless of app and range,
                                                      used by persistent processes e.g. compositor, window mgr etc/ */

#define PDUMP_FLAGS_DEBUG			0x00010000U  /*<! For internal debugging use */

#define PDUMP_FLAGS_NOHW			0x00000001U  /* For internal use: Skip sending instructions to the hardware */ 

#define PDUMP_FILEOFFSET_FMTSPEC "0x%08X"
typedef IMG_UINT32 PDUMP_FILEOFFSET_T;

#define PDUMP_PARAM_CHANNEL_NAME  "ParamChannel2"
#define PDUMP_SCRIPT_CHANNEL_NAME "ScriptChannel2"

#define PDUMP_CHANNEL_PARAM		0
#define PDUMP_CHANNEL_SCRIPT	1
#define PDUMP_NUM_CHANNELS      2

#define PDUMP_PARAM_0_FILE_NAME "%%0%%.prm"
#define PDUMP_PARAM_N_FILE_NAME "%%0%%_%02u.prm"


#endif /* _SERVICES_PDUMP_H_ */
