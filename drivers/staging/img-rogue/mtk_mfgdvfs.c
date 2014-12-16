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
 *   mtk_mfgdvfs.c
 *
 * Project:
 * --------
 *   MT8135
 *
 * Description:
 * ------------
 *   Implementation power policy and loading detection for DVFS
 *
 * Author:
 * -------
 *   Enzhu Wang
 *
 *============================================================================
 * History and modification
 *1. Initial @ July 10th 2013, to verify V & F switch,not according to loading currently
 *2. Added GPU DVFS mtk flow and timer proc implemetation
 *============================================================================
 ****************************************************************************/
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>

#include <asm/io.h>


#include "mtk_mfgsys.h"
#include "mtk_mfgdvfs.h"

#include "mach/sync_write.h"
//#include "mach/mt_gpufreq.h"
//#include "mach/mt_clkmgr.h"
//#include "mach/mt_typedefs.h"
//#include "mach/upmu_common.h"
#include "pvrsrv_error.h"
#include "allocmem.h"
#include "osfunc.h"
#include "pvrsrv.h"
#include "power.h"


#define DRV_WriteReg32(ptr, data)     mt65xx_reg_sync_writel(data, ptr)
#define DRV_Reg32(ptr)              (*((volatile unsigned int * const)(ptr)))

static unsigned int g_addr_clk_cfg0;

#define ADDR_CLK_CFG_0           0x00000000 + 0x0140
#define SIZE_CLK_CFG_0           0x30
#define CLK_CFG_0               (g_addr_clk_cfg0 + 0x0)
#define CLK_CFG_1               (g_addr_clk_cfg0 + 0x4)
#define CLK_CFG_2               (g_addr_clk_cfg0 + 0x8)
#define CLK_CFG_3               (g_addr_clk_cfg0 + 0xC)
#define CLK_CFG_4               (g_addr_clk_cfg0 + 0x10)
#define CLK_CFG_5               (g_addr_clk_cfg0 + 0x14)
#define CLK_CFG_6               (g_addr_clk_cfg0 + 0x18)
#define CLK_CFG_7               (g_addr_clk_cfg0 + 0x1C)
#define CLK_CFG_8               (g_addr_clk_cfg0 + 0x24)
#define CLK_CFG_9               (g_addr_clk_cfg0 + 0x28)

static unsigned int g_addr_clk26_cali;

#define ADDR_CLK26_CALI         0x00000000 + 0x1C0
#define SIZE_CLK26_CALI         0x8

#define CLK26CALI       		(g_addr_clk26_cali + 0x0)//(0xF00001C0)
#define CKSTA_REG       		(g_addr_clk26_cali + 0x4)//(0xF00001c4)

static unsigned int g_addr_mbist_cfg0;

#define ADDR_MBIST_CFG_0           0x00000000 + 0x0208
#define SIZE_MBIST_CFG_0           0x10

#define MBIST_CFG_0     (g_addr_mbist_cfg0 + 0x0)//(0xF0000208)
#define MBIST_CFG_1     (g_addr_mbist_cfg0 + 0x4)//(0xF000020C)
#define MBIST_CFG_2     (g_addr_mbist_cfg0 + 0x8)//(0xF0000210)
#define MBIST_CFG_3     (g_addr_mbist_cfg0 + 0xC)//(0xF0000214)



static unsigned int g_addr_hw_res4;
static unsigned int g_addr_hw2_res4;

#define ADDR_HW_RES_4 				0x00000000 + 0x9174
#define ADDR_HW2_RES_4 				0x00000000 + 0x91c4


#define MFG_CONFIG_BASE			 0xF0206000


static unsigned int g_gpu_max_freq = GPU_DVFS_F1;
static unsigned int g_cur_gpu_freq = GPU_DVFS_F3;
static unsigned int g_cur_gpu_volt = GPU_POWER_VGPU_1_15V_VOL;

static unsigned int g_gpufreq_enable_univpll;
static unsigned int g_gpufreq_enable_mmpll;

