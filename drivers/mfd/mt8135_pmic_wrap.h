/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: Flora.Fu <flora.fu@mediatek.com>
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

#ifndef __PMIC_WRAP_REGS_H__
#define __PMIC_WRAP_REGS_H__

/* -------macro for timeout setting-------------------------------- */
#define TIMEOUT_READ	    0xFF	/* us */
#define TIMEOUT_WAIT_IDLE	0xFF	/* us */

/* -----macro for wrapper  regsister---------------------------------------- */
#define PMIC_WRAP_MUX_SEL               0x0
#define PMIC_WRAP_WRAP_EN               0x4
#define PMIC_WRAP_DIO_EN                0x8
#define PMIC_WRAP_SIDLY                 0xC
#define PMIC_WRAP_CSHEXT                0x10
#define PMIC_WRAP_CSHEXT_WRITE          0x14
#define PMIC_WRAP_CSHEXT_READ           0x18
#define PMIC_WRAP_CSLEXT_START          0x1C
#define PMIC_WRAP_CSLEXT_END            0x20
#define PMIC_WRAP_STAUPD_PRD            0x24
#define PMIC_WRAP_STAUPD_GRPEN          0x28
#define PMIC_WRAP_STAUPD_MAN_TRIG       0x2C
#define PMIC_WRAP_STAUPD_STA            0x30
#define PMIC_WRAP_EVENT_IN_EN           0x34
#define PMIC_WRAP_EVENT_DST_EN          0x38
#define PMIC_WRAP_WRAP_STA              0x3C
#define PMIC_WRAP_RRARB_INIT            0x40
#define PMIC_WRAP_RRARB_EN              0x44
#define PMIC_WRAP_RRARB_STA0            0x48
#define PMIC_WRAP_RRARB_STA1            0x4C
#define PMIC_WRAP_HARB_INIT             0x50
#define PMIC_WRAP_HARB_HPRIO            0x54
#define PMIC_WRAP_HIPRIO_ARB_EN         0x58
#define PMIC_WRAP_HARB_STA0             0x5C
#define PMIC_WRAP_HARB_STA1             0x60
#define PMIC_WRAP_MAN_EN                0x64
#define PMIC_WRAP_MAN_CMD               0x68
#define PMIC_WRAP_MAN_RDATA             0x6C
#define PMIC_WRAP_MAN_VLDCLR            0x70
#define PMIC_WRAP_WACS0_EN              0x74
#define PMIC_WRAP_INIT_DONE0            0x78
#define PMIC_WRAP_WACS0_CMD             0x7C
#define PMIC_WRAP_WACS0_RDATA           0x80
#define PMIC_WRAP_WACS0_VLDCLR          0x84
#define PMIC_WRAP_WACS1_EN              0x88
#define PMIC_WRAP_INIT_DONE1            0x8C
#define PMIC_WRAP_WACS1_CMD             0x90
#define PMIC_WRAP_WACS1_RDATA           0x94
#define PMIC_WRAP_WACS1_VLDCLR          0x98
#define PMIC_WRAP_WACS2_EN              0x9C
#define PMIC_WRAP_INIT_DONE2            0xA0
#define PMIC_WRAP_WACS2_CMD             0xA4
#define PMIC_WRAP_WACS2_RDATA           0xA8
#define PMIC_WRAP_WACS2_VLDCLR          0xAC
#define PMIC_WRAP_INT_EN                0xB0
#define PMIC_WRAP_INT_FLG_RAW           0xB4
#define PMIC_WRAP_INT_FLG               0xB8
#define PMIC_WRAP_INT_CLR               0xBC
#define PMIC_WRAP_SIG_ADR               0xC0
#define PMIC_WRAP_SIG_MODE              0xC4
#define PMIC_WRAP_SIG_VALUE             0xC8
#define PMIC_WRAP_SIG_ERRVAL            0xCC
#define PMIC_WRAP_CRC_EN                0xD0
#define PMIC_WRAP_EVENT_STA             0xD4
#define PMIC_WRAP_EVENT_STACLR          0xD8
#define PMIC_WRAP_TIMER_EN              0xDC
#define PMIC_WRAP_TIMER_STA             0xE0
#define PMIC_WRAP_WDT_UNIT              0xE4
#define PMIC_WRAP_WDT_SRC_EN            0xE8
#define PMIC_WRAP_WDT_FLG               0xEC
#define PMIC_WRAP_DEBUG_INT_SEL         0xF0
#define PMIC_WRAP_DVFS_ADR0             0xF4
#define PMIC_WRAP_DVFS_WDATA0           0xF8
#define PMIC_WRAP_DVFS_ADR1             0xFC
#define PMIC_WRAP_DVFS_WDATA1           0x100
#define PMIC_WRAP_DVFS_ADR2             0x104
#define PMIC_WRAP_DVFS_WDATA2           0x108
#define PMIC_WRAP_DVFS_ADR3             0x10C
#define PMIC_WRAP_DVFS_WDATA3           0x110
#define PMIC_WRAP_DVFS_ADR4             0x114
#define PMIC_WRAP_DVFS_WDATA4           0x118
#define PMIC_WRAP_DVFS_ADR5             0x11C
#define PMIC_WRAP_DVFS_WDATA5           0x120
#define PMIC_WRAP_DVFS_ADR6             0x124
#define PMIC_WRAP_DVFS_WDATA6           0x128
#define PMIC_WRAP_DVFS_ADR7             0x12C
#define PMIC_WRAP_DVFS_WDATA7           0x130
#define PMIC_WRAP_CIPHER_KEY_SEL        0x134
#define PMIC_WRAP_CIPHER_IV_SEL         0x138
#define PMIC_WRAP_CIPHER_LOAD           0x13C
#define PMIC_WRAP_CIPHER_START          0x140
#define PMIC_WRAP_CIPHER_RDY            0x144
#define PMIC_WRAP_CIPHER_MODE           0x148
#define PMIC_WRAP_CIPHER_SWRST          0x14C
#define PMIC_WRAP_CIPHER_IV0            0x150
#define PMIC_WRAP_CIPHER_IV1            0x154
#define PMIC_WRAP_CIPHER_IV2            0x158
#define PMIC_WRAP_DCM_EN                0x15C
#define PMIC_WRAP_DCM_DBC_PRD           0x160


