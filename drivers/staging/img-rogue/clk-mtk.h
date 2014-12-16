#ifndef __DRV_CLK_MTK_H
#define __DRV_CLK_MTK_H

/*
 * This is a private header file. DO NOT include it except clk-*.c.
 */

#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>

#define CLK_DEBUG		0
#define DUMMY_REG_TEST		0

#ifndef GENMASK
#define GENMASK(h, l)	(((U32_C(1) << ((h) - (l) + 1)) - 1) << (l))
#endif

#define clk_readl(addr)		readl(addr)
#define clk_writel(val, addr)	do { writel(val, addr); dsb(); } while (0)
#define clk_setl(mask, addr)	clk_writel(clk_readl(addr) | (mask), addr)
#define clk_clrl(mask, addr)	clk_writel(clk_readl(addr) & ~(mask), addr)

extern spinlock_t *get_mt_clk_lock(void);

#define mt_clk_lock(flags)	spin_lock_irqsave(get_mt_clk_lock(), flags)
#define mt_clk_unlock(flags)	spin_unlock_irqrestore(get_mt_clk_lock(), flags)

#define MAX_MUX_GATE_BIT	31
#define INVALID_MUX_GATE_BIT	(MAX_MUX_GATE_BIT + 1)

enum subsys_id {
	SYS_VDE,
	SYS_MFG_2D,
	SYS_MFG,
	SYS_VEN,
	SYS_ISP,
	SYS_DIS,
	SYS_IFR,

	NR_SYSS,
};

extern int enable_subsys(enum subsys_id id);
extern int disable_subsys(enum subsys_id id);
extern void init_device_node(void);

struct clk *mt_clk_register_mux(
		const char *name,
		const char **parent_names,
		uint8_t num_parents,
		void __iomem *base_addr,
		uint8_t shift,
		uint8_t width,
		uint8_t gate_bit);

#endif /* __DRV_CLK_MTK_H */
