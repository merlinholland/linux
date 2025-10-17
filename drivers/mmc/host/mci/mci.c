/*
 *
 * Copyright (c) 2015-2021 Shenshu Technologies Co., Ltd.
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
#define pr_fmt(fmt) "mci: " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>

#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/card.h>
#include <linux/mmc/core.h>
#include <linux/mmc/sd.h>
#include <linux/slab.h>

#include <linux/ioport.h>
#include <linux/device.h>
#include <linux/spinlock.h>

#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/kthread.h>
#include <linux/workqueue.h>
#include <linux/freezer.h>
#include <asm/dma.h>
#include <asm/irq.h>
#include <linux/sizes.h>
#include <mach/io.h>

#include <linux/io.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/reset.h>

#include "mci_reg.h"
#include "mci.h"
#include "mci_proc.h"
#include <linux/securec.h>

#ifdef CONFIG_ARCH_SS812V100
#include "mci_ss812v100.c"
#endif

#ifdef CONFIG_ARCH_SS813V100
#include "mci_ss813v100.c"
#endif

#ifdef CONFIG_ARCH_SS312V100
#include "mci_ss312v100.c"
#endif

#ifdef CONFIG_ARCH_SS313V100
#include "mci_ss313v100.c"
#endif

#ifdef CONFIG_ARCH_SS011V100
#include "mci_ss313v100.c"
#endif

#ifdef CONFIG_ARCH_SS012V100
#include "mci_ss313v100.c"
#endif

#define DRIVER_NAME "mci"

#define mci_is_port_2(devid) ((devid) == 2)

#ifndef CONFIG_BSP_MC
#define CMD_DES_PAGE_SIZE	(3 * PAGE_SIZE)
#else
#define CMD_DES_PAGE_SIZE	(8 * PAGE_SIZE)
#endif

#define REG_MISC_CTRL0 0x12030000
#define MISC_CTRL0_FIX_MASK (GENMASK(12, 10) | GENMASK(8, 7))

#if defined(CONFIG_ARCH_SS812V100) || defined(CONFIG_ARCH_SS813V100) || \
	defined(CONFIG_ARCH_SS312V100)  || defined(CONFIG_ARCH_SS313V100)  || \
	defined(CONFIG_ARCH_SS011V100)  || defined(CONFIG_ARCH_SS012V100)
void __iomem *crg_ctrl, *misc_ctrl_1;
#endif

static unsigned int detect_time = MCI_DETECT_TIMEOUT;
static unsigned int retry_count = MAX_RETRY_COUNT;
static unsigned int request_timeout = MCI_REQUEST_TIMEOUT;
int trace_level = MCI_TRACE_LEVEL;
unsigned int slot_index = 0;
struct mci_host *mci_host[MCI_SLOT_NUM] = {NULL};
#ifdef MODULE

module_param(detect_time, uint, 0600);
MODULE_PARM_DESC(detect_timer, "card detect time (default:500ms))");

module_param(retry_count, uint, 0600);
MODULE_PARM_DESC(retry_count, "retry count times (default:100))");

module_param(request_timeout, uint, 0600);
MODULE_PARM_DESC(request_timeout, "Request timeout time (default:3s))");

module_param(trace_level, int, 0600);
MODULE_PARM_DESC(trace_level, "MCI_TRACE_LEVEL");

#endif
#define MCI_SYS_RESET_DELAY 10
/* reset MMC host controller */
static void mci_sys_reset(const struct mci_host *host)
{
	unsigned int reg_value;
	unsigned long flags;

	mci_trace(MCI_TRACE_LEVEL_INFO, "reset");

	local_irq_save(flags);

	reg_value = mci_readl(host->base + MCI_BMOD);
	reg_value |= BMOD_SWR;
	mci_writel(reg_value, host->base + MCI_BMOD);
	mdelay(MCI_SYS_RESET_DELAY);

	reg_value = mci_readl(host->base + MCI_BMOD);
	reg_value |= BURST_16 | BURST_INCR;
	mci_writel(reg_value, host->base + MCI_BMOD);

	reg_value = mci_readl(host->base + MCI_CTRL);
	reg_value |=  CTRL_RESET | FIFO_RESET | DMA_RESET;
	mci_writel(reg_value, host->base + MCI_CTRL);

	local_irq_restore(flags);
}

#define POWER_SWITCH_DELAY 100
static void mci_ctrl_power(struct mci_host *host,
			     unsigned int flag, unsigned int force)
{
	unsigned int port;

	mci_trace(MCI_TRACE_LEVEL_INFO, "begin");

	port = host->port;

	if (host->power_status != flag || force == FORCE_ENABLE) {
		unsigned int reg_value;

		if (flag == POWER_OFF) {
			reg_value = mci_readl(host->base + MCI_RESET_N);
			reg_value &= ~(MMC_RST_N << port);
			mci_writel(reg_value, host->base + MCI_RESET_N);
		}

		reg_value = mci_readl(host->base + MCI_PWREN);
		if (flag == POWER_OFF)
			reg_value &= ~(0x1 << port);
		else
			reg_value |= (0x1 << port);

		mci_writel(reg_value, host->base + MCI_PWREN);

		if (flag == POWER_ON) {
			reg_value = mci_readl(host->base + MCI_RESET_N);
			reg_value |= (MMC_RST_N << port);
			mci_writel(reg_value, host->base + MCI_RESET_N);
		}

		if (in_interrupt())
			mdelay(POWER_SWITCH_DELAY);
		else
			msleep(POWER_SWITCH_DELAY);

		host->power_status = flag;
	}
}

/**********************************************
 * 1: card off
 * 0: card on
 ***********************************************/
static unsigned int mci_sys_card_detect(const struct mci_host *host)
{
	unsigned int card_status;

	card_status = readl(host->base + MCI_CDETECT);
	card_status &= (MCI_CARD0 << host->port);
	if (card_status)
		card_status = 1;
	else
		card_status = 0;

	return card_status;
}

/**********************************************
 * 1: card readonly
 * 0: card read/write
 ***********************************************/
static unsigned int mci_ctrl_card_readonly(const struct mci_host *host)
{
	unsigned int card_value = mci_readl(host->base + MCI_WRTPRT);
	return card_value & (MCI_CARD0 << host->port);
}

static int tuning_reset_flag = 0;

static int mci_wait_cmd(struct mci_host *host)
{
	int wait_retry_count = 0;
	int retry_count_cmd = 500;
	unsigned int reg_data;
	unsigned long flags;

	mci_trace(MCI_TRACE_LEVEL_INFO, "begin");
	mci_assert(host);

	while (1) {
		/*
		 * Check if CMD::start_cmd bit is clear.
		 * start_cmd = 0 means MMC Host controller has loaded registers
		 * and next command can be loaded in.
		 */
		reg_data = mci_readl(host->base + MCI_CMD);
		if ((reg_data & START_CMD) == 0)
			return 0;

		/* Check if Raw_Intr_Status::HLE bit is set. */
		spin_lock_irqsave(&host->lock, flags);
		reg_data = mci_readl(host->base + MCI_RINTSTS);
		if (reg_data & HLE_INT_STATUS) {
			reg_data |= HLE_INT_STATUS;
			mci_writel(reg_data, host->base + MCI_RINTSTS);
			spin_unlock_irqrestore(&host->lock, flags);

			mci_trace(MCI_TRACE_LEVEL_ERR, "Other CMD is running,"
				    "please operate cmd again!");
			return 1;
		}

		spin_unlock_irqrestore(&host->lock, flags);
		udelay(1);

		/* Check if number of retries for this are over. */
		wait_retry_count++;
		if (wait_retry_count >= retry_count_cmd) {
			if (host->is_tuning)
				tuning_reset_flag = 1;
			mci_trace(MCI_TRACE_LEVEL_WAR, "send cmd is timeout!");
			return -1;
		}
	}
}

static int mci_control_cclk(struct mci_host *host, unsigned int flag)
{
	unsigned int reg;
	union cmd_arg_u cmd_reg;

	mci_trace(MCI_TRACE_LEVEL_INFO, "begin");
	mci_assert(host);

	reg = mci_readl(host->base + MCI_CLKENA);
	if (flag == ENABLE) {
		reg |= (CCLK_ENABLE << host->port);
		reg |= (CCLK_LOW_POWER << host->port);
	} else {
		reg &= ~(CCLK_ENABLE << host->port);
		reg &= ~(CCLK_LOW_POWER << host->port);
	}
#if defined(CONFIG_ARCH_SS812V100) || defined(CONFIG_ARCH_SS813V100) || \
	defined(CONFIG_ARCH_SS312V100)  || defined(CONFIG_ARCH_SS313V100)  || \
	defined(CONFIG_ARCH_SS011V100)  || defined(CONFIG_ARCH_SS012V100)
	if (mci_is_port_2(host->devid))
		reg &= ~(CCLK_LOW_POWER << host->port);
#endif
	mci_writel(reg, host->base + MCI_CLKENA);

	cmd_reg.cmd_arg = mci_readl(host->base + MCI_CMD);
	cmd_reg.bits.start_cmd = 1;
	cmd_reg.bits.card_number = host->port;
	cmd_reg.bits.cmd_index = 0;
	cmd_reg.bits.data_transfer_expected = 0;
	cmd_reg.bits.update_clk_reg_only = 1;
	cmd_reg.bits.response_expect = 0;
	cmd_reg.bits.send_auto_stop = 0;
	cmd_reg.bits.wait_prvdata_complete = 0;
	cmd_reg.bits.check_response_crc = 0;
#if defined(CONFIG_ARCH_SS812V100) || defined(CONFIG_ARCH_SS813V100) || \
	defined(CONFIG_ARCH_SS312V100)  || defined(CONFIG_ARCH_SS313V100)  || \
	defined(CONFIG_ARCH_SS011V100)  || defined(CONFIG_ARCH_SS012V100)
	cmd_reg.bits.use_hold_reg = 1;
#endif
	mci_writel(cmd_reg.cmd_arg, host->base + MCI_CMD);
	if (mci_wait_cmd(host) != 0) {
		mci_trace(MCI_TRACE_LEVEL_WAR, "disable or enable clk is timeout!");
		return -ETIMEDOUT;
	} else {
		return 0;
	}
}
#define MCI_REG_NUM_OF_PORT 8
static void mci_set_cclk(struct mci_host *host, unsigned int cclk)
{
	unsigned int reg_value;
	union cmd_arg_u clk_cmd;
	unsigned int hclk;

	mci_trace(MCI_TRACE_LEVEL_INFO, "begin");
	mci_assert(host);
	mci_assert(cclk);

	hclk = cclk > MMC_CRG_MIN ? cclk : MMC_CRG_MIN;
	clk_set_rate(host->clk, hclk);

	hclk = clk_get_rate(host->clk);
	host->mmc->actual_clock = hclk;

	/*
	 * set card clk divider value,
	 * clk_divider = Fmmcclk/(Fmmc_cclk * 2)
	 */
	reg_value = hclk / (cclk * 2);
	if ((hclk % (cclk * 2)) && (hclk > cclk))
		reg_value++;
	if (reg_value > 0xFF)
		reg_value = 0xFF;

	host->hclk = hclk;
	host->cclk = reg_value ? (hclk / (reg_value * 2)) : hclk;
	mci_writel((reg_value << (host->port * MCI_REG_NUM_OF_PORT)),
		     host->base + MCI_CLKDIV);

	clk_cmd.cmd_arg = mci_readl(host->base + MCI_CMD);
	clk_cmd.bits.start_cmd = 1;
	clk_cmd.bits.card_number = host->port;
	clk_cmd.bits.update_clk_reg_only = 1;
	clk_cmd.bits.cmd_index = 0;
	clk_cmd.bits.data_transfer_expected = 0;
	clk_cmd.bits.response_expect = 0;
	mci_writel(clk_cmd.cmd_arg, host->base + MCI_CMD);
	if (mci_wait_cmd(host) != 0)
		mci_trace(MCI_TRACE_LEVEL_ERR, "set card clk divider is failed!");
}

