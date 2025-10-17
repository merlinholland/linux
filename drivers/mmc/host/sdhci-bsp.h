/*
 * Copyright (c) Shenshu Technologies Co., Ltd. 2017-2020. All rights reserved.
 * Description: sdhci header
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _DRIVERS_MMC_SDHCI_BSP_H
#define _DRIVERS_MMC_SDHCI_BSP_H

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include <linux/reset.h>
#include <linux/mmc/host.h>
#include <linux/pm_runtime.h>
#include "sdhci-pltfm.h"

#define SDHCI_BSP_EDGE_TUNING /* enable edge tuning */

#define PHASE_SCALE	32
#define NOT_FOUND	(-1)
#define MAX_TUNING_NUM	1
#define MAX_FREQ	200000000
#define EDGE_TUNING_PHASE_STEP	4
#define MMC_BLOCK_SIZE	512

/* Software auto suspend delay */
#define BSP_MMC_AUTOSUSPEND_DELAY_MS 50

static inline void *sdhci_get_pltfm_priv(struct sdhci_host *host)
{
	return sdhci_pltfm_priv(sdhci_priv(host));
}

struct sdhci_bsp_priv {
	struct reset_control *crg_rst;
	struct reset_control *dll_rst;
	struct reset_control *sampl_rst;
	struct regmap *crg_regmap;
	struct regmap *misc_regmap;
	struct regmap *iocfg_regmap;
	void __iomem *phy_addr;
	unsigned int devid;
	unsigned int drv_phase;
	unsigned int sample_phase;
	unsigned int tuning_phase;
	unsigned int bus_width;
};

/* extended host controller registers. */
#define  SDHCI_CTRL_HOST_VER4_ENABLE	0x1000
#define  SDHCI_CLOCK_PLL_EN	0x0008
#define  SDHCI_CTRL_64BIT_ADDR	0x2000
#define  SDHCI_CAN_DO_ADMA3	0x08000000

/* extended registers */
#define SDHCI_MSHC_CTRL		0x508
#define SDHCI_CMD_CONFLIT_CHECK	0x01

#define SDHCI_AXI_MBIIU_CTRL	0x510
#define SDHCI_GM_WR_OSRC_LMT_MASK	(0x7 << 24)
#define sdhci_gm_wr_osrc_lmt_sel(x)	((x) << 24)
#define SDHCI_GM_RD_OSRC_LMT_MASK	(0x7 << 16)
#define sdhci_gm_rd_osrc_lmt_sel(x)	((x) << 16)
#define SDHCI_UNDEFL_INCR_EN		0x1

#define SDHCI_EMMC_CTRL		0x52C
#define  SDHCI_CARD_IS_EMMC	0x0001
#define  SDHCI_ENH_STROBE_EN	0x0100

#define SDHCI_EMMC_HW_RESET	0x534

#define SDHCI_AT_CTRL		0x540
#define  SDHCI_SAMPLE_EN	0x00000010

#define SDHCI_AT_STAT		0x544
#define  SDHCI_PHASE_SEL_MASK	0x000000FF

#define SDHCI_MULTI_CYCLE	0x54C
#define  SDHCI_FIND_EDGE_CLR	(0x1 << 14)
#define  SDHCI_FOUND_EDGE	(0x1 << 11)
#define  SDHCI_EDGE_DETECT_EN	(0x1 << 8)
#define  SDHCI_DOUT_EN_F_EDGE	(0x1 << 6)
#define  SDHCI_DATA_DLY_EN	(0x1 << 3)
#define  SDHCI_CMD_DLY_EN	(0x1 << 2)

void bsp_set_drv_cap(struct sdhci_host *host);
void bsp_get_phase(struct sdhci_host *host);
void bsp_set_drv_phase(struct sdhci_host *host, unsigned int phase);
void bsp_enable_sample(struct sdhci_host *host);
void bsp_set_sample_phase(struct sdhci_host *host, u32 phase);
void bsp_disable_card_clk(struct sdhci_host *host);
void bsp_enable_card_clk(struct sdhci_host *host);
void bsp_disable_internal_clk(struct sdhci_host *host);
void bsp_enable_internal_clk(struct sdhci_host *host);
void bsp_enable_sample_dll_slave(struct sdhci_host *host);
void bsp_wait_sample_dll_ready(struct sdhci_host *host);
void bsp_wait_p4_dll_lock(struct sdhci_host *host);
void bsp_wait_ds_dll_ready(struct sdhci_host *host);
void bsp_wait_ds_180_dll_ready(struct sdhci_host *host);
void bsp_wait_ds_dll_lock(struct sdhci_host *host);
void bsp_set_ds_dll_delay(struct sdhci_host *host);
void sdhci_bsp_set_clock(struct sdhci_host *host, unsigned int clk);
void sdhci_bsp_writel(struct sdhci_host *host, u32 val, int reg);
void sdhci_bsp_writew(struct sdhci_host *host, u16 val, int reg);
void sdhci_bsp_writeb(struct sdhci_host *host, u8 val, int reg);
u32 sdhci_bsp_readl(struct sdhci_host *host, int reg);
u16 sdhci_bsp_readw(struct sdhci_host *host, int reg);
u8 sdhci_bsp_readb(struct sdhci_host *host, int reg);
int bsp_support_runtime_pm(struct sdhci_host *host);
int sdhci_bsp_pltfm_init(struct platform_device *pdev,
    struct sdhci_host *host);
void sdhci_bsp_extra_init(struct sdhci_host *host);
int sdhci_bsp_parse_dt(struct sdhci_host *host);
int sdhci_bsp_start_signal_voltage_switch(struct sdhci_host *host,
    struct mmc_ios *ios);
#if defined(CONFIG_ARCH_SS919V100)
void bsp_find_edge_clear(struct sdhci_host *host);
#endif
#endif /* _DRIVERS_MMC_SDHCI_BSP_H */
