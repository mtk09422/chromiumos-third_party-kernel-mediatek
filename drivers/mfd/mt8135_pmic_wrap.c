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

#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/reset.h>
#include <linux/clk.h>
#include <linux/regmap.h>
#include "mt_pmic_wrap.h"
#include "mt8135_pmic_wrap.h"

#define PMIC_WRAP_DEVICE "mt8135-pwrap"

static bool _pwrap_timeout_ns(struct pmic_wrapper *wrp,
		u64 start_time_ns, u64 timeout_time_ns)
{
	u64 cur_time = 0;
	u64 elapse_time = 0;

	cur_time = sched_clock();
	if (cur_time < start_time_ns) {
		dev_err(&wrp->pdev->dev,
			"%s: Timer overflow! start%lld cur timer%lld\n",
			__func__, start_time_ns, cur_time);
		start_time_ns = cur_time;
		timeout_time_ns = 255 * 1000;	/* 255us */
		dev_err(&wrp->pdev->dev,
			"%s: reset timer! start%lld setting%lld\n",
			__func__, start_time_ns, timeout_time_ns);
	}
	elapse_time = cur_time - start_time_ns;

	if (timeout_time_ns <= elapse_time)
		return true;

	return false;
}

static u64 _pwrap_time2ns(u64 time_us)
{
	return time_us * 1000;
}

static inline bool is_fsm_idle(u32 x)
{
	return GET_WACS0_FSM(x) != WACS_FSM_IDLE;
}

static inline bool is_fsm_vldclr(u32 x)
{
	return GET_WACS0_FSM(x) != WACS_FSM_WFVLDCLR;
}

static inline bool is_sync_idle(u32 x)
{
	return GET_SYNC_IDLE0(x) != WACS_SYNC_IDLE;
}

static inline bool is_fsm_idle_and_sync_idle(u32 x)
{
	return (GET_WACS0_FSM(x) != WACS_FSM_IDLE) ||
	 (GET_SYNC_IDLE0(x) != WACS_SYNC_IDLE);
}

static inline bool is_cipher_ready(u32 x)
{
	return x != 1;
}

static inline int wait_for_state_ready(struct pmic_wrapper *wrp,
		bool (*fp)(u32), u32 timeout_us, void *wacs_register,
		void *wacs_vldclr_register, u32 *read_reg)
{
	u64 start_time_ns = 0, timeout_ns = 0;
	u32 reg_rdata;

	start_time_ns = sched_clock();
	timeout_ns = _pwrap_time2ns(timeout_us);

	do {
		reg_rdata = readl(wacs_register);
		if ((wrp->is_done) && (NULL != wacs_vldclr_register)) {
			/*
			 * if last read command timeout,clear vldclr bit
			 * read command state machine:
			 * FSM_REQ-->wfdle-->WFVLDCLR;write:FSM_REQ-->idle
			 */
			switch (GET_WACS0_FSM(reg_rdata)) {
			case WACS_FSM_WFVLDCLR:
				writel(1, wacs_vldclr_register);
				dev_err(&wrp->pdev->dev,
					"WACS_FSM = PMIC_WRAP_WACS_VLDCLR\n");
				break;
			case WACS_FSM_WFDLE:
				dev_err(&wrp->pdev->dev, "WACS_FSM = WACS_FSM_WFDLE\n");
				break;
			case WACS_FSM_REQ:
				dev_err(&wrp->pdev->dev, "WACS_FSM = WACS_FSM_REQ\n");
				break;
			default:
				break;
			}
		}
		if (_pwrap_timeout_ns(wrp, start_time_ns, timeout_ns)) {
			dev_err(&wrp->pdev->dev, "timeout when waiting for idle\n");
			return -ETIMEDOUT;
		}
	} while (fp(reg_rdata));

	if (read_reg)
		*read_reg = reg_rdata;

	return 0;
}

