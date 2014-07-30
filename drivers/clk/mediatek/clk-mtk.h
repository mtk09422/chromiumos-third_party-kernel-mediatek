#ifndef __MACH_CLK_MTK_H
#define __MACH_CLK_MTK_H

/*
 * This is a private header file. DO NOT include it except clk-*.c.
 */

#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>


#define DUMMY_REG_TEST		0
#define CLK_DEBUG		0
#define HARDCODE_CLKS		0


struct mt_clk_fixed_rate {
	struct		clk_hw hw;
	unsigned long	fixed_rate;
};


#define to_clk_fixed_rate(_hw) container_of(_hw, struct mt_clk_fixed_rate, hw)


struct mt_clk_gate;
typedef void (*mt_clk_gate_cb)(struct mt_clk_gate *);
typedef int (*mt_clk_gate_bool_cb)(struct mt_clk_gate *);


struct mt_clk_gate {
	struct clk_hw		hw;
	struct clk		*pre_clk;
	struct mt_clk_regs	*regs;
	uint8_t			bit;
	uint32_t		flags;
	mt_clk_gate_cb		before_enable;
	mt_clk_gate_cb		after_disable;
	mt_clk_gate_bool_cb	pre_clk_is_enabled;
};


#define to_clk_gate(_hw) container_of(_hw, struct mt_clk_gate, hw)


#define CLK_GATE_INVERSE	BIT(0)
#define CLK_GATE_NO_SETCLR_REG	BIT(1)


struct mt_clk_mux {
	struct clk_hw	hw;
	void __iomem	*base_addr;
	uint32_t	mask;
	uint8_t		shift;
	uint8_t		gate_bit;
};


#define to_mt_clk_mux(_hw) container_of(_hw, struct mt_clk_mux, hw)


#define MAX_MUX_GATE_BIT	31
#define INVALID_MUX_GATE_BIT	(MAX_MUX_GATE_BIT + 1)


struct mt_clk_fixed_factor {
	struct clk_hw	hw;
	uint32_t	mult;
	uint32_t	div;
};


#define to_mt_clk_fixed_factor(_hw) \
		container_of(_hw, struct mt_clk_fixed_factor, hw)


struct mt_clk_pll {
	struct clk_hw	hw;
	void __iomem	*base_addr;
	void __iomem	*pwr_addr;
	uint32_t	en_mask;
	uint32_t	flags;
};


#define to_mt_clk_pll(_hw) container_of(_hw, struct mt_clk_pll, hw)


#define HAVE_RST_BAR	BIT(0)
#define HAVE_PLL_HP	BIT(1)
#define HAVE_FIX_FRQ	BIT(2)
#define PLL_AO		BIT(3)

extern const struct clk_ops mt_clk_pll_ops;
extern const struct clk_ops mt_clk_arm_pll_ops;
extern const struct clk_ops mt_clk_lc_pll_ops;
extern const struct clk_ops mt_clk_aud_pll_ops;
extern const struct clk_ops mt_clk_tvd_pll_ops;


struct mt_clk_regs {
	void __iomem	*sta_addr;
	void __iomem	*set_addr;
	void __iomem	*clr_addr;
	uint32_t	cg_mask;
	uint32_t	en_mask;
};


struct clk *mt_clk_register_fixed_rate(
		const char *name,
		const char *parent_name,
		unsigned long fixed_rate);

struct clk *mt_clk_register_gate(
		const char *name,
		const char *parent_name,
		const char *pre_clk_name,
		struct mt_clk_regs *regs,
		uint8_t bit,
		uint32_t flags,
		mt_clk_gate_cb before_enable,
		mt_clk_gate_cb after_disable,
		mt_clk_gate_bool_cb pre_clk_is_enabled);

struct clk *mt_clk_register_mux_table(
		const char *name,
		const char **parent_names,
		uint8_t num_parents,
		void __iomem *base_addr,
		uint8_t shift,
		uint32_t mask,
		uint8_t gate_bit);

struct clk *mt_clk_register_mux(
		const char *name,
		const char **parent_names,
		uint8_t num_parents,
		void __iomem *base_addr,
		uint8_t shift,
		uint8_t width,
		uint8_t gate_bit);

struct clk *mt_clk_register_fixed_factor(
		const char *name,
		const char *parent_name,
		uint32_t mult,
		uint32_t div);

struct clk *mt_clk_register_pll(
		const char *name,
		const char *parent_name,
		uint32_t *base_addr,
		uint32_t *pwr_addr,
		uint32_t en_mask,
		uint32_t flags,
		const struct clk_ops *ops);


#endif /* __MACH_CLK_MTK_H */
