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
#define DEF_VAL_3632		0x10331
#define USB2_0_SRST_REQ		(0x1 << 0)
#define	USB2_0_BUS_CKEN		(0x1 << 4)
#define	USB2_0_REF_CKEN		(0x1 << 5)
#define	USB2_0_UTMI_CKEN	(0x1 << 8)
#define	USB2_2_UTMI_CKEN	(0x1 << 9)
#define USB2_0_FREECLK_CKSEL	(0x1 << 16)

#define PERI_CRG3636		0x10
#define DEF_VAL_3636		0x153
#define	USB2_PHY0_REQ		(0x1 << 0)
#define	USB2_PHY0_TREQ		(0x1 << 1)
#define	USB2_PHY0_APB_SRST_REQ	(0x1 << 2)
#define	USB2_PHY0_XTAL_CKEN	(0x1 << 4)

#define PERI_CRG3640		0x20
#define DEF_VAL_3640		0x10131
#define USB2_1_SRST_REQ		(0x1 << 0)
#define	USB2_1_BUS_CKEN		(0x1 << 4)
#define	USB2_1_REF_CKEN		(0x1 << 5)
#define	USB2_1_UTMI_CKEN	(0x1 << 8)

#define PERI_CRG3644		0x30
#define DEF_VAL_3644		0x153
#define	USB2_PHY1_REQ		(0x1 << 0)
#define	USB2_PHY1_TREQ		(0x1 << 1)
#define	USB2_PHY1_APB_SRST_REQ	(0x1 << 2)
#define	USB2_PHY1_XTAL_CKEN	(0x1 << 4)

#define PERI_CRG3672		0xa0
#define DEF_VAL_3672		0x153
#define	USB2_PHY2_REQ		(0x1 << 0)
#define	USB2_PHY2_TREQ		(0x1 << 1)
#define	USB2_PHY2_APB_SRST_REQ	(0x1 << 2)
#define	USB2_PHY2_XTAL_CKEN	(0x1 << 4)

#define USB_CTRL_BASE		0x10300000
#define USB2_PHY2_BASE		0x10310000
#define USB2_PHY1_BASE		0x10350000
#define USB2_PHY0_BASE		0x10330000

#define GUSB2PHYCFG			0xc204
#define U2_FREECLK_EXISTS	(0x1 << 30)

#define	PHY_PLL_ENABLE		(0x3 << 0)
#define	PHY_PLL_OFFSET		0x14

#define	RG_HSTX_MBIAS		0x0
#define	RG_HSTX_MBIAS_MASK	(0xf << 0)
#define	RG_HSTX_MBIAS_VAL	(0xb << 0)
#define RG_HSTX_DEEN        (0x1 << 5)
#define RG_HSTX_DE          (0x4 << 8)

#define	TX_TEST_BIT			0x8
#define	TX_TEST_BIT_VAL		(0x3 << 20)

#define	DISC_REF_VOL_SEL	0x8
#define	DISC_REF_VOL_SEL_MASK	(0x7 << 16)
#define	DISC_REF_VOL_SEL_VAL	(0x5 << 16)

#define	SLEW_RATE_OPTION	0xc
#define	SLEW_RATE_OPTION_MASK	(0x3 << 20)
#define	SLEW_RATE_OPTION_VAL	(0x1 << 20)

#define	TX_REF_VOL_SEL		0x10
#define	TX_REF_VOL_SEL_MASK	(0x7 << 4)
#define	TX_REF_VOL_SEL_VAL	(0x6 << 4)

#define	RG_FL_EDGE_MODE		0x10
#define	RG_FL_EDGE_MODE_VAL	(0x1 << 13)

#define U2_TRIM_VAL_MIN	0x09
#define U2_TRIM_VAL_MAX	0x1d
#define RT_TRIM_VAL_MASK	0x1f
#define usb2_2_trim_val(p)	(((p) >> 10) & RT_TRIM_VAL_MASK)
#define usb2_1_trim_val(p)	(((p) >> 5) & RT_TRIM_VAL_MASK)
#define usb2_0_trim_val(p)	(((p) >> 0) & RT_TRIM_VAL_MASK)