static int pwrap_write(struct pmic_wrapper *wrp, u32 adr, u32 wdata)
{
	u32 wacs_cmd;
	int ret;
	void __iomem *pwrap_base = wrp->pwrap_base;

	ret = wait_for_state_ready(wrp, is_fsm_idle,
		TIMEOUT_WAIT_IDLE,
		pwrap_base + PMIC_WRAP_WACS2_RDATA,
		pwrap_base + PMIC_WRAP_WACS2_VLDCLR, 0);
	if (ret) {
		dev_err(&wrp->pdev->dev, "%s write command fail, ret=%d\n",
			__func__, ret);
		return ret;
	}

	wacs_cmd = (1 << 31) | ((adr >> 1) << 16) | wdata;
	writel(wacs_cmd, pwrap_base + PMIC_WRAP_WACS2_CMD);

	return 0;
}

static int pwrap_read(struct pmic_wrapper *wrp, u32 adr, u32 *rdata)
{
	u32 reg_rdata;
	u32 wacs_cmd;
	int ret;
	u32 rval;
	void __iomem *pwrap_base = wrp->pwrap_base;

	if (!rdata)
		return -EINVAL;

	ret = wait_for_state_ready(wrp, is_fsm_idle,
		TIMEOUT_WAIT_IDLE,
		pwrap_base + PMIC_WRAP_WACS2_RDATA,
		pwrap_base + PMIC_WRAP_WACS2_VLDCLR, 0);
	if (ret) {
		dev_err(&wrp->pdev->dev, "%s read command fail, ret=%d\n",
			__func__, ret);
		return ret;
	}

	wacs_cmd = (adr >> 1) << 16;
	writel(wacs_cmd, pwrap_base + PMIC_WRAP_WACS2_CMD);

	ret = wait_for_state_ready(wrp, is_fsm_vldclr,
		TIMEOUT_WAIT_IDLE,
		pwrap_base + PMIC_WRAP_WACS2_RDATA, NULL, &reg_rdata);
	if (ret) {
		dev_err(&wrp->pdev->dev, "%s read fail, ret=%d\n",
			__func__, ret);
		return ret;
	}
	rval = GET_WACS0_RDATA(reg_rdata);
	writel(1, pwrap_base + PMIC_WRAP_WACS2_VLDCLR);
	*rdata = rval;

	return 0;
}

static int pwrap_regmap_read(void *context, u32 adr, u32 *rdata)
{
	struct pmic_wrapper *wrp = context;

	if (!wrp->is_done)
		return -ENODEV;

	return pwrap_read(wrp, adr, rdata);
}

static int pwrap_regmap_write(void *context, u32 adr, u32 wdata)
{
	struct pmic_wrapper *wrp = context;

	if (!wrp->is_done)
		return -ENODEV;

	return pwrap_write(wrp, adr, wdata);
}

static int pwrap_init_dio(struct pmic_wrapper *wrp, u32 dio_en)
{
	u32 arb_en_backup;
	u32 rdata;
	int ret;
	void __iomem *pwrap_base = wrp->pwrap_base;

	arb_en_backup = readl(pwrap_base + PMIC_WRAP_HIPRIO_ARB_EN);
	writel(WACS2, pwrap_base + PMIC_WRAP_HIPRIO_ARB_EN);
	pwrap_write(wrp, DEW_DIO_EN, dio_en);

	/* Check IDLE & INIT_DONE in advance */
	ret = wait_for_state_ready(wrp, is_fsm_idle_and_sync_idle,
		TIMEOUT_WAIT_IDLE,
		pwrap_base + PMIC_WRAP_WACS2_RDATA, NULL, 0);
	if (ret) {
		dev_err(&wrp->pdev->dev, "pwrap_init_dio fail, ret=%d\n", ret);
		return ret;
	}
	writel(dio_en, pwrap_base + PMIC_WRAP_DIO_EN);

	/* Read Test */
	pwrap_read(wrp, DEW_READ_TEST, &rdata);
	if (rdata != DEFAULT_VALUE_READ_TEST) {
		dev_err(&wrp->pdev->dev,
			"[Dio_mode][Read Test]Fail dio_en=%x, rdata=%x\n",
			dio_en, rdata);
		return -EFAULT;
	}

	writel(arb_en_backup, pwrap_base + PMIC_WRAP_HIPRIO_ARB_EN);
	return 0;
}

