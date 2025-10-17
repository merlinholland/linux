/*
 * Copyright (c) Shenshu Technologies Co., Ltd. 2017-2020. All rights reserved.
 * Description: sdhci driver
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

#include "sdhci-bsp.h"
#include <linux/mmc/mmc.h>
#include "cqhci.h"
#include "mci_proc.h"
#include "bsp_quirk_ids.h"

static u32 __read_mostly g_cmdq_flag = 0;

static int __init bsp_cmdq_setup(char *str)
{
	/* off */
	if (!strcasecmp(str, "off") ) {
		g_cmdq_flag |= MMC_CMDQ_FORCE_OFF;
	}

	/* no whitelist */
	if (!strcasecmp(str, "nowhitelist") ) {
		g_cmdq_flag |= MMC_CMDQ_DIS_WHITELIST;
	}

	return 1;
}
__setup("cmdq=", bsp_cmdq_setup);

int sdhci_bsp_parse_dt(struct sdhci_host *host)
{
	struct sdhci_bsp_priv *priv = sdhci_get_pltfm_priv(host);
	struct device_node *np = host->mmc->parent->of_node;
	u32 bus_width;
	int ret;

	ret = mmc_of_parse(host->mmc);
	if (ret)
		return ret;

#ifdef CONFIG_MMC_CQHCI
	if (of_get_property(np, "mmc-cmd-queue", NULL))
		host->mmc->caps2 |= MMC_CAP2_CQE | MMC_CAP2_CQE_DCMD;
#endif
	if (of_get_property(np, "mmc-broken-cmd23", NULL))
		host->quirks2 |= SDHCI_QUIRK2_HOST_NO_CMD23;

	if (of_property_read_u32(np, "bus-width", &bus_width) == 0) {
		priv->bus_width = bus_width;
	} else {
		pr_err("%s: \"bus-width\" property is missing, assuming 1 bit.\n",
		       mmc_hostname(host->mmc));
		priv->bus_width = 1;
	}

	if (of_get_property(np, "sdhci,1-bit-only", NULL) ||
			(priv->bus_width == 1)) {
		priv->bus_width = 1;
		host->quirks |= SDHCI_QUIRK_FORCE_1_BIT_DATA;
	}

	return 0;
}

void bsp_enable_sample(struct sdhci_host *host)
{
	unsigned int reg;

	reg = sdhci_readl(host, SDHCI_AT_CTRL);
	reg |= SDHCI_SAMPLE_EN;
	sdhci_writel(host, reg, SDHCI_AT_CTRL);
}

void bsp_set_sample_phase(struct sdhci_host *host, u32 phase)
{
	unsigned int reg;

	reg = sdhci_readl(host, SDHCI_AT_STAT);
	reg &= ~SDHCI_PHASE_SEL_MASK;
	reg |= phase;
	sdhci_writel(host, reg, SDHCI_AT_STAT);
}

void bsp_disable_card_clk(struct sdhci_host *host)
{
	u16 clk;

	clk = sdhci_readw(host, SDHCI_CLOCK_CONTROL);
	clk &= ~SDHCI_CLOCK_CARD_EN;
	sdhci_writew(host, clk, SDHCI_CLOCK_CONTROL);
}

void bsp_enable_card_clk(struct sdhci_host *host)
{
	u16 clk;

	clk = sdhci_readw(host, SDHCI_CLOCK_CONTROL);
	clk |= SDHCI_CLOCK_CARD_EN;
	sdhci_writew(host, clk, SDHCI_CLOCK_CONTROL);
}

void bsp_disable_internal_clk(struct sdhci_host *host)
{
	u16 clk;

	clk = sdhci_readw(host, SDHCI_CLOCK_CONTROL);
	clk &= ~SDHCI_CLOCK_INT_EN;
	sdhci_writew(host, clk, SDHCI_CLOCK_CONTROL);
}

void bsp_enable_internal_clk(struct sdhci_host *host)
{
	unsigned int timeout = 20;
	u16 clk;

	clk = sdhci_readw(host, SDHCI_CLOCK_CONTROL);
	clk |= SDHCI_CLOCK_INT_EN | SDHCI_CLOCK_PLL_EN;
	sdhci_writew(host, clk, SDHCI_CLOCK_CONTROL);

	clk = sdhci_readw(host, SDHCI_CLOCK_CONTROL);
	while (!(clk & SDHCI_CLOCK_INT_STABLE)) {
		if (timeout == 0) {
			pr_err("%s: Internal clock never stabilised.\n",
			       __func__);
			return;
		}
		timeout--;
		udelay(1000); /* delay 1000us */
		clk = sdhci_readw(host, SDHCI_CLOCK_CONTROL);
	}
}

