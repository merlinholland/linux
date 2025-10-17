/*
 * Copyright (c) 2016-2019 Shenshu Technologies Co., Ltd.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
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
#include <linux/io.h>
#include <linux/io.h>

#include "phy-bsp-sata.h"

#define BSP_SATA_PHY0_CTLL	0xA0
#define BSP_SATA_PHY0_CTLH	0xA4
#define BSP_SATA_PHY1_CTLL	0xAC
#define BSP_SATA_PHY1_CTLH	0xB0

#define BSP_SATA_PORT_FIFOTH	0x44
#define BSP_SATA_PORT_PHYCTL1	0x70
#define BSP_SATA_PORT_PHYCTL2	0x74
#define BSP_SATA_PORT_PHYCTL3	0x78

#define BSP_SYS_CTRL_REG_BASE		0x11020000
#define BSP_SYS_STAT_REG		0x0018
#define BSP_SYS_CTRL_REG_MAP_SIZE	0x1000
#define get_ups_mode_val(reg_val)	(((reg_val) >> 16) & 0x7)

#define SATA_PHY_CTRL0	0x140
#define SATA_PHY_CTRL1	0x144

#define P1_PHY_SERDES_ARCH	BIT(21)
#define P0_PHY_SERDES_ARCH	BIT(5)

#define P3_PHY_SERDES_ARCH	BIT(21)
#define P2_PHY_SERDES_ARCH	BIT(5)

#define BSP_MISC_REG_BASE	0x11024000
#define BSP_MISC_REG_MAP_SIZE  0x1000

#define BSP_COMB_PHY1_TEST_CTRL	0x1cc
#define BSP_COMB_PHY2_TEST_CTRL	0x1d0

#define BSP_CRG_REG_BASE	0x11010000

#define SATA_CLK_RST_CTRL_1_REG	0x3B40
#define SATA_CLK_RST_CTRL_2_REG	0x3B48

#define BSP_SATA_CKO_ALIVE_SRST_REQ	BIT(0)
#define BSP_SATA_CKO_CKEN		BIT(4)
#define BSP_SATA_BUS_SRST_REQ		BIT(0)
#define BSP_SATA_BUS_CKEN		BIT(4)

#define SATA_PHY0_CLK_RST_REG	0x3B60
#define SATA_PHY1_CLK_RST_REG	0x3B80
#define SATA_PHY2_CLK_RST_REG	0x3BA0
#define SATA_PHY3_CLK_RST_REG	0x3BC0

#define SATA_CTRL_RX_RST	BIT(0)
#define SATA_CTRL_SATA_RST	BIT(1)
#define SATA_CTRL_RX_CKEN	BIT(4)
#define SATA_CTRL_TX_CKEN	BIT(5)

#define COMB_PHY1_PORT_A_REG	0x3B70
#define COMB_PHY1_PORT_B_REG	0x3B90
#define COMB_PHY2_PORT_A_REG	0x3BB0
#define COMB_PHY2_PORT_B_REG	0x3BD0

#define COMB_PHY_REST		BIT(0)
#define COMB_PHY_TEST_REST	BIT(1)
#define COMB_PHY_REF_CLKEN	BIT(4)
#define COM_PHY_REF_CLKSEL	BIT(12)
#define com_phy_ref_clksel_100m(reg_val) ((reg_val) & (~COM_PHY_REF_CLKSEL))

#define SATA_CRG_MAP_SIZE	0x100

#define CASE_NUM0 0
#define CASE_NUM1 1
#define CASE_NUM2 2
#define CASE_NUM3 3
#define CASE_NUM4 4
#define CASE_NUM5 5

enum {
	FIFOTH_VALUE    = 0xdffeffff,
	PHY_VALUE       = 0x4900003d,
	PHYCTL2_VALUE   = 0x60555,

	PORT_BIGENDINE  = 0x82e5cb8,

	PX_TX_AMPLITUDE = 0x36089,
	PX_TX_PREEMPH   = 0x486186,

	PHY_SG_1_5G  = 0xe000030,
	PHY_SG_3G    = 0xe200030,
	PHY_SG_6G    = 0xe400030,
};

static void bsp_sata_poweron(void)
{
}

static void bsp_sata_poweroff(void)
{
}

static void bsp_ctrl_sata_rx_reset(void *crg_base, unsigned int crg_base_offset)
{
	unsigned int reg_val;

	reg_val = readl(crg_base + crg_base_offset);
	reg_val |= (SATA_CTRL_RX_RST | SATA_CTRL_SATA_RST);
	writel(reg_val, crg_base + crg_base_offset);
}

static void bsp_ctrl_sata_rx_unreset(void *crg_base, unsigned int crg_base_offset)
{
	unsigned int reg_val;

	reg_val = readl(crg_base + crg_base_offset);
	reg_val &= ~(SATA_CTRL_RX_RST | SATA_CTRL_SATA_RST);
	writel(reg_val, crg_base + crg_base_offset);
}

static void bsp_comb_phy_reset(void *comb_phy_crg_base, unsigned int comb_phy_crg_base_offset)
{
	unsigned int reg_val;

	reg_val = readl(comb_phy_crg_base + comb_phy_crg_base_offset);
	reg_val |= COMB_PHY_REST;
	writel(reg_val, comb_phy_crg_base + comb_phy_crg_base_offset);
}

static void bsp_comb_phy_unreset(void *comb_phy_crg_base, unsigned int comb_phy_crg_base_offset)
{
	unsigned int reg_val;

	reg_val = readl(comb_phy_crg_base + comb_phy_crg_base_offset);
	reg_val &= ~(COMB_PHY_REST);
	writel(reg_val, comb_phy_crg_base + comb_phy_crg_base_offset);
}

static void bsp_sata_rx_reset(void *crg_base, unsigned int port_no)
{
	switch (port_no) {
	case CASE_NUM1:
		bsp_ctrl_sata_rx_reset(crg_base, 0x20);
		break;
	case CASE_NUM2:
		bsp_ctrl_sata_rx_reset(crg_base, 0x20);
		bsp_ctrl_sata_rx_reset(crg_base, 0x40);
		break;
	case CASE_NUM3:
		bsp_ctrl_sata_rx_reset(crg_base, 0x20);
		bsp_ctrl_sata_rx_reset(crg_base, 0x40);
		bsp_ctrl_sata_rx_reset(crg_base, 0x60);
		break;
	case CASE_NUM4:
		bsp_ctrl_sata_rx_reset(crg_base, 0x20);
		bsp_ctrl_sata_rx_reset(crg_base, 0x40);
		bsp_ctrl_sata_rx_reset(crg_base, 0x60);
		bsp_ctrl_sata_rx_reset(crg_base, 0x80);
		break;
	default:
		break;
	}
}

static void bsp_sata_rx_unreset(void *crg_base, unsigned int port_no)
{
	switch (port_no) {
	case CASE_NUM1:
		bsp_ctrl_sata_rx_unreset(crg_base, 0x20);
		break;
	case CASE_NUM2:
		bsp_ctrl_sata_rx_unreset(crg_base, 0x20);
		bsp_ctrl_sata_rx_unreset(crg_base, 0x40);
		break;
	case CASE_NUM3:
		bsp_ctrl_sata_rx_unreset(crg_base, 0x20);
		bsp_ctrl_sata_rx_unreset(crg_base, 0x40);
		bsp_ctrl_sata_rx_unreset(crg_base, 0x60);
		break;
	case CASE_NUM4:
		bsp_ctrl_sata_rx_unreset(crg_base, 0x20);
		bsp_ctrl_sata_rx_unreset(crg_base, 0x40);
		bsp_ctrl_sata_rx_unreset(crg_base, 0x60);
		bsp_ctrl_sata_rx_unreset(crg_base, 0x80);
		break;
	default:
		break;
	}
}

void bsp_sata_reset_rxtx_assert(unsigned int port_no)
{
	void *crg_base = NULL;
	void *comb_phy_crg_base = NULL;

	comb_phy_crg_base = ioremap((BSP_CRG_REG_BASE + COMB_PHY1_PORT_A_REG),
				    SATA_CRG_MAP_SIZE);
	if (!comb_phy_crg_base) {
		pr_err("ioremap comb phy crg base failed! func:%s, line:%d\n", __func__, __LINE__);
		return;
	}

	crg_base = ioremap((BSP_CRG_REG_BASE + SATA_CLK_RST_CTRL_1_REG),
			   SATA_CRG_MAP_SIZE);
	if (!crg_base) {
		pr_err("ioremap sata reset register base failed! func:%s, line:%d\n", __func__,
		       __LINE__);
		iounmap(comb_phy_crg_base);
		return;
	}

	switch (port_no) {
	case CASE_NUM0:
		bsp_ctrl_sata_rx_reset(crg_base, 0x20);
		bsp_comb_phy_reset(comb_phy_crg_base, 0x20);
		break;
	case CASE_NUM1:
		bsp_ctrl_sata_rx_reset(crg_base, 0x40);
		bsp_comb_phy_reset(comb_phy_crg_base, 0x00);
		break;
	case CASE_NUM2:
		bsp_ctrl_sata_rx_reset(crg_base, 0x60);
		bsp_comb_phy_reset(comb_phy_crg_base, 0x60);
		break;
	case CASE_NUM3:
		bsp_ctrl_sata_rx_reset(crg_base, 0x80);
		bsp_comb_phy_reset(comb_phy_crg_base, 0x40);
		break;
	default:
		break;
	}

	iounmap(crg_base);
	iounmap(comb_phy_crg_base);
}
EXPORT_SYMBOL(bsp_sata_reset_rxtx_assert);

void bsp_sata_reset_rxtx_deassert(unsigned int port_no)
{
	void *crg_base = NULL;
	void *comb_phy_crg_base = NULL;

	comb_phy_crg_base = ioremap((BSP_CRG_REG_BASE + COMB_PHY1_PORT_A_REG),
				    SATA_CRG_MAP_SIZE);
	if (!comb_phy_crg_base) {
		pr_err("ioremap comb phy crg base failed! func:%s, line:%d\n", __func__, __LINE__);
		return;
	}

	crg_base = ioremap((BSP_CRG_REG_BASE + SATA_CLK_RST_CTRL_1_REG),
			   SATA_CRG_MAP_SIZE);
	if (!crg_base) {
		pr_err("ioremap sata reset register base failed! func:%s, line:%d\n", __func__,
		       __LINE__);
		iounmap(comb_phy_crg_base);
		return;
	}

	switch (port_no) {
	case CASE_NUM0:
		bsp_ctrl_sata_rx_unreset(crg_base, 0x20);
		bsp_comb_phy_unreset(comb_phy_crg_base, 0x20);
		break;
	case CASE_NUM1:
		bsp_ctrl_sata_rx_unreset(crg_base, 0x40);
		bsp_comb_phy_unreset(comb_phy_crg_base, 0x00);
		break;
	case CASE_NUM2:
		bsp_ctrl_sata_rx_unreset(crg_base, 0x60);
		bsp_comb_phy_unreset(comb_phy_crg_base, 0x60);
		break;
	case CASE_NUM3:
		bsp_ctrl_sata_rx_unreset(crg_base, 0x80);
		bsp_comb_phy_unreset(comb_phy_crg_base, 0x40);
		break;
	default:
		break;
	}
	iounmap(crg_base);
	iounmap(comb_phy_crg_base);
	udelay(100); /* delay 100 us */
}
EXPORT_SYMBOL(bsp_sata_reset_rxtx_deassert);

