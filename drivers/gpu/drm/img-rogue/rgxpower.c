/*************************************************************************/ /*!
@File
@Title          Device specific power routines
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Device specific functions
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#include <stddef.h>
#include "rgxpower.h"
#include "rgx_fwif_km.h"
#include "rgxutils.h"
#include "rgxfwutils.h"
#include "pdump_km.h"
#include "rgxdefs_km.h"
#include "pvrsrv.h"
#include "pvr_debug.h"
#include "osfunc.h"
#include "rgxdebug.h"
#include "rgx_meta.h"
#include "devicemem_pdump.h"
#include "rgxapi_km.h"
#include "rgxtimecorr.h"
#if defined(PVRSRV_ENABLE_PROCESS_STATS)
#include "process_stats.h"
#endif

extern IMG_UINT32 g_ui32HostSampleIRQCount;

#if ! defined(FIX_HW_BRN_37453)
/*!
*******************************************************************************

 @Function	RGXEnableClocks

 @Description Enable RGX Clocks

 @Input psDevInfo - device info structure

 @Return   void

******************************************************************************/
static void RGXEnableClocks(PVRSRV_RGXDEV_INFO	*psDevInfo)
{
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS, "RGX clock: use default (automatic clock gating)");
}
#endif


/*!
*******************************************************************************

 @Function	_RGXInitSLC

 @Description Initialise RGX SLC

 @Input psDevInfo - device info structure

 @Return   void

******************************************************************************/
#if !defined(RGX_FEATURE_S7_CACHE_HIERARCHY)

#define RGX_INIT_SLC _RGXInitSLC

static void _RGXInitSLC(PVRSRV_RGXDEV_INFO	*psDevInfo)
{
	IMG_UINT32	ui32Reg;
	IMG_UINT32	ui32RegVal;

#if defined(FIX_HW_BRN_36492)
	/* Because the WA for this BRN forbids using SLC reset, need to inval it instead */
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS, "Invalidate the SLC");
	OSWriteHWReg32(psDevInfo->pvRegsBaseKM, 
			RGX_CR_SLC_CTRL_FLUSH_INVAL, RGX_CR_SLC_CTRL_FLUSH_INVAL_ALL_EN);
	PDUMPREG32(RGX_PDUMPREG_NAME, 
			RGX_CR_SLC_CTRL_FLUSH_INVAL, RGX_CR_SLC_CTRL_FLUSH_INVAL_ALL_EN, 
			PDUMP_FLAGS_CONTINUOUS);

	/* poll for completion */
	PVRSRVPollForValueKM((IMG_UINT32 *)((IMG_UINT8*)psDevInfo->pvRegsBaseKM + RGX_CR_SLC_STATUS0),
							 0x0,
							 RGX_CR_SLC_STATUS0_MASKFULL);

	PDUMPREGPOL(RGX_PDUMPREG_NAME,
				RGX_CR_SLC_STATUS0,
				0x0,
				RGX_CR_SLC_STATUS0_MASKFULL,
				PDUMP_FLAGS_CONTINUOUS,
				PDUMP_POLL_OPERATOR_EQUAL);
#endif
	 
	if (!PVRSRVSystemSnoopingOfCPUCache() && !PVRSRVSystemSnoopingOfDeviceCache())
	{
		PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS, "System has NO cache snooping");
	}
	else
	{
		if (PVRSRVSystemSnoopingOfCPUCache())
		{
			PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS, "System has CPU cache snooping");
		}
		if (PVRSRVSystemSnoopingOfDeviceCache())
		{
			PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS, "System has DEVICE cache snooping");
		}
	}

#if (RGX_FEATURE_SLC_SIZE_IN_BYTES < (128*1024))
	/*
	 * SLC Bypass control
	 */
	ui32Reg = RGX_CR_SLC_CTRL_BYPASS;

	/* Bypass SLC for textures if the SLC size is less than 128kB */
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS, "Bypass SLC for TPU");
	ui32RegVal = RGX_CR_SLC_CTRL_BYPASS_REQ_TPU_EN;

	OSWriteHWReg32(psDevInfo->pvRegsBaseKM, ui32Reg, ui32RegVal);
	PDUMPREG32(RGX_PDUMPREG_NAME, ui32Reg, ui32RegVal, PDUMP_FLAGS_CONTINUOUS);
#endif

	/*
	 * SLC Bypass control
	 */
	ui32Reg = RGX_CR_SLC_CTRL_MISC;
	ui32RegVal = RGX_CR_SLC_CTRL_MISC_ADDR_DECODE_MODE_PVR_HASH1;

	/* Bypass burst combiner if SLC line size is smaller than 1024 bits */
#if (RGX_FEATURE_SLC_CACHE_LINE_SIZE_BITS < 1024)
	ui32RegVal |= RGX_CR_SLC_CTRL_MISC_BYPASS_BURST_COMBINER_EN;
#endif
	OSWriteHWReg32(psDevInfo->pvRegsBaseKM, ui32Reg, ui32RegVal);
	PDUMPREG32(RGX_PDUMPREG_NAME, ui32Reg, ui32RegVal, PDUMP_FLAGS_CONTINUOUS);

}
#endif /* RGX_FEATURE_S7_CACHE_HIERARCHY */


/*!
*******************************************************************************

 @Function	RGXInitSLC3

 @Description Initialise RGX SLC3

 @Input psDevInfo - device info structure

 @Return   void

******************************************************************************/
#if defined(RGX_FEATURE_S7_CACHE_HIERARCHY)

#define RGX_INIT_SLC _RGXInitSLC3

static void _RGXInitSLC3(PVRSRV_RGXDEV_INFO	*psDevInfo)
{
	IMG_UINT32	ui32Reg;
	IMG_UINT32	ui32RegVal;


#if defined(HW_ERN_51468)
    /*
     * SLC control
     */
	ui32Reg = RGX_CR_SLC3_CTRL_MISC;
	ui32RegVal = RGX_CR_SLC3_CTRL_MISC_ADDR_DECODE_MODE_WEAVED_HASH;
	OSWriteHWReg32(psDevInfo->pvRegsBaseKM, ui32Reg, ui32RegVal);
	PDUMPREG32(RGX_PDUMPREG_NAME, ui32Reg, ui32RegVal, PDUMP_FLAGS_CONTINUOUS);

#else

    /*
     * SLC control
     */
	ui32Reg = RGX_CR_SLC3_CTRL_MISC;
	ui32RegVal = RGX_CR_SLC3_CTRL_MISC_ADDR_DECODE_MODE_SCRAMBLE_PVR_HASH;
	OSWriteHWReg32(psDevInfo->pvRegsBaseKM, ui32Reg, ui32RegVal);
	PDUMPREG32(RGX_PDUMPREG_NAME, ui32Reg, ui32RegVal, PDUMP_FLAGS_CONTINUOUS);

	/*
	 * SLC scramble bits
	 */
	{
		IMG_UINT32 i;
		IMG_UINT32 aui32ScrambleRegs[] = {
		    RGX_CR_SLC3_SCRAMBLE, 
		    RGX_CR_SLC3_SCRAMBLE2,
		    RGX_CR_SLC3_SCRAMBLE3,
		    RGX_CR_SLC3_SCRAMBLE4};

		IMG_UINT64 aui64ScrambleValues[] = {
#if (RGX_FEATURE_SLC_BANKS == 2)
		   IMG_UINT64_C(0x6965a99a55696a6a),
		   IMG_UINT64_C(0x6aa9aa66959aaa9a),
		   IMG_UINT64_C(0x9a5665965a99a566),
		   IMG_UINT64_C(0x5aa69596aa66669a)
#elif (RGX_FEATURE_SLC_BANKS == 4)
		   IMG_UINT64_C(0xc6788d722dd29ce4),
		   IMG_UINT64_C(0x7272e4e11b279372),
		   IMG_UINT64_C(0x87d872d26c6c4be1),
		   IMG_UINT64_C(0xe1b4878d4b36e478)
#elif (RGX_FEATURE_SLC_BANKS == 8)
		   IMG_UINT64_C(0x859d6569e8fac688),
		   IMG_UINT64_C(0xf285e1eae4299d33),
		   IMG_UINT64_C(0x1e1af2be3c0aa447)
#endif        
		};

		for (i = 0;
		     i < sizeof(aui64ScrambleValues)/sizeof(IMG_UINT64);
			 i++)
		{
			IMG_UINT32 ui32Reg = aui32ScrambleRegs[i];
			IMG_UINT64 ui64Value = aui64ScrambleValues[i];

			OSWriteHWReg64(psDevInfo->pvRegsBaseKM, ui32Reg, ui64Value);
			PDUMPREG64(RGX_PDUMPREG_NAME, ui32Reg, ui64Value, PDUMP_FLAGS_CONTINUOUS);
		}
	}
#endif

#if defined(HW_ERN_45914)
	/* Disable the forced SLC coherency which the hardware enables for compatibility with older pdumps */
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS, "RGXStart: disable forced SLC coherency");
	OSWriteHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_GARTEN_SLC, 0);
	PDUMPREG64(RGX_PDUMPREG_NAME, RGX_CR_GARTEN_SLC, 0, PDUMP_FLAGS_CONTINUOUS);