static void bsp_select_sample_phase(struct sdhci_host *host,
    unsigned int phase)
{
	bsp_disable_card_clk(host);
	bsp_set_sample_phase(host, phase);
	bsp_wait_sample_dll_ready(host);
	bsp_enable_card_clk(host);
	udelay(1);
}

static int sd_abort_tuning(struct mmc_host *host, u32 opcode)
{
	struct mmc_command cmd = {};

	if (opcode != MMC_SEND_TUNING_BLOCK)
		return 0;

	cmd.opcode = MMC_STOP_TRANSMISSION;
	cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_AC;

	cmd.busy_timeout = 150; /* 150 ms */
	return mmc_wait_for_cmd(host, &cmd, 0);
}

static int bsp_send_status(struct mmc_host *host)
{
	struct mmc_command cmd = {0};

	BUG_ON(!host);

	cmd.opcode = MMC_SEND_STATUS;
	cmd.flags = MMC_RSP_SPI_R2 | MMC_RSP_R1 | MMC_CMD_AC;

	return mmc_wait_for_cmd(host, &cmd, 1);
}

static int bsp_send_tuning(struct sdhci_host *host, u32 opcode)
{
	int count, err;

	count = 0;
	do {
		err = mmc_send_tuning(host->mmc, opcode, NULL);
		if (err) {
			if (opcode == MMC_SEND_TUNING_BLOCK)
				sd_abort_tuning(host->mmc, opcode);
			else
				mmc_abort_tuning(host->mmc, opcode);
			bsp_send_status(host->mmc);
			break;
		}
		count++;
	} while (count < MAX_TUNING_NUM);

	return err;
}

static void bsp_pre_tuning(struct sdhci_host *host)
{
	sdhci_writel(host, host->ier | SDHCI_INT_DATA_AVAIL, SDHCI_INT_ENABLE);
	sdhci_writel(host, host->ier | SDHCI_INT_DATA_AVAIL, SDHCI_SIGNAL_ENABLE);

	bsp_enable_sample(host);
	host->is_tuning = 1;
}

static void bsp_post_tuning(struct sdhci_host *host)
{
	unsigned short ctrl;

	ctrl = sdhci_readw(host, SDHCI_HOST_CONTROL2);
	ctrl |= SDHCI_CTRL_TUNED_CLK;
	sdhci_writew(host, ctrl, SDHCI_HOST_CONTROL2);

	sdhci_writel(host, host->ier, SDHCI_INT_ENABLE);
	sdhci_writel(host, host->ier, SDHCI_SIGNAL_ENABLE);
	host->is_tuning = 0;
}

#ifndef SDHCI_BSP_EDGE_TUNING
static int bsp_get_best_sample(u32 candidates)
{
	int rise = NOT_FOUND;
	int fall, i, win;
	int win_max_r = NOT_FOUND;
	int win_max_f = NOT_FOUND;
	int end_fall = NOT_FOUND;
	int found = NOT_FOUND;
	int win_max = 0;

	for (i = 0; i < PHASE_SCALE; i++) {
		if ((candidates & 0x3) == 0x2)
			rise = (i + 1) % PHASE_SCALE;

		if ((candidates & 0x3) == 0x1) {
			fall = i;
			if (rise != NOT_FOUND) {
				win = fall - rise + 1;
				if (win > win_max) {
					win_max = win;
					found = (fall + rise) / 2; /* Get window center by devide 2 */
					win_max_r = rise;
					win_max_f = fall;
					rise = NOT_FOUND;
					fall = NOT_FOUND;
				}
			} else {
				end_fall = fall;
			}
		}
		candidates = ror32(candidates, 1);
	}

	if (end_fall != NOT_FOUND && rise != NOT_FOUND) {
		fall = end_fall;
		if (end_fall < rise)
			end_fall += PHASE_SCALE;

		win = end_fall - rise + 1;
		if (win > win_max) {
			found = (rise + (win / 2)) % PHASE_SCALE; /* Get window center by devide 2 */
			win_max_r = rise;
			win_max_f = fall;
		}
	}

	if (found != NOT_FOUND)
		pr_err("valid phase shift [%d, %d] Final Phase:%d\n",
		       win_max_r, win_max_f, found);

	return found;
}

