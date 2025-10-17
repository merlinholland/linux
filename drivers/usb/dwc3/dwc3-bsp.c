// SPDX-License-Identifier: GPL-2.0
/*
*
* Copyright (c) 2012-2018 Shenshu Technologies Co., Ltd.
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
*/
#include <linux/clk-provider.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>
#include <linux/slab.h>
#include <linux/usb/ch9.h>

#include "dwc3-bsp.h"

#define USB3_CTRL           0x190
#define REG_SYS_STAT        0x8c
#define PCIE_USB3_MODE_MASK (0x3 << 12)
#define USB3_PCLK_OCC_SEL   (0x1 << 30)

#define PERI_USB3_GTXTHRCFG 0x2310000

#define REG_GUSB3PIPECTL0 0xc2c0
#define GTXTHRCFG         0xc108

#define PCS_SSP_SOFT_RESET  (0x1 << 31)
#define SUSPEND_USB3_SS_PHY (0x1 << 17)

#define GUSB2PHYCFG_OFFSET 0xc200
#define GCTL_OFFSET        0xc110
#define GUCTL_OFFSET       0xc12C
#define GFLADJ_OFFSET      0xc630

#define U2_FREECLK_EXISTS (0x1 << 30)
#define SOFITPSYNC        (0x1 << 10)
#define REFCLKPER_MASK    0xffc00000
#define REFCLKPER_VAL	0x29
#define set_refclkper(a)  (((a) << 22) & REFCLKPER_MASK)

#define PLS1        (0x1 << 31)
#define DECR_MASK   0x7f000000
#define DECR_VAL	0xa
#define set_decr(a) (((a) << 24) & DECR_MASK)

#define LPM_SEL      (0x1 << 23)
#define FLADJ_MASK   0x003fff00
#define FLADJ_VAL	0x7f0
#define set_fladj(a) (((a) << 8) & FLADJ_MASK)

/* ss919v100 */
#define DOUBLE_PCIE_MODE    0x0
#define P0_PCIE_ADD_P1_USB3 (0x1 << 12)
#define DOUBLE_USB3         (0x2 << 12)

/* ss318v100,ss918v100 */
#define PCIE_X1_MODE (0x0 << 12)
#define USB3_MODE    (0x1 << 12)

static struct bsp_priv *usb_priv = NULL;

#if defined(CONFIG_ARCH_SS919V100) || defined(CONFIG_ARCH_SS015V100)
static int speed_adapt_for_ss919v100(struct device_node *np)
{
	unsigned int ret;
	unsigned int reg;

	if (np == NULL)
		return -EINVAL;

	usb_priv->speed_id = -1;

	reg = readl(usb_priv->sys_ctrl + REG_SYS_STAT);
	reg &= PCIE_USB3_MODE_MASK;

	switch (reg) {
	case DOUBLE_PCIE_MODE:
		ret = USB_SPEED_HIGH;
		break;
	case P0_PCIE_ADD_P1_USB3:
		if (of_property_read_u32(np, "port_speed", &usb_priv->speed_id))
			usb_priv->speed_id = -1;

		if (usb_priv->speed_id == 0)
			ret = USB_SPEED_HIGH;
		else if (usb_priv->speed_id == 1)
			ret = USB_SPEED_SUPER;
		else
			ret = USB_SPEED_UNKNOWN;

		break;
	case DOUBLE_USB3:
		ret = USB_SPEED_SUPER;
		break;
	default:
		ret = USB_SPEED_UNKNOWN;
	}

	return ret;
}
#endif

#if defined(CONFIG_ARCH_SS318V100) || defined(CONFIG_ARCH_SS918V100)
static int speed_adapt_for_ss318v100(struct device *dev)
{
	unsigned int ret;
	unsigned int reg;

	if (dev == NULL)
		return -EINVAL;

	reg = readl(usb_priv->sys_ctrl + REG_SYS_STAT);
	reg &= PCIE_USB3_MODE_MASK;

	if (reg == PCIE_X1_MODE)
		ret = USB_SPEED_HIGH;
	else
		ret = usb_get_maximum_speed(dev);

	return ret;
}
#endif

