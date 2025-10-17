/*
 * SS928V100 Clock Driver
 *
 * Copyright (c) 2016-2017 Shenshu Technologies Co., Ltd.
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

#include <dt-bindings/clock/ss928v100_clock.h>
#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "clk.h"
#include "crg.h"
#include "reset.h"

struct ss928v100_pll_clock {
	u32     id;
	const char  *name;
	const char  *parent_name;
	u32     ctrl_reg1;
	u8      frac_shift;
	u8      frac_width;
	u8      postdiv1_shift;
	u8      postdiv1_width;
	u8      postdiv2_shift;
	u8      postdiv2_width;
	u32     ctrl_reg2;
	u8      fbdiv_shift;
	u8      fbdiv_width;
	u8      refdiv_shift;
	u8      refdiv_width;
};

struct ss928v100_clk_pll {
	struct clk_hw   hw;
	u32     id;
	void __iomem    *ctrl_reg1;
	u8      frac_shift;
	u8      frac_width;
	u8      postdiv1_shift;
	u8      postdiv1_width;
	u8      postdiv2_shift;
	u8      postdiv2_width;
	void __iomem    *ctrl_reg2;
	u8      fbdiv_shift;
	u8      fbdiv_width;
	u8      refdiv_shift;
	u8      refdiv_width;
};

/* soc clk config */
static const struct bsp_fixed_rate_clock ss928v100_fixed_rate_clks_crg[] = {
	{ SS928V100_FIXED_2400M, "2400m",   NULL, 0, 2400000000, },
	{ SS928V100_FIXED_1200M, "1200m",   NULL, 0, 1200000000, },
	{ SS928V100_FIXED_1188M, "1188m",   NULL, 0, 1188000000, },
	{ SS928V100_FIXED_896M, "896m",    NULL, 0, 896000000, },
	{ SS928V100_FIXED_800M, "800m",    NULL, 0, 800000000, },
	{ SS928V100_FIXED_792M, "792m",    NULL, 0, 792000000, },
	{ SS928V100_FIXED_786M, "786m",    NULL, 0, 786000000, },
	{ SS928V100_FIXED_750M, "750m",    NULL, 0, 750000000, },
	{ SS928V100_FIXED_700M, "700m",    NULL, 0, 700000000, },
	{ SS928V100_FIXED_672M, "672m",    NULL, 0, 672000000, },
	{ SS928V100_FIXED_600M, "600m",    NULL, 0, 600000000, },
	{ SS928V100_FIXED_594M, "594m",    NULL, 0, 594000000, },
	{ SS928V100_FIXED_560M, "560m",    NULL, 0, 560000000, },
	{ SS928V100_FIXED_500M, "500m",    NULL, 0, 500000000, },
	{ SS928V100_FIXED_475M, "475m",    NULL, 0, 475000000, },
	{ SS928V100_FIXED_396M, "396m",    NULL, 0, 396000000, },
	{ SS928V100_FIXED_300M, "300m",    NULL, 0, 300000000, },
	{ SS928V100_FIXED_297M, "297m",    NULL, 0, 297000000, },
	{ SS928V100_FIXED_257M, "257m",    NULL, 0, 257000000, },
	{ SS928V100_FIXED_250M, "250m",    NULL, 0, 250000000, },
	{ SS928V100_FIXED_200M, "200m",    NULL, 0, 200000000, },
	{ SS928V100_FIXED_198M, "198m",    NULL, 0, 198000000, },
	{ SS928V100_FIXED_187P_5M, "187p5m",   NULL, 0, 187500000, },
	{ SS928V100_FIXED_150M, "150m",    NULL, 0, 150000000, },
	{ SS928V100_FIXED_148P_5M, "148p5m",  NULL, 0, 148500000, },
	{ SS928V100_FIXED_134M, "134m",    NULL, 0, 134000000, },
	{ SS928V100_FIXED_108M, "108m",    NULL, 0, 108000000, },
	{ SS928V100_FIXED_100M, "100m",    NULL, 0, 100000000, },
	{ SS928V100_FIXED_99M, "99m",     NULL, 0, 99000000, },
	{ SS928V100_FIXED_74P_25M, "74p25m",  NULL, 0, 74250000, },
	{ SS928V100_FIXED_72M, "72m",     NULL, 0, 72000000, },
	{ SS928V100_FIXED_64M, "64m",     NULL, 0, 64000000, },
	{ SS928V100_FIXED_60M, "60m",     NULL, 0, 60000000, },
	{ SS928V100_FIXED_54M, "54m",     NULL, 0, 54000000, },
	{ SS928V100_FIXED_50M, "50m",     NULL, 0, 50000000, },
	{ SS928V100_FIXED_49P_5M, "49p5m",   NULL, 0, 49500000, },
	{ SS928V100_FIXED_37P_125M, "37p125m", NULL, 0, 37125000, },
	{ SS928V100_FIXED_36M, "36m",     NULL, 0, 36000000, },
	{ SS928V100_FIXED_27M, "27m",     NULL, 0, 27000000, },
	{ SS928V100_FIXED_25M, "25m",     NULL, 0, 25000000, },
	{ SS928V100_FIXED_24M, "24m",     NULL, 0, 24000000, },
	{ SS928V100_FIXED_12M, "12m",     NULL, 0, 12000000, },
	{ SS928V100_FIXED_12P_288M, "12p288m", NULL, 0, 12288000, },
	{ SS928V100_FIXED_6M, "6m",      NULL, 0, 6000000, },
	{ SS928V100_FIXED_3M, "3m",      NULL, 0, 3000000, },
	{ SS928V100_FIXED_1P_6M, "1p6m",    NULL, 0, 1600000, },
	{ SS928V100_FIXED_400K, "400k",    NULL, 0, 400000, },
	{ SS928V100_FIXED_100K, "100k",    NULL, 0, 100000, },
};


