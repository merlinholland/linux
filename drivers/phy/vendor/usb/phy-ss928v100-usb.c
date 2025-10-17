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
#include <linux/delay.h>
#include <linux/of_address.h>
#include <linux/phy/phy.h>
#include <linux/usb/ch9.h>

#include "phy-bsp-usb.h"

#define PINOUT_REG_BASE					(0x10230000)
#define PITOUT_CTRL0_PWREN_OFFSET		(0x44)
#define PITOUT_CTRL1_PWREN_OFFSET		(0X3C)
#define PITOUT_CTRL1_VBUS_OFFSET		(0x38)
#define PINOUT_USB_VAL					(0x1201)

#define PCIE_X2_MODE			(0x0 << 16)
#define USB3_MODE				(0x1 << 16)
#define PORT0U2_PORT1U3_MODE	(0x2 << 16)
#define COMBPHY_MODE_MASK		(0x3 << 16)
#define SYSSTAT					0x18

#define USB3_CTRL_CRG		0x3940
#define USB3_CTRL_CRG_1		0x3960
#define USB2_PHY_CRG		0x38c0
#define USB2_PHY_CRG_1		0x38e0
#define USB3_PHY_CRG		0x3944
#define USB3_PHY_CRG_1		0x3964
#define USB3_U2_PHY_ADDR	0x10310000
#define USB3_U2_PHY_ADDR_1	0x10330000
#define USB3_CTRL_ADDR		0x10300000
#define USB3_CTRL_ADDR_1		0x10320000

#define USB3_CTRL_CRG_DEFAULT_VALUE 	0x30001
#define USB2_PHY_CRG_DEFAULT_VALUE		0x57
#define USB3_PHY_CRG_DEFAULT_VALUE		0x13

#define USB3_CRG_PCLK_OCC_SEL			(0x1 << 18)
#define USB3_CRG_PIPE_CKEN			(0x1 << 12)
#define USB3_CRG_UTMI_CKEN			(0x1 << 8)
#define USB3_CRG_SUSPEND_CKEN			(0x1 << 6)
#define USB3_CRG_REF_CKEN			(0x1 << 5)
#define USB3_CRG_BUS_CKEN			(0x1 << 4)
#define USB3_CRG_SRST_REQ			(0x1 << 0)

#define USB2_PHY_CRG_APB_SREQ			(0x1 << 2)
#define USB2_PHY_CRG_TREQ			(0x1 << 1)
#define USB2_PHY_CRG_REQ			(0x1 << 0)

#define USB3_PHY_CRG_TREQ			(0x1 << 1)
#define USB3_PHY_CRG_REQ			(0x1 << 0)

#define COMBPHY_REF_CKEN			(0x1<<24)
#define COMBPHY_SRST_REQ			(0x1<<16)

#define USB3_VCC_SRST_REQ		(0x1<<0)
#define USB3_UTMI_CKSEL			(0x1<<13)
#define USB3_PCLK_OCC_SEL		(0x1<<14)

#define USB2_PHY_PLLCK_ADDR_OFFSET	0x14
#define USB2_PHY_PLLCK_MASK			0x00000003
#define USB2_PHY_PLLCK_VAL			((0x3 << 0) & USB2_PHY_PLLCK_MASK)

#define GTXTHRCFG	0xc108
#define GRXTHRCFG	0xc10c
#define REG_GCTL	0xc110

#define PORT_CAP_DIR		(0x3 << 12)
#define DEFAULT_HOST_MOD	(0x1 << 12)

#define USB_TXPKT_CNT_SEL		(0x1 << 29)
#define USB_TXPKT_CNT			(0x11 << 24)
#define USB_MAXTX_BURST_SIZE	(0x1 << 20)
#define CLEAN_USB3_GTXTHRCFG	0x0

#define REG_GUSB3PIPECTL0		0xc2c0
#define PCS_SSP_SOFT_RESET		(0x1 << 31)
#define SUSPEND_USB3_SS_PHY		(0x1 << 17)
#define USB3_TX_MARGIN_VAL		0x10c0012

#define USB3_GUSB2PHYCFGN	0xc200
#define USB3_SUSPENDUSB20_PHY		(0x1 << 6)