int usb_get_max_speed(struct device *dev)
{
	unsigned int ret;
	struct device_node *np = dev->of_node;

	if (np == NULL)
		return -EINVAL;

	usb_priv = kzalloc(sizeof(struct bsp_priv), GFP_KERNEL);
	if (usb_priv == NULL)
		return -ENOMEM;

	usb_priv->peri_crg = of_iomap(np, DEV_NODE_FLAG1);
	if (IS_ERR(usb_priv->peri_crg)) {
		kfree(usb_priv);
		usb_priv = NULL;
		return -ENOMEM;
	}

	usb_priv->sys_ctrl = of_iomap(np, DEV_NODE_FLAG2);
	if (IS_ERR(usb_priv->sys_ctrl)) {
		iounmap(usb_priv->peri_crg);

		kfree(usb_priv);
		usb_priv = NULL;
		return -ENOMEM;
	}

#if defined(CONFIG_ARCH_SS919V100) || defined(CONFIG_ARCH_SS015V100)
	ret = speed_adapt_for_ss919v100(np);
#elif defined(CONFIG_ARCH_SS318V100) || defined(CONFIG_ARCH_SS918V100)
	ret = speed_adapt_for_ss318v100(dev);
#else
	ret = usb_get_maximum_speed(dev);
#endif

	iounmap(usb_priv->sys_ctrl);
	iounmap(usb_priv->peri_crg);

	return ret;
}
EXPORT_SYMBOL(usb_get_max_speed);

void bsp_dwc3_exited(void)
{
	if (usb_priv == NULL)
		return;

	kfree(usb_priv);
	usb_priv = NULL;
}
EXPORT_SYMBOL(bsp_dwc3_exited);

static int set_ctrl_crg_val(struct device_node *np, struct dwc3_bsp *bsp)
{
	unsigned int ret;
	unsigned int reg;

	if ((np == NULL) || (bsp == NULL))
		return -EINVAL;

	/* get usb ctrl crg para */
	ret = of_property_read_u32(np, "crg_offset", &bsp->crg_offset);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "crg_ctrl_def_mask", &bsp->crg_ctrl_def_mask);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "crg_ctrl_def_val", &bsp->crg_ctrl_def_val);
	if (ret)
		return ret;

	/* write usb ctrl crg default value */
	reg = readl(bsp->crg_base + bsp->crg_offset);
	reg &= ~bsp->crg_ctrl_def_mask;
	reg |= bsp->crg_ctrl_def_val;
	writel(reg, bsp->crg_base + bsp->crg_offset);

	return 0;
}

static int dwc3_bsp_clk_init(struct dwc3_bsp *bsp, int count)
{
	struct device *dev = bsp->dev;
	struct device_node *np = dev->of_node;
	int i, ret;

	if (!count)
		return -EINVAL;

	if (np == NULL)
		return -EINVAL;

	bsp->num_clocks = count;

	bsp->clks = devm_kcalloc(dev, bsp->num_clocks, sizeof(struct clk *),
				  GFP_KERNEL);
	if (bsp->clks == NULL)
		return -ENOMEM;

	for (i = 0; i < bsp->num_clocks; i++) {
		struct clk *clk;

		clk = of_clk_get(np, i);
		if (IS_ERR(clk)) {
			while (--i >= 0)
				clk_put(bsp->clks[i]);

			ret = PTR_ERR(clk);
			goto clk_free;
		}

		ret = clk_prepare_enable(clk);
		if (ret < 0) {
			while (--i >= 0) {
				clk_disable_unprepare(bsp->clks[i]);
				clk_put(bsp->clks[i]);
			}
			clk_put(clk);

			goto clk_free;
		}

		bsp->clks[i] = clk;
	}

	return 0;
clk_free:
	devm_kfree(dev, bsp->clks);
	bsp->clks = NULL;

	return ret;
}