static int pwrap_init_cipher(struct pmic_wrapper *wrp)
{
	int ret;
	u32 arb_en_backup;
	u32 rdata;
	u32 start_time_ns = 0, timeout_ns = 0;
	void __iomem *pwrap_base = wrp->pwrap_base;

	arb_en_backup = readl(pwrap_base + PMIC_WRAP_HIPRIO_ARB_EN);

	writel(WACS2, pwrap_base + PMIC_WRAP_HIPRIO_ARB_EN);
	writel(1, pwrap_base + PMIC_WRAP_CIPHER_SWRST);
	writel(0, pwrap_base + PMIC_WRAP_CIPHER_SWRST);
	writel(1, pwrap_base + PMIC_WRAP_CIPHER_KEY_SEL);
	writel(2, pwrap_base + PMIC_WRAP_CIPHER_IV_SEL);
	writel(1, pwrap_base + PMIC_WRAP_CIPHER_LOAD);
	writel(1, pwrap_base + PMIC_WRAP_CIPHER_START);

	/* Config CIPHER @ PMIC */
	pwrap_write(wrp, DEW_CIPHER_SWRST, 0x1);
	pwrap_write(wrp, DEW_CIPHER_SWRST, 0x0);
	pwrap_write(wrp, DEW_CIPHER_KEY_SEL, 0x1);
	pwrap_write(wrp, DEW_CIPHER_IV_SEL, 0x2);
	pwrap_write(wrp, DEW_CIPHER_LOAD, 0x1);
	pwrap_write(wrp, DEW_CIPHER_START, 0x1);

	/* wait for cipher data ready@AP */
	ret = wait_for_state_ready(wrp, is_cipher_ready,
		TIMEOUT_WAIT_IDLE,
		pwrap_base + PMIC_WRAP_CIPHER_RDY, NULL, 0);
	if (ret) {
		dev_err(&wrp->pdev->dev,
			"wait for cipher data ready@AP fail, ret=%d\n", ret);
		return ret;
	}

	/* wait for cipher data ready@PMIC */
	start_time_ns = sched_clock();
	timeout_ns = _pwrap_time2ns(0xFFFFFF);
	do {
		pwrap_read(wrp, DEW_CIPHER_RDY, &rdata);
		if (_pwrap_timeout_ns(wrp, start_time_ns, timeout_ns)) {
			dev_err(&wrp->pdev->dev, "wait for cipher data ready@PMIC\n");
			return -ETIMEDOUT;
		}
	} while (rdata != 0x1);

	/* wait for cipher mode idle */
	pwrap_write(wrp, DEW_CIPHER_MODE, 0x1);
	ret = wait_for_state_ready(wrp, is_fsm_idle_and_sync_idle,
		TIMEOUT_WAIT_IDLE,
		pwrap_base + PMIC_WRAP_WACS2_RDATA, NULL, 0);
	if (ret) {
		dev_err(&wrp->pdev->dev,
			"wait for cipher mode idle fail, ret=%d\n", ret);
		return ret;
	}
	writel(1, pwrap_base + PMIC_WRAP_CIPHER_MODE);

	/* Read Test */
	pwrap_read(wrp, DEW_READ_TEST, &rdata);
	if (rdata != DEFAULT_VALUE_READ_TEST) {
		dev_err(&wrp->pdev->dev,
			"pwrap_init_cipher, error code=%x, rdata=%x\n",
			1, rdata);
		return -EFAULT;
	}

	writel(arb_en_backup, pwrap_base + PMIC_WRAP_HIPRIO_ARB_EN);
	return 0;
}

