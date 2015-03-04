/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: James Liao <jamesjj.liao@mediatek.com>
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

#include <linux/of.h>
#include <linux/of_address.h>

#include <linux/io.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/clkdev.h>

#include "clk-mtk.h"
#include "clk-mt8173-pg.h"

#include <dt-bindings/clock/mt8173-clk.h>

#define	CHECK_PWR_ST	0

#ifndef GENMASK
#define GENMASK(h, l)	(((U32_C(1) << ((h) - (l) + 1)) - 1) << (l))
#endif

#define clk_readl(addr)		readl(addr)
#define clk_writel(val, addr)	\
	do { writel(val, addr); wmb(); } while (0)	/* sync_write */
#define clk_setl(mask, addr)	clk_writel(clk_readl(addr) | (mask), addr)
#define clk_clrl(mask, addr)	clk_writel(clk_readl(addr) & ~(mask), addr)

/*
 * MTCMOS
 */

#define STA_POWER_DOWN	0
#define STA_POWER_ON	1
#define PWR_DOWN	0
#define PWR_ON		1

struct subsys;

struct subsys_ops {
	int (*enable)(struct subsys *sys);
	int (*disable)(struct subsys *sys);
	int (*get_state)(struct subsys *sys);
};

struct subsys {
	const char		*name;
	uint32_t		sta_mask;
	void __iomem		*ctl_addr;
	uint32_t		sram_pdn_bits;
	uint32_t		sram_pdn_ack_bits;
	uint32_t		bus_prot_mask;
	spinlock_t		*lock;
	struct subsys_ops	*ops;
};

static struct subsys_ops general_sys_ops;

static void __iomem *infracfg_base;
static void __iomem *spm_base;

#define INFRACFG_REG(offset)		(infracfg_base + offset)
#define SPM_REG(offset)			(spm_base + offset)

#define SPM_PWR_STATUS			SPM_REG(0x060c)
#define SPM_PWR_STATUS_2ND		SPM_REG(0x0610)
#define SPM_VDE_PWR_CON			SPM_REG(0x0210)
#define SPM_MFG_PWR_CON			SPM_REG(0x0214)
#define SPM_VEN_PWR_CON			SPM_REG(0x0230)
#define SPM_ISP_PWR_CON			SPM_REG(0x0238)
#define SPM_DIS_PWR_CON			SPM_REG(0x023c)
#define SPM_VEN2_PWR_CON		SPM_REG(0x0298)
#define SPM_AUDIO_PWR_CON		SPM_REG(0x029c)
#define SPM_MFG_2D_PWR_CON		SPM_REG(0x02c0)
#define SPM_MFG_ASYNC_PWR_CON		SPM_REG(0x02c4)
#define SPM_USB_PWR_CON			SPM_REG(0x02cc)

#define INFRA_TOPAXI_PROTECTEN		INFRACFG_REG(0x0220)
#define INFRA_TOPAXI_PROTECTSTA1	INFRACFG_REG(0x0228)

#define SPM_PROJECT_CODE		0xb16

#define PWR_RST_B_BIT			BIT(0)
#define PWR_ISO_BIT			BIT(1)
#define PWR_ON_BIT			BIT(2)
#define PWR_ON_2ND_BIT			BIT(3)
#define PWR_CLK_DIS_BIT			BIT(4)

#define DIS_PWR_STA_MASK		BIT(3)
#define MFG_PWR_STA_MASK		BIT(4)
#define ISP_PWR_STA_MASK		BIT(5)
#define VDE_PWR_STA_MASK		BIT(7)
#define VEN2_PWR_STA_MASK		BIT(20)
#define VEN_PWR_STA_MASK		BIT(21)
#define MFG_2D_PWR_STA_MASK		BIT(22)
#define MFG_ASYNC_PWR_STA_MASK		BIT(23)
#define AUDIO_PWR_STA_MASK		BIT(24)
#define USB_PWR_STA_MASK		BIT(25)

#define DISP_PROT_MASK			(BIT(1) | BIT(2))
#define MFG_2D_PROT_MASK		(BIT(14) | GENMASK(23, 21))

