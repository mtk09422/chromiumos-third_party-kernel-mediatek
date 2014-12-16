/*************************************************************************/ /*!
@File
@Title          Misc memory management utility functions for Linux
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Strictly Confidential.
*/ /**************************************************************************/

#include <asm/io.h>

#include "img_defs.h"
#include "pvr_debug.h"
#include "mm.h"
#include "pvrsrv_memallocflags.h"
#include "devicemem_server_utils.h"

//#if defined(CONFIG_ARM)
//#define ioremap_cache(x,y) ioremap_cached(x,y)
//#endif

void *
_IORemapWrapper(IMG_CPU_PHYADDR BasePAddr,
               IMG_UINT32 ui32Bytes,
               IMG_UINT32 ui32MappingFlags,
               IMG_CHAR *pszFileName,
               IMG_UINT32 ui32Line)
{
	void *pvIORemapCookie;
	IMG_UINT32 ui32CPUCacheMode = DevmemCPUCacheMode(ui32MappingFlags);

	switch (ui32CPUCacheMode)
	{
		case PVRSRV_MEMALLOCFLAG_CPU_UNCACHED:
				pvIORemapCookie = (void *)ioremap_nocache(BasePAddr.uiAddr, ui32Bytes);
				break;
		case PVRSRV_MEMALLOCFLAG_CPU_WRITE_COMBINE:
#if defined(CONFIG_X86) || defined(CONFIG_ARM) || defined(CONFIG_ARM64)
				pvIORemapCookie = (void *)ioremap_wc(BasePAddr.uiAddr, ui32Bytes);
#else
				pvIORemapCookie = (void *)ioremap_nocache(BasePAddr.uiAddr, ui32Bytes);
#endif
				break;
		case PVRSRV_MEMALLOCFLAG_CPU_CACHED:
#if defined(CONFIG_X86) || defined(CONFIG_ARM)
				pvIORemapCookie = (void *)ioremap_cache(BasePAddr.uiAddr, ui32Bytes);
#else
				pvIORemapCookie = (void *)ioremap(BasePAddr.uiAddr, ui32Bytes);
#endif
				break;
		default:
				return IMG_NULL;
				break;
	}

    PVR_UNREFERENCED_PARAMETER(pszFileName);
    PVR_UNREFERENCED_PARAMETER(ui32Line);

    return pvIORemapCookie;
}


void
_IOUnmapWrapper(void *pvIORemapCookie, IMG_CHAR *pszFileName, IMG_UINT32 ui32Line)
{
    PVR_UNREFERENCED_PARAMETER(pszFileName);
    PVR_UNREFERENCED_PARAMETER(ui32Line);

    iounmap(pvIORemapCookie);
}