static int pwrap_init_sidly(struct pmic_wrapper *wrp)
{
	u32 arb_en_backup;
	u32 rdata;
	u32 ind = 0;
	u32 result;
	bool failed = false;
	void __iomem *pwrap_base = wrp->pwrap_base;

	arb_en_backup = readl(pwrap_base + PMIC_WRAP_HIPRIO_ARB_EN);
	writel(WACS2, pwrap_base + PMIC_WRAP_HIPRIO_ARB_EN);

	/* Scan all SIDLY by Read Test */
	result = 0;
	for (ind = 0; ind < 4; ind++) {
		writel(ind, wrp->pwrap_base + PMIC_WRAP_SIDLY);
		pwrap_read(wrp, DEW_READ_TEST, &rdata);
		if (rdata == DEFAULT_VALUE_READ_TEST) {
			dev_err(&wrp->pdev->dev, "[Read Test] pass,SIDLY=%x rdata=%x\n",
				ind, rdata);
			result |= (0x1 << ind);
		} else
			dev_err(&wrp->pdev->dev, "[Read Test] fail,SIDLY=%x,rdata=%x\n",
				ind, rdata);
	}

	/* Config SIDLY according to result */
	switch (result) {
		/* Only 1 pass, choose it */
	case 0x1:
		writel(0, pwrap_base + PMIC_WRAP_SIDLY);
		break;
	case 0x2:
		writel(1, pwrap_base + PMIC_WRAP_SIDLY);
		break;
	case 0x4:
		writel(2, pwrap_base + PMIC_WRAP_SIDLY);
		break;
	case 0x8:
		writel(3, pwrap_base + PMIC_WRAP_SIDLY);
		break;

		/* two pass, choose the one on SIDLY boundary */
	case 0x3:
		writel(0, pwrap_base + PMIC_WRAP_SIDLY);
		break;
	case 0x6:
		/* no boundary, choose smaller one */
		writel(1, pwrap_base + PMIC_WRAP_SIDLY);
		break;
	case 0xc:
		writel(3, pwrap_base + PMIC_WRAP_SIDLY);
		break;

		/* three pass, choose the middle one */
	case 0x7:
		writel(1, pwrap_base + PMIC_WRAP_SIDLY);
		break;
	case 0xe:
		writel(2, pwrap_base + PMIC_WRAP_SIDLY);
		break;
		/* four pass, choose the smaller middle one */
	case 0xf:
		writel(1, pwrap_base + PMIC_WRAP_SIDLY);
		break;

		/* pass range not continuous, should not happen */
	default:
		writel(0, pwrap_base + PMIC_WRAP_SIDLY);
		failed = true;
		break;
	}

	writel(arb_en_backup, pwrap_base + PMIC_WRAP_HIPRIO_ARB_EN);
	if (!failed)
		return 0;

	dev_err(&wrp->pdev->dev,
		"error,pwrap_init_sidly fail, result=%x\n", result);
	return -EIO;
}

static int pwrap_reset_spislv(struct pmic_wrapper *wrp)
{
	int ret;
	void __iomem *pwrap_base = wrp->pwrap_base;

	writel(0, pwrap_base + PMIC_WRAP_HIPRIO_ARB_EN);
	writel(0, pwrap_base + PMIC_WRAP_WRAP_EN);
	writel(1, pwrap_base + PMIC_WRAP_MUX_SEL);
	writel(1, pwrap_base + PMIC_WRAP_MAN_EN);
	writel(0, pwrap_base + PMIC_WRAP_DIO_EN);

	writel((OP_WR << 13) | (OP_CSL << 8),
		pwrap_base + PMIC_WRAP_MAN_CMD);
	writel((OP_WR << 13) | (OP_OUTS << 8),
		pwrap_base + PMIC_WRAP_MAN_CMD);
	writel((OP_WR << 13) | (OP_CSH << 8),
		pwrap_base + PMIC_WRAP_MAN_CMD);
	writel((OP_WR << 13) | (OP_OUTS << 8),
		pwrap_base + PMIC_WRAP_MAN_CMD);
	writel((OP_WR << 13) | (OP_OUTS << 8),
		pwrap_base + PMIC_WRAP_MAN_CMD);
	writel((OP_WR << 13) | (OP_OUTS << 8),
		pwrap_base + PMIC_WRAP_MAN_CMD);
	writel((OP_WR << 13) | (OP_OUTS << 8),
		pwrap_base + PMIC_WRAP_MAN_CMD);

	ret = wait_for_state_ready(wrp, is_sync_idle,
		TIMEOUT_WAIT_IDLE,
		pwrap_base + PMIC_WRAP_WACS2_RDATA, NULL, 0);
	if (ret)
		dev_err(&wrp->pdev->dev,
			"pwrap_reset_spislv fail, ret=%d\n", ret);

	writel(0, pwrap_base + PMIC_WRAP_MAN_EN);
	writel(0, pwrap_base + PMIC_WRAP_MUX_SEL);

	return ret;
}

