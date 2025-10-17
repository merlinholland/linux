/*
 * Vendor SS625V100 gemac pinout config.
 *
 * Copyright (C) Shenshu Technologies Co., Ltd. 2019. All rights reserved.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/of_net.h>
#include <linux/of_mdio.h>
#include <linux/platform_device.h>
#include "mdio_bsp_gemac.h"

/* MDIO0 pinctrl phyical addr */
#define PHY_ADDR_MDCK0			0x017C70138
#define PHY_ADDR_MDIO0			0x017C7013C
/* MDIO1 pinctrl phyical addr */
#define PHY_ADDR_MDCK1			0x017C701E0
#define PHY_ADDR_MDIO1			0x017C70200

/* PHY0 pinctrl phyical addr */
#define PHY_ADDR_EPHY0_CLK		0x017C7011C
#define PHY_ADDR_EPHY0_RSTN		0x017C7014C
/* PHY1 pinctrl phyical addr */
#define PHY_ADDR_EPHY1_CLK		0x017C701F0
#define PHY_ADDR_EPHY1_RSTN		0x017C701DC

/* RGMII0 pinctrl phyical addr */
#define PHY_ADDR_RGMII0_TXCKOUT	0x017C7012C
#define PHY_ADDR_RGMII0_TXD0	0x017C70114
#define PHY_ADDR_RGMII0_TXD1	0x017C70118
#define PHY_ADDR_RGMII0_TXD2	0x017C70120
#define PHY_ADDR_RGMII0_TXD3	0x017C70124
#define PHY_ADDR_RGMII0_TXEN	0x017C70128
#define PHY_ADDR_RGMII0_RXCK	0x017C70154
#define PHY_ADDR_RGMII0_RXD0	0x017C70144
#define PHY_ADDR_RGMII0_RXD1	0x017C70140
#define PHY_ADDR_RGMII0_RXD2	0x017C70134
#define PHY_ADDR_RGMII0_RXD3	0x017C70130
#define PHY_ADDR_RGMII0_RXDV	0x017C70150

/* RGMII1 pinctrl phyical addr */
#define PHY_ADDR_RGMII1_TXCKOUT	0x017C70218
#define PHY_ADDR_RGMII1_TXD0	0x017C7020C
#define PHY_ADDR_RGMII1_TXD1	0x017C70214
#define PHY_ADDR_RGMII1_TXD2	0x017C701F4
#define PHY_ADDR_RGMII1_TXD3	0x017C70208
#define PHY_ADDR_RGMII1_TXEN	0x017C70210
#define PHY_ADDR_RGMII1_RXCK	0x017C701EC
#define PHY_ADDR_RGMII1_RXD0	0x017C701E4
#define PHY_ADDR_RGMII1_RXD1	0x017C70204
#define PHY_ADDR_RGMII1_RXD2	0x017C701FC
#define PHY_ADDR_RGMII1_RXD3	0x017C701F8
#define PHY_ADDR_RGMII1_RXDV	0x017C701E8

/* RMII0 pinctrl phyical addr */
#define PHY_ADDR_RMII0_CLK		0x017C7012C
#define PHY_ADDR_RMII0_TXD0		0x017C70114
#define PHY_ADDR_RMII0_TXD1		0x017C70118
#define PHY_ADDR_RMII0_TXEN		0x017C70128
#define PHY_ADDR_RMII0_RXD0		0x017C70144
#define PHY_ADDR_RMII0_RXD1		0x017C70140
#define PHY_ADDR_RMII0_RXDV		0x017C70150

/* RMII1 pinctrl phyical addr */
#define PHY_ADDR_RMII1_CLK		0x017C70218
#define PHY_ADDR_RMII1_TXD0		0x017C7020C
#define PHY_ADDR_RMII1_TXD1		0x017C70214
#define PHY_ADDR_RMII1_TXEN		0x017C70210
#define PHY_ADDR_RMII1_RXD0		0x017C701E4
#define PHY_ADDR_RMII1_RXD1		0x017C70204
#define PHY_ADDR_RMII1_RXDV		0x017C701E8

