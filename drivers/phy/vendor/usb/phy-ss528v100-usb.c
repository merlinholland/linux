/*
*
* Copyright (c) 2012-2021 Shenshu Technologies Co., Ltd.
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
#include <linux/phy/phy.h>
#include <linux/of_address.h>
#include <linux/usb/ch9.h>
#include "phy-bsp-usb.h"

#define PERI_CRG3632		0x0
#define	USB2_0_UTMI_CKEN	(0x1 << 8)

#define PERI_CRG3636		0x10
#define	USB2_PHY0_REQ		(0x1 << 0)
#define	USB2_PHY0_TREQ		(0x1 << 1)
#define	USB2_PHY0_APB_SRST_REQ	(0x1 << 2)
#define	USB2_PHY0_XTAL_CKEN	(0x1 << 4)

#define PERI_CRG3640		0x20
#define USB2_1_SRST_REQ		(0x1 << 0)
#define	USB2_1_BUS_CKEN		(0x1 << 4)
#define	USB2_1_REF_CKEN		(0x1 << 5)
#define	USB2_1_UTMI_CKEN	(0x1 << 8)

#define PERI_CRG3644		0x30
#define	USB2_PHY1_REQ		(0x1 << 0)
#define	USB2_PHY1_TREQ		(0x1 << 1)
#define	USB2_PHY1_APB_SRST_REQ	(0x1 << 2)
#define	USB2_PHY1_XTAL_CKEN	(0x1 << 4)

#define PERI_CRG3664		0x80
#define	USB3_SRST_REQ		(0x1 << 0)
#define	USB3_BUS_CKEN		(0x1 << 4)
#define	USB3_REF_CKEN		(0x1 << 5)
#define	USB3_SUSPEND_CKEN	(0x1 << 6)
#define	USB3_UTMI_CKEN		(0x1 << 8)
#define	USB3_PIPE_CKEN		(0x1 << 12)

#define PERI_CRG3672		0xa0
#define	USB2_PHY2_REQ		(0x1 << 0)
#define	USB2_PHY2_TREQ		(0x1 << 1)
#define	USB2_PHY2_APB_SRST_REQ	(0x1 << 2)
#define	USB2_PHY2_XTAL_CKEN	(0x1 << 4)

#define PERI_CRG3676		0xb0
#define	COMBPHY0_SRST_REQ	(0x1 << 0)
#define	COMBPHY0_TEST_SRST_REQ	(0x1 << 1)
#define	COMBPHY0_REF_CKEN	(0x1 << 4)

#define USB_CTRL6	0xc
#define U3_PORT_DISABLE	(0x1 << 12)

#define USB2_PHY2_BASE		0x10310000
#define USB2_PHY1_BASE		0x10350000
#define USB2_PHY0_BASE		0x10330000

#define U2_ANA_CFG0		0x0
#define HSTX_MBIAS_MASK	(0xf << 0)
#define ana_cfg0_val(p)	((p) & (~HSTX_MBIAS_MASK))
#define U2_2_HSTX_MBIAS	(0x3 << 0)
#define U2_1_HSTX_MBIAS	(0x3 << 0)
#define U2_0_HSTX_MBIAS	(0x3 << 0)

#define U2_2_HSTX_DEEN	(0x1 << 5)
#define U2_1_HSTX_DEEN	(0x1 << 5)
#define U2_0_HSTX_DEEN	(0x1 << 5)

#define HSTX_DE_MASK	(0xf << 8)
#define U2_2_HSTX_DE	(0x8 << 8)
#define U2_1_HSTX_DE	(0x8 << 8)
#define U2_0_HSTX_DE	(0x8 << 8)

#define U2_ANA_CFG2		0x8
#define VDISCREF_SEL_MASK	(0x7 << 16)
#define ana_cfg2_val(p)	((p) & (~VDISCREF_SEL_MASK))
#define U2_2_VDISCREF_SEL	(0x6 << 16)
#define U2_1_VDISCREF_SEL	(0x6 << 16)
#define U2_0_VDISCREF_SEL	(0x6 << 16)
#define U2_TEST_TX		(0x1 << 20)
#define U2_TEST_TX_HALT_DEEN	(0x1 << 21)

#define U2_TRIM_VAL_MIN	0x09
#define U2_TRIM_VAL_MAX	0x1d
#define RT_TRIM_VAL_MASK	0x1f
#define usb2_2_trim_val(p)	(((p) >> 10) & RT_TRIM_VAL_MASK)
#define usb2_1_trim_val(p)	(((p) >> 5) & RT_TRIM_VAL_MASK)
#define usb2_0_trim_val(p)	(((p) >> 0) & RT_TRIM_VAL_MASK)

#define usb2_rt_trim_clr(p)	((p) & (~(RT_TRIM_VAL_MASK << 8)))
#define usb2_rt_trim_set(p)	((p) << 8)

#define U2_ANA_CFG3		0xc
#define SLEW_RATE_OPT_MASK	(0x3 << 20)
#define ana_cfg3_val(p)	((p) & (~SLEW_RATE_OPT_MASK))
#define U2_2_SLEW_RATE_OPT	(0x1 << 20)
#define U2_1_SLEW_RATE_OPT	(0x1 << 20)
#define U2_0_SLEW_RATE_OPT	(0x1 << 20)

#define U2_ANA_CFG4		0x10
#define VTXREF_SEL_MASK	(0x7 << 4)
#define ana_cfg4_val(p)	((p) & (~VTXREF_SEL_MASK))
#define U2_VTXREF_SEL		(0x5 << 4)
#define U2_FLS_EDGE_MODE	(0x1 << 13)
#define U2_VTXREF_SEL_U3P	(0x6 << 4)

#define COMBPHY_CTRL0	0x40
#define PI_CURRENT_TRIM_ENABLE	0x11100
#define PI_CURRENT_TRIM_VAL	0x11101
#define TX_SWING_COMP_ENABLE	0xc1200
#define TX_SWING_COMP_VAL	0xc1201

#define	PHY_PLL_ENABLE		(0x3 << 0)
#define	PHY_PLL_OFFSET		0x14

static void bsp_usb2_phy1_eye_config(struct phy *phy)
{
	unsigned int reg;
	unsigned int trim_val;
	void __iomem *usb2_phy1 = NULL;
	struct bsp_priv *priv = phy_get_drvdata(phy);

	usb2_phy1 = ioremap_nocache(USB2_PHY1_BASE, __64K__);
	if (usb2_phy1 == NULL)
		return;

	/* adjust the hstx mbias deen de */
	reg = readl(usb2_phy1 + U2_ANA_CFG0);
	reg = ana_cfg0_val(reg);
	reg |= U2_1_HSTX_MBIAS;
	reg |= U2_1_HSTX_DEEN;
	reg &= ~HSTX_DE_MASK;
	reg |= U2_1_HSTX_DE;
	writel(reg, usb2_phy1 + U2_ANA_CFG0);
	udelay(U_LEVEL5);

	/* vdiscref sel and test tx set */
	reg = readl(usb2_phy1 + U2_ANA_CFG2);
	reg = ana_cfg2_val(reg);
	reg |= U2_1_VDISCREF_SEL;
	reg |= U2_TEST_TX;
	reg |= U2_TEST_TX_HALT_DEEN;
	writel(reg, usb2_phy1 + U2_ANA_CFG2);
	udelay(U_LEVEL5);

	/* OTP usb2 phy1 */
	trim_val = readl(priv->sys_ctrl);
	trim_val = usb2_1_trim_val(trim_val);
	if ((trim_val >= U2_TRIM_VAL_MIN) && (trim_val <= U2_TRIM_VAL_MAX)) {
		reg = readl(usb2_phy1 + U2_ANA_CFG2);
		reg = usb2_rt_trim_clr(reg);
		reg |= usb2_rt_trim_set(trim_val);
		writel(reg, usb2_phy1 + U2_ANA_CFG2);
		udelay(U_LEVEL5);
	}

	/* ATOP TEST bit */
	reg = readl(usb2_phy1 + U2_ANA_CFG3);
	reg = ana_cfg3_val(reg);
	reg |= U2_1_SLEW_RATE_OPT;
	writel(reg, usb2_phy1 + U2_ANA_CFG3);
	udelay(U_LEVEL5);

	/* vtxref sel==>430mV, enable fls edge mode */
	reg = readl(usb2_phy1 + U2_ANA_CFG4);
	reg = ana_cfg4_val(reg);
	reg |= U2_VTXREF_SEL;
	reg |= U2_FLS_EDGE_MODE;
	writel(reg, usb2_phy1 + U2_ANA_CFG4);
	udelay(U_LEVEL5);

	iounmap(usb2_phy1);
}