#define U2_ANA_CFG2		0x8
#define usb2_rt_trim_clr(p)	((p) & (~(RT_TRIM_VAL_MASK << 8)))
#define usb2_rt_trim_set(p)	((p) << 8)

void bsp_usb_def_config(struct phy *phy)
{
	struct bsp_priv *priv = phy_get_drvdata(phy);

	/* default value rewrite */
	writel(DEF_VAL_3632, priv->peri_crg + PERI_CRG3632);
	writel(DEF_VAL_3636, priv->peri_crg + PERI_CRG3636);
	writel(DEF_VAL_3640, priv->peri_crg + PERI_CRG3640);
	writel(DEF_VAL_3644, priv->peri_crg + PERI_CRG3644);
	writel(DEF_VAL_3672, priv->peri_crg + PERI_CRG3672);
	udelay(U_LEVEL6);
}

void bsp_usb2_phy0_config(struct phy *phy)
{
	unsigned int reg;
	unsigned int trim_val;
	void __iomem *usb2_phy0 = NULL;
	struct bsp_priv *priv = phy_get_drvdata(phy);

	usb2_phy0 = ioremap_nocache(USB2_PHY0_BASE, __64K__);
	if (usb2_phy0 == NULL)
		return;

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

	/* usb2 phy0 pll enable */
	reg = readl(usb2_phy0 + PHY_PLL_OFFSET);
	reg |= PHY_PLL_ENABLE;
	writel(reg, usb2_phy0 + PHY_PLL_OFFSET);
	udelay(U_LEVEL5);

	/* rg_hstx_mbias: 4'b0011==>4'b1011 */
	reg = readl(usb2_phy0 + RG_HSTX_MBIAS);
	reg &= ~RG_HSTX_MBIAS_MASK;
	reg |= (RG_HSTX_MBIAS_VAL | RG_HSTX_DEEN | RG_HSTX_DE);
	writel(reg, usb2_phy0 + RG_HSTX_MBIAS);
	udelay(U_LEVEL5);

	/* TX TEST bit Chirp KJ: 1'b0==>1'b1 */
	reg = readl(usb2_phy0 + TX_TEST_BIT);
	reg |= TX_TEST_BIT_VAL;
	writel(reg, usb2_phy0 + TX_TEST_BIT);
	udelay(U_LEVEL5);

	/* disconnect reference voltage sel: 3'b001==>3'b011 */
	reg = readl(usb2_phy0 + DISC_REF_VOL_SEL);
	reg &= ~DISC_REF_VOL_SEL_MASK;
	reg |= DISC_REF_VOL_SEL_VAL;
	writel(reg, usb2_phy0 + DISC_REF_VOL_SEL);
	udelay(U_LEVEL5);

	/* slew rate option: 2b'00==>2b'01 */
	reg = readl(usb2_phy0 + SLEW_RATE_OPTION);
	reg &= ~SLEW_RATE_OPTION_MASK;
	reg |= SLEW_RATE_OPTION_VAL;
	writel(reg, usb2_phy0 + SLEW_RATE_OPTION);
	udelay(U_LEVEL5);

	/* TX reference voltage sel: 4'100==>4'110 */
	reg = readl(usb2_phy0 + TX_REF_VOL_SEL);
	reg &= ~TX_REF_VOL_SEL_MASK;
	reg |= TX_REF_VOL_SEL_VAL;
	writel(reg, usb2_phy0 + TX_REF_VOL_SEL);
	udelay(U_LEVEL5);

	/* rg_fls_edge_mode: 1'b0==>1'b1 */
	reg = readl(usb2_phy0 + RG_FL_EDGE_MODE);
	reg |= RG_FL_EDGE_MODE_VAL;
	writel(reg, usb2_phy0 + RG_FL_EDGE_MODE);
	udelay(U_LEVEL5);

	iounmap(usb2_phy0);
}