static void bsp_sata_reset(void)
{
	unsigned int reg_val;
	void *crg_base = NULL;

	crg_base = ioremap((BSP_CRG_REG_BASE + SATA_CLK_RST_CTRL_1_REG),
			   SATA_CRG_MAP_SIZE);
	if (!crg_base) {
		pr_err("ioremap sata reset register base failed! func:%s, line:%d\n", __func__,
		       __LINE__);
		return;
	}

	reg_val = readl(crg_base);
	reg_val |= BSP_SATA_CKO_ALIVE_SRST_REQ;
	writel(reg_val, crg_base);

	reg_val = readl(crg_base + 0x8);
	reg_val |= BSP_SATA_BUS_SRST_REQ;
	writel(reg_val, crg_base + 0x8);

	bsp_sata_rx_reset(crg_base, ports_num);

	iounmap(crg_base);
}

static void bsp_sata_unreset(void)
{
	unsigned int reg_val;
	void *crg_base = NULL;

	crg_base = ioremap((BSP_CRG_REG_BASE + SATA_CLK_RST_CTRL_1_REG),
			   SATA_CRG_MAP_SIZE);
	if (!crg_base) {
		pr_err("ioremap sata reset register base failed! func:%s, line:%d\n", __func__,
		       __LINE__);
		return;
	}

	bsp_sata_rx_unreset(crg_base, ports_num);

	reg_val = readl(crg_base);
	reg_val &= ~BSP_SATA_CKO_ALIVE_SRST_REQ;
	writel(reg_val, crg_base);

	reg_val = readl(crg_base + 0x8);
	reg_val &= ~BSP_SATA_BUS_SRST_REQ;
	writel(reg_val, crg_base + 0x8);

	iounmap(crg_base);
}

