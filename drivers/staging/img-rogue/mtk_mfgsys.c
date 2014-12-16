/*****************************************************************************
*  Copyright Statement:
*  --------------------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of MediaTek Inc. (C) 2005
*
*  BY OPENING THIS FILE, BUYER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
*  THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
*  RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO BUYER ON
*  AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
*  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
*  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
*  NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
*  SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
*  SUPPLIED WITH THE MEDIATEK SOFTWARE, AND BUYER AGREES TO LOOK ONLY TO SUCH
*  THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. MEDIATEK SHALL ALSO
*  NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE RELEASES MADE TO BUYER'S
*  SPECIFICATION OR TO CONFORM TO A PARTICULAR STANDARD OR OPEN FORUM.
*
*  BUYER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND CUMULATIVE
*  LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
*  AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
*  OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY BUYER TO
*  MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE. 
*
*  THE TRANSACTION CONTEMPLATED HEREUNDER SHALL BE CONSTRUED IN ACCORDANCE
*  WITH THE LAWS OF THE STATE OF CALIFORNIA, USA, EXCLUDING ITS CONFLICT OF
*  LAWS PRINCIPLES.  ANY DISPUTES, CONTROVERSIES OR CLAIMS ARISING THEREOF AND
*  RELATED THERETO SHALL BE SETTLED BY ARBITRATION IN SAN FRANCISCO, CA, UNDER
*  THE RULES OF THE INTERNATIONAL CHAMBER OF COMMERCE (ICC).
*
*****************************************************************************/

/*****************************************************************************
 *
 * Filename:
 * ---------
 *   mtk_mfgsys.c
 *
 * Project:
 * --------
 *   MT8135
 *
 * Description:
 * ------------
 *   Implementation interface between RGX DDK and kernel GPU DVFS module 
 *
 * Author:
 * -------
 *   Enzhu Wang
 *
 *============================================================================
 * History and modification
 *1. Initial @ April 27th 2013
 *2. Add  Enable/Disable MFG sytem API, Enable/Disable MFG clock API @May 20th 2013
 *3. Add MTKDevPrePowerState/MTKDevPostPowerState/MTKSystemPrePowerState/MTKSystemPostPowerState @ May 29th 2013
 *4. Move some interface to mtk_mfgdvfs.c @ July 10th 2013
 *5. E2 above IC version support HW APM feature @ Sep 17th 2013 
 *============================================================================
 ****************************************************************************/

#include <linux/delay.h>
#include <linux/mutex.h>

//#include "mach/mt_gpufreq.h"
//#include "mach/mt_typedefs.h"
//#include "mach/mt_boot.h"
//#include "mach/upmu_common.h"

//#include <dt-bindings/clock/mt8173-clk.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/regulator/consumer.h>

#include "pvrsrv_error.h"

#include "mtk_mfgsys.h"
#include "mtk_mfgdvfs.h"

//#include "val_types_private.h"
//#include "hal_types_private.h"
//#include "val_api_private.h"
//#include "val_log.h"
//#include "common/drv/inc/drv_api.h"
#include "clk/clk-mtk.h"
#include "mach/sync_write.h"

//for mfgdvfs.c
struct regulator * g_vgpu;

static struct clk* g_mfgclk_axi;
static struct clk* g_mfgclk_mem;
static struct clk* g_mfgclk_g3d;
static struct clk* g_mfgclk_26m;
/* eTblType declare for DVFS support or not and which level be defined */
//static FREQ_TABLE_DVFS_TYPE eTblType = TBL_TYPE_DVFS_2_LEVEL;
static FREQ_TABLE_DVFS_TYPE eTblType = TBL_TYPE_DVFS_NOT_SUPPORT;

static void __iomem * gbRegBase;
static struct platform_device *gpsPVRLDMDev = NULL;

static bool bGetClock = false;
static bool bMfgInit = false;