static int pwrap_init_reg_clock(struct pmic_wrapper *wrp, u32 regck_sel)
{
	u32 wdata;
	u32 rdata;
	void __iomem *pwrap_base = wrp->pwrap_base;

	pwrap_read(wrp, PMIC_TOP_CKCON2, &rdata);

	if (regck_sel == 1)
		wdata = (rdata & (~(0x3 << 10))) | (0x1 << 10);
	else
		wdata = rdata & (~(0x3 << 10));

	pwrap_write(wrp, PMIC_TOP_CKCON2, wdata);
	pwrap_read(wrp, PMIC_TOP_CKCON2, &rdata);
	if (rdata != wdata) {
		dev_err(&wrp->pdev->dev,
			"PMIC_TOP_CKCON2 Write [15]=1 Fail, rdata=%x\n",
			rdata);
		return -EFAULT;
	}

	if (regck_sel == 1) {
		writel(0xc, pwrap_base + PMIC_WRAP_CSHEXT);
		writel(0x4, pwrap_base + PMIC_WRAP_CSHEXT_WRITE);
		writel(0xc, pwrap_base + PMIC_WRAP_CSHEXT_READ);
		writel(0x0, pwrap_base + PMIC_WRAP_CSLEXT_START);
		writel(0x0, pwrap_base + PMIC_WRAP_CSLEXT_END);
	} else if (regck_sel == 2) {
		writel(0x4, pwrap_base + PMIC_WRAP_CSHEXT);
		writel(0x0, pwrap_base + PMIC_WRAP_CSHEXT_WRITE);
		writel(0x4, pwrap_base + PMIC_WRAP_CSHEXT_READ);
		writel(0x0, pwrap_base + PMIC_WRAP_CSLEXT_START);
		writel(0x0, pwrap_base + PMIC_WRAP_CSLEXT_END);
	} else {
		writel(0xf, pwrap_base + PMIC_WRAP_CSHEXT);
		writel(0xf, pwrap_base + PMIC_WRAP_CSHEXT_WRITE);
		writel(0xf, pwrap_base + PMIC_WRAP_CSHEXT_READ);
		writel(0xf, pwrap_base + PMIC_WRAP_CSLEXT_START);
		writel(0xf, pwrap_base + PMIC_WRAP_CSLEXT_END);
	}

	return 0;
}

static irqreturn_t mt_pmic_wrap_interrupt(int irqno, void *dev_id)
{
	struct pmic_wrapper *wrp = dev_id;
	void __iomem *pwrap_base = wrp->pwrap_base;

	mutex_lock(&wrp->lock);
	writel(WACS2, pwrap_base + PMIC_WRAP_HARB_HPRIO);
	writel(0xffffffff, pwrap_base + PMIC_WRAP_INT_CLR);
	mutex_unlock(&wrp->lock);
	WARN_ON(1);

	return IRQ_HANDLED;
}

static int pwrap_reset(struct pmic_wrapper *wrp)
{
	struct reset_control *rstc_infracfg, *rstc_pericfg;

	rstc_infracfg = devm_reset_control_get(&wrp->pdev->dev, "infracfg");
	if (IS_ERR(rstc_infracfg)) {
		dev_err(&wrp->pdev->dev, "get infracfg reset failed\n");
		return PTR_ERR(rstc_infracfg);
	}
	rstc_pericfg = devm_reset_control_get(&wrp->pdev->dev, "pericfg");
	if (IS_ERR(rstc_pericfg)) {
		dev_err(&wrp->pdev->dev, "get pericfg reset failed\n");
		return PTR_ERR(rstc_pericfg);
	}

	reset_control_assert(rstc_infracfg);
	reset_control_assert(rstc_pericfg);
	reset_control_deassert(rstc_infracfg);
	reset_control_deassert(rstc_pericfg);

	return 0;
}