static void mci_sys_ctrl_init(const struct mci_host *host)
{
	reset_control_assert(host->crg_rst);
	udelay(POWER_SWITCH_DELAY);
	reset_control_deassert(host->crg_rst);
}

static void mci_init_host(struct mci_host *host)
{
	unsigned int tmp_reg;
	unsigned long flags;

	mci_trace(MCI_TRACE_LEVEL_INFO, "begin");
	mci_assert(host);

	mci_sys_reset(host);

#if defined(CONFIG_ARCH_SS812V100) || defined(CONFIG_ARCH_SS813V100) || \
	defined(CONFIG_ARCH_SS312V100)  || defined(CONFIG_ARCH_SS313V100)  || \
	defined(CONFIG_ARCH_SS011V100)  || defined(CONFIG_ARCH_SS012V100)
	/* controller config gpio */
	tmp_reg = mci_readl(host->base + MCI_GPIO);
	tmp_reg |= DTO_FIX_BYPASS;
	mci_writel(tmp_reg, host->base + MCI_GPIO);
#endif

	/* set drv/smpl phase shift */
	tmp_reg = 0;
	tmp_reg |= SMPL_PHASE_DFLT | DRV_PHASE_DFLT;
	mci_writel(tmp_reg, host->base + MCI_UHS_REG_EXT);

	/* set card read threshold */
	mci_writel(RW_THRESHOLD_SIZE, host->base + MCI_CARDTHRCTL);

	/* clear MMC host intr */
	mci_writel(ALL_INT_CLR, host->base + MCI_RINTSTS);

	spin_lock_irqsave(&host->lock, flags);
	host->pending_events = 0;
	spin_unlock_irqrestore(&host->lock, flags);

	/* MASK MMC all host intr */
	tmp_reg = mci_readl(host->base + MCI_INTMASK);
	tmp_reg &= ~ALL_INT_MASK;
	tmp_reg |= DATA_INT_MASK;
	mci_writel(tmp_reg, host->base + MCI_INTMASK);

	/* enable inner DMA mode and close intr of MMC host controler */
	tmp_reg = mci_readl(host->base + MCI_CTRL);
	tmp_reg &= ~INTR_EN;
	tmp_reg |= USE_INTERNAL_DMA | INTR_EN;
	mci_writel(tmp_reg, host->base + MCI_CTRL);

	/* set timeout param */
	mci_writel(DATA_TIMEOUT | RESPONSE_TIMEOUT, host->base + MCI_TIMEOUT);

	/* set FIFO param */
	tmp_reg = 0;
	tmp_reg |= BURST_SIZE | RX_WMARK | TX_WMARK;
	mci_writel(tmp_reg, host->base + MCI_FIFOTH);

	host->error_count = 0;
	host->data_error_count = 0;
}

#define MCI_CARD_DETECT_TIMES 5
#define MCI_CARD_DETECT_DELAY 10
static void mci_detect_card(struct timer_list *timer)
{
	struct mci_host *host = container_of(timer, struct mci_host, timer);
	unsigned int i, curr_status, status[5];
	unsigned int detect_retry_count = 0;

	mci_assert(host);

	while (1) {
		for (i = 0; i < MCI_CARD_DETECT_TIMES; i++) {
			status[i] = mci_sys_card_detect(host);
			udelay(MCI_CARD_DETECT_DELAY);
		}
		if ((status[0] == status[1])
		    && (status[0] == status[2])
		    && (status[0] == status[3])
		    && (status[0] == status[4]))
			break;

		detect_retry_count++;
		if (detect_retry_count >= retry_count) {
			mci_error("this is a dithering, card detect error!");
			goto err;
		}
	}
	curr_status = status[0];
	if (curr_status != host->card_status) {
		mci_trace(MCI_TRACE_LEVEL_INFO, "begin card_status = %d\n", host->card_status);
		host->card_status = curr_status;
		if (curr_status != CARD_UNPLUGED) {
			mci_sys_ctrl_init(host);
			mci_init_host(host);
			pr_info("card connected!\n");
		} else {
			pr_info("card disconnected!\n");
		}

		mmc_detect_change(host->mmc, 0);
	}
err:
	mod_timer(timer, jiffies + detect_time);
}

static void mci_idma_start(const struct mci_host *host)
{
	unsigned int tmp;

	mci_trace(MCI_TRACE_LEVEL_INFO, "begin");
	mci_writel(host->dma_paddr, host->base + MCI_DBADDR);
	tmp = mci_readl(host->base + MCI_BMOD);
	tmp |= BMOD_DMA_EN;
	mci_writel(tmp, host->base + MCI_BMOD);
}

static void mci_idma_stop(const struct mci_host *host)
{
	unsigned int tmp_reg;

	mci_trace(MCI_TRACE_LEVEL_INFO, "begin");
	tmp_reg = mci_readl(host->base + MCI_BMOD);
	tmp_reg &= ~BMOD_DMA_EN;
	mci_writel(tmp_reg, host->base + MCI_BMOD);
}

static void mci_idma_reset(const struct mci_host *host)
{
	u32 regval;

	regval = mci_readl(host->base + MCI_BMOD);
	regval |= BMOD_SWR;
	mci_writel(regval, host->base + MCI_BMOD);

	regval = mci_readl(host->base + MCI_CTRL);
	regval |= CTRL_RESET | FIFO_RESET | DMA_RESET;
	mci_writel(regval, host->base + MCI_CTRL);

	udelay(1);
	mci_writel(ALL_INT_CLR, host->base + MCI_RINTSTS);
}

static void mci_dump_des(const struct mci_des *des, unsigned int i)
{
	mci_trace(MCI_TRACE_LEVEL_INFO, "des[%d] vaddr  is 0x%08X", i,
		    (unsigned int)(uintptr_t)&des[i]);
	mci_trace(MCI_TRACE_LEVEL_INFO, "des[%d].idmac_des_ctrl is 0x%08X",
		    i, (unsigned int)des[i].idmac_des_ctrl);
	mci_trace(MCI_TRACE_LEVEL_INFO, "des[%d].idmac_des_buf_size is 0x%08X",
		    i, (unsigned int)des[i].idmac_des_buf_size);
	mci_trace(MCI_TRACE_LEVEL_INFO, "des[%d].idmac_des_buf_addr 0x%08X",
		    i, (unsigned int)des[i].idmac_des_buf_addr);
	mci_trace(MCI_TRACE_LEVEL_INFO, "des[%d].idmac_des_next_addr is 0x%08X",
		    i, (unsigned int)des[i].idmac_des_next_addr);
}

static unsigned long mci_filling_des(struct mci_des *des, unsigned long sg_length,
				       unsigned int des_cnt, unsigned long sg_phyaddr, const struct mci_host *host)
{
	des[des_cnt].idmac_des_ctrl = DMA_DES_OWN | DMA_DES_NEXT_DES;
	des[des_cnt].idmac_des_buf_addr = sg_phyaddr;
	/* idmac_des_next_addr is paddr for dma */
	des[des_cnt].idmac_des_next_addr = host->dma_paddr
					   + (des_cnt + 1) * sizeof(struct mci_des);
	if (sg_length >= 0x1000) {
		des[des_cnt].idmac_des_buf_size = 0x1000;
		return des[des_cnt].idmac_des_buf_size;
	} else {
		des[des_cnt].idmac_des_buf_size = sg_length;
		return sg_length;
	}
}

static void mci_setup_host(struct mci_host *host, struct mmc_data *data)
{
	mci_trace(MCI_TRACE_LEVEL_INFO, "begin");
	mci_assert(host);
	mci_assert(data);

	mci_trace(MCI_TRACE_LEVEL_INFO, "host->dma_paddr is 0x%08lx,host->dma_vaddr is 0x%08lx\n",
		    (uintptr_t)host->dma_paddr,
		    (uintptr_t)host->dma_vaddr);

	host->data = data;
	if (data->flags & MMC_DATA_READ)
		host->dma_dir = DMA_FROM_DEVICE;
	else
		host->dma_dir = DMA_TO_DEVICE;

	host->dma_sg = data->sg;
	host->dma_sg_num = dma_map_sg(mmc_dev(host->mmc),
				      data->sg, data->sg_len, host->dma_dir);
	mci_assert(host->dma_sg_num);
	mci_trace(MCI_TRACE_LEVEL_INFO, "host->dma_sg_num is %d\n", host->dma_sg_num);
}

static int mci_setup_data(struct mci_host *host, struct mmc_data *data)
{
	unsigned long sg_phyaddr, sg_length;
	unsigned int i;
	unsigned int ret = 0;
	unsigned int data_size;
	unsigned int max_des, des_cnt;
	struct mci_des *des = NULL;

	mci_setup_host(host, data);
	data_size = data->blksz * data->blocks;
	if (data_size > (DMA_BUFFER * MAX_DMA_DES)) {
		mci_error("mci request data_size is too big!\n");
		ret = -1;
		goto out;
	}

	max_des = (CMD_DES_PAGE_SIZE / sizeof(struct mci_des));
	des = (struct mci_des *)host->dma_vaddr;
	des_cnt = 0;

	for (i = 0; i < host->dma_sg_num; i++) {
		sg_length = sg_dma_len(&data->sg[i]);
		sg_phyaddr = sg_dma_address(&data->sg[i]);
		mci_trace(MCI_TRACE_LEVEL_INFO, "sg[%d] sg_length is 0x%08X, sg_phyaddr is 0x%08X\n",
			    i, (unsigned int)sg_length, (unsigned int)sg_phyaddr);
		while (sg_length) {
			unsigned long des_len = mci_filling_des(des, sg_length, des_cnt,
						sg_phyaddr, host);
			/* buffer size <= 4k */
			sg_length -= des_len;
			sg_phyaddr += des_len;
			des_cnt++;
		}

		mci_dump_des(des, i);
		mci_assert(des_cnt < max_des);
	}

	des[0].idmac_des_ctrl |= DMA_DES_FIRST_DES;
	des[des_cnt - 1].idmac_des_ctrl |= DMA_DES_LAST_DES;
	des[des_cnt - 1].idmac_des_next_addr = 0;
out:
	return ret;
}