#define DRV_WriteReg32(ptr, data)     mt65xx_reg_sync_writel(data, ptr)
#define DRV_Reg32(ptr)              (*((volatile unsigned int * const)(ptr)))

static int MtkEnableMfgClock(void)
{
	mtk_mfg_debug("MtkEnableMfgClock[%d] Begin... clks %p %p %p %p\n",bGetClock, g_mfgclk_axi,g_mfgclk_mem,g_mfgclk_g3d,g_mfgclk_26m);
	
	if(bGetClock == false)
		return PVRSRV_OK;

	clk_prepare(g_mfgclk_axi);
	clk_enable(g_mfgclk_axi);

	clk_prepare(g_mfgclk_mem);
	clk_enable(g_mfgclk_mem);

	clk_prepare(g_mfgclk_g3d);
	clk_enable(g_mfgclk_g3d);

	clk_prepare(g_mfgclk_26m);
	clk_enable(g_mfgclk_26m);
		
	mtk_mfg_debug("MtkEnableMfgClock  end...\n");
	return PVRSRV_OK;
}

static int MtkDisableMfgClock(void)
{
	mtk_mfg_debug("MtkDisableMfgClock[%d] Begin\n",bGetClock);
	
	if(bGetClock == false) 
		return PVRSRV_OK;
	
	mtk_mfg_debug("disable g_mfgclk_26m\n");	
	clk_disable(g_mfgclk_26m);
	clk_unprepare(g_mfgclk_26m);

	clk_disable(g_mfgclk_g3d);
	clk_unprepare(g_mfgclk_g3d);

	clk_disable(g_mfgclk_mem);
	clk_unprepare(g_mfgclk_mem);

	clk_disable(g_mfgclk_axi);
	clk_unprepare(g_mfgclk_axi);

	mtk_mfg_debug("MtkDisableMfgClock  end...\n");
	return PVRSRV_OK;
}

static int MtkEnableMfgSystem(void)
{
#if 1	//enable cmos !!!
	mtk_mfg_debug("enable_subsys  begin...\n");
	enable_subsys(SYS_MFG_2D);		
	enable_subsys(SYS_MFG);	
	mtk_mfg_debug("enable_subsys  end...\n");
#endif
	return PVRSRV_OK;
}


static int MtkDisableMfgSystem(void)
{
#if 1    
	//enable_clock(MT_CG_MFG_BMEM_PDN, "MFG");
	//enable MFG MEM, or disable subsys failed
	/* disable subsystem first before disable clock */
	mtk_mfg_debug("disable_subsys begin...\n");	 	

	disable_subsys(SYS_MFG);	
	disable_subsys(SYS_MFG_2D);
	mtk_mfg_debug("disable_subsys end...\n");	 	
#endif	
	return PVRSRV_OK;
}


static int MTKEnableHWAPM(void)
{
	mtk_mfg_debug("MTKEnableHWAPM...\n");
	//DRV_WriteReg32((unsigned int *)((char *)gbRegBase + 0x24), 0x8007050f);
	//DRV_WriteReg32((unsigned int *)((char *)gbRegBase + 0x28), 0x0e0c0a09);
	return PVRSRV_OK;
}


static int MTKDisableHWAPM(void)
{
	mtk_mfg_debug("MTKDisableHWAPM...\n");
	//DRV_WriteReg32(((char *)gbRegBase + 0x24), 0x0);
	//DRV_WriteReg32(((char *)gbRegBase + 0x28), 0x0);
	return PVRSRV_OK;
}

int MTKMFGSystemInit(void)
{

	mtk_mfg_debug("[MFG]MTKMFGSystemInit Begin\n");

	bMfgInit = true;
	
	init_device_node();
	MtkEnableMfgSystem();

	//MTKMFGClockPowerOn();
#ifndef MTK_MFG_DVFS 
	MTKInitFreqInfo(gpsPVRLDMDev);
#else
	MTKSetFreqInfo(eTblType);
#endif

	//MTKEnableHWAPM();

	/*DVFS  feature entry */
	if(TBL_TYPE_DVFS_NOT_SUPPORT !=eTblType)
	{
		//MtkDVFSTimerSetup();
		MtkDVFSInit();
	}

	mtk_mfg_debug("[MFG]MTKMFGSystemInit End \n");
	
	return PVRSRV_OK;
}

