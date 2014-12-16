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

#include "clk/clk-mtk.h"

#define ENABLE_DEBUG 0

#if ENABLE_DEBUG
#define PRDPF(fmt, args...)     pr_info("[MFG]" fmt, ##args)
#else
#define PRDPF(fmt, args...) 
#endif

/*
 * MTCMOS
 */

#define STA_POWER_DOWN	0
#define STA_POWER_ON	1
#define PWR_DOWN	0
#define PWR_ON		1

struct subsys;

struct subsys_ops {
	int (*enable) (struct subsys *sys);
	int (*disable) (struct subsys *sys);
	int (*get_state) (struct subsys *sys);
};

struct subsys {
	const char		*name;
	uint32_t		sta_mask;
	void __iomem		*ctl_addr;
	uint32_t		sram_pdn_bits;
	uint32_t		sram_pdn_ack_bits;
	uint32_t		bus_prot_mask;
	uint32_t		si0_way_en_mask;
	struct subsys_ops	*ops;
};

static struct subsys_ops general_sys_ops;
static struct subsys_ops dis_sys_ops;
static struct subsys_ops mfg_sys_ops;
static struct subsys_ops isp_sys_ops;
static struct subsys_ops ven_sys_ops;
static struct subsys_ops vde_sys_ops;

#if CLK_DEBUG && 0

static void __iomem *infracfg_base	= ((void __iomem *)0xF0001000);
static void __iomem *spm_base		= ((void __iomem *)0xF0006000);
static void __iomem *mfg_config_base	= ((void __iomem *)0xF0206000);

#else

static void __iomem *infracfg_base;
static void __iomem *spm_base;
static void __iomem *mfg_config_base;

#endif

#define INFRACFG_REG(offset)		(infracfg_base + offset)
#define SPM_REG(offset)			(spm_base + offset)
#define MFG_CONFIG_REG(offset)		(mfg_config_base + offset)

#define SPM_POWERON_CONFIG_SET		SPM_REG(0x0000)
#define SPM_PWR_STATUS			SPM_REG(0x060c)
#define SPM_PWR_STATUS_S		SPM_REG(0x0610)
#define SPM_VDE_PWR_CON			SPM_REG(0x0210)
#define SPM_MFG_PWR_CON			SPM_REG(0x0214)
#define SPM_VEN_PWR_CON			SPM_REG(0x0230)
#define SPM_IFR_PWR_CON			SPM_REG(0x0234)
#define SPM_ISP_PWR_CON			SPM_REG(0x0238)
#define SPM_DIS_PWR_CON			SPM_REG(0x023c)
#define SPM_MFG_2D_PWR_CON		SPM_REG(0x02b8)

#define TOPAXI_SI0_CTL			INFRACFG_REG(0x0200)
#define INFRA_TOPAXI_PROTECTEN		INFRACFG_REG(0x0220)
#define INFRA_TOPAXI_PROTECTSTA1	INFRACFG_REG(0x0228)

#define SPM_PROJECT_CODE		0xb16

#define PWR_RST_B_BIT			BIT(0)
#define PWR_ISO_BIT			BIT(1)
#define PWR_ON_BIT			BIT(2)
#define PWR_ON_S_BIT			BIT(3)
#define PWR_CLK_DIS_BIT			BIT(4)

#define DISP_PWR_STA_MASK		BIT(3)
#define MFG_3D_PWR_STA_MASK		BIT(4)
#define ISP_PWR_STA_MASK		BIT(5)
#define INFRA_PWR_STA_MASK		BIT(6)
#define VDEC_PWR_STA_MASK		BIT(7)
#define VEN_PWR_STA_MASK		BIT(14)
#define MFG_2D_PWR_STA_MASK		BIT(22)