static int mci_set_cmd_arg(volatile union cmd_arg_u *cmd_regs, const struct mci_host *host,
			     const struct mmc_command *cmd)
{
	cmd_regs->bits.send_auto_stop = 0;
#ifdef CONFIG_SEND_AUTO_STOP
	if ((host->mrq->stop) && (!(host->is_tuning)))
		cmd_regs->bits.send_auto_stop = 1;
#endif
	if (cmd == host->mrq->stop ||
	    cmd->opcode == MMC_STOP_TRANSMISSION) {
		cmd_regs->bits.stop_abort_cmd = 1;
		cmd_regs->bits.wait_prvdata_complete = 0;
	} else if (cmd->opcode == MMC_SEND_STATUS) {
		cmd_regs->bits.stop_abort_cmd = 0;
		cmd_regs->bits.wait_prvdata_complete = 0;
	} else {
		cmd_regs->bits.stop_abort_cmd = 0;
		cmd_regs->bits.wait_prvdata_complete = 1;
	}

	switch (mmc_resp_type(cmd)) {
	case MMC_RSP_NONE:
		cmd_regs->bits.response_expect = 0;
		cmd_regs->bits.response_length = 0;
		cmd_regs->bits.check_response_crc = 0;
		break;
	case MMC_RSP_R1:
	case MMC_RSP_R1B:
		cmd_regs->bits.response_expect = 1;
		cmd_regs->bits.response_length = 0;
		cmd_regs->bits.check_response_crc = 1;
		break;
	case MMC_RSP_R2:
		cmd_regs->bits.response_expect = 1;
		cmd_regs->bits.response_length = 1;
		cmd_regs->bits.check_response_crc = 1;
		break;
	case MMC_RSP_R3:
	case MMC_RSP_R1 & (~MMC_RSP_CRC):
		cmd_regs->bits.response_expect = 1;
		cmd_regs->bits.response_length = 0;
		cmd_regs->bits.check_response_crc = 0;
		break;
	default:
		host->cmd->error = -EINVAL;
		mci_error("mci: unhandled response type %02x\n", mmc_resp_type(cmd));
		return -EINVAL;
	}
	return 0;
}

static int mci_exec_cmd(struct mci_host *host, struct mmc_command *cmd,
			  const struct mmc_data *data)
{
	volatile union cmd_arg_u cmd_regs;

	mci_trace(MCI_TRACE_LEVEL_INFO, "begin");
	mci_assert(host);
	mci_assert(cmd);

	host->cmd = cmd;

	mci_writel(cmd->arg, host->base + MCI_CMDARG);
	mci_trace(MCI_TRACE_LEVEL_REG, "arg_reg 0x%x, val 0x%x", MCI_CMDARG, cmd->arg);
	cmd_regs.cmd_arg = mci_readl(host->base + MCI_CMD);
	if (data) {
		cmd_regs.bits.data_transfer_expected = 1;
		if (data->flags & (MMC_DATA_WRITE | MMC_DATA_READ))
			cmd_regs.bits.transfer_mode = 0;

		if (data->flags & MMC_DATA_WRITE)
			cmd_regs.bits.read_write = 1;
		else if (data->flags & MMC_DATA_READ)
			cmd_regs.bits.read_write = 0;
	} else {
		cmd_regs.bits.data_transfer_expected = 0;
		cmd_regs.bits.transfer_mode = 0;
		cmd_regs.bits.read_write = 0;
	}

	if (mci_set_cmd_arg(&cmd_regs, host, cmd))
		return -EINVAL;

	mci_trace(MCI_TRACE_LEVEL_WAR, "cmd->opcode = %d cmd->arg = 0x%X\n", cmd->opcode, cmd->arg);
	if (cmd->opcode == MMC_SELECT_CARD)
		host->card_rca = (cmd->arg >> 16);

	if (cmd->opcode == MMC_GO_IDLE_STATE)
		cmd_regs.bits.send_initialization = 1;
	else
		cmd_regs.bits.send_initialization = 0;
	/* CMD 11 check switch voltage */
	if (cmd->opcode == SD_SWITCH_VOLTAGE)
		cmd_regs.bits.volt_switch = 1;
	else
		cmd_regs.bits.volt_switch = 0;

	cmd_regs.bits.card_number = host->port;
	cmd_regs.bits.cmd_index = cmd->opcode;
	cmd_regs.bits.start_cmd = 1;
	cmd_regs.bits.update_clk_reg_only = 0;

	mci_writel(DATA_INT_MASK, host->base + MCI_RINTSTS);
	mci_writel(cmd_regs.cmd_arg, host->base + MCI_CMD);
	mci_trace(MCI_TRACE_LEVEL_REG, "cmd_reg 0x%x, val 0x%x\n", MCI_CMD, cmd_regs.cmd_arg);

	if (mci_wait_cmd(host) != 0) {
		mci_trace(MCI_TRACE_LEVEL_WAR, "send card cmd is failed!");
		return -EINVAL;
	}
	return 0;
}

static void mci_finish_request(struct mci_host *host,
				 struct mmc_request *mrq)
{
	mci_trace(MCI_TRACE_LEVEL_INFO, "begin");
	mci_assert(host);
	mci_assert(mrq);

	host->mrq = NULL;
	host->cmd = NULL;
	host->data = NULL;
	mmc_request_done(host->mmc, mrq);
}

#define CMD_ERRORS                          \
	(R1_OUT_OF_RANGE |  /* Command argument out of range */ \
	 R1_ADDRESS_ERROR | /* Misaligned address */        \
	 R1_BLOCK_LEN_ERROR |   /* Transferred block length incorrect */ \
	 R1_WP_VIOLATION |  /* Tried to write to protected block */ \
	 R1_CC_ERROR |      /* Card controller error */     \
	 R1_ERROR)      /* General/unknown error */

#define MCI_WAIR_CMD_DONE_DELAY 1000
static void mci_cmd_done(struct mci_host *host, unsigned int stat)
{
	unsigned int i;
	struct mmc_command *cmd = host->cmd;

	mci_trace(MCI_TRACE_LEVEL_INFO, "begin");
	mci_assert(host);
	mci_assert(cmd);

	for (i = 0; i < 4; i++) {
		if (mmc_resp_type(cmd) == MMC_RSP_R2) {
			cmd->resp[i] = mci_readl(host->base +
						   MCI_RESP3 - i * 0x4);
			/* R2 must delay some time here ,when use UHI card,
			   need check why */
			udelay(MCI_WAIR_CMD_DONE_DELAY);
		} else {
			cmd->resp[i] = mci_readl(host->base +
						   MCI_RESP0 + i * 0x4);
		}
	}

	if (stat & RTO_INT_STATUS) {
		cmd->error = -ETIMEDOUT;
		mci_trace(MCI_TRACE_LEVEL_WAR, "irq cmd status stat = 0x%x is timeout error!",
			    stat);
	} else if (stat & (RCRC_INT_STATUS | RE_INT_STATUS)) {
		cmd->error = -EILSEQ;
		mci_trace(MCI_TRACE_LEVEL_WAR, "irq cmd status stat = 0x%x is response error!",
			    stat);
	}

	if (((cmd->flags & MMC_RSP_R1) == MMC_RSP_R1) &&
	    ((cmd->flags & MMC_CMD_MASK) != MMC_CMD_BCR)) {
		if ((cmd->resp[0] & CMD_ERRORS) && !host->is_tuning) {
			host->error_count++;
			host->mrq->cmd->error = -EACCES;
			mci_trace(MCI_TRACE_LEVEL_ERR, "The status of the card is abnormal, cmd->resp[0]: %x",
				    cmd->resp[0]);
		}

		/* bad card situation: the TF card returns the contradictory card statut.
		 * that is, the card is in the ready state and in the programming state.
		 */
		if ((cmd->resp[0] & R1_READY_FOR_DATA) && (R1_CURRENT_STATE(cmd->resp[0]) ==
				R1_STATE_PRG)) {
			host->error_count++;
			host->mrq->cmd->error = -EACCES;
			mci_trace(MCI_TRACE_LEVEL_ERR, "The status of the card is abnormal, cmd->resp[0]: %x",
				    cmd->resp[0]);
		}
	}

	host->cmd = NULL;
}

static void mci_data_done(struct mci_host *host, unsigned int stat)
{
	struct mmc_data *data = host->data;

	mci_trace(MCI_TRACE_LEVEL_INFO, "begin");
	mci_assert(host);
	mci_assert(data);

	dma_unmap_sg(mmc_dev(host->mmc), data->sg, data->sg_len, host->dma_dir);

	if (stat & (HTO_INT_STATUS | DRTO_INT_STATUS)) {
		data->error = -ETIMEDOUT;
		mci_trace(MCI_TRACE_LEVEL_WAR, "irq data status stat = 0x%x is timeout error!",
			    stat);
	} else if (stat & (EBE_INT_STATUS | SBE_INT_STATUS |
			   FRUN_INT_STATUS | DCRC_INT_STATUS)) {
		data->error = -EILSEQ;
		mci_trace(MCI_TRACE_LEVEL_WAR, "irq data status stat = 0x%x is data error!",
			    stat);
	}

	if (!data->error)
		data->bytes_xfered = data->blocks * data->blksz;
	else
		data->bytes_xfered = 0;

	host->data = NULL;
}

static int mci_wait_cmd_complete_time_out(struct mci_host *host)
{
	unsigned int cmd_retry_count = 0;
	unsigned long flags;
	unsigned int cmd_irq_reg;
	do {
		spin_lock_irqsave(&host->lock, flags);
		cmd_irq_reg = readl(host->base + MCI_RINTSTS);
		if (cmd_irq_reg & CD_INT_STATUS) {
			mci_writel((CD_INT_STATUS | RTO_INT_STATUS
				      | RCRC_INT_STATUS | RE_INT_STATUS),
				     host->base + MCI_RINTSTS);
			spin_unlock_irqrestore(&host->lock, flags);
			mci_cmd_done(host, cmd_irq_reg);
			return 0;
		} else if (cmd_irq_reg & VOLT_SWITCH_INT_STATUS) {
			mci_writel(VOLT_SWITCH_INT_STATUS,
				     host->base + MCI_RINTSTS);
			spin_unlock_irqrestore(&host->lock, flags);
			mci_cmd_done(host, cmd_irq_reg);
			return 0;
		}
		spin_unlock_irqrestore(&host->lock, flags);
		cmd_retry_count++;
	} while (cmd_retry_count < retry_count);
	return 1;
}