static struct subsys syss[] =	/* NR_SYSS */
{
	[SYS_VDE] = {
		.name = __stringify(SYS_VDE),
		.sta_mask = VDE_PWR_STA_MASK,
		.ctl_addr = NULL, /* SPM_VDE_PWR_CON, */
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12), /* GENMASK(15, 12), */
		.bus_prot_mask = 0,
		.ops = &general_sys_ops,
	},
	[SYS_MFG] = {
		.name = __stringify(SYS_MFG),
		.sta_mask = MFG_PWR_STA_MASK,
		.ctl_addr = NULL, /* SPM_MFG_PWR_CON, */
		.sram_pdn_bits = GENMASK(13, 8),
		.sram_pdn_ack_bits = GENMASK(21, 16),
		.bus_prot_mask = MFG_2D_PROT_MASK,
		.ops = &general_sys_ops,
	},
	[SYS_VEN] = {
		.name = __stringify(SYS_VEN),
		.sta_mask = VEN_PWR_STA_MASK,
		.ctl_addr = NULL, /* SPM_VEN_PWR_CON, */
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
		.bus_prot_mask = 0,
		.ops = &general_sys_ops,
	},
	[SYS_ISP] = {
		.name = __stringify(SYS_ISP),
		.sta_mask = ISP_PWR_STA_MASK,
		.ctl_addr = NULL, /* SPM_ISP_PWR_CON, */
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(13, 12), /* GENMASK(15, 12), */
		.bus_prot_mask = 0,
		.ops = &general_sys_ops,
	},
	[SYS_DIS] = {
		.name = __stringify(SYS_DIS),
		.sta_mask = DIS_PWR_STA_MASK,
		.ctl_addr = NULL, /* SPM_DIS_PWR_CON, */
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12), /* GENMASK(15, 12), */
		.bus_prot_mask = DISP_PROT_MASK,
		.ops = &general_sys_ops,
	},
	[SYS_VEN2] = {
		.name = __stringify(SYS_VEN2),
		.sta_mask = VEN2_PWR_STA_MASK,
		.ctl_addr = NULL, /* SPM_VEN2_PWR_CON, */
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
		.bus_prot_mask = 0,
		.ops = &general_sys_ops,
	},
	[SYS_AUDIO] = {
		.name = __stringify(SYS_AUDIO),
		.sta_mask = AUDIO_PWR_STA_MASK,
		.ctl_addr = NULL, /* SPM_AUDIO_PWR_CON, */
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
		.bus_prot_mask = 0,
		.ops = &general_sys_ops,
	},
	[SYS_MFG_2D] = {
		.name = __stringify(SYS_MFG_2D),
		.sta_mask = MFG_2D_PWR_STA_MASK,
		.ctl_addr = NULL, /* SPM_MFG_2D_PWR_CON, */
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(13, 12), /* GENMASK(15, 12), */
		.bus_prot_mask = 0,
		.ops = &general_sys_ops,
	},
	[SYS_MFG_ASYNC] = {
		.name = __stringify(SYS_MFG_ASYNC),
		.sta_mask = MFG_ASYNC_PWR_STA_MASK,
		.ctl_addr = NULL, /* SPM_MFG_ASYNC_PWR_CON, */
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = 0, /* GENMASK(15, 12), */
		.bus_prot_mask = 0,
		.ops = &general_sys_ops,
	},
	[SYS_USB] = {
		.name = __stringify(SYS_USB),
		.sta_mask = USB_PWR_STA_MASK,
		.ctl_addr = NULL, /* SPM_USB_PWR_CON, */
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
		.bus_prot_mask = 0,
		.ops = &general_sys_ops,
	},
};

static struct pg_callbacks *g_pgcb;

struct pg_callbacks *register_pg_callback(struct pg_callbacks *pgcb)
{
	struct pg_callbacks *old_pgcb = g_pgcb;

	g_pgcb = pgcb;
	return old_pgcb;
}

static struct subsys *id_to_sys(unsigned int id)
{
	return id < NR_SYSS ? &syss[id] : NULL;
}

static void set_bus_protect(int en, uint32_t mask, unsigned long expired)
{
	if (!mask)
		return;

	if (en) {
		clk_setl(mask, INFRA_TOPAXI_PROTECTEN);

		while ((clk_readl(INFRA_TOPAXI_PROTECTSTA1) & mask) != mask) {
			if (time_after(jiffies, expired)) {
				WARN_ON(1);
				break;
			}
		}
	} else {
		clk_clrl(mask, INFRA_TOPAXI_PROTECTEN);

		while (clk_readl(INFRA_TOPAXI_PROTECTSTA1) & mask) {
			if (time_after(jiffies, expired)) {
				WARN_ON(1);
				break;
			}
		}
	}
}