void bsp_usb2_phy1_config(struct phy *phy)
{
	unsigned int reg;
	unsigned int trim_val;
	void __iomem *usb2_phy1 = NULL;
	struct bsp_priv *priv = phy_get_drvdata(phy);

	usb2_phy1 = ioremap_nocache(USB2_PHY1_BASE, __64K__);
	if (usb2_phy1 == NULL)
		return;

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

	/* usb2 phy1 pll enable */
	reg = readl(usb2_phy1 + PHY_PLL_OFFSET);
	reg |= PHY_PLL_ENABLE;
	writel(reg, usb2_phy1 + PHY_PLL_OFFSET);
	udelay(U_LEVEL5);

	/* rg_hstx_mbias: 4'b0011==>4'b1011 */
	reg = readl(usb2_phy1 + RG_HSTX_MBIAS);
	reg &= ~RG_HSTX_MBIAS_MASK;
	reg |= (RG_HSTX_MBIAS_VAL | RG_HSTX_DEEN | RG_HSTX_DE);
	writel(reg, usb2_phy1 + RG_HSTX_MBIAS);
	udelay(U_LEVEL5);

	/* TX TEST bit Chirp KJ: 1'b0==>1'b1 */
	reg = readl(usb2_phy1 + TX_TEST_BIT);
	reg |= TX_TEST_BIT_VAL;
	writel(reg, usb2_phy1 + TX_TEST_BIT);
	udelay(U_LEVEL5);

	/* disconnect reference voltage sel: 3'b001==>3'b011 */
	reg = readl(usb2_phy1 + DISC_REF_VOL_SEL);
	reg &= ~DISC_REF_VOL_SEL_MASK;
	reg |= DISC_REF_VOL_SEL_VAL;
	writel(reg, usb2_phy1 + DISC_REF_VOL_SEL);
	udelay(U_LEVEL5);

	/* slew rate option: 2b'00==>2b'01 */
	reg = readl(usb2_phy1 + SLEW_RATE_OPTION);
	reg &= ~SLEW_RATE_OPTION_MASK;
	reg |= SLEW_RATE_OPTION_VAL;
	writel(reg, usb2_phy1 + SLEW_RATE_OPTION);
	udelay(U_LEVEL5);

	/* TX reference voltage sel: 4'100==>4'110 */
	reg = readl(usb2_phy1 + TX_REF_VOL_SEL);
	reg &= ~TX_REF_VOL_SEL_MASK;
	reg |= TX_REF_VOL_SEL_VAL;
	writel(reg, usb2_phy1 + TX_REF_VOL_SEL);
	udelay(U_LEVEL5);

	/* rg_fls_edge_mode: 1'b0==>1'b1 */
	reg = readl(usb2_phy1 + RG_FL_EDGE_MODE);
	reg |= RG_FL_EDGE_MODE_VAL;
	writel(reg, usb2_phy1 + RG_FL_EDGE_MODE);
	udelay(U_LEVEL5);

	iounmap(usb2_phy1);
}