static int mci_wait_cmd_complete(struct mci_host *host)
{
	unsigned int cmd_retry_count;
	unsigned long cmd_jiffies_timeout;
	struct mmc_command *cmd = host->cmd;

	mci_trace(MCI_TRACE_LEVEL_INFO, "begin");
	mci_assert(host);
	mci_assert(cmd);

	cmd_jiffies_timeout = jiffies + request_timeout;
	while (1) {
		if (!mci_wait_cmd_complete_time_out(host))
			return 0;

		cmd_retry_count = 0;

		if (host->card_status == CARD_UNPLUGED) {
			cmd->error = -ETIMEDOUT;
			return -1;
		}

		if (!time_before(jiffies, cmd_jiffies_timeout)) {
			unsigned int i;
			for (i = 0; i < 4; i++) {
				cmd->resp[i] = mci_readl(host->base +
							   MCI_RESP0 +
							   i * 0x4);
				pr_err("voltage switch read MCI_RESP");
				pr_err("%d : 0x%x\n", i, cmd->resp[i]);
			}
			cmd->error = -ETIMEDOUT;
			mci_trace(MCI_TRACE_LEVEL_WAR, "wait cmd request complete is timeout!");
			return -1;
		}

		schedule();
	}
}
/*
 * designware support send stop command automatically when
 * read or wirte multi blocks
 */
#ifdef CONFIG_SEND_AUTO_STOP
static int mci_wait_auto_stop_complete(struct mci_host *host)
{
	unsigned int cmd_retry_count = 0;
	unsigned long cmd_jiffies_timeout;
	unsigned int cmd_irq_reg;
	unsigned long flags;

	mci_trace(MCI_TRACE_LEVEL_INFO, "begin");
	mci_assert(host);

	cmd_jiffies_timeout = jiffies + request_timeout;
	while (1) {
		do {
			spin_lock_irqsave(&host->lock, flags);
			cmd_irq_reg = readl(host->base + MCI_RINTSTS);
			if (cmd_irq_reg & ACD_INT_STATUS) {
				mci_writel((ACD_INT_STATUS | RTO_INT_STATUS |
					      RCRC_INT_STATUS |
					      RE_INT_STATUS),
					     host->base + MCI_RINTSTS);
				spin_unlock_irqrestore(&host->lock, flags);
				return 0;
			}
			spin_unlock_irqrestore(&host->lock, flags);
			cmd_retry_count++;
		} while (cmd_retry_count < retry_count);

		cmd_retry_count = 0;
		if (host->card_status == CARD_UNPLUGED)
			return -1;
		if (!time_before(jiffies, cmd_jiffies_timeout)) {
			mci_trace(MCI_TRACE_LEVEL_WAR, "wait auto stop complete is timeout!");
			return -1;
		}

		schedule();
	}
}
#endif

static int mci_wait_data_complete(struct mci_host *host)
{
	unsigned int tmp_reg;
	struct mmc_data *data = host->data;
	long time = request_timeout;
	unsigned long flags;

	mci_trace(MCI_TRACE_LEVEL_INFO, "begin");
	mci_assert(host);
	mci_assert(data);

	time = wait_event_timeout(host->intr_wait,
				  test_bit(MCI_PEND_DTO_B,
					   &host->pending_events), time);

	/* Mask MMC host data intr */
	spin_lock_irqsave(&host->lock, flags);
	tmp_reg = mci_readl(host->base + MCI_INTMASK);
	tmp_reg &= ~DATA_INT_MASK;
	mci_writel(tmp_reg, host->base + MCI_INTMASK);
	host->pending_events &= ~MCI_PEND_DTO_M;
	spin_unlock_irqrestore(&host->lock, flags);

	if (((time <= 0) &&
	     (!test_bit(MCI_PEND_DTO_B, &host->pending_events))) ||
	    (host->card_status == CARD_UNPLUGED)) {
		data->error = -ETIMEDOUT;
		mci_trace(MCI_TRACE_LEVEL_ERR, "wait data request complete is timeout! 0x%08X",
			    host->irq_status);
		mci_idma_stop(host);
		mci_data_done(host, host->irq_status);
		return -1;
	}

	mci_idma_stop(host);
	mci_data_done(host, host->irq_status);
	return 0;
}

static int mci_wait_card_complete(const struct mci_host *host,
				    const struct mmc_data *data)
{
	unsigned int card_retry_count = 0;
	unsigned long card_jiffies_timeout;
	unsigned int card_status_reg;

	mci_trace(MCI_TRACE_LEVEL_INFO, "begin");
	mci_assert(host);

	card_jiffies_timeout = jiffies + request_timeout;
	while (1) {
		do {
			card_status_reg = readl(host->base + MCI_STATUS);
			if (!(card_status_reg & DATA_BUSY)) {
				mci_trace(MCI_TRACE_LEVEL_INFO, "end");
				return 0;
			}
			card_retry_count++;
		} while (card_retry_count < retry_count);

		card_retry_count = 0;

		if (host->card_status == CARD_UNPLUGED) {
			host->mrq->cmd->error = -ETIMEDOUT;
			mci_trace(MCI_TRACE_LEVEL_WAR, "card is unpluged!");
			return -1;
		}

		if (!time_before(jiffies, card_jiffies_timeout)) {
			host->mrq->cmd->error = -ETIMEDOUT;
			mci_trace(MCI_TRACE_LEVEL_WAR, "wait card ready complete is timeout!");
			return -1;
		}

		schedule();
	}
}

static int mci_request_prepare_data(const struct mmc_request *mrq,
				      struct mci_host *host,
				      int *byte_cnt)
{
	int ret;
	unsigned int tmp_reg;
	unsigned int fifo_count = 0;
	if (mrq->data) {
		ret = mci_setup_data(host, mrq->data);
		if (ret) {
			mrq->data->error = ret;
			mci_trace(MCI_TRACE_LEVEL_WAR, "data setup is error!");
			return ret;
		}

		*byte_cnt = mrq->data->blksz * mrq->data->blocks;
		mci_writel(*byte_cnt, host->base + MCI_BYTCNT);
		mci_writel(mrq->data->blksz, host->base + MCI_BLKSIZ);

		/* reset fifo */
		tmp_reg = mci_readl(host->base + MCI_CTRL);
		tmp_reg |= FIFO_RESET;
		mci_writel(tmp_reg, host->base + MCI_CTRL);

		do {
			tmp_reg = mci_readl(host->base + MCI_CTRL);
			fifo_count++;
			if (fifo_count >= retry_count) {
				pr_info("fifo reset is timeout!");
				return ret;
			}
		} while (tmp_reg & FIFO_RESET);

		/* start DMA */
		mci_idma_start(host);
	} else {
		mci_writel(0, host->base + MCI_BYTCNT);
		mci_writel(0, host->base + MCI_BLKSIZ);
	}
	if (mrq->sbc) {
		ret = mci_exec_cmd(host, mrq->sbc, NULL);
		if (ret) {
			mrq->sbc->error = ret;
			return ret;
		}

		/* wait command send complete */
		ret = mci_wait_cmd_complete(host);
		if (ret) {
			mrq->sbc->error = ret;
			return ret;
		}
	}
	return 0;
}

#define MCI_CLEAR_ERR_INT_DELAY 100
#define MCI_CLEAR_ERR_INT_TRY_TIMES 1000
static void mci_wait_err_handle_done(const struct mci_host *host)
{
	unsigned int stat;
	unsigned int wait_retry_count = 0;

	do {
		stat = mci_readl(host->base + MCI_RINTSTS);
		if (stat & (HTO_INT_STATUS | DRTO_INT_STATUS |
			    EBE_INT_STATUS | SBE_INT_STATUS |
			    FRUN_INT_STATUS | DCRC_INT_STATUS)) {
			mci_writel(stat, host->base + MCI_RINTSTS);
			mci_trace(MCI_TRACE_LEVEL_WAR, "data status = 0x%x is error!", stat);
			mci_trace(MCI_TRACE_LEVEL_WAR, "udelay count = %d is error!", wait_retry_count);
			break;
		}
		udelay(MCI_CLEAR_ERR_INT_DELAY);
		wait_retry_count++;
	} while (wait_retry_count < MCI_CLEAR_ERR_INT_TRY_TIMES);
}

static void mci_open_data_intreg(struct mci_host *host)
{
	unsigned int tmp_reg;
	unsigned long flags;
	spin_lock_irqsave(&host->lock, flags);
	tmp_reg = mci_readl(host->base + MCI_INTMASK);
	tmp_reg |= DATA_INT_MASK;
	mci_writel(tmp_reg, host->base + MCI_INTMASK);
	spin_unlock_irqrestore(&host->lock, flags);
}

static void mci_request_start_transfer(const struct mmc_request *mrq,
		struct mci_host *host, int byte_cnt)
{
	int ret;
	if (mrq->data == NULL)
		return;

	if (!(mrq->cmd->error)) {
		/* Open MMC host data intr */
		mci_open_data_intreg(host);
		/* wait data transfer complete */
		mci_wait_data_complete(host);
	} else if (host->is_tuning) {
		mci_wait_err_handle_done(host);
		/* CMD error in data command */
		mci_idma_stop(host);
	} else {
		/* CMD error in data command */
		mci_idma_stop(host);
	}

	if (mrq->stop && (!mrq->sbc
				|| (mrq->sbc && (mrq->cmd->error || mrq->data->error)))) {
#ifdef CONFIG_SEND_AUTO_STOP
		int trans_cnt;

		trans_cnt = mci_readl(host->base + MCI_TCBCNT);
		/* send auto stop */
		if ((trans_cnt == byte_cnt) && (!(host->is_tuning))) {
			mci_trace(MCI_TRACE_LEVEL_WAR, "byte_cnt = %d, trans_cnt = %d",
					byte_cnt, trans_cnt);
			ret = mci_wait_auto_stop_complete(host);
			if (ret) {
				mrq->stop->error = -ETIMEDOUT;
				return;
			}
		} else {
#endif
			/* send soft stop command */
			mci_trace(MCI_TRACE_LEVEL_WAR, "this time, send soft stop");
			ret = mci_exec_cmd(host, host->mrq->stop,
					host->data);
			if (ret) {
				mrq->stop->error = ret;
				return;
			}
			ret = mci_wait_cmd_complete(host);
			if (ret)
				return;
#ifdef CONFIG_SEND_AUTO_STOP
		}
#endif
	}
}

static void mci_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct mci_host *host = mmc_priv(mmc);
	int byte_cnt = 0;
	int ret;
	unsigned long flags;

	mci_trace(MCI_TRACE_LEVEL_INFO, "begin");
	mci_assert(mmc);
	mci_assert(mrq);
	mci_assert(host);

	host->mrq = mrq;
	host->irq_status = 0;

	if (host->card_status == CARD_UNPLUGED) {
		mrq->cmd->error = -ENODEV;
		goto request_end;
	}

	ret = mci_wait_card_complete(host, mrq->data);
	if (ret) {
		mrq->cmd->error = ret;
		goto request_end;
	}
	if (mci_request_prepare_data(mrq, host, &byte_cnt))
		goto request_end;
	/* send command */
	ret = mci_exec_cmd(host, mrq->cmd, mrq->data);
	if (ret) {
		mrq->cmd->error = ret;
		mci_idma_stop(host);
		mci_trace(MCI_TRACE_LEVEL_WAR, "can't send card cmd! ret = %d", ret);
		goto request_end;
	}

	/* wait command send complete */
	mci_wait_cmd_complete(host);

	/* start data transfer */
	mci_request_start_transfer(mrq, host, byte_cnt);