static int pwrap_init(struct pmic_wrapper *wrp)
{
	u32 rdata;
	void __iomem *pwrap_base = wrp->pwrap_base;
	void __iomem *pwrap_bridge_base = wrp->pwrap_bridge_base;

	if (pwrap_reset(wrp))
		return -EFAULT;

	/* Set SPI_CK freq = 26MHz */
	if (clk_set_parent(wrp->pmicspi, wrp->pmicspi_parent))
		return -EFAULT;

	/* Enable DCM */
	writel(1, pwrap_base + PMIC_WRAP_DCM_EN);
	writel(0, pwrap_base + PMIC_WRAP_DCM_DBC_PRD);

	/* Reset SPISLV */
	if (pwrap_reset_spislv(wrp) < 0)
		return -EFAULT;

	/* Enable WACS2 */
	writel(1, pwrap_base + PMIC_WRAP_WRAP_EN);
	writel(WACS2, pwrap_base + PMIC_WRAP_HIPRIO_ARB_EN);
	writel(1, pwrap_base + PMIC_WRAP_WACS2_EN);

	/* SIDLY setting */
	if (pwrap_init_sidly(wrp))
		return -EFAULT;

	/* SPI Waveform Configuration 0:safe mode, 1:18MHz, 2:26MHz */
	if (pwrap_init_reg_clock(wrp, 2))
		return -EFAULT;
	/*
	 * Enable PMIC
	 * (May not be necessary, depending on S/W partition)
	 * set dewrap clock bit  and clear dewrap reset bit
	 */
	if (pwrap_write(wrp, PMIC_WRP_CKPDN, 0) ||
		pwrap_write(wrp, PMIC_WRP_RST_CON, 0)) {
		dev_err(&wrp->pdev->dev, "Enable PMIC fail\n");
		return -EFAULT;
	}

	/* Enable DIO mode */
	if (pwrap_init_dio(wrp, 1))
		return -EFAULT;

	/* Enable Encryption */
	if (pwrap_init_cipher(wrp))
		return -EFAULT;

	/* Write test using WACS2 */
	if (pwrap_write(wrp, DEW_WRITE_TEST, WRITE_TEST_VALUE) ||
	    pwrap_read(wrp, DEW_WRITE_TEST, &rdata) ||
			(rdata != WRITE_TEST_VALUE)) {
		dev_err(&wrp->pdev->dev, "write test error,rdata=0x%04X,exp=0x%04X\n",
			rdata, WRITE_TEST_VALUE);
		return -EFAULT;
	}

	/* Signature Checking - Using CRC */
	if (pwrap_write(wrp, DEW_CRC_EN, 0x1))
		return -EFAULT;
	writel(0x1, pwrap_base + PMIC_WRAP_CRC_EN);
	writel(0x0, pwrap_base + PMIC_WRAP_SIG_MODE);
	writel(DEW_CRC_VAL, pwrap_base + PMIC_WRAP_SIG_ADR);

	/* PMIC_WRAP enables */
	writel(0x1ff, pwrap_base + PMIC_WRAP_HIPRIO_ARB_EN);
	writel(0x7, pwrap_base + PMIC_WRAP_RRARB_EN);
	writel(0x1, pwrap_base + PMIC_WRAP_WACS0_EN);
	writel(0x1, pwrap_base + PMIC_WRAP_WACS1_EN);
	writel(0x1, pwrap_base + PMIC_WRAP_WACS2_EN);
	writel(0x5, pwrap_base + PMIC_WRAP_STAUPD_PRD);
	writel(0xff, pwrap_base + PMIC_WRAP_STAUPD_GRPEN);
	writel(0xf, pwrap_base + PMIC_WRAP_WDT_UNIT);
	writel(0xffffffff, pwrap_base + PMIC_WRAP_WDT_SRC_EN);
	writel(0x1, pwrap_base + PMIC_WRAP_TIMER_EN);
	writel(~(PWRAP_INT_DEBUG | PWRAP_INT_SIG_ERR),
		pwrap_base + PMIC_WRAP_INT_EN);

	/*
	 * switch event pin from usbdl mode to normal mode for
	 * pmic interrupt, NEW@MT6397
	 */
	if (pwrap_read(wrp, PMIC_TOP_CKCON3, &rdata) ||
	    pwrap_write(wrp, PMIC_TOP_CKCON3, (rdata & 0x0007))) {
		dev_err(&wrp->pdev->dev, "switch event pin fail\n");
		return -EFAULT;
	}

	/* PMIC_WRAP enables for event  */
	writel(0x1, pwrap_base + PMIC_WRAP_EVENT_IN_EN);
	writel(0xffff, pwrap_base + PMIC_WRAP_EVENT_DST_EN);

	/* PERI_PWRAP_BRIDGE enables */
	writel(0x7f, pwrap_bridge_base + PERI_PWRAP_BRIDGE_IORD_ARB_EN);
	writel(0x1, pwrap_bridge_base + PERI_PWRAP_BRIDGE_WACS3_EN);
	writel(0x1, pwrap_bridge_base + PERI_PWRAP_BRIDGE_WACS4_EN);
	writel(0x1, pwrap_bridge_base + PERI_PWRAP_BRIDGE_WDT_UNIT);
	writel(0xffff, pwrap_bridge_base + PERI_PWRAP_BRIDGE_WDT_SRC_EN);
	writel(0x1, pwrap_bridge_base + PERI_PWRAP_BRIDGE_TIMER_EN);
	writel(0x7ff, pwrap_bridge_base + PERI_PWRAP_BRIDGE_INT_EN);

	/* PMIC_DEWRAP enables */
	if (pwrap_write(wrp, DEW_EVENT_OUT_EN, 0x1) ||
	    pwrap_write(wrp, DEW_EVENT_SRC_EN, 0xffff)) {
		dev_err(&wrp->pdev->dev, "enable dewrap fail\n");
		return -EFAULT;
	}

	writel(1, pwrap_base + PMIC_WRAP_INIT_DONE2);
	writel(1, pwrap_base + PMIC_WRAP_INIT_DONE0);
	writel(1, pwrap_base + PMIC_WRAP_INIT_DONE1);
	writel(1, pwrap_bridge_base + PERI_PWRAP_BRIDGE_INIT_DONE3);
	writel(1, pwrap_bridge_base + PERI_PWRAP_BRIDGE_INIT_DONE4);

	return 0;
}