/* MDIO0 config value */
#define VALUE_MDCK0				0x1002
#define VALUE_MDIO0				0x1002
/* MDIO1 config value */
#define VALUE_MDCK1				0x1022
#define VALUE_MDIO1				0x1032

/* PHY0 config value */
#define VALUE_EPHY0_CLK			0x1012
#define VALUE_EPHY0_RSTN		0x1002
/* PHY1 config value */
#define VALUE_EPHY1_CLK			0x1002
#define VALUE_EPHY1_RSTN		0x1002

/* RGMII0 config value */
#define VALUE_RGMII0_TXCKOUT	0x1042
#define VALUE_RGMII0_TXD0		0x1052
#define VALUE_RGMII0_TXD1		0x1052
#define VALUE_RGMII0_TXD2		0x1052
#define VALUE_RGMII0_TXD3		0x1052
#define VALUE_RGMII0_TXEN		0x1052
#define VALUE_RGMII0_RXCK		0x1002
#define VALUE_RGMII0_RXD0		0x1002
#define VALUE_RGMII0_RXD1		0x1002
#define VALUE_RGMII0_RXD2		0x1002
#define VALUE_RGMII0_RXD3		0x1002
#define VALUE_RGMII0_RXDV		0x1002

/* RGMII1 config value */
#define VALUE_RGMII1_TXCKOUT	0x1042
#define VALUE_RGMII1_TXD0		0x1052
#define VALUE_RGMII1_TXD1		0x1052
#define VALUE_RGMII1_TXD2		0x1052
#define VALUE_RGMII1_TXD3		0x1052
#define VALUE_RGMII1_TXEN		0x1052
#define VALUE_RGMII1_RXCK		0x1032
#define VALUE_RGMII1_RXD0		0x1032
#define VALUE_RGMII1_RXD1		0x1032
#define VALUE_RGMII1_RXD2		0x1032
#define VALUE_RGMII1_RXD3		0x1032
#define VALUE_RGMII1_RXDV		0x1032

/* RMII0 config value */
#define VALUE_RMII0_CLK			0x1053
#define VALUE_RMII0_TXD0		0x1052
#define VALUE_RMII0_TXD1		0x1052
#define VALUE_RMII0_TXEN		0x1052
#define VALUE_RMII0_RXD0		0x1032
#define VALUE_RMII0_RXD1		0x1032
#define VALUE_RMII0_RXDV		0x1032

/* RMII1 config value */
#define VALUE_RMII1_CLK			0x1053
#define VALUE_RMII1_TXD0		0x1052
#define VALUE_RMII1_TXD1		0x1052
#define VALUE_RMII1_TXEN		0x1052
#define VALUE_RMII1_RXD0		0x1032
#define VALUE_RMII1_RXD1		0x1032
#define VALUE_RMII1_RXDV		0x1032

#define REG_SIZE_BYTES			4

static u32 gmac_pinctrl_writel(struct platform_device *pdev, u32 val,
				 resource_size_t phyaddr)
{
	void __iomem *pinout = NULL;

	pinout = devm_ioremap(&pdev->dev, phyaddr, REG_SIZE_BYTES);
	if (!pinout) {
		pr_err("gmac devm_ioremap error!\n");
		return ENOMEM;
	}
	writel(val, pinout);
	devm_iounmap(&pdev->dev, pinout);

	return 0;
}

