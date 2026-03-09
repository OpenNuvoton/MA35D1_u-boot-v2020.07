// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Nuvoton Technology Corp.
 */

#include <common.h>
#include <dm.h>
#include <log.h>
#include <dm/device-internal.h>
#include <dm/lists.h>
#include <dm/pinctrl.h>
#include <regmap.h>
#include <syscon.h>
#include <fdtdec.h>
#include <linux/bitops.h>
#include <linux/libfdt.h>
#include <linux/err.h>
#include <asm/io.h>
#include <dm/ofnode.h>
#include <linux/ioport.h>

#include "pinctrl-nvt.h"

#define NVT_MUX_MASK	0xF

#define GPIO_PORT_NUM 14
#define GPIO_OFFSET 0x10

#define GPIO_MODE 0x0
#define GPIO_DOUT 0x4
#define GPIO_PIN 0x8
#define GPIO_INTSRC 0x20
#define GPIO_SMTEN 0x24
#define GPIO_SLEWCTL 0x28
#define GPIO_SPW 0x2C
#define GPIO_PUSEL 0x30
#define GPIO_DS 0x38
#define GPIO_UDS 0x3C

/*!< Input Mode */
#define GPIO_MODE_INPUT 0x0UL
/*!< Output Mode */
#define GPIO_MODE_OUTPUT 0x1UL
/*!< Open-Drain Mode */
#define GPIO_MODE_OPEN_DRAIN 0x2UL
/*!< Quasi-bidirectional Mode */
#define GPIO_MODE_QUASI 0x3UL

/*!< GPIO PUSEL setting for Pull-up Mode */
#define GPIO_PUSEL_DISABLE 0x0UL
/*!< GPIO PUSEL setting for Pull-up Mode */
#define GPIO_PUSEL_PULL_UP 0x1UL
/*!< GPIO PUSEL setting for Pull-down Mode */
#define GPIO_PUSEL_PULL_DOWN 0x2UL
#define GPIO_PUSEL_MASK 0x3UL	/*!< GPIO PUSEL Mask */

/*!< Generate the MODE mode setting for each pin  */
#define GPIO_SET_MODE(pin, mode) ((mode) << ((pin)<<1))

#define GPIO_PIN_DATA(base, pin) \
	(*((unsigned int *)((base+0x800) + ((pin)<<2))))

char *gpio_port_name[] = {
	"gpioa", "gpiob", "gpioc", "gpiod",
	"gpioe", "gpiof", "gpiog", "gpioh",
	"gpioi", "gpioj", "gpiok", "gpiol",
	"gpiom", "gpion"
};

static int nvt_set_mux(struct nvt_pinctrl_priv *priv,
		       unsigned int offset,
		       unsigned int shift, unsigned int muxval)
{
	unsigned int reg;

	regmap_read(priv->regmap, offset, &reg);
	reg &= ~(NVT_MUX_MASK << shift);
	reg |= muxval << shift;
	regmap_write(priv->regmap, offset, reg);
	return 0;
}

#if defined(CONFIG_MA35D1_GPIO) || defined(CONFIG_MA35D0_GPIO) || defined(CONFIG_MA35H0_GPIO)
static void nvt_gpio_cla_port(unsigned int gpio_num, int *group, int *num)
{
	*group = gpio_num / GPIO_OFFSET;
	*num = gpio_num % GPIO_OFFSET;
}

static int nvt_set_drive_perpin(struct nvt_pinctrl_priv *priv,
				unsigned int pin_id, int strength)
{

	int port_num, group_num;
	unsigned long value;
	void __iomem *base;
	struct nvt_pin_ctrl *ctrl = priv->ctrl;

	nvt_gpio_cla_port(pin_id, &group_num, &port_num);
	base = ctrl->pin_banks[group_num].reg_base;
	if(port_num<8) {
		value = __raw_readl(base + GPIO_DS);
		value = value & ~(0x7<<(port_num*4));
		value = value | ((strength&0x7)<<(port_num*4));
		__raw_writel(value, base + GPIO_DS);
	} else {
		value = __raw_readl(base + GPIO_UDS);
		value = value & ~(0x7<<((port_num-8)*4));
		value = value | ((strength&0x7)<<((port_num-8)*4));
		__raw_writel(value, base + GPIO_UDS);
	}

	return 0;
}