//static DEFINE_SPINLOCK(clock_lock);
//#define clkset_lock(flags)	    spin_lock_irqsave(&clock_lock, flags)
//#define clkset_unlock(flags)	spin_unlock_irqrestore(&clock_lock, flags)
//#define clkset_locked()		    spin_is_locked(&clock_lock)

static struct clk* g_mmpll;
static struct clk* g_univpll;

extern struct regulator * g_vgpu;

//TODO : ADD DVFS in drivers\devfreq\mediatek for gpu vol and freq settting

//PLL setting
static void _mtk_set_mm_pll_set_freq(unsigned int freq_khz)
{	
	unsigned int err;
	//lock
	err = clk_set_rate(g_mmpll, freq_khz * 1000);	
	//unlock
	if(err)
	{
	    mtk_mfg_debug("[DVFS] _mtk_set_mm_pll_set_freq err %d\n", err);
	}
}

static void _mtk_pll_enable(unsigned int idx, char *msg)
{
	if(idx == MMPLL)
	{
 		clk_prepare_enable(g_mmpll);
 	}
 	else if(idx == UNIVPLL)
 	{
 		clk_prepare_enable(g_univpll);
 	}
 	
    mtk_mfg_debug("[DVFS] _mtk_pll_enable[%s] idx %d\n",msg,idx);
}