void bsp_usb2_phy1_config(struct phy *phy)
{
	unsigned int reg;
	void __iomem *usb2_phy1 = NULL;

	usb2_phy1 = ioremap_nocache(USB2_PHY1_BASE, __64K__);
	if (usb2_phy1 == NULL)
		return;

	/* usb2 phy1 pll enable */
	reg = readl(usb2_phy1 + PHY_PLL_OFFSET);
	reg |= PHY_PLL_ENABLE;
	writel(reg, usb2_phy1 + PHY_PLL_OFFSET);
	udelay(U_LEVEL5);

	iounmap(usb2_phy1);
}

void bsp_usb_crg_config(struct phy *phy)
{
	unsigned int reg;
	struct bsp_priv *priv = phy_get_drvdata(phy);

	/* ctrl1 rst */
	reg = readl(priv->peri_crg + PERI_CRG3640);
	reg |= USB2_1_SRST_REQ;
	writel(reg, priv->peri_crg + PERI_CRG3640);
	udelay(U_LEVEL6);

	/* usb2 phy1 rst */
	reg = readl(priv->peri_crg + PERI_CRG3644);
	reg |= (USB2_PHY1_REQ | USB2_PHY1_TREQ | USB2_PHY1_APB_SRST_REQ);
	writel(reg, priv->peri_crg + PERI_CRG3644);
	udelay(U_LEVEL6);

	/* open usb2 phy1 clk */
	reg = readl(priv->peri_crg + PERI_CRG3644);
	reg |= USB2_PHY1_XTAL_CKEN;
	writel(reg, priv->peri_crg + PERI_CRG3644);
	udelay(U_LEVEL6);

	/* cancel usb2 phy1 rst */
	reg = readl(priv->peri_crg + PERI_CRG3644);
	reg &= ~(USB2_PHY1_REQ | USB2_PHY1_TREQ | USB2_PHY1_APB_SRST_REQ);
	writel(reg, priv->peri_crg + PERI_CRG3644);
	mdelay(M_LEVEL1);

	/* open utmi/ref/bus clk */
	reg = readl(priv->peri_crg + PERI_CRG3640);
	reg |= USB2_1_BUS_CKEN;
	reg |= USB2_1_REF_CKEN;
	reg |= USB2_1_UTMI_CKEN;
	writel(reg, priv->peri_crg + PERI_CRG3640);
	udelay(U_LEVEL6);

	/* cancel ctrl1 rst */
	reg = readl(priv->peri_crg + PERI_CRG3640);
	reg &= ~USB2_1_SRST_REQ;
	writel(reg, priv->peri_crg + PERI_CRG3640);
	udelay(U_LEVEL6);
}

