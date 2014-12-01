/*
 * arch/arm/mach-mediatek/platsmp.c
 *
 * Copyright (c) 2014 Mediatek Inc.
 * Author: Shunli Wang <shunli.wang@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/string.h>
#include <linux/threads.h>
#include <asm/cacheflush.h>
#include <asm/io.h>
#include <asm/smp.h>

#define BASE			IOMEM(PAGE_OFFSET + 0x2000)
#define STR_PP			"mediatek,smp-reg"
#define MAGIC_REG		0x03f8
#define JUMP_REG		0x03fc
#define BOOT_REG		0x03f4
#define DEF_NUM			4

#define DECLARE_DEFAULT_BOOT_INFO {	\
		.m_keys = mtk_smp_m_keys,	\
		.m_regs = mtk_smp_m_regs,	\
		.j_reg = JUMP_REG,	\
		.b_reg = BOOT_REG,	\
		.base = BASE,	\
}

struct mtk_smp_boot_info {
	unsigned int *m_keys;
	unsigned int *m_regs;
	unsigned int j_reg;
	unsigned int b_reg;
	void __iomem *base;
};

static unsigned int mtk_smp_m_keys[] = {
	0x534c4131, 0x534c4131, 0x4c415332, 0x41534c33,
	0x534c4134, 0x4c415335, 0x41534c36, 0x534c4137
};
static unsigned int mtk_smp_m_regs[NR_CPUS] = {
	[1 ... DEF_NUM - 1] = MAGIC_REG,
};
static struct mtk_smp_boot_info core_boot =
				DECLARE_DEFAULT_BOOT_INFO;

static int __cpuinit mtk_boot_secondary(unsigned int cpu,
						struct task_struct *idle)
{
	writel(core_boot.m_keys[cpu],
		core_boot.base + core_boot.m_regs[cpu]);

	/* memory barrior need to be used here
	 * to keep normal hardeare access order.
	 */
	smp_wmb();
	sync_cache_w(core_boot.base + core_boot.m_regs[cpu]);

	arch_send_wakeup_ipi_mask(cpumask_of(cpu));

	return 0;
}

static void __init mtk_smp_boot_info_prepare(unsigned int cpu,
							struct device_node *np)
{
	struct of_phandle_args args;
	void __iomem *base = NULL;

	if (!of_get_property(np, STR_PP, NULL))
		return;

	/*
	 * it is a phandle list with three arguments
	 * like <phandle arg1 arg2 arg3>, and
	 * arg1 = m_reg, arg2 = j_reg, arg3 = b_reg.
	 */
	if (of_parse_phandle_with_fixed_args(np, STR_PP, 3, 0, &args))
		return;

	/*
	 * ONLY support unified register base,
	 * so m_reg, j_reg and b_reg of all cores
	 * have same base address.
	 */
	if (core_boot.base == BASE) {
		base = of_iomap(args.np, 0);
		core_boot.base = base ? base : BASE;
		core_boot.j_reg = args.args[1];
		core_boot.b_reg = args.args[2];
	}

	core_boot.m_regs[cpu] = args.args[0];
}

static void __init mtk_smp_prepare_cpus(unsigned int max_cpus)
{
	struct device_node *cpu_node;
	unsigned int cpu;

	/*
	 * traverse all the possible cpus and get smp boot base like
	 * "mediatek,smp-reg = <&sramrom 0x3c 0x34 0x30>".
	 */
	for_each_possible_cpu(cpu) {
		cpu_node = of_get_cpu_node(cpu, NULL);
		if (!cpu_node)
			continue;
		mtk_smp_boot_info_prepare(cpu, cpu_node);
	}

	/*
	 * write the address of slave startup
	 * into the system-wide flags register
	 * or a ram reserved position
	 */
	writel(virt_to_phys(secondary_startup),
				core_boot.base + core_boot.j_reg);
}

static struct smp_operations mt81xx_smp_ops __initdata = {
	.smp_prepare_cpus = mtk_smp_prepare_cpus,
	.smp_boot_secondary = mtk_boot_secondary,
};

CPU_METHOD_OF_DECLARE(mt81xx_smp, "mediatek,mt81xx-smp", &mt81xx_smp_ops);
