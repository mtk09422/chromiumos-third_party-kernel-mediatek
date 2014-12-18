/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: Flora Fu <flora.fu@mediatek.com>
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

#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <linux/mfd/mt6397/registers.h>
#include "mt8135-pmic-wrap.h"

/* macro for wrapper status */
#define PWRAP_GET_WACS0_WDATA(x)	(((x) >> 0) & 0x0000ffff)
#define PWRAP_GET_WACS0_ADR(x)		(((x) >> 16) & 0x00007fff)
#define PWRAP_GET_WACS0_WRITE(x)	(((x) >> 31) & 0x00000001)
#define PWRAP_GET_WACS0_RDATA(x)	(((x) >> 0) & 0x0000ffff)
#define PWRAP_GET_WACS0_FSM(x)		(((x) >> 16) & 0x00000007)
#define PWRAP_STATE_SYNC_IDLE0		(1 << 20)
#define PWRAP_STATE_INIT_DONE0		(1 << 21)

/* macro for WACS FSM */
#define PWRAP_WACS_FSM_IDLE		0x00
#define PWRAP_WACS_FSM_REQ		0x02
#define PWRAP_WACS_FSM_WFDLE		0x04
#define PWRAP_WACS_FSM_WFVLDCLR		0x06
#define PWRAP_WACS_INIT_DONE		0x01
#define PWRAP_WACS_WACS_SYNC_IDLE	0x01

/* macro for device wrapper default value */
#define PWRAP_DEW_READ_TEST_VAL		0x5aa5
#define PWRAP_DEW_WRITE_TEST_VAL	0xa55a

/* macro for manual command */
#define PWRAP_OP_WR			0x1
#define PWRAP_OP_RD			0x0
#define PWRAP_OP_CSH			0x0
#define PWRAP_OP_CSL			0x1
#define PWRAP_OP_OUTS			0x8
#define PWRAP_OP_OUTD			0x9
#define PWRAP_OP_OUTQ			0xA

struct pmic_wrapper {
	struct platform_device *pdev;
	void __iomem *pwrap_base;
	void __iomem *pwrap_bridge_base;
	struct regmap *regmap;
};

static bool is_fsm_idle(u32 x)
{
	return PWRAP_GET_WACS0_FSM(x) == PWRAP_WACS_FSM_IDLE;
}

static bool is_fsm_vldclr(u32 x)
{
	return PWRAP_GET_WACS0_FSM(x) == PWRAP_WACS_FSM_WFVLDCLR;
}

static bool is_sync_idle(u32 x)
{
	return x & PWRAP_STATE_SYNC_IDLE0;
}

static bool is_fsm_idle_and_sync_idle(u32 x)
{
	return (PWRAP_GET_WACS0_FSM(x) == PWRAP_WACS_FSM_IDLE) &&
	 (x & PWRAP_STATE_SYNC_IDLE0);
}

static bool is_cipher_ready(u32 x)
{
	return x == 1;
}

static int wait_for_state_ready(
	struct pmic_wrapper *wrp, bool (*fp)(u32),
	void *wacs_register, void *wacs_vldclr_register, u32 *read_reg)
{
	u32 reg_rdata;
	unsigned long timeout;
	int timeout_retry = 0;
	struct device *dev = &wrp->pdev->dev;

	timeout = jiffies + usecs_to_jiffies(255);
	do {
		reg_rdata = readl(wacs_register);
		if (time_after(jiffies, timeout)) {
			if (timeout_retry) {
				dev_err(dev, "timeout when waiting for idle\n");
				return -ETIMEDOUT;
			}
			timeout_retry = 1;
		}
	} while (!fp(reg_rdata));

	if (read_reg)
		*read_reg = reg_rdata;

	return 0;
}

