/*
* mt65xx pinctrl driver based on Allwinner A1X pinctrl driver.
* Copyright (c) 2014 MediaTek Inc.
* Author: Hongzhou.Yang <hongzhou.yang@mediatek.com>
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

#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/irqdomain.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/machine.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "pinctrl-mt65xx.h"
#include "pinctrl-mtk-mt8135.h"

#define PINMUX_MAX_VAL 8
#define MAX_GPIO_MODE_PER_REG 5
#define GPIO_MODE_BITS        3

enum MT_CONFIG_OPTION {
	MT_CONFIG_PULL_ENABLE,
	MT_CONFIG_PULL_SELECT,
	MT_CONFIG_PULL_END,
};

static const struct mt_pinctrl_desc mt8135_pinctrl_data = {
	.pins = mt_pins_mt8135,
	.npins = ARRAY_SIZE(mt_pins_mt8135),
};

static const struct gpio_offsets mt8135_gpio_offsets = {
	.dir_offset = 0x0000,
	.pullen_offset = 0x0200,
	.pullsel_offset = 0x0400,
	.invser_offset = 0x0600,
	.dout_offset = 0x0800,
	.din_offset = 0x0A00,
	.pinmux_offset = 0x0C00,
	.type1_start = 34,
	.type1_end = 149,
};

static void __iomem *_get_base_addr(struct mt_pinctrl *pctl,
		unsigned long pin)
{
	if (pin >= pctl->offset->type1_start && pin < pctl->offset->type1_end)
		return pctl->membase2;
	return pctl->membase1;
}

static void pinctrl_read_reg(struct mt_pinctrl *pctl,
		unsigned long pin,
		u32 reg, u32 *d)
{
	*d = readl((void *)(_get_base_addr(pctl, pin) + reg));
}

static void pinctrl_write_reg(struct mt_pinctrl *pctl,
		unsigned long pin,
		u32 reg, u32 d)
{
	writel(d, (void *)(_get_base_addr(pctl, pin) + reg));
}

static unsigned int _get_port(unsigned long pin)
{
	return ((pin >> 4) & 0xF) << 4;
}

static int mt_gpio_set_pull_conf(struct pinctrl_dev *pctldev,
		unsigned long pin, unsigned long enable,
		enum MT_CONFIG_OPTION option)
{
	unsigned int reg_addr;
	unsigned int bit;
	unsigned long flags;
	struct mt_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	if (option == MT_CONFIG_PULL_ENABLE)
		reg_addr = _get_port(pin) + pctl->offset->pullen_offset;
	else
		reg_addr = _get_port(pin) + pctl->offset->pullsel_offset;

	bit = 1 << (pin & 0xF);

	spin_lock_irqsave(&pctl->lock, flags);
	if (enable)
		pinctrl_write_reg(pctl, pin,
				reg_addr + 4, bit);
	else
		pinctrl_write_reg(pctl, pin,
				reg_addr + (4 << 1), bit);
	spin_unlock_irqrestore(&pctl->lock, flags);

	return 0;
}

static int mt_gpio_get_pull_cof(struct pinctrl_dev *pctldev,
		unsigned long pin, enum MT_CONFIG_OPTION option)
{
	unsigned int reg_addr;
	unsigned int bit;
	unsigned int read_val = 0;
	struct mt_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	if (option == MT_CONFIG_PULL_ENABLE)
		reg_addr = _get_port(pin) + pctl->offset->pullen_offset;
	else
		reg_addr = _get_port(pin) + pctl->offset->pullsel_offset;

	bit = 1 << (pin & 0xF);
	pinctrl_read_reg(pctl, pin, reg_addr, &read_val);
	return ((read_val & bit) != 0) ? 1 : 0;
}


static int mt_pconf_set(struct pinctrl_dev *pctldev,
				 unsigned pin,
				 unsigned long *configs,
				 unsigned num_configs)
{
	switch (pinconf_to_config_param(configs[0])) {
	case PIN_CONFIG_BIAS_DISABLE:
		mt_gpio_set_pull_conf(pctldev, pin, 0, MT_CONFIG_PULL_ENABLE);
		break;
	case PIN_CONFIG_BIAS_PULL_UP:
		mt_gpio_set_pull_conf(pctldev, pin, 1, MT_CONFIG_PULL_ENABLE);
		mt_gpio_set_pull_conf(pctldev, pin, 1, MT_CONFIG_PULL_SELECT);
		break;

	case PIN_CONFIG_BIAS_PULL_DOWN:
		mt_gpio_set_pull_conf(pctldev, pin, 1, MT_CONFIG_PULL_ENABLE);
		mt_gpio_set_pull_conf(pctldev, pin, 0, MT_CONFIG_PULL_SELECT);
		break;

	default:
		break;
	}

	return 0;
}

static int mt_pconf_get(struct pinctrl_dev *pctldev,
		unsigned pin,
		unsigned long *configs)
{
	int pull_en = 0;
	int pull_sel = 0;
	pull_en = mt_gpio_get_pull_cof(pctldev, pin, MT_CONFIG_PULL_ENABLE);
	if (!pull_en) {
		*configs = PIN_CONFIG_BIAS_DISABLE;
		return 0;
	}

	pull_sel = mt_gpio_get_pull_cof(pctldev, pin, MT_CONFIG_PULL_SELECT);
	if (pull_sel)
		*configs = PIN_CONFIG_BIAS_PULL_UP;
	else
		*configs = PIN_CONFIG_BIAS_PULL_DOWN;

	return 0;
}

static int mt_pconf_group_get(struct pinctrl_dev *pctldev,
				 unsigned group,
				 unsigned long *config)
{
	struct mt_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	*config = pctl->groups[group].config;

	return 0;
}

static int mt_pconf_group_set(struct pinctrl_dev *pctldev,
				 unsigned group,
				 unsigned long *configs,
				 unsigned num_configs)
{
	struct mt_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	struct mt_pinctrl_group *g = &pctl->groups[group];
	int i;

	for (i = 0; i < num_configs; i++) {
		switch (pinconf_to_config_param(configs[i])) {
		case PIN_CONFIG_BIAS_PULL_UP:
			mt_gpio_set_pull_conf(pctldev,
					g->pin, 1, MT_CONFIG_PULL_ENABLE);
			mt_gpio_set_pull_conf(pctldev,
					g->pin, 1, MT_CONFIG_PULL_SELECT);
			break;
		case PIN_CONFIG_BIAS_PULL_DOWN:
			mt_gpio_set_pull_conf(pctldev,
					g->pin, 1, MT_CONFIG_PULL_ENABLE);
			mt_gpio_set_pull_conf(pctldev,
					g->pin, 0, MT_CONFIG_PULL_SELECT);
			break;
		default:
			break;
		}
		/* cache the config value */
		g->config = configs[i];
	} /* for each config */

	return 0;
}