static int mt_pwrap_check_and_init(struct pmic_wrapper *wrp)
{
	int ret;
	u32 rdata;
	void __iomem *pwrap_base = wrp->pwrap_base;

	rdata = readl(pwrap_base + PMIC_WRAP_INIT_DONE2);
	if (rdata)
		goto done;

	ret = pwrap_init(wrp);
	if (ret) {
		dev_err(&wrp->pdev->dev,
			"PMIC wrapper init error, ret=%d\n", ret);
		return ret;
	}

done:
	rdata = readl(pwrap_base + PMIC_WRAP_WACS2_RDATA);
	if (GET_INIT_DONE0(rdata) != WACS_INIT_DONE) {
		dev_err(&wrp->pdev->dev, "initialization isn't finished\n");
		return -ENODEV;
	}
	wrp->is_done = true;
	return 0;
}

static int mt_pmic_iomap_init(struct platform_device *pdev)
{
	struct resource *res;
	struct pmic_wrapper *wrp = platform_get_drvdata(pdev);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pwrap-base");
	if (!res) {
		dev_err(&pdev->dev, "could not get resource\n");
		return -ENODEV;
	}
	wrp->pwrap_base = devm_ioremap(&pdev->dev, res->start,
		resource_size(res));
	if (!wrp->pwrap_base) {
		dev_err(&pdev->dev, "could not get pwrap_base\n");
		return -ENOMEM;
	}
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
		"pwrap-bridge-base");
	if (!res) {
		dev_err(&pdev->dev, "could not get resource\n");
		return -ENODEV;
	}
	wrp->pwrap_bridge_base = devm_ioremap(&pdev->dev, res->start,
		resource_size(res));
	if (!wrp->pwrap_bridge_base) {
		dev_err(&pdev->dev, "could not get pwrap_bridge_base\n");
		return -ENOMEM;
	}

	return 0;
}