static void bsp_sata_phy1_portb_reset(void *crg_base)
{
	unsigned int reg_val;

	reg_val = readl(crg_base + 0x20);
	reg_val |= (COMB_PHY_REST | COMB_PHY_TEST_REST);
	writel(reg_val, crg_base + 0x20);
}

static void bsp_sata_phy1_portb_unreset(void *crg_base)
{
	unsigned int reg_val;

	reg_val = readl(crg_base + 0x20);
	reg_val &= ~(COMB_PHY_REST | COMB_PHY_TEST_REST);
	writel(reg_val, crg_base + 0x20);
}

static void bsp_sata_phy1_porta_reset(void *crg_base)
{
	unsigned int reg_val;

	reg_val = readl(crg_base);
	reg_val |= (COMB_PHY_REST | COMB_PHY_TEST_REST);
	writel(reg_val, crg_base);
}

static void bsp_sata_phy1_porta_unreset(void *crg_base)
{
	unsigned int reg_val;

	reg_val = readl(crg_base);
	reg_val &= ~(COMB_PHY_REST | COMB_PHY_TEST_REST);
	writel(reg_val, crg_base);
}

static void bsp_sata_phy2_portb_reset(void *crg_base)
{
	unsigned int reg_val;

	reg_val = readl(crg_base + 0x60);
	reg_val |= (COMB_PHY_REST | COMB_PHY_TEST_REST);
	writel(reg_val, crg_base + 0x60);
}

static void bsp_sata_phy2_portb_unreset(void *crg_base)
{
	unsigned int reg_val;

	reg_val = readl(crg_base + 0x60);
	reg_val &= ~(COMB_PHY_REST | COMB_PHY_TEST_REST);
	writel(reg_val, crg_base + 0x60);
}

static void bsp_sata_phy2_porta_reset(void *crg_base)
{
	unsigned int reg_val;

	reg_val = readl(crg_base + 0x40);
	reg_val |= (COMB_PHY_REST | COMB_PHY_TEST_REST);
	writel(reg_val, crg_base + 0x40);
}

static void bsp_sata_phy2_porta_unreset(void *crg_base)
{
	unsigned int reg_val;

	reg_val = readl(crg_base + 0x40);
	reg_val &= ~(COMB_PHY_REST | COMB_PHY_TEST_REST);
	writel(reg_val, crg_base + 0x40);
}