static const struct pinconf_ops mt_pconf_ops = {
	.pin_config_get	= mt_pconf_get,
	.pin_config_set	= mt_pconf_set,
	.pin_config_group_get	= mt_pconf_group_get,
	.pin_config_group_set	= mt_pconf_group_set,
};

static struct mt_pinctrl_group *
mt_pinctrl_find_group_by_pad_name(struct mt_pinctrl *pctl, const char *pad_name)
{
	int i;

	for (i = 0; i < pctl->ngroups; i++) {
		struct mt_pinctrl_group *grp = pctl->groups + i;

		if (grp->pad && !strcmp(grp->pad, pad_name))
			return grp;
	}

	return NULL;
}

static struct mt_desc_function *
mt_pinctrl_desc_find_function_by_number(struct mt_pinctrl *pctl,
					 const char *pin_name,
					 const int number)
{
	int i;

	for (i = 0; i < pctl->desc->npins; i++) {
		const struct mt_desc_pin *pin = pctl->desc->pins + i;
		struct mt_desc_function *func = pin->functions;

		if (!strcmp(pin->pin.name, pin_name))
			return func + number;
	}

	return NULL;
}

static int mt_pctrl_dt_node_to_map(struct pinctrl_dev *pctldev,
				      struct device_node *node,
				      struct pinctrl_map **map,
				      unsigned *num_maps)
{
	struct mt_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	unsigned long *pinconfig;
	struct property *prop;
	struct mt_desc_function *func;
	u32 function;
	const char *pad_name;
	int ret, nmaps, i = 0;
	u32 val;

	*map = NULL;
	*num_maps = 0;

	ret = of_property_read_u32(node, "mediatek,function", &function);
	if (ret) {
		dev_err(pctl->dev,
			"missing mediatek,function property in node %s\n",
			node->name);
		return -EINVAL;
	}

	nmaps = of_property_count_strings(node, "mediatek,pins") * 2;
	if (nmaps < 0) {
		dev_err(pctl->dev,
			"missing mediatek,pins property in node %s\n",
			node->name);
		return -EINVAL;
	}

	*map = kmalloc(nmaps * sizeof(struct pinctrl_map), GFP_KERNEL);
	if (!*map)
		return -ENOMEM;

	of_property_for_each_string(node, "mediatek,pins", prop, pad_name) {
		struct mt_pinctrl_group *grp =
				mt_pinctrl_find_group_by_pad_name(pctl,
						pad_name);
		int j = 0, configlen = 0;

		if (!grp) {
			dev_err(pctl->dev, "unknown pin %s", pad_name);
			continue;
		}

		func = mt_pinctrl_desc_find_function_by_number(pctl,
			      grp->name,
			      function);
		if (!func) {
			dev_err(pctl->dev, "unsupported function %d on pin %s",
				function, pad_name);
			continue;
		}

		(*map)[i].type = PIN_MAP_TYPE_MUX_GROUP;
		(*map)[i].data.mux.group = grp->name;
		(*map)[i].data.mux.function = func->name;

		i++;

		(*map)[i].type = PIN_MAP_TYPE_CONFIGS_GROUP;
		(*map)[i].data.configs.group_or_pin = grp->name;

		if (of_find_property(node, "mediatek,pull", NULL))
			configlen++;

		pinconfig = kzalloc(configlen * sizeof(*pinconfig), GFP_KERNEL);

		if (!of_property_read_u32(node, "mediatek,pull", &val)) {
			enum pin_config_param pull = PIN_CONFIG_END;
			if (val == 1)
				pull = PIN_CONFIG_BIAS_PULL_UP;
			else if (val == 2)
				pull = PIN_CONFIG_BIAS_PULL_DOWN;
			pinconfig[j++] = pinconf_to_config_packed(pull, 0);
		}

		(*map)[i].data.configs.configs = pinconfig;
		(*map)[i].data.configs.num_configs = configlen;

		i++;
	}

	*num_maps = nmaps;

	return 0;
}