request_end:
	/* clear MMC host intr */
	spin_lock_irqsave(&host->lock, flags);
	mci_writel(ALL_SD_INT_CLR, host->base + MCI_RINTSTS);
	spin_unlock_irqrestore(&host->lock, flags);

	if (mrq->data && mrq->data->error && !host->is_tuning)
		host->data_error_count++;
	mci_finish_request(host, mrq);
}

static int mci_switch_to_33_and_fail(const struct mci_host *host)
{
	u32 ctrl;
	ctrl = mci_readl(host->base + MCI_UHS_REG);
	/* Set 1.8V Signal Enable in the MCI_UHS_REG to 1 */
	mci_trace(MCI_TRACE_LEVEL_WAR, "switch voltage 330");
	ctrl &= ~(SDXC_CTRL_VDD_180 << host->port);
	mci_writel(ctrl, host->base + MCI_UHS_REG);

	/* Wait for 5ms */
	usleep_range(5000, 5500);

	/* 3.3V regulator output should be stable within 5 ms */
	ctrl = mci_readl(host->base + MCI_UHS_REG);
	if (!(ctrl & (SDXC_CTRL_VDD_180 << host->port))) {
		/* config Pin drive capability */
		mci_set_drv_cap(host, 0);
		return 0;
	} else {
		mci_error(": Switching to 3.3V ");
		mci_error("signalling voltage failed\n");
		return -EIO;
	}
}

static int mci_switch_to_18_and_fail(struct mci_host *host)
{
	u32 ctrl;
	ctrl = mci_readl(host->base + MCI_UHS_REG);
	mci_trace(MCI_TRACE_LEVEL_WAR, "switch voltage 180");
	mci_control_cclk(host, DISABLE);

	/*
	 * Enable 1.8V Signal Enable in the MCI_UHS_REG
	 */
	ctrl |= (SDXC_CTRL_VDD_180 << host->port);
	mci_writel(ctrl, host->base + MCI_UHS_REG);

	/* Wait for 5ms */
	usleep_range(8000, 8500);

	ctrl = mci_readl(host->base + MCI_UHS_REG);
	if (ctrl & (SDXC_CTRL_VDD_180 << host->port)) {
		/* Provide SDCLK again and wait for 1ms */
		mci_control_cclk(host, ENABLE);
		usleep_range(1000, 1500);

		/* eMMC needn't to check the int status */
		if (host->mmc->caps2 & MMC_CAP2_HS200)
			return 0;
		/*
		 * If CMD11 return CMD down, then the card
		 * was successfully switched to 1.8V signaling.
		 */
		ctrl = mci_readl(host->base + MCI_RINTSTS);
		if ((ctrl & VOLT_SWITCH_INT_STATUS)
		    && (ctrl & CD_INT_STATUS)) {
			mci_writel(VOLT_SWITCH_INT_STATUS | CD_INT_STATUS,
				     host->base + MCI_RINTSTS);
			/* config Pin drive capability */
			mci_set_drv_cap(host, 1);
			return 0;
		}
	}
	return -1;
}

static int mci_do_voltage_switch(struct mci_host *host,
				   const struct mmc_ios *ios)
{
	u32 ctrl;

	/*
	 * We first check whether the request is to set signalling voltage
	 * to 3.3V. If so, we change the voltage to 3.3V and return quickly.
	 */
	ctrl = mci_readl(host->base + MCI_UHS_REG);
	if (ios->signal_voltage == MMC_SIGNAL_VOLTAGE_330) {
		if (mci_switch_to_33_and_fail(host))
			return -EIO;
	} else if (!(ctrl & (SDXC_CTRL_VDD_180 << host->port)) &&
		   (ios->signal_voltage == MMC_SIGNAL_VOLTAGE_180)) {
		/* Stop SDCLK */
		if (!mci_switch_to_18_and_fail(host))
			return 0;

		/*
		 * If we are here, that means the switch to 1.8V signaling
		 * failed. We power cycle the card, and retry initialization
		 * sequence by setting S18R to 0.
		 */

		ctrl = mci_readl(host->base + MCI_UHS_REG);
		ctrl &= ~(SDXC_CTRL_VDD_180 << host->port);
		mci_writel(ctrl, host->base + MCI_UHS_REG);

		/* Wait for 5ms */
		usleep_range(5000, 5500);

		mci_ctrl_power(host, POWER_OFF, FORCE_DISABLE);
		/* Wait for 1ms as per the spec */
		usleep_range(1000, 1500);
		mci_ctrl_power(host, POWER_ON, FORCE_DISABLE);

		mci_control_cclk(host, DISABLE);

		/* Wait for 1ms as per the spec */
		usleep_range(1000, 1500);
		mci_control_cclk(host, ENABLE);

		mci_error(": Switching to 1.8V signalling ");
		mci_error("voltage failed, retrying with S18R set to 0\n");
		return -EAGAIN;
	}
	/* No signal voltage switch required */
	return 0;
}

static int mci_start_signal_voltage_switch(struct mmc_host *mmc,
		struct mmc_ios *ios)
{
	struct mci_host *host = mmc_priv(mmc);
	int err;

	err = mci_do_voltage_switch(host, ios);
	return err;
}

static int mci_send_stop(struct mmc_host *host)
{
	struct mmc_command cmd = {0};
	int err;

	cmd.opcode = MMC_STOP_TRANSMISSION;
	cmd.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC;
	err = mmc_wait_for_cmd(host, &cmd, 0);
	return err;
}

static void mci_set_sap_phase(struct mci_host *host, u32 phase)
{
	unsigned int reg_value;
	unsigned long flags;

	spin_lock_irqsave(&host->lock, flags);

	reg_value = mci_readl(host->base + MCI_UHS_REG_EXT);
	reg_value &= ~CLK_SMPL_PHS_MASK;
	reg_value |= (phase << CLK_SMPL_PHS_SHIFT);
	mci_writel(reg_value, host->base + MCI_UHS_REG_EXT);

	spin_unlock_irqrestore(&host->lock, flags);
}

#if defined(CONFIG_ARCH_SS813V100) || defined(CONFIG_ARCH_SS812V100) || \
	defined(CONFIG_ARCH_SS313V100)  || defined(CONFIG_ARCH_SS312V100)  || \
	defined(CONFIG_ARCH_SS011V100)  || defined(CONFIG_ARCH_SS012V100)
static void mci_edge_tuning_enable(struct mci_host *host)
{
	unsigned int val;
	void __iomem *tmp_reg;

	if (host->devid == 0) {
		tmp_reg  = crg_ctrl + 0x14c;
	} else if (host->devid == 1) {
		tmp_reg = crg_ctrl + 0x164;
	} else if (mci_is_port_2(host->devid)) {
		tmp_reg = crg_ctrl + 0x158;
	} else {
		mci_trace(MCI_TRACE_LEVEL_ERR, "Devid error, host->devid: %x", host->devid);
		return;
	}

	mci_writel(0x80001, tmp_reg);

	val = mci_readl(host->base + MCI_TUNING_CTRL);
	val |= HW_TUNING_EN;
	mci_writel(val, host->base + MCI_TUNING_CTRL);
}

static void mci_edge_tuning_disable(struct mci_host *host)
{
	unsigned int val;
	void __iomem *tmp_reg;

	if (host->devid == 0) {
		tmp_reg  = crg_ctrl + 0x14c;
	} else if (host->devid == 1) {
		tmp_reg = crg_ctrl + 0x164;
	} else if (mci_is_port_2(host->devid)) {
		tmp_reg = crg_ctrl + 0x158;
	} else {
		mci_trace(MCI_TRACE_LEVEL_ERR, "Devid error, host->devid: %x", host->devid);
		return;
	}
	val = mci_readl(tmp_reg);
	val |= (1 << 16);
	mci_writel(val, tmp_reg);

	val = mci_readl(host->base + MCI_TUNING_CTRL);
	val &= ~HW_TUNING_EN;
	mci_writel(val, host->base + MCI_TUNING_CTRL);
}

static int mci_send_status(struct mmc_host *mmc)
{
	int err;
	struct mmc_command cmd = {0};
	struct mci_host *host = NULL;

	BUG_ON(!mmc);

	host = mmc_priv(mmc);
	cmd.opcode = MMC_SEND_STATUS;
	if (!mmc_host_is_spi(mmc))
		cmd.arg = (host->card_rca << 16);
	cmd.flags = MMC_RSP_SPI_R2 | MMC_RSP_R1 | MMC_CMD_AC;

	err = mmc_wait_for_cmd(mmc, &cmd, 1);
	if (err)
		return err;

	return 0;
}

static int mci_send_tuning(struct mmc_host *mmc, u32 opcode)
{
	int err;
	struct mci_host *host;
	/* fix a problem that When the I/O voltage is increased to 1.89 V or 1.91V
	 * at high and low temperatures, the system is suspended during the reboot test.
	 */
	unsigned cmd_count = 1000;

	host = mmc_priv(mmc);
	mci_control_cclk(host, DISABLE);
tuning_retry:
	mci_idma_reset(host);
	mci_writel(ALL_INT_CLR, host->base + MCI_RINTSTS);
	mci_control_cclk(host, ENABLE);
	if (tuning_reset_flag == 1) {
		tuning_reset_flag = 0;
		cmd_count--;
		if (cmd_count == 0) {
			printk("BUG_ON:controller reset is failed!!!\n");
			return -EINVAL;
		}
		goto tuning_retry;
	}

	err = mmc_send_tuning(mmc, opcode, NULL);
	mci_send_stop(mmc);
	mci_send_status(mmc);
	return err;
}

static u32 mci_get_sap_dll_taps(struct mci_host *host)
{
	u32 regval;
	void __iomem *reg_sap_dll_status = 0;

	if (host->devid == 0) {
		reg_sap_dll_status = crg_ctrl + 0x150;
	} else if (host->devid == 1) {
		reg_sap_dll_status = crg_ctrl + 0x168;
	} else if (mci_is_port_2(host->devid)) {
		reg_sap_dll_status = crg_ctrl + 0x15c;
	} else {
		mci_trace(MCI_TRACE_LEVEL_ERR, "Devid error, host->devid: %x", host->devid);
		return 0;
	}
	regval = mci_readl(reg_sap_dll_status);

	return (regval & 0xff);
}

static void mci_set_dll_element(struct mci_host *host, u32 element)
{
	u32 regval;
	void __iomem *reg_sap_dll_ctrl = 0;

	if (host->devid == 0) {
		reg_sap_dll_ctrl = crg_ctrl + 0x14c;
	} else if (host->devid == 1) {
		reg_sap_dll_ctrl = crg_ctrl + 0x164;
	} else if (mci_is_port_2(host->devid)) {
		reg_sap_dll_ctrl = crg_ctrl + 0x158;
	} else {
		mci_trace(MCI_TRACE_LEVEL_ERR, "Devid error, host->devid: %x", host->devid);
		return;
	}
	regval = mci_readl(reg_sap_dll_ctrl);
	regval &= ~(0xFF << 8);
	regval |= (element << 8);
	mci_writel(regval, reg_sap_dll_ctrl);
}