static const char *fmc_mux_p[] __initdata = {
	"24m", "99m", "148p5m", "198m", "250m", "297m", "396m"
};
static u32 fmc_mux_table[] = {0, 1, 2, 3, 4, 5, 6};

static const char *mmc_mux_p[] __initdata = {
	"400k", "25m", "50m", "100m", "150m", "187p5m", "200m"
};
static u32 mmc_mux_table[] = {0, 1, 2, 3, 4, 5, 6};

static const char *sdio0_mux_p[] __initdata = {
	"400k", "25m", "50m", "100m", "150m", "187p5m", "200m"
};
static u32 sdio0_mux_table[] = {0, 1, 2, 3, 4, 5, 6};

static const char *sdio1_mux_p[] __initdata = {
	"400k", "25m", "50m", "100m", "150m", "187p5m", "200m"
};
static u32 sdio1_mux_table[] = {0, 1, 2, 3, 4, 5, 6};

static const char *hpaxi_mux_p[] __initdata = {
	"24m", "396m", "475m"
};
static u32 hpaxi_mux_table[] = {0, 1, 2};

static const char *ddraxi_mux_p[] __initdata = {
	"24m", "500m", "672m", "750m"
};
static u32 ddraxi_mux_table[] = {0, 1, 2, 3};

static const char *uart_mux_p[] __initdata = {"50m", "24m", "3m", "100m"};
static u32 uart_mux_table[] = {0, 1, 2, 3};

static const char *i2c_mux_p[] __initdata = {
	"50m", "100m"
};
static u32 i2c_mux_table[] = {0, 1};

static const char * pwm0_mux_p[] __initdata = {"200m"};
static u32 pwm0_mux_table[] = {0};

static const char * pwm1_mux_p[] __initdata = {"200m"};
static u32 pwm1_mux_table[] = {0};