int MTKMFGSystemDeInit(void)
{
    if(TBL_TYPE_DVFS_NOT_SUPPORT !=eTblType)
	{
		MtkDVFSDeInit();
	}
	
#ifndef MTK_MFG_DVFS 
	MTKDeInitFreqInfo(gpsPVRLDMDev);
#endif
	MtkDisableMfgSystem();	
	return PVRSRV_OK;
}

static DEFINE_MUTEX(g_DevPreMutex);
static DEFINE_MUTEX(g_DevPostMutex);


int MTKMFGGetClocks(struct platform_device* pdev)
{
	int volt, enable;
	gbRegBase = of_iomap(pdev->dev.of_node, 0); 
	if (!gbRegBase) 
	{		
		mtk_mfg_debug("Unable to ioremap registers pdev %p\n",pdev); 
	}		
	g_mfgclk_axi = devm_clk_get(&pdev->dev,"mfg_axi"); //MT_CG_MFG_AXI baxi_ck
	g_mfgclk_mem = devm_clk_get(&pdev->dev,"mfg_mem"); //MT_CG_MFG_MEM bmem_ck
    g_mfgclk_g3d = devm_clk_get(&pdev->dev,"mfg_g3d"); //MT_CG_MFG_G3D bg3d_ck
	g_mfgclk_26m = devm_clk_get(&pdev->dev,"mfg_26m"); //MT_CG_MFG_26M b26m_ck
	

    g_vgpu       = devm_regulator_get(&pdev->dev,"mfgsys-power");
	volt = regulator_get_voltage(g_vgpu);
	enable = regulator_is_enabled(g_vgpu);

	mtk_mfg_debug("g_vgpu = %p, volt = %d, enable = %d\n",g_vgpu, volt, enable); 
    if(enable==0)
    {
    	enable = regulator_enable(g_vgpu);
    	if(enable!=0)
    	    mtk_mfg_debug("failed to enable regulator  %d\n", regulator_is_enabled(g_vgpu)); 
    }

	
	bGetClock = true;
	gpsPVRLDMDev = pdev;
	
	if( IS_ERR(g_mfgclk_axi) || 
		IS_ERR(g_mfgclk_mem) ||
		IS_ERR(g_mfgclk_g3d) ||
		IS_ERR(g_mfgclk_26m))
		bGetClock = false;
	
	mtk_mfg_debug("MTKMFGGetClocks[%d] clocks %p %p %p %p\n",bGetClock, g_mfgclk_axi,g_mfgclk_mem,g_mfgclk_g3d,g_mfgclk_26m);

	if(bGetClock && !bMfgInit)
	{
		MTKMFGSystemInit();	
	}

		
	return (int)bGetClock;
}

static PVRSRV_DEV_POWER_STATE g_eCurrPowerState = PVRSRV_DEV_POWER_STATE_ON;//PVRSRV_DEV_POWER_STATE_DEFAULT;
static PVRSRV_DEV_POWER_STATE g_eNewPowerState = PVRSRV_DEV_POWER_STATE_DEFAULT;

void MTKSysSetInitialPowerState(void)
{
	mtk_mfg_debug("MTKSysSetInitialPowerState ---\n");
    //MtkDisableMfgClock();
}

void MTKSysRestoreInitialPowerState(void)
{
	mtk_mfg_debug("MTKSysRestoreInitialPowerState ---\n");
    //MtkEnableMfgClock();
}

PVRSRV_ERROR MTKSysDevPrePowerState(PVRSRV_DEV_POWER_STATE eNewPowerState,
                                         PVRSRV_DEV_POWER_STATE eCurrentPowerState,
									     IMG_BOOL bForced)