#endif

}
#endif


/*!
*******************************************************************************

 @Function	RGXInitBIF

 @Description Initialise RGX BIF

 @Input psDevInfo - device info structure

 @Return   void

******************************************************************************/
static void RGXInitBIF(PVRSRV_RGXDEV_INFO	*psDevInfo)
{
	PVRSRV_ERROR	eError;
	IMG_DEV_PHYADDR sPCAddr;

	/*
		Acquire the address of the Kernel Page Catalogue.
	*/
	eError = MMU_AcquireBaseAddr(psDevInfo->psKernelMMUCtx, &sPCAddr);
	PVR_ASSERT(eError == PVRSRV_OK);

	/* Sanity check Cat-Base address */
	PVR_ASSERT((((sPCAddr.uiAddr
			>> psDevInfo->ui32KernelCatBaseAlignShift)
			<< psDevInfo->ui32KernelCatBaseShift)
			& ~psDevInfo->ui64KernelCatBaseMask) == 0x0UL);

	/*
		Write the kernel catalogue base.
	*/
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS, "RGX firmware MMU Page Catalogue");

	if (psDevInfo->ui32KernelCatBaseIdReg != -1)
	{
		/* Set the mapping index */
		OSWriteHWReg32(psDevInfo->pvRegsBaseKM,
						psDevInfo->ui32KernelCatBaseIdReg,
						psDevInfo->ui32KernelCatBaseId);

		/* pdump mapping context */
		PDUMPREG32(RGX_PDUMPREG_NAME,
							psDevInfo->ui32KernelCatBaseIdReg,
							psDevInfo->ui32KernelCatBaseId,
							PDUMP_FLAGS_CONTINUOUS);
	}

	if (psDevInfo->ui32KernelCatBaseWordSize == 8)
	{
		/* Write the cat-base address */
		OSWriteHWReg64(psDevInfo->pvRegsBaseKM,
						psDevInfo->ui32KernelCatBaseReg,
						((sPCAddr.uiAddr
							>> psDevInfo->ui32KernelCatBaseAlignShift)
							<< psDevInfo->ui32KernelCatBaseShift)
							& psDevInfo->ui64KernelCatBaseMask);
	}
	else
	{
		/* Write the cat-base address */
		OSWriteHWReg32(psDevInfo->pvRegsBaseKM,
						psDevInfo->ui32KernelCatBaseReg,
						(IMG_UINT32)(((sPCAddr.uiAddr
							>> psDevInfo->ui32KernelCatBaseAlignShift)
							<< psDevInfo->ui32KernelCatBaseShift)
							& psDevInfo->ui64KernelCatBaseMask));
	}

	/* pdump catbase address */
	MMU_PDumpWritePageCatBase(psDevInfo->psKernelMMUCtx,
							  RGX_PDUMPREG_NAME,
							  psDevInfo->ui32KernelCatBaseReg,
							  psDevInfo->ui32KernelCatBaseWordSize,
							  psDevInfo->ui32KernelCatBaseAlignShift,
							  psDevInfo->ui32KernelCatBaseShift,
							  PDUMP_FLAGS_CONTINUOUS);

	/*
	 * Trusted META boot
	 */
#if defined(SUPPORT_TRUSTED_DEVICE)
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS, "RGXInitBIF: Trusted Device enabled");
	OSWriteHWReg32(psDevInfo->pvRegsBaseKM, RGX_CR_BIF_TRUST, RGX_CR_BIF_TRUST_ENABLE_EN);
	PDUMPREG32(RGX_PDUMPREG_NAME, RGX_CR_BIF_TRUST, RGX_CR_BIF_TRUST_ENABLE_EN, PDUMP_FLAGS_CONTINUOUS);
#endif

}

#if defined(RGX_FEATURE_AXI_ACELITE)
/*!
*******************************************************************************

 @Function	RGXAXIACELiteInit

 @Description Initialise AXI-ACE Lite interface

 @Input psDevInfo - device info structure

 @Return   void

******************************************************************************/
static void RGXAXIACELiteInit(PVRSRV_RGXDEV_INFO *psDevInfo)
{
	IMG_UINT32 ui32RegAddr;
	IMG_UINT64 ui64RegVal;

	ui32RegAddr = RGX_CR_AXI_ACE_LITE_CONFIGURATION;

	/* Setup AXI-ACE config. Set everything to outer cache */
	ui64RegVal =   (3U << RGX_CR_AXI_ACE_LITE_CONFIGURATION_AWDOMAIN_NON_SNOOPING_SHIFT) |
				   (3U << RGX_CR_AXI_ACE_LITE_CONFIGURATION_ARDOMAIN_NON_SNOOPING_SHIFT) |
				   (2U << RGX_CR_AXI_ACE_LITE_CONFIGURATION_ARDOMAIN_CACHE_MAINTENANCE_SHIFT)  |
				   (2U << RGX_CR_AXI_ACE_LITE_CONFIGURATION_AWDOMAIN_COHERENT_SHIFT) |
				   (2U << RGX_CR_AXI_ACE_LITE_CONFIGURATION_ARDOMAIN_COHERENT_SHIFT) |
				   (((IMG_UINT64) 1) << RGX_CR_AXI_ACE_LITE_CONFIGURATION_DISABLE_COHERENT_WRITELINEUNIQUE_SHIFT) |
				   (2U << RGX_CR_AXI_ACE_LITE_CONFIGURATION_AWCACHE_COHERENT_SHIFT) |
				   (2U << RGX_CR_AXI_ACE_LITE_CONFIGURATION_ARCACHE_COHERENT_SHIFT) |
				   (2U << RGX_CR_AXI_ACE_LITE_CONFIGURATION_ARCACHE_CACHE_MAINTENANCE_SHIFT);

	OSWriteHWReg64(psDevInfo->pvRegsBaseKM,
				   ui32RegAddr,
				   ui64RegVal);
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS, "Init AXI-ACE interface");
	PDUMPREG64(RGX_PDUMPREG_NAME, ui32RegAddr, ui64RegVal, PDUMP_FLAGS_CONTINUOUS);
}
#endif