static int pwrap_write(struct pmic_wrapper *wrp, u32 adr, u32 wdata)
{
	u32 wacs_cmd;
	int ret;
	void __iomem *pwrap_base = wrp->pwrap_base;
	struct device *dev = &wrp->pdev->dev;

	ret = wait_for_state_ready(wrp, is_fsm_idle,
		pwrap_base + PWRAP_WACS2_RDATA,
		pwrap_base + PWRAP_WACS2_VLDCLR, 0);
	if (ret) {
		dev_err(dev, "%s command fail, ret=%d\n", __func__, ret);
		return ret;
	}

	wacs_cmd = (1 << 31) | ((adr >> 1) << 16) | wdata;
	writel(wacs_cmd, pwrap_base + PWRAP_WACS2_CMD);

	return 0;
}

static int pwrap_read(struct pmic_wrapper *wrp, u32 adr, u32 *rdata)
{
	u32 reg_rdata;
	u32 wacs_cmd;
	int ret;
	u32 rval;
	void __iomem *pwrap_base = wrp->pwrap_base;
	struct device *dev = &wrp->pdev->dev;

	if (!rdata)
		return -EINVAL;

	ret = wait_for_state_ready(wrp, is_fsm_idle,
		pwrap_base + PWRAP_WACS2_RDATA,
		pwrap_base + PWRAP_WACS2_VLDCLR, 0);
	if (ret) {
		dev_err(dev, "%s command fail, ret=%d\n", __func__, ret);
		return ret;
	}

	wacs_cmd = (adr >> 1) << 16;
	writel(wacs_cmd, pwrap_base + PWRAP_WACS2_CMD);

	ret = wait_for_state_ready(wrp, is_fsm_vldclr,
		pwrap_base + PWRAP_WACS2_RDATA, NULL, &reg_rdata);
	if (ret) {
		dev_err(dev, "%s read fail, ret=%d\n", __func__, ret);
		return ret;
	}
	rval = PWRAP_GET_WACS0_RDATA(reg_rdata);
	writel(1, pwrap_base + PWRAP_WACS2_VLDCLR);
	*rdata = rval;

	return 0;
}

static int pwrap_regmap_read(void *context, u32 adr, u32 *rdata)
{
	u32 reg_rdata;

	struct pmic_wrapper *wrp = context;
	void __iomem *pwrap_base = wrp->pwrap_base;

	reg_rdata = readl(pwrap_base + PWRAP_WACS2_RDATA);
	if (PWRAP_GET_WACS0_FSM(reg_rdata) == PWRAP_WACS_FSM_WFVLDCLR)
		writel(1, pwrap_base + PWRAP_WACS2_VLDCLR);

	return pwrap_read(wrp, adr, rdata);
}

static int pwrap_regmap_write(void *context, u32 adr, u32 wdata)
{
	u32 reg_rdata;

	struct pmic_wrapper *wrp = context;
	void __iomem *pwrap_base = wrp->pwrap_base;

	reg_rdata = readl(pwrap_base + PWRAP_WACS2_RDATA);
	if (PWRAP_GET_WACS0_FSM(reg_rdata) == PWRAP_WACS_FSM_WFVLDCLR)
		writel(1, pwrap_base + PWRAP_WACS2_VLDCLR);

	return pwrap_write(wrp, adr, wdata);
}

static int pwrap_reset(struct pmic_wrapper *wrp)
{
	int ret;
	struct reset_control *rstc_infracfg, *rstc_pericfg;
	struct device *dev = &wrp->pdev->dev;

	rstc_infracfg = devm_reset_control_get(dev, "infra-pwrap-rst");
	if (IS_ERR(rstc_infracfg)) {
		ret = PTR_ERR(rstc_infracfg);
		dev_err(dev, "get pwrap-rst failed=%d\n", ret);
		return ret;
	}
	rstc_pericfg = devm_reset_control_get(dev, "peri-pwrap-bridge-rst");
	if (IS_ERR(rstc_pericfg)) {
		ret = PTR_ERR(rstc_pericfg);
		dev_err(dev, "get peri-pwrap-bridge-rst failed=%d\n", ret);
		return ret;
	}

	reset_control_assert(rstc_infracfg);
	reset_control_assert(rstc_pericfg);
	reset_control_deassert(rstc_infracfg);
	reset_control_deassert(rstc_pericfg);

	return 0;
}

