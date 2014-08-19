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

#include <linux/init.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/mt6397-regulator.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/mfd/mt6397/registers.h>

struct mt6397_buck_conf {
	unsigned int vosel_reg;
	unsigned int voselon_reg;
	unsigned int nivosel_reg;
	unsigned int vosel_mask;
};

struct mt6397_ldo_conf {
	unsigned int is_fixed;
	unsigned int vosel_reg;
	unsigned int vosel_mask;
};

struct mt6397_regulator_info {
	struct regulator_desc desc;
	struct mt6397_buck_conf buck_conf;
	struct mt6397_ldo_conf ldo_conf;
	unsigned int qi;
	unsigned int qi_mask;
};

#define MT6397_BUCK(vreg, min, max, step, volt_ranges,	\
		enreg, vosel, voselon, nivosel)		\
[MT6397_ID_##vreg] = {	\
	.desc = {							\
		.name = #vreg,				\
		.ops = &mt6397_volt_range_ops,			\
		.type = REGULATOR_VOLTAGE,				\
		.id = MT6397_ID_##vreg,					\
		.owner = THIS_MODULE,					\
		.n_voltages	= (max - min)/step + 1,		\
		.linear_ranges = volt_ranges,			\
		.n_linear_ranges = ARRAY_SIZE(volt_ranges),	\
		.enable_reg = enreg,		\
		.enable_mask = BIT(0),		\
	},	\
	.qi = enreg,	\
	.qi_mask = BIT(13),	\
	.buck_conf = {	\
		.vosel_reg = vosel,	\
		.voselon_reg = voselon,	\
		.nivosel_reg = nivosel, \
		.vosel_mask = 0x7f,	\
	},	\
}

#define MT6397_BUCK_TABLE(vreg, buck_volt_table,	\
		enreg, vosel, voselon, nivosel)		\
[MT6397_ID_##vreg] = {	\
	.desc = {							\
		.name = #vreg,				\
		.ops = &mt6397_volt_table_ops,			\
		.type = REGULATOR_VOLTAGE,				\
		.id = MT6397_ID_##vreg,					\
		.owner = THIS_MODULE,					\
		.n_voltages = 1,		\
		.volt_table = buck_volt_table,			\
		.enable_reg = enreg,		\
		.enable_mask = BIT(0),		\
	},	\
	.qi = enreg,	\
	.qi_mask = BIT(13),	\
	.buck_conf = {	\
		.vosel_reg = vosel,	\
		.voselon_reg = voselon,	\
		.nivosel_reg = nivosel, \
		.vosel_mask = 0x1f,	\
	},	\
}

#define MT6397_LDO(vreg, ldo_volt_table,	\
		enreg, enbit, fixed, vosel, _vosel_mask)		\
[MT6397_ID_##vreg] = {	\
	.desc = {							\
		.name = #vreg,				\
		.ops = &mt6397_volt_table_ops,			\
		.type = REGULATOR_VOLTAGE,				\
		.id = MT6397_ID_##vreg,					\
		.owner = THIS_MODULE,					\
		.n_voltages = ARRAY_SIZE(ldo_volt_table),		\
		.volt_table = ldo_volt_table,			\
		.enable_reg = enreg,		\
		.enable_mask = BIT(enbit),		\
	},	\
	.qi = enreg,	\
	.qi_mask = BIT(15),	\
	.ldo_conf = {	\
		.is_fixed = fixed,	\
		.vosel_reg = vosel,	\
		.vosel_mask = _vosel_mask,	\
	},	\
}

static const struct regulator_linear_range buck_volt_range1[] = {
	REGULATOR_LINEAR_RANGE(700000, 0, 0x7f, 6250),
};

static const struct regulator_linear_range buck_volt_range2[] = {
	REGULATOR_LINEAR_RANGE(800000, 0, 0x7f, 6250),
};

static const unsigned int fixed_1800000_voltage[] = {
	1800000,
};

static const unsigned int fixed_2800000_voltage[] = {
	2800000,
};