/*!
*******************************************************************************

 @Function	RGXStart

 @Description

 (client invoked) chip-reset and initialisation

 @Input psDevInfo - device info structure

 @Return   PVRSRV_ERROR

******************************************************************************/
static PVRSRV_ERROR RGXStart(PVRSRV_RGXDEV_INFO	*psDevInfo, PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	PVRSRV_ERROR	eError = PVRSRV_OK;
	RGXFWIF_INIT	*psRGXFWInit;

#if defined(FIX_HW_BRN_37453)
	/* Force all clocks on*/
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS, "RGXStart: force all clocks on");
	OSWriteHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_CLK_CTRL, RGX_CR_CLK_CTRL_ALL_ON);
	PDUMPREG64(RGX_PDUMPREG_NAME, RGX_CR_CLK_CTRL, RGX_CR_CLK_CTRL_ALL_ON, PDUMP_FLAGS_CONTINUOUS);
#endif

#if defined(SUPPORT_SHARED_SLC)	&& !defined(FIX_HW_BRN_36492)
	/* When the SLC is shared, the SLC reset is performed by the System layer when calling
	 * RGXInitSLC (before any device uses it), therefore mask out the SLC bit to avoid
	 * soft_resetting it here. If HW_BRN_36492, the bit is already masked out. 
	 */
#define	RGX_CR_SOFT_RESET_ALL	(RGX_CR_SOFT_RESET_MASKFULL ^ RGX_CR_SOFT_RESET_SLC_EN)
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS, "RGXStart: Shared SLC (don't reset SLC as part of RGX reset)");
#else
#define	RGX_CR_SOFT_RESET_ALL	(RGX_CR_SOFT_RESET_MASKFULL)
#endif

#if defined(RGX_FEATURE_S7_TOP_INFRASTRUCTURE)
	/* Set RGX in soft-reset */
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS, "RGXStart: soft reset assert step 1");
	OSWriteHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET, RGX_S7_SOFT_RESET_DUSTS);
	PDUMPREG64(RGX_PDUMPREG_NAME, RGX_CR_SOFT_RESET, RGX_S7_SOFT_RESET_DUSTS, PDUMP_FLAGS_CONTINUOUS);

	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS, "RGXStart: soft reset assert step 2");
	OSWriteHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET, RGX_S7_SOFT_RESET_JONES_ALL | RGX_S7_SOFT_RESET_DUSTS);
	PDUMPREG64(RGX_PDUMPREG_NAME, RGX_CR_SOFT_RESET, RGX_S7_SOFT_RESET_JONES_ALL | RGX_S7_SOFT_RESET_DUSTS, PDUMP_FLAGS_CONTINUOUS);

	OSWriteHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET2, RGX_S7_SOFT_RESET2);
	PDUMPREG64(RGX_PDUMPREG_NAME, RGX_CR_SOFT_RESET2, RGX_S7_SOFT_RESET2, PDUMP_FLAGS_CONTINUOUS);

	/* Read soft-reset to fence previous write in order to clear the SOCIF pipeline */
	(void) OSReadHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET);
	PDUMPREGREAD64(RGX_PDUMPREG_NAME, RGX_CR_SOFT_RESET, PDUMP_FLAGS_CONTINUOUS);

	/* Take everything out of reset but META */
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS, "RGXStart: soft reset de-assert step 1 excluding META");
	OSWriteHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET, RGX_S7_SOFT_RESET_DUSTS | RGX_CR_SOFT_RESET_GARTEN_EN);
	PDUMPREG64(RGX_PDUMPREG_NAME, RGX_CR_SOFT_RESET, RGX_S7_SOFT_RESET_DUSTS | RGX_CR_SOFT_RESET_GARTEN_EN, PDUMP_FLAGS_CONTINUOUS);

	OSWriteHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET2, 0x0);
	PDUMPREG64(RGX_PDUMPREG_NAME, RGX_CR_SOFT_RESET2, 0x0, PDUMP_FLAGS_CONTINUOUS);
	(void) OSReadHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET);
	PDUMPREGREAD64(RGX_PDUMPREG_NAME, RGX_CR_SOFT_RESET, PDUMP_FLAGS_CONTINUOUS);

	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS, "RGXStart: soft reset de-assert step 2 excluding META");
	OSWriteHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET, RGX_CR_SOFT_RESET_GARTEN_EN);
	PDUMPREG64(RGX_PDUMPREG_NAME, RGX_CR_SOFT_RESET, RGX_CR_SOFT_RESET_GARTEN_EN, PDUMP_FLAGS_CONTINUOUS);
	(void) OSReadHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET);
	PDUMPREGREAD64(RGX_PDUMPREG_NAME, RGX_CR_SOFT_RESET, PDUMP_FLAGS_CONTINUOUS);
#else
	/* Set RGX in soft-reset */
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS, "RGXStart: soft reset everything");
	OSWriteHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET, RGX_CR_SOFT_RESET_ALL);
	PDUMPREG64(RGX_PDUMPREG_NAME, RGX_CR_SOFT_RESET, RGX_CR_SOFT_RESET_ALL, PDUMP_FLAGS_CONTINUOUS);

	/* Take Rascal and Dust out of reset */
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS, "RGXStart: Rascal and Dust out of reset");
	OSWriteHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET, RGX_CR_SOFT_RESET_ALL ^ RGX_CR_SOFT_RESET_RASCALDUSTS_EN);
	PDUMPREG64(RGX_PDUMPREG_NAME, RGX_CR_SOFT_RESET, RGX_CR_SOFT_RESET_ALL ^ RGX_CR_SOFT_RESET_RASCALDUSTS_EN, PDUMP_FLAGS_CONTINUOUS);

	/* Read soft-reset to fence previous write in order to clear the SOCIF pipeline */
	(void) OSReadHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET);
	PDUMPREGREAD64(RGX_PDUMPREG_NAME, RGX_CR_SOFT_RESET, PDUMP_FLAGS_CONTINUOUS);

	/* Take everything out of reset but META */
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS, "RGXStart: Take everything out of reset but META");
	OSWriteHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET, RGX_CR_SOFT_RESET_GARTEN_EN);
	PDUMPREG64(RGX_PDUMPREG_NAME, RGX_CR_SOFT_RESET, RGX_CR_SOFT_RESET_GARTEN_EN, PDUMP_FLAGS_CONTINUOUS);
#endif

#if ! defined(FIX_HW_BRN_37453)
	/*
	 * Enable clocks.
	 */
	RGXEnableClocks(psDevInfo);
#endif

	/*
	 * Initialise SLC.
	 */
#if !defined(SUPPORT_SHARED_SLC)	
	RGX_INIT_SLC(psDevInfo);
#endif

#if !defined(SUPPORT_META_SLAVE_BOOT)
	/* Configure META to Master boot */
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS, "RGXStart: META Master boot");
	OSWriteHWReg32(psDevInfo->pvRegsBaseKM, RGX_CR_META_BOOT, RGX_CR_META_BOOT_MODE_EN);
	PDUMPREG32(RGX_PDUMPREG_NAME, RGX_CR_META_BOOT, RGX_CR_META_BOOT_MODE_EN, PDUMP_FLAGS_CONTINUOUS);