static void _mtk_gpufreq_sel(unsigned int src, unsigned int sel)
{
	unsigned int clk_cfg = 0;
	//CHIP_SW_VER ver = 0;

	if (src == GPU_FMFG) {
		clk_cfg = DRV_Reg32(CLK_CFG_0);

		switch (sel) {
		case GPU_CKSQ_MUX_CK:
			clk_cfg = (clk_cfg & 0xFFF0FFFF) | (sel << 16);
			DRV_WriteReg32(CLK_CFG_0, clk_cfg);
			mtk_mfg_debug("[DVFS] _mtk_gpufreq_sel - GPU_CKSQ_MUX_CK\n");
			break;
		case GPU_UNIVPLL1_D4:
			clk_cfg = (clk_cfg & 0xFFF0FFFF) | (sel << 16);
			DRV_WriteReg32(CLK_CFG_0, clk_cfg);
			mtk_mfg_debug("[DVFS] _mtk_gpufreq_sel - GPU_UNIVPLL1_D4\n");
			break;
		case GPU_SYSPLL_D2:
			clk_cfg = (clk_cfg & 0xFFF0FFFF) | (sel << 16);
			DRV_WriteReg32(CLK_CFG_0, clk_cfg);
			mtk_mfg_debug("[DVFS] _mtk_gpufreq_sel - GPU_SYSPLL_D2\n");
			break;
		case GPU_SYSPLL_D2P5:
			clk_cfg = (clk_cfg & 0xFFF0FFFF) | (sel << 16);
			DRV_WriteReg32(CLK_CFG_0, clk_cfg);
			mtk_mfg_debug("[DVFS] _mtk_gpufreq_sel - GPU_SYSPLL_D2P5\n");
			break;
		case GPU_SYSPLL_D3:
			clk_cfg = (clk_cfg & 0xFFF0FFFF) | (sel << 16);
			DRV_WriteReg32(CLK_CFG_0, clk_cfg);
			mtk_mfg_debug("[DVFS] _mtk_gpufreq_sel - GPU_SYSPLL_D3\n");
			break;
		case GPU_UNIVPLL_D5:
			clk_cfg = (clk_cfg & 0xFFF0FFFF) | (sel << 16);
			DRV_WriteReg32(CLK_CFG_0, clk_cfg);
			mtk_mfg_debug("[DVFS] _mtk_gpufreq_sel - GPU_UNIVPLL_D5\n");
			break;
		case GPU_UNIVPLL1_D2:
			clk_cfg = (clk_cfg & 0xFFF0FFFF) | (sel << 16);
			DRV_WriteReg32(CLK_CFG_0, clk_cfg);
			mtk_mfg_debug("[DVFS] _mtk_gpufreq_sel - GPU_UNIVPLL1_D2\n");
			break;
		case GPU_MMPLL_D2:
			clk_cfg = (clk_cfg & 0xFFF0FFFF) | (sel << 16);
			DRV_WriteReg32(CLK_CFG_0, clk_cfg);
			mtk_mfg_debug("[DVFS] _mtk_gpufreq_sel - GPU_MMPLL_D2\n");
			break;
		case GPU_MMPLL_D3:
			clk_cfg = (clk_cfg & 0xFFF0FFFF) | (sel << 16);
			DRV_WriteReg32(CLK_CFG_0, clk_cfg);
			mtk_mfg_debug("[DVFS] _mtk_gpufreq_sel - GPU_MMPLL_D3\n");
			break;
		case GPU_MMPLL_D4:
			clk_cfg = (clk_cfg & 0xFFF0FFFF) | (sel << 16);
			DRV_WriteReg32(CLK_CFG_0, clk_cfg);
			mtk_mfg_debug("[DVFS] _mtk_gpufreq_sel - GPU_MMPLL_D4\n");
			break;
		case GPU_MMPLL_D5:
			clk_cfg = (clk_cfg & 0xFFF0FFFF) | (sel << 16);
			DRV_WriteReg32(CLK_CFG_0, clk_cfg);
			mtk_mfg_debug("[DVFS] _mtk_gpufreq_sel - GPU_MMPLL_D5\n");
			break;
		case GPU_MMPLL_D6:
			clk_cfg = (clk_cfg & 0xFFF0FFFF) | (sel << 16);
			DRV_WriteReg32(CLK_CFG_0, clk_cfg);
			mtk_mfg_debug("[DVFS] _mtk_gpufreq_sel - GPU_MMPLL_D6\n");
			break;
		case GPU_MMPLL_D7:
			clk_cfg = (clk_cfg & 0xFFF0FFFF) | (sel << 16);
			DRV_WriteReg32(CLK_CFG_0, clk_cfg);
			mtk_mfg_debug("[DVFS] _mtk_gpufreq_sel - GPU_MMPLL_D7\n");
			break;
		default:
			mtk_mfg_debug("[DVFS] _mtk_gpufreq_sel - NOT support PLL select\n");
			break;
		}
	}

	else {
		clk_cfg = DRV_Reg32(CLK_CFG_8);

#if 0 //need to be check
		/* added 11th, Aug, 2013 */
		ver = mt_get_chip_sw_ver();
		if (CHIP_SW_VER_02 <= ver) {
			/* MT8135 SW V2 */
			if (GPU_MEM_TOP_MEM == sel) {
				sel = GPU_MEM_TOP_SMI;
			}
		}
#endif		

		switch (sel) {
		case GPU_MEM_CKSQ_MUX_CK:
			clk_cfg = (clk_cfg & 0xfffcffff) | (sel << 16);
			DRV_WriteReg32(CLK_CFG_8, clk_cfg);
			mtk_mfg_debug("[DVFS] _mtk_gpufreq_sel - GPU_MEM_CKSQ_MUX_CK\n");
			break;
		case GPU_MEM_TOP_SMI:
			clk_cfg = (clk_cfg & 0xfffcffff) | (sel << 16);
			DRV_WriteReg32(CLK_CFG_8, clk_cfg);
			mtk_mfg_debug("[DVFS] _mtk_gpufreq_sel - GPU_MEM_TOP_SMI\n");
			break;
		case GPU_MEM_TOP_MFG:
			clk_cfg = (clk_cfg & 0xfffcffff) | (sel << 16);
			DRV_WriteReg32(CLK_CFG_8, clk_cfg);
			mtk_mfg_debug("[DVFS] _mtk_gpufreq_sel - GPU_MEM_TOP_MFG\n");
			break;
		case GPU_MEM_TOP_MEM:
			clk_cfg = (clk_cfg & 0xfffcffff) | (sel << 16);
			DRV_WriteReg32(CLK_CFG_8, clk_cfg);
			mtk_mfg_debug("[DVFS] _mtk_gpufreq_sel - GPU_MEM_TOP_MEM\n");
			break;
		default:
			mtk_mfg_debug("[DVFS] _mtk_gpufreq_sel - not support mem sel\n");
			break;
		}
	}
}