#define ANA_CFG0_OFFSET					(0x0)
#define TX_DEEMPHASIS_ENABLE			(0x1 << 5)
#define TX_DEEMPHASIS_STRENGTH_MASK		(0xF << 8)
#define TX_DEEMPHASIS_STRENGTH_VAL		(0xC << 8)
#define MBIAS_MASK						(0xF << 0)
#define MBIAS_VAL						(0xB << 0)
#define ANA_CFG2_OFFSET					(0x8)
#define DEEMPHASIS_HALF_BIT_MASK		(0xFF << 20)
#define DEEMPHASIS_HALF_BIT_VAL			(0x2 << 20)
#define DISCONNECT_VREF_MASK			(0x7 << 16)
#define DISCONNECT_VREF_VAL				(0x6 << 16)

static combphy_mode mode_flag;

static void usb_pinout_cfg(void)
{
	void __iomem *pinout_regbase = ioremap_nocache(PINOUT_REG_BASE, __64K__);
	writel(PINOUT_USB_VAL, pinout_regbase + PITOUT_CTRL0_PWREN_OFFSET);
	writel(PINOUT_USB_VAL, pinout_regbase + PITOUT_CTRL1_PWREN_OFFSET);
	writel(PINOUT_USB_VAL, pinout_regbase + PITOUT_CTRL1_VBUS_OFFSET);
	udelay(U_LEVEL6);
	iounmap(pinout_regbase);
}

static void get_combphy_mode(struct phy *phy)
{
	unsigned int reg;
	struct bsp_priv *priv = phy_get_drvdata(phy);

	reg = readl(priv->sys_ctrl + SYSSTAT);
	reg &= COMBPHY_MODE_MASK;

	switch (reg) {
	case PCIE_X2_MODE:
		mode_flag = PCIE_X2;
		break;
	case PORT0U2_PORT1U3_MODE:
		mode_flag = PCIE_X1;
		break;
	case USB3_MODE:
		mode_flag = USB3;
		break;
	default:
		break;
	}
}

void usb_phy_reset(void __iomem *ctrl_crg_base, void __iomem *u2phy_crg_base,
	void __iomem *u3phy_crg_base, void __iomem *u2_phy_base)
{
	unsigned int reg;

	/* U3 phy TPOR &POR reset */
	reg = readl(u3phy_crg_base);
	reg &= ~(USB3_PHY_CRG_TREQ | USB3_PHY_CRG_REQ);
	writel(reg, u3phy_crg_base);
	udelay(U_LEVEL6); // delay 200us

	/* U2 phy POR reset */
	reg = readl(u2phy_crg_base);
	reg &= ~USB2_PHY_CRG_REQ;
	writel(reg, u2phy_crg_base);

	reg = readl(u2_phy_base + USB2_PHY_PLLCK_ADDR_OFFSET);
	reg &= ~USB2_PHY_PLLCK_MASK;
	reg |= USB2_PHY_PLLCK_VAL;
	writel(reg, u2_phy_base + USB2_PHY_PLLCK_ADDR_OFFSET);
	mdelay(M_LEVEL1); // delay 2ms

	/* U2 phy TPOR reset */
	reg = readl(u2phy_crg_base);
	reg &= ~USB2_PHY_CRG_TREQ;
	writel(reg, u2phy_crg_base);
	udelay(U_LEVEL6); // delay 200us
}