#endif

	/* Set Garten IDLE to META idle and Set the Garten Wrapper BIF Fence address */
	{
		IMG_UINT64 ui64GartenConfig;

		/* Garten IDLE bit controlled by META */
		ui64GartenConfig = RGX_CR_MTS_GARTEN_WRAPPER_CONFIG_IDLE_CTRL_META;

		/* Set fence addr to the bootloader */
		ui64GartenConfig |= (RGXFW_BOOTLDR_DEVV_ADDR & ~RGX_CR_MTS_GARTEN_WRAPPER_CONFIG_FENCE_ADDR_CLRMSK);

		/* Set PC = 0 for fences */
		ui64GartenConfig &= RGX_CR_MTS_GARTEN_WRAPPER_CONFIG_FENCE_PC_BASE_CLRMSK;

#if defined(RGX_FEATURE_SLC_VIVT)
#if !defined(FIX_HW_BRN_51281)
		/* Ensure the META fences go all the way to external memory */
		ui64GartenConfig |= RGX_CR_MTS_GARTEN_WRAPPER_CONFIG_FENCE_SLC_COHERENT_EN;    /* SLC Coherent 1 */
		ui64GartenConfig &= RGX_CR_MTS_GARTEN_WRAPPER_CONFIG_FENCE_PERSISTENCE_CLRMSK; /* SLC Persistence 0 */
#endif

#else 
		/* Set SLC DM=META */
		ui64GartenConfig |= ((IMG_UINT64) RGXFW_SEGMMU_META_DM_ID) << RGX_CR_MTS_GARTEN_WRAPPER_CONFIG_FENCE_DM_SHIFT;

#endif

		PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS, "RGXStart: Configure META wrapper");
		OSWriteHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_MTS_GARTEN_WRAPPER_CONFIG, ui64GartenConfig);
		PDUMPREG64(RGX_PDUMPREG_NAME, RGX_CR_MTS_GARTEN_WRAPPER_CONFIG, ui64GartenConfig, PDUMP_FLAGS_CONTINUOUS);
	}

#if defined(RGX_FEATURE_AXI_ACELITE)
	/*
		We must init the AXI-ACE interface before 1st BIF transaction
	*/
	RGXAXIACELiteInit(psDevInfo);
#endif

	/*
	 * Initialise BIF.
	 */
	RGXInitBIF(psDevInfo);

	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS, "RGXStart: Take META out of reset");
	/* need to wait for at least 16 cycles before taking meta out of reset ... */
	PVRSRVSystemWaitCycles(psDevConfig, 32);
	PDUMPIDLWITHFLAGS(32, PDUMP_FLAGS_CONTINUOUS);
	
	OSWriteHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET, 0x0);
	PDUMPREG64(RGX_PDUMPREG_NAME, RGX_CR_SOFT_RESET, 0x0, PDUMP_FLAGS_CONTINUOUS);

	(void) OSReadHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET);
	PDUMPREGREAD64(RGX_PDUMPREG_NAME, RGX_CR_SOFT_RESET, PDUMP_FLAGS_CONTINUOUS);
	
	/* ... and afterwards */
	PVRSRVSystemWaitCycles(psDevConfig, 32);
	PDUMPIDLWITHFLAGS(32, PDUMP_FLAGS_CONTINUOUS);
#if defined(FIX_HW_BRN_37453)
	/* we rely on the 32 clk sleep from above */

	/* switch clocks back to auto */
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS, "RGXStart: set clocks back to auto");
	OSWriteHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_CLK_CTRL, RGX_CR_CLK_CTRL_ALL_AUTO);
	PDUMPREG64(RGX_PDUMPREG_NAME, RGX_CR_CLK_CTRL, RGX_CR_CLK_CTRL_ALL_AUTO, PDUMP_FLAGS_CONTINUOUS);
#endif

	/*
	 * Start the firmware.
	 */
#if defined(SUPPORT_META_SLAVE_BOOT)
	RGXStartFirmware(psDevInfo);
#else
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS, "RGXStart: RGX Firmware Master boot Start");
#endif
	
	OSMemoryBarrier();

	/* Check whether the FW has started by polling on bFirmwareStarted flag */
	eError = DevmemAcquireCpuVirtAddr(psDevInfo->psRGXFWIfInitMemDesc,
									  (void **)&psRGXFWInit);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"RGXStart: Failed to acquire kernel fw if ctl (%u)", eError));
		return eError;
	}

	if (PVRSRVPollForValueKM((IMG_UINT32 *)&psRGXFWInit->bFirmwareStarted,
							 IMG_TRUE,
							 0xFFFFFFFF) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "RGXStart: Polling for 'FW started' flag failed."));
		eError = PVRSRV_ERROR_TIMEOUT;
		DevmemReleaseCpuVirtAddr(psDevInfo->psRGXFWIfInitMemDesc);
		return eError;
	}

#if defined(PDUMP)
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS, "Wait for the Firmware to start.");
	eError = DevmemPDumpDevmemPol32(psDevInfo->psRGXFWIfInitMemDesc,
											offsetof(RGXFWIF_INIT, bFirmwareStarted),
											IMG_TRUE,
											0xFFFFFFFFU,
											PDUMP_POLL_OPERATOR_EQUAL,
											PDUMP_FLAGS_CONTINUOUS);
	
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "RGXStart: problem pdumping POL for psRGXFWIfInitMemDesc (%d)", eError));
		DevmemReleaseCpuVirtAddr(psDevInfo->psRGXFWIfInitMemDesc);
		return eError;
	}
#endif

#if defined(PVRSRV_ENABLE_PROCESS_STATS)
	SetFirmwareStartTime(psRGXFWInit->ui32FirmwareStartedTimeStamp);
#endif

	DevmemReleaseCpuVirtAddr(psDevInfo->psRGXFWIfInitMemDesc);

	/* Enable Sys Bus security */
#if defined(SUPPORT_TRUSTED_DEVICE)
	OSWriteHWReg32(psDevInfo->pvRegsBaseKM, RGX_CR_SYS_BUS_SECURE, RGX_CR_SYS_BUS_SECURE_ENABLE_EN);
	PDUMPREG32(RGX_PDUMPREG_NAME, RGX_CR_SYS_BUS_SECURE, RGX_CR_SYS_BUS_SECURE_ENABLE_EN, PDUMP_FLAGS_CONTINUOUS);
#endif
	

	return eError;
}


/*!
*******************************************************************************

 @Function	RGXStop

 @Description Stop RGX in preparation for power down

 @Input psDevInfo - RGX device info

 @Return   PVRSRV_ERROR

******************************************************************************/
static PVRSRV_ERROR RGXStop(PVRSRV_RGXDEV_INFO	*psDevInfo)

{
	PVRSRV_ERROR		eError; 


	eError = RGXRunScript(psDevInfo, psDevInfo->psScripts->asDeinitCommands, RGX_MAX_DEINIT_COMMANDS, PDUMP_FLAGS_CONTINUOUS, NULL);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"RGXStop: RGXRunScript failed (%d)", eError));
		return eError;
	}


	return PVRSRV_OK;
}

/*
	RGXInitSLC
*/
#if defined(SUPPORT_SHARED_SLC)
PVRSRV_ERROR RGXInitSLC(IMG_HANDLE hDevHandle)
{

	PVRSRV_DEVICE_NODE	*psDeviceNode = hDevHandle;
	PVRSRV_RGXDEV_INFO	*psDevInfo;

	if (psDeviceNode == NULL)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	psDevInfo = psDeviceNode->pvDevice;

#if !defined(FIX_HW_BRN_36492)

	/* reset the SLC */
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS, "RGXInitSLC: soft reset SLC");
	OSWriteHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET, RGX_CR_SOFT_RESET_SLC_EN);
	PDUMPREG64(RGX_PDUMPREG_NAME, RGX_CR_SOFT_RESET, RGX_CR_SOFT_RESET_SLC_EN, PDUMP_FLAGS_CONTINUOUS);

	/* Read soft-reset to fence previous write in order to clear the SOCIF pipeline */
	OSReadHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET);
	PDUMPREGREAD64(RGX_PDUMPREG_NAME, RGX_CR_SOFT_RESET, PDUMP_FLAGS_CONTINUOUS);

	/* Take everything out of reset */
	OSWriteHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET, 0);
	PDUMPREG64(RGX_PDUMPREG_NAME, RGX_CR_SOFT_RESET, 0, PDUMP_FLAGS_CONTINUOUS);