static struct subsys syss[] =	/* NR_SYSS */
{
	[SYS_VDE] = {
		.name = __stringify(SYS_VDE),
		.sta_mask = VDEC_PWR_STA_MASK,
		.ctl_addr = NULL, /* SPM_VDE_PWR_CON, */
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bus_prot_mask = 0,
		.si0_way_en_mask = 0,
		.ops = &vde_sys_ops,
	},
	[SYS_MFG_2D] = {
		.name = __stringify(SYS_MFG_2D),
		.sta_mask = MFG_2D_PWR_STA_MASK,
		.ctl_addr = NULL, /* SPM_MFG_2D_PWR_CON, */
		.sram_pdn_bits = GENMASK(9, 8),
		.sram_pdn_ack_bits = GENMASK(21, 20),
		.bus_prot_mask = BIT(5) | BIT(8) | BIT(9) | BIT(14),
		.si0_way_en_mask = BIT(10),
		.ops = &mfg_sys_ops,
	},
	[SYS_MFG] = {
		.name = __stringify(SYS_MFG),
		.sta_mask = MFG_3D_PWR_STA_MASK,
		.ctl_addr = NULL, /* SPM_MFG_PWR_CON, */
		.sram_pdn_bits = GENMASK(13, 8),
		.sram_pdn_ack_bits = GENMASK(25, 20),
		.bus_prot_mask = 0,
		.si0_way_en_mask = 0,
		.ops = &mfg_sys_ops,
	},
	[SYS_VEN] = {
		.name = __stringify(SYS_VEN),
		.sta_mask = VEN_PWR_STA_MASK,
		.ctl_addr = NULL, /* SPM_VEN_PWR_CON, */
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
		.bus_prot_mask = 0,
		.si0_way_en_mask = 0,
		.ops = &ven_sys_ops,
	},
	[SYS_ISP] = {
		.name = __stringify(SYS_ISP),
		.sta_mask = ISP_PWR_STA_MASK,
		.ctl_addr = NULL, /* SPM_ISP_PWR_CON, */
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(13, 12),
		.bus_prot_mask = 0,
		.si0_way_en_mask = 0,
		.ops = &isp_sys_ops,
	},
	[SYS_DIS] = {
		.name = __stringify(SYS_DIS),
		.sta_mask = DISP_PWR_STA_MASK,
		.ctl_addr = NULL, /* SPM_DIS_PWR_CON, */
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = 0,
		.bus_prot_mask = 0,
		.si0_way_en_mask = 0,
		.ops = &dis_sys_ops,
	},
	[SYS_IFR] = {
		.name = __stringify(SYS_IFR),
		.sta_mask = INFRA_PWR_STA_MASK,
		.ctl_addr = NULL, /* SPM_IFR_PWR_CON, */
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
		.bus_prot_mask = 0,
		.si0_way_en_mask = 0,
		.ops = &general_sys_ops,
	},
};

static struct subsys *id_to_sys(unsigned int id)
{
	return id < NR_SYSS ? &syss[id] : NULL;
}

static int spm_mtcmos_power_off_general_locked(struct subsys *sys,
		int wait_power_ack, int ext_power_delay)
{
	int i;
	int err = 0;
	unsigned long expired = jiffies + HZ / 10;

	PRDPF("+spm_mtcmos_power_off_general_locked\n");

	clk_writel((SPM_PROJECT_CODE << 16) | BIT(0), SPM_POWERON_CONFIG_SET);

	/* BUS_PROTECT */
	if (0 != sys->bus_prot_mask) {
		void __iomem *psta = INFRA_TOPAXI_PROTECTSTA1;
		/* enable BMEM_ASYNC_FREE_RUN  */
		clk_setl(BIT(29), MFG_CONFIG_REG(0x0010));

		clk_setl(sys->bus_prot_mask, INFRA_TOPAXI_PROTECTEN);
		PRDPF("+.1spm_mtcmos_power_off_general_locked\n");
		while ((clk_readl(psta) & sys->bus_prot_mask) !=
			sys->bus_prot_mask) {
			if (time_after(jiffies, expired)) {
				WARN_ON(1);
				break;
			}
		}
		PRDPF("-.1spm_mtcmos_power_off_general_locked\n");

		/* disable BMEM_ASYNC_FREE_RUN */
		clk_clrl(BIT(29), MFG_CONFIG_REG(0x0010));
	}

	if (0 != sys->si0_way_en_mask)
		clk_clrl(sys->si0_way_en_mask, TOPAXI_SI0_CTL);

	/* SRAM_PDN */
	for (i = BIT(8); i <= sys->sram_pdn_bits; i = (i << 1) + BIT(8))
		clk_writel(clk_readl(sys->ctl_addr) | i, sys->ctl_addr);

	/* wait until SRAM_PDN_ACK all 1 */
	PRDPF("+.2spm_mtcmos_power_off_general_locked\n");
	while (sys->sram_pdn_ack_bits &&
		((clk_readl(sys->ctl_addr) & sys->sram_pdn_ack_bits) !=
			sys->sram_pdn_ack_bits)) {
		if (time_after(jiffies, expired)) {
			WARN_ON(1);
			break;
		}
	}
	PRDPF("-.2spm_mtcmos_power_off_general_locked\n");

	clk_writel(clk_readl(sys->ctl_addr) | PWR_ISO_BIT, sys->ctl_addr);
	clk_writel(clk_readl(sys->ctl_addr) & ~PWR_RST_B_BIT, sys->ctl_addr);
	clk_writel(clk_readl(sys->ctl_addr) | PWR_CLK_DIS_BIT, sys->ctl_addr);

	clk_writel(clk_readl(sys->ctl_addr) & ~(PWR_ON_BIT), sys->ctl_addr);
	udelay(1);		/* delay 1 us */
	clk_writel(clk_readl(sys->ctl_addr) & ~(PWR_ON_S_BIT), sys->ctl_addr);
	udelay(1);		/* delay 1 us */

	/* extra delay after power off */
	if (ext_power_delay > 0)
		udelay(ext_power_delay);

	if (wait_power_ack) {
		/* wait until PWR_ACK = 0 */
		PRDPF("+.3spm_mtcmos_power_off_general_locked\n");
		while ((clk_readl(SPM_PWR_STATUS) & sys->sta_mask)
		       || (clk_readl(SPM_PWR_STATUS_S) & sys->sta_mask)) {
			if (time_after(jiffies, expired)) {
				WARN_ON(1);
				break;
			}
		}
		PRDPF("-.3spm_mtcmos_power_off_general_locked\n");
	}
	PRDPF("-spm_mtcmos_power_off_general_locked\n");

	return err;
}