static int nvt_set_pull(struct nvt_pinctrl_priv *priv,
			unsigned int pin_id, int pull_up)
{

	int port_num, group_num;
	unsigned long value;
	void __iomem *base;
	struct nvt_pin_ctrl *ctrl = priv->ctrl;

	nvt_gpio_cla_port(pin_id, &group_num, &port_num);
	base = ctrl->pin_banks[group_num].reg_base;
	value = __raw_readl(base + GPIO_PUSEL);
	value &= ~GPIO_SET_MODE(port_num, GPIO_PUSEL_MASK);
	switch (pull_up) {
	case PIN_CONFIG_BIAS_PULL_UP:
		value |= GPIO_SET_MODE(port_num, GPIO_PUSEL_PULL_UP);
		break;
	case PIN_CONFIG_BIAS_PULL_DOWN:
		value |= GPIO_SET_MODE(port_num, GPIO_PUSEL_PULL_DOWN);
		break;
	case GPIO_PUSEL_DISABLE:
	default:
		value |= GPIO_SET_MODE(port_num, GPIO_PUSEL_DISABLE);
		break;
	}
	__raw_writel(value, base + GPIO_PUSEL);
	return 0;
}

static int nvt_set_schmitt(struct nvt_pinctrl_priv *priv,
			   unsigned int pin_id, int enable)
{

	int port_num, group_num;
	unsigned long value;
	void __iomem *base;
	struct nvt_pin_ctrl *ctrl = priv->ctrl;

	nvt_gpio_cla_port(pin_id, &group_num, &port_num);
	base = ctrl->pin_banks[group_num].reg_base;
	value = __raw_readl(base + GPIO_SMTEN);
	value &= (1<<port_num);
	value |= ((enable&0x1)<<port_num);
	__raw_writel(value, base + GPIO_SMTEN);
	return 0;
}

static int nvt_set_pwr_src(struct nvt_pinctrl_priv *priv,
                           unsigned int pin_id, int src)
{
        int port_num, group_num;
        unsigned long value;
        void __iomem *base;
        struct nvt_pin_ctrl *ctrl = priv->ctrl;
	int v=0;

	if(src!=1800) v=1;
        nvt_gpio_cla_port(pin_id, &group_num, &port_num);
        base = ctrl->pin_banks[group_num].reg_base;
        value = __raw_readl(base + GPIO_SPW);
        value &= ~(1<<port_num);
        value |= (v<<port_num);
        __raw_writel(value, base + GPIO_SPW);
	return 0;
}

static int nvt_set_slew_rate(struct nvt_pinctrl_priv *priv,
                           unsigned int pin_id, int rate)
{
	int port_num, group_num;
	unsigned long value;
	void __iomem *base;
	struct nvt_pin_ctrl *ctrl = priv->ctrl;

	nvt_gpio_cla_port(pin_id, &group_num, &port_num);
	base = ctrl->pin_banks[group_num].reg_base;
	value = __raw_readl(base + GPIO_SLEWCTL);
	value &= ~GPIO_SET_MODE(port_num, 0x3);
	value |= GPIO_SET_MODE(port_num, rate & 0x1);
	__raw_writel(value, base + GPIO_SLEWCTL);
	return 0;

}


/* set the pin config settings for a specified pin */
static int nvt_pinconf_set(struct nvt_pinctrl_priv *priv,
			   u32 pin, u32 param, u32 arg)
{
	int rc;

	switch (param) {
	case PIN_CONFIG_BIAS_DISABLE:
	case PIN_CONFIG_BIAS_PULL_UP:
	case PIN_CONFIG_BIAS_PULL_DOWN:
	case PIN_CONFIG_BIAS_PULL_PIN_DEFAULT:
	case PIN_CONFIG_BIAS_BUS_HOLD:
		rc = nvt_set_pull(priv, pin, param);
		if (rc)
			return rc;
		break;

	case PIN_CONFIG_DRIVE_STRENGTH:
		rc = nvt_set_drive_perpin(priv, pin, arg);
		if (rc < 0)
			return rc;
		break;

	case PIN_CONFIG_INPUT_SCHMITT_ENABLE:
		rc = nvt_set_schmitt(priv, pin, arg);
		if (rc < 0)
			return rc;
		break;

	case PIN_CONFIG_POWER_SOURCE:
		rc = nvt_set_pwr_src(priv, pin, arg);
		if (rc < 0)
			return rc;
		break;

	case PIN_CONFIG_SLEW_RATE:
		rc = nvt_set_slew_rate(priv, pin, arg);
		if (rc < 0)
			return rc;
		break;
	default:
		break;
	}

	return 0;
}