static void bsp_sata_phy_reset(void)
{
	void *comb_phy_crg_base = NULL;

	comb_phy_crg_base = ioremap((BSP_CRG_REG_BASE + COMB_PHY1_PORT_A_REG),
				    SATA_CRG_MAP_SIZE);
	if (!comb_phy_crg_base) {
		pr_err("ioremap comb phy crg base failed! func:%s, line:%d\n", __func__,
		       __LINE__);
		return;
	}

	switch (ports_num) {
	case CASE_NUM1:
		/* COMBPHY1 PORT B */
		bsp_sata_phy1_portb_reset(comb_phy_crg_base);
		break;
	case CASE_NUM2:
		/* COMBPHY1 PORT B */
		bsp_sata_phy1_portb_reset(comb_phy_crg_base);

		/* COMBPHY1 PORT A */
		bsp_sata_phy1_porta_reset(comb_phy_crg_base);
		break;
	case CASE_NUM3:
		/* COMBPHY1 PORT B */
		bsp_sata_phy1_portb_reset(comb_phy_crg_base);

		/* COMBPHY1 PORT A */
		bsp_sata_phy1_porta_reset(comb_phy_crg_base);

		/* COMBPHY2 PORT B */
		bsp_sata_phy2_portb_reset(comb_phy_crg_base);
		break;
	case CASE_NUM4:
		/* COMBPHY1 PORT B */
		bsp_sata_phy1_portb_reset(comb_phy_crg_base);

		/* COMBPHY1 PORT A */
		bsp_sata_phy1_porta_reset(comb_phy_crg_base);

		/* COMBPHY2 PORT B */
		bsp_sata_phy2_portb_reset(comb_phy_crg_base);

		/* COMBPHY2 PORT A */
		bsp_sata_phy2_porta_reset(comb_phy_crg_base);
		break;
	default:
		break;
	}

	iounmap(comb_phy_crg_base);
}

static void bsp_sata_phy_unreset(void)
{
	void *comb_phy_crg_base = NULL;

	comb_phy_crg_base = ioremap((BSP_CRG_REG_BASE + COMB_PHY1_PORT_A_REG),
				    SATA_CRG_MAP_SIZE);
	if (!comb_phy_crg_base) {
		pr_err("ioremap comb phy crg base failed! func:%s, line:%d\n", __func__,
		       __LINE__);
		return;
	}

	switch (ports_num) {
	case CASE_NUM1:
		/* COMBPHY1 PORT B */
		bsp_sata_phy1_portb_unreset(comb_phy_crg_base);
		break;
	case CASE_NUM2:
		/* COMBPHY1 PORT B */
		bsp_sata_phy1_portb_unreset(comb_phy_crg_base);

		/* COMBPHY1 PORT A */
		bsp_sata_phy1_porta_unreset(comb_phy_crg_base);
		break;
	case CASE_NUM3:
		/* COMBPHY1 PORT B */
		bsp_sata_phy1_portb_unreset(comb_phy_crg_base);

		/* COMBPHY1 PORT A */
		bsp_sata_phy1_porta_unreset(comb_phy_crg_base);

		/* COMBPHY2 PORT B */
		bsp_sata_phy2_portb_unreset(comb_phy_crg_base);
		break;
	case CASE_NUM4:
		/* COMBPHY1 PORT B */
		bsp_sata_phy1_portb_unreset(comb_phy_crg_base);

		/* COMBPHY1 PORT A */
		bsp_sata_phy1_porta_unreset(comb_phy_crg_base);

		/* COMBPHY2 PORT B */
		bsp_sata_phy2_portb_unreset(comb_phy_crg_base);

		/* COMBPHY2 PORT A */
		bsp_sata_phy2_porta_unreset(comb_phy_crg_base);
		break;
	default:
		break;
	}

	udelay(60); /* delay 60 us */

	iounmap(comb_phy_crg_base);
}

static void bsp_sata_tx0_rx0_clk_enable(void *crg_base)
{
	unsigned int reg_val;

	reg_val = readl(crg_base + 0x20);
	reg_val |= (SATA_CTRL_RX_CKEN | SATA_CTRL_TX_CKEN);
	writel(reg_val, crg_base + 0x20);
}

static void bsp_sata_tx1_rx1_clk_enable(void *crg_base)
{
	unsigned int reg_val;

	reg_val = readl(crg_base + 0x40);
	reg_val |= (SATA_CTRL_RX_CKEN | SATA_CTRL_TX_CKEN);
	writel(reg_val, crg_base + 0x40);
}

static void bsp_sata_tx2_rx2_clk_enable(void *crg_base)
{
	unsigned int reg_val;

	reg_val = readl(crg_base + 0x60);
	reg_val |= (SATA_CTRL_RX_CKEN | SATA_CTRL_TX_CKEN);
	writel(reg_val, crg_base + 0x60);
}

static void bsp_sata_tx3_rx3_clk_enable(void *crg_base)
{
	unsigned int reg_val;

	reg_val = readl(crg_base + 0x80);
	reg_val |= (SATA_CTRL_RX_CKEN | SATA_CTRL_TX_CKEN);
	writel(reg_val, crg_base + 0x80);
}