static struct bsp_mux_clock ss928v100_mux_clks_crg[] __initdata = {
	{
		SS928V100_FMC_MUX, "fmc_mux",
		fmc_mux_p, ARRAY_SIZE(fmc_mux_p),
		CLK_SET_RATE_PARENT, 0x3f40, 12, 3, 0, fmc_mux_table,
	},
	{
		SS928V100_MMC0_MUX, "mmc0_mux",
		mmc_mux_p, ARRAY_SIZE(mmc_mux_p),
		CLK_SET_RATE_PARENT, 0x34c0, 24, 3, 0, mmc_mux_table,
	},
	{
		SS928V100_MMC1_MUX, "mmc1_mux",
		sdio0_mux_p, ARRAY_SIZE(sdio0_mux_p),
		CLK_SET_RATE_PARENT, 0x35c0, 24, 3, 0, sdio0_mux_table,
	},
	{
		SS928V100_MMC2_MUX, "mmc2_mux",
		sdio1_mux_p, ARRAY_SIZE(sdio1_mux_p),
		CLK_SET_RATE_PARENT, 0x36c0, 24, 3, 0, sdio1_mux_table,
	},
	{
		SS928V100_HPAXI_MUX, "hpaxi_mux",
		hpaxi_mux_p, ARRAY_SIZE(hpaxi_mux_p),
		CLK_SET_RATE_PARENT, 0x2000, 16, 2, 0, hpaxi_mux_table
	},
	{
		SS928V100_DDRAXI_MUX, "ddraxi_mux",
		ddraxi_mux_p, ARRAY_SIZE(ddraxi_mux_p),
		CLK_SET_RATE_PARENT, 0x2000, 12, 2, 0, ddraxi_mux_table
	},
	{
		SS928V100_UART0_MUX, "uart0_mux",
		uart_mux_p, ARRAY_SIZE(uart_mux_p),
		CLK_SET_RATE_PARENT, 0x4180, 12, 2, 0, uart_mux_table
	},
	{
		SS928V100_UART1_MUX, "uart1_mux",
		uart_mux_p, ARRAY_SIZE(uart_mux_p),
		CLK_SET_RATE_PARENT, 0x4188, 12, 2, 0, uart_mux_table
	},
	{
		SS928V100_UART2_MUX, "uart2_mux",
		uart_mux_p, ARRAY_SIZE(uart_mux_p),
		CLK_SET_RATE_PARENT, 0x4190, 12, 2, 0, uart_mux_table
	},
	{
		SS928V100_UART3_MUX, "uart3_mux",
		uart_mux_p, ARRAY_SIZE(uart_mux_p),
		CLK_SET_RATE_PARENT, 0x4198, 12, 2, 0, uart_mux_table
	},
	{
		SS928V100_UART4_MUX, "uart4_mux",
		uart_mux_p, ARRAY_SIZE(uart_mux_p),
		CLK_SET_RATE_PARENT, 0x41a0, 12, 2, 0, uart_mux_table
	},
	{
	    SS928V100_UART5_MUX, "uart5_mux",
		uart_mux_p, ARRAY_SIZE(uart_mux_p),
		CLK_SET_RATE_PARENT, 0x41a8, 12, 2, 0, uart_mux_table
	},
	{
		SS928V100_I2C0_MUX, "i2c0_mux",
		i2c_mux_p, ARRAY_SIZE(i2c_mux_p),
		CLK_SET_RATE_PARENT, 0x4280, 12, 1, 0, i2c_mux_table
	},
	{
		SS928V100_I2C1_MUX, "i2c1_mux",
		i2c_mux_p, ARRAY_SIZE(i2c_mux_p),
		CLK_SET_RATE_PARENT, 0x4288, 12, 1, 0, i2c_mux_table
	},
	{
		SS928V100_I2C2_MUX, "i2c2_mux",
		i2c_mux_p, ARRAY_SIZE(i2c_mux_p),
		CLK_SET_RATE_PARENT, 0x4290, 12, 1, 0, i2c_mux_table
	},
	{
		SS928V100_I2C3_MUX, "i2c3_mux",
		i2c_mux_p, ARRAY_SIZE(i2c_mux_p),
		CLK_SET_RATE_PARENT, 0x4298, 12, 1, 0, i2c_mux_table
	},
	{
		SS928V100_I2C4_MUX, "i2c4_mux",
		i2c_mux_p, ARRAY_SIZE(i2c_mux_p),
		CLK_SET_RATE_PARENT, 0x42a0, 12, 1, 0, i2c_mux_table
	},
	{
		SS928V100_I2C5_MUX, "i2c5_mux",
		i2c_mux_p, ARRAY_SIZE(i2c_mux_p),
		CLK_SET_RATE_PARENT, 0x42a8, 12, 1, 0, i2c_mux_table
	},
	{
		SS928V100_PWM0_MUX, "pwm0_mux",
		pwm0_mux_p, ARRAY_SIZE(pwm0_mux_p),
		CLK_SET_RATE_PARENT, 0x4588, 12, 2, 0, pwm0_mux_table
	},
	{
		SS928V100_PWM1_MUX, "pwm1_mux",
		pwm1_mux_p, ARRAY_SIZE(pwm1_mux_p),
		CLK_SET_RATE_PARENT, 0x4590, 12, 2, 0, pwm1_mux_table
	},
};