static const struct pinconf_param nvt_conf_params[] = {
	{"bias-disable", PIN_CONFIG_BIAS_DISABLE, 0},
	{"bias-bus-hold", PIN_CONFIG_BIAS_BUS_HOLD, 0},
	{"bias-pull-up", PIN_CONFIG_BIAS_PULL_UP, 1},
	{"bias-pull-down", PIN_CONFIG_BIAS_PULL_DOWN, 1},
	{"bias-pull-pin-default", PIN_CONFIG_BIAS_PULL_PIN_DEFAULT, 1},
	{"drive-strength", PIN_CONFIG_DRIVE_STRENGTH, 0},
	{"input-schmitt-disable", PIN_CONFIG_INPUT_SCHMITT_ENABLE, 0},
	{"input-schmitt-enable", PIN_CONFIG_INPUT_SCHMITT_ENABLE, 1},
	{"power-source", PIN_CONFIG_POWER_SOURCE, 0 },
	{"slew-rate", PIN_CONFIG_SLEW_RATE, 0},
};

static int nvt_pinconf_prop_name_to_param(const char *property,
					  u32 *default_value)
{
	const struct pinconf_param *p, *end;

	p = nvt_conf_params;
	end = p + sizeof(nvt_conf_params) / sizeof(struct pinconf_param);

	/* See if this pctldev supports this parameter */
	for (; p < end; p++) {
		if (!strcmp(property, p->property)) {
			*default_value = p->default_value;
			return p->param;
		}
	}

	*default_value = 0;
	return -EPERM;
}
#endif

static int nvt_pinctrl_set_state(struct udevice *dev,
		struct udevice *config)
{
	struct nvt_pinctrl_priv *priv = dev_get_priv(dev);
	struct nvt_pin_ctrl *ctrl = priv->ctrl;
	u32 cells[MAX_NVT_PINS_ENTRIES * 4];
	int ret, size, i;
	const u32 *data;
	u32 offset, shift, muxval, conf, pins;
#if defined(CONFIG_MA35D1_GPIO) || defined(CONFIG_MA35D0_GPIO) || defined(CONFIG_MA35H0_GPIO)
	const void *value;
	const char *prop_name;
	int prop_len, param;
	ofnode node;
#ifdef CONFIG_OF_LIVE
	const struct device_node *np;
	struct property *pp;
#else
	int property_offset, pcfg_node;
	const void *blob = gd->fdt_blob;
#endif
	u32 default_val, arg;
#endif
	/*
	 * the binding format is nuvoton,pins = <bank pin pin-function>,
	 * do sanity check and calculate pins number
	 */
	data = dev_read_prop(config, "nuvoton,pins", &size);
	if (size < 0) {
		debug("%s: bad array size %d\n", __func__, size);
		return -EINVAL;
	}
	size /= sizeof(u32);
	if (size > MAX_NVT_PINS_ENTRIES * 4) {
		debug("%s: unsupported pins array count %d\n", __func__, size);
		return -EINVAL;
	}

	for (i = 0; i < size; i++)
		cells[i] = fdt32_to_cpu(data[i]);