static int spm_mtcmos_power_on_general_locked(
		struct subsys *sys, int wait_power_ack, int ext_power_delay)
{
	int i;
	int err = 0;
	unsigned long expired = jiffies + HZ / 10;

	PRDPF("+spm_mtcmos_power_on_general_locked\n");

	clk_writel((SPM_PROJECT_CODE << 16) | BIT(0), SPM_POWERON_CONFIG_SET);

	clk_writel(clk_readl(sys->ctl_addr) | PWR_ON_BIT, sys->ctl_addr);
	udelay(1);		/* delay 1 us */
	clk_writel(clk_readl(sys->ctl_addr) | PWR_ON_S_BIT, sys->ctl_addr);
	udelay(3);		/* delay 3 us */

	/* extra delay after power on */
	if (ext_power_delay > 0)
		udelay(ext_power_delay);

	if (wait_power_ack) {
		/* wait until PWR_ACK = 1 */
		PRDPF("+.1spm_mtcmos_power_on_general_locked\n");
		while (!(clk_readl(SPM_PWR_STATUS) & sys->sta_mask)
		       || !(clk_readl(SPM_PWR_STATUS_S) & sys->sta_mask)) {
			if (time_after(jiffies, expired)) {
				WARN_ON(1);
				break;
			}
		}
		PRDPF("-.1spm_mtcmos_power_on_general_locked\n");
	}

	clk_writel(clk_readl(sys->ctl_addr) & ~PWR_CLK_DIS_BIT, sys->ctl_addr);
	clk_writel(clk_readl(sys->ctl_addr) & ~PWR_ISO_BIT, sys->ctl_addr);
	clk_writel(clk_readl(sys->ctl_addr) | PWR_RST_B_BIT, sys->ctl_addr);

	PRDPF("+.2spm_mtcmos_power_on_general_locked\n");
	/* SRAM_PDN */
	for (i = BIT(8); i <= sys->sram_pdn_bits; i = (i << 1) + BIT(8))
		clk_writel(clk_readl(sys->ctl_addr) & ~i, sys->ctl_addr);
	PRDPF("-.2spm_mtcmos_power_on_general_locked\n");

	PRDPF("+.3spm_mtcmos_power_on_general_locked\n");
	/* wait until SRAM_PDN_ACK all 0 */
	while (sys->sram_pdn_ack_bits &&
		(clk_readl(sys->ctl_addr) & sys->sram_pdn_ack_bits)) {
		if (time_after(jiffies, expired)) {
			WARN_ON(1);
			break;
		}
	}
	PRDPF("-.3spm_mtcmos_power_on_general_locked\n");