static struct bsp_fixed_factor_clock
	ss928v100_fixed_factor_clks[] __initdata = {
};

static struct bsp_gate_clock ss928v100_gate_clks[] __initdata = {
	{
		SS928V100_FMC_CLK, "clk_fmc", "fmc_mux",
		CLK_SET_RATE_PARENT, 0x3f40, 4, 0,
	},
	{
		SS928V100_MMC0_CLK, "clk_mmc0", "mmc0_mux",
		CLK_SET_RATE_PARENT, 0x34c0, 0, 0,
	},
	{
		SS928V100_MMC1_CLK, "clk_mmc1", "mmc1_mux",
		CLK_SET_RATE_PARENT, 0x35c0, 0, 0,
	},
	{
		SS928V100_MMC2_CLK, "clk_mmc2", "mmc2_mux",
		CLK_SET_RATE_PARENT, 0x36c0, 0, 0,
	},
	{
		SS928V100_UART0_CLK, "clk_uart0", "uart0_mux",
		CLK_SET_RATE_PARENT, 0x4180, 4, 0,
	},
	{
		SS928V100_UART1_CLK, "clk_uart1", "uart1_mux",
		CLK_SET_RATE_PARENT, 0x4188, 4, 0,
	},
	{
		SS928V100_UART2_CLK, "clk_uart2", "uart2_mux",
		CLK_SET_RATE_PARENT, 0x4190, 4, 0,
	},
	{
		SS928V100_UART3_CLK, "clk_uart3", "uart3_mux",
		CLK_SET_RATE_PARENT, 0x4198, 4, 0,
	},
	{
		SS928V100_UART4_CLK, "clk_uart4", "uart4_mux",
		CLK_SET_RATE_PARENT, 0x41A0, 4, 0,
	},
	{
	    SS928V100_UART5_CLK, "clk_uart5", "uart5_mux",
		CLK_SET_RATE_PARENT, 0x41a8, 4, 0,
	},
	/* ethernet mac */
	{
		SS928V100_ETH_CLK, "clk_eth", NULL,
		CLK_SET_RATE_PARENT, 0x37c4, 4, 0,
	},
	{
		SS928V100_ETH_MACIF_CLK, "clk_eth_macif", NULL,
		CLK_SET_RATE_PARENT, 0x37c0, 4, 0,
	},
	{
		SS928V100_ETH1_CLK, "clk_eth1", NULL,
		CLK_SET_RATE_PARENT, 0x3804, 4, 0,
	},
	{
		SS928V100_ETH1_MACIF_CLK, "clk_eth1_macif", NULL,
		CLK_SET_RATE_PARENT, 0x3800, 4, 0,
	},
	{
		SS928V100_I2C0_CLK, "clk_i2c0", "i2c0_mux",
		CLK_SET_RATE_PARENT, 0x4280, 4, 0,
	},
	{
		SS928V100_I2C1_CLK, "clk_i2c1", "i2c1_mux",
		CLK_SET_RATE_PARENT, 0x4288, 4, 0,
	},
	{
		SS928V100_I2C2_CLK, "clk_i2c2", "i2c2_mux",
		CLK_SET_RATE_PARENT, 0x4290, 4, 0,
	},
	{
		SS928V100_I2C3_CLK, "clk_i2c3", "i2c3_mux",
		CLK_SET_RATE_PARENT, 0x4298, 4, 0,
	},
	{
		SS928V100_I2C4_CLK, "clk_i2c4", "i2c4_mux",
		CLK_SET_RATE_PARENT, 0x42a0, 4, 0,
	},
	{ 	SS928V100_I2C5_CLK, "clk_i2c5", "i2c5_mux",
		CLK_SET_RATE_PARENT, 0x42a8, 4, 0,
	},
	/* spi */
	{
		SS928V100_SPI0_CLK, "clk_spi0", "100m",
		CLK_SET_RATE_PARENT, 0x4480, 4, 0,
	},
	{
		SS928V100_SPI1_CLK, "clk_spi1", "100m",
		CLK_SET_RATE_PARENT, 0x4488, 4, 0,
	},
	{
		SS928V100_SPI2_CLK, "clk_spi2", "100m",
		CLK_SET_RATE_PARENT, 0x4490, 4, 0,
	},
	{
		SS928V100_SPI3_CLK, "clk_spi3", "100m",
		CLK_SET_RATE_PARENT, 0x4498, 4, 0,
	},
	{
		SS928V100_EDMAC_AXICLK, "axi_clk_edmac", NULL,
		CLK_SET_RATE_PARENT, 0x2a80, 5, 0,
	},
	{
		SS928V100_EDMAC_CLK, "clk_edmac", NULL,
		CLK_SET_RATE_PARENT, 0x2a80, 4, 0,
	},
	/* lsadc */
	{
		SS928V100_LSADC_CLK, "clk_lsadc", NULL,
		CLK_SET_RATE_PARENT, 0x46c0, 4, 0,
	},
	/* pwm0 */
	{
		SS928V100_PWM0_CLK, "clk_pwm0", "pwm0_mux",
		CLK_SET_RATE_PARENT, 0x4588, 4, 0,
	},
	/* pwm1 */
	{
		SS928V100_PWM1_CLK, "clk_pwm1", "pwm1_mux",
		CLK_SET_RATE_PARENT, 0x4590, 4, 0,
	},
};