static int sdhci_bsp_exec_tuning(struct sdhci_host *host, u32 opcode)
{
	struct sdhci_bsp_priv *priv = sdhci_get_pltfm_priv(host);
	unsigned int sample;
	unsigned int candidates = 0;
	int phase, err;

	bsp_pre_tuning(host);

	for (sample = 0; sample < PHASE_SCALE; sample++) {
		bsp_select_sample_phase(host, sample);

		err = bsp_send_tuning(host, opcode);
		if (err)
			pr_debug("send tuning CMD%u fail! phase:%d err:%d\n",
				 opcode, sample, err);
		else
			candidates |= (0x1 << sample);
	}

	pr_info("%s: tuning done! candidates 0x%X: ",
		mmc_hostname(host->mmc), candidates);

	phase = bsp_get_best_sample(candidates);
	if (phase == NOT_FOUND) {
		phase = priv->sample_phase;
		pr_err("no valid phase shift! use default %d\n", phase);
	}

	priv->tuning_phase = phase;
	bsp_select_sample_phase(host, phase);
	bsp_post_tuning(host);

	return 0;
}
#else
static void bsp_enable_edge_tuning(struct sdhci_host *host)
{
	unsigned int reg;

	reg = sdhci_readl(host, SDHCI_MULTI_CYCLE);
	reg |= SDHCI_EDGE_DETECT_EN;
	sdhci_writel(host, reg, SDHCI_MULTI_CYCLE);
}

static void bsp_disable_edge_tuning(struct sdhci_host *host)
{
	unsigned int reg;

	reg = sdhci_readl(host, SDHCI_MULTI_CYCLE);
	reg &= ~SDHCI_EDGE_DETECT_EN;
	sdhci_writel(host, reg, SDHCI_MULTI_CYCLE);
}

static int sdhci_bsp_exec_edge_tuning(struct sdhci_host *host, u32 opcode)
{
	struct sdhci_bsp_priv *priv = sdhci_get_pltfm_priv(host);
	unsigned int index, val;
	unsigned int found;
	unsigned int prev_found = 0;
	unsigned int edge_p2f, edge_f2p, start, end;
	unsigned int phase, fall, rise;
	unsigned int fall_updat_flag = 0;
	int err;
	int prev_err = 0;

	bsp_pre_tuning(host);
	bsp_enable_edge_tuning(host);

	start = 0;
	end = PHASE_SCALE / EDGE_TUNING_PHASE_STEP;

	edge_p2f = start;
	edge_f2p = end;
	for (index = 0; index <= end; index++) {
		bsp_select_sample_phase(host, index * EDGE_TUNING_PHASE_STEP);
#if defined(CONFIG_ARCH_SS919V100)
		bsp_find_edge_clear(host);
#endif
		err = bsp_send_tuning(host, opcode);
		if (!err) {
			val = sdhci_readl(host, SDHCI_MULTI_CYCLE);
			found = val & SDHCI_FOUND_EDGE;
		} else {
			found = 1;
		}

		if (prev_found && !found)
			edge_f2p = index;
		else if (!prev_found && found)
			edge_p2f = index;

		if ((edge_p2f != start) && (edge_f2p != end))
			break;

		prev_found = found;
	}

	if ((edge_p2f == start) && (edge_f2p == end)) {
		pr_err("%s: tuning failed! can not found edge!\n",
		       mmc_hostname(host->mmc));
		return -1;
	}

	bsp_disable_edge_tuning(host);

	start = edge_p2f * EDGE_TUNING_PHASE_STEP;
	end = edge_f2p * EDGE_TUNING_PHASE_STEP;
	if (end <= start)
		end += PHASE_SCALE;

	fall = start;
	rise = end;
	for (index = start; index <= end; index++) {
		bsp_select_sample_phase(host, index % PHASE_SCALE);
		err = bsp_send_tuning(host, opcode);
		if (err)
			pr_debug("send tuning CMD%u fail! phase:%d err:%d\n",
				 opcode, index, err);

		if (err && index == start) {
			if (!fall_updat_flag) {
				fall_updat_flag = 1;
				fall = start;
			}
		} else if (!prev_err && err) {
			if (!fall_updat_flag) {
				fall_updat_flag = 1;
				fall = index;
			}
		}

		if (prev_err && !err)
			rise = index;

		if (err && index == end)
			rise = end;

		prev_err = err;
	}

	phase = ((fall + rise) / 2 + PHASE_SCALE / 2) % /* 2 for cal average */
		PHASE_SCALE;

	pr_info("%s: tuning done! valid phase shift [%d, %d] Final Phase:%d\n",
		mmc_hostname(host->mmc), rise % PHASE_SCALE,
		fall % PHASE_SCALE, phase);

	priv->tuning_phase = phase;
	bsp_select_sample_phase(host, phase);
	bsp_post_tuning(host);

	return 0;
}
#endif

