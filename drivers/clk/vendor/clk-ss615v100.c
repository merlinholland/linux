/*
 * SS615V100 Clock Driver
 *
 * Copyright (c) 2021 Shenshu Technologies Co., Ltd.
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

#include <dt-bindings/clock/ss615v100-clock.h>
#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include "clk.h"
#include "crg.h"
#include "reset.h"

static struct bsp_fixed_rate_clock ss615v100_fixed_rate_clks[] __initdata = {
	{ SS615V100_FIXED_100K, "100k", NULL, 0, 100000, },
	{ SS615V100_FIXED_400K, "400k", NULL, 0, 400000, },
	{ SS615V100_FIXED_3M, "3m", NULL, 0, 3000000, },
	{ SS615V100_FIXED_6M, "6m", NULL, 0, 6000000, },
	{ SS615V100_FIXED_24M, "24m", NULL, 0, 24000000, },
	{ SS615V100_FIXED_24P75M, "24p75m", NULL, 0, 24750000, },
	{ SS615V100_FIXED_49P5M, "49p5m", NULL, 0, 49500000, },
	{ SS615V100_FIXED_50M, "50m", NULL, 0, 50000000, },
	{ SS615V100_FIXED_54M, "54m", NULL, 0, 54000000, },
	{ SS615V100_FIXED_90M, "90m", NULL, 0, 90000000, },
	{ SS615V100_FIXED_99M, "99m", NULL, 0, 99000000, },
	{ SS615V100_FIXED_112M, "112m", NULL, 0, 112000000, },
	{ SS615V100_FIXED_125M, "125m", NULL, 0, 125000000, },
	{ SS615V100_FIXED_148P5M, "148p5m", NULL, 0, 148500000, },
	{ SS615V100_FIXED_198M, "198m", NULL, 0, 198000000, },
	{ SS615V100_FIXED_250M, "250m", NULL, 0, 250000000, },
	{ SS615V100_FIXED_297M, "297m", NULL, 0, 297000000, },
	{ SS615V100_FIXED_324M, "324m", NULL, 0, 324000000, },
	{ SS615V100_FIXED_342M, "342m", NULL, 0, 342000000, },
	{ SS615V100_FIXED_342M, "375m", NULL, 0, 375000000, },
	{ SS615V100_FIXED_396M, "396m", NULL, 0, 396000000, },
	{ SS615V100_FIXED_448M, "448m", NULL, 0, 448000000, },
	{ SS615V100_FIXED_500M, "500m", NULL, 0, 500000000, },
	{ SS615V100_FIXED_540M, "540m", NULL, 0, 540000000, },
	{ SS615V100_FIXED_600M, "600m", NULL, 0, 600000000, },
	{ SS615V100_FIXED_750M, "750m", NULL, 0, 750000000, },
	{ SS615V100_FIXED_900M, "900m", NULL, 0, 900000000, },
	{ SS615V100_FIXED_1200M, "1200m", NULL, 0, 1200000000UL, },
};

static const char *sysaxi_mux_p[] __initconst =
			{"24m", "198m", "396m", "297m"};
static u32 sysaxi_mux_table[] = {0, 1, 2, 3};

static const char *syscfg_mux_p[] __initconst = {"24m", "99m", "198m"};
static u32 syscfg_mux_table[] = {0, 1, 2};

static const char *fmc_mux_p[] __initconst = {
	"24m", "99m", "148p5m", "198m", "250m", "297m", "396m"
};
static u32 fmc_mux_table[] = {0, 1, 3, 4, 5, 6, 7};

static const char *mmc_mux_p[] __initdata = {
	"400k", "24p75m", "49p5m", "99m", "148p5m", "100k", "6m"
};
static u32 mmc_mux_table[] = {0, 1, 2, 3, 4, 5, 6};

static const char *eth_mux_p[] __initconst = {"100m", "54m"};
static u32 eth_mux_table[] = {0, 1};

static const char *uart_mux_p[] __initconst = {"clk_sysapb", "24m", "3m"};
static u32 uart_mux_table[] = {0, 1, 2};

static const char *i2c_mux_p[] __initconst = {"clk_sysapb", "50m"};
static u32 i2c_mux_table[] = {0, 1};

static struct bsp_mux_clock ss615v100_mux_clks[] __initdata = {
	{
		SS615V100_SYSAXI_CLK, "sysaxi_mux", sysaxi_mux_p,
		ARRAY_SIZE(sysaxi_mux_p),
		CLK_SET_RATE_PARENT, 0x2000, 0, 2, 0, sysaxi_mux_table,
	},
	{
		SS615V100_SYSCFG_CLK, "syscfg_mux", syscfg_mux_p,
		ARRAY_SIZE(syscfg_mux_p),
		CLK_SET_RATE_PARENT, 0x2000, 4, 2, 0, syscfg_mux_table,
	},
	{
		SS615V100_FMC_MUX, "fmc_mux",
		fmc_mux_p, ARRAY_SIZE(fmc_mux_p),
		CLK_SET_RATE_PARENT, 0x3f40, 12, 3, 0, fmc_mux_table,
	},
	{
		SS615V100_MMC0_MUX, "mmc0_mux",
		mmc_mux_p, ARRAY_SIZE(mmc_mux_p),
		CLK_SET_RATE_PARENT, 0x34c0, 24, 3, 0, mmc_mux_table,
	},
	{
		SS615V100_ETH_MUX, "eth_mux",
		eth_mux_p, ARRAY_SIZE(eth_mux_p),
		CLK_SET_RATE_PARENT, 0x37c8, 12, 1, 0, eth_mux_table,
	},

	{
		SS615V100_UART0_MUX, "uart0_mux", uart_mux_p,
		ARRAY_SIZE(uart_mux_p),
		CLK_SET_RATE_PARENT, 0x4180, 12, 2, 0, uart_mux_table,
	},
	{
		SS615V100_UART1_MUX, "uart1_mux", uart_mux_p,
		ARRAY_SIZE(uart_mux_p),
		CLK_SET_RATE_PARENT, 0x4188, 12, 2, 0, uart_mux_table,
	},
	{
		SS615V100_UART2_MUX, "uart2_mux", uart_mux_p,
		ARRAY_SIZE(uart_mux_p),
		CLK_SET_RATE_PARENT, 0x4190, 12, 2, 0, uart_mux_table,
	},
	{
		SS615V100_UART3_MUX, "uart3_mux", uart_mux_p,
		ARRAY_SIZE(uart_mux_p),
		CLK_SET_RATE_PARENT, 0x4198, 12, 2, 0, uart_mux_table,
	},
	{
		SS615V100_UART4_MUX, "uart4_mux", uart_mux_p,
		ARRAY_SIZE(uart_mux_p),
		CLK_SET_RATE_PARENT, 0x41a0, 12, 2, 0, uart_mux_table,
	},
	{
		SS615V100_I2C0_MUX, "i2c0_mux",
		i2c_mux_p, ARRAY_SIZE(i2c_mux_p),
		CLK_SET_RATE_PARENT, 0x4280, 12, 1, 0, i2c_mux_table,
	},
	{
		SS615V100_I2C1_MUX, "i2c1_mux",
		i2c_mux_p, ARRAY_SIZE(i2c_mux_p),
		CLK_SET_RATE_PARENT, 0x4288, 12, 1, 0, i2c_mux_table,
	},
};

static struct bsp_fixed_factor_clock
	ss615v100_fixed_factor_clks[] __initdata = {
	{
		SS615V100_SYSAPB_CLK, "clk_sysapb", "syscfg_mux", 1, 2,
		CLK_SET_RATE_PARENT
	},
};

static struct bsp_gate_clock ss615v100_gate_clks[] __initdata = {
	{
		SS615V100_FMC_CLK, "clk_fmc", "fmc_mux",
		CLK_SET_RATE_PARENT, 0x3f40, 4, 0,
	},
	{
		SS615V100_MMC0_CLK, "clk_mmc0", "mmc0_mux",
		CLK_SET_RATE_PARENT, 0x34c0, 0, 0,
	},
	{
		SS615V100_UART0_CLK, "clk_uart0", "uart0_mux",
		CLK_SET_RATE_PARENT, 0x4180, 4, 0,
	},
	{
		SS615V100_UART1_CLK, "clk_uart1", "uart1_mux",
		CLK_SET_RATE_PARENT, 0x4188, 4, 0,
	},
	{
		SS615V100_UART2_CLK, "clk_uart2", "uart2_mux",
		CLK_SET_RATE_PARENT, 0x4190, 4, 0,
	},
	{
		SS615V100_UART3_CLK, "clk_uart3", "uart3_mux",
		CLK_SET_RATE_PARENT, 0x4198, 4, 0,
	},
	{
		SS615V100_UART4_CLK, "clk_uart4", "uart4_mux",
		CLK_SET_RATE_PARENT, 0x41a0, 4, 0,
	},
	{
		SS615V100_I2C0_CLK, "clk_i2c0", "i2c0_mux",
		CLK_SET_RATE_PARENT, 0x4280, 4, 0,
	},
	{
		SS615V100_I2C1_CLK, "clk_i2c1", "i2c1_mux",
		CLK_SET_RATE_PARENT, 0x4288, 4, 0,
	},
	{
		SS615V100_SPI0_CLK, "clk_spi0", "clk_sysapb",
		CLK_SET_RATE_PARENT, 0x4480, 4, 0,
	},
	{
		SS615V100_SPI1_CLK, "clk_spi1", "clk_sysapb",
		CLK_SET_RATE_PARENT, 0x4488, 4, 0,
	},
	{
		SS615V100_ETH0_CLK, "clk_eth", NULL,
		CLK_SET_RATE_PARENT, 0x37c4, 4, 0,
	},
	{
		SS615V100_ETH0_MACIF_CLK, "clk_eth_macif", NULL,
		CLK_SET_RATE_PARENT, 0x37c0, 4, 0,
	},
	{
		SS615V100_ETH1_CLK, "clk_eth1", "eth_mux",
		CLK_SET_RATE_PARENT, 0x37c8, 4, 0,
	},
	{
		SS615V100_ETH1_FEPHY_CLK, "clk_eth1_fephy", NULL,
		CLK_SET_RATE_PARENT, 0x37d0, 4, 0,
	},
	{
		SS615V100_EDMAC_AXICLK, "axi_clk_edmac", NULL,
		CLK_SET_RATE_PARENT, 0x2a80, 5, 0,
	},
	{
		SS615V100_EDMAC_CLK, "clk_edmac", NULL,
		CLK_SET_RATE_PARENT, 0x2a80, 4, 0,
	},
};

static void __init ss615v100_clk_init(struct device_node *np)
{
	struct bsp_clock_data *clk_data;

	clk_data = bsp_clk_init(np, SS615V100_NR_CLKS);
	if (!clk_data)
		return;
	if (IS_ENABLED(CONFIG_RESET_CONTROLLER))
		bsp_reset_init(np, SS615V100_NR_RSTS);

	bsp_clk_register_fixed_rate(ss615v100_fixed_rate_clks,
				     ARRAY_SIZE(ss615v100_fixed_rate_clks),
				     clk_data);
	bsp_clk_register_mux(ss615v100_mux_clks,
			      ARRAY_SIZE(ss615v100_mux_clks), clk_data);

	bsp_clk_register_fixed_factor(ss615v100_fixed_factor_clks,
			ARRAY_SIZE(ss615v100_fixed_factor_clks), clk_data);

	bsp_clk_register_gate(ss615v100_gate_clks,
			       ARRAY_SIZE(ss615v100_gate_clks), clk_data);
}

CLK_OF_DECLARE(ss615v100_clk, "vendor,ss615v100-clock",
	       ss615v100_clk_init);