static void usb3_crg_config(struct phy *phy, int index)
{
	unsigned int reg;
	struct bsp_priv *priv = phy_get_drvdata(phy);
	void __iomem	*ctrl_crg_base = NULL;
	void __iomem	*u2phy_crg_base = NULL;
	void __iomem	*u3phy_crg_base = NULL;
	void __iomem	*u2_phy_base = NULL;

	if (index == 0)  {
		ctrl_crg_base = priv->peri_crg + USB3_CTRL_CRG;
		u2phy_crg_base = priv->peri_crg + USB2_PHY_CRG;
		u3phy_crg_base = priv->peri_crg + USB3_PHY_CRG;
		u2_phy_base = ioremap_nocache(USB3_U2_PHY_ADDR, __64K__);
	} else if (index == 1) {
		ctrl_crg_base = priv->peri_crg + USB3_CTRL_CRG_1;
		u2phy_crg_base = priv->peri_crg + USB2_PHY_CRG_1;
		u3phy_crg_base = priv->peri_crg + USB3_PHY_CRG_1;
		u2_phy_base = ioremap_nocache(USB3_U2_PHY_ADDR_1, __64K__);
	} else {
		return;
	}
	if (u2_phy_base == NULL)
		return;
	/* enable port0 ss */

	/* write default crg value */
	writel(USB3_CTRL_CRG_DEFAULT_VALUE, ctrl_crg_base);
	writel(USB2_PHY_CRG_DEFAULT_VALUE, u2phy_crg_base);
	writel(USB3_PHY_CRG_DEFAULT_VALUE, u3phy_crg_base);
	udelay(U_LEVEL6);

	/* phy crg setting */
	reg = readl(u2phy_crg_base);
	reg &= ~(USB2_PHY_CRG_APB_SREQ);
	writel(reg, u2phy_crg_base);
	udelay(U_LEVEL6);

	/* ctrl crg setting */
	/* usb3 occ pclk sel */
	reg = readl(ctrl_crg_base);
	if (mode_flag == PCIE_X2)
		reg |= USB3_CRG_PCLK_OCC_SEL;
	else if (mode_flag == PCIE_X1 && index == 0)
		reg |= USB3_CRG_PCLK_OCC_SEL;
	else
		reg &= ~(USB3_CRG_PCLK_OCC_SEL);

	reg |= (USB3_CRG_PIPE_CKEN | USB3_CRG_UTMI_CKEN |
			USB3_CRG_SUSPEND_CKEN | USB3_CRG_REF_CKEN | USB3_CRG_BUS_CKEN);
	writel(reg, ctrl_crg_base);
	udelay(U_LEVEL6); // delay 200us

	usb_phy_reset(ctrl_crg_base, u2phy_crg_base,
		u3phy_crg_base, u2_phy_base);

	/* ctrl crg reset release*/
	reg = readl(ctrl_crg_base);
	reg &= ~USB3_CRG_SRST_REQ;
	writel(reg, ctrl_crg_base);
	udelay(U_LEVEL6); // delay 200us

	iounmap(u2_phy_base);
	u2_phy_base = NULL;
}

static void usb3_ctrl_config(struct phy *phy, int index)
{
	unsigned int reg;
	void __iomem	*ctrl_base;

	if (index == 0) {
		ctrl_base = ioremap_nocache(USB3_CTRL_ADDR, __64K__);
	} else if (index == 1) {
		ctrl_base = ioremap_nocache(USB3_CTRL_ADDR_1, __64K__);
	} else {
		return;
	}

	reg = readl(ctrl_base + USB3_GUSB2PHYCFGN);
	if (mode_flag == PCIE_X2)
		reg &= ~(USB3_SUSPENDUSB20_PHY);
	else if (mode_flag == PCIE_X1 && index == 0)
		reg &= ~(USB3_SUSPENDUSB20_PHY);
	else
		reg |= (USB3_SUSPENDUSB20_PHY);
	writel(reg, ctrl_base + USB3_GUSB2PHYCFGN);
	udelay(U_LEVEL6);

	reg = readl(ctrl_base + REG_GUSB3PIPECTL0);
	reg |= PCS_SSP_SOFT_RESET;
	writel(reg, ctrl_base + REG_GUSB3PIPECTL0);
	udelay(U_LEVEL6);

	reg = readl(ctrl_base + REG_GCTL);
	reg &= ~PORT_CAP_DIR;
	reg |= DEFAULT_HOST_MOD; /* [13:12] 01: Host; 10: Device; 11: OTG */
	writel(reg, ctrl_base + REG_GCTL);
	udelay(U_LEVEL2);

	reg = readl(ctrl_base + REG_GUSB3PIPECTL0);
	reg &= ~PCS_SSP_SOFT_RESET;
	reg &= ~SUSPEND_USB3_SS_PHY;  /* disable suspend */
	writel(reg, ctrl_base + REG_GUSB3PIPECTL0);
	udelay(U_LEVEL2);

	reg &= CLEAN_USB3_GTXTHRCFG;
	reg |= USB_TXPKT_CNT_SEL;
	reg |= USB_TXPKT_CNT;
	reg |= USB_MAXTX_BURST_SIZE;
	writel(reg, ctrl_base + GTXTHRCFG);
	udelay(U_LEVEL2);
	writel(reg, ctrl_base + GRXTHRCFG);
	udelay(U_LEVEL2);

	iounmap(ctrl_base);
}