static int gmac_config_rgmii0(struct platform_device *pdev)
{
	unsigned int ret;

	ret = gmac_pinctrl_writel(pdev, VALUE_MDCK0, PHY_ADDR_MDCK0);
	ret |= gmac_pinctrl_writel(pdev, VALUE_MDIO0, PHY_ADDR_MDIO0);
	ret |= gmac_pinctrl_writel(pdev, VALUE_EPHY0_CLK, PHY_ADDR_EPHY0_CLK);
	ret |= gmac_pinctrl_writel(pdev, VALUE_EPHY0_RSTN, PHY_ADDR_EPHY0_RSTN);
	ret |= gmac_pinctrl_writel(pdev, VALUE_RGMII0_TXCKOUT,
				     PHY_ADDR_RGMII0_TXCKOUT);
	ret |= gmac_pinctrl_writel(pdev, VALUE_RGMII0_TXD0, PHY_ADDR_RGMII0_TXD0);
	ret |= gmac_pinctrl_writel(pdev, VALUE_RGMII0_TXD1, PHY_ADDR_RGMII0_TXD1);
	ret |= gmac_pinctrl_writel(pdev, VALUE_RGMII0_TXD2, PHY_ADDR_RGMII0_TXD2);
	ret |= gmac_pinctrl_writel(pdev, VALUE_RGMII0_TXD3, PHY_ADDR_RGMII0_TXD3);
	ret |= gmac_pinctrl_writel(pdev, VALUE_RGMII0_TXEN, PHY_ADDR_RGMII0_TXEN);
	ret |= gmac_pinctrl_writel(pdev, VALUE_RGMII0_RXCK, PHY_ADDR_RGMII0_RXCK);
	ret |= gmac_pinctrl_writel(pdev, VALUE_RGMII0_RXD0, PHY_ADDR_RGMII0_RXD0);
	ret |= gmac_pinctrl_writel(pdev, VALUE_RGMII0_RXD1, PHY_ADDR_RGMII0_RXD1);
	ret |= gmac_pinctrl_writel(pdev, VALUE_RGMII0_RXD2, PHY_ADDR_RGMII0_RXD2);
	ret |= gmac_pinctrl_writel(pdev, VALUE_RGMII0_RXD3, PHY_ADDR_RGMII0_RXD3);
	ret |= gmac_pinctrl_writel(pdev, VALUE_RGMII0_RXDV, PHY_ADDR_RGMII0_RXDV);

	return (int)ret;
}

static int gmac_config_rgmii1(struct platform_device *pdev)
{
	unsigned int ret;

	ret = gmac_pinctrl_writel(pdev, VALUE_MDCK1, PHY_ADDR_MDCK1);
	ret |= gmac_pinctrl_writel(pdev, VALUE_MDIO1, PHY_ADDR_MDIO1);
	ret |= gmac_pinctrl_writel(pdev, VALUE_EPHY1_CLK, PHY_ADDR_EPHY1_CLK);
	ret |= gmac_pinctrl_writel(pdev, VALUE_EPHY1_RSTN, PHY_ADDR_EPHY1_RSTN);
	ret |= gmac_pinctrl_writel(pdev, VALUE_RGMII1_TXCKOUT,
				     PHY_ADDR_RGMII1_TXCKOUT);
	ret |= gmac_pinctrl_writel(pdev, VALUE_RGMII1_TXD0, PHY_ADDR_RGMII1_TXD0);
	ret |= gmac_pinctrl_writel(pdev, VALUE_RGMII1_TXD1, PHY_ADDR_RGMII1_TXD1);
	ret |= gmac_pinctrl_writel(pdev, VALUE_RGMII1_TXD2, PHY_ADDR_RGMII1_TXD2);
	ret |= gmac_pinctrl_writel(pdev, VALUE_RGMII1_TXD3, PHY_ADDR_RGMII1_TXD3);
	ret |= gmac_pinctrl_writel(pdev, VALUE_RGMII1_TXEN, PHY_ADDR_RGMII1_TXEN);
	ret |= gmac_pinctrl_writel(pdev, VALUE_RGMII1_RXCK, PHY_ADDR_RGMII1_RXCK);
	ret |= gmac_pinctrl_writel(pdev, VALUE_RGMII1_RXD0, PHY_ADDR_RGMII1_RXD0);
	ret |= gmac_pinctrl_writel(pdev, VALUE_RGMII1_RXD1, PHY_ADDR_RGMII1_RXD1);
	ret |= gmac_pinctrl_writel(pdev, VALUE_RGMII1_RXD2, PHY_ADDR_RGMII1_RXD2);
	ret |= gmac_pinctrl_writel(pdev, VALUE_RGMII1_RXD3, PHY_ADDR_RGMII1_RXD3);
	ret |= gmac_pinctrl_writel(pdev, VALUE_RGMII1_RXDV, PHY_ADDR_RGMII1_RXDV);

	return (int)ret;
}