/*********************************************
 *********************************************
 EdgeMode A:
 |<---- totalphases(ele) ---->|
        _____________
 ______|||||||||||||||_______
 edge_p2f       edge_f2p
 (endp)         (startp)

 EdgeMode B:
 |<---- totalphases(ele) ---->|
  ________           _________
 ||||||||||_________|||||||||||
 edge_f2p     edge_p2f
 (startp)     (endp)

 BestPhase:
 if(endp < startp)
 endp = endp + totalphases;
 Best = ((startp + endp) / 2) % totalphases
**********************************************
**********************************************/
static int mci_edgedll_mode_tuning(struct mci_host *host, u32 opcode,
				     int edge_p2f, int edge_f2p)
{
	u32 index, found, startp, endp, startp_init, endp_init, phaseoffset, totalphases;
	u16 ele, start_ele, phase_dll_elements;
	u8 mdly_tap_flag;
	int prev_err = 0;
	int err;
	u32 phase_num = MCI_PHASE_SCALE;

	mci_trace(MCI_TRACE_LEVEL_WAR, "begin");

	mdly_tap_flag = mci_get_sap_dll_taps(host);
	phase_dll_elements = mdly_tap_flag / MCI_PHASE_SCALE;
	totalphases = phase_dll_elements * phase_num;

	startp_init = edge_f2p * phase_dll_elements;
	endp_init = edge_p2f * phase_dll_elements;
	startp = startp_init;
	endp = endp_init;

	found = 1;
	start_ele = 2;

	/* Note: edgedll tuning must from edge_p2f to edge_f2p */
	if (edge_f2p >=  edge_p2f) {
		phaseoffset = edge_p2f * phase_dll_elements;
		for (index = edge_p2f; index < edge_f2p; index++) {
			/* set phase shift */
			mci_set_sap_phase(host, index);
			for (ele = start_ele; ele <= phase_dll_elements; ele++) {
				mci_set_dll_element(host, ele);
				err = mci_send_tuning(host->mmc, opcode);
				if (!err)
					found = 1;
				if (!prev_err && err && (endp == endp_init))
					endp = phaseoffset + ele;
				if (err)
					startp = phaseoffset + ele;

#ifdef TUNING_PROC_DEBUG
				printk("\tphase:%01d ele:%02d st:%03d end:%03d error:%d\n", index, ele, startp,
				       endp, err);
#endif

				prev_err = err;
				err = 0;
			}
			phaseoffset += phase_dll_elements;
		}
	} else {
		phaseoffset = edge_p2f * phase_dll_elements;
		for (index = edge_p2f; index < phase_num; index++) {
			/* set phase shift */
			mci_set_sap_phase(host, index);
			for (ele = start_ele; ele <= phase_dll_elements; ele++) {
				mci_set_dll_element(host, ele);
				err = mci_send_tuning(host->mmc, opcode);
				if (!err)
					found = 1;
				if (!prev_err && err && (endp == endp_init))
					endp = phaseoffset + ele;
				if (err)
					startp = phaseoffset + ele;

#ifdef TUNING_PROC_DEBUG
				printk("\tphase:%02d ele:%02d st:%03d end:%03d error:%d\n", index, ele, startp,
				       endp, err);
#endif

				prev_err = err;
				err = 0;
			}
			phaseoffset += phase_dll_elements;
		}

		phaseoffset = 0;
		for (index = 0; index < edge_f2p; index++) {
			/* set phase shift */
			mci_set_sap_phase(host, index);
			for (ele = start_ele; ele <= phase_dll_elements; ele++) {
				mci_set_dll_element(host, ele);
				err = mci_send_tuning(host->mmc, opcode);
				if (!err)
					found = 1;

				if (!prev_err && err && (endp == endp_init))
					endp = phaseoffset + ele;

				if (err)
					startp = phaseoffset + ele;

#ifdef TUNING_PROC_DEBUG
				printk("\tphase:%02d ele:%02d st:%03d end:%03d error:%d\n", index, ele, startp,
				       endp, err);
#endif

				prev_err = err;
				err = 0;
			}
			phaseoffset += phase_dll_elements;
		}
	}

	if (found) {
		printk("scan elemnts: startp:%d endp:%d\n", startp, endp);

		if (endp <= startp)
			endp += totalphases;

		if (totalphases == 0) {
			printk(KERN_NOTICE "totalphases is zero\n");
			return -1;
		}
		phaseoffset = ((startp + endp) / 2) % totalphases;
		index = (phaseoffset / phase_dll_elements);
		ele = (phaseoffset % phase_dll_elements);
		ele = ((ele > start_ele) ? ele : start_ele);

		mci_set_sap_phase(host, index);
		mci_set_dll_element(host, ele);

		printk(KERN_NOTICE
		       "Tuning SampleClock. mix set phase:[%02d/%02d] ele:[%02d/%02d] \n", index,
		       (phase_num - 1), ele,
		       phase_dll_elements);
		mci_writel(ALL_INT_CLR, host->base + MCI_RINTSTS);
		return 0;
	}
	printk(KERN_NOTICE "No valid phase shift! use default\n");
	return -1;
}

static void mci_tuning_feedback(struct mmc_host *mmc)
{
	struct mci_host *host = mmc_priv(mmc);

	mci_control_cclk(host, DISABLE);
	msleep(1);
	mci_sys_reset(host);
	msleep(1);
	mci_writel(ALL_INT_CLR, host->base + MCI_RINTSTS);
	mci_control_cclk(host, ENABLE);
	msleep(1);
	host->pending_events = 0;
}

static int mci_check_tuning(struct mmc_host *mmc, u32 opcode)
{
	int err;

	err = mci_send_tuning(mmc, opcode);

	return 	err;
}

static int mci_found_edge(struct mmc_host *mmc, u32 opcode)
{
	u32 regval;
	int err;
	struct mci_host *host = mmc_priv(mmc);

	err = mci_send_tuning(mmc, opcode);
	if (!err) {
		regval = mci_readl(host->base + MCI_TUNING_CTRL);
		return ((regval & FOUND_EDGE) == FOUND_EDGE);
	} else {
		return 1;
	}

#ifdef TUNING_PROC_DEBUG
	printk("\tphase:%02d found:%02d p2f:%d f2p:%d error:%d\n", index, found,
	       edge_p2f, edge_f2p, err);
#endif
	return 0;
}

static int mci_execute_mix_mode_tuning(struct mmc_host *mmc, u32 opcode)
{
	struct mci_host *host = mmc_priv(mmc);
	u32 index;
	u32 prefound = 0;
	u32 edge_p2f, edge_f2p;
	u32 edge_num = 0;
	int err;
	u32 phase_num = MCI_PHASE_SCALE;

	mci_trace(MCI_TRACE_LEVEL_WAR, "begin");
	edge_p2f = 0;
	edge_f2p = phase_num;

	mci_edge_tuning_enable(host);

	for (index = 0; index < MCI_PHASE_SCALE; index++) {
		/* set phase shift */
		mci_set_sap_phase(host, index);
		if (mci_found_edge(mmc, opcode)) {
			edge_num++;
			if (!prefound)
				edge_p2f = index;
			prefound = 1;
		} else {
			if (prefound)
				edge_f2p = index;
			prefound = 0;
		}
		if ((edge_p2f != 0) && (edge_f2p != phase_num))
			break;
	}

	if ((edge_p2f == 0) && (edge_f2p == phase_num)) {
		printk("unfound correct edge! check your config is correct!!\n");
		return -1;
	}
	printk("scan edges:%d p2f:%d f2p:%d\n", edge_num, edge_p2f, edge_f2p);

	if (edge_f2p < edge_p2f)
		index = (edge_f2p + edge_p2f) / 2 % phase_num;
	else
		index = (edge_f2p + phase_num + edge_p2f) / 2 % phase_num;
	printk("mix set temp-phase %d\n", index);
	mci_set_sap_phase(host, index);
	err = mci_send_tuning(mmc, opcode);

	mci_edge_tuning_disable(host);

	err = mci_edgedll_mode_tuning(host, opcode, edge_p2f, edge_f2p);
	return err;
}

/*
 * The procedure of tuning the phase shift of sampling clock
 *
 * 1.Set a phase shift of 0° on cclk_in_sample
 * 2.Send the Tuning command to the card
 * 3.increase the phase shift value of cclk_in_sample until the
 *   correct sampling point is received such that the host does not
 *   see any of the errors.
 * 4.Mark this phase shift value as the starting point of the sampling
 *   window.
 * 5.increase the phase shift value of cclk_in_sample until the host
 *   sees the errors starting to come again or the phase shift value
 *   reaches 360°.
 * 6.Mark the last successful phase shift value as the ending
 *   point of the sampling window.
 *
 *     A window is established where the tuning block is matched.
 * For example, for a scenario where the tuning block is received
 * correctly for a phase shift window of 90°and 180°, then an appropriate
 * sampling point is established as 135°. Once a sampling point is
 * established, no errors should be visible in the tuning block.
 *
 */
static int mci_execute_tuning(struct mmc_host *mmc, u32 opcode)
{
	struct mci_host *host = mmc_priv(mmc);
	int err;

	mci_trace(MCI_TRACE_LEVEL_WAR, "begin");

	host->is_tuning = 1;
	err = mci_execute_mix_mode_tuning(mmc, opcode);
	mci_tuning_feedback(mmc);
	if (!err)
		err = mci_check_tuning(mmc, opcode);
	host->is_tuning = 0;
	return err;
}
#else
static void mci_tuning_out(struct mci_host *host, unsigned int found,
			     unsigned int raise_point, unsigned int fall_point)
{
	host->is_tuning = 0;
	if (!found) {
		mci_trace(MCI_TRACE_LEVEL_ERR, "%s: no valid phase shift! use default",
			    mmc_hostname(mmc));
		mci_writel(DEFAULT_PHASE, host->base + MCI_UHS_REG_EXT);
	} else {
		mci_trace(MCI_TRACE_LEVEL_WAR, "Tuning finished!!");
		if (fall_point < raise_point) {
			phase = (raise_point + fall_point) / 2;
			phase = phase - (MCI_PHASE_SCALE / 2);
			phase = (phase < 0) ? (MCI_PHASE_SCALE + phase) : phase;
		} else {
			phase = (raise_point + fall_point) / 2;
		}

		mci_set_sap_phase(host, phase);

		pr_info("tuning %s: valid phase shift [%d, %d] Final Phase %d\n",
			mmc_hostname(mmc), raise_point, fall_point, phase);
	}

	mci_writel(RW_THRESHOLD_SIZE, host->base + MCI_CARDTHRCTL);
}