/* -----macro for wrapper  status---------------------------------------- */
#define GET_WACS0_WDATA(x)           (((x)>>0)  & 0x0000ffff)
#define GET_WACS0_ADR(x)             (((x)>>16) & 0x00007fff)
#define GET_WACS0_WRITE(x)           (((x)>>31) & 0x00000001)
#define GET_WACS0_RDATA(x)           (((x)>>0)  & 0x0000ffff)
#define GET_WACS0_FSM(x)             (((x)>>16) & 0x00000007)
#define GET_WACS0_REQ(x)             (((x)>>19) & 0x00000001)
#define GET_SYNC_IDLE0(x)            (((x)>>20) & 0x00000001)
#define GET_INIT_DONE0(x)            (((x)>>21) & 0x00000001)

/* -----macro for wrapper bridge regsister---------------------------------- */
#define PERI_PWRAP_BRIDGE_IARB_INIT         0x0
#define PERI_PWRAP_BRIDGE_IORD_ARB_EN       0x4
#define PERI_PWRAP_BRIDGE_IARB_STA0         0x8
#define PERI_PWRAP_BRIDGE_IARB_STA1         0xC
#define PERI_PWRAP_BRIDGE_WACS3_EN          0x10
#define PERI_PWRAP_BRIDGE_INIT_DONE3        0x14
#define PERI_PWRAP_BRIDGE_WACS3_CMD         0x18
#define PERI_PWRAP_BRIDGE_WACS3_RDATA       0x1C
#define PERI_PWRAP_BRIDGE_WACS3_VLDCLR      0x20
#define PERI_PWRAP_BRIDGE_WACS4_EN          0x24
#define PERI_PWRAP_BRIDGE_INIT_DONE4        0x28
#define PERI_PWRAP_BRIDGE_WACS4_CMD         0x2C
#define PERI_PWRAP_BRIDGE_WACS4_RDATA       0x30
#define PERI_PWRAP_BRIDGE_WACS4_VLDCLR      0x34
#define PERI_PWRAP_BRIDGE_INT_EN            0x38
#define PERI_PWRAP_BRIDGE_INT_FLG_RAW       0x3C
#define PERI_PWRAP_BRIDGE_INT_FLG           0x40
#define PERI_PWRAP_BRIDGE_INT_CLR           0x44
#define PERI_PWRAP_BRIDGE_TIMER_EN          0x48
#define PERI_PWRAP_BRIDGE_TIMER_STA         0x4C
#define PERI_PWRAP_BRIDGE_WDT_UNIT          0x50
#define PERI_PWRAP_BRIDGE_WDT_SRC_EN        0x54
#define PERI_PWRAP_BRIDGE_WDT_FLG           0x58
#define PERI_PWRAP_BRIDGE_DEBUG_INT_SEL     0x5C

/* HIPRIS_ARB */
#define MDINF		(1 << 0)
#define WACS0		(1 << 1)
#define WACS1		(1 << 2)
#define WACS2		(1 << 3)
#define DVFSINF		(1 << 4)
#define STAUPD		(1 << 5)
#define GPSINF		(1 << 6)

/* macro for WACS_FSM */
#define WACS_FSM_IDLE               0x00
#define WACS_FSM_REQ                0x02
#define WACS_FSM_WFDLE              0x04
#define WACS_FSM_WFVLDCLR           0x06
#define WACS_INIT_DONE              0x01
#define WACS_SYNC_IDLE              0x01
#define WACS_SYNC_BUSY              0x00

/* -----macro for  regsister@PMIC ------------------------------------------ */
#define PMIC_BASE                 (0x0000)
#define PMIC_WRP_CKPDN            (PMIC_BASE+0x011A)
#define PMIC_WRP_RST_CON          (PMIC_BASE+0x0120)
#define PMIC_TOP_CKCON2           (PMIC_BASE+0x012A)
#define PMIC_TOP_CKCON3           (PMIC_BASE+0x01D4)