	for (i = 0; i < (size >> 2); i++) {
		offset = cells[4 * i + 0];
		shift = cells[4 * i + 1];
		muxval = cells[4 * i + 2];
		conf = cells[4 * i + 3];
		pins = ctrl->get_pin_num(offset, shift);

		ret = nvt_set_mux(priv, offset, shift, muxval);
		if (ret)
			return ret;
#if defined(CONFIG_MA35D1_GPIO) || defined(CONFIG_MA35D0_GPIO) || defined(CONFIG_MA35H0_GPIO)
		node = ofnode_get_by_phandle(conf);
		if (!ofnode_valid(node))
			return -ENODEV;
#ifdef CONFIG_OF_LIVE
		np = ofnode_to_np(node);
		for (pp = np->properties; pp; pp = pp->next) {
			prop_name = pp->name;
			prop_len = pp->length;
			value = pp->value;
#else
		pcfg_node = ofnode_to_offset(node);
		fdt_for_each_property_offset(property_offset, blob, pcfg_node) {
			value = fdt_getprop_by_offset(blob, property_offset,
						      &prop_name, &prop_len);

#endif
			param = nvt_pinconf_prop_name_to_param(prop_name,
							       &default_val);

			if (param < 0)
				continue;
			if (prop_len >= sizeof(fdt32_t))
				arg = fdt32_to_cpu(*(fdt32_t *) value);
			else
				arg = default_val;
			ret = nvt_pinconf_set(priv, pins, param, arg);
			if (ret) {
				debug("%s: nvt_pinconf_set fail: %d\n",
				      __func__, ret);
				return ret;
			}
		}
#endif
	}
	return 0;
}

const struct pinctrl_ops nvt_pinctrl_ops = {
	.set_state = nvt_pinctrl_set_state,
};

static int nvt_pinctrl_get_soc_data(struct udevice *dev)
{
	struct nvt_pinctrl_priv *priv = dev_get_priv(dev);
	struct nvt_pin_ctrl *ctrl = priv->ctrl;
#if defined(CONFIG_MA35D1_GPIO) || defined(CONFIG_MA35D0_GPIO) || defined(CONFIG_MA35H0_GPIO)
	int i;
	ofnode node;
	struct nvt_pin_bank *bank;

	ctrl->nr_banks = GPIO_PORT_NUM;
	ctrl->pin_banks = malloc(ctrl->nr_banks * sizeof(*ctrl->pin_banks));
	if (!ctrl->pin_banks) {
		dev_err(dev, "Not enough memory\n");
		return -ENOMEM;
	}

	for (i = 0; i < ctrl->nr_banks; i++)
		ctrl->pin_banks[i].name = gpio_port_name[i];

	dev_for_each_subnode(node, dev) {
		if (!ofnode_read_bool(node, "gpio-controller"))
			continue;
		debug("%s: name: %s\n", __func__, ofnode_get_name(node));
		bank = ctrl->pin_banks;
		for (i = 0; i < ctrl->nr_banks; ++i, ++bank) {
			if (!strncmp(bank->name, ofnode_get_name(node), 5)) {
				#ifdef CONFIG_OF_LIVE
				struct resource res;
				#endif
				#if defined(CONFIG_MA35D1_GPIO)
				device_bind_driver_to_node(dev, "ma35d1_gpio",
							   ofnode_get_name
							   (node), node,
							   &bank->dev);
				#elif defined(CONFIG_MA35D0_GPIO)
				device_bind_driver_to_node(dev, "ma35d0_gpio",
							   ofnode_get_name
							   (node), node,
							   &bank->dev);
				#elif defined(CONFIG_MA35H0_GPIO)
				device_bind_driver_to_node(dev, "ma35h0_gpio",
							   ofnode_get_name
							   (node), node,
							   &bank->dev);
				#endif
				bank->dev->driver_data = i;
				#ifdef CONFIG_OF_LIVE
				dev_read_resource(bank->dev, 0, &res);
				bank->reg_base =(void __iomem *)res.start;
				#else
				device_probe(bank->dev);
				bank->reg_base =
				    (void __iomem *)devfdt_get_addr(bank->dev);			
				#endif
				bank->nr_pins = 16;
				bank->pin_base = (i * bank->nr_pins);
				bank->valid = true;
				break;
			}
		}
	}
#endif

	return 0;
}

int nvt_pinctrl_probe(struct udevice *dev)
{
	struct nvt_pinctrl_priv *priv = dev_get_priv(dev);
	struct udevice *syscon;
	struct regmap *regmap;
	int ret = 0;

	priv->ctrl = (struct nvt_pin_ctrl *)dev_get_driver_data(dev);

	/* get nvt grf syscon phandle */
	ret = uclass_get_device_by_phandle(UCLASS_SYSCON, dev, "nuvoton,sys",
					   &syscon);
	if (ret) {
		debug("unable to find nuvoton,sys syscon device (%d)\n", ret);
		return ret;
	}

	/* get nuvoton,sys base address */
	regmap = syscon_get_regmap(syscon);
	if (!regmap) {
		debug("unable to find nvt grf regmap\n");
		return -ENODEV;
	}
	priv->regmap = regmap;

	nvt_pinctrl_get_soc_data(dev);

	return 0;
}