static void mt_pctrl_dt_free_map(struct pinctrl_dev *pctldev,
				    struct pinctrl_map *map,
				    unsigned num_maps)
{
	int i;

	for (i = 0; i < num_maps; i++) {
		if (map[i].type == PIN_MAP_TYPE_CONFIGS_GROUP)
			kfree(map[i].data.configs.configs);
	}

	kfree(map);
}

static int mt_pctrl_get_groups_count(struct pinctrl_dev *pctldev)
{
	struct mt_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	return pctl->ngroups;
}

static const char *mt_pctrl_get_group_name(struct pinctrl_dev *pctldev,
					      unsigned group)
{
	struct mt_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	return pctl->groups[group].name;
}

static int mt_pctrl_get_group_pins(struct pinctrl_dev *pctldev,
				      unsigned group,
				      const unsigned **pins,
				      unsigned *num_pins)
{
	struct mt_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	*pins = (unsigned *)&pctl->groups[group].pin;
	*num_pins = 1;

	return 0;
}

static const struct pinctrl_ops mt_pctrl_ops = {
	.dt_node_to_map		= mt_pctrl_dt_node_to_map,
	.dt_free_map		= mt_pctrl_dt_free_map,
	.get_groups_count	= mt_pctrl_get_groups_count,
	.get_group_name		= mt_pctrl_get_group_name,
	.get_group_pins		= mt_pctrl_get_group_pins,
};

static int mt_pmx_get_funcs_cnt(struct pinctrl_dev *pctldev)
{
	struct mt_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	return pctl->nfunctions;
}

static const char *mt_pmx_get_func_name(struct pinctrl_dev *pctldev,
					   unsigned function)
{
	struct mt_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	return pctl->functions[function].name;
}

static int mt_pmx_get_func_groups(struct pinctrl_dev *pctldev,
				     unsigned function,
				     const char * const **groups,
				     unsigned * const num_groups)
{
	struct mt_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	*groups = pctl->functions[function].groups;
	*num_groups = pctl->functions[function].ngroups;

	return 0;
}