#endif

	RGX_INIT_SLC(psDevInfo);

	return PVRSRV_OK;
}
#endif

static void _RGXGpuUtilFWCBEntryAdd(PVRSRV_DEVICE_NODE *psDeviceNode, IMG_UINT64 ui64TimeStamp, IMG_UINT32 ui32Type)
{
	PVRSRV_RGXDEV_INFO    *psDevInfo = psDeviceNode->pvDevice;
	RGXFWIF_GPU_UTIL_FWCB *psRGXFWIfGpuUtilFWCb = psDevInfo->psRGXFWIfGpuUtilFWCb;

	/* Populate the GPU utilisation CB (the GPU state is the same as in the last FW entry in this CB) */
	switch(ui32Type)
	{
		case RGXFWIF_GPU_UTIL_FWCB_TYPE_CRTIME:
		case RGXFWIF_GPU_UTIL_FWCB_TYPE_END_CRTIME:
			psRGXFWIfGpuUtilFWCb->aui64CB[psRGXFWIfGpuUtilFWCb->ui32WriteOffset] =
			        ((ui64TimeStamp << RGXFWIF_GPU_UTIL_FWCB_TIMER_SHIFT) & RGXFWIF_GPU_UTIL_FWCB_CR_TIMER_MASK) |
			        (((IMG_UINT64)psRGXFWIfGpuUtilFWCb->ui32LastGpuUtilState << RGXFWIF_GPU_UTIL_FWCB_STATE_SHIFT) & RGXFWIF_GPU_UTIL_FWCB_STATE_MASK) |
			        (((IMG_UINT64)ui32Type << RGXFWIF_GPU_UTIL_FWCB_TYPE_SHIFT) & RGXFWIF_GPU_UTIL_FWCB_TYPE_MASK) |
			        (((IMG_UINT64)psRGXFWIfGpuUtilFWCb->ui32CurrentDVFSId << RGXFWIF_GPU_UTIL_FWCB_ID_SHIFT) & RGXFWIF_GPU_UTIL_FWCB_ID_MASK);
			break;

		case RGXFWIF_GPU_UTIL_FWCB_TYPE_POWER_ON:
		case RGXFWIF_GPU_UTIL_FWCB_TYPE_POWER_OFF:
			psRGXFWIfGpuUtilFWCb->aui64CB[psRGXFWIfGpuUtilFWCb->ui32WriteOffset] =
			        ((ui64TimeStamp << RGXFWIF_GPU_UTIL_FWCB_TIMER_SHIFT) & RGXFWIF_GPU_UTIL_FWCB_OS_TIMER_MASK) |
			        (((IMG_UINT64)psRGXFWIfGpuUtilFWCb->ui32LastGpuUtilState << RGXFWIF_GPU_UTIL_FWCB_STATE_SHIFT) & RGXFWIF_GPU_UTIL_FWCB_STATE_MASK) |
			        (((IMG_UINT64)ui32Type << RGXFWIF_GPU_UTIL_FWCB_TYPE_SHIFT) & RGXFWIF_GPU_UTIL_FWCB_TYPE_MASK);
			break;

		default:
			PVR_DPF((PVR_DBG_ERROR,"RGXFWCBEntryAdd: Wrong entry type"));
			break;
	}

	psRGXFWIfGpuUtilFWCb->ui32WriteOffset++;
	if(psRGXFWIfGpuUtilFWCb->ui32WriteOffset >= RGXFWIF_GPU_UTIL_FWCB_SIZE)
	{
		psRGXFWIfGpuUtilFWCb->ui32WriteOffset = 0;
	}
}

/*
	RGXPrePowerState
*/
PVRSRV_ERROR RGXPrePowerState (IMG_HANDLE				hDevHandle,
							   PVRSRV_DEV_POWER_STATE	eNewPowerState,
							   PVRSRV_DEV_POWER_STATE	eCurrentPowerState,
							   IMG_BOOL					bForced)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	if ((eNewPowerState != eCurrentPowerState) &&
		(eNewPowerState != PVRSRV_DEV_POWER_STATE_ON))
	{
		PVRSRV_DEVICE_NODE	*psDeviceNode = hDevHandle;
		PVRSRV_RGXDEV_INFO	*psDevInfo = psDeviceNode->pvDevice;
		RGXFWIF_KCCB_CMD	sPowCmd;
		RGXFWIF_TRACEBUF	*psFWTraceBuf = psDevInfo->psRGXFWIfTraceBuf;
		IMG_UINT32			ui32DM;

		/* Send the Power off request to the FW */
		sPowCmd.eCmdType = RGXFWIF_KCCB_CMD_POW;
		sPowCmd.uCmdData.sPowData.ePowType = RGXFWIF_POW_OFF_REQ;
		sPowCmd.uCmdData.sPowData.uPoweReqData.bForced = bForced;

		SyncPrimSet(psDevInfo->psPowSyncPrim, 0);

		/* Send one pow command to each DM to make sure we flush all the DMs pipelines */
		for (ui32DM = 0; ui32DM < RGXFWIF_DM_MAX; ui32DM++)
		{
			eError = RGXSendCommandRaw(psDevInfo,
					ui32DM,
					&sPowCmd,
					sizeof(sPowCmd),
					0);
			if (eError != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR,"RGXPrePowerState: Failed to send Power off request for DM%d", ui32DM));
				return eError;
			}
		}

		/* Wait for the firmware to complete processing. It cannot use PVRSRVWaitForValueKM as it relies 
		   on the EventObject which is signalled in this MISR */
		eError = PVRSRVPollForValueKM(psDevInfo->psPowSyncPrim->pui32LinAddr, 0x1, 0xFFFFFFFF);

		/* Check the Power state after the answer */
		if (eError == PVRSRV_OK)	
		{
			/* Finally, de-initialise some registers. */
			if (psFWTraceBuf->ePowState == RGXFWIF_POW_OFF)
			{
#if !defined(NO_HARDWARE)
				/* Wait for the pending META to host interrupts to come back. */
				eError = PVRSRVPollForValueKM(&g_ui32HostSampleIRQCount,
									          psDevInfo->psRGXFWIfTraceBuf->ui32InterruptCount,
									          0xffffffff);
#endif /* NO_HARDWARE */

				if (eError != PVRSRV_OK)
				{
					PVR_DPF((PVR_DBG_ERROR,"RGXPrePowerState: Wait for pending interrupts failed. Host:%d, FW: %d",
					g_ui32HostSampleIRQCount,
					psDevInfo->psRGXFWIfTraceBuf->ui32InterruptCount));
				}
				else
				{
					IMG_UINT64 ui64CRTimeStamp = RGXReadHWTimerReg(psDevInfo);
					IMG_UINT64 ui64OSTimeStamp = OSClockus64();

					/* Add two entries to the GPU utilisation FWCB (current CR timestamp and current OS timestamp)
					 * so that RGXGetGpuUtilStats() can link a power-on period to a previous power-off period (this one) */
					_RGXGpuUtilFWCBEntryAdd(psDeviceNode, ui64CRTimeStamp, RGXFWIF_GPU_UTIL_FWCB_TYPE_END_CRTIME);
					_RGXGpuUtilFWCBEntryAdd(psDeviceNode, ui64OSTimeStamp, RGXFWIF_GPU_UTIL_FWCB_TYPE_POWER_OFF);

					/* Update GPU frequency and timer correlation related data */
					RGXGPUFreqCalibratePrePowerState(psDeviceNode);

					psDevInfo->bRGXPowered = IMG_FALSE;

					eError = RGXStop(psDevInfo);
					if (eError != PVRSRV_OK)
					{
						PVR_DPF((PVR_DBG_ERROR,"RGXPrePowerState: RGXStop failed (%s)", PVRSRVGetErrorStringKM(eError)));
						eError = PVRSRV_ERROR_DEVICE_POWER_CHANGE_FAILURE;
					}
				}
			}
			else
			{
				/* the sync was updated bu the pow state isn't off -> the FW denied the transition */
				eError = PVRSRV_ERROR_DEVICE_POWER_CHANGE_DENIED;

				if (bForced)
				{	/* It is an error for a forced request to be denied */
					PVR_DPF((PVR_DBG_ERROR,"RGXPrePowerState: Failure to power off during a forced power off. FW: %d", psFWTraceBuf->ePowState));
				}
			}
		}
		else if (eError == PVRSRV_ERROR_TIMEOUT)
		{
			/* timeout waiting for the FW to ack the request: return timeout */
			PVR_DPF((PVR_DBG_WARNING,"RGXPrePowerState: Timeout waiting for powoff ack from the FW"));
		}
		else
		{
			PVR_DPF((PVR_DBG_ERROR,"RGXPrePowerState: Error waiting for powoff ack from the FW (%s)", PVRSRVGetErrorStringKM(eError)));
			eError = PVRSRV_ERROR_DEVICE_POWER_CHANGE_FAILURE;
		}

	}

	return eError;
}


