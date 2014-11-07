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

/* macro for wrapper registers */
#define PWRAP_MUX_SEL               0x0
#define PWRAP_WRAP_EN               0x4
#define PWRAP_DIO_EN                0x8
#define PWRAP_SIDLY                 0xC
#define PWRAP_CSHEXT                0x10
#define PWRAP_CSHEXT_WRITE          0x14
#define PWRAP_CSHEXT_READ           0x18
#define PWRAP_CSLEXT_START          0x1C
#define PWRAP_CSLEXT_END            0x20
#define PWRAP_STAUPD_PRD            0x24
#define PWRAP_STAUPD_GRPEN          0x28
#define PWRAP_STAUPD_MAN_TRIG       0x2C
#define PWRAP_STAUPD_STA            0x30
#define PWRAP_EVENT_IN_EN           0x34
#define PWRAP_EVENT_DST_EN          0x38
#define PWRAP_WRAP_STA              0x3C
#define PWRAP_RRARB_INIT            0x40
#define PWRAP_RRARB_EN              0x44
#define PWRAP_RRARB_STA0            0x48
#define PWRAP_RRARB_STA1            0x4C
#define PWRAP_HARB_INIT             0x50
#define PWRAP_HARB_HPRIO            0x54
#define PWRAP_HIPRIO_ARB_EN         0x58
#define PWRAP_HARB_STA0             0x5C
#define PWRAP_HARB_STA1             0x60
#define PWRAP_MAN_EN                0x64
#define PWRAP_MAN_CMD               0x68
#define PWRAP_MAN_RDATA             0x6C
#define PWRAP_MAN_VLDCLR            0x70
#define PWRAP_WACS0_EN              0x74
#define PWRAP_INIT_DONE0            0x78
#define PWRAP_WACS0_CMD             0x7C
#define PWRAP_WACS0_RDATA           0x80
#define PWRAP_WACS0_VLDCLR          0x84
#define PWRAP_WACS1_EN              0x88
#define PWRAP_INIT_DONE1            0x8C
#define PWRAP_WACS1_CMD             0x90
#define PWRAP_WACS1_RDATA           0x94
#define PWRAP_WACS1_VLDCLR          0x98
#define PWRAP_WACS2_EN              0x9C
#define PWRAP_INIT_DONE2            0xA0
#define PWRAP_WACS2_CMD             0xA4
#define PWRAP_WACS2_RDATA           0xA8
#define PWRAP_WACS2_VLDCLR          0xAC
#define PWRAP_INT_EN                0xB0
#define PWRAP_INT_FLG_RAW           0xB4
#define PWRAP_INT_FLG               0xB8
#define PWRAP_INT_CLR               0xBC
#define PWRAP_SIG_ADR               0xC0
#define PWRAP_SIG_MODE              0xC4
#define PWRAP_SIG_VALUE             0xC8
#define PWRAP_SIG_ERRVAL            0xCC
#define PWRAP_CRC_EN                0xD0
#define PWRAP_EVENT_STA             0xD4
#define PWRAP_EVENT_STACLR          0xD8
#define PWRAP_TIMER_EN              0xDC
#define PWRAP_TIMER_STA             0xE0
#define PWRAP_WDT_UNIT              0xE4
#define PWRAP_WDT_SRC_EN            0xE8
#define PWRAP_WDT_FLG               0xEC
#define PWRAP_DEBUG_INT_SEL         0xF0
#define PWRAP_CIPHER_KEY_SEL        0x134
#define PWRAP_CIPHER_IV_SEL         0x138
#define PWRAP_CIPHER_LOAD           0x13C
#define PWRAP_CIPHER_START          0x140
#define PWRAP_CIPHER_RDY            0x144
#define PWRAP_CIPHER_MODE           0x148
#define PWRAP_CIPHER_SWRST          0x14C
#define PWRAP_DCM_EN                0x15C
#define PWRAP_DCM_DBC_PRD           0x160