{

	mtk_mfg_debug("MTKSysDevPrePowerState (%d->%d), bForced = %d\n",eCurrentPowerState,eNewPowerState, bForced);
	mutex_lock(&g_DevPreMutex);

	if(PVRSRV_DEV_POWER_STATE_OFF == eNewPowerState
	 &&PVRSRV_DEV_POWER_STATE_ON == eCurrentPowerState)
	{
		MTKDisableHWAPM();
		MtkDisableMfgClock();	
		//MtkDisableMfgSystem();		
		
	}
	else if(PVRSRV_DEV_POWER_STATE_ON == eNewPowerState
	      &&PVRSRV_DEV_POWER_STATE_OFF == eCurrentPowerState)
	{	
	
		//MtkEnableMfgSystem();	
		MtkEnableMfgClock();	
		//mtk_mfg_debug("[MFG_ON]0xf0206000: 0x%X\n",DRV_Reg32(0xf0206000));
		MTKEnableHWAPM();
	}
	else
	{
		mtk_mfg_debug("MTKSysDevPrePowerState do nothing!\n");

	}
	
	g_eCurrPowerState = eCurrentPowerState; 
	g_eNewPowerState = eNewPowerState;
	
	mutex_unlock(&g_DevPreMutex);
	
	return PVRSRV_OK;	

}

PVRSRV_ERROR MTKSysDevPostPowerState(PVRSRV_DEV_POWER_STATE eNewPowerState,
                                          PVRSRV_DEV_POWER_STATE eCurrentPowerState,
									      IMG_BOOL bForced)
{
    /* Post power sequence move to PrePowerState */
    return PVRSRV_OK;

    mtk_mfg_debug("MTKSysDevPostPowerState (%d->%d)\n",eCurrentPowerState,eNewPowerState);

	mutex_lock(&g_DevPostMutex);

	if(PVRSRV_DEV_POWER_STATE_ON == eNewPowerState
	 &&PVRSRV_DEV_POWER_STATE_OFF == eCurrentPowerState)
	{    
		MtkEnableMfgClock();
        
	}
	else if(PVRSRV_DEV_POWER_STATE_OFF == eNewPowerState
	      &&PVRSRV_DEV_POWER_STATE_ON == eCurrentPowerState)
	{
	    MtkDisableMfgClock();
		//clock_dump_info();
	}
	else
	{
		mtk_mfg_debug("MTKSysDevPostPowerState do nothing!\n");
	}
	
	mutex_unlock(&g_DevPostMutex);

	return PVRSRV_OK;

}

PVRSRV_ERROR MTKSystemPrePowerState(PVRSRV_SYS_POWER_STATE eNewPowerState)
{

	mtk_mfg_debug("MTKSystemPrePowerState eNewPowerState %d\n",eNewPowerState);

	if(PVRSRV_SYS_POWER_STATE_OFF == eNewPowerState)
	{
        #if 0//!MTK_PM_SUPPORT
        MtkDisableMfgSystem();
		#endif
		//mt_gpu_disable_vgpu();
	}
	else
	{
		mtk_mfg_debug("MTKSystemPrePowerState do nothing!\n");	
	}

	return PVRSRV_OK;

}
PVRSRV_ERROR MTKSystemPostPowerState(PVRSRV_SYS_POWER_STATE eNewPowerState)
{

    mtk_mfg_debug("MTKSystemPostPowerState eNewPowerState %d\n",eNewPowerState);

	if(PVRSRV_SYS_POWER_STATE_ON == eNewPowerState)
	{
        #if 0//!MTK_PM_SUPPORT
		MtkEnableMfgSystem();
        #endif
		//mt_gpu_vgpu_sel_1_15();
	}
	else
	{
		mtk_mfg_debug("MTKSystemPostPowerState do nothing!\n");

	}
		
	return PVRSRV_OK;
}