static void bsp_sata_clk_enable(void)
{
	unsigned int reg_val;
	void *crg_base = NULL;

	crg_base = ioremap((BSP_CRG_REG_BASE + SATA_CLK_RST_CTRL_1_REG),
			   SATA_CRG_MAP_SIZE);
	if (!crg_base) {
		pr_err("ioremap sata reset register base failed! func:%s, line:%d\n", __func__,
		       __LINE__);
		return;
	}

	switch (ports_num) {
	case CASE_NUM1:
		bsp_sata_tx0_rx0_clk_enable(crg_base);
		break;
	case CASE_NUM2:
		bsp_sata_tx0_rx0_clk_enable(crg_base);
		bsp_sata_tx1_rx1_clk_enable(crg_base);
		break;
	case CASE_NUM3:
		bsp_sata_tx0_rx0_clk_enable(crg_base);
		bsp_sata_tx1_rx1_clk_enable(crg_base);
		bsp_sata_tx2_rx2_clk_enable(crg_base);
		break;
	case CASE_NUM4:
		bsp_sata_tx0_rx0_clk_enable(crg_base);
		bsp_sata_tx1_rx1_clk_enable(crg_base);
		bsp_sata_tx2_rx2_clk_enable(crg_base);
		bsp_sata_tx3_rx3_clk_enable(crg_base);
		break;
	default:
		break;
	}

	reg_val = readl(crg_base);
	reg_val |= BSP_SATA_CKO_CKEN;
	writel(reg_val, crg_base);

	reg_val = readl(crg_base + 0x8);
	reg_val |= BSP_SATA_BUS_CKEN;
	writel(reg_val, crg_base + 0x8);

	iounmap(crg_base);
}

static void bsp_sata_clk_disable(void)
{
}

static void bsp_sata_clk_reset(void)
{
}

static void bsp_sata_phy1_portb_clk_sel(void *crg_base)
{
	unsigned int reg_val;

	reg_val = readl(crg_base + 0x20);
	reg_val |= COMB_PHY_REF_CLKEN;
	reg_val = com_phy_ref_clksel_100m(reg_val);
	writel(reg_val, crg_base + 0x20);
}

static void bsp_sata_phy1_porta_clk_sel(void *crg_base)
{
	unsigned int reg_val;

	reg_val = readl(crg_base);
	reg_val |= COMB_PHY_REF_CLKEN;
	reg_val = com_phy_ref_clksel_100m(reg_val);
	writel(reg_val, crg_base);
}

static void bsp_sata_phy2_portb_clk_sel(void *crg_base)
{
	unsigned int reg_val;

	reg_val = readl(crg_base + 0x60);
	reg_val |= COMB_PHY_REF_CLKEN;
	reg_val = com_phy_ref_clksel_100m(reg_val);
	writel(reg_val, crg_base + 0x60);
}

static void bsp_sata_phy2_porta_clk_sel(void *crg_base)
{
	unsigned int reg_val;

	reg_val = readl(crg_base + 0x40);
	reg_val |= COMB_PHY_REF_CLKEN;
	reg_val = com_phy_ref_clksel_100m(reg_val);
	writel(reg_val, crg_base + 0x40);
}

static void bsp_sata_phy_clk_sel(void)
{
	void *comb_phy_crg_base = NULL;

	comb_phy_crg_base = ioremap((BSP_CRG_REG_BASE + COMB_PHY1_PORT_A_REG),
				    SATA_CRG_MAP_SIZE);
	if (!comb_phy_crg_base) {
		pr_err("ioremap comb phy crg base failed! func:%s, line:%d\n", __func__,
		       __LINE__);
		return;
	}

	switch (ports_num) {
	case CASE_NUM1:
		/* COMBPHY1 PORT B */
		bsp_sata_phy1_portb_clk_sel(comb_phy_crg_base);
		break;
	case CASE_NUM2:
		/* COMBPHY1 PORT B */
		bsp_sata_phy1_portb_clk_sel(comb_phy_crg_base);

		/* COMBPHY1 PORT A */
		bsp_sata_phy1_porta_clk_sel(comb_phy_crg_base);
		break;
	case CASE_NUM3:
		/* COMBPHY1 PORT B */
		bsp_sata_phy1_portb_clk_sel(comb_phy_crg_base);

		/* COMBPHY1 PORT A */
		bsp_sata_phy1_porta_clk_sel(comb_phy_crg_base);

		/* COMBPHY2 PORT B */
		bsp_sata_phy2_portb_clk_sel(comb_phy_crg_base);
		break;
	case CASE_NUM4:
		/* COMBPHY1 PORT B */
		bsp_sata_phy1_portb_clk_sel(comb_phy_crg_base);

		/* COMBPHY1 PORT A */
		bsp_sata_phy1_porta_clk_sel(comb_phy_crg_base);

		/* COMBPHY2 PORT B */
		bsp_sata_phy2_portb_clk_sel(comb_phy_crg_base);

		/* COMBPHY2 PORT A */
		bsp_sata_phy2_porta_clk_sel(comb_phy_crg_base);
		break;
	default:
		break;
	}

	iounmap(comb_phy_crg_base);
}

void bsp_sata_set_fifoth(void *mmio)
{
	unsigned int port_idx;

	for (port_idx = 0; port_idx < ports_num; port_idx++)
		writel(FIFOTH_VALUE, (mmio + 0x100 + port_idx * 0x80 + PORT_FIFOTH));
}
EXPORT_SYMBOL(bsp_sata_set_fifoth);