/* macro for wrapper bridge registers */
#define PWRAP_BRIDGE_IARB_INIT      0x0
#define PWRAP_BRIDGE_IORD_ARB_EN    0x4
#define PWRAP_BRIDGE_IARB_STA0      0x8
#define PWRAP_BRIDGE_IARB_STA1      0xC
#define PWRAP_BRIDGE_WACS3_EN       0x10
#define PWRAP_BRIDGE_INIT_DONE3     0x14
#define PWRAP_BRIDGE_WACS3_CMD      0x18
#define PWRAP_BRIDGE_WACS3_RDATA    0x1C
#define PWRAP_BRIDGE_WACS3_VLDCLR   0x20
#define PWRAP_BRIDGE_WACS4_EN       0x24
#define PWRAP_BRIDGE_INIT_DONE4     0x28
#define PWRAP_BRIDGE_WACS4_CMD      0x2C
#define PWRAP_BRIDGE_WACS4_RDATA    0x30
#define PWRAP_BRIDGE_WACS4_VLDCLR   0x34
#define PWRAP_BRIDGE_INT_EN         0x38
#define PWRAP_BRIDGE_INT_FLG_RAW    0x3C
#define PWRAP_BRIDGE_INT_FLG        0x40
#define PWRAP_BRIDGE_INT_CLR        0x44
#define PWRAP_BRIDGE_TIMER_EN       0x48
#define PWRAP_BRIDGE_TIMER_STA      0x4C
#define PWRAP_BRIDGE_WDT_UNIT       0x50
#define PWRAP_BRIDGE_WDT_SRC_EN     0x54
#define PWRAP_BRIDGE_WDT_FLG        0x58
#define PWRAP_BRIDGE_DEBUG_INT_SE   0x5C

/* macro for slave device wrapper registers */
#define PWRAP_DEW_BASE              0xBC00
#define PWRAP_DEW_EVENT_OUT_EN      (PWRAP_DEW_BASE + 0x0)
#define PWRAP_DEW_DIO_EN            (PWRAP_DEW_BASE + 0x2)
#define PWRAP_DEW_EVENT_SRC_EN      (PWRAP_DEW_BASE + 0x4)
#define PWRAP_DEW_EVENT_SRC         (PWRAP_DEW_BASE + 0x6)
#define PWRAP_DEW_EVENT_FLAG        (PWRAP_DEW_BASE + 0x8)
#define PWRAP_DEW_READ_TEST         (PWRAP_DEW_BASE + 0xA)
#define PWRAP_DEW_WRITE_TEST        (PWRAP_DEW_BASE + 0xC)
#define PWRAP_DEW_CRC_EN            (PWRAP_DEW_BASE + 0xE)
#define PWRAP_DEW_CRC_VAL           (PWRAP_DEW_BASE + 0x10)
#define PWRAP_DEW_MON_GRP_SEL       (PWRAP_DEW_BASE + 0x12)
#define PWRAP_DEW_MON_FLAG_SEL      (PWRAP_DEW_BASE + 0x14)
#define PWRAP_DEW_EVENT_TEST        (PWRAP_DEW_BASE + 0x16)
#define PWRAP_DEW_CIPHER_KEY_SEL    (PWRAP_DEW_BASE + 0x18)
#define PWRAP_DEW_CIPHER_IV_SEL     (PWRAP_DEW_BASE + 0x1A)
#define PWRAP_DEW_CIPHER_LOAD       (PWRAP_DEW_BASE + 0x1C)
#define PWRAP_DEW_CIPHER_START      (PWRAP_DEW_BASE + 0x1E)
#define PWRAP_DEW_CIPHER_RDY        (PWRAP_DEW_BASE + 0x20)
#define PWRAP_DEW_CIPHER_MODE       (PWRAP_DEW_BASE + 0x22)
#define PWRAP_DEW_CIPHER_SWRST      (PWRAP_DEW_BASE + 0x24)

#endif	/* __PMIC_WRAP_REGS_H__ */
