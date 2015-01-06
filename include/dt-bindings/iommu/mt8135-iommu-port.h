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

#ifndef __DTS_IOMMU_PORT_MT8135_H
#define __DTS_IOMMU_PORT_MT8135_H

#define M4U_LARB0_PORT(n)      ((n)+0)
#define M4U_LARB1_PORT(n)      ((n)+9)
#define M4U_LARB2_PORT(n)      ((n)+16)
#define M4U_LARB3_PORT(n)      ((n)+28)
#define M4U_LARB4_PORT(n)      ((n)+42)
#define M4U_LARB5_PORT(n)      ((n)+52)
#define M4U_LARB6_PORT(n)      ((n)+53)


#define	M4U_PORT_VENC_REF_LUMA         M4U_LARB0_PORT(0)
#define	M4U_PORT_VENC_REF_CHROMA       M4U_LARB0_PORT(1)
#define	M4U_PORT_VENC_REC_FRM          M4U_LARB0_PORT(2)
#define	M4U_PORT_VENC_RCPU             M4U_LARB0_PORT(3)
#define	M4U_PORT_VENC_BSDMA            M4U_LARB0_PORT(4)
#define	M4U_PORT_VENC_CUR_LUMA         M4U_LARB0_PORT(5)
#define	M4U_PORT_VENC_CUR_CHROMA       M4U_LARB0_PORT(6)
#define	M4U_PORT_VENC_RD_COMV          M4U_LARB0_PORT(7)
#define	M4U_PORT_VENC_SV_COMV          M4U_LARB0_PORT(8)

#define	M4U_PORT_HW_VDEC_MC_EXT        M4U_LARB1_PORT(0)
#define	M4U_PORT_HW_VDEC_PP_EXT        M4U_LARB1_PORT(1)
#define	M4U_PORT_HW_VDEC_AVC_MV_EXT    M4U_LARB1_PORT(2)
#define	M4U_PORT_HW_VDEC_PRED_RD_EXT   M4U_LARB1_PORT(3)
#define	M4U_PORT_HW_VDEC_PRED_WR_EXT   M4U_LARB1_PORT(4)
#define	M4U_PORT_HW_VDEC_VLD_EXT       M4U_LARB1_PORT(5)
#define	M4U_PORT_HW_VDEC_VLD2_EXT      M4U_LARB1_PORT(6)

#define	M4U_PORT_ROT_EXT               M4U_LARB2_PORT(0)
#define	M4U_PORT_OVL_CH0               M4U_LARB2_PORT(1)
#define	M4U_PORT_OVL_CH1               M4U_LARB2_PORT(2)
#define	M4U_PORT_OVL_CH2               M4U_LARB2_PORT(3)
#define	M4U_PORT_OVL_CH3               M4U_LARB2_PORT(4)
#define	M4U_PORT_WDMA0                 M4U_LARB2_PORT(5)
#define	M4U_PORT_WDMA1                 M4U_LARB2_PORT(6)
#define	M4U_PORT_RDMA0                 M4U_LARB2_PORT(7)
#define	M4U_PORT_RDMA1                 M4U_LARB2_PORT(8)
#define	M4U_PORT_CMDQ                  M4U_LARB2_PORT(9)
#define	M4U_PORT_DBI                   M4U_LARB2_PORT(10)
#define	M4U_PORT_G2D                   M4U_LARB2_PORT(11)


#define	M4U_PORT_JPGDEC_WDMA           M4U_LARB3_PORT(0)
#define	M4U_PORT_JPGENC_RDMA           M4U_LARB3_PORT(1)
#define	M4U_PORT_IMGI                  M4U_LARB3_PORT(2)
#define	M4U_PORT_VIPI                  M4U_LARB3_PORT(3)
#define	M4U_PORT_VIP2I                 M4U_LARB3_PORT(4)
#define	M4U_PORT_DISPO                 M4U_LARB3_PORT(5)
#define	M4U_PORT_DISPCO                M4U_LARB3_PORT(6)
#define	M4U_PORT_DISPVO                M4U_LARB3_PORT(7)
#define	M4U_PORT_VIDO                  M4U_LARB3_PORT(8)
#define	M4U_PORT_VIDCO                 M4U_LARB3_PORT(9)
#define	M4U_PORT_VIDVO                 M4U_LARB3_PORT(10)
#define	M4U_PORT_GDMA_SMI_WR           M4U_LARB3_PORT(11)
#define	M4U_PORT_JPGDEC_BSDMA          M4U_LARB3_PORT(12)
#define	M4U_PORT_JPGENC_BSDMA          M4U_LARB3_PORT(13)

#define	M4U_PORT_GDMA_SMI_RD           M4U_LARB4_PORT(0)
#define	M4U_PORT_IMGCI                 M4U_LARB4_PORT(1)
#define	M4U_PORT_IMGO                  M4U_LARB4_PORT(2)
#define	M4U_PORT_IMG2O                 M4U_LARB4_PORT(3)
#define	M4U_PORT_LSCI                  M4U_LARB4_PORT(4)
#define	M4U_PORT_FLKI                  M4U_LARB4_PORT(5)
#define	M4U_PORT_LCEI                  M4U_LARB4_PORT(6)
#define	M4U_PORT_LCSO                  M4U_LARB4_PORT(7)
#define	M4U_PORT_ESFKO                 M4U_LARB4_PORT(8)
#define	M4U_PORT_AAO                   M4U_LARB4_PORT(9)

#define	M4U_PORT_AUDIO                 M4U_LARB5_PORT(0)
#define	M4U_PORT_GCPU                  M4U_LARB6_PORT(0)

#define	M4U_PORT_UNKNOWN	       M4U_LARB6_PORT(1)

#endif