static int sdhci_bsp_execute_tuning(struct sdhci_host *host, u32 opcode)
{
#ifdef SDHCI_BSP_EDGE_TUNING
	return sdhci_bsp_exec_edge_tuning(host, opcode);
#else
	return sdhci_bsp_exec_tuning(host, opcode);
#endif
}

static void bsp_set_emmc_card(struct sdhci_host *host)
{
	unsigned int reg;

	if (host->timing == MMC_TIMING_MMC_HS ||
			host->timing == MMC_TIMING_MMC_DDR52 ||
			host->timing == MMC_TIMING_MMC_HS200 ||
			host->timing == MMC_TIMING_MMC_HS400) {
		reg = sdhci_readl(host, SDHCI_EMMC_CTRL);
		reg |= SDHCI_CARD_IS_EMMC;
		sdhci_writel(host, reg, SDHCI_EMMC_CTRL);
	}
}

static void sdhci_bsp_set_uhs_signaling(struct sdhci_host *host,
    unsigned int timing)
{
	sdhci_set_uhs_signaling(host, timing);
	host->timing = timing;
	bsp_set_emmc_card(host);
	bsp_set_drv_cap(host);
}

static void sdhci_bsp_hw_reset(struct sdhci_host *host)
{
	sdhci_writel(host, 0x0, SDHCI_EMMC_HW_RESET);
	udelay(10);  /* delay 10us */
	sdhci_writel(host, 0x1, SDHCI_EMMC_HW_RESET);
	udelay(200); /* delay 200us */
}

/*
 * This api is for wifi driver rescan the sdio device
 */
int bsp_sdio_rescan(int slot)
{
	struct mmc_host *mmc = NULL;

	if ((slot >= MCI_SLOT_NUM) || (slot <= 0)) {
		pr_err("invalid mmc slot, please check the argument\n");
		return -EINVAL;
	}

	mmc = mci_host[slot];
	if (mmc == NULL) {
		pr_err("invalid mmc, please check the argument\n");
		return -EINVAL;
	}

	mmc_detect_change(mmc, 0);
	return 0;
}
EXPORT_SYMBOL_GPL(bsp_sdio_rescan);

static const struct of_device_id sdhci_bsp_match[] = {
	{ .compatible = "vendor,sdhci" },
	{},
};

MODULE_DEVICE_TABLE(of, sdhci_bsp_match);

static struct sdhci_ops sdhci_bsp_ops = {
	.platform_execute_tuning = sdhci_bsp_execute_tuning,
	.reset = sdhci_reset,
	.set_clock = sdhci_bsp_set_clock,
	.set_bus_width = sdhci_set_bus_width,
	.set_uhs_signaling = sdhci_bsp_set_uhs_signaling,
	.hw_reset = sdhci_bsp_hw_reset,
#if defined(CONFIG_ARCH_SS318V100) || defined(CONFIG_ARCH_SS919V100) || \
	defined(CONFIG_ARCH_SS918V100) || defined(CONFIG_ARCH_SS015V100) || \
	defined(CONFIG_ARCH_SS013V100) || defined(CONFIG_ARCH_SS928V100) || \
	defined(CONFIG_ARCH_SS927V100)
	.start_signal_voltage_switch = sdhci_bsp_start_signal_voltage_switch,
#endif
#ifdef CONFIG_MMC_SDHCI_IO_ACCESSORS
#if defined(CONFIG_ARCH_SS928V100) || defined(CONFIG_ARCH_SS927V100)
	.write_l = sdhci_bsp_writel,
	.write_w = sdhci_bsp_writew,
	.write_b = sdhci_bsp_writeb,
	.read_l = sdhci_bsp_readl,
	.read_w = sdhci_bsp_readw,
	.read_b = sdhci_bsp_readb,
#endif
#endif
	.init = sdhci_bsp_extra_init,
};