/*
	RGXPostPowerState
*/
PVRSRV_ERROR RGXPostPowerState (IMG_HANDLE				hDevHandle,
								PVRSRV_DEV_POWER_STATE	eNewPowerState,
								PVRSRV_DEV_POWER_STATE	eCurrentPowerState,
								IMG_BOOL				bForced)
{
	if ((eNewPowerState != eCurrentPowerState) &&
		(eCurrentPowerState != PVRSRV_DEV_POWER_STATE_ON))
	{
		PVRSRV_ERROR		 eError;
		PVRSRV_DEVICE_NODE	 *psDeviceNode = hDevHandle;
		PVRSRV_RGXDEV_INFO	 *psDevInfo = psDeviceNode->pvDevice;
		PVRSRV_DEVICE_CONFIG *psDevConfig = psDeviceNode->psDevConfig;

		if (eCurrentPowerState == PVRSRV_DEV_POWER_STATE_OFF)
		{
			IMG_UINT64 ui64CRTimeStamp = RGXReadHWTimerReg(psDevInfo);
			IMG_UINT64 ui64OSTimeStamp = OSClockus64();

			/* Update GPU frequency and timer correlation related data */
			RGXGPUFreqCalibratePostPowerState(psDeviceNode);

			/* Add two entries to the GPU utilisation FWCB (current OS timestamp and current CR timestamp)
			 * so that RGXGetGpuUtilStats() can link a power-on (this one) period to a  previous power-off period */
			_RGXGpuUtilFWCBEntryAdd(psDeviceNode, ui64OSTimeStamp, RGXFWIF_GPU_UTIL_FWCB_TYPE_POWER_ON);
			_RGXGpuUtilFWCBEntryAdd(psDeviceNode, ui64CRTimeStamp, RGXFWIF_GPU_UTIL_FWCB_TYPE_CRTIME);

			/*
				Run the RGX init script.
			*/

			eError = RGXStart(psDevInfo, psDevConfig);
			if (eError != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR,"RGXPostPowerState: RGXStart failed"));
				return eError;
			}

			psDevInfo->bRGXPowered = IMG_TRUE;

		}
	}

	PDUMPCOMMENT("RGXPostPowerState: Current state: %d, New state: %d", eCurrentPowerState, eNewPowerState);

	return PVRSRV_OK;
}


/*
	RGXPreClockSpeedChange
*/
PVRSRV_ERROR RGXPreClockSpeedChange (IMG_HANDLE				hDevHandle,
									 PVRSRV_DEV_POWER_STATE	eCurrentPowerState)
{
	PVRSRV_ERROR		eError = PVRSRV_OK;
	PVRSRV_DEVICE_NODE	*psDeviceNode = hDevHandle;
	PVRSRV_RGXDEV_INFO	*psDevInfo = psDeviceNode->pvDevice;
	RGX_DATA			*psRGXData = (RGX_DATA*)psDeviceNode->psDevConfig->hDevData;
	RGXFWIF_TRACEBUF	*psFWTraceBuf = psDevInfo->psRGXFWIfTraceBuf;

	PVR_UNREFERENCED_PARAMETER(psRGXData);

	PVR_DPF((PVR_DBG_MESSAGE,"RGXPreClockSpeedChange: RGX clock speed was %uHz",
			psRGXData->psRGXTimingInfo->ui32CoreClockSpeed));

    if ((eCurrentPowerState != PVRSRV_DEV_POWER_STATE_OFF)
		&& (psFWTraceBuf->ePowState != RGXFWIF_POW_OFF))
	{
		/* Update GPU frequency and timer correlation related data */
		RGXGPUFreqCalibratePreClockSpeedChange(psDeviceNode);
	}

	return eError;
}


/*
	RGXPostClockSpeedChange
*/
PVRSRV_ERROR RGXPostClockSpeedChange (IMG_HANDLE				hDevHandle,
									  PVRSRV_DEV_POWER_STATE	eCurrentPowerState)
{
	PVRSRV_DEVICE_NODE	*psDeviceNode = hDevHandle;
	PVRSRV_RGXDEV_INFO	*psDevInfo = psDeviceNode->pvDevice;
	RGX_DATA			*psRGXData = (RGX_DATA*)psDeviceNode->psDevConfig->hDevData;
	PVRSRV_ERROR		eError = PVRSRV_OK;
	RGXFWIF_TRACEBUF	*psFWTraceBuf = psDevInfo->psRGXFWIfTraceBuf;
	IMG_UINT32 		ui32NewClockSpeed = psRGXData->psRGXTimingInfo->ui32CoreClockSpeed;

	/* Update runtime configuration with the new value */
	psDevInfo->psRGXFWIfRuntimeCfg->ui32CoreClockSpeed = ui32NewClockSpeed;

    if ((eCurrentPowerState != PVRSRV_DEV_POWER_STATE_OFF) 
		&& (psFWTraceBuf->ePowState != RGXFWIF_POW_OFF))
	{
		RGXFWIF_KCCB_CMD	sCOREClkSpeedChangeCmd;

		RGXGPUFreqCalibratePostClockSpeedChange(psDeviceNode, ui32NewClockSpeed);

		sCOREClkSpeedChangeCmd.eCmdType = RGXFWIF_KCCB_CMD_CORECLKSPEEDCHANGE;
		sCOREClkSpeedChangeCmd.uCmdData.sCORECLKSPEEDCHANGEData.ui32NewClockSpeed = ui32NewClockSpeed;

		/* Ensure the new clock speed is written to memory before requesting the FW to read it */
		OSMemoryBarrier();

		PDUMPCOMMENT("Scheduling CORE clock speed change command");

		PDUMPPOWCMDSTART();
		eError = RGXSendCommandRaw(psDeviceNode->pvDevice,
		                           RGXFWIF_DM_GP,
		                           &sCOREClkSpeedChangeCmd,
		                           sizeof(sCOREClkSpeedChangeCmd),
		                           0);
		PDUMPPOWCMDEND();

		if (eError != PVRSRV_OK)
		{
			PDUMPCOMMENT("Scheduling CORE clock speed change command failed");
			PVR_DPF((PVR_DBG_ERROR, "RGXPostClockSpeedChange: Scheduling KCCB command failed. Error:%u", eError));
			return eError;
		}
 
		PVR_DPF((PVR_DBG_MESSAGE,"RGXPostClockSpeedChange: RGX clock speed changed to %uHz",
				psRGXData->psRGXTimingInfo->ui32CoreClockSpeed));
	}

	return eError;
}