static int spm_mtcmos_power_off_general_locked(struct subsys *sys,
		int wait_power_ack, int ext_pwr_delay)
{
	unsigned long expired = jiffies + HZ / 10;
	void __iomem *ctl_addr = sys->ctl_addr;

	uint32_t sram_pdn_ack = sys->sram_pdn_ack_bits;

	pr_debug("%s(): sys: %s\n", __func__, sys->name);

	/* BUS_PROTECT */
	set_bus_protect(1, sys->bus_prot_mask, expired);

	/* SRAM_PDN */
	clk_setl(sys->sram_pdn_bits, ctl_addr);

	/* wait until SRAM_PDN_ACK all 1 */
	while (((clk_readl(ctl_addr) & sram_pdn_ack) != sram_pdn_ack)) {
		if (time_after(jiffies, expired)) {
			WARN_ON(1);
			break;
		}
	}

	clk_setl(PWR_ISO_BIT, ctl_addr);
	clk_clrl(PWR_RST_B_BIT, ctl_addr);
	clk_setl(PWR_CLK_DIS_BIT, ctl_addr);

	clk_clrl(PWR_ON_BIT, ctl_addr);
	clk_clrl(PWR_ON_2ND_BIT, ctl_addr);

	/* extra delay after power off */
	if (ext_pwr_delay > 0)
		udelay(ext_pwr_delay);

	if (wait_power_ack) {
		/* wait until PWR_ACK = 0 */
		while ((clk_readl(SPM_PWR_STATUS) & sys->sta_mask)
			|| (clk_readl(SPM_PWR_STATUS_2ND) & sys->sta_mask)) {
			if (time_after(jiffies, expired)) {
				WARN_ON(1);
				break;
			}
		}
	}

	return 0;
}

static int spm_mtcmos_power_on_general_locked(
		struct subsys *sys, int wait_power_ack, int ext_pwr_delay)
{
	unsigned long expired = jiffies + HZ / 10;
	void __iomem *ctl_addr = sys->ctl_addr;

	uint32_t sram_pdn_ack = sys->sram_pdn_ack_bits;

	pr_debug("%s(): sys: %s\n", __func__, sys->name);

	clk_setl(PWR_ON_BIT, ctl_addr);
	clk_setl(PWR_ON_2ND_BIT, ctl_addr);

	/* extra delay after power on */
	if (ext_pwr_delay > 0)
		udelay(ext_pwr_delay);

	if (wait_power_ack) {
		/* wait until PWR_ACK = 1 */
		while (!(clk_readl(SPM_PWR_STATUS) & sys->sta_mask)
			|| !(clk_readl(SPM_PWR_STATUS_2ND) & sys->sta_mask)) {
			if (time_after(jiffies, expired)) {
				WARN_ON(1);
				break;
			}
		}
	}

	clk_clrl(PWR_CLK_DIS_BIT, ctl_addr);
	clk_clrl(PWR_ISO_BIT, ctl_addr);
	clk_setl(PWR_RST_B_BIT, ctl_addr);

	/* SRAM_PDN */
	clk_clrl(sys->sram_pdn_bits, ctl_addr);

	/* wait until SRAM_PDN_ACK all 0 */
	while (sram_pdn_ack && (clk_readl(ctl_addr) & sram_pdn_ack)) {
		if (time_after(jiffies, expired)) {
			WARN_ON(1);
			break;
		}
	}

	/* BUS_PROTECT */
	set_bus_protect(0, sys->bus_prot_mask, expired);

	return 0;
}

static int general_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_power_on_general_locked(sys, 1, 0);
}

static int general_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_power_off_general_locked(sys, 1, 0);
}

static int sys_get_state_op(struct subsys *sys)
{
	unsigned int sta = clk_readl(SPM_PWR_STATUS);
	unsigned int sta_s = clk_readl(SPM_PWR_STATUS_2ND);

	return (sta & sys->sta_mask) && (sta_s & sys->sta_mask);
}

static struct subsys_ops general_sys_ops = {
	.enable = general_sys_enable_op,
	.disable = general_sys_disable_op,
	.get_state = sys_get_state_op,
};

static int subsys_is_on(enum subsys_id id)
{
	struct subsys *sys = id_to_sys(id);

	BUG_ON(!sys);

	return sys->ops->get_state(sys);
}