static int pwrap_set_clock(struct pmic_wrapper *wrp)
{
	int ret;
	void __iomem *pwrap_base = wrp->pwrap_base;
	struct device *dev = &wrp->pdev->dev;
	struct clk *pmicspi;
	struct clk *pmicspi_parent;

	pmicspi = devm_clk_get(dev, "pmicspi-sel");
	if (IS_ERR(pmicspi)) {
		ret = PTR_ERR(pmicspi);
		dev_err(dev, "pmicspi-sel fail ret=%d\n", ret);
		return ret;
	}
	pmicspi_parent = devm_clk_get(dev, "pmicspi-parent");
	if (IS_ERR(pmicspi_parent)) {
		ret = PTR_ERR(pmicspi_parent);
		dev_err(dev, "pmicspi-parent fail ret=%d\n", ret);
		return ret;
	}

	/* Note: HW design, enable clock mux and then switch to new source. */
	ret = clk_set_parent(pmicspi, pmicspi_parent);
	if (ret) {
		dev_err(dev, "prepare pmicspi clock fail, ret=%d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(pmicspi);
	if (ret) {
		dev_err(dev, "prepare pmicspi clock fail, ret=%d\n", ret);
		return ret;
	}

	/* Enable internal dynamic clock */
	writel(1, pwrap_base + PWRAP_DCM_EN);
	writel(0, pwrap_base + PWRAP_DCM_DBC_PRD);

	return 0;
}

static int pwrap_reset_spislv(struct pmic_wrapper *wrp)
{
	int ret, i;
	u32 cmd;
	void __iomem *pwrap_base = wrp->pwrap_base;
	struct device *dev = &wrp->pdev->dev;

	writel(0, pwrap_base + PWRAP_HIPRIO_ARB_EN);
	writel(0, pwrap_base + PWRAP_WRAP_EN);
	writel(1, pwrap_base + PWRAP_MUX_SEL);
	writel(1, pwrap_base + PWRAP_MAN_EN);
	writel(0, pwrap_base + PWRAP_DIO_EN);

	cmd = (PWRAP_OP_WR << 13) | (PWRAP_OP_CSL << 8);
	writel(cmd, pwrap_base + PWRAP_MAN_CMD);

	cmd = (PWRAP_OP_WR << 13) | (PWRAP_OP_OUTS << 8);
	writel(cmd, pwrap_base + PWRAP_MAN_CMD);

	cmd = (PWRAP_OP_WR << 13) | (PWRAP_OP_CSH << 8);
	writel(cmd, pwrap_base + PWRAP_MAN_CMD);

	for (i = 0; i < 4; i++) {
		cmd = (PWRAP_OP_WR << 13) | (PWRAP_OP_OUTS << 8);
		writel(cmd, pwrap_base + PWRAP_MAN_CMD);
	}
	ret = wait_for_state_ready(wrp, is_sync_idle,
	pwrap_base + PWRAP_WACS2_RDATA, NULL, 0);
	if (ret)
		dev_err(dev, "%s fail, ret=%d\n", __func__, ret);

	writel(0, pwrap_base + PWRAP_MAN_EN);
	writel(0, pwrap_base + PWRAP_MUX_SEL);

	return 0;
}

static int pwrap_init_sidly(struct pmic_wrapper *wrp)
{
	u32 arb_en_backup;
	u32 rdata;
	u32 ind;
	u32 result;
	u32 sidly;
	bool failed = false;
	void __iomem *pwrap_base = wrp->pwrap_base;
	struct device *dev = &wrp->pdev->dev;

	/* Enable WACS2 to do read test on bus */
	writel(1, pwrap_base + PWRAP_WRAP_EN);
	writel(0x8, pwrap_base + PWRAP_HIPRIO_ARB_EN);
	writel(1, pwrap_base + PWRAP_WACS2_EN);
	arb_en_backup = readl(pwrap_base + PWRAP_HIPRIO_ARB_EN);

	/* Scan all SIDLY by Read Test */
	result = 0;
	for (ind = 0; ind < 4; ind++) {
		writel(ind, wrp->pwrap_base + PWRAP_SIDLY);
		pwrap_read(wrp, PWRAP_DEW_READ_TEST, &rdata);
		if (rdata == PWRAP_DEW_READ_TEST_VAL) {
			dev_info(dev, "[Read Test] pass, SIDLY=%x\n", ind);
			result |= (0x1 << ind);
		}
	}

	/* Config SIDLY according to results */
	switch (result) {
	/* only 1 pass, choose it */
	case 0x1:
		sidly = 0;
		break;
	case 0x2:
		sidly = 1;
		break;
	case 0x4:
		sidly = 2;
		break;
	case 0x8:
		sidly = 3;
		break;
		/* two pass, choose the one on SIDLY boundary */
	case 0x3:
		sidly = 0;
		break;
	case 0x6: /* no boundary, choose smaller one */
		sidly = 1;
		break;
	case 0xc:
		sidly = 3;
		break;
		/* three pass, choose the middle one */
	case 0x7:
		sidly = 1;
		break;
	case 0xe:
		sidly = 2;
		break;
		/* four pass, choose the smaller middle one */
	case 0xf:
		sidly = 1;
		break;
		/* pass range not continuous, should not happen */
	default:
		sidly = 0;
		failed = true;
		break;
	}
	writel(sidly, pwrap_base + PWRAP_SIDLY);
	writel(arb_en_backup, pwrap_base + PWRAP_HIPRIO_ARB_EN);
	if (!failed)
		return 0;

	dev_err(dev, "%s fail, result=%x\n", __func__, result);

	return -EIO;
}

static int pwrap_init_reg_clock(struct pmic_wrapper *wrp, u32 regck_sel)
{
	u32 wdata;
	u32 rdata;
	void __iomem *pwrap_base = wrp->pwrap_base;
	struct device *dev = &wrp->pdev->dev;

	pwrap_read(wrp, MT6397_TOP_CKCON2, &rdata);
	wdata = rdata & (~(0x3 << 10));
	if (regck_sel == 1)
		wdata |= (0x1 << 10);

	if (pwrap_write(wrp, MT6397_TOP_CKCON2, wdata))  {
		dev_err(dev, "Enable PMIC TOP_CKCON2 fail\n");
		return -EFAULT;
	}
	switch (regck_sel) {
	case 1:
		writel(0xc, pwrap_base + PWRAP_CSHEXT);
		writel(0x4, pwrap_base + PWRAP_CSHEXT_WRITE);
		writel(0xc, pwrap_base + PWRAP_CSHEXT_READ);
		writel(0x0, pwrap_base + PWRAP_CSLEXT_START);
		writel(0x0, pwrap_base + PWRAP_CSLEXT_END);
		break;
	case 2:
		writel(0x4, pwrap_base + PWRAP_CSHEXT);
		writel(0x0, pwrap_base + PWRAP_CSHEXT_WRITE);
		writel(0x4, pwrap_base + PWRAP_CSHEXT_READ);
		writel(0x0, pwrap_base + PWRAP_CSLEXT_START);
		writel(0x0, pwrap_base + PWRAP_CSLEXT_END);
		break;
	default:
		writel(0xf, pwrap_base + PWRAP_CSHEXT);
		writel(0xf, pwrap_base + PWRAP_CSHEXT_WRITE);
		writel(0xf, pwrap_base + PWRAP_CSHEXT_READ);
		writel(0xf, pwrap_base + PWRAP_CSLEXT_START);
		writel(0xf, pwrap_base + PWRAP_CSLEXT_END);
		break;
	}

	/* Enable PMIC side reg clock */
	if (pwrap_write(wrp, MT6397_WRP_CKPDN, 0) ||
		pwrap_write(wrp, MT6397_WRP_RST_CON, 0)) {
		dev_err(dev, "Enable PMIC fail\n");
		return -EFAULT;
	}

	return 0;
}

static int pwrap_init_dio(struct pmic_wrapper *wrp, u32 dio_en)
{
	u32 arb_en_backup;
	u32 rdata;
	int ret;
	void __iomem *pwrap_base = wrp->pwrap_base;
	struct device *dev = &wrp->pdev->dev;

	arb_en_backup = readl(pwrap_base + PWRAP_HIPRIO_ARB_EN);
	writel(0x8, pwrap_base + PWRAP_HIPRIO_ARB_EN);
	pwrap_write(wrp, PWRAP_DEW_DIO_EN, dio_en);

	/* Check IDLE & INIT_DONE in advance */
	ret = wait_for_state_ready(wrp, is_fsm_idle_and_sync_idle,
		pwrap_base + PWRAP_WACS2_RDATA, NULL, 0);
	if (ret) {
		dev_err(dev, "%s fail, ret=%d\n", __func__, ret);
		return ret;
	}
	writel(dio_en, pwrap_base + PWRAP_DIO_EN);

	/* Read Test */
	pwrap_read(wrp, PWRAP_DEW_READ_TEST, &rdata);
	if (rdata != PWRAP_DEW_READ_TEST_VAL) {
		dev_err(dev, "DIO Test Fail en=%x, rdata=%x\n", dio_en, rdata);
		return -EFAULT;
	}

	writel(arb_en_backup, pwrap_base + PWRAP_HIPRIO_ARB_EN);
	return 0;
}

static int pwrap_init_cipher(struct pmic_wrapper *wrp)
{
	int ret;
	u32 arb_en_backup;
	u32 rdata;
	unsigned long timeout;
	int timeout_retry = 0;
	void __iomem *pwrap_base = wrp->pwrap_base;
	struct device *dev = &wrp->pdev->dev;

	arb_en_backup = readl(pwrap_base + PWRAP_HIPRIO_ARB_EN);

	writel(0x8, pwrap_base + PWRAP_HIPRIO_ARB_EN);
	writel(1, pwrap_base + PWRAP_CIPHER_SWRST);
	writel(0, pwrap_base + PWRAP_CIPHER_SWRST);
	writel(1, pwrap_base + PWRAP_CIPHER_KEY_SEL);
	writel(2, pwrap_base + PWRAP_CIPHER_IV_SEL);
	writel(1, pwrap_base + PWRAP_CIPHER_LOAD);
	writel(1, pwrap_base + PWRAP_CIPHER_START);

	/* Config cipher mode @PMIC */
	pwrap_write(wrp, PWRAP_DEW_CIPHER_SWRST, 0x1);
	pwrap_write(wrp, PWRAP_DEW_CIPHER_SWRST, 0x0);
	pwrap_write(wrp, PWRAP_DEW_CIPHER_KEY_SEL, 0x1);
	pwrap_write(wrp, PWRAP_DEW_CIPHER_IV_SEL, 0x2);
	pwrap_write(wrp, PWRAP_DEW_CIPHER_LOAD, 0x1);
	pwrap_write(wrp, PWRAP_DEW_CIPHER_START, 0x1);

	/* wait for cipher data ready@AP */
	ret = wait_for_state_ready(wrp, is_cipher_ready,
		pwrap_base + PWRAP_CIPHER_RDY, NULL, 0);
	if (ret) {
		dev_err(dev, "cipher data ready@AP fail, ret=%d\n", ret);
		return ret;
	}

	/* wait for cipher data ready@PMIC */
	timeout = jiffies + usecs_to_jiffies(255);
	do {
		pwrap_read(wrp, PWRAP_DEW_CIPHER_RDY, &rdata);
		if (time_after(jiffies, timeout)) {
			if (timeout_retry) {
				dev_err(dev, "timeout when waiting for idle\n");
				return -ETIMEDOUT;
			}
			timeout_retry = 1;
		}
	} while (rdata != 0x1);

	/* wait for cipher mode idle */
	pwrap_write(wrp, PWRAP_DEW_CIPHER_MODE, 0x1);
	ret = wait_for_state_ready(wrp, is_fsm_idle_and_sync_idle,
		pwrap_base + PWRAP_WACS2_RDATA, NULL, 0);
	if (ret) {
		dev_err(dev, "cipher mode idle fail, ret=%d\n", ret);
		return ret;
	}

	writel(1, pwrap_base + PWRAP_CIPHER_MODE);
	writel(arb_en_backup, pwrap_base + PWRAP_HIPRIO_ARB_EN);
	/* Write Test */
	if (pwrap_write(wrp, PWRAP_DEW_WRITE_TEST, PWRAP_DEW_WRITE_TEST_VAL) ||
	    pwrap_read(wrp, PWRAP_DEW_WRITE_TEST, &rdata) ||
			(rdata != PWRAP_DEW_WRITE_TEST_VAL)) {
		dev_err(dev, "rdata=0x%04X\n", rdata);
		return -EFAULT;
	}

	return 0;
}

static int pwrap_init_crc(struct pmic_wrapper *wrp)
{
	void __iomem *pwrap_base = wrp->pwrap_base;

	if (pwrap_write(wrp, PWRAP_DEW_CRC_EN, 0x1))
		return -EFAULT;

	writel(0x1, pwrap_base + PWRAP_CRC_EN);
	writel(0x0, pwrap_base + PWRAP_SIG_MODE);
	writel(PWRAP_DEW_CRC_VAL, pwrap_base + PWRAP_SIG_ADR);

	return 0;
}

static int pwrap_enable_wacs(struct pmic_wrapper *wrp)
{
	void __iomem *pwrap_base = wrp->pwrap_base;

	writel(0x1ff, pwrap_base + PWRAP_HIPRIO_ARB_EN);
	writel(0x7, pwrap_base + PWRAP_RRARB_EN);
	writel(0x1, pwrap_base + PWRAP_WACS0_EN);
	writel(0x1, pwrap_base + PWRAP_WACS1_EN);
	writel(0x1, pwrap_base + PWRAP_WACS2_EN);
	writel(0x5, pwrap_base + PWRAP_STAUPD_PRD);
	writel(0xff, pwrap_base + PWRAP_STAUPD_GRPEN);
	writel(0xf, pwrap_base + PWRAP_WDT_UNIT);
	writel(0xffffffff, pwrap_base + PWRAP_WDT_SRC_EN);
	writel(0x1, pwrap_base + PWRAP_TIMER_EN);
	writel(~((1 << 31) | (1 << 1)), pwrap_base + PWRAP_INT_EN);

	return 0;
}

static int pwrap_enable_event_bridge(struct pmic_wrapper *wrp)
{
	void __iomem *pwrap_base = wrp->pwrap_base;
	void __iomem *pwrap_bridge_base = wrp->pwrap_bridge_base;
	struct device *dev = &wrp->pdev->dev;

	/* enable pwrap events and pwrap bridge in AP side */
	writel(0x1, pwrap_base + PWRAP_EVENT_IN_EN);
	writel(0xffff, pwrap_base + PWRAP_EVENT_DST_EN);
	writel(0x7f, pwrap_bridge_base + PWRAP_BRIDGE_IORD_ARB_EN);
	writel(0x1, pwrap_bridge_base + PWRAP_BRIDGE_WACS3_EN);
	writel(0x1, pwrap_bridge_base + PWRAP_BRIDGE_WACS4_EN);
	writel(0x1, pwrap_bridge_base + PWRAP_BRIDGE_WDT_UNIT);
	writel(0xffff, pwrap_bridge_base + PWRAP_BRIDGE_WDT_SRC_EN);
	writel(0x1, pwrap_bridge_base + PWRAP_BRIDGE_TIMER_EN);
	writel(0x7ff, pwrap_bridge_base + PWRAP_BRIDGE_INT_EN);

	/* enable PMIC event out and sources */
	if (pwrap_write(wrp, PWRAP_DEW_EVENT_OUT_EN, 0x1) ||
	    pwrap_write(wrp, PWRAP_DEW_EVENT_SRC_EN, 0xffff)) {
		dev_err(dev, "enable dewrap fail\n");
		return -EFAULT;
	}

	return 0;
}

static int pwrap_switch_event_pin(struct pmic_wrapper *wrp)
{
	u32 rdata;
	struct device *dev = &wrp->pdev->dev;

	/* switch event pin from usbdl mode to normal mode @ MT6397 */
	if (pwrap_read(wrp, MT6397_TOP_CKCON3, &rdata) ||
	    pwrap_write(wrp, MT6397_TOP_CKCON3, (rdata & 0x0007))) {
		dev_err(dev, "switch event pin fail\n");
		return -EFAULT;
	}

	return 0;
};

static int pwrap_init_done(struct pmic_wrapper *wrp)
{
	void __iomem *pwrap_base = wrp->pwrap_base;
	void __iomem *pwrap_bridge_base = wrp->pwrap_bridge_base;

	writel(1, pwrap_base + PWRAP_INIT_DONE2);
	writel(1, pwrap_base + PWRAP_INIT_DONE0);
	writel(1, pwrap_base + PWRAP_INIT_DONE1);
	writel(1, pwrap_bridge_base + PWRAP_BRIDGE_INIT_DONE3);
	writel(1, pwrap_bridge_base + PWRAP_BRIDGE_INIT_DONE4);

	return 0;
}

static int pwrap_init(struct pmic_wrapper *wrp)
{
	int ret;

	/* Reset pwrap and pwrap bridge in infracfg and perifcfg. */
	ret = pwrap_reset(wrp);
	if (ret)
		return ret;

	/* Set SPI_CK frequency = 26MHz */
	ret = pwrap_set_clock(wrp);
	if (ret)
		return ret;

	/* Reset SPI slave */
	ret = pwrap_reset_spislv(wrp);
	if (ret)
		return ret;

	/* Setup serial input delay */
	ret = pwrap_init_sidly(wrp);
	if (ret)
		return ret;

	/* SPI Waveform Configuration 0:safe mode, 1:18MHz, 2:26MHz */
	ret = pwrap_init_reg_clock(wrp, 2);
	if (ret)
		return ret;

	/* Enable dual IO mode */
	ret = pwrap_init_dio(wrp, 1);
	if (ret)
		return ret;

	/* Enable encryption */
	ret = pwrap_init_cipher(wrp);
	if (ret)
		return ret;

	/* Signature checking - using CRC */
	ret = pwrap_init_crc(wrp);
	if (ret)
		return ret;

	/* Enable all wrapper access */
	ret = pwrap_enable_wacs(wrp);
	if (ret)
		return ret;

	 /* Switch event pin from usbdl mode to normal mode */
	ret = pwrap_switch_event_pin(wrp);
	if (ret)
		return ret;

	/* Enable evnts and pwrap bridge */
	ret = pwrap_enable_event_bridge(wrp);
	if (ret)
		return ret;

	/* Setup the init done registers */
	ret = pwrap_init_done(wrp);
	if (ret)
		return ret;

	return 0;
}

static int pwrap_iomap_init(struct platform_device *pdev)
{
	struct resource *res;
	struct pmic_wrapper *wrp = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pwrap-base");
	if (!res) {
		dev_err(dev, "could not get pwrap-base resource\n");
		return -ENODEV;
	}
	wrp->pwrap_base = devm_ioremap(dev, res->start, resource_size(res));
	if (!wrp->pwrap_base) {
		dev_err(dev, "could not get pwrap_base\n");
		return -ENOMEM;
	}
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
		"pwrap-bridge-base");
	if (!res) {
		dev_err(dev, "could not get pwrap-bridge-base resource\n");
		return -ENODEV;
	}
	wrp->pwrap_bridge_base = devm_ioremap(dev, res->start,
		resource_size(res));
	if (!wrp->pwrap_bridge_base) {
		dev_err(dev, "could not get pwrap_bridge_base\n");
		return -ENOMEM;
	}

	return 0;
}

static int pwrap_check_and_init(struct pmic_wrapper *wrp)
{
	int ret;
	u32 rdata;
	void __iomem *pwrap_base = wrp->pwrap_base;
	struct device *dev = &wrp->pdev->dev;

	rdata = readl(pwrap_base + PWRAP_INIT_DONE2);
	if (rdata)
		goto done;

	ret = pwrap_init(wrp);
	if (ret) {
		dev_err(dev, "PMIC wrapper init error, ret=%d\n", ret);
		return ret;
	}

done:
	rdata = readl(pwrap_base + PWRAP_WACS2_RDATA);
	if (!(rdata & PWRAP_STATE_INIT_DONE0)) {
		dev_err(dev, "initialization isn't finished\n");
		return -ENODEV;
	}

	return 0;
}

static irqreturn_t pwrap_interrupt(int irqno, void *dev_id)
{
	u32 rdata;
	struct pmic_wrapper *wrp = dev_id;
	void __iomem *pwrap_base = wrp->pwrap_base;
	struct device *dev = &wrp->pdev->dev;

	rdata = readl(pwrap_base + PWRAP_INT_FLG);
	dev_err(dev, "unexpected interrupt int=0x%x\n", rdata);

	writel(0x8, pwrap_base + PWRAP_HARB_HPRIO);
	writel(0xffffffff, pwrap_base + PWRAP_INT_CLR);

	return IRQ_HANDLED;
}

const struct regmap_config pwrap_regmap_config = {
	.reg_bits = 16,
	.val_bits = 16,
	.reg_read = pwrap_regmap_read,
	.reg_write = pwrap_regmap_write,
};

static struct of_dev_auxdata pwrap_auxdata_lookup[] = {
	OF_DEV_AUXDATA("mediatek,mt6397", 0, "mt6397", NULL),
	{},
};

static int pwrap_probe(struct platform_device *pdev)
{
	int ret, irq;
	struct pmic_wrapper *wrp;
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;

	wrp = devm_kzalloc(dev, sizeof(struct pmic_wrapper), GFP_KERNEL);
	if (!wrp)
		return -ENOMEM;

	platform_set_drvdata(pdev, wrp);

	ret = pwrap_iomap_init(pdev);
	if (ret) {
		dev_err(dev, "pwrap_iomap_init, ret=%d\n", ret);
		return ret;
	}

	wrp->pdev = pdev;
	ret = pwrap_check_and_init(wrp);
	if (ret) {
		dev_err(dev, "PMIC wrapper HW init failed=%d\n", ret);
		return ret;
	}

	/* Register pwrap irq to catch interrupt when pwrap behaves abnormal. */
	irq = platform_get_irq(pdev, 0);
	ret = devm_request_threaded_irq(dev, irq,
			pwrap_interrupt, NULL,
			IRQF_TRIGGER_HIGH, "mt8135-pwrap", wrp);
	if (ret) {
		dev_err(dev, "Register IRQ failed, ret=%d\n", ret);
		return ret;
	}

	wrp->regmap = devm_regmap_init(dev, NULL, wrp, &pwrap_regmap_config);
	if (IS_ERR(wrp->regmap)) {
		ret = PTR_ERR(wrp->regmap);
		dev_err(dev, "Failed to allocate register map, ret=%d\n",
			ret);
		return ret;
	}

	pwrap_auxdata_lookup[0].platform_data = wrp->regmap;
	ret = of_platform_populate(np, NULL, pwrap_auxdata_lookup, dev);
	if (ret) {
		dev_err(dev, "%s fail to create devices\n", np->full_name);
		return ret;
	}

	return 0;
}

static struct of_device_id of_pwrap_match_tbl[] = {
	{.compatible = "mediatek,mt8135-pwrap",},
	{}
};
MODULE_DEVICE_TABLE(of, of_pwrap_match_tbl);

static struct platform_driver pwrap_drv = {
	.driver = {
		.name = "mt8135-pwrap",
		.of_match_table = of_match_ptr(of_pwrap_match_tbl),
	},
	.probe = pwrap_probe,
};

module_platform_driver(pwrap_drv);

MODULE_AUTHOR("Flora Fu <flora.fu@mediatek.com>");
MODULE_DESCRIPTION("MediaTek MT8135 PMIC Wrapper Driver");
MODULE_LICENSE("GPL");