static int mt_gpio_set_mode(struct pinctrl_dev *pctldev,
		unsigned long pin, unsigned long mode)
{
	unsigned int reg_addr;
	unsigned char bit;
	unsigned int val;
	unsigned long flags;
	unsigned int mask = (1L << GPIO_MODE_BITS) - 1;
	struct mt_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	reg_addr = ((int)(pin / 5) << 4)
			+ pctl->offset->pinmux_offset;

	spin_lock_irqsave(&pctl->lock, flags);
	val = readl((void *)(pctl->membase1 + reg_addr));
	bit = pin % MAX_GPIO_MODE_PER_REG;
	val &= ~(mask << (GPIO_MODE_BITS*bit));
	val |= (mode << (GPIO_MODE_BITS*bit));
	writel(val, (void *)(pctl->membase1 + reg_addr));
	spin_unlock_irqrestore(&pctl->lock, flags);
	return 0;
}

static struct mt_desc_function *
mt_pinctrl_desc_find_function_by_name(struct mt_pinctrl *pctl,
					 const char *pin_name,
					 const char *func_name)
{
	int i, j;

	for (i = 0; i < pctl->desc->npins; i++) {
		const struct mt_desc_pin *pin = pctl->desc->pins + i;

		if (!strcmp(pin->pin.name, pin_name)) {
			struct mt_desc_function *func = pin->functions;

			for (j = 0; j < PINMUX_MAX_VAL; j++) {
				if (!strcmp(func->name, func_name))
					return func;

				func++;
			}
		}
	}

	return NULL;
}

static struct mt_desc_function *
mt_pinctrl_desc_find_irq_by_name(struct mt_pinctrl *pctl,
					 const char *pin_name)
{
	int i, j;

	for (i = 0; i < pctl->desc->npins; i++) {
		const struct mt_desc_pin *pin = pctl->desc->pins + i;

		if (!strcmp(pin->pin.name, pin_name)) {
			struct mt_desc_function *func = pin->functions;

			for (j = 0; j < PINMUX_MAX_VAL; j++) {
				if (func->irqnum != 255)
					return func;

				func++;
			}
		}
	}

	return NULL;
}

static int mt_pmx_enable(struct pinctrl_dev *pctldev,
			    unsigned function,
			    unsigned group)
{
	struct mt_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	struct mt_pinctrl_group *g = pctl->groups + group;
	struct mt_pinctrl_function *func = pctl->functions + function;
	struct mt_desc_function *desc =
		mt_pinctrl_desc_find_function_by_name(pctl,
							 g->name,
							 func->name);

	if (!desc)
		return -EINVAL;

	mt_gpio_set_mode(pctldev, g->pin, desc->muxval);
	return 0;
}

static void mt_pmx_disable(struct pinctrl_dev *pctldev,
			    unsigned function,
			    unsigned group)
{
	struct mt_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	struct mt_pinctrl_group *g = pctl->groups + group;
	struct mt_pinctrl_function *func = pctl->functions + function;
	struct mt_desc_function *desc =
		mt_pinctrl_desc_find_function_by_name(pctl,
							 g->name,
							 func->name);

	if (!desc)
		return;

	mt_gpio_set_mode(pctldev, g->pin, 0);
	return;
}

static int mt_pmx_gpio_set_direction(struct pinctrl_dev *pctldev,
			struct pinctrl_gpio_range *range,
			unsigned offset,
			bool input)
{
	unsigned int reg_addr;
	unsigned int bit;
	unsigned long flags;
	struct mt_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	reg_addr = _get_port(offset) + pctl->offset->dir_offset;
	bit = 1 << (offset & 0xF);

	if (input)
		reg_addr += (4 << 1);
	else
		reg_addr += 4;

	spin_lock_irqsave(&pctl->lock, flags);
	writel(bit, (void *)(pctl->membase1 + reg_addr));
	spin_unlock_irqrestore(&pctl->lock, flags);
	return 0;
}

static const struct pinmux_ops mt_pmx_ops = {
	.get_functions_count	= mt_pmx_get_funcs_cnt,
	.get_function_name	= mt_pmx_get_func_name,
	.get_function_groups	= mt_pmx_get_func_groups,
	.enable			= mt_pmx_enable,
	.disable	= mt_pmx_disable,
	.gpio_set_direction	= mt_pmx_gpio_set_direction,
};