const struct regmap_config pwrap_regmap_config = {
	.reg_bits = 16,
	.val_bits = 16,
	.reg_read = pwrap_regmap_read,
	.reg_write = pwrap_regmap_write,
};

static int mt_pmic_wrap_probe(struct platform_device *pdev)
{
	int ret;
	struct pmic_wrapper *wrp;
	int irq;

	wrp = devm_kzalloc(&pdev->dev, sizeof(struct pmic_wrapper), GFP_KERNEL);
	if (!wrp) {
		dev_err(&pdev->dev, "Error: No memory\n");
		return -ENOMEM;
	}
	platform_set_drvdata(pdev, wrp);

	ret = mt_pmic_iomap_init(pdev);
	if (ret) {
		dev_err(&pdev->dev, "mt_pmic_iomap_init, ret=%d\n", ret);
		return ret;
	}

	mutex_init(&wrp->lock);
	wrp->pdev = pdev;
	wrp->pmicspi = devm_clk_get(&pdev->dev, "pmicspi-sel");
	if (IS_ERR(wrp->pmicspi)) {
		dev_err(&pdev->dev, "pmicspi-sel fail\n");
		return PTR_ERR(wrp->pmicspi);
	}
	wrp->pmicspi_parent = devm_clk_get(&pdev->dev, "pmicspi-parent");
	wrp->pmicspi = devm_clk_get(&pdev->dev, "pmicspi-sel");
	if (IS_ERR(wrp->pmicspi_parent)) {
		dev_err(&pdev->dev, "pmicspi-parent parient\n");
		return PTR_ERR(wrp->pmicspi_parent);
	}
	ret = clk_prepare_enable(wrp->pmicspi);
	if (ret) {
		dev_err(&pdev->dev, "pmicspi clock fail, ret=%d\n", ret);
		return ret;
	}

	if (mt_pwrap_check_and_init(wrp)) {
		dev_err(&pdev->dev, "PMIC wrapper HW init failed\n");
		ret = -EIO;
		return ret;
	}

	irq = platform_get_irq(pdev, 0);
	ret = request_threaded_irq(irq,
			mt_pmic_wrap_interrupt, NULL,
			IRQF_TRIGGER_HIGH, PMIC_WRAP_DEVICE, wrp);
	if (ret) {
		dev_err(&pdev->dev, "register IRQ failed, ret=%d\n", ret);
		return ret;
	}

	wrp->regmap = devm_regmap_init(&pdev->dev, NULL,
			wrp, &pwrap_regmap_config);
	if (IS_ERR(wrp->regmap)) {
		ret = PTR_ERR(wrp->regmap);
		dev_err(&pdev->dev, "Failed to allocate register map, ret=%d\n",
			ret);
		return ret;
	}

	ret = of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);
	if (ret) {
		dev_err(&pdev->dev, "%s fail to create devices.\n",
			pdev->dev.of_node->full_name);
		return ret;
	}

	return 0;
}

static struct of_device_id of_pmic_wrap_match_tbl[] = {
	{.compatible = "mediatek,mt8135-pwrap",},
	{}
};
MODULE_DEVICE_TABLE(of, of_pmic_wrap_match_tbl);

static struct platform_driver mt_pmic_wrap_drv = {
	.driver = {
		   .name = PMIC_WRAP_DEVICE,
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(of_pmic_wrap_match_tbl),
		   },
	.probe = mt_pmic_wrap_probe,
};

static int __init mt_pwrap_drv_init(void)
{
	return platform_driver_register(&mt_pmic_wrap_drv);
}

postcore_initcall(mt_pwrap_drv_init);

MODULE_AUTHOR("Flora Fu <flora.fu@mediatek.com>");
MODULE_DESCRIPTION("PMIC WRAPPER Driver");
MODULE_LICENSE("GPL");