static void _mtk_gpufreq_read_current_clock_from_reg(void)
{
	unsigned int reg_value = 0;
	unsigned int freq = 0;
	DRV_WriteReg32(MBIST_CFG_3, 0x02000000);	/* hf_fmfg_ck */
	DRV_WriteReg32(MBIST_CFG_2, 0x00000000);
	DRV_WriteReg32(CLK26CALI, 0x00000010);
	//mt_gpufreq_udelay(50);
	udelay(30);
	/* reg_value = (DRV_Reg32(CKSTA_REG) & BITMASK(31 : 16)) >> 16; */
	reg_value = (DRV_Reg32(CKSTA_REG) & 0xffff0000) >> 16;
	freq = reg_value * 26000 / 1024;	/* KHz */
	mtk_mfg_debug("[DVFS] _mtk_gpufreq_read_current_clock_from_reg: freq(%d)\n", freq);
} 

static void _mtk_gpu_clock_switch(unsigned int sel)
{
	switch (sel) {
	case GPU_MMPLL_D3_CLOCK:
	case GPU_MMPLL_D4_CLOCK:
	case GPU_MMPLL_D5_CLOCK:
	case GPU_MMPLL_D6_CLOCK:
	case GPU_MMPLL_D7_CLOCK:
		if (g_gpufreq_enable_mmpll == 0) {
			_mtk_pll_enable(MMPLL, "GPU_DVFS");
			g_gpufreq_enable_mmpll = 1;
		}
		_mtk_set_mm_pll_set_freq(GPU_MMPLL_D3_CLOCK * 3);

		if (GPU_MMPLL_D3_CLOCK == sel) {
			_mtk_gpufreq_sel(GPU_FMFG, GPU_MMPLL_D3);
		} else if (GPU_MMPLL_D4_CLOCK == sel) {
			_mtk_gpufreq_sel(GPU_FMFG, GPU_MMPLL_D4);
		} else if (GPU_MMPLL_D5_CLOCK == sel) {
			_mtk_gpufreq_sel(GPU_FMFG, GPU_MMPLL_D5);
		} else if (GPU_MMPLL_D6_CLOCK == sel) {
			_mtk_gpufreq_sel(GPU_FMFG, GPU_MMPLL_D6);
		} else {
			_mtk_gpufreq_sel(GPU_FMFG, GPU_MMPLL_D7);
		}

		/*
		   GPU internal mem clock:
		   E1, GPU core clock > 260M, sel TOP_MEM(333 if MEM clock is 1333)
		   else                 sel TOP_MFG(equal to GPU core clock)
		   E2, GPU core clock > 260M, sel TOP_SMI(322, DRAM will be 1600, can not use TOP_MEM)
		   else                 sel TOP_MFG(equal to GPU core clock)
		 */
		if (sel >= GPU_DVFS_F2) {
			_mtk_gpufreq_sel(GPU_FMEM, GPU_MEM_TOP_MEM);
		} else {
			_mtk_gpufreq_sel(GPU_FMEM, GPU_MEM_TOP_MFG);
		}

		break;

	case GPU_MMPLL_773_D3_CLOCK:
		if (g_gpufreq_enable_mmpll == 0) {
			_mtk_pll_enable(MMPLL, "GPU_DVFS");
			g_gpufreq_enable_mmpll = 1;
		}
		_mtk_set_mm_pll_set_freq(GPU_MMPLL_773_D3_CLOCK * 3);

		_mtk_gpufreq_sel(GPU_FMFG, GPU_MMPLL_D3);
		_mtk_gpufreq_sel(GPU_FMEM, GPU_MEM_TOP_MFG);
		break;

		/* for GPU overclocking */
	case GPU_MMPLL_1350_D3_CLOCK:
		if (g_gpufreq_enable_mmpll == 0) {
			_mtk_pll_enable(MMPLL, "GPU_DVFS");
			g_gpufreq_enable_mmpll = 1;
		}

		_mtk_set_mm_pll_set_freq(GPU_MMPLL_1350_D3_CLOCK * 3);
		_mtk_gpufreq_sel(GPU_FMFG, GPU_MMPLL_D3);
		_mtk_gpufreq_sel(GPU_FMEM, GPU_MEM_TOP_MEM);

		break;

	case GPU_MMPLL_1425_D3_CLOCK:
		if (g_gpufreq_enable_mmpll == 0) {
			_mtk_pll_enable(MMPLL, "GPU_DVFS");
			g_gpufreq_enable_mmpll = 1;
		}

		_mtk_set_mm_pll_set_freq(GPU_MMPLL_1425_D3_CLOCK * 3);
		_mtk_gpufreq_sel(GPU_FMFG, GPU_MMPLL_D3);
		_mtk_gpufreq_sel(GPU_FMEM, GPU_MEM_TOP_MEM);
		break;

	case GPU_MMPLL_1500_D3_CLOCK:
		if (g_gpufreq_enable_mmpll == 0) {
			_mtk_pll_enable(MMPLL, "GPU_DVFS");
			g_gpufreq_enable_mmpll = 1;
		}

		_mtk_set_mm_pll_set_freq(GPU_MMPLL_1500_D3_CLOCK * 3);
		_mtk_gpufreq_sel(GPU_FMFG, GPU_MMPLL_D3);
		_mtk_gpufreq_sel(GPU_FMEM, GPU_MEM_TOP_MEM);
		break;

	case GPU_MMPLL_1650_D3_CLOCK:
		if (g_gpufreq_enable_mmpll == 0) {
			_mtk_pll_enable(MMPLL, "GPU_DVFS");
			g_gpufreq_enable_mmpll = 1;
		}

		_mtk_set_mm_pll_set_freq(GPU_MMPLL_1650_D3_CLOCK * 3);
		_mtk_gpufreq_sel(GPU_FMFG, GPU_MMPLL_D3);
		_mtk_gpufreq_sel(GPU_FMEM, GPU_MEM_TOP_MEM);
		break;


	case GPU_UNIVPLL_D5_CLOCK:
		if (g_gpufreq_enable_univpll == 0) {
			_mtk_pll_enable(UNIVPLL, "GPU_DVFS");
			g_gpufreq_enable_univpll = 1;
		}
		_mtk_gpufreq_sel(GPU_FMFG, GPU_UNIVPLL_D5);
		_mtk_gpufreq_sel(GPU_FMEM, GPU_MEM_TOP_MFG);
		break;

	default:
		if (g_gpufreq_enable_mmpll == 0) {
			_mtk_pll_enable(MMPLL, "GPU_DVFS");
			g_gpufreq_enable_mmpll = 1;
		}
		_mtk_set_mm_pll_set_freq(sel * 3);

		if (sel >= GPU_DVFS_F2) {
			_mtk_gpufreq_sel(GPU_FMEM, GPU_MEM_TOP_MEM);
		} else {
			_mtk_gpufreq_sel(GPU_FMEM, GPU_MEM_TOP_MFG);
		}
		break;
		/* not return for support any freq set by sysfs */

	}

	g_cur_gpu_freq = sel;

	_mtk_gpufreq_read_current_clock_from_reg();

}