static void bsp_sata_phy1_portb_config(void *misc_base)
{
	writel(0x88200, misc_base + BSP_COMB_PHY1_TEST_CTRL);
	writel(0x88201, misc_base + BSP_COMB_PHY1_TEST_CTRL);
	writel(0x88200, misc_base + BSP_COMB_PHY1_TEST_CTRL);
	writel(0x0, misc_base + BSP_COMB_PHY1_TEST_CTRL);

	writel(0x19100, misc_base + BSP_COMB_PHY1_TEST_CTRL);
	writel(0x19101, misc_base + BSP_COMB_PHY1_TEST_CTRL);
	writel(0x19100, misc_base + BSP_COMB_PHY1_TEST_CTRL);
	writel(0x0, misc_base + BSP_COMB_PHY1_TEST_CTRL);

	writel(0xC08C00, misc_base + BSP_COMB_PHY1_TEST_CTRL);
	writel(0xC08C01, misc_base + BSP_COMB_PHY1_TEST_CTRL);
	writel(0xC08C00, misc_base + BSP_COMB_PHY1_TEST_CTRL);
	writel(0x0, misc_base + BSP_COMB_PHY1_TEST_CTRL);

	writel(0xd88d00, misc_base + BSP_COMB_PHY1_TEST_CTRL);
	writel(0xd88d01, misc_base + BSP_COMB_PHY1_TEST_CTRL);
	writel(0xd88d00, misc_base + BSP_COMB_PHY1_TEST_CTRL);
	writel(0x0, misc_base + BSP_COMB_PHY1_TEST_CTRL);

	writel(0x48700, misc_base + BSP_COMB_PHY1_TEST_CTRL);
	writel(0x48701, misc_base + BSP_COMB_PHY1_TEST_CTRL);
	writel(0x48700, misc_base + BSP_COMB_PHY1_TEST_CTRL);
	writel(0x0, misc_base + BSP_COMB_PHY1_TEST_CTRL);

	writel(0x58300, misc_base + BSP_COMB_PHY1_TEST_CTRL);
	writel(0x58301, misc_base + BSP_COMB_PHY1_TEST_CTRL);
	writel(0x58300, misc_base + BSP_COMB_PHY1_TEST_CTRL);
	writel(0x0, misc_base + BSP_COMB_PHY1_TEST_CTRL);
}

static void bsp_sata_phy1_porta_config(void *misc_base)
{
	writel(0x80200, misc_base + BSP_COMB_PHY1_TEST_CTRL);
	writel(0x80201, misc_base + BSP_COMB_PHY1_TEST_CTRL);
	writel(0x80200, misc_base + BSP_COMB_PHY1_TEST_CTRL);
	writel(0x0, misc_base + BSP_COMB_PHY1_TEST_CTRL);

	writel(0x11100, misc_base + BSP_COMB_PHY1_TEST_CTRL);
	writel(0x11101, misc_base + BSP_COMB_PHY1_TEST_CTRL);
	writel(0x11100, misc_base + BSP_COMB_PHY1_TEST_CTRL);
	writel(0x0, misc_base + BSP_COMB_PHY1_TEST_CTRL);

	writel(0xC00C00, misc_base + BSP_COMB_PHY1_TEST_CTRL);
	writel(0xC00C01, misc_base + BSP_COMB_PHY1_TEST_CTRL);
	writel(0xC00C00, misc_base + BSP_COMB_PHY1_TEST_CTRL);
	writel(0x0, misc_base + BSP_COMB_PHY1_TEST_CTRL);

	writel(0xd80d00, misc_base + BSP_COMB_PHY1_TEST_CTRL);
	writel(0xd80d01, misc_base + BSP_COMB_PHY1_TEST_CTRL);
	writel(0xd80d00, misc_base + BSP_COMB_PHY1_TEST_CTRL);
	writel(0x0, misc_base + BSP_COMB_PHY1_TEST_CTRL);

	writel(0x40700, misc_base + BSP_COMB_PHY1_TEST_CTRL);
	writel(0x40701, misc_base + BSP_COMB_PHY1_TEST_CTRL);
	writel(0x40700, misc_base + BSP_COMB_PHY1_TEST_CTRL);
	writel(0x0, misc_base + BSP_COMB_PHY1_TEST_CTRL);

	writel(0x50300, misc_base + BSP_COMB_PHY1_TEST_CTRL);
	writel(0x50301, misc_base + BSP_COMB_PHY1_TEST_CTRL);
	writel(0x50300, misc_base + BSP_COMB_PHY1_TEST_CTRL);
	writel(0x0, misc_base + BSP_COMB_PHY1_TEST_CTRL);
}