static struct ss928v100_pll_clock ss928v100_pll_clks[] __initdata = {
	{
		SS928V100_APLL_CLK, "apll", NULL, 0x0, 0, 24, 24, 3, 28, 3,
		0x4, 0, 12, 12, 6
	},
};

#define to_pll_clk(_hw) container_of(_hw, struct ss928v100_clk_pll, hw)
static void ss928v100_calc_pll(u32 *frac_val, u32 *fbdiv_val,
				u32 *refdiv_val, u64 rate)
{
	u64 rem;
	*frac_val = 0;
	/* Frequency divided by 1000000 can be converted from Hz to MHz. */
	rem = do_div(rate, 1000000);
	/* rate/24 is the integral part of the frequency multiplication coefficient. */
	*fbdiv_val = rate / 24;
	*refdiv_val = 1;
	/* 2 to the 24th power */
	rem = rem * (1 << 24);
	/* Frequency divided by 1000000 can be converted from Hz to MHz. */
	do_div(rem, 1000000);
	*frac_val = rem;
}

static int clk_pll_set_rate(struct clk_hw *hw,
		unsigned long rate,
		unsigned long parent_rate)
{
	struct ss928v100_clk_pll *clk = to_pll_clk(hw);
	u32 frac_val, postdiv1_val, postdiv2_val, fbdiv_val, refdiv_val;
	u32 val;

	/*Fixme  ignore postdives now because apll don't use them*/
	postdiv1_val = postdiv2_val = 0;

	ss928v100_calc_pll(&frac_val, &fbdiv_val, &refdiv_val, (u64)rate);

	val = readl_relaxed(clk->ctrl_reg1);
	val &= ~(((1 << clk->frac_width) - 1) << clk->frac_shift);
	val &= ~(((1 << clk->postdiv1_width) - 1) << clk->postdiv1_shift);
	val &= ~(((1 << clk->postdiv2_width) - 1) << clk->postdiv2_shift);

	val |= frac_val << clk->frac_shift;
	val |= postdiv1_val << clk->postdiv1_shift;
	val |= postdiv2_val << clk->postdiv2_shift;
	writel_relaxed(val, clk->ctrl_reg1);

	val = readl_relaxed(clk->ctrl_reg2);
	val &= ~(((1 << clk->fbdiv_width) - 1) << clk->fbdiv_shift);
	val &= ~(((1 << clk->refdiv_width) - 1) << clk->refdiv_shift);

	val |= fbdiv_val << clk->fbdiv_shift;
	val |= refdiv_val << clk->refdiv_shift;
	writel_relaxed(val, clk->ctrl_reg2);

	return 0;
}

