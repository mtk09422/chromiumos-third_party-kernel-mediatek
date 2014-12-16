/*************************************************************************/ /*!
@File
@Title          Version numbers and strings.
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Version numbers and strings for PVR Consumer services
                components.
@License        Strictly Confidential.
*/ /**************************************************************************/

#ifndef _PVRVERSION_H_
#define _PVRVERSION_H_

#define PVR_STR(X) #X
#define PVR_STR2(X) PVR_STR(X)

#define PVRVERSION_MAJ               1
#define PVRVERSION_MIN               5

#define PVRVERSION_FAMILY           "rogueddk"
#define PVRVERSION_BRANCHNAME       "MAIN"
#define PVRVERSION_BUILD             3263703//3262842
#define PVRVERSION_BSCONTROL        "Local:Unknown"

#define PVRVERSION_STRING           "Local:Unknown rogueddk MAIN@" PVR_STR2(PVRVERSION_BUILD)
#define PVRVERSION_STRING_SHORT     "1.5@" PVR_STR2(PVRVERSION_BUILD) " (MAIN)"

#define COPYRIGHT_TXT               "Copyright (c) Imagination Technologies Ltd. All Rights Reserved."

#define PVRVERSION_BUILD_HI          326
#define PVRVERSION_BUILD_LO          3703
#define PVRVERSION_STRING_NUMERIC    PVR_STR2(PVRVERSION_MAJ) "." PVR_STR2(PVRVERSION_MIN) "." PVR_STR2(PVRVERSION_BUILD_HI) "." PVR_STR2(PVRVERSION_BUILD_LO)

#define PVRVERSION_PACK(MAJ,MIN) ((((MAJ)&0xFFFF) << 16) | (((MIN)&0xFFFF) << 0))
#define PVRVERSION_UNPACK_MAJ(VERSION) (((VERSION) >> 16) & 0xFFFF)
#define PVRVERSION_UNPACK_MIN(VERSION) (((VERSION) >> 0) & 0xFFFF)

#endif /* _PVRVERSION_H_ */