static void bsp_sata_phy2_portb_config(void *misc_base)
{
	writel(0x88200, misc_base + BSP_COMB_PHY2_TEST_CTRL);
	writel(0x88201, misc_base + BSP_COMB_PHY2_TEST_CTRL);
	writel(0x88200, misc_base + BSP_COMB_PHY2_TEST_CTRL);
	writel(0x0, misc_base + BSP_COMB_PHY2_TEST_CTRL);

	writel(0x19100, misc_base + BSP_COMB_PHY2_TEST_CTRL);
	writel(0x19101, misc_base + BSP_COMB_PHY2_TEST_CTRL);
	writel(0x19100, misc_base + BSP_COMB_PHY2_TEST_CTRL);
	writel(0x0, misc_base + BSP_COMB_PHY2_TEST_CTRL);

	writel(0xC08C00, misc_base + BSP_COMB_PHY2_TEST_CTRL);
	writel(0xC08C01, misc_base + BSP_COMB_PHY2_TEST_CTRL);
	writel(0xC08C00, misc_base + BSP_COMB_PHY2_TEST_CTRL);
	writel(0x0, misc_base + BSP_COMB_PHY2_TEST_CTRL);

	writel(0xd88d00, misc_base + BSP_COMB_PHY2_TEST_CTRL);
	writel(0xd88d01, misc_base + BSP_COMB_PHY2_TEST_CTRL);
	writel(0xd88d00, misc_base + BSP_COMB_PHY2_TEST_CTRL);
	writel(0x0, misc_base + BSP_COMB_PHY2_TEST_CTRL);

	writel(0x48700, misc_base + BSP_COMB_PHY2_TEST_CTRL);
	writel(0x48701, misc_base + BSP_COMB_PHY2_TEST_CTRL);
	writel(0x48700, misc_base + BSP_COMB_PHY2_TEST_CTRL);
	writel(0x0, misc_base + BSP_COMB_PHY2_TEST_CTRL);

	writel(0x58300, misc_base + BSP_COMB_PHY2_TEST_CTRL);
	writel(0x58301, misc_base + BSP_COMB_PHY2_TEST_CTRL);
	writel(0x58300, misc_base + BSP_COMB_PHY2_TEST_CTRL);
	writel(0x0, misc_base + BSP_COMB_PHY2_TEST_CTRL);
}

static void bsp_sata_phy2_porta_config(void *misc_base)
{
	writel(0x80200, misc_base + BSP_COMB_PHY2_TEST_CTRL);
	writel(0x80201, misc_base + BSP_COMB_PHY2_TEST_CTRL);
	writel(0x80200, misc_base + BSP_COMB_PHY2_TEST_CTRL);
	writel(0x0, misc_base + BSP_COMB_PHY2_TEST_CTRL);

	writel(0x11100, misc_base + BSP_COMB_PHY2_TEST_CTRL);
	writel(0x11101, misc_base + BSP_COMB_PHY2_TEST_CTRL);
	writel(0x11100, misc_base + BSP_COMB_PHY2_TEST_CTRL);
	writel(0x0, misc_base + BSP_COMB_PHY2_TEST_CTRL);

	writel(0xC00C00, misc_base + BSP_COMB_PHY2_TEST_CTRL);
	writel(0xC00C01, misc_base + BSP_COMB_PHY2_TEST_CTRL);
	writel(0xC00C00, misc_base + BSP_COMB_PHY2_TEST_CTRL);
	writel(0x0, misc_base + BSP_COMB_PHY2_TEST_CTRL);

	writel(0xd80d00, misc_base + BSP_COMB_PHY2_TEST_CTRL);
	writel(0xd80d01, misc_base + BSP_COMB_PHY2_TEST_CTRL);
	writel(0xd80d00, misc_base + BSP_COMB_PHY2_TEST_CTRL);
	writel(0x0, misc_base + BSP_COMB_PHY2_TEST_CTRL);

	writel(0x40700, misc_base + BSP_COMB_PHY2_TEST_CTRL);
	writel(0x40701, misc_base + BSP_COMB_PHY2_TEST_CTRL);
	writel(0x40700, misc_base + BSP_COMB_PHY2_TEST_CTRL);
	writel(0x0, misc_base + BSP_COMB_PHY2_TEST_CTRL);

	writel(0x50300, misc_base + BSP_COMB_PHY2_TEST_CTRL);
	writel(0x50301, misc_base + BSP_COMB_PHY2_TEST_CTRL);
	writel(0x50300, misc_base + BSP_COMB_PHY2_TEST_CTRL);
	writel(0x0, misc_base + BSP_COMB_PHY2_TEST_CTRL);
}

static void bsp_sata_port_cfg(void)
{
	void *misc_base = NULL;

	misc_base = ioremap(BSP_MISC_REG_BASE, BSP_MISC_REG_MAP_SIZE);
	if (!misc_base) {
		pr_err("ioremap misc reg failed! func:%s, line:%d\n", __func__, __LINE__);
		return;
	}

	switch (ports_num) {
	case CASE_NUM1:
		/* cfg COMBPHY1 PORT B */
		bsp_sata_phy1_portb_config(misc_base);
		break;
	case CASE_NUM2:
		/* cfg COMBPHY1 PORT B */
		bsp_sata_phy1_portb_config(misc_base);

		/* cfg COMBPHY1 PORT A */
		bsp_sata_phy1_porta_config(misc_base);
		break;
	case CASE_NUM3:
		/* cfg COMBPHY1 PORT B */
		bsp_sata_phy1_portb_config(misc_base);

		/* cfg COMBPHY1 PORT A */
		bsp_sata_phy1_porta_config(misc_base);

		/* cfg COMBPHY2 PORT B */
		bsp_sata_phy2_portb_config(misc_base);
		break;
	case CASE_NUM4:
		/* cfg COMBPHY1 PORT B */
		bsp_sata_phy1_portb_config(misc_base);

		/* cfg COMBPHY1 PORT A */
		bsp_sata_phy1_porta_config(misc_base);

		/* cfg COMBPHY2 PORT B */
		bsp_sata_phy2_portb_config(misc_base);

		/* cfg COMBPHY2 PORT A */
		bsp_sata_phy2_porta_config(misc_base);
		break;
	default:
		break;
	}

	iounmap(misc_base);
}