/*****************************************************************
* Check GPU current frequency and enable pll in initial and late resume.
*****************************************************************/
static void _mtk_gpufreq_check_freq_and_set_pll(unsigned int freq_new)
{
	mtk_mfg_debug("[DVFS] mt_gpufreq_check_freq_and_set_pll, freq = %d\n", freq_new);

	switch (freq_new) {
	case GPU_MMPLL_D3_CLOCK:
	case GPU_MMPLL_D4_CLOCK:
	case GPU_MMPLL_D5_CLOCK:
	case GPU_MMPLL_D6_CLOCK:
	case GPU_MMPLL_D7_CLOCK:
	case GPU_MMPLL_773_D3_CLOCK:
	case GPU_MMPLL_1350_D3_CLOCK:
	case GPU_MMPLL_1425_D3_CLOCK:
	case GPU_MMPLL_1500_D3_CLOCK:
	case GPU_MMPLL_1650_D3_CLOCK:
		if (g_gpufreq_enable_mmpll == 0) {
			_mtk_pll_enable(MMPLL, "GPU_DVFS");
			g_gpufreq_enable_mmpll = 1;
		}
		break;

	case GPU_UNIVPLL_D5_CLOCK:
		if (g_gpufreq_enable_univpll == 0) {
			_mtk_pll_enable(UNIVPLL, "GPU_DVFS");
			g_gpufreq_enable_univpll = 1;
		}
		break;
	default:
		if (g_gpufreq_enable_mmpll == 0) {
			_mtk_pll_enable(MMPLL, "GPU_DVFS");
			g_gpufreq_enable_mmpll = 1;
		}
		break;

	}
}