void bsp_usb_phy_on(struct phy *phy)
{
	bsp_usb_crg_config(phy);

	bsp_usb2_phy1_config(phy);

	bsp_usb2_phy1_eye_config(phy);
}
EXPORT_SYMBOL(bsp_usb_phy_on);

void bsp_usb_phy_off(struct phy *phy)
{
	unsigned int reg;
	struct bsp_priv *priv = phy_get_drvdata(phy);

	/* ctrl1 rst */
	reg = readl(priv->peri_crg + PERI_CRG3640);
	reg |= USB2_1_SRST_REQ;
	writel(reg, priv->peri_crg + PERI_CRG3640);
	udelay(U_LEVEL6);

	/* usb2 phy1 rst */
	reg = readl(priv->peri_crg + PERI_CRG3644);
	reg |= (USB2_PHY1_REQ | USB2_PHY1_TREQ | USB2_PHY1_APB_SRST_REQ);
	writel(reg, priv->peri_crg + PERI_CRG3644);
	udelay(U_LEVEL6);
}
EXPORT_SYMBOL(bsp_usb_phy_off);

static void bsp_usb2_phy0_eye_config(struct phy *phy)
{
	unsigned int reg;
	unsigned int trim_val;
	void __iomem *usb2_phy0 = NULL;
	struct bsp_priv *priv = phy_get_drvdata(phy);

	usb2_phy0 = ioremap_nocache(USB2_PHY0_BASE, __64K__);
	if (usb2_phy0 == NULL)
		return;

	/* adjust the hstx mbias deen DE */
	reg = readl(usb2_phy0 + U2_ANA_CFG0);
	reg = ana_cfg0_val(reg);
	reg |= U2_0_HSTX_MBIAS;
	reg |= U2_0_HSTX_DEEN;
	reg &= ~HSTX_DE_MASK;
	reg |= U2_0_HSTX_DE;
	writel(reg, usb2_phy0 + U2_ANA_CFG0);
	udelay(U_LEVEL5);

	/* vdiscref sel and test tx set */
	reg = readl(usb2_phy0 + U2_ANA_CFG2);
	reg = ana_cfg2_val(reg);
	reg |= U2_0_VDISCREF_SEL;
	reg |= U2_TEST_TX;
	reg |= U2_TEST_TX_HALT_DEEN;
	writel(reg, usb2_phy0 + U2_ANA_CFG2);
	udelay(U_LEVEL5);

	/* OTP usb2 phy0 */
	trim_val = readl(priv->sys_ctrl);
	trim_val = usb2_0_trim_val(trim_val);
	if ((trim_val >= U2_TRIM_VAL_MIN) && (trim_val <= U2_TRIM_VAL_MAX)) {
		reg = readl(usb2_phy0 + U2_ANA_CFG2);
		reg = usb2_rt_trim_clr(reg);
		reg |= usb2_rt_trim_set(trim_val);
		writel(reg, usb2_phy0 + U2_ANA_CFG2);
		udelay(U_LEVEL5);
	}

	/* ATOP TEST bit */
	reg = readl(usb2_phy0 + U2_ANA_CFG3);
	reg = ana_cfg3_val(reg);
	reg |= U2_0_SLEW_RATE_OPT;
	writel(reg, usb2_phy0 + U2_ANA_CFG3);
	udelay(U_LEVEL5);

	/* vtxref sel==>430mV, enable fls edge mode */
	reg = readl(usb2_phy0 + U2_ANA_CFG4);
	reg = ana_cfg4_val(reg);
	reg |= U2_VTXREF_SEL;
	reg |= U2_FLS_EDGE_MODE;
	writel(reg, usb2_phy0 + U2_ANA_CFG4);
	udelay(U_LEVEL5);

	iounmap(usb2_phy0);
}