static int mt_gpio_request(struct gpio_chip *chip, unsigned offset)
{
	return pinctrl_request_gpio(chip->base + offset);
}

static void mt_gpio_free(struct gpio_chip *chip, unsigned offset)
{
	pinctrl_free_gpio(chip->base + offset);
}

static int mt_gpio_direction_input(struct gpio_chip *chip,
					unsigned offset)
{
	return pinctrl_gpio_direction_input(chip->base + offset);
}

static int mt_gpio_direction_output(struct gpio_chip *chip,
					unsigned offset, int value)
{
	return pinctrl_gpio_direction_output(chip->base + offset);
}

static int mt_gpio_get_direction(struct gpio_chip *chip, unsigned offset)
{
	unsigned int reg_addr;
	unsigned int bit;
	unsigned int read_val = 0;

	struct mt_pinctrl *pctl = dev_get_drvdata(chip->dev);

	reg_addr =  _get_port(offset) + pctl->offset->dir_offset;
	bit = 1 << (offset & 0xF);
	read_val = readl((void *)(pctl->membase1 + reg_addr));
	return ((read_val & bit) != 0) ? 1 : 0;
}

static int mt_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	unsigned int reg_addr;
	unsigned int bit;
	unsigned int read_val = 0;
	struct mt_pinctrl *pctl = dev_get_drvdata(chip->dev);

	if (mt_gpio_get_direction(chip, offset))
		reg_addr = _get_port(offset) + pctl->offset->dout_offset;
	else
		reg_addr = _get_port(offset) + pctl->offset->din_offset;

	bit = 1 << (offset & 0xF);
	read_val = readl((void *)(pctl->membase1 + reg_addr));
	return ((read_val & bit) != 0) ? 1 : 0;
}

static void mt_gpio_set(struct gpio_chip *chip,
				unsigned offset, int value)
{
	unsigned int reg_addr;
	unsigned int bit;
	unsigned long flags;
	struct mt_pinctrl *pctl = dev_get_drvdata(chip->dev);

	reg_addr = _get_port(offset) + pctl->offset->dout_offset;
	bit = 1 << (offset & 0xF);

	spin_lock_irqsave(&pctl->lock, flags);
	if (value)
		writel(bit, (void *)(pctl->membase1 + reg_addr + 4));
	else
		writel(bit, (void *)(pctl->membase1 + reg_addr + (4 << 1)));
	spin_unlock_irqrestore(&pctl->lock, flags);
}

static int mt_gpio_to_irq(struct gpio_chip *chip, unsigned offset)
{
	struct mt_pinctrl *pctl = dev_get_drvdata(chip->dev);
	struct mt_pinctrl_group *g = pctl->groups + offset;
	struct mt_desc_function *desc =
			mt_pinctrl_desc_find_irq_by_name(pctl,
							 g->name);

	if (!desc)
		return -EINVAL;

	mt_gpio_set_mode(pctl->pctl_dev, g->pin, desc->muxval);
	return desc->irqnum;
}

static struct gpio_chip mt_gpio_chip = {
	.owner			= THIS_MODULE,
	.request		= mt_gpio_request,
	.free			= mt_gpio_free,
	.direction_input	= mt_gpio_direction_input,
	.direction_output	= mt_gpio_direction_output,
	.get			= mt_gpio_get,
	.set			= mt_gpio_set,
	.to_irq			= mt_gpio_to_irq,
};

static struct mt_pinctrl_function *
mt_pinctrl_find_function_by_name(struct mt_pinctrl *pctl,
				    const char *name)
{
	struct mt_pinctrl_function *func = pctl->functions;
	int i;

	for (i = 0; i < pctl->nfunctions; i++) {
		if (!func[i].name)
			break;

		if (!strcmp(func[i].name, name))
			return func + i;
	}

	return NULL;
}

static int mt_pinctrl_add_function(struct mt_pinctrl *pctl,
					const char *name)
{
	struct mt_pinctrl_function *func = pctl->functions;

	if (!name)
		return -EINVAL;

	while (func->name) {
		/* function already there */
		if (strcmp(func->name, name) == 0) {
			func->ngroups++;
			return -EEXIST;
		}
		func++;
	}

	func->name = name;
	func->ngroups = 1;

	pctl->nfunctions++;

	return 0;
}