static const unsigned int fixed_3300000_voltage[] = {
	3300000,
};

static const unsigned int ldo_volt_table1[] = {
	1500000, 1800000, 2500000, 2800000,
};

static const unsigned int ldo_volt_table2[] = {
	1800000, 3300000,
};

static const unsigned int ldo_volt_table3[] = {
	3000000, 3300000,
};

static const unsigned int ldo_volt_table4[] = {
	1220000, 1300000, 1500000, 1800000, 2500000, 2800000, 3000000, 3300000,
};

static const unsigned int ldo_volt_table5[] = {
	1200000, 1300000, 1500000, 1800000, 2500000, 2800000, 3000000, 3300000,
};

static const unsigned int ldo_volt_table6[] = {
	1200000, 1300000, 1500000, 1800000, 2500000, 2800000, 3000000, 2000000,
};

static const unsigned int ldo_volt_table7[] = {
	1300000, 1500000, 1800000, 2000000, 2500000, 2800000, 3000000, 3300000,
};

static int mt6397_buck_set_voltage_sel(struct regulator_dev *rdev,
			unsigned sel)
{
	int ret;
	unsigned int vosel;
	unsigned int voselon;
	unsigned int vosel_mask;
	struct mt6397_regulator_info *info = rdev_get_drvdata(rdev);

	vosel = info->buck_conf.vosel_reg;
	voselon =  info->buck_conf.voselon_reg;
	vosel_mask =  info->buck_conf.vosel_mask;

	ret = regmap_update_bits(rdev->regmap, vosel, vosel_mask, sel);
	if (ret != 0) {
		dev_err(&rdev->dev, "Failed to update vosel: %d\n",
			ret);
		return ret;
	}

	ret = regmap_update_bits(rdev->regmap, voselon, vosel_mask, sel);
	if (ret != 0) {
		dev_err(&rdev->dev, "Failed to update vosel_on: %d\n",
			ret);
		return ret;
	}
	return 0;
}

static int mt6397_buck_get_voltage_sel(struct regulator_dev *rdev)
{
	int ret;
	unsigned int nivosel;
	unsigned int vosel_mask;
	unsigned int regval;
	struct mt6397_regulator_info *info = rdev_get_drvdata(rdev);

	nivosel = info->buck_conf.nivosel_reg;
	vosel_mask = info->buck_conf.vosel_mask;

	ret = regmap_read(rdev->regmap, nivosel, &regval);
	if (ret != 0) {
		dev_err(&rdev->dev, "Failed to get vosel: %d\n",
			ret);
		return ret;
	}

	regval &= vosel_mask;
	regval >>= ffs(vosel_mask) - 1;

	return regval;
}


static int mt6397_ldo_set_voltage_sel(struct regulator_dev *rdev,
			unsigned sel)
{
	int ret;
	unsigned int is_fixed;
	unsigned int vosel;
	unsigned int vosel_mask;
	unsigned int regval;
	struct mt6397_regulator_info *info = rdev_get_drvdata(rdev);

	is_fixed = info->ldo_conf.is_fixed;
	vosel = info->ldo_conf.vosel_reg;
	vosel_mask =  info->ldo_conf.vosel_mask;

	if (is_fixed) {
		return 0;
	} else {
		ret = regmap_update_bits(rdev->regmap, vosel, vosel_mask, sel);
		if (ret != 0) {
			dev_err(&rdev->dev, "Failed to update vosel: %d\n",
				ret);
			return ret;
		}

		ret = regmap_read(rdev->regmap, vosel, &regval);
		if (ret != 0) {
			dev_err(&rdev->dev, "Failed to update vosel: %d\n",
				ret);
			return ret;
		}
	}

	return 0;
}