	/* BUS_PROTECT */
	if (0 != sys->bus_prot_mask) {
		void __iomem *psta = INFRA_TOPAXI_PROTECTSTA1;
		clk_setl(BIT(29), MFG_CONFIG_REG(0x0010));

		clk_clrl(sys->bus_prot_mask, INFRA_TOPAXI_PROTECTEN);
		PRDPF("+.4spm_mtcmos_power_on_general_locked\n");
		while (clk_readl(psta) & sys->bus_prot_mask) {
			if (time_after(jiffies, expired)) {
				WARN_ON(1);
				break;
			}
		}
		PRDPF("-.4spm_mtcmos_power_on_general_locked\n");

		clk_clrl(BIT(29), MFG_CONFIG_REG(0x0010));
	}

	if (0 != sys->si0_way_en_mask)
		clk_setl(sys->si0_way_en_mask, TOPAXI_SI0_CTL);
	PRDPF("-spm_mtcmos_power_on_general_locked\n");

	return err;
}

static int spm_mtcmos_ctrl_general_locked(struct subsys *sys, int state)
{
	int err;
	PRDPF("+spm_mtcmos_ctrl_general_locked\n");

	if (state == STA_POWER_DOWN)
		err = spm_mtcmos_power_off_general_locked(sys, 1, 0);
	else
		err = spm_mtcmos_power_on_general_locked(sys, 1, 0);

	PRDPF("-spm_mtcmos_ctrl_general_locked\n");
	return err;
}

static int spm_mtcmos_ctrl_mfg_locked(struct subsys *sys, int state)
{
	int err;
	PRDPF("+spm_mtcmos_ctrl_mfg_locked\n");

	if (state == STA_POWER_DOWN)
		err = spm_mtcmos_power_off_general_locked(sys, 1, 10);
	else
		err = spm_mtcmos_power_on_general_locked(sys, 0, 10);

	PRDPF("-spm_mtcmos_ctrl_mfg_locked\n");
	return err;
}

static int general_sys_enable_op(struct subsys *sys)
{
	PRDPF("+general_sys_enable_op\n");
	return spm_mtcmos_ctrl_general_locked(sys, STA_POWER_ON);
}

static int general_sys_disable_op(struct subsys *sys)
{
	PRDPF("+general_sys_disable_op\n");
	return spm_mtcmos_ctrl_general_locked(sys, STA_POWER_DOWN);
}

static int mfg_sys_enable_op(struct subsys *sys)
{
	PRDPF("+mfg_sys_enable_op\n");
	return spm_mtcmos_ctrl_mfg_locked(sys, STA_POWER_ON);
}

static int mfg_sys_disable_op(struct subsys *sys)
{
	PRDPF("+mfg_sys_disable_op\n");
	return spm_mtcmos_ctrl_mfg_locked(sys, STA_POWER_DOWN);
}

static int dis_sys_enable_op(struct subsys *sys)
{
	int err;
	PRDPF("+dis_sys_enable_op\n");
	err = general_sys_enable_op(sys);

#if 0
	/* disable ROT clock after MTCMOS power on */
	clk_writel(DISP_ROT_DISP_BIT, DISP_CG_SET0);
#endif
	PRDPF("-dis_sys_enable_op\n");
	return err;
}

static int dis_sys_disable_op(struct subsys *sys)
{
	PRDPF("+dis_sys_disable_op\n");
	return general_sys_disable_op(sys);
}

static int isp_sys_enable_op(struct subsys *sys)
{
	PRDPF("+isp_sys_enable_op\n");
	return general_sys_enable_op(sys);
}

static int isp_sys_disable_op(struct subsys *sys)
{
	PRDPF("+isp_sys_disable_op\n");
	return general_sys_disable_op(sys);
}

static int ven_sys_enable_op(struct subsys *sys)
{
	PRDPF("+ven_sys_enable_op\n");
	return general_sys_enable_op(sys);
}

static int ven_sys_disable_op(struct subsys *sys)
{
	PRDPF("+ven_sys_disable_op\n");
	return general_sys_disable_op(sys);
}

static int vde_sys_enable_op(struct subsys *sys)
{
	PRDPF("+vde_sys_enable_op\n");
	return general_sys_enable_op(sys);
}

static int vde_sys_disable_op(struct subsys *sys)
{
	PRDPF("+vde_sys_disable_op\n");
	return general_sys_disable_op(sys);
}