static int mt_pinctrl_build_state(struct platform_device *pdev)
{
	struct mt_pinctrl *pctl = platform_get_drvdata(pdev);
	int i, j, ret;

	pctl->ngroups = pctl->desc->npins;

	/* Allocate groups */
	pctl->groups = devm_kzalloc(&pdev->dev,
				    pctl->ngroups * sizeof(*pctl->groups),
				    GFP_KERNEL);
	if (!pctl->groups)
		return -ENOMEM;

	for (i = 0; i < pctl->desc->npins; i++) {
		const struct mt_desc_pin *pin = pctl->desc->pins + i;
		struct mt_pinctrl_group *group = pctl->groups + i;

		group->name = pin->pin.name;
		group->pad = pin->pad;
		group->pin = pin->pin.number;
	}

	pctl->functions = devm_kzalloc(&pdev->dev,
				pctl->desc->npins * PINMUX_MAX_VAL
				* sizeof(*pctl->functions),
				GFP_KERNEL);
	if (!pctl->functions) {
		ret = -ENOMEM;
		goto func_error;
	}

	for (i = 0; i < pctl->desc->npins; i++) {
		const struct mt_desc_pin *pin = pctl->desc->pins + i;
		struct mt_desc_function *func = pin->functions;

		for (j = 0; j < PINMUX_MAX_VAL; j++) {
			mt_pinctrl_add_function(pctl, func->name);
			func++;
		}
	}

	for (i = 0; i < pctl->desc->npins; i++) {
		const struct mt_desc_pin *pin = pctl->desc->pins + i;
		struct mt_desc_function *func = pin->functions;

		for (j = 0; j < PINMUX_MAX_VAL; j++) {
			struct mt_pinctrl_function *func_item;
			const char **func_grp;

			if (!func->name)
				continue;

			func_item = mt_pinctrl_find_function_by_name(
					pctl, func->name);
			if (!func_item) {
				dev_err(&pdev->dev, "find function by name error, func: %s.\n",
						func->name);
				ret = -EINVAL;
				goto build_grp_error;
			}

			if (!func_item->groups) {
				func_item->groups =
					devm_kzalloc(&pdev->dev,
						     func_item->ngroups *
						     sizeof(*func_item->groups),
						     GFP_KERNEL);
				if (!func_item->groups) {
					ret =  -ENOMEM;
					goto build_grp_error;
				}
			}

			func_grp = func_item->groups;
			while (*func_grp)
				func_grp++;

			*func_grp = pin->pin.name;
			func++;
		}
	}

	return 0;

build_grp_error:
	devm_kfree(&pdev->dev, pctl->functions);
func_error:
	devm_kfree(&pdev->dev, pctl->groups);
	return ret;
}

static struct pinctrl_desc mt_pctrl_desc = {
	.confops	= &mt_pconf_ops,
	.pctlops	= &mt_pctrl_ops,
	.pmxops		= &mt_pmx_ops,
};

static struct of_device_id mt_pinctrl_match_data[] = {
	{ .compatible = "mediatek,mt8135-pinctrl",
	  .data = (void *)&mt8135_pinctrl_data }
};
MODULE_DEVICE_TABLE(of, mt_pinctrl_match_data);

static struct of_device_id mt_pinctrl_match_offset[] = {
	{ .compatible = "mediatek,mt8135-pinctrl",
	  .data = (void *)&mt8135_gpio_offsets }
};