static int enable_subsys(enum subsys_id id)
{
	int r;
	unsigned long flags;
	struct subsys *sys = id_to_sys(id);

	BUG_ON(!sys);

	spin_lock_irqsave(sys->lock, flags);

#if CHECK_PWR_ST
	if (sys->ops->get_state(sys) == PWR_ON) {
		mtk_clk_unlock(flags);
		return 0;
	}
#endif /* CHECK_PWR_ST */

	r = sys->ops->enable(sys);
	WARN_ON(r);

	spin_unlock_irqrestore(sys->lock, flags);

	if (g_pgcb && g_pgcb->after_on)
		g_pgcb->after_on(id);

	return r;
}

static int disable_subsys(enum subsys_id id)
{
	int r;
	unsigned long flags;
	struct subsys *sys = id_to_sys(id);

	BUG_ON(!sys);

	/* TODO: check all clocks related to this subsys are off */
	/* could be power off or not */

	if (g_pgcb && g_pgcb->before_off)
		g_pgcb->before_off(id);

	spin_lock_irqsave(sys->lock, flags);

#if CHECK_PWR_ST
	if (sys->ops->get_state(sys) == PWR_DOWN) {
		mtk_clk_unlock(flags);
		return 0;
	}
#endif /* CHECK_PWR_ST */

	r = sys->ops->disable(sys);
	WARN_ON(r);

	spin_unlock_irqrestore(sys->lock, flags);

	return r;
}

/*
 * power_gate
 */

struct mt_power_gate {
	struct clk_hw	hw;
	struct clk	*pre_clk;
	enum subsys_id	pd_id;
};

#define to_power_gate(_hw) container_of(_hw, struct mt_power_gate, hw)

static int pg_enable(struct clk_hw *hw)
{
	struct mt_power_gate *pg = to_power_gate(hw);

	pr_debug("%s(): %s, id: %u\n",
		__func__, __clk_get_name(hw->clk), pg->pd_id);

	return enable_subsys(pg->pd_id);
}

static void pg_disable(struct clk_hw *hw)
{
	struct mt_power_gate *pg = to_power_gate(hw);

	pr_debug("%s(): %s, id: %u\n",
		__func__, __clk_get_name(hw->clk), pg->pd_id);

	disable_subsys(pg->pd_id);
}

static int pg_is_enabled(struct clk_hw *hw)
{
	struct mt_power_gate *pg = to_power_gate(hw);

	return subsys_is_on(pg->pd_id);
}

int pg_prepare(struct clk_hw *hw)
{
	int r;
	struct mt_power_gate *pg = to_power_gate(hw);

	pr_debug("%s(): %s, pre_clk: %s\n", __func__, __clk_get_name(hw->clk),
		pg->pre_clk ? __clk_get_name(pg->pre_clk) : "");

	if (pg->pre_clk) {
		r = clk_prepare_enable(pg->pre_clk);
		if (r)
			return r;
	}

	return pg_enable(hw);

}

void pg_unprepare(struct clk_hw *hw)
{
	struct mt_power_gate *pg = to_power_gate(hw);

	pr_debug("%s(): %s, pre_clk: %s\n", __func__, __clk_get_name(hw->clk),
		pg->pre_clk ? __clk_get_name(pg->pre_clk) : "");

	pg_disable(hw);

	if (pg->pre_clk)
		clk_disable_unprepare(pg->pre_clk);
}

static const struct clk_ops mt_power_gate_ops = {
	.prepare	= pg_prepare,
	.unprepare	= pg_unprepare,
	.is_enabled	= pg_is_enabled,
};

struct clk *mt_clk_register_power_gate(
		const char *name,
		const char *parent_name,
		struct clk *pre_clk,
		enum subsys_id pd_id)
{
	struct mt_power_gate *pg;
	struct clk *clk;
	struct clk_init_data init;

	pg = kzalloc(sizeof(*pg), GFP_KERNEL);
	if (!pg)
		return ERR_PTR(-ENOMEM);

	init.name = name;
	init.flags = CLK_IGNORE_UNUSED;
	init.parent_names = parent_name ? &parent_name : NULL;
	init.num_parents = parent_name ? 1 : 0;
	init.ops = &mt_power_gate_ops;

	pg->pre_clk = pre_clk;
	pg->pd_id = pd_id;
	pg->hw.init = &init;

	clk = clk_register(NULL, &pg->hw);
	if (IS_ERR(clk))
		kfree(pg);

	return clk;
}

#define pg_vde			"pg_vde"
#define pg_mfg			"pg_mfg"
#define pg_ven			"pg_ven"
#define pg_isp			"pg_isp"
#define pg_dis			"pg_dis"
#define pg_ven2			"pg_ven2"
#define pg_audio		"pg_audio"
#define pg_mfg_2d		"pg_mfg_2d"
#define pg_mfg_async		"pg_mfg_async"
#define pg_usb			"pg_usb"