/*!
******************************************************************************

 @Function	RGXDustCountChange

 @Description

	Does change of number of DUSTs

 @Input	   hDevHandle : RGX Device Node
 @Input	   ui32NumberOfDusts : Number of DUSTs to make transition to

 @Return   PVRSRV_ERROR :

******************************************************************************/
PVRSRV_ERROR RGXDustCountChange(IMG_HANDLE				hDevHandle,
								IMG_UINT32				ui32NumberOfDusts)
{

	PVRSRV_DEVICE_NODE	*psDeviceNode = hDevHandle;
	PVRSRV_RGXDEV_INFO	*psDevInfo = psDeviceNode->pvDevice;
	PVRSRV_ERROR		eError;
	RGXFWIF_KCCB_CMD 	sDustCountChange;
	IMG_UINT32			ui32MaxAvailableDusts = MAX(1, RGX_FEATURE_NUM_CLUSTERS/2);
	RGXFWIF_RUNTIME_CFG *psRuntimeCfg = psDevInfo->psRGXFWIfRuntimeCfg;

	if (ui32NumberOfDusts > ui32MaxAvailableDusts)
	{
		eError = PVRSRV_ERROR_INVALID_PARAMS;
		PVR_DPF((PVR_DBG_ERROR,
				"RGXDustCountChange: Invalid number of DUSTs (%u) while expecting value within <0,%u>. Error:%u",
				ui32NumberOfDusts,
				ui32MaxAvailableDusts,
				eError));
		return eError;
	}

	psRuntimeCfg->ui32DefaultDustsNumInit = ui32NumberOfDusts;

	#if !defined(NO_HARDWARE)
	{
		RGXFWIF_TRACEBUF 	*psFWTraceBuf = psDevInfo->psRGXFWIfTraceBuf;

		if (psFWTraceBuf->ePowState == RGXFWIF_POW_OFF)
		{
			return PVRSRV_OK;
		}

		if (psFWTraceBuf->ePowState != RGXFWIF_POW_FORCED_IDLE)
		{
			eError = PVRSRV_ERROR_DEVICE_POWER_CHANGE_DENIED;
			PVR_DPF((PVR_DBG_ERROR,"RGXDustCountChange: Attempt to change dust count when not IDLE"));
			return eError;
		}
	}
	#endif

	SyncPrimSet(psDevInfo->psPowSyncPrim, 0);

	sDustCountChange.eCmdType = RGXFWIF_KCCB_CMD_POW;
	sDustCountChange.uCmdData.sPowData.ePowType = RGXFWIF_POW_NUMDUST_CHANGE;
	sDustCountChange.uCmdData.sPowData.uPoweReqData.ui32NumOfDusts = ui32NumberOfDusts;

	PDUMPCOMMENT("Scheduling command to change Dust Count to %u", ui32NumberOfDusts);
	eError = RGXSendCommandRaw(psDeviceNode->pvDevice,
				RGXFWIF_DM_GP,
				&sDustCountChange,
				sizeof(sDustCountChange),
				0);

	if (eError != PVRSRV_OK)
	{
		PDUMPCOMMENT("Scheduling command to change Dust Count failed. Error:%u", eError);
		PVR_DPF((PVR_DBG_ERROR, "RGXDustCountChange: Scheduling KCCB to change Dust Count failed. Error:%u", eError));
		return eError;
	}

	/* Wait for the firmware to answer. */
	eError = PVRSRVPollForValueKM(psDevInfo->psPowSyncPrim->pui32LinAddr, 0x1, 0xFFFFFFFF);

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"RGXDustCountChange: Timeout waiting for idle request"));
		return eError;
	}

#if defined(PDUMP)
	PDUMPCOMMENT("RGXDustCountChange: Poll for Kernel SyncPrim [0x%p] on DM %d ", psDevInfo->psPowSyncPrim->pui32LinAddr, RGXFWIF_DM_GP);

	SyncPrimPDumpPol(psDevInfo->psPowSyncPrim,
					1,
					0xffffffff,
					PDUMP_POLL_OPERATOR_EQUAL,
					0);
#endif

	return PVRSRV_OK;
}
/*
 @Function	RGXAPMLatencyChange
*/
PVRSRV_ERROR RGXAPMLatencyChange(IMG_HANDLE				hDevHandle,
				IMG_UINT32				ui32ActivePMLatencyms,
				IMG_BOOL				bActivePMLatencyPersistant)
{

	PVRSRV_DEVICE_NODE	*psDeviceNode = hDevHandle;
	PVRSRV_RGXDEV_INFO	*psDevInfo = psDeviceNode->pvDevice;
	PVRSRV_ERROR		eError;
	RGXFWIF_RUNTIME_CFG	*psRuntimeCfg = psDevInfo->psRGXFWIfRuntimeCfg;
	PVRSRV_DEV_POWER_STATE	ePowerState;

	eError = PVRSRVPowerLock();
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"RGXAPMLatencyChange: Failed to acquire power lock"));
		return eError;
	}

	/* Update runtime configuration with the new values */
	psRuntimeCfg->ui32ActivePMLatencyms = ui32ActivePMLatencyms;
	psRuntimeCfg->bActivePMLatencyPersistant = bActivePMLatencyPersistant;

	eError = PVRSRVGetDevicePowerState(psDeviceNode->sDevId.ui32DeviceIndex, &ePowerState);

	if ((eError == PVRSRV_OK) && (ePowerState != PVRSRV_DEV_POWER_STATE_OFF))
	{
		RGXFWIF_KCCB_CMD	sActivePMLatencyChange;
		sActivePMLatencyChange.eCmdType = RGXFWIF_KCCB_CMD_POW;
		sActivePMLatencyChange.uCmdData.sPowData.ePowType = RGXFWIF_POW_APM_LATENCY_CHANGE;
		sActivePMLatencyChange.uCmdData.sPowData.uPoweReqData.ui32ActivePMLatencyms = ui32ActivePMLatencyms;

		/* Ensure the new APM latency is written to memory before requesting the FW to read it */
		OSMemoryBarrier();

		PDUMPCOMMENT("Scheduling command to change APM latency to %u", ui32ActivePMLatencyms);
		eError = RGXSendCommandRaw(psDeviceNode->pvDevice,
					RGXFWIF_DM_GP,
					&sActivePMLatencyChange,
					sizeof(sActivePMLatencyChange),
					0);

		if (eError != PVRSRV_OK)
		{
			PDUMPCOMMENT("Scheduling command to change APM latency failed. Error:%u", eError);
			PVR_DPF((PVR_DBG_ERROR, "RGXAPMLatencyChange: Scheduling KCCB to change APM latency failed. Error:%u", eError));
			return eError;
		}
	}

	PVRSRVPowerUnlock();

	return PVRSRV_OK;
}