static int mci_execute_tuning(struct mmc_host *mmc, u32 opcode)
{
	struct mci_host *host;
	/* found: identify if we have found a valid phase */
	unsigned int index, count, start_point, end_point, found;
	unsigned int err = 0;
	unsigned int prev_err = NOT_FOUND;
	unsigned int raise_point = NOT_FOUND;
	unsigned int fall_point = NOT_FOUND;
	int ret;

	start_point = TUNING_START_PHASE;
	end_point = TUNING_END_PHASE;

	host = mmc_priv(mmc);

	mci_writel(0x1, host->base + MCI_CARDTHRCTL);

	mci_trace(MCI_TRACE_LEVEL_WAR, "start sd3.0 phase tuning...");
	host->is_tuning = 1;
	for (index = start_point; index <= end_point; index++) {
		/* set sample clk phase shift */
		mci_set_sap_phase(host, index);

		count = 0;
		do {
			ret = mmc_send_tuning(mmc, opcode, NULL);
			mci_send_stop(mmc); /* send soft_stop tail */

			if (ret) {
				mci_trace(MCI_TRACE_LEVEL_WAR, "send tuning CMD%u fail! phase:%d err:%d\n",
					    opcode, index, ret);
				err = 1;
				break;
			}
			count++;
		} while (count < 1);

		if (!err)
			found = 1; /* found a valid phase */

		if (index > start_point) {
			if (err && !prev_err)
				fall_point = index - 1;

			if (!err && prev_err)
				raise_point = index;
		}

		if ((raise_point != NOT_FOUND) && (fall_point != NOT_FOUND))
			goto tuning_out;

		prev_err = err;
		err = 0;
	}

tuning_out:
	if (NOT_FOUND == raise_point)
		raise_point = start_point;
	if (NOT_FOUND == fall_point)
		fall_point = end_point;
	mci_tuning_out(host, found, raise_point, fall_point);
	return 0;
}
#endif

static void mci_set_bus_width(const struct mci_host *host, const struct mmc_ios *ios)
{
	unsigned int tmp_reg;
	/* set bus_width */
	mci_trace(MCI_TRACE_LEVEL_WAR, "ios->bus_width = %d ", ios->bus_width);
	tmp_reg = mci_readl(host->base + MCI_CTYPE);
	tmp_reg &= ~((CARD_WIDTH_0 | CARD_WIDTH_1) << host->port);

	if (ios->bus_width == MMC_BUS_WIDTH_8)
		tmp_reg |= (CARD_WIDTH_0 << host->port);
	else if (ios->bus_width == MMC_BUS_WIDTH_4)
		tmp_reg |= (CARD_WIDTH_1 << host->port);

	mci_writel(tmp_reg, host->base + MCI_CTYPE);
}

static void mci_set_iso_clock(struct mci_host *host, const struct mmc_ios *ios)
{
	u32 ctrl;
	if (ios->clock) {
		if (mci_control_cclk(host, DISABLE))
			return;
		mci_set_cclk(host, ios->clock);
		mci_control_cclk(host, ENABLE);

		mci_set_default_phase(host);

		/* speed mode check, if it is DDR50 set DDR mode */
		if (ios->timing == MMC_TIMING_UHS_DDR50) {
			ctrl = mci_readl(host->base + MCI_UHS_REG);
			if (!((SDXC_CTRL_DDR_REG << host->port) & ctrl)) {
				ctrl |= (SDXC_CTRL_DDR_REG << host->port);
				mci_writel(ctrl, host->base + MCI_UHS_REG);
			}
		}
	} else {
		if (mci_control_cclk(host, DISABLE))
			return;
		if (ios->timing != MMC_TIMING_UHS_DDR50) {
			ctrl = mci_readl(host->base + MCI_UHS_REG);
			if ((SDXC_CTRL_DDR_REG << host->port) & ctrl) {
				ctrl &= ~(SDXC_CTRL_DDR_REG << host->port);
				mci_writel(ctrl, host->base + MCI_UHS_REG);
			}
		}
	}
}

static void mci_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct mci_host *host = mmc_priv(mmc);

	mci_trace(MCI_TRACE_LEVEL_INFO, "begin");
	mci_assert(mmc);
	mci_assert(ios);
	mci_assert(host);

	mci_trace(MCI_TRACE_LEVEL_WAR, "ios->power_mode = %d ", ios->power_mode);
	if (!ios->clock)
		if (mci_control_cclk(host, DISABLE))
			return;

	switch (ios->power_mode) {
	case MMC_POWER_OFF:
		mci_ctrl_power(host, POWER_OFF, FORCE_DISABLE);
		break;
	case MMC_POWER_UP:
	case MMC_POWER_ON:
		mci_ctrl_power(host, POWER_ON, FORCE_DISABLE);
		break;
	default:
		break;
	}

	mci_trace(MCI_TRACE_LEVEL_WAR, "ios->clock = %d ", ios->clock);
	mci_set_iso_clock(host, ios);
	mci_set_drv_cap(host, 0);
	mci_set_bus_width(host, ios);
}

static void mci_enable_sdio_irq(struct mmc_host *mmc, int enable)
{
	struct mci_host *host = mmc_priv(mmc);
	unsigned int reg_value;
	unsigned long flags;

	spin_lock_irqsave(&host->lock, flags);
	reg_value = mci_readl(host->base + MCI_INTMASK);
	if (enable)
		reg_value |= SDIO_INT_MASK;
	else
		reg_value &= ~SDIO_INT_MASK;
	mci_writel(reg_value, host->base + MCI_INTMASK);
	spin_unlock_irqrestore(&host->lock, flags);
}

static int mci_get_card_detect(struct mmc_host *mmc)
{
	unsigned ret;
	struct mci_host *host = mmc_priv(mmc);

	mci_trace(MCI_TRACE_LEVEL_INFO, "begin");
	ret = mci_sys_card_detect(host);
	if (ret)
		return 0;
	else
		return 1;
}

static int mci_get_ro(struct mmc_host *mmc)
{
	unsigned ret;
	struct mci_host *host = mmc_priv(mmc);

	mci_trace(MCI_TRACE_LEVEL_INFO, "begin");
	mci_assert(mmc);

	ret = mci_ctrl_card_readonly(host);

	return ret;
}

static void mci_hw_reset(struct mmc_host *mmc)
{
	unsigned int reg_value;
	struct mci_host *host = mmc_priv(mmc);
	unsigned int port = host->port;

	reg_value = mci_readl(host->base + MCI_RESET_N);
	reg_value &= ~(MMC_RST_N << port);
	mci_writel(reg_value, host->base + MCI_RESET_N);

	/* For eMMC, minimum is 1us but give it 10us for good measure */
	udelay(10);
	reg_value = mci_readl(host->base + MCI_RESET_N);
	reg_value |= (MMC_RST_N << port);
	mci_writel(reg_value, host->base + MCI_RESET_N);

	/* For eMMC, minimum is 200us but give it 300us for good measure */
	usleep_range(300, 1000);
}

static int mci_card_busy(struct mmc_host *mmc)
{
	struct mci_host *host = mmc_priv(mmc);
	u32 regval;

	mci_trace(MCI_TRACE_LEVEL_INFO, "begin");

	regval = mci_readl(host->base + MCI_STATUS);
	regval &= DATA_BUSY;

	return regval;
}

static int mci_card_info_save(struct mmc_host *mmc)
{
	struct mmc_card *card = mmc->card;
	struct mci_host *host = mmc_priv(mmc);
	struct card_info *c_info = &host->c_info;
	int ret;

	if (!card) {
		(void)memset_s(c_info, sizeof(struct card_info),
			0, sizeof(struct card_info));
		c_info->card_connect = CARD_DISCONNECT;
		goto out;
	}

	c_info->card_type = card->type;
	c_info->card_state = card->state;

	c_info->timing = mmc->ios.timing;
	c_info->card_support_clock = mmc->ios.clock;

	c_info->sd_bus_speed = card->sd_bus_speed;

	ret = memcpy_s(c_info->ssr, sizeof(c_info->ssr), card->raw_ssr,
			sizeof(c_info->ssr));
	if (ret) {
		printk("%s:memcpy_s failed\n", __func__);
		return ret;
	}

	c_info->card_connect = CARD_CONNECT;
out:
	return 0;
}

static const struct mmc_host_ops mci_ops = {
	.request = mci_request,
	.set_ios = mci_set_ios,
	.get_ro = mci_get_ro,
	.card_busy = mci_card_busy,
	.start_signal_voltage_switch = mci_start_signal_voltage_switch,
	.execute_tuning	= mci_execute_tuning,
	.enable_sdio_irq = mci_enable_sdio_irq,
	.hw_reset = mci_hw_reset,
	.get_cd = mci_get_card_detect,
	.card_info_save = mci_card_info_save,
};

static irqreturn_t bspsd_irq(int irq, void *dev_id)
{
	struct mci_host *host = dev_id;
	u32 state, mstate;
	int handle = 0;

	spin_lock(&host->lock);
	state = mci_readl(host->base + MCI_RINTSTS);
	spin_unlock(&host->lock);

	/* bugfix: when send soft stop to SD Card, Host will report
	   sdio interrupt, This situation needs to be avoided */
	if (host->mmc->caps & MMC_CAP_SDIO_IRQ) {
		if ((host->mmc->card != NULL)
		    && (host->mmc->card->type == MMC_TYPE_SDIO)) {
			mstate = mci_readl(host->base + MCI_INTMASK);
			if ((state & SDIO_INT_STATUS) &&
			    (mstate & SDIO_INT_MASK)) {
				spin_lock(&host->lock);
				mci_writel(SDIO_INT_STATUS,
					     host->base + MCI_RINTSTS);
				spin_unlock(&host->lock);
				handle = 1;
				mmc_signal_sdio_irq(host->mmc);
			}
		}
	}

	if (state & DATA_INT_MASK) {
		handle = 1;
		host->pending_events |= MCI_PEND_DTO_M;

		spin_lock(&host->lock);
		host->irq_status = mci_readl(host->base + MCI_RINTSTS);
		mci_writel(DATA_INT_MASK, host->base + MCI_RINTSTS);
		spin_unlock(&host->lock);

		wake_up(&host->intr_wait);
	}

	if (handle)
		return IRQ_HANDLED;

	return IRQ_NONE;
}

static int mci_of_parse(const struct device_node *np, struct mmc_host *mmc)
{
	struct mci_host *host = mmc_priv(mmc);
	int ret = mmc_of_parse(mmc);
	int len;

	if (ret)
		return ret;

	mmc->caps |= MMC_CAP_ERASE;

	if (of_property_read_u32(np, "min-frequency", &mmc->f_min))
		mmc->f_min = MMC_CCLK_MIN;

	if (of_property_read_u32(np, "devid", &host->devid))
		return -EINVAL;

	if (of_find_property(np, "cap-mmc-hw-reset", &len))
		mmc->caps |= MMC_CAP_HW_RESET;

	if (host->devid == 0 || host->devid == 1)
		mmc->caps |= MMC_CAP_CMD23;
	return 0;
}