static unsigned long clk_pll_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct ss928v100_clk_pll *clk = to_pll_clk(hw);
	u64 frac_val, fbdiv_val, refdiv_val;
	u32 val;
	u64 tmp, rate;

	val = readl_relaxed(clk->ctrl_reg1);
	val = val >> clk->frac_shift;
	val &= ((1 << clk->frac_width) - 1);
	frac_val = val;

	val = readl_relaxed(clk->ctrl_reg2);
	val = val >> clk->fbdiv_shift;
	val &= ((1 << clk->fbdiv_width) - 1);
	fbdiv_val = val;

	val = readl_relaxed(clk->ctrl_reg2);
	val = val >> clk->refdiv_shift;
	val &= ((1 << clk->refdiv_width) - 1);
	refdiv_val = val;

	rate = 0;
	/* rate = 24000000 * (fbdiv + frac / (1<<24) ) / refdiv  */
	tmp = 24000000 * fbdiv_val;
	rate += tmp;
	do_div(rate, refdiv_val);

	return rate;
}

static int clk_pll_determine_rate(struct clk_hw *hw,
		struct clk_rate_request *req)
{
	return req->rate;
}

static struct clk_ops clk_pll_ops = {
	.set_rate = clk_pll_set_rate,
	.determine_rate = clk_pll_determine_rate,
	.recalc_rate = clk_pll_recalc_rate,
};

void clk_register_pll(const struct ss928v100_pll_clock *clks,
		int nums, const struct bsp_clock_data *data)
{
	void __iomem *base = data->base;
	int i;

	for (i = 0; i < nums; i++) {
		struct ss928v100_clk_pll *p_clk = NULL;
		struct clk *clk = NULL;
		struct clk_init_data init;

		p_clk = kzalloc(sizeof(*p_clk), GFP_KERNEL);
		if (p_clk == NULL)
			return;

		init.name = clks[i].name;
		init.flags = CLK_IS_BASIC;
		init.parent_names =
			(clks[i].parent_name ? &clks[i].parent_name : NULL);
		init.num_parents = (clks[i].parent_name ? 1 : 0);
		init.ops = &clk_pll_ops;

		p_clk->ctrl_reg1 = base + clks[i].ctrl_reg1;
		p_clk->frac_shift = clks[i].frac_shift;
		p_clk->frac_width = clks[i].frac_width;
		p_clk->postdiv1_shift = clks[i].postdiv1_shift;
		p_clk->postdiv1_width = clks[i].postdiv1_width;
		p_clk->postdiv2_shift = clks[i].postdiv2_shift;
		p_clk->postdiv2_width = clks[i].postdiv2_width;

		p_clk->ctrl_reg2 = base + clks[i].ctrl_reg2;
		p_clk->fbdiv_shift = clks[i].fbdiv_shift;
		p_clk->fbdiv_width = clks[i].fbdiv_width;
		p_clk->refdiv_shift = clks[i].refdiv_shift;
		p_clk->refdiv_width = clks[i].refdiv_width;
		p_clk->hw.init = &init;

		clk = clk_register(NULL, &p_clk->hw);
		if (IS_ERR(clk)) {
			kfree(p_clk);
			pr_err("%s: failed to register clock %s\n",
					__func__, clks[i].name);
			continue;
		}

		data->clk_data.clks[clks[i].id] = clk;
	}
}

static __init struct bsp_clock_data *ss928v100_clk_register(
		struct platform_device *pdev)
{
	struct bsp_clock_data *clk_data = NULL;
	int ret;

	clk_data = bsp_clk_alloc(pdev, SS928V100_CRG_NR_CLKS);
	if (clk_data == NULL)
		return ERR_PTR(-ENOMEM);

	ret = bsp_clk_register_fixed_rate(ss928v100_fixed_rate_clks_crg,
			ARRAY_SIZE(ss928v100_fixed_rate_clks_crg), clk_data);
	if (ret)
		return ERR_PTR(ret);

	clk_register_pll(ss928v100_pll_clks,
			ARRAY_SIZE(ss928v100_pll_clks), clk_data);

	ret = bsp_clk_register_mux(ss928v100_mux_clks_crg,
			ARRAY_SIZE(ss928v100_mux_clks_crg),
			clk_data);
	if (ret)
		goto unregister_fixed_rate;

	ret = bsp_clk_register_fixed_factor(ss928v100_fixed_factor_clks,
			ARRAY_SIZE(ss928v100_fixed_factor_clks), clk_data);
	if (ret)
		goto unregister_mux;