static void control_free_clk_config(struct dwc3_bsp *bsp)
{
	unsigned int reg;

	if (bsp == NULL)
		return;

	reg = readl(bsp->ctrl_base + GUSB2PHYCFG_OFFSET);
	reg &= ~U2_FREECLK_EXISTS;
	writel(reg, bsp->ctrl_base + GUSB2PHYCFG_OFFSET);

	reg = readl(bsp->ctrl_base + GCTL_OFFSET);
	reg &= ~SOFITPSYNC;
	writel(reg, bsp->ctrl_base + GCTL_OFFSET);

	reg = readl(bsp->ctrl_base + GUCTL_OFFSET);
	reg &= ~REFCLKPER_MASK;
	reg |= set_refclkper(REFCLKPER_VAL);
	writel(reg, bsp->ctrl_base + GUCTL_OFFSET);

	reg = readl(bsp->ctrl_base + GFLADJ_OFFSET);
	reg &= ~PLS1;
	writel(reg, bsp->ctrl_base + GFLADJ_OFFSET);

	reg = readl(bsp->ctrl_base + GFLADJ_OFFSET);
	reg &= ~DECR_MASK;
	reg |= set_decr(DECR_VAL);
	writel(reg, bsp->ctrl_base + GFLADJ_OFFSET);

	reg = readl(bsp->ctrl_base + GFLADJ_OFFSET);
	reg |= LPM_SEL;
	writel(reg, bsp->ctrl_base + GFLADJ_OFFSET);

	reg = readl(bsp->ctrl_base + GFLADJ_OFFSET);
	reg &= ~FLADJ_MASK;
	reg |= set_fladj(FLADJ_VAL);
	writel(reg, bsp->ctrl_base + GFLADJ_OFFSET);
}

static int dwc3_bsp_iomap(struct device_node *np, struct dwc3_bsp *bsp)
{
	if ((np == NULL) || (bsp == NULL))
		return -EINVAL;

	bsp->ctrl_base = of_iomap(np, DEV_NODE_FLAG0);
	if (IS_ERR(bsp->ctrl_base))
		return -ENOMEM;

	bsp->crg_base = of_iomap(np, DEV_NODE_FLAG1);
	if (IS_ERR(bsp->crg_base)) {
		iounmap(bsp->ctrl_base);
		return -ENOMEM;
	}

	return 0;
}

static int dwc3_bsp_probe(struct platform_device *pdev)
{
	struct dwc3_bsp *bsp = NULL;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	int ret, i;

	bsp = devm_kzalloc(dev, sizeof(*bsp), GFP_KERNEL);
	if (bsp == NULL)
		return -ENOMEM;

	platform_set_drvdata(pdev, bsp);
	bsp->dev = dev;

	ret = dwc3_bsp_iomap(np, bsp);
	if (ret) {
		devm_kfree(dev, bsp);
		bsp = NULL;

		return -ENOMEM;
	}

	bsp->port_rst = devm_reset_control_get(dev, "vcc_reset");
	if (IS_ERR_OR_NULL(bsp->port_rst)) {
		ret = PTR_ERR(bsp->port_rst);
		goto dwc3_unmap;
	}

	ret = set_ctrl_crg_val(np, bsp);
	if (ret)
		goto dwc3_unmap;

	reset_control_assert(bsp->port_rst);

	ret = dwc3_bsp_clk_init(bsp, of_clk_get_parent_count(np));
	if (ret)
		goto dwc3_unmap;

	reset_control_deassert(bsp->port_rst);

	control_free_clk_config(bsp);

	udelay(U_LEVEL2);

	ret = of_platform_populate(np, NULL, NULL, dev);
	if (ret) {
		for (i = 0; i < bsp->num_clocks; i++) {
			clk_disable_unprepare(bsp->clks[i]);
			clk_put(bsp->clks[i]);
		}
		goto dwc3_unmap;
	}

	return 0;
dwc3_unmap:
	iounmap(bsp->ctrl_base);
	iounmap(bsp->crg_base);

	devm_kfree(dev, bsp);
	bsp = NULL;

	return ret;
}

static int dwc3_bsp_remove(struct platform_device *pdev)
{
	struct dwc3_bsp *bsp = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;
	int i;

	for (i = 0; i < bsp->num_clocks; i++) {
		clk_disable_unprepare(bsp->clks[i]);
		clk_put(bsp->clks[i]);
	}

	reset_control_assert(bsp->port_rst);

	of_platform_depopulate(dev);

	iounmap(bsp->ctrl_base);
	iounmap(bsp->crg_base);

	devm_kfree(dev, bsp);
	bsp = NULL;

	return 0;
}

static const struct of_device_id bsp_dwc3_match[] = {
	{ .compatible = "vendor,dwusb2" },
	{ .compatible = "vendor,dwusb3" },
	{},
};
MODULE_DEVICE_TABLE(of, bsp_dwc3_match);

static struct platform_driver dwc3_bsp_driver = {
	.probe = dwc3_bsp_probe,
	.remove = dwc3_bsp_remove,
	.driver = {
		.name = "bsp-dwc3",
		.of_match_table = bsp_dwc3_match,
	},
};
module_platform_driver(dwc3_bsp_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("DesignWare USB3");