static int mt6397_ldo_get_voltage_sel(struct regulator_dev *rdev)
{
	int ret;
	unsigned int vosel;
	unsigned int vosel_mask;
	unsigned int regval;
	struct mt6397_regulator_info *info = rdev_get_drvdata(rdev);

	vosel = info->ldo_conf.vosel_reg;
	vosel_mask = info->ldo_conf.vosel_mask;

	ret = regmap_read(rdev->regmap, vosel, &regval);
	if (ret != 0) {
		dev_err(&rdev->dev, "Failed to get vosel: %d\n",
			ret);
		return ret;
	}

	regval &= vosel_mask;
	regval >>= ffs(vosel_mask) - 1;

	return regval;
}

static int mt6397_regulator_is_enabled(struct regulator_dev *rdev)
{
	int ret;
	unsigned int regaddr;
	unsigned int regmask;
	unsigned int regval;
	struct mt6397_regulator_info *info = rdev_get_drvdata(rdev);

	regaddr = info->qi;
	regmask = info->qi_mask;

	ret = regmap_read(rdev->regmap, regaddr, &regval);
	if (ret != 0) {
		dev_err(&rdev->dev, "Failed to get vosel: %d\n",
			ret);
		return ret;
	}

	return (regval & info->qi_mask) ? 1 : 0;
}

static struct regulator_ops mt6397_volt_range_ops = {
	.list_voltage = regulator_list_voltage_linear_range,
	.map_voltage = regulator_map_voltage_linear_range,
	.set_voltage_sel = mt6397_buck_set_voltage_sel,
	.get_voltage_sel = mt6397_buck_get_voltage_sel,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = mt6397_regulator_is_enabled,
};

static struct regulator_ops mt6397_volt_table_ops = {
	.list_voltage = regulator_list_voltage_table,
	.map_voltage = regulator_map_voltage_iterate,
	.set_voltage_sel = mt6397_ldo_set_voltage_sel,
	.get_voltage_sel = mt6397_ldo_get_voltage_sel,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = mt6397_regulator_is_enabled,
};

/* The array is indexed by id(MT6397_ID_XXX) */
static struct mt6397_regulator_info mt6397_regulators[] = {
	MT6397_BUCK(VPCA15, 700000, 1493750, 625, buck_volt_range1,
		VCA15_CON7, VCA15_CON9, VCA15_CON10, VCA15_CON12),
	MT6397_BUCK(VPCA7, 700000, 1493750, 625, buck_volt_range1,
		VPCA7_CON7, VPCA7_CON9, VPCA7_CON10, VPCA7_CON12),
	MT6397_BUCK(VSRAMCA15, 700000, 1493750, 625, buck_volt_range1,
		VSRMCA15_CON7, VSRMCA15_CON9, VSRMCA15_CON10, VSRMCA15_CON12),
	MT6397_BUCK(VSRAMCA7, 700000, 1493750, 625, buck_volt_range1,
		VSRMCA7_CON7, VSRMCA7_CON9, VSRMCA7_CON10, VSRMCA7_CON12),
	MT6397_BUCK(VCORE, 700000, 1493750, 625, buck_volt_range1,
		VCORE_CON7, VCORE_CON9, VCORE_CON10, VCORE_CON12),
	MT6397_BUCK(VGPU, 700000, 1493750, 625, buck_volt_range1,
		VGPU_CON7, VGPU_CON9, VGPU_CON10, VCORE_CON12),
	MT6397_BUCK(VDRM, 800000, 1593750, 625, buck_volt_range2,
		VDRM_CON7, VDRM_CON9, VDRM_CON10, VDRM_CON12),
	MT6397_BUCK_TABLE(VIO18, fixed_1800000_voltage,
		VIO18_CON7, VIO18_CON9, VIO18_CON10, VIO18_CON12),
	MT6397_LDO(VTCXO, fixed_2800000_voltage,
		ANALDO_CON0, 10, 1, 0, 0),
	MT6397_LDO(VA28, fixed_2800000_voltage,
		ANALDO_CON1, 14, 1, 0, 0),
	MT6397_LDO(VCAMA, ldo_volt_table1,
		ANALDO_CON2, 15, 0, ANALDO_CON6, 0xC0),
	MT6397_LDO(VIO28, fixed_2800000_voltage,
		DIGLDO_CON0, 14, 1, 0, 0),
	MT6397_LDO(USB, fixed_3300000_voltage,
		DIGLDO_CON1, 14, 1, 0, 0),
	MT6397_LDO(VMC, ldo_volt_table2,
		DIGLDO_CON2, 12, 0, DIGLDO_CON29, 0x10),
	MT6397_LDO(VMCH, ldo_volt_table3,
		DIGLDO_CON3, 14, 0, DIGLDO_CON17, 0x80),
	MT6397_LDO(VEMC3V3, ldo_volt_table3,
		DIGLDO_CON4, 14, 0, DIGLDO_CON18, 0x10),
	MT6397_LDO(VCAMD, ldo_volt_table4,
		DIGLDO_CON5, 15, 0, DIGLDO_CON19, 0xE0),
	MT6397_LDO(VCAMIO, ldo_volt_table5,
		DIGLDO_CON6, 15, 0, DIGLDO_CON20, 0xE0),
	MT6397_LDO(VCAMAF, ldo_volt_table5,
		DIGLDO_CON7, 15, 0, DIGLDO_CON21, 0xE0),
	MT6397_LDO(VGP4, ldo_volt_table5,
		DIGLDO_CON8, 15, 0, DIGLDO_CON22, 0xE0),
	MT6397_LDO(VGP5, ldo_volt_table6,
		DIGLDO_CON9, 15, 0, DIGLDO_CON23, 0xE0),
	MT6397_LDO(VGP6, ldo_volt_table5,
		DIGLDO_CON10, 15, 0, DIGLDO_CON33, 0xE0),
	MT6397_LDO(VIBR, ldo_volt_table7,
		DIGLDO_CON24, 15, 0, DIGLDO_CON25, 0xE00),
};