/* -----macro for dewrapper regsister--------------------------------------- */
#define DEW_BASE           (0xBC00)
#define DEW_EVENT_OUT_EN   (DEW_BASE+0x0)
#define DEW_DIO_EN         (DEW_BASE+0x2)
#define DEW_EVENT_SRC_EN   (DEW_BASE+0x4)
#define DEW_EVENT_SRC      (DEW_BASE+0x6)
#define DEW_EVENT_FLAG     (DEW_BASE+0x8)
#define DEW_READ_TEST      (DEW_BASE+0xA)
#define DEW_WRITE_TEST     (DEW_BASE+0xC)
#define DEW_CRC_EN         (DEW_BASE+0xE)
#define DEW_CRC_VAL        (DEW_BASE+0x10)
#define DEW_MON_GRP_SEL    (DEW_BASE+0x12)
#define DEW_MON_FLAG_SEL   (DEW_BASE+0x14)
#define DEW_EVENT_TEST     (DEW_BASE+0x16)
#define DEW_CIPHER_KEY_SEL (DEW_BASE+0x18)
#define DEW_CIPHER_IV_SEL  (DEW_BASE+0x1A)
#define DEW_CIPHER_LOAD    (DEW_BASE+0x1C)
#define DEW_CIPHER_START   (DEW_BASE+0x1E)
#define DEW_CIPHER_RDY     (DEW_BASE+0x20)
#define DEW_CIPHER_MODE    (DEW_BASE+0x22)
#define DEW_CIPHER_SWRST   (DEW_BASE+0x24)
#define DEW_CIPHER_IV0     (DEW_BASE+0x26)
#define DEW_CIPHER_IV1     (DEW_BASE+0x28)
#define DEW_CIPHER_IV2     (DEW_BASE+0x2A)
#define DEW_CIPHER_IV3     (DEW_BASE+0x2C)
#define DEW_CIPHER_IV4     (DEW_BASE+0x2E)
#define DEW_CIPHER_IV5     (DEW_BASE+0x30)

/* -----macro for dewrapper defaule value----------------------------------- */
#define DEFAULT_VALUE_READ_TEST      0x5aa5
#define WRITE_TEST_VALUE             0xa55a

/* -----macro for manual commnd -------------------------------------------- */
#define OP_WR    0x1
#define OP_RD    0x0

#define OP_CSH   0x0
#define OP_CSL   0x1
#define OP_CK    0x2

#define OP_OUTS  0x8
#define OP_OUTD  0x9
#define OP_OUTQ  0xA

#define PWRAP_INT_DEBUG              0x80000000
#define PWRAP_INT_RRARB_DLE_FIFO_UNF 0x08000000
#define PWRAP_INT_RRARB_DLE_FIFO_OVF 0x04000000
#define PWRAP_INT_RRARB_REQ_FIFO_UNF 0x02000000
#define PWRAP_INT_RRARB_REQ_FIFO_OVF 0x01000000
#define PWRAP_INT_STAUPD_DLE_CNT_OVF 0x00800000
#define PWRAP_INT_STAUPD_ALE_CNT_OVF 0x00400000
#define PWRAP_INT_PERI_BRIDGE        0x00200000
#define PWRAP_INT_WACS2_CMD_MISS     0x00100000
#define PWRAP_INT_WACS2_UNEXP_DLE    0x00080000
#define PWRAP_INT_WACS2_UNEXP_VLDCLR 0x00040000
#define PWRAP_INT_WACS1_CMD_MISS     0x00020000
#define PWRAP_INT_WACS1_UNEXP_DLE    0x00010000
#define PWRAP_INT_WACS1_UNEXP_VLDCLR 0x00008000
#define PWRAP_INT_WACS0_CMD_MISS     0x00004000
#define PWRAP_INT_WACS0_UNEXP_DLE    0x00002000
#define PWRAP_INT_WACS0_UNEXP_VLDCLR 0x00001000
#define PWRAP_INT_HARBDLE_OVF        0x00000800
#define PWRAP_INT_HARBDLE_UNF        0x00000400
#define PWRAP_INT_AGDLE_OVF          0x00000200
#define PWRAP_INT_AGDLE_UNF          0x00000100
#define PWRAP_INT_CHDLE_OVF          0x00000080
#define PWRAP_INT_CHDLE_UNF          0x00000040
#define PWRAP_INT_MAN_CMD_MISS       0x00000020
#define PWRAP_INT_MAN_UNEXP_DLE      0x00000010
#define PWRAP_INT_MAN_UNEXP_VLDCLR   0x00000008
#define PWRAP_INT_EVENT_EMPTY        0x00000004
#define PWRAP_INT_SIG_ERR            0x00000002
#define PWRAP_INT_WDT                0x00000001

#endif				/* __PMIC_WRAP_REGS_H__ */