static int sys_get_state_op(struct subsys *sys)
{
	unsigned int sta = clk_readl(SPM_PWR_STATUS);
	unsigned int sta_s = clk_readl(SPM_PWR_STATUS_S);
	PRDPF("+sys_get_state_op\n");
	return (sta & sys->sta_mask) && (sta_s & sys->sta_mask);
}

static struct subsys_ops general_sys_ops = {
	.enable = general_sys_enable_op,
	.disable = general_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops mfg_sys_ops = {
	.enable = mfg_sys_enable_op,
	.disable = mfg_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops isp_sys_ops = {
	.enable = isp_sys_enable_op,
	.disable = isp_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops ven_sys_ops = {
	.enable = ven_sys_enable_op,
	.disable = ven_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops vde_sys_ops = {
	.enable = vde_sys_enable_op,
	.disable = vde_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops dis_sys_ops = {
	.enable = dis_sys_enable_op,
	.disable = dis_sys_disable_op,
	.get_state = sys_get_state_op,
};

static int enable_subsys_locked(struct subsys *sys)
{
	int err;
	PRDPF("+enable_subsys_locked\n");

	if (sys->ops->get_state(sys) == PWR_ON)
		return 0;

	err = sys->ops->enable(sys);
	WARN_ON(err);

	PRDPF("-enable_subsys_locked\n");
	return err;
}

static int disable_subsys_locked(struct subsys *sys, int force_off)
{
	int err;
	PRDPF("+disable_subsys_locked\n");

	if (!force_off) {
		/* TODO: check all clocks related to this subsys are off */
		/* could be power off or not */
	}

	if (sys->ops->get_state(sys) == PWR_DOWN)
		return 0;

	err = sys->ops->disable(sys);
	WARN_ON(err);

	PRDPF("-enable_subsys_locked\n");
	return err;
}

int enable_subsys(enum subsys_id id)
{
	struct subsys *sys = id_to_sys(id);
	BUG_ON(!sys);
	PRDPF("+enable_subsys\n");

	return enable_subsys_locked(sys);
}

int disable_subsys(enum subsys_id id)
{
	struct subsys *sys = id_to_sys(id);
	BUG_ON(!sys);
	PRDPF("+disable_subsys\n");

	return disable_subsys_locked(sys, 0);
}

static void __init init_clk_scpsys(
	void __iomem *infracfg_reg,
	void __iomem *spm_reg ,
	void __iomem *mfg_config_reg)
{
	PRDPF("+init_clk_scpsys\n");

	infracfg_base = infracfg_reg;
	spm_base = spm_reg;
	mfg_config_base = mfg_config_reg;

	syss[SYS_VDE].ctl_addr = SPM_VDE_PWR_CON;
	syss[SYS_MFG_2D].ctl_addr = SPM_MFG_2D_PWR_CON;
	syss[SYS_MFG].ctl_addr = SPM_MFG_PWR_CON;
	syss[SYS_VEN].ctl_addr = SPM_VEN_PWR_CON;
	syss[SYS_ISP].ctl_addr = SPM_ISP_PWR_CON;
	syss[SYS_DIS].ctl_addr = SPM_DIS_PWR_CON;
	syss[SYS_IFR].ctl_addr = SPM_IFR_PWR_CON;

	PRDPF("-init_clk_scpsys\n");
} 

void init_device_node(void)
{
	struct device_node *device_node; 
	void __iomem *infracfg_reg;
	void __iomem *spm_reg;
	void __iomem *mfg_config_reg; 

	PRDPF("+init_device_node\n");
	device_node = of_find_compatible_node(NULL, NULL, "mediatek,mt8135-scpsys");
	if (!device_node) {
		PRDPF("device_node get node failed\n");
		return;
	}

	infracfg_reg = of_iomap(device_node, 0);
	spm_reg = of_iomap(device_node, 1);
	mfg_config_reg = of_iomap(device_node, 2);
	
	if (!infracfg_reg || !spm_reg || !mfg_config_reg) {
		PRDPF("clk-pg-mt8135: missing reg\n");
		return;
	}

	PRDPF("sys: %s, reg: 0x%08x, 0x%08x, 0x%08x\n",
		device_node->name, (uint32_t)infracfg_reg, (uint32_t)spm_reg,
		(uint32_t)mfg_config_reg); 

	init_clk_scpsys(infracfg_reg, spm_reg, mfg_config_reg);

	PRDPF("-init_device_node\n");
}

