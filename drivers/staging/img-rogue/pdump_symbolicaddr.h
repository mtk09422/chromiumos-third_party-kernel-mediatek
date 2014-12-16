/**************************************************************************/ /*!
@File
@Title          Abstraction of PDUMP symbolic address derivation
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Allows pdump functions to derive symbolic addresses on-the-fly
@License        Strictly Confidential.
*/ /***************************************************************************/

#ifndef SRVKM_PDUMP_SYMBOLICADDR_H
#define SRVKM_PDUMP_SYMBOLICADDR_H

#include "img_types.h"

#include "pvrsrv_error.h"

/* pdump symbolic addresses are generated on-the-fly with a callback */

typedef PVRSRV_ERROR (*PVRSRV_SYMADDRFUNCPTR)(IMG_HANDLE hPriv, IMG_UINT32 uiOffset, IMG_CHAR *pszSymbolicAddr, IMG_UINT32 ui32SymbolicAddrLen, IMG_UINT32 *pui32NewOffset);

#endif /* #ifndef SRVKM_PDUMP_SYMBOLICADDR_H */