static int mci_mmc_host_init(struct mmc_host *mmc)
{
	u32 regval;
	void __iomem *misc_ctrl_0;
	mmc->ops = &mci_ops;
#if defined(CONFIG_ARCH_SS812V100) || defined(CONFIG_ARCH_SS813V100) || \
	defined(CONFIG_ARCH_SS312V100)  || defined(CONFIG_ARCH_SS313V100)  || \
	defined(CONFIG_ARCH_SS011V100)  || defined(CONFIG_ARCH_SS012V100)

	crg_ctrl = ioremap(0x12010000, 0x1000);
	if (!crg_ctrl) {
		printk("%s ioremap fail\n", __func__);
		return -ENOMEM;
	}

	misc_ctrl_0 = ioremap(REG_MISC_CTRL0, 0x4);
	if (misc_ctrl_0) {
		regval = readl(misc_ctrl_0);
		regval |= MISC_CTRL0_FIX_MASK;
		writel(regval, misc_ctrl_0);
		iounmap(misc_ctrl_0);
	}

	misc_ctrl_1 = ioremap(0x12030004, 0x4);
	if (!misc_ctrl_1) {
		printk("%s ioremap fail\n", __func__);
		return -ENOMEM;
	}

	regval = readl(misc_ctrl_1);
	/* clear sdio0_pswitch_ctrl_sel bit */
	regval &= ~(0x1 << 2);
	writel(regval, misc_ctrl_1);
	iounmap(misc_ctrl_1);
#endif
	return 0;
}

static void mci_release_resource(struct mci_host *host, struct mmc_host *mmc,
				   struct platform_device *pdev)
{
	if (host) {
		del_timer(&host->timer);

		if (host->base)
			devm_iounmap(&pdev->dev, host->base);

		if (host->dma_vaddr)
			dma_free_coherent(&pdev->dev, CMD_DES_PAGE_SIZE,
					  host->dma_vaddr, host->dma_paddr);
	}
	if (mmc)
		mmc_free_host(mmc);
#if defined(CONFIG_ARCH_SS812V100) || defined(CONFIG_ARCH_SS813V100) || \
	defined(CONFIG_ARCH_SS312V100)  || defined(CONFIG_ARCH_SS313V100)  || \
	defined(CONFIG_ARCH_SS011V100)  || defined(CONFIG_ARCH_SS012V100)
	if (crg_ctrl) {
		iounmap(crg_ctrl);
		crg_ctrl = NULL;
	}
#endif
}

static int mci_init_mmc_host(struct platform_device *pdev, struct mmc_host *mmc,
			       struct mci_host *host)
{
	int ret;
	/* reload by this controller */
#ifndef CONFIG_BSP_MC
	mmc->max_blk_count = 2048;
#else
	mmc->max_blk_count = 4096;
#endif
	mmc->max_segs = 1024;
	mmc->max_seg_size = mmc->max_blk_size * mmc->max_blk_count;
	mmc->max_req_size = mmc->max_blk_size * mmc->max_blk_count;
	mmc->ocr_avail = MMC_VDD_32_33 | MMC_VDD_33_34;

	mci_host[slot_index++] = host;
	pdev->id = host->devid;
	host->pdev = pdev;
	host->mmc = mmc;
	host->port = 0;
	host->dma_vaddr = dma_alloc_coherent(&pdev->dev, CMD_DES_PAGE_SIZE,
					     &host->dma_paddr, GFP_KERNEL);
	if (!host->dma_vaddr) {
		mci_error("no mem for mci dma!\n");
		return -ENOMEM;
	}

	spin_lock_init(&host->lock);

	host->crg_rst = devm_reset_control_get(&pdev->dev, "mmc_reset");
	if (IS_ERR_OR_NULL(host->crg_rst)) {
		mci_error("get rst fail.\n");
		ret = PTR_ERR(host->crg_rst);
		return ret;
	}

	host->power_status = POWER_OFF;

	timer_setup(&host->timer, mci_detect_card, 0);
	host->timer.expires = jiffies + detect_time;
	add_timer(&host->timer);
	return 0;
}

static int mci_init_mmc_host_dev(struct platform_device *pdev, struct mci_host *host)
{
	int ret, irq;
	reset_control_assert(host->crg_rst);
	usleep_range(50, 60);
	reset_control_deassert(host->crg_rst);

	host->clk = devm_clk_get(&pdev->dev, "mmc_clk");
	if (IS_ERR_OR_NULL(host->clk)) {
		mci_error("get clock fail.\n");
		ret = PTR_ERR(host->clk);
		return -ENOMEM;
	}

	clk_prepare_enable(host->clk);
	/* enable card */
	mci_init_host(host);
	host->card_status = mci_sys_card_detect(host);

	init_waitqueue_head(&host->intr_wait);
	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		pr_err("no IRQ defined!\n");
		return -ENOMEM;
	}

	host->irq = irq;
	ret = request_irq(irq, bspsd_irq, 0, DRIVER_NAME, host);
	if (ret) {
		pr_err("request_irq error!\n");
		return ret;
	}
	return 0;
}

static int mci_probe(struct platform_device *pdev)
{
	struct mmc_host *mmc = NULL;
	struct mci_host *host = NULL;
	struct resource *host_ioaddr_res = NULL;
	struct device_node *np = pdev->dev.of_node;

	mci_trace(MCI_TRACE_LEVEL_INFO, "begin");
	pr_info("mmc host probe\n");
	mci_assert(pdev);

	mmc = mmc_alloc_host(sizeof(struct mci_host), &pdev->dev);
	if (!mmc) {
		mci_error("no mem for mci host controller!\n");
		goto out;
	}

	platform_set_drvdata(pdev, mmc);
	host = mmc_priv(mmc);

	if (mci_mmc_host_init(mmc)) {
		mci_error("no ioaddr rescources config!\n");
		goto out;
	}

	host_ioaddr_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (NULL == host_ioaddr_res) {
		mci_error("no ioaddr rescources config!\n");
		goto out;
	}

	if (mci_of_parse(np, mmc)) {
		mci_error("failed to parse mmc dts!\n");
		goto out;
	}
	host->base = devm_ioremap_resource(&pdev->dev, host_ioaddr_res);
	if (IS_ERR_OR_NULL(host->base)) {
		mci_error("no mem for mci base!\n");
		return -ENOMEM;
	}

	if (mci_init_mmc_host(pdev, mmc, host))
		goto out;

	if (mci_init_mmc_host_dev(pdev, host))
		goto out;

	mmc_add_host(mmc);
	return 0;
out:
	mci_release_resource(host, mmc, pdev);
	return -ENOMEM;
}

static int __exit mci_remove(struct platform_device *pdev)
{
	struct mmc_host *mmc = platform_get_drvdata(pdev);

	mci_trace(MCI_TRACE_LEVEL_INFO, "begin");
	mci_assert(pdev);

	platform_set_drvdata(pdev, NULL);

	if (mmc) {
		struct mci_host *host = mmc_priv(mmc);

		mmc_remove_host(mmc);
		free_irq(host->irq, host);
		del_timer_sync(&host->timer);
		mci_ctrl_power(host, POWER_OFF, FORCE_DISABLE);
		mci_control_cclk(host, DISABLE);
		devm_iounmap(&pdev->dev, host->base);
		dma_free_coherent(&pdev->dev, CMD_DES_PAGE_SIZE, host->dma_vaddr,
				  host->dma_paddr);
		mmc_free_host(mmc);
	}
	return 0;
}

static void mci_shutdown(struct platform_device *pdev)
{
	struct mmc_host *mmc = platform_get_drvdata(pdev);

	mci_trace(MCI_TRACE_LEVEL_WAR, "shutdown");
	if (mmc) {
		unsigned int val;
		struct mci_host *host = mmc_priv(mmc);

		/* bugfix: host reset can trigger error intr */
		mci_writel(0, host->base + MCI_IDINTEN);
		mci_writel(0, host->base + MCI_INTMASK);

		val = mci_readl(host->base + MCI_CTRL);
		val |= CTRL_RESET | FIFO_RESET | DMA_RESET;
		mci_writel(val, host->base + MCI_CTRL);
	}
}

#ifdef CONFIG_PM
static int mci_pltm_suspend(struct platform_device *pdev,
			      pm_message_t state)
{
	struct mmc_host *mmc = platform_get_drvdata(pdev);
	struct mci_host *host = NULL;
	int ret = 0;

	if (mmc) {
		host = mmc_priv(mmc);
		del_timer_sync(&host->timer);

		if (__clk_is_enabled(host->clk))
			clk_disable_unprepare(host->clk);
	}

	return ret;
}

static int mci_pltm_resume(struct platform_device *pdev)
{
	struct mmc_host *mmc = platform_get_drvdata(pdev);
	struct mci_host *host = NULL;
	int ret = 0;
	if (mmc) {
		host = mmc_priv(mmc);
		if (!__clk_is_enabled(host->clk))
			clk_prepare_enable(host->clk);
		mci_sys_ctrl_init(host);
		mci_init_host(host);
		add_timer(&host->timer);
	}
	return ret;
}
#else
void mci_pltm_suspend(int slot)
{
}
void mci_pltm_resume(int slot)
{
}
#endif

void bsp_sdio_rescan(int slot)
{
	struct mmc_host *mmc = NULL;
	struct mci_host *host;

	if (slot >= MCI_SLOT_NUM) {
		mci_trace(MCI_TRACE_LEVEL_ERR, "mmc%d: invalid slot!\n", slot);
		return;
	}

	host = mci_host[slot];
	if (!host || !host->mmc) {
		mci_trace(MCI_TRACE_LEVEL_ERR, "mmc%d: invalid slot!\n", slot);
		return;
	}

	mmc = host->mmc;
	del_timer_sync(&host->timer);

	mmc_remove_host(mmc);

	mmc_add_host(mmc);

	add_timer(&host->timer);
}
EXPORT_SYMBOL(bsp_sdio_rescan);

static const struct of_device_id
	mci_match[] __maybe_unused = {
	{.compatible = "vendor,ss812v100-mci"},
	{.compatible = "vendor,ss813v100-mci"},
	{.compatible = "vendor,ss312v100-mci"},
	{.compatible = "vendor,ss313v100-mci"},
	{},
};

static struct platform_driver mci_driver = {
	.probe = mci_probe,
	.remove = mci_remove,
	.shutdown = mci_shutdown,
	.suspend = mci_pltm_suspend,
	.resume = mci_pltm_resume,
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(mci_match),
	},
};

static int __init mci_init(void)
{
	int ret;

	mci_trace(MCI_TRACE_LEVEL_INFO, "begin");

	/*
	 * We should register SDIO1 first to make sure that
	 * the eMMC device,which connected to SDIO1 is mmcblk0.
	 */

	ret = platform_driver_register(&mci_driver);
	if (ret) {
		platform_driver_unregister(&mci_driver);
		mci_error("driver register failed!");
		return ret;
	}

	/* device proc entry */
	ret = mci_proc_init();
	if (ret)
		mci_error("device proc init is failed!");

	return ret;
}

static void __exit mci_exit(void)
{
	mci_trace(MCI_TRACE_LEVEL_INFO, "begin");

	mci_proc_shutdown();

	platform_driver_unregister(&mci_driver);
}

module_init(mci_init);
module_exit(mci_exit);

#ifdef MODULE
MODULE_DESCRIPTION("MMC/SD driver for the MMC/SD Host Controller");
MODULE_LICENSE("GPL");
#endif