static void _mtk_gpu_volt_switch_initial(unsigned int volt_new)
{
	int ret;
	// always turn on power 
	ret = regulator_set_voltage(g_vgpu, volt_new*1000, MAX_SAFEVOLT);
	mtk_mfg_debug("[DVFS] _mtk_gpu_volt_switch_initial set volt : (%d)v, ret = %d\n",volt_new,ret);
	
	mtk_mfg_debug("[DVFS] regulator_get_voltage = %d\n",regulator_get_voltage(g_vgpu));
	
	g_cur_gpu_volt = volt_new;
}

/* get bonding option for GPU max clock setting */
static unsigned int _mtk_gpufreq_get_max_speed(void)
{
	unsigned int m_hw_res4 = 0;
	unsigned int m_hw2_res4 = 0;

	m_hw_res4  = DRV_Reg32(g_addr_hw_res4);
	m_hw2_res4 = DRV_Reg32(g_addr_hw2_res4);

	mtk_mfg_debug("[DVFS] mt_gpufreq_get_max_speed m_hw_res4 is %d\n", m_hw_res4);

	/* Turbo with */
	if (0x1000 == m_hw_res4) {
		mtk_mfg_debug("[DVFS] mt_gpufreq_get_max_speed:450M\n");
		return GPU_DVFS_OVER_F450;
	}
	/* MT8131 */
	else if (0x1003 == m_hw_res4) {
		mtk_mfg_debug("[DVFS] mt_gpufreq_get_max_speed:MT8131\n");
		return GPU_DVFS_F1;
	}
	/* non-turbo PTPOD_SW_BOND  */
	else if (0x1001 == m_hw_res4) {
		m_hw2_res4 = ((m_hw2_res4 & 0x00000700) >> 8);
		mtk_mfg_debug("[DVFS] mt_gpufreq_get_max_speed:m_hw2_res4 is %d\n", m_hw2_res4);
		switch (m_hw2_res4) {
		case 0x001:
			return GPU_BONDING_001;
		case 0x010:
			return GPU_BONDING_010;
		case 0x011:
			return GPU_BONDING_011;
		case 0x100:
			return GPU_BONDING_100;
		case 0x101:
			return GPU_BONDING_101;
		case 0x110:
			return GPU_BONDING_110;
		case 0x111:
			return GPU_BONDING_111;
		case 0x000:
		default:
			return GPU_BONDING_000;

		}
	}
	else
	{
		mtk_mfg_debug("[DVFS] mt_gpufreq_get_max_speed:default\n");
		return GPU_DVFS_F1;
	}

}

