/*
* Copyright (c) 2014 MediaTek Inc.
* Author: YongWu <yong.wu@mediatek.com>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/

#include "mtk_iommu_platform.h"

static const struct mtk_port mt8135_port[MTK_IOMMU_PORT_MAX_NR] = {
	/*larb0*/
	{"M4U_PORT_VENC_REF_LUMA",      0,  0},
	{"M4U_PORT_VENC_REF_CHROMA",    0,  1},
	{"M4U_PORT_VENC_REC_FRM",       0,  2},
	{"M4U_PORT_VENC_RCPU",          0,  3},
	{"M4U_PORT_VENC_BSDMA",         0,  4},
	{"M4U_PORT_VENC_CUR_LUMA",      0,  5},
	{"M4U_PORT_VENC_CUR_CHROMA",    0,  6},
	{"M4U_PORT_VENC_RD_COMV",       0,  7},
	{"M4U_PORT_VENC_SV_COMV",       0,  8},

	/*larb1*/
	{"M4U_PORT_HW_VDEC_MC_EXT",      1,  0},
	{"M4U_PORT_HW_VDEC_PP_EXT",      1,  1},
	{"M4U_PORT_HW_VDEC_AVC_MV_EXT",  1,  2},
	{"M4U_PORT_HW_VDEC_PRED_RD_EXT", 1,  3},
	{"M4U_PORT_HW_VDEC_PRED_WR_EXT", 1,  4},
	{"M4U_PORT_HW_VDEC_VLD_EXT",     1,  5},
	{"M4U_PORT_HW_VDEC_VLD2_EXT",    1,  6},

	/*larb2*/
	{"M4U_PORT_ROT_EXT",             2,  0},
	{"M4U_PORT_OVL_CH0",             2,  1},
	{"M4U_PORT_OVL_CH1",             2,  2},
	{"M4U_PORT_OVL_CH2",             2,  3},
	{"M4U_PORT_OVL_CH3",             2,  4},
	{"M4U_PORT_WDMA0",               2,  5},
	{"M4U_PORT_WDMA1",               2,  6},
	{"M4U_PORT_RDMA0",               2,  7},
	{"M4U_PORT_RDMA1",               2,  8},
	{"M4U_PORT_CMDQ",                2,  9},
	{"M4U_PORT_DBI",                 2,  10},
	{"M4U_PORT_G2D",                 2,  11},


	/*larb3*/
	{"M4U_PORT_JPGDEC_WDMA",         3,  0},
	{"M4U_PORT_JPGENC_RDMA",         3,  1},
	{"M4U_PORT_IMGI",                3,  2},
	{"M4U_PORT_VIPI",                3,  3},
	{"M4U_PORT_VIP2I",               3,  4},
	{"M4U_PORT_DISPO",               3,  5},
	{"M4U_PORT_DISPCO",              3,  6},
	{"M4U_PORT_DISPVO",              3,  7},
	{"M4U_PORT_VIDO",                3,  8},
	{"M4U_PORT_VIDCO",               3,  9},
	{"M4U_PORT_DIDVO",               3,  10},
	{"M4U_PORT_GDMA_SMI_WR",         3,  12},
	{"M4U_PORT_JPGDEC_BSDMA",        3,  13},
	{"M4U_PORT_JPGENC_BSDMA",        3,  14},

	/*larb4*/
	{"M4U_PORT_GDMA_SMI_RD",         4,  0},
	{"M4U_PORT_IMGCI",               4,  1},
	{"M4U_PORT_IMGO",                4,  2},
	{"M4U_PORT_IMG2O",               4,  3},
	{"M4U_PORT_LSCI",                4,  4},
	{"M4U_PORT_FLKI",                4,  5},
	{"M4U_PORT_LCEI",                4,  6},
	{"M4U_PORT_LCSO",                4,  7},
	{"M4U_PORT_ESFKO",               4,  8},
	{"M4U_PORT_AAO",                 4,  9},

	/*larb5*/
	{"M4U_PORT_AUDIO",               5,  0},

	/*larb6*/
	{"M4U_PORT_GCPU",                6,  0},
};

static const struct mtk_iommu_cfg mt8135_cfg = {
	.m4u_nr = 2,
	.m4u0_offset = 0x200,
	.m4u1_offset = 0x800,
	.L2_offset = 0x100,
	.larb_nr = 5,
	.larb_nr_real = 7,
	.m4u_tf_larbid = 6,
	.m4u_port_in_larbx = {0, 9, 16, 28, 42, 52, 53},
	.m4u_gpu_port = 53,
	.main_tlb_nr = 48,
	.pfn_tlb_nr = 48,
	.m4u_seq_nr = 16,
	.m4u_port_nr = 54,
	.pport = mt8135_port,
};


const struct mtk_iommu_cfg *mtk_iommu_getcfg(void)
{
	return &mt8135_cfg;
}