void bsp_usb2_phy0_config(struct phy *phy)
{
	unsigned int reg;
	void __iomem *usb2_phy0 = NULL;

	usb2_phy0 = ioremap_nocache(USB2_PHY0_BASE, __64K__);
	if (usb2_phy0 == NULL)
		return;

	/* usb2 phy0 pll enable */
	reg = readl(usb2_phy0 + PHY_PLL_OFFSET);
	reg |= PHY_PLL_ENABLE;
	writel(reg, usb2_phy0 + PHY_PLL_OFFSET);
	udelay(U_LEVEL5);

	iounmap(usb2_phy0);
}

static void bsp_usb2_phy2_eye_config(struct phy *phy)
{
	unsigned int reg;
	unsigned int trim_val;
	void __iomem *usb2_phy2 = NULL;
	struct bsp_priv *priv = phy_get_drvdata(phy);

	usb2_phy2 = ioremap_nocache(USB2_PHY2_BASE, __64K__);
	if (usb2_phy2 == NULL)
		return;

	/* adjust the hstx mbias deen de */
	reg = readl(usb2_phy2 + U2_ANA_CFG0);
	reg = ana_cfg0_val(reg);
	reg |= U2_2_HSTX_MBIAS;
	reg |= U2_2_HSTX_DEEN;
	reg &= ~HSTX_DE_MASK;
	reg |= U2_2_HSTX_DE;
	writel(reg, usb2_phy2 + U2_ANA_CFG0);
	udelay(U_LEVEL5);

	/* vdiscref sel and test tx set */
	reg = readl(usb2_phy2 + U2_ANA_CFG2);
	reg = ana_cfg2_val(reg);
	reg |= U2_2_VDISCREF_SEL;
	reg |= U2_TEST_TX;
	reg |= U2_TEST_TX_HALT_DEEN;
	writel(reg, usb2_phy2 + U2_ANA_CFG2);
	udelay(U_LEVEL5);

	/* OTP usb2 phy2 */
	trim_val = readl(priv->sys_ctrl);
	trim_val = usb2_2_trim_val(trim_val);
	if ((trim_val >= U2_TRIM_VAL_MIN) && (trim_val <= U2_TRIM_VAL_MAX)) {
		reg = readl(usb2_phy2 + U2_ANA_CFG2);
		reg = usb2_rt_trim_clr(reg);
		reg |= usb2_rt_trim_set(trim_val);
		writel(reg, usb2_phy2 + U2_ANA_CFG2);
		udelay(U_LEVEL5);
	}

	/* ATOP TEST bit */
	reg = readl(usb2_phy2 + U2_ANA_CFG3);
	reg = ana_cfg3_val(reg);
	reg |= U2_2_SLEW_RATE_OPT;
	writel(reg, usb2_phy2 + U2_ANA_CFG3);
	udelay(U_LEVEL5);

	reg = readl(usb2_phy2 + U2_ANA_CFG4);
	reg = ana_cfg4_val(reg);
#ifdef CONFIG_ARCH_SS528V100
	reg |= U2_VTXREF_SEL;
#endif
#ifdef CONFIG_ARCH_SS625V100
	reg |= U2_VTXREF_SEL_U3P;
#endif
	reg |= U2_FLS_EDGE_MODE;
	writel(reg, usb2_phy2 + U2_ANA_CFG4);
	udelay(U_LEVEL5);

	iounmap(usb2_phy2);
}