static void usb3_eye_config(struct phy *phy)
{
	unsigned int reg;
	void __iomem	*u2_phy0_base = ioremap_nocache(USB3_U2_PHY_ADDR, __64K__);
	void __iomem	*u2_phy1_base = ioremap_nocache(USB3_U2_PHY_ADDR_1, __64K__);
	if (u2_phy0_base == NULL)
		return;
	if (u2_phy1_base == NULL)
		return;

	reg = readl(u2_phy0_base + ANA_CFG0_OFFSET);
	reg |= TX_DEEMPHASIS_ENABLE;
	reg &= ~(TX_DEEMPHASIS_STRENGTH_MASK | MBIAS_MASK);
	reg |= (TX_DEEMPHASIS_STRENGTH_VAL | MBIAS_VAL);
	writel(reg, u2_phy0_base + ANA_CFG0_OFFSET);
	udelay(U_LEVEL6);

	reg = readl(u2_phy0_base + ANA_CFG2_OFFSET);
	reg &= ~(DEEMPHASIS_HALF_BIT_MASK | DISCONNECT_VREF_MASK);
	reg |= (DEEMPHASIS_HALF_BIT_VAL | DISCONNECT_VREF_VAL);
	writel(reg, u2_phy0_base + ANA_CFG2_OFFSET);
	udelay(U_LEVEL6);

	reg = readl(u2_phy1_base + ANA_CFG0_OFFSET);
	reg |= TX_DEEMPHASIS_ENABLE;
	reg &= ~(TX_DEEMPHASIS_STRENGTH_MASK | MBIAS_MASK);
	reg |= (TX_DEEMPHASIS_STRENGTH_VAL | MBIAS_VAL);
	writel(reg, u2_phy1_base + ANA_CFG0_OFFSET);
	udelay(U_LEVEL6);

	reg = readl(u2_phy1_base + ANA_CFG2_OFFSET);
	reg &= ~(DEEMPHASIS_HALF_BIT_MASK | DISCONNECT_VREF_MASK);
	reg |= (DEEMPHASIS_HALF_BIT_VAL | DISCONNECT_VREF_VAL);
	writel(reg, u2_phy1_base + ANA_CFG2_OFFSET);
	udelay(U_LEVEL6);

	iounmap(u2_phy0_base);
	iounmap(u2_phy1_base);
}

void bsp_usb3_phy_on(struct phy *phy)
{
	usb_pinout_cfg();
	udelay(U_LEVEL6);
	get_combphy_mode(phy);
	usb3_crg_config(phy, 0);
	usb3_ctrl_config(phy, 0);

	usb3_crg_config(phy, 1);
	usb3_ctrl_config(phy, 1);

	usb3_eye_config(phy);
}
EXPORT_SYMBOL(bsp_usb3_phy_on);

void bsp_usb3_phy_off(struct phy *phy)
{
	struct bsp_priv *priv = phy_get_drvdata(phy);
	/* write default crg value */
	writel(USB3_CTRL_CRG_DEFAULT_VALUE, priv->peri_crg + USB3_CTRL_CRG);
	writel(USB2_PHY_CRG_DEFAULT_VALUE, priv->peri_crg + USB2_PHY_CRG);
	writel(USB3_PHY_CRG_DEFAULT_VALUE, priv->peri_crg + USB3_PHY_CRG);
	udelay(U_LEVEL6);

	writel(USB3_CTRL_CRG_DEFAULT_VALUE, priv->peri_crg + USB3_CTRL_CRG_1);
	writel(USB2_PHY_CRG_DEFAULT_VALUE, priv->peri_crg + USB2_PHY_CRG_1);
	writel(USB3_PHY_CRG_DEFAULT_VALUE, priv->peri_crg + USB3_PHY_CRG_1);
	udelay(U_LEVEL6);
}
EXPORT_SYMBOL(bsp_usb3_phy_off);