static int mt_pinctrl_probe(struct platform_device *pdev)
{
	const struct of_device_id *device;
	struct pinctrl_pin_desc *pins;
	struct mt_pinctrl *pctl;
	int i, ret;

	pctl = devm_kzalloc(&pdev->dev, sizeof(*pctl), GFP_KERNEL);
	if (!pctl)
		return -ENOMEM;

	platform_set_drvdata(pdev, pctl);

	spin_lock_init(&pctl->lock);

	pctl->membase1 = of_io_request_and_map(pdev->dev.of_node,
			0, pdev->name);
	if (!pctl->membase1) {
		ret = -ENOMEM;
		goto pctl_error;
	}

	pctl->membase2 = of_io_request_and_map(pdev->dev.of_node,
			1, pdev->name);
	if (!pctl->membase2) {
		ret = -ENOMEM;
		goto pctl_error;
	}

	device = of_match_device(mt_pinctrl_match_data, &pdev->dev);
	if (!device) {
		ret = -ENODEV;
		goto pctl_error;
	}

	pctl->desc = (struct mt_pinctrl_desc *)device->data;

	device = of_match_device(mt_pinctrl_match_offset, &pdev->dev);
	if (!device) {
		ret = -ENODEV;
		goto pctl_error;
	}

	pctl->offset = (struct gpio_offsets *) device->data;
	ret = mt_pinctrl_build_state(pdev);
	if (ret) {
		dev_err(&pdev->dev, "build state failed: %d\n", ret);
		ret = -EINVAL;
		goto pctl_error;
	}

	pins = devm_kzalloc(&pdev->dev,
			    pctl->desc->npins * sizeof(*pins),
			    GFP_KERNEL);
	if (!pins) {
		ret = -ENOMEM;
		goto pctl_error;
	}

	for (i = 0; i < pctl->desc->npins; i++)
		pins[i] = pctl->desc->pins[i].pin;
	mt_pctrl_desc.name = dev_name(&pdev->dev);
	mt_pctrl_desc.owner = THIS_MODULE;
	mt_pctrl_desc.pins = pins;
	mt_pctrl_desc.npins = pctl->desc->npins;
	pctl->dev = &pdev->dev;
	pctl->pctl_dev = pinctrl_register(&mt_pctrl_desc,
					  &pdev->dev, pctl);
	if (!pctl->pctl_dev) {
		dev_err(&pdev->dev, "couldn't register pinctrl driver\n");
		ret = -EINVAL;
		goto pins_error;
	}

	pctl->chip = devm_kzalloc(&pdev->dev, sizeof(*pctl->chip), GFP_KERNEL);
	if (!pctl->chip) {
		ret = -ENOMEM;
		goto chip_error;
	}

	pctl->chip = &mt_gpio_chip;
	pctl->chip->ngpio = pctl->desc->npins;
	pctl->chip->label = dev_name(&pdev->dev);
	pctl->chip->dev = &pdev->dev;
	pctl->chip->base = 0;

	ret = gpiochip_add(pctl->chip);
	if (ret) {
		ret = -EINVAL;
		goto chip_error;
	}

	for (i = 0; i < pctl->desc->npins; i++) {
		const struct mt_desc_pin *pin = pctl->desc->pins + i;

		ret = gpiochip_add_pin_range(pctl->chip, dev_name(&pdev->dev),
					     pin->pin.number,
					     pin->pin.number, 1);
		if (ret) {
			ret = -EINVAL;
			goto chip_error;
		}
	}

	return 0;

chip_error:
	if (gpiochip_remove(pctl->chip))
		dev_err(&pdev->dev, "failed to remove gpio chip\n");

	devm_kfree(&pdev->dev, pctl->chip);
	pinctrl_unregister(pctl->pctl_dev);
pins_error:
	devm_kfree(&pdev->dev, pins);
pctl_error:
	devm_kfree(&pdev->dev, pctl);
	return ret;
}

static int mt_pinctrl_remove(struct platform_device *pdev)
{
	int ret;
	struct mt_pinctrl *pctl = platform_get_drvdata(pdev);

	pinctrl_unregister(pctl->pctl_dev);

	ret = gpiochip_remove(pctl->chip);
	if (ret) {
		dev_err(&pdev->dev, "%s failed, %d\n",
					"gpiochip_remove()", ret);
		return ret;
	}

	return 0;
}

static struct platform_driver mt_pinctrl_driver = {
	.probe = mt_pinctrl_probe,
	.remove = mt_pinctrl_remove,
	.driver = {
		.name = "mediatek-pinctrl",
		.owner = THIS_MODULE,
		.of_match_table = mt_pinctrl_match_data,
	},
};

static int __init mtk_pinctrl_init(void)
{
	return platform_driver_register(&mt_pinctrl_driver);
}

static void __exit mtk_pinctrl_exit(void)
{
	platform_driver_unregister(&mt_pinctrl_driver);
}

postcore_initcall(mtk_pinctrl_init);
module_exit(mtk_pinctrl_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek Pinctrl Driver");
MODULE_AUTHOR("Hongzhou Yang <hongzhou.yang@mediatek.com>");