/*
	RGXActivePowerRequest
*/
PVRSRV_ERROR RGXActivePowerRequest(IMG_HANDLE hDevHandle)
{
	PVRSRV_ERROR eError = PVRSRV_OK;
	PVRSRV_DEVICE_NODE	*psDeviceNode = hDevHandle;

	PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
	RGXFWIF_TRACEBUF *psFWTraceBuf = psDevInfo->psRGXFWIfTraceBuf;

	OSAcquireBridgeLock();
	/* NOTE: If this function were to wait for event object attempt should be
	   made to prevent releasing bridge lock during sleep. Bridge lock should
	   be held during sleep. */

	/* Powerlock to avoid further requests from racing with the FW hand-shake from now on
	   (previous kicks to this point are detected by the FW) */
	eError = PVRSRVPowerLock();
	if(eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"RGXActivePowerRequest: Failed to acquire PowerLock (device index: %d, error: %s)", 
					psDeviceNode->sDevId.ui32DeviceIndex,
					PVRSRVGetErrorStringKM(eError)));
		goto _RGXActivePowerRequest_PowerLock_failed;
	}

	/* Check again for IDLE once we have the power lock */
	if (psFWTraceBuf->ePowState == RGXFWIF_POW_IDLE)
	{

		psDevInfo->ui32ActivePMReqTotal++;

#if defined(PVRSRV_ENABLE_PROCESS_STATS)
        SetFirmwareHandshakeIdleTime(RGXReadHWTimerReg(psDevInfo)-psFWTraceBuf->ui64StartIdleTime);
#endif

		PDUMPPOWCMDSTART();
		eError = 
			PVRSRVSetDevicePowerStateKM(psDeviceNode->sDevId.ui32DeviceIndex,
					PVRSRV_DEV_POWER_STATE_OFF,
					IMG_FALSE); /* forced */
		PDUMPPOWCMDEND();

		if (eError == PVRSRV_OK)
		{
			psDevInfo->ui32ActivePMReqOk++;
		}
		else if (eError == PVRSRV_ERROR_DEVICE_POWER_CHANGE_DENIED)
		{
			psDevInfo->ui32ActivePMReqDenied++;
		}

	}

	PVRSRVPowerUnlock();

_RGXActivePowerRequest_PowerLock_failed:
	OSReleaseBridgeLock();

	return eError;

}
/*
	RGXForcedIdleRequest
*/
PVRSRV_ERROR RGXForcedIdleRequest(IMG_HANDLE hDevHandle, IMG_BOOL bDeviceOffPermitted)
{
	PVRSRV_DEVICE_NODE	*psDeviceNode = hDevHandle;
	PVRSRV_RGXDEV_INFO	*psDevInfo = psDeviceNode->pvDevice;
	RGXFWIF_KCCB_CMD	sPowCmd;
	PVRSRV_ERROR		eError;

#if !defined(NO_HARDWARE)
	RGXFWIF_TRACEBUF	*psFWTraceBuf = psDevInfo->psRGXFWIfTraceBuf;

	/* Firmware already forced idle */
	if (psFWTraceBuf->ePowState == RGXFWIF_POW_FORCED_IDLE)
	{
		return PVRSRV_OK;
	}

	/* Firmware is not powered. Sometimes this is permitted, for instance we were forcing idle to power down. */
	if (psFWTraceBuf->ePowState == RGXFWIF_POW_OFF)
	{
		return (bDeviceOffPermitted) ? PVRSRV_OK : PVRSRV_ERROR_DEVICE_IDLE_REQUEST_DENIED;
	}
#endif

	SyncPrimSet(psDevInfo->psPowSyncPrim, 0);
	sPowCmd.eCmdType = RGXFWIF_KCCB_CMD_POW;
	sPowCmd.uCmdData.sPowData.ePowType = RGXFWIF_POW_FORCED_IDLE_REQ;
	sPowCmd.uCmdData.sPowData.uPoweReqData.bCancelForcedIdle = IMG_FALSE;

	PDUMPCOMMENT("RGXForcedIdleRequest: Sending forced idle command");

	/* Send one forced IDLE command to GP */
	eError = RGXSendCommandRaw(psDevInfo,
			RGXFWIF_DM_GP,
			&sPowCmd,
			sizeof(sPowCmd),
			0);

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"RGXForcedIdleRequest: Failed to send idle request"));
		return eError;
	}

	/* Wait for the firmware to answer. */
#if defined(VIRTUAL_PLATFORM) || defined(EMULATOR)
	do {	/* On VP/EMU may take a very long time for commands to flush. Timeouts can be ignored. */
		eError = PVRSRVPollForValueKM(psDevInfo->psPowSyncPrim->pui32LinAddr, 0x1, 0xFFFFFFFF);
		PVR_DPF((PVR_DBG_ERROR,"RGXForcedIdleRequest: On VP/EMU not considered an error. Retrying..."));
	} while (eError != PVRSRV_OK);
#else
	eError = PVRSRVPollForValueKM(psDevInfo->psPowSyncPrim->pui32LinAddr, 0x1, 0xFFFFFFFF);
#endif

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"RGXForcedIdleRequest: Timeout waiting for idle request"));
		PVR_DPF((PVR_DBG_ERROR,"RGXForcedIdleRequest: Firmware potentially left in forced idle state"));
		return eError;
	}

#if defined(PDUMP)
	PDUMPCOMMENT("RGXForcedIdleRequest: Poll for Kernel SyncPrim [0x%p] on DM %d ", psDevInfo->psPowSyncPrim->pui32LinAddr, RGXFWIF_DM_GP);

	SyncPrimPDumpPol(psDevInfo->psPowSyncPrim,
					1,
					0xffffffff,
					PDUMP_POLL_OPERATOR_EQUAL,
					0);
#endif

#if !defined(NO_HARDWARE)
	/* Check the firmware state for idleness */
	if (psFWTraceBuf->ePowState != RGXFWIF_POW_FORCED_IDLE)
	{
		PVR_DPF((PVR_DBG_ERROR,"RGXForcedIdleRequest: Failed to force IDLE"));

		return PVRSRV_ERROR_DEVICE_IDLE_REQUEST_DENIED;
	}
#endif

	return PVRSRV_OK;
}

/*
	RGXCancelForcedIdleRequest
*/
PVRSRV_ERROR RGXCancelForcedIdleRequest(IMG_HANDLE hDevHandle)
{
	PVRSRV_DEVICE_NODE	*psDeviceNode = hDevHandle;
	PVRSRV_RGXDEV_INFO	*psDevInfo = psDeviceNode->pvDevice;
	RGXFWIF_KCCB_CMD	sPowCmd;
	PVRSRV_ERROR		eError = PVRSRV_OK;

	SyncPrimSet(psDevInfo->psPowSyncPrim, 0);

	/* Send the IDLE request to the FW */
	sPowCmd.eCmdType = RGXFWIF_KCCB_CMD_POW;
	sPowCmd.uCmdData.sPowData.ePowType = RGXFWIF_POW_FORCED_IDLE_REQ;
	sPowCmd.uCmdData.sPowData.uPoweReqData.bCancelForcedIdle = IMG_TRUE;

	PDUMPCOMMENT("RGXForcedIdleRequest: Sending cancel forced idle command");

	/* Send cancel forced IDLE command to GP */
	eError = RGXSendCommandRaw(psDevInfo,
			RGXFWIF_DM_GP,
			&sPowCmd,
			sizeof(sPowCmd),
			0);

	if (eError != PVRSRV_OK)
	{
		PDUMPCOMMENT("RGXCancelForcedIdleRequest: Failed to send cancel IDLE request for DM%d", RGXFWIF_DM_GP);
		goto ErrorExit;
	}

	/* Wait for the firmware to answer. */
	eError = PVRSRVPollForValueKM(psDevInfo->psPowSyncPrim->pui32LinAddr, 1, 0xFFFFFFFF);

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"RGXCancelForcedIdleRequest: Timeout waiting for cancel idle request"));
		goto ErrorExit;
	}

#if defined(PDUMP)
	PDUMPCOMMENT("RGXCancelForcedIdleRequest: Poll for Kernel SyncPrim [0x%p] on DM %d ", psDevInfo->psPowSyncPrim->pui32LinAddr, RGXFWIF_DM_GP);

	SyncPrimPDumpPol(psDevInfo->psPowSyncPrim,
					1,
					0xffffffff,
					PDUMP_POLL_OPERATOR_EQUAL,
					0);
#endif

	return eError;

ErrorExit:
	PVR_DPF((PVR_DBG_ERROR,"RGXCancelForcedIdleRequest: Firmware potentially left in forced idle state"));
	return eError;
}

/******************************************************************************
 End of file (rgxpower.c)
******************************************************************************/