static const struct sdhci_pltfm_data sdhci_bsp_pdata = {
	.ops = &sdhci_bsp_ops,
	.quirks = SDHCI_QUIRK_BROKEN_TIMEOUT_VAL,
	.quirks2 = SDHCI_QUIRK2_PRESET_VALUE_BROKEN,
};

#ifdef CONFIG_MMC_CQHCI
static u32 sdhci_bsp_cqhci_irq(struct sdhci_host *host, u32 intmask)
{
	int cmd_error = 0;
	int data_error = 0;

	if (!sdhci_cqe_irq(host, intmask, &cmd_error, &data_error))
		return intmask;

	cqhci_irq(host->mmc, intmask, cmd_error, data_error);

	return 0;
}

static void sdhci_bsp_controller_v4_enable(struct sdhci_host *host, int enable)
{
	u16 ctrl;

	ctrl = sdhci_readw(host, SDHCI_HOST_CONTROL2);
	if (enable)
		ctrl |= SDHCI_CTRL_HOST_VER4_ENABLE;
	else
		ctrl &= ~SDHCI_CTRL_HOST_VER4_ENABLE;

	if (host->flags & SDHCI_USE_64_BIT_DMA)
		ctrl |= SDHCI_CTRL_64BIT_ADDR;

	sdhci_writew(host, ctrl, SDHCI_HOST_CONTROL2);
}

static void sdhci_bsp_cqe_enable(struct mmc_host *mmc)
{
	struct sdhci_host *host = mmc_priv(mmc);
	unsigned int timeout = 10000;
	u16 reg, clk;
	u8 ctrl;

	/* SW_RST_DAT */
	sdhci_reset(host, SDHCI_RESET_DATA);

	sdhci_bsp_controller_v4_enable(host, 1);

	/* Set the DMA boundary value and block size */
	sdhci_writew(host, SDHCI_MAKE_BLKSZ(host->sdma_boundary,
					    MMC_BLOCK_SIZE), SDHCI_BLOCK_SIZE);

	/* need to set multitransfer for cmdq */
	reg = sdhci_readw(host, SDHCI_TRANSFER_MODE);
	reg |= SDHCI_TRNS_MULTI;
	reg |= SDHCI_TRNS_BLK_CNT_EN;
	sdhci_writew(host, reg, SDHCI_TRANSFER_MODE);

	/* ADMA2 only */
	ctrl = sdhci_readb(host, SDHCI_HOST_CONTROL);
	ctrl &= ~SDHCI_CTRL_DMA_MASK;
	ctrl |= SDHCI_CTRL_ADMA32;
	sdhci_writeb(host, ctrl, SDHCI_HOST_CONTROL);

	clk = sdhci_readw(host, SDHCI_CLOCK_CONTROL);
	clk |= SDHCI_CLOCK_PLL_EN;
	sdhci_writew(host, clk, SDHCI_CLOCK_CONTROL);

	while (mmc->ops->card_busy(mmc)) {
		timeout--;
		if (!timeout) {
			pr_err("%s: wait busy timeout\n", __func__);
			break;
		}
		udelay(1);
	}

	sdhci_cqe_enable(mmc);
}

static void sdhci_bsp_cqe_disable(struct mmc_host *mmc, bool recovery)
{
	int timeout = 10000;

	while (mmc->ops->card_busy(mmc)) {
		timeout--;
		if (!timeout) {
			pr_err("%s: wait busy timeout\n", __func__);
			break;
		}
		udelay(1);
	}

	sdhci_bsp_controller_v4_enable(mmc_priv(mmc), 0);

	sdhci_cqe_disable(mmc, recovery);
}

static void sdhci_bsp_dumpregs(struct mmc_host *mmc)
{
	sdhci_dumpregs(mmc_priv(mmc));
}

static const struct cqhci_host_ops sdhci_bsp_cqhci_ops = {
	.enable = sdhci_bsp_cqe_enable,
	.disable = sdhci_bsp_cqe_disable,
	.dumpregs = sdhci_bsp_dumpregs,
};

