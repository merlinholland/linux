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
#ifndef _MCI_H_
#define _MCI_H_

extern unsigned int slot_index;
extern int trace_level;

#define MCI_TRACE_LEVEL_INFO 2
#define MCI_TRACE_LEVEL_WAR 3
#define MCI_TRACE_LEVEL_REG 4
#define MCI_TRACE_LEVEL_ERR 5

#define MCI_TRACE_LEVEL 5
/*
   0 - all message
   1 - dump all register read/write
   2 - flow trace
   3 - timeout err and protocol err
   */

#define MCI_TRACE_FMT KERN_INFO

#define NOT_FOUND (-1)
#define POWER_ON 1
#define POWER_OFF 0
#define FORCE_ENABLE 1
#define FORCE_DISABLE 0

#define CARD_UNPLUGED 1
#define CARD_PLUGED 0

#define ENABLE 1
#define DISABLE 0

#define MCI_DETECT_TIMEOUT (HZ / 2)

#define MCI_REQUEST_TIMEOUT (30 * HZ)

#define MAX_RETRY_COUNT 100

#define MMC_CCLK_MIN  100000

/* Base address of SD card register */
#define MCI_INTR (49 + 32)

#define mci_trace(level, msg...) do { \
	if ((level) >= trace_level) { \
		printk(MCI_TRACE_FMT "%s:%d: ", __func__, __LINE__); \
		printk(msg); \
		printk("\n"); \
	} \
} while (0)

#define mci_assert(cond) do { \
	if (!(cond)) { \
		printk(KERN_ERR "Assert:mci:%s:%d\n", \
				__func__, \
				__LINE__); \
		BUG(); \
	} \
} while (0)

#define mci_error(s...) do { \
	printk(KERN_ERR "mci:%s:%d: ", __func__, __LINE__); \
	printk(s); \
	printk("\n"); \
} while (0)

#define mci_readl(addr) ({unsigned int reg = readl(IOMEM((uintptr_t)(addr))); \
	mci_trace(1, "readl(0x%04X) = 0x%08X", (unsigned int)(uintptr_t)(addr), reg); \
	reg; })

#define mci_writel(v, addr) do { writel((v), IOMEM((uintptr_t)(addr))); \
	mci_trace(1, "writel(0x%04X) = 0x%08X", (unsigned int)(uintptr_t)(addr), \
			(unsigned int)(uintptr_t)(v));} while (0)

struct mci_des {
	unsigned long idmac_des_ctrl;
	unsigned long idmac_des_buf_size;
	unsigned long idmac_des_buf_addr;
	unsigned long idmac_des_next_addr;
};

struct card_info {
	unsigned int     card_type;
	unsigned char    timing;
	unsigned char    card_connect;
#define CARD_CONNECT    1
#define CARD_DISCONNECT 0
	unsigned int     card_support_clock; /* clock rate */
	unsigned int     card_state;      /* (our) card state */
	unsigned int     sd_bus_speed;
	unsigned int     ssr[16];
};

struct mci_host {
	struct mmc_host *mmc;
	struct platform_device *pdev;
	spinlock_t lock;
	struct mmc_request *mrq;
	struct mmc_command *cmd;
	struct mmc_data *data;
	void __iomem *base;
	struct scatterlist *dma_sg;
	unsigned int dma_sg_num;
	unsigned int dma_dir;
	dma_addr_t dma_paddr;
	unsigned int *dma_vaddr;
	struct timer_list timer;
	unsigned int irq;
	unsigned int irq_status;
	unsigned int is_tuning;
	wait_queue_head_t intr_wait;
#define MCI_PEND_DTO_B (0)
#define MCI_PEND_DTO_M (1 << MCI_PEND_DTO_B)
	unsigned long pending_events;
	unsigned int power_status;
	unsigned int card_rca;
	unsigned int card_status;
	unsigned int devid;
	unsigned int hclk;
	unsigned int cclk;
	struct clk *clk;
	struct reset_control *crg_rst;
	unsigned int port;
	unsigned int error_count;
	unsigned int data_error_count;
	struct card_info c_info;
};

union cmd_arg_u {
	unsigned int cmd_arg;
	struct cmd_bits_arg {
		unsigned int cmd_index : 6;
		unsigned int response_expect : 1;
		unsigned int response_length : 1;
		unsigned int check_response_crc : 1;
		unsigned int data_transfer_expected : 1;
		unsigned int read_write : 1;
		unsigned int transfer_mode : 1;
		unsigned int send_auto_stop : 1;
		unsigned int wait_prvdata_complete : 1;
		unsigned int stop_abort_cmd : 1;
		unsigned int send_initialization : 1;
		unsigned int card_number : 5;
		unsigned int update_clk_reg_only : 1; /* bit 21 */
		unsigned int read_ceata_device : 1;
		unsigned int ccs_expected : 1;
		unsigned int enable_boot : 1;
		unsigned int expect_boot_ack : 1;
		unsigned int disable_boot : 1;
		unsigned int boot_mode : 1;
		unsigned int volt_switch : 1;
		unsigned int use_hold_reg : 1;
		unsigned int reserved : 1;
		unsigned int start_cmd : 1; /* HSB */
	} bits;
};

struct mmc_host *get_mmchost(int hostid);
#endif