//
static void _mtk_gpufreq_set_initial(void)
{

	unsigned int clk_sel = 0;
	unsigned int clk_cfg_0 = 0;
	
		
	clk_cfg_0 = DRV_Reg32(CLK_CFG_0);
	clk_cfg_0 = (clk_cfg_0 & 0x000F0000) >> 16;

	mtk_mfg_debug("[DVFS] mt_gpufreq_init, clk_cfg_0 = %d\n", clk_cfg_0);

	mtk_mfg_debug("[DVFS] g_cur_gpu_freq = %d, before _mtk_gpufreq_set_initial\n", g_cur_gpu_freq);

	_mtk_gpufreq_check_freq_and_set_pll(g_cur_gpu_freq);
	_mtk_gpu_volt_switch_initial(g_cur_gpu_volt);

	clk_sel = _mtk_gpufreq_get_max_speed();
	_mtk_gpu_clock_switch(clk_sel);
	g_gpu_max_freq = clk_sel;

	mtk_mfg_debug("[DVFS] _mtk_gpufreq_set_initial, g_cur_gpu_volt = %d, g_cur_gpu_freq = %d\n",
		    		g_cur_gpu_volt, g_cur_gpu_freq);	
		    		
	//regulator_set_voltage(data->vdd_int, volt, MAX_SAFEVOLT);
}



IMG_INT32 MTKInitFreqInfo(struct platform_device *dev)
{
	if(dev == NULL)
		return PVRSRV_ERROR_INVALID_PARAMS;

	//init clk	
    g_mmpll   = devm_clk_get(&dev->dev,"mmpll");
    g_univpll = devm_clk_get(&dev->dev,"univpll");
    mtk_mfg_debug("[DVFS] g_mmpll=%p, g_univpll=%p \n",g_mmpll,g_univpll);
    if(IS_ERR(g_mmpll) || IS_ERR(g_univpll))
    	return PVRSRV_ERROR_INVALID_PARAMS;
   
    //init regulator
    //g_vgpu    = devm_regulator_get(&dev->dev,"mfgsys-power");
   	//if (IS_ERR(g_vgpu)) {
	//	mtk_mfg_debug("[DVFS] Cannot get the regulator g_vgpu \n");
	//	return PVRSRV_ERROR_INVALID_PARAMS;
	//} 
    //init register map
    g_addr_clk_cfg0   = (unsigned int)ioremap(ADDR_CLK_CFG_0, SIZE_CLK_CFG_0);
    g_addr_clk26_cali = (unsigned int)ioremap(ADDR_CLK26_CALI, SIZE_CLK26_CALI);
    g_addr_mbist_cfg0 = (unsigned int)ioremap(ADDR_MBIST_CFG_0, SIZE_MBIST_CFG_0);

	g_addr_hw_res4  = (unsigned int)ioremap(ADDR_HW_RES_4,  4);
    g_addr_hw2_res4 = (unsigned int)ioremap(ADDR_HW2_RES_4, 4);
    
#if ENABLE_MTK_MFG_DEBUG 
	mtk_mfg_debug("=====================================\n");
	_mtk_gpufreq_read_current_clock_from_reg();
	mtk_mfg_debug("[DVFS] regulator_get_voltage = %d\n",regulator_get_voltage(g_vgpu));
	mtk_mfg_debug("=====================================\n");
#endif    	
    
    
    _mtk_gpufreq_set_initial();
    //MtkInitSetFreqTbl(TblType);
    
#if ENABLE_MTK_MFG_DEBUG 
	mtk_mfg_debug("=====================================\n");
	_mtk_gpufreq_read_current_clock_from_reg();
	mtk_mfg_debug("[DVFS] regulator_get_voltage = %d\n",regulator_get_voltage(g_vgpu));
	mtk_mfg_debug("=====================================\n");
#endif    
    return PVRSRV_OK;
}

void MTKDeInitFreqInfo(struct platform_device *dev)
{
	if(dev == NULL)
		return;	
		
	iounmap((void *)g_addr_clk_cfg0);
	iounmap((void *)g_addr_clk26_cali);
	iounmap((void *)g_addr_mbist_cfg0);
	iounmap((void *)g_addr_hw_res4);
	iounmap((void *)g_addr_hw2_res4);

}