static int gmac_config_rmii0(struct platform_device *pdev)
{
	unsigned int ret;

	ret = gmac_pinctrl_writel(pdev, VALUE_MDCK0, PHY_ADDR_MDCK0);
	ret |= gmac_pinctrl_writel(pdev, VALUE_MDIO0, PHY_ADDR_MDIO0);
	ret |= gmac_pinctrl_writel(pdev, VALUE_EPHY0_CLK, PHY_ADDR_EPHY0_CLK);
	ret |= gmac_pinctrl_writel(pdev, VALUE_EPHY0_RSTN, PHY_ADDR_EPHY0_RSTN);
	ret |= gmac_pinctrl_writel(pdev, VALUE_RMII0_CLK, PHY_ADDR_RMII0_CLK);
	ret |= gmac_pinctrl_writel(pdev, VALUE_RMII0_TXD0, PHY_ADDR_RMII0_TXD0);
	ret |= gmac_pinctrl_writel(pdev, VALUE_RMII0_TXD1, PHY_ADDR_RMII0_TXD1);
	ret |= gmac_pinctrl_writel(pdev, VALUE_RMII0_TXEN, PHY_ADDR_RMII0_TXEN);
	ret |= gmac_pinctrl_writel(pdev, VALUE_RMII0_RXD0, PHY_ADDR_RMII0_RXD0);
	ret |= gmac_pinctrl_writel(pdev, VALUE_RMII0_RXD1, PHY_ADDR_RMII0_RXD1);
	ret |= gmac_pinctrl_writel(pdev, VALUE_RMII0_RXDV, PHY_ADDR_RMII0_RXDV);

	return (int)ret;
}

static int gmac_config_rmii1(struct platform_device *pdev)
{
	unsigned int ret;

	ret = gmac_pinctrl_writel(pdev, VALUE_MDCK1, PHY_ADDR_MDCK1);
	ret |= gmac_pinctrl_writel(pdev, VALUE_MDIO1, PHY_ADDR_MDIO1);
	ret |= gmac_pinctrl_writel(pdev, VALUE_EPHY1_CLK, PHY_ADDR_EPHY1_CLK);
	ret |= gmac_pinctrl_writel(pdev, VALUE_EPHY1_RSTN, PHY_ADDR_EPHY1_RSTN);
	ret |= gmac_pinctrl_writel(pdev, VALUE_RMII1_CLK, PHY_ADDR_RMII1_CLK);
	ret |= gmac_pinctrl_writel(pdev, VALUE_RMII1_TXD0, PHY_ADDR_RMII1_TXD0);
	ret |= gmac_pinctrl_writel(pdev, VALUE_RMII1_TXD1, PHY_ADDR_RMII1_TXD1);
	ret |= gmac_pinctrl_writel(pdev, VALUE_RMII1_TXEN, PHY_ADDR_RMII1_TXEN);
	ret |= gmac_pinctrl_writel(pdev, VALUE_RMII1_RXD0, PHY_ADDR_RMII1_RXD0);
	ret |= gmac_pinctrl_writel(pdev, VALUE_RMII1_RXD1, PHY_ADDR_RMII1_RXD1);
	ret |= gmac_pinctrl_writel(pdev, VALUE_RMII1_RXDV, PHY_ADDR_RMII1_RXDV);

	return (int)ret;
}

int bsp_gemac_pinctrl_config(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	const char *pm = NULL;
	int err;

	err = of_property_read_string(np, "pinctrl", &pm);
	if (err) {
		/* Just print warring, mybe pinctrl config in uboot */
		pr_warning("gmac have no pinctrl config\n");
		return 0;
	}

	pr_info("gmac pinctrl type=%s\n", pm);
	if (!strcasecmp(pm, "rgmii0"))
		err = gmac_config_rgmii0(pdev);
	else if (!strcasecmp(pm, "rgmii1"))
		err = gmac_config_rgmii1(pdev);
	else if (!strcasecmp(pm, "rmii0"))
		err = gmac_config_rmii0(pdev);
	else if (!strcasecmp(pm, "rmii1"))
		err = gmac_config_rmii1(pdev);
	else
		return -EINVAL;

	return err;
}