#define MT6397_REGULATOR_OF_MATCH(_name, _id)				\
[MT6397_ID_##_id] = {	\
	.name = #_name,			\
	.driver_data = &mt6397_regulators[MT6397_ID_##_id],		\
}

static struct of_regulator_match mt6397_regulator_matches[] = {
	MT6397_REGULATOR_OF_MATCH(buck_vpca15, VPCA15),
	MT6397_REGULATOR_OF_MATCH(buck_vpca7, VPCA7),
	MT6397_REGULATOR_OF_MATCH(buck_vsramca15, VSRAMCA15),
	MT6397_REGULATOR_OF_MATCH(buck_vsramca7, VSRAMCA7),
	MT6397_REGULATOR_OF_MATCH(buck_vcore, VCORE),
	MT6397_REGULATOR_OF_MATCH(buck_vgpu, VGPU),
	MT6397_REGULATOR_OF_MATCH(buck_vdrm, VDRM),
	MT6397_REGULATOR_OF_MATCH(buck_vio18, VIO18),
	MT6397_REGULATOR_OF_MATCH(ldo_vtcxo, VTCXO),
	MT6397_REGULATOR_OF_MATCH(ldo_va28, VA28),
	MT6397_REGULATOR_OF_MATCH(ldo_vcama, VCAMA),
	MT6397_REGULATOR_OF_MATCH(ldo_vio28, VIO28),
	MT6397_REGULATOR_OF_MATCH(ldo_usb, USB),
	MT6397_REGULATOR_OF_MATCH(ldo_vmc, VMC),
	MT6397_REGULATOR_OF_MATCH(ldo_vmch, VMCH),
	MT6397_REGULATOR_OF_MATCH(ldo_vemc3v3, VEMC3V3),
	MT6397_REGULATOR_OF_MATCH(ldo_vcamd, VCAMD),
	MT6397_REGULATOR_OF_MATCH(ldo_vcamio, VCAMIO),
	MT6397_REGULATOR_OF_MATCH(ldo_vcamaf, VCAMAF),
	MT6397_REGULATOR_OF_MATCH(ldo_vgp4, VGP4),
	MT6397_REGULATOR_OF_MATCH(ldo_vgp5, VGP5),
	MT6397_REGULATOR_OF_MATCH(ldo_vgp6, VGP6),
	MT6397_REGULATOR_OF_MATCH(ldo_vibr, VIBR),
};

static int mt6397_regulator_dt_init(struct platform_device *pdev,
			struct mt6397_platform_data *pdata)
{
	struct device_node *np, *regulators;
	struct mt6397_regulator_data *rdata;
	int matched, i;

	np = of_node_get(pdev->dev.parent->of_node);
	if (!np)
		return -EINVAL;

	regulators = of_get_child_by_name(np, "regulators");
	if (!regulators) {
		dev_err(&pdev->dev, "regulators node not found\n");
		return -EINVAL;
	}
	matched = of_regulator_match(&pdev->dev, regulators,
				mt6397_regulator_matches,
				ARRAY_SIZE(mt6397_regulator_matches));
	of_node_put(regulators);
	if (matched < 0) {
		dev_err(&pdev->dev, "Error parsing regulator init data: %d\n",
			matched);
		return matched;
	}

	if (!pdata) {
		pr_info("mt6397 platform data missing\n");
		return -EINVAL;
	}
	pdata->num_regulators = matched;
	pdata->regulators = devm_kzalloc(&pdev->dev,
			(sizeof(struct mt6397_regulator_data) *
			ARRAY_SIZE(mt6397_regulator_matches)), GFP_KERNEL);
	if (!pdata->regulators)
		return -ENOMEM;

	rdata = pdata->regulators;
	for (i = 0; i < ARRAY_SIZE(mt6397_regulator_matches); i++) {
		rdata->id = i;
		rdata->name = mt6397_regulator_matches[i].name;
		rdata->initdata = mt6397_regulator_matches[i].init_data;
		rdata->reg_node = mt6397_regulator_matches[i].of_node;
		rdata++;
	}

	return 0;

}

static int mt6397_regulator_probe(struct platform_device *pdev)
{
	struct mt6397_chip *mt6397 = dev_get_drvdata(pdev->dev.parent);
	struct mt6397_platform_data *pdata = dev_get_platdata(mt6397->dev);
	int ret, i, id;
	struct regulator_config config = {};
	struct regulator_dev *rdev;

	ret = mt6397_regulator_dt_init(pdev, pdata);
	if (ret)
		return ret;

	for (i = 0; i < MT6397_ID_RG_MAX; i++) {
		id = pdata->regulators[i].id;
		config.dev = &pdev->dev;
		config.init_data = pdata->regulators[i].initdata;
		config.of_node = pdata->regulators[i].reg_node;
		config.driver_data = mt6397_regulator_matches[i].driver_data;
		config.regmap = mt6397->regmap;

		rdev = devm_regulator_register(&pdev->dev,
				&mt6397_regulators[i].desc, &config);
		if (IS_ERR(rdev)) {
			dev_err(&pdev->dev, "failed to register %s\n",
				mt6397_regulators[id].desc.name);
			return PTR_ERR(rdev);
		}
	}

	return 0;
}

static const struct platform_device_id mt6397_regulator_id[] = {
	{"mt6397-regulator", 0},
	{ },
};

static struct platform_driver mt6397_regulator_driver = {
	.driver		= {
		.name	= "mt6397-regulator",
		.owner	= THIS_MODULE,
	},
	.probe		= mt6397_regulator_probe,
	.id_table   = mt6397_regulator_id,
};

static int __init mt6397_regulator_init(void)
{
	pr_info("mt6397_regulator_init\n");
	return platform_driver_register(&mt6397_regulator_driver);
}
subsys_initcall(mt6397_regulator_init);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Flora Fu <flora.fu@mediatek.com>");
MODULE_DESCRIPTION("Regulator Driver for MediaTek MT6397 PMIC");
MODULE_ALIAS("platform:mt6397-regulator");