void bsp_usb2_phy2_config(struct phy *phy)
{
	unsigned int reg;
	unsigned int trim_val;
	void __iomem *usb2_phy2 = NULL;
	struct bsp_priv *priv = phy_get_drvdata(phy);
	if (priv == NULL)
		return;

	if (priv->sys_ctrl == NULL)
		return;

	usb2_phy2 = ioremap_nocache(USB2_PHY2_BASE, __64K__);
	if (usb2_phy2 == NULL)
		return;

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

	/* rg_hstx_mbias: 4'b0011==>4'b1011 */
	reg = readl(usb2_phy2 + RG_HSTX_MBIAS);
	reg &= ~RG_HSTX_MBIAS_MASK;
	reg |= (RG_HSTX_MBIAS_VAL | RG_HSTX_DEEN | RG_HSTX_DE);
	writel(reg, usb2_phy2 + RG_HSTX_MBIAS);
	udelay(U_LEVEL5);

	/* TX TEST bit Chirp KJ: 1'b0==>1'b1 */
	reg = readl(usb2_phy2 + TX_TEST_BIT);
	reg |= TX_TEST_BIT_VAL;
	writel(reg, usb2_phy2 + TX_TEST_BIT);
	udelay(U_LEVEL5);

	/* disconnect reference voltage sel: 3'b001==>3'b011 */
	reg = readl(usb2_phy2 + DISC_REF_VOL_SEL);
	reg &= ~DISC_REF_VOL_SEL_MASK;
	reg |= DISC_REF_VOL_SEL_VAL;
	writel(reg, usb2_phy2 + DISC_REF_VOL_SEL);
	udelay(U_LEVEL5);

	/* slew rate option: 2b'00==>2b'01 */
	reg = readl(usb2_phy2 + SLEW_RATE_OPTION);
	reg &= ~SLEW_RATE_OPTION_MASK;
	reg |= SLEW_RATE_OPTION_VAL;
	writel(reg, usb2_phy2 + SLEW_RATE_OPTION);
	udelay(U_LEVEL5);

	/* TX reference voltage sel: 4'100==>4'110 */
	reg = readl(usb2_phy2 + TX_REF_VOL_SEL);
	reg &= ~TX_REF_VOL_SEL_MASK;
	reg |= TX_REF_VOL_SEL_VAL;
	writel(reg, usb2_phy2 + TX_REF_VOL_SEL);
	udelay(U_LEVEL5);

	/* rg_fls_edge_mode: 1'b0==>1'b1 */
	reg = readl(usb2_phy2 + RG_FL_EDGE_MODE);
	reg |= RG_FL_EDGE_MODE_VAL;
	writel(reg, usb2_phy2 + RG_FL_EDGE_MODE);
	udelay(U_LEVEL5);

	iounmap(usb2_phy2);
	usb2_phy2 = NULL;
}

void bsp_usb_ctrl_config(void)
{
	unsigned int reg;
	void __iomem *ctrl_base = NULL;

	ctrl_base = ioremap_nocache(USB_CTRL_BASE, __64K__);
	if (ctrl_base == NULL)
		return;

	reg = readl(ctrl_base + GUSB2PHYCFG);
	reg &= ~U2_FREECLK_EXISTS;
	writel(reg, ctrl_base + GUSB2PHYCFG);
	udelay(U_LEVEL6);

	iounmap(ctrl_base);
}

void bsp_usb2_0_config(struct phy *phy)
{
	unsigned int reg;
	struct bsp_priv *priv = phy_get_drvdata(phy);

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

	bsp_usb2_phy0_config(phy);

	bsp_usb2_phy2_config(phy);

	/* open pipe/suspend/ref/bus clk */
	reg = readl(priv->peri_crg + PERI_CRG3632);
	reg |= USB2_0_REF_CKEN;
	reg |= USB2_0_BUS_CKEN;
	reg |= USB2_0_UTMI_CKEN;
	reg |= USB2_2_UTMI_CKEN;
	reg |= USB2_0_FREECLK_CKSEL;
	writel(reg, priv->peri_crg + PERI_CRG3632);
	udelay(U_LEVEL6);

	/* cancel ctrl0 rst */
	reg = readl(priv->peri_crg + PERI_CRG3632);
	reg &= ~USB2_0_SRST_REQ;
	writel(reg, priv->peri_crg + PERI_CRG3632);
	udelay(U_LEVEL6);

	bsp_usb_ctrl_config();
}

void bsp_usb2_1_config(struct phy *phy)
{
	unsigned int reg;
	struct bsp_priv *priv = phy_get_drvdata(phy);
	if (priv == NULL)
		return;

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

	bsp_usb2_phy1_config(phy);

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
	bsp_usb_def_config(phy);

	bsp_usb2_0_config(phy);

	bsp_usb2_1_config(phy);
}
EXPORT_SYMBOL(bsp_usb_phy_on);

void bsp_usb_phy_off(struct phy *phy)
{
	bsp_usb_def_config(phy);
}
EXPORT_SYMBOL(bsp_usb_phy_off);