#define mm_sel			"mm_sel"
#define vdec_sel		"vdec_sel"
#define venc_sel		"venc_sel"
#define mfg_sel			"mfg_sel"
#define aud_intbus_sel		"aud_intbus_sel"
#define venc_lt_sel		"venc_lt_sel"


struct mtk_power_gate {
	int id;
	const char *name;
	const char *parent_name;
	const char *pre_clk_name;
	enum subsys_id pd_id;
};

#define PGATE(_id, _name, _parent, _pre_clk, _pd_id) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.pre_clk_name = _pre_clk,		\
		.pd_id = _pd_id,			\
	}

struct mtk_power_gate scp_clks[] __initdata = {
	PGATE(SCP_SYS_VDE, pg_vde, NULL, vdec_sel, SYS_VDE),
	PGATE(SCP_SYS_MFG, pg_mfg, pg_mfg_2d, mfg_sel, SYS_MFG),
	PGATE(SCP_SYS_VEN, pg_ven, NULL, venc_sel, SYS_VEN),
	PGATE(SCP_SYS_ISP, pg_isp, NULL, NULL, SYS_ISP),
	PGATE(SCP_SYS_DIS, pg_dis, NULL, mm_sel, SYS_DIS),
	PGATE(SCP_SYS_VEN2, pg_ven2, NULL, venc_lt_sel, SYS_VEN2),
	PGATE(SCP_SYS_AUDIO, pg_audio, NULL, NULL, SYS_AUDIO),
	PGATE(SCP_SYS_MFG_2D, pg_mfg_2d, pg_mfg_async, NULL, SYS_MFG_2D),
	PGATE(SCP_SYS_MFG_ASYNC, pg_mfg_async, NULL, NULL, SYS_MFG_ASYNC),
	PGATE(SCP_SYS_USB, pg_usb, NULL, NULL, SYS_USB),
};

void init_clk_scpsys(
		void __iomem *infracfg_reg,
		void __iomem *spm_reg,
		struct clk_onecell_data *clk_data, spinlock_t *lock)
{
	int i;
	struct clk *clk;
	struct clk *pre_clk;

	infracfg_base = infracfg_reg;
	spm_base = spm_reg;

	syss[SYS_VDE].ctl_addr = SPM_VDE_PWR_CON;
	syss[SYS_MFG].ctl_addr = SPM_MFG_PWR_CON;
	syss[SYS_VEN].ctl_addr = SPM_VEN_PWR_CON;
	syss[SYS_ISP].ctl_addr = SPM_ISP_PWR_CON;
	syss[SYS_DIS].ctl_addr = SPM_DIS_PWR_CON;
	syss[SYS_VEN2].ctl_addr = SPM_VEN2_PWR_CON;
	syss[SYS_AUDIO].ctl_addr = SPM_AUDIO_PWR_CON;
	syss[SYS_MFG_2D].ctl_addr = SPM_MFG_2D_PWR_CON;
	syss[SYS_MFG_ASYNC].ctl_addr = SPM_MFG_ASYNC_PWR_CON;
	syss[SYS_USB].ctl_addr = SPM_USB_PWR_CON;

	syss[SYS_VDE].lock = lock;
	syss[SYS_MFG].lock = lock;
	syss[SYS_VEN].lock = lock;
	syss[SYS_ISP].lock = lock;
	syss[SYS_DIS].lock = lock;
	syss[SYS_VEN2].lock = lock;
	syss[SYS_AUDIO].lock = lock;
	syss[SYS_MFG_2D].lock = lock;
	syss[SYS_MFG_ASYNC].lock = lock;
	syss[SYS_USB].lock = lock;

	for (i = 0; i < ARRAY_SIZE(scp_clks); i++) {
		struct mtk_power_gate *pg = &scp_clks[i];

		pre_clk = pg->pre_clk_name ?
				__clk_lookup(pg->pre_clk_name) : NULL;

		clk = mt_clk_register_power_gate(pg->name, pg->parent_name,
				pre_clk, pg->pd_id);

		if (IS_ERR(clk)) {
			pr_err("Failed to register clk %s: %ld\n",
					pg->name, PTR_ERR(clk));
			continue;
		}

		if (clk_data)
			clk_data->clks[pg->id] = clk;
	}
}