static const struct sdhci_ops sdhci_bsp_cqe_ops = {
	.platform_execute_tuning = sdhci_bsp_execute_tuning,
	.reset = sdhci_reset,
	.set_clock = sdhci_bsp_set_clock,
	.set_bus_width = sdhci_set_bus_width,
	.set_uhs_signaling = sdhci_bsp_set_uhs_signaling,
#if defined(CONFIG_ARCH_SS928V100) || defined(CONFIG_ARCH_SS927V100)
	.start_signal_voltage_switch =
		sdhci_bsp_start_signal_voltage_switch,
#endif
	.hw_reset = sdhci_bsp_hw_reset,
	.irq = sdhci_bsp_cqhci_irq,
	.init = sdhci_bsp_extra_init,
};

static const struct sdhci_pltfm_data sdhci_bsp_cqe_pdata = {
	.ops = &sdhci_bsp_cqe_ops,
	.quirks = SDHCI_QUIRK_BROKEN_TIMEOUT_VAL,
	.quirks2 = SDHCI_QUIRK2_PRESET_VALUE_BROKEN,
};

static int sdhci_bsp_add_host(struct sdhci_host *host)
{
	struct cqhci_host *cq_host = NULL;
	bool dma64 = false;
	int ret;

	if (g_cmdq_flag & MMC_CMDQ_FORCE_OFF) {
		/* force cmdq off by bootarges */
		host->mmc->caps2 &= ~MMC_CAP2_CQE;
	}

	if (!(host->mmc->caps2 & MMC_CAP2_CQE))
		return sdhci_add_host(host);

	ret = sdhci_setup_host(host);
	if (ret)
		return ret;

	cq_host = devm_kzalloc(host->mmc->parent, sizeof(*cq_host), GFP_KERNEL);
	if (cq_host == NULL) {
		pr_err("%s: allocate memory for CQE fail\n", __func__);
		ret = -ENOMEM;
		goto cleanup;
	}

	cq_host->mmio = host->ioaddr + 0x180;
	cq_host->ops = &sdhci_bsp_cqhci_ops;

	/*
	 * synopsys controller has dma 128M algin limit,
	 * may split the trans descriptors
	 */
	cq_host->quirks |= CQHCI_QUIRK_TXFR_DESC_SZ_SPLIT;

	dma64 = host->flags & SDHCI_USE_64_BIT_DMA;
	if (dma64)
		cq_host->caps |= CQHCI_TASK_DESC_SZ_128;

	ret = cqhci_init(cq_host, host->mmc, dma64);
	if (ret) {
		pr_err("%s: CQE init fail\n", __func__);
		return ret;
	}

	ret = __sdhci_add_host(host);
	if (ret)
		return ret;

	return 0;

cleanup:
	sdhci_cleanup_host(host);
	return ret;
}
#else
static int sdhci_bsp_add_host(struct sdhci_host *host)
{
	return sdhci_add_host(host);
}
#endif

static void sdhci_bsp_init_card(struct mmc_host *host, struct mmc_card *card)
{
#ifdef CONFIG_MMC_CQHCI
	u32 idx;
	/* eMMC spec: cid product name offset: 0, 7, 6, 5, 4, 11 */
	const u8 cid_pnm_offset[] = {0, 7, 6, 5, 4, 11};

	if (host == NULL || card == NULL) {
		pr_err("null card or host\n");
		return;
	}

	if ((card->type == MMC_TYPE_MMC) && (host->caps2 & MMC_CAP2_CQE)) {
		u8 *raw_cid = (u8 *)card->raw_cid;

		/* Skip whitelist */
		if (g_cmdq_flag & MMC_CMDQ_DIS_WHITELIST) {
			return;
		}

		/* Clear MMC CQE capblility */
		host->caps2 &= ~(MMC_CAP2_CQE | MMC_CAP2_CQE_DCMD);

		/* Manufacturer ID: b[127:120](eMMC v2.0 and upper), 0/24: idx/bit offset */
		card->cid.manfid = card->raw_cid[0] >> 24 & 0xFF;

		/* Decode CID with eMMC v2.0 and upper */
		for (idx = 0; idx < sizeof(cid_pnm_offset); idx++) {
			card->cid.prod_name[idx] = raw_cid[cid_pnm_offset[idx]];
		}
		card->cid.prod_name[++idx] = 0;
		mmc_fixup_device(card, mmc_cmdq_whitelist);
	}
#endif
}