void bsp_usb2_phy2_config(struct phy *phy)
{
	unsigned int reg;
	void __iomem *usb2_phy2 = NULL;

	usb2_phy2 = ioremap_nocache(USB2_PHY2_BASE, __64K__);
	if (usb2_phy2 == NULL)
		return;

	/* usb2 phy2 pll enable */
	reg = readl(usb2_phy2 + PHY_PLL_OFFSET);
	reg |= PHY_PLL_ENABLE;
	writel(reg, usb2_phy2 + PHY_PLL_OFFSET);
	udelay(U_LEVEL5);

	iounmap(usb2_phy2);
}

void bsp_usb3_phy_config(struct phy *phy)
{
	struct bsp_priv *priv = phy_get_drvdata(phy);

	/* PI_CURRENT_TRIM ==>2'b00 to 2'b01 */
	writel(PI_CURRENT_TRIM_ENABLE, priv->misc_ctrl + COMBPHY_CTRL0);
	writel(PI_CURRENT_TRIM_VAL, priv->misc_ctrl + COMBPHY_CTRL0);
	writel(PI_CURRENT_TRIM_ENABLE, priv->misc_ctrl + COMBPHY_CTRL0);
	udelay(U_LEVEL5);

	/* TX_SWING_COMP ==>4'b1000 to 4b'1100 */
	writel(TX_SWING_COMP_ENABLE, priv->misc_ctrl + COMBPHY_CTRL0);
	writel(TX_SWING_COMP_VAL, priv->misc_ctrl + COMBPHY_CTRL0);
	writel(TX_SWING_COMP_ENABLE, priv->misc_ctrl + COMBPHY_CTRL0);
	udelay(U_LEVEL5);
}