#define IOCONFIG1_REG_BASE	0x10ff0000
#define IOCONFIG1_REG_MAP_SIZE	0x80
static void bsp_sata_set_led(void)
{
	void *reg_addr = NULL;
	unsigned int port_idx;

	reg_addr = ioremap(IOCONFIG1_REG_BASE, IOCONFIG1_REG_MAP_SIZE);
	if (reg_addr == NULL) {
		pr_err("ioremap ioconfg1 register addr for sata failed! func:%s, line:%d\n",
		       __func__, __LINE__);
		return;
	}

	/* Set SATA_LED_N */
	for (port_idx = 0; port_idx < ports_num; port_idx++) {
		if (port_idx == 0)
			writel(0x1201, reg_addr + 0x48);
		else if (port_idx == 1)
			writel(0x1201, reg_addr + 0x4c);
		else if (port_idx == 2) /* 2  ports_num */
			writel(0x1201, reg_addr + 0x50);
		else
			writel(0x1201, reg_addr + 0x54);
	}

	iounmap(reg_addr);
}

static void bsp_sata_phy_config(void *mmio, int phy_mode)
{
	unsigned int phy_config = PHY_SG_6G;
	unsigned int port_idx;
	unsigned int val;
	void *misc_base = NULL;

	bsp_sata_set_fifoth(mmio);

	/* Set SATA_LED_N */
	bsp_sata_set_led();

	/* set phy PX TX amplitude */
	/* set phy PX TX pre-emphasis */
	for (port_idx = 0; port_idx < ports_num; port_idx++) {
		writel(PX_TX_PREEMPH, (mmio + 0x100 + port_idx * 0x80 + BSP_SATA_PORT_PHYCTL2));
		writel(PX_TX_AMPLITUDE, (mmio + 0x100 + port_idx * 0x80 + BSP_SATA_PORT_PHYCTL1));
		writel(phy_config, (mmio + 0x100 + port_idx * 0x80 + BSP_SATA_PORT_PHYCTL3));
	}

	misc_base = ioremap(BSP_MISC_REG_BASE, BSP_MISC_REG_MAP_SIZE);
	if (!misc_base) {
		pr_err("ioremap misc reg failed! func:%s, line:%d\n", __func__, __LINE__);
		return;
	}

	switch (ports_num) {
	case CASE_NUM1:
		val = readl(misc_base + SATA_PHY_CTRL0);
		val |= P0_PHY_SERDES_ARCH;
		writel(val, misc_base + SATA_PHY_CTRL0);
		break;
	case CASE_NUM2:
		val = readl(misc_base + SATA_PHY_CTRL0);
		val |= P0_PHY_SERDES_ARCH | P1_PHY_SERDES_ARCH;
		writel(val, misc_base + SATA_PHY_CTRL0);
		break;
	case CASE_NUM3:
		val = readl(misc_base + SATA_PHY_CTRL0);
		val |= P0_PHY_SERDES_ARCH | P1_PHY_SERDES_ARCH;
		writel(val, misc_base + SATA_PHY_CTRL0);

		val = readl(misc_base + SATA_PHY_CTRL1);
		val |= P2_PHY_SERDES_ARCH;
		writel(val, misc_base + SATA_PHY_CTRL1);
		break;
	case CASE_NUM4:
		val = readl(misc_base + SATA_PHY_CTRL0);
		val |= P0_PHY_SERDES_ARCH | P1_PHY_SERDES_ARCH;
		writel(val, misc_base + SATA_PHY_CTRL0);

		val = readl(misc_base + SATA_PHY_CTRL1);
		val |= P2_PHY_SERDES_ARCH | P3_PHY_SERDES_ARCH;
		writel(val, misc_base + SATA_PHY_CTRL1);
		break;
	default:
		break;
	}

	iounmap(misc_base);
	bsp_sata_port_cfg();
}

unsigned int bsp_sata_get_port_info(void)
{
	unsigned int ups_mode;
	unsigned int reg_val;
	unsigned int sata_port_num;
	void *sysctrl_reg_base = NULL;

	sysctrl_reg_base = ioremap(BSP_SYS_CTRL_REG_BASE, BSP_SYS_CTRL_REG_MAP_SIZE);
	if (!sysctrl_reg_base) {
		pr_err("ioremap sysctrl reg base failed! func:%s, line:%d\n", __func__,
		       __LINE__);
		return 0;
	}

	reg_val = readl(sysctrl_reg_base + BSP_SYS_STAT_REG);
	ups_mode = get_ups_mode_val(reg_val);
	switch (ups_mode) {
	case CASE_NUM1:
		sata_port_num = 1;
		sata_port_map = 0x1;
		break;
	case CASE_NUM2:
	case CASE_NUM3:
		sata_port_num = 2; /* 2 sata_port_num */
		sata_port_map = 0x3;
		break;
	case CASE_NUM4:
		sata_port_num = 3;
		sata_port_map = 0x7;
		break;
	case CASE_NUM5:
		sata_port_num = 4;
		sata_port_map = 0xf;
		break;
	default:
		sata_port_num = 0;
		sata_port_map = 0;
		break;
	}

	return sata_port_num;
}