static int sdhci_bsp_probe(struct platform_device *pdev)
{
	struct sdhci_host *host = NULL;
	const struct sdhci_pltfm_data *pdata = NULL;
	int ret;

#ifdef CONFIG_MMC_CQHCI
	if (of_get_property(pdev->dev.of_node, "mmc-cmd-queue", NULL))
		pdata = &sdhci_bsp_cqe_pdata;
	else
		pdata = &sdhci_bsp_pdata;
#else
	pdata = &sdhci_bsp_pdata;
#endif
	host = sdhci_pltfm_init(pdev, pdata, sizeof(struct sdhci_bsp_priv));
	if (IS_ERR(host))
		return PTR_ERR(host);

	host->mmc_host_ops.init_card = sdhci_bsp_init_card;

	ret = sdhci_bsp_pltfm_init(pdev, host);
	if (ret)
		goto pltfm_free;

	if (bsp_support_runtime_pm(host)) {
		pm_runtime_get_noresume(&pdev->dev);
		pm_runtime_set_autosuspend_delay(&pdev->dev, BSP_MMC_AUTOSUSPEND_DELAY_MS);
		pm_runtime_use_autosuspend(&pdev->dev);
		pm_runtime_set_active(&pdev->dev);
		pm_runtime_enable(&pdev->dev);
	}

	ret = sdhci_bsp_add_host(host);
	if (ret)
		goto pm_runtime_disable;

	if (bsp_support_runtime_pm(host)) {
		pm_runtime_mark_last_busy(&pdev->dev);
		pm_runtime_put_autosuspend(&pdev->dev);
	}

	return 0;

pm_runtime_disable:
	if (bsp_support_runtime_pm(host)) {
		pm_runtime_disable(&pdev->dev);
		pm_runtime_set_suspended(&pdev->dev);
		pm_runtime_put_noidle(&pdev->dev);
	}

pltfm_free:
	sdhci_pltfm_free(pdev);
	return ret;
}

static int sdhci_bsp_remove(struct platform_device *pdev)
{
	struct sdhci_host *host = platform_get_drvdata(pdev);
	int dead = (readl_relaxed(host->ioaddr + SDHCI_INT_STATUS) ==
		    0xffffffff);

	if (bsp_support_runtime_pm(host)) {
		pm_runtime_get_sync(&pdev->dev);
		pm_runtime_disable(&pdev->dev);
		pm_runtime_put_noidle(&pdev->dev);
	}

	sdhci_remove_host(host, dead);
	sdhci_pltfm_free(pdev);

	return 0;
}

#ifdef CONFIG_PM
static int sdhci_bsp_runtime_suspend(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);

	bsp_disable_card_clk(host);
	return 0;
}

static int sdhci_bsp_runtime_resume(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);

	bsp_enable_card_clk(host);
	return 0;
}
#endif

static const struct dev_pm_ops sdhci_bsp_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sdhci_pltfm_suspend,
				sdhci_pltfm_resume)

	SET_RUNTIME_PM_OPS(sdhci_bsp_runtime_suspend,
			   sdhci_bsp_runtime_resume,
			   NULL)
};

static struct platform_driver sdhci_bsp_driver = {
	.probe = sdhci_bsp_probe,
	.remove = sdhci_bsp_remove,
	.driver = {
		.name = "sdhci_bsp",
		.of_match_table = sdhci_bsp_match,
		.pm = &sdhci_bsp_pm_ops,
	},
};

static int __init sdhci_bsp_init(void)
{
	int ret;

	ret = platform_driver_register(&sdhci_bsp_driver);
	if (ret)
		return ret;

	ret = mci_proc_init();
	if (ret)
		platform_driver_unregister(&sdhci_bsp_driver);

	return ret;
}

static void __exit sdhci_bsp_exit(void)
{
	mci_proc_shutdown();

	platform_driver_unregister(&sdhci_bsp_driver);
}

module_init(sdhci_bsp_init);
module_exit(sdhci_bsp_exit);
MODULE_DESCRIPTION("SDHCI driver for Vendor");
MODULE_LICENSE("GPL v2");