void bsp_usb3_crg_config(struct phy *phy)
{
	unsigned int reg;
	struct bsp_priv *priv = phy_get_drvdata(phy);

	/* USB3 enable */
	reg = readl(priv->misc_ctrl + USB_CTRL6);
	reg &= ~U3_PORT_DISABLE;
	writel(reg, priv->misc_ctrl + USB_CTRL6);
	udelay(U_LEVEL6);

	/* ctrl0 rst */
	reg = readl(priv->peri_crg + PERI_CRG3664);
	reg |= USB3_SRST_REQ;
	writel(reg, priv->peri_crg + PERI_CRG3664);
	udelay(U_LEVEL6);

	/* combphy0 rst */
	reg = readl(priv->peri_crg + PERI_CRG3676);
	reg |= (COMBPHY0_SRST_REQ | COMBPHY0_TEST_SRST_REQ);
	writel(reg, priv->peri_crg + PERI_CRG3676);
	udelay(U_LEVEL6);

	/* usb2 phy0 rst */
	reg = readl(priv->peri_crg + PERI_CRG3636);
	reg |= (USB2_PHY0_REQ | USB2_PHY0_TREQ | USB2_PHY0_APB_SRST_REQ);
	writel(reg, priv->peri_crg + PERI_CRG3636);
	udelay(U_LEVEL6);

	/* usb2 phy2 rst */
	reg = readl(priv->peri_crg + PERI_CRG3672);
	reg |= (USB2_PHY2_REQ | USB2_PHY2_TREQ | USB2_PHY2_APB_SRST_REQ);
	writel(reg, priv->peri_crg + PERI_CRG3672);
	udelay(U_LEVEL6);

	/* open usb2 phy0 clk */
	reg = readl(priv->peri_crg + PERI_CRG3636);
	reg |= USB2_PHY0_XTAL_CKEN;
	writel(reg, priv->peri_crg + PERI_CRG3636);
	udelay(U_LEVEL6);

	/* open usb2 phy2 clk */
	reg = readl(priv->peri_crg + PERI_CRG3672);
	reg |= USB2_PHY2_XTAL_CKEN;
	writel(reg, priv->peri_crg + PERI_CRG3672);
	udelay(U_LEVEL6);

	/* cancel usb2 phy0 rst */
	reg = readl(priv->peri_crg + PERI_CRG3636);
	reg &= ~(USB2_PHY0_REQ | USB2_PHY0_TREQ | USB2_PHY0_APB_SRST_REQ);
	writel(reg, priv->peri_crg + PERI_CRG3636);
	mdelay(M_LEVEL1);

	/* cancel usb2 phy2 rst */
	reg = readl(priv->peri_crg + PERI_CRG3672);
	reg &= ~(USB2_PHY2_REQ | USB2_PHY2_TREQ | USB2_PHY2_APB_SRST_REQ);
	writel(reg, priv->peri_crg + PERI_CRG3672);
	mdelay(M_LEVEL1);

	/* open combphy0 clk */
	reg = readl(priv->peri_crg + PERI_CRG3676);
	reg |= COMBPHY0_REF_CKEN;
	writel(reg, priv->peri_crg + PERI_CRG3676);
	udelay(U_LEVEL6);

	/* cancel combphy0 rst */
	reg = readl(priv->peri_crg + PERI_CRG3676);
	reg &= ~(COMBPHY0_SRST_REQ | COMBPHY0_TEST_SRST_REQ);
	writel(reg, priv->peri_crg + PERI_CRG3676);
	udelay(U_LEVEL5);

	/* open pipe/suspend/ref/bus clk */
	reg = readl(priv->peri_crg + PERI_CRG3664);
	reg |= USB3_BUS_CKEN;
	reg |= USB3_REF_CKEN;
	reg |= USB3_SUSPEND_CKEN;
	reg |= USB3_UTMI_CKEN;
	reg |= USB3_PIPE_CKEN;
	writel(reg, priv->peri_crg + PERI_CRG3664);
	udelay(U_LEVEL6);

	/* open utmi clk */
	reg = readl(priv->peri_crg + PERI_CRG3632);
	reg |= USB2_0_UTMI_CKEN;
	writel(reg, priv->peri_crg + PERI_CRG3632);
	udelay(U_LEVEL6);

	/* cancel ctrl0 rst */
	reg = readl(priv->peri_crg + PERI_CRG3664);
	reg &= ~USB3_SRST_REQ;
	writel(reg, priv->peri_crg + PERI_CRG3664);
	udelay(U_LEVEL6);
}

void bsp_usb3_phy_on(struct phy *phy)
{
	bsp_usb3_crg_config(phy);

	bsp_usb3_phy_config(phy);

	bsp_usb2_phy0_config(phy);

	bsp_usb2_phy0_eye_config(phy);

	bsp_usb2_phy2_config(phy);

	bsp_usb2_phy2_eye_config(phy);
}
EXPORT_SYMBOL(bsp_usb3_phy_on);

void bsp_usb3_phy_off(struct phy *phy)
{
	unsigned int reg;
	struct bsp_priv *priv = phy_get_drvdata(phy);

	/* ctrl0 rst */
	reg = readl(priv->peri_crg + PERI_CRG3664);
	reg |= USB3_SRST_REQ;
	writel(reg, priv->peri_crg + PERI_CRG3664);
	udelay(U_LEVEL6);

	/* usb2 phy0 rst */
	reg = readl(priv->peri_crg + PERI_CRG3636);
	reg |= (USB2_PHY0_REQ | USB2_PHY0_TREQ | USB2_PHY0_APB_SRST_REQ);
	writel(reg, priv->peri_crg + PERI_CRG3636);
	udelay(U_LEVEL6);

	/* usb2 phy2 rst */
	reg = readl(priv->peri_crg + PERI_CRG3672);
	reg |= (USB2_PHY2_REQ | USB2_PHY2_TREQ | USB2_PHY2_APB_SRST_REQ);
	writel(reg, priv->peri_crg + PERI_CRG3672);
	udelay(U_LEVEL6);
}
EXPORT_SYMBOL(bsp_usb3_phy_off);