	ret = bsp_clk_register_gate(ss928v100_gate_clks,
			ARRAY_SIZE(ss928v100_gate_clks),
			clk_data);
	if (ret)
		goto unregister_factor;

	ret = of_clk_add_provider(pdev->dev.of_node,
			of_clk_src_onecell_get, &clk_data->clk_data);
	if (ret)
		goto unregister_gate;

	return clk_data;

unregister_gate:
	bsp_clk_unregister_gate(ss928v100_gate_clks,
			ARRAY_SIZE(ss928v100_gate_clks), clk_data);
unregister_factor:
	bsp_clk_unregister_fixed_factor(ss928v100_fixed_factor_clks,
			ARRAY_SIZE(ss928v100_fixed_factor_clks), clk_data);
unregister_mux:
	bsp_clk_unregister_mux(ss928v100_mux_clks_crg,
			ARRAY_SIZE(ss928v100_mux_clks_crg),
			clk_data);
unregister_fixed_rate:
	bsp_clk_unregister_fixed_rate(ss928v100_fixed_rate_clks_crg,
			ARRAY_SIZE(ss928v100_fixed_rate_clks_crg), clk_data);
	return ERR_PTR(ret);
}

static __init void ss928v100_clk_unregister(const struct platform_device *pdev)
{
	struct bsp_crg_dev *crg = platform_get_drvdata(pdev);

	of_clk_del_provider(pdev->dev.of_node);

	bsp_clk_unregister_gate(ss928v100_gate_clks,
			ARRAY_SIZE(ss928v100_gate_clks), crg->clk_data);
	bsp_clk_unregister_mux(ss928v100_mux_clks_crg,
			ARRAY_SIZE(ss928v100_mux_clks_crg), crg->clk_data);
	bsp_clk_unregister_fixed_factor(ss928v100_fixed_factor_clks,
			ARRAY_SIZE(ss928v100_fixed_factor_clks), crg->clk_data);
	bsp_clk_unregister_fixed_rate(ss928v100_fixed_rate_clks_crg,
			ARRAY_SIZE(ss928v100_fixed_rate_clks_crg), crg->clk_data);
}

static const struct bsp_crg_funcs ss928v100_crg_funcs = {
	.register_clks = ss928v100_clk_register,
	.unregister_clks = ss928v100_clk_unregister,
};


static const struct of_device_id ss928v100_crg_match_table[] = {
	{
		.compatible = "vendor,ss928v100_clock",
		.data = &ss928v100_crg_funcs
	},
	{ }
};
MODULE_DEVICE_TABLE(of, ss928v100_crg_match_table);

static int ss928v100_crg_probe(struct platform_device *pdev)
{
	struct bsp_crg_dev *crg = NULL;

	crg = devm_kmalloc(&pdev->dev, sizeof(*crg), GFP_KERNEL);
	if (crg == NULL)
		return -ENOMEM;

	crg->funcs = of_device_get_match_data(&pdev->dev);
	if (crg->funcs == NULL)
		return -ENOENT;

	crg->rstc = vendor_reset_init(pdev);
	if (crg->rstc == NULL)
		return -ENOMEM;

	crg->clk_data = crg->funcs->register_clks(pdev);
	if (IS_ERR(crg->clk_data)) {
		bsp_reset_exit(crg->rstc);
		return PTR_ERR(crg->clk_data);
	}

	platform_set_drvdata(pdev, crg);
	return 0;
}

static int ss928v100_crg_remove(struct platform_device *pdev)
{
	struct bsp_crg_dev *crg = platform_get_drvdata(pdev);

	bsp_reset_exit(crg->rstc);
	crg->funcs->unregister_clks(pdev);
	return 0;
}

static struct platform_driver ss928v100_crg_driver = {
	.probe          = ss928v100_crg_probe,
	.remove     = ss928v100_crg_remove,
	.driver         = {
		.name   = "ss928v100_clock",
		.of_match_table = ss928v100_crg_match_table,
	},
};

static int __init ss928v100_crg_init(void)
{
	return platform_driver_register(&ss928v100_crg_driver);
}
core_initcall(ss928v100_crg_init);

static void __exit ss928v100_crg_exit(void)
{
	platform_driver_unregister(&ss928v100_crg_driver);
}
module_exit(ss928v100_crg_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("SS928V100 CRG Driver");
