/*
 *
 * Copyright (c) 2020-2021 Shenshu Technologies Co., Ltd.
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

#include <linux/vendor/peri_io.h>
#include "sdhci-bsp.h"
#include "mci_proc.h"

#define GET_EMMC_BUS_WIDTH_FORM_DTS

#define REG_BASE_EMMC_PHY           0x10010000
#define EMMC_PHY_INITCTRL           (REG_BASE_EMMC_PHY + 0x4)
#define  EMMC_INIT_EN               0x1
#define  EMMC_DLYMEAS_EN            (0x1 << 2)
#define  EMMC_ZCAL_EN               (0x1 << 3)
#define INITCTRL_CHECK_TIMES        100

#define PERI_CRG_MMC_DRV_DLL        0x34c8
#define PERI_CRG_SDIO0_DRV_DLL      0x35c8
#define PERI_CRG_SDIO1_DRV_DLL      0x36c8
#define  CRG_DRV_PHASE_SEL_SHIFT    15
#define  CRG_DRV_PHASE_SEL_MASK     (0x1F << 15)

#define PERI_CRG_MMC_STAT     0x34d8
#define  CRG_SAM_DLL_READY    BIT(12)
#define  CRG_DS_DLL_READY     BIT(10)
#define  CRG_P4_DLL_LOCKED    BIT(9)

/* MMC IO */
#define REG_MMC_CLK_IO    0x00
#define REG_MMC_CMD_IO    0x04
#define REG_MMC_D0_IO     0x08
#define REG_MMC_D1_IO     0x0c
#define REG_MMC_D2_IO     0x10
#define REG_MMC_D3_IO     0x14
#define REG_MMC_D4_IO     0x18
#define REG_MMC_D5_IO     0x1c
#define REG_MMC_D6_IO     0x20
#define REG_MMC_D7_IO     0x24
#define REG_MMC_DQS_IO    0x28
#define REG_MMC_RST_IO   0x2c

/* SDIO0 IO */

#define REG_SDIO0_DETECT_IO  0x80
#define REG_SDIO0_PWEN_N  0x84
#define REG_SDIO0_CMD_IO  0x88
#define REG_SDIO0_D0_IO   0x8c
#define REG_SDIO0_D1_IO   0x90
#define REG_SDIO0_D2_IO   0x94
#define REG_SDIO0_D3_IO   0x98
#define REG_SDIO0_CLK_IO  0x9c

/* SDIO1 IO */
#define REG_SDIO1_D0_IO   0x40
#define REG_SDIO1_D1_IO   0x44
#define REG_SDIO1_D2_IO   0x48
#define REG_SDIO1_D3_IO   0x4c
#define REG_SDIO1_CLK_IO  0x50
#define REG_SDIO1_CMD_IO  0x54

/* IO CFG */
#define IO_CFG_DRV_STR_MASK      (0xf << 4) /* eMMC or SDIO */
#define io_cfg_drv_str_sel(str)  ((str) << 4)

#define IO_CFG_PULL_UPDW_MASK    (0x03 << 8) /* eMMC or SDIO */
#define IO_CFG_EMMC_PULL_UP      BIT(9) /* eMMC */
#define IO_CFG_EMMC_PULL_UPDW_EN BIT(8) /* eMMC */
#define IO_CFG_PULL_DOWN         BIT(9) /* SDIO */
#define IO_CFG_PULL_UP           BIT(8) /* SDIO */
#define IO_CFG_SDIO_MASK         (IO_CFG_DRV_STR_MASK | IO_CFG_PULL_UPDW_MASK)

#define IO_CFG_SDIO_MUX      0x1
#define IO_CFG_EMMC_MUX      0x2
#define IO_CFG_MUX_MASK      0xF
#define MMC_BUS_WIDTH_8_BIT  8
#define MMC_BUS_WIDTH_4_BIT  4

#define REG_SYSSTAT         0x11020018
#define  BOOT_MEDIA_EMMC    0xc
#define  EMMC_BOOT_8BIT     BIT(11)
#define  BOOT_FLAG_MASK     (0x3 << 2)

#define REG_MISC_PWR_SWITCH		0x102E0010
#define SDIO0_PWRSW_SEL_1V8		BIT(5)
#define SDIO0_PWR_EN			BIT(4)
#define SDIO0_IO_MODE_SEL_1V8	BIT(1)
#define SDIO0_PWR_CTRL_BY_MISC	BIT(0)

#define IO_CLK              0
#define IO_CMD              1
#define IO_DATA             2
#define IO_RST              3
#define IO_DS               4
#define EMMC_IO_TYPE_NUM    5
#define SDIO_IO_TYPE_NUM    3

/* sample drive phase */
#define DRIVE             0
#define SAMPLE            1
#define PHASE_TYPE_NUM    2

static unsigned int reg_mmc_data_io[] = {
	REG_MMC_D0_IO, REG_MMC_D1_IO,
	REG_MMC_D2_IO, REG_MMC_D3_IO,
	REG_MMC_D4_IO, REG_MMC_D5_IO,
	REG_MMC_D6_IO, REG_MMC_D7_IO
};

static unsigned int reg_sdio0_data_io[] = {
	REG_SDIO0_D0_IO, REG_SDIO0_D1_IO,
	REG_SDIO0_D2_IO, REG_SDIO0_D3_IO,
};

static unsigned int reg_sdio1_data_io[] = {
	REG_SDIO1_D0_IO, REG_SDIO1_D1_IO,
	REG_SDIO1_D2_IO, REG_SDIO1_D3_IO,
};

/* drive capabilities */
static u32 mmc_io_cfg[][EMMC_IO_TYPE_NUM] = { /* CLK CMD DATA RST DQS */
	[MMC_TIMING_LEGACY] = {
		io_cfg_drv_str_sel(0x4) | IO_CFG_EMMC_PULL_UPDW_EN, /* 0x4 is 0b100 */
		io_cfg_drv_str_sel(0x2) | IO_CFG_EMMC_PULL_UPDW_EN | /* 0x2 is 0b010 */
			IO_CFG_EMMC_PULL_UP,
		io_cfg_drv_str_sel(0x2) | IO_CFG_EMMC_PULL_UPDW_EN | /* 0x2 is 0b010 */
			IO_CFG_EMMC_PULL_UP,
		io_cfg_drv_str_sel(0x0) | IO_CFG_PULL_UP
	},
	[MMC_TIMING_MMC_HS] = {
		io_cfg_drv_str_sel(0x4) | IO_CFG_EMMC_PULL_UPDW_EN, /* 0x4 is 0b100 */
		io_cfg_drv_str_sel(0x2) | IO_CFG_EMMC_PULL_UPDW_EN | /* 0x2 is 0b010 */
			IO_CFG_EMMC_PULL_UP,
		io_cfg_drv_str_sel(0x2) | IO_CFG_EMMC_PULL_UPDW_EN | /* 0x2 is 0b010 */
			IO_CFG_EMMC_PULL_UP,
		io_cfg_drv_str_sel(0x0) | IO_CFG_PULL_UP
	},
	[MMC_TIMING_MMC_HS200] = {
		io_cfg_drv_str_sel(0x4) | IO_CFG_EMMC_PULL_UPDW_EN, /* 0x4 is 0b100 */
		io_cfg_drv_str_sel(0x5) | IO_CFG_EMMC_PULL_UPDW_EN | /* 0x5 is 0b101 */
			IO_CFG_EMMC_PULL_UP,
		io_cfg_drv_str_sel(0x5) | IO_CFG_EMMC_PULL_UPDW_EN | /* 0x5 is 0b101 */
			IO_CFG_EMMC_PULL_UP,
		io_cfg_drv_str_sel(0x0) | IO_CFG_PULL_UP
	},
	[MMC_TIMING_MMC_HS400] = {
		io_cfg_drv_str_sel(0x4) | IO_CFG_EMMC_PULL_UPDW_EN, /* 0x4 is 0b100 */
		io_cfg_drv_str_sel(0x5) | IO_CFG_EMMC_PULL_UPDW_EN | /* 0x5 is 0b101 */
			IO_CFG_EMMC_PULL_UP,
		io_cfg_drv_str_sel(0x5) | IO_CFG_EMMC_PULL_UPDW_EN | /* 0x5 is 0b101 */
			IO_CFG_EMMC_PULL_UP,
		io_cfg_drv_str_sel(0x0) | IO_CFG_PULL_UP,
		io_cfg_drv_str_sel(0x4) | IO_CFG_EMMC_PULL_UPDW_EN   /* 0x4 is 0b100 */
	}
};

static u32 sdio0_io_cfg[][SDIO_IO_TYPE_NUM] = { /* CLK CMD DATA */
	[MMC_TIMING_LEGACY] = {
		io_cfg_drv_str_sel(0x9) | IO_CFG_PULL_DOWN, /* 0x9 is 0b1001 */
		io_cfg_drv_str_sel(0x3) | IO_CFG_PULL_UP,   /* 0x3 is 0b0011 */
		io_cfg_drv_str_sel(0x3) | IO_CFG_PULL_UP    /* 0x3 is 0b0011 */
	},
	[MMC_TIMING_SD_HS] = {
		io_cfg_drv_str_sel(0xa) | IO_CFG_PULL_DOWN, /* 0xa is 0b1010 */
		io_cfg_drv_str_sel(0x4) | IO_CFG_PULL_UP,   /* 0x4 is 0b0100 */
		io_cfg_drv_str_sel(0x4) | IO_CFG_PULL_UP    /* 0x4 is 0b0100 */
	},
	[MMC_TIMING_UHS_SDR12] = {
		io_cfg_drv_str_sel(0x5) | IO_CFG_PULL_DOWN, /* 0x5 is 0b0101 */
		io_cfg_drv_str_sel(0x0) | IO_CFG_PULL_UP,
		io_cfg_drv_str_sel(0x0) | IO_CFG_PULL_UP
	},
	[MMC_TIMING_UHS_SDR25] = {
		io_cfg_drv_str_sel(0x7) | IO_CFG_PULL_DOWN, /* 0x7 is 0b0111 */
		io_cfg_drv_str_sel(0x1) | IO_CFG_PULL_UP,   /* 0x1 is 0b0001 */
		io_cfg_drv_str_sel(0x1) | IO_CFG_PULL_UP    /* 0x1 is 0b0001 */
	},
	[MMC_TIMING_UHS_SDR50] = {
		io_cfg_drv_str_sel(0x7) | IO_CFG_PULL_DOWN, /* 0x7 is 0b0111 */
		io_cfg_drv_str_sel(0x3) | IO_CFG_PULL_UP,   /* 0x3 is 0b0011 */
		io_cfg_drv_str_sel(0x3) | IO_CFG_PULL_UP    /* 0x3 is 0b0011 */
	},
	[MMC_TIMING_UHS_SDR104] = {
		io_cfg_drv_str_sel(0xd) | IO_CFG_PULL_DOWN, /* 0xd is 0b1101 */
		io_cfg_drv_str_sel(0x7) | IO_CFG_PULL_UP,   /* 0x7 is 0b0111 */
		io_cfg_drv_str_sel(0x7) | IO_CFG_PULL_UP    /* 0x7 is 0b0111 */
	}
};

static u32 sdio1_io_cfg[][SDIO_IO_TYPE_NUM] = { /* CLK CMD DATA */
	[MMC_TIMING_LEGACY] = {
		io_cfg_drv_str_sel(0x8) | IO_CFG_PULL_DOWN, /* 0x8 is 0b1000 */
		io_cfg_drv_str_sel(0x2) | IO_CFG_PULL_UP,   /* 0x2 is 0b0010 */
		io_cfg_drv_str_sel(0x2) | IO_CFG_PULL_UP    /* 0x2 is 0b0010 */
	},
	[MMC_TIMING_MMC_HS] = {
		io_cfg_drv_str_sel(0x9) | IO_CFG_PULL_DOWN, /* 0x9 is 0b1001 */
		io_cfg_drv_str_sel(0x3) | IO_CFG_PULL_UP,   /* 0x3 is 0b0011 */
		io_cfg_drv_str_sel(0x3) | IO_CFG_PULL_UP    /* 0x3 is 0b0011 */
	},
	[MMC_TIMING_SD_HS] = {
		io_cfg_drv_str_sel(0x4) | IO_CFG_PULL_DOWN, /* 0x4 is 0b0100 */
		io_cfg_drv_str_sel(0x3) | IO_CFG_PULL_UP,   /* 0x3 is 0b0011 */
		io_cfg_drv_str_sel(0x3) | IO_CFG_PULL_UP    /* 0x3 is 0b0011 */
	},
	[MMC_TIMING_UHS_SDR12] = {
		io_cfg_drv_str_sel(0x4) | IO_CFG_PULL_DOWN, /* 0x4 is 0b0100 */
		io_cfg_drv_str_sel(0x1) | IO_CFG_PULL_UP,   /* 0x4 is 0b0001 */
		io_cfg_drv_str_sel(0x1) | IO_CFG_PULL_UP    /* 0x4 is 0b0001 */
	},
	[MMC_TIMING_UHS_SDR25] = {
		io_cfg_drv_str_sel(0x6) | IO_CFG_PULL_DOWN, /* 0x6 is 0b0110 */
		io_cfg_drv_str_sel(0x2) | IO_CFG_PULL_UP,   /* 0x2 is 0b0010 */
		io_cfg_drv_str_sel(0x2) | IO_CFG_PULL_UP    /* 0x2 is 0b0010 */
	},
	[MMC_TIMING_UHS_SDR50] = {
		io_cfg_drv_str_sel(0x6) | IO_CFG_PULL_DOWN, /* 0x6 is 0b0110 */
		io_cfg_drv_str_sel(0x2) | IO_CFG_PULL_UP,   /* 0x2 is 0b0010 */
		io_cfg_drv_str_sel(0x2) | IO_CFG_PULL_UP    /* 0x2 is 0b0010 */
	},
	[MMC_TIMING_UHS_SDR104] = {
		io_cfg_drv_str_sel(0xb) | IO_CFG_PULL_DOWN, /* 0xb is 0b1011 */
		io_cfg_drv_str_sel(0x6) | IO_CFG_PULL_UP,   /* 0x6 is 0b0110 */
		io_cfg_drv_str_sel(0x6) | IO_CFG_PULL_UP    /* 0x6 is 0b0110 */
	},
	[MMC_TIMING_MMC_HS200] = {
		io_cfg_drv_str_sel(0xb) | IO_CFG_PULL_DOWN, /* 0xb is 0b1011 */
		io_cfg_drv_str_sel(0x6) | IO_CFG_PULL_UP,   /* 0x6 is 0b0110 */
		io_cfg_drv_str_sel(0x6) | IO_CFG_PULL_UP,   /* 0x6 is 0b0110 */
	}
};

static u32 mmc_phase_cfg[][PHASE_TYPE_NUM] = {  /* drive,sample phase */
	[MMC_TIMING_LEGACY]     = { 16, 0 }, /* 16 for 180 degree */
	[MMC_TIMING_MMC_HS]     = { 16, 4 }, /* 16 for 180 degree,4 for 45 degree */
	[MMC_TIMING_MMC_HS200]  = { 18, 0 }, /* 18 for 202.5 degree */
	[MMC_TIMING_MMC_HS400]  = {  7, 0 }  /* 7 for 78.75 degree */
};

static u32 sdio0_phase_cfg[][PHASE_TYPE_NUM] = {  /* drive , sample phase */
	[MMC_TIMING_LEGACY]     = { 16, 0 }, /* 16 for 180 degree */
	[MMC_TIMING_SD_HS]      = { 18, 4 }, /* 18 for 202.5 degree */
	[MMC_TIMING_UHS_SDR12]  = { 16, 0 }, /* 16 for 180 degree */
	[MMC_TIMING_UHS_SDR25]  = { 16, 4 }, /* 16 for 180 degree,4 for 45 degree */
	[MMC_TIMING_UHS_SDR50]  = { 20, 0 }, /* 20 for 225 degree */
	[MMC_TIMING_UHS_SDR104] = { 20, 0 }, /* 20 for 225 degree */
};

static u32 sdio1_phase_cfg[][PHASE_TYPE_NUM] = {  /* drive , sample phase */
	[MMC_TIMING_LEGACY]     = { 16, 0 }, /* 16 for 180 degree */
	[MMC_TIMING_MMC_HS]     = { 16, 4 }, /* 16 for 180 degree,4 for 45 degree */
	[MMC_TIMING_SD_HS]      = { 16, 4 }, /* 16 for 180 degree,4 for 45 degree */
	[MMC_TIMING_UHS_SDR12]  = { 16, 0 }, /* 16 for 180 degree */
	[MMC_TIMING_UHS_SDR25]  = { 16, 4 }, /* 16 for 180 degree,4 for 45 degree */
	[MMC_TIMING_UHS_SDR50]  = { 20, 0 }, /* 20 for 225 degree */
	[MMC_TIMING_UHS_SDR104] = { 20, 0 }, /* 20 for 225 degree */
	[MMC_TIMING_MMC_HS200]  = { 20, 0 }, /* 20 for 225 degree */
};

/* Do ZQ resistance calibration for eMMC PHY IO */
static int resistance_calibration(void)
{
	int i;
	u32 reg_val;
	void __iomem *viraddr;

	viraddr = ioremap_nocache(EMMC_PHY_INITCTRL, sizeof(u32));
	if (!viraddr) {
		pr_err("resistance_calibration ioremap error.\n");
		return -ENOMEM;
	}
	reg_val = readl(viraddr);
	reg_val |= EMMC_INIT_EN | EMMC_ZCAL_EN;
	writel(reg_val, viraddr);

	for (i = 0; i < INITCTRL_CHECK_TIMES; i++) {
		reg_val = readl(viraddr);
		if ((reg_val & (EMMC_INIT_EN | EMMC_ZCAL_EN)) == 0) {
			iounmap(viraddr);
			return 0;
		}
		udelay(10); /* delay 10 us */
	}

	iounmap(viraddr);
	return -ETIMEDOUT;
}

void sdhci_bsp_extra_init(struct sdhci_host *host)
{
	struct sdhci_bsp_priv *priv = sdhci_get_pltfm_priv(host);
	u32 ctrl;

	ctrl = sdhci_readl(host, SDHCI_AXI_MBIIU_CTRL);
	ctrl &= ~SDHCI_UNDEFL_INCR_EN;
	sdhci_writel(host, ctrl, SDHCI_AXI_MBIIU_CTRL);

	/* eMMC device */
	if (priv->devid == 0) {
		ctrl = sdhci_readl(host, SDHCI_EMMC_CTRL);
		ctrl |= SDHCI_CARD_IS_EMMC;
		sdhci_writel(host, ctrl, SDHCI_EMMC_CTRL);
	}

	host->error_count = 0;
}

static void set_drv_str(struct sdhci_host *host,
		unsigned int offset, unsigned int drv_str)
{
	struct sdhci_bsp_priv *priv = sdhci_get_pltfm_priv(host);

	regmap_write_bits(priv->iocfg_regmap, offset, IO_CFG_SDIO_MASK, drv_str);
}

void bsp_set_drv_cap(struct sdhci_host *host)
{
	struct sdhci_bsp_priv *priv = sdhci_get_pltfm_priv(host);
	unsigned int i;

	if (priv->devid == 0) { /* emmc devices */
		set_drv_str(host, REG_MMC_CLK_IO,
			mmc_io_cfg[host->timing][IO_CLK]);
		set_drv_str(host, REG_MMC_CMD_IO,
			mmc_io_cfg[host->timing][IO_CMD]);

		for (i = 0; i < priv->bus_width; i++)
			set_drv_str(host, reg_mmc_data_io[i],
				mmc_io_cfg[host->timing][IO_DATA]);

		set_drv_str(host, REG_MMC_RST_IO,
			mmc_io_cfg[host->timing][IO_RST]);

		if (host->timing == MMC_TIMING_MMC_HS400)
			set_drv_str(host, REG_MMC_DQS_IO,
				mmc_io_cfg[host->timing][IO_DS]);
	} else if (priv->devid == 1) { /* sdio0 devices */
		set_drv_str(host, REG_SDIO0_CLK_IO,
			sdio0_io_cfg[host->timing][IO_CLK]);
		set_drv_str(host, REG_SDIO0_CMD_IO,
			sdio0_io_cfg[host->timing][IO_CMD]);
		for (i = 0; i < priv->bus_width; i++)
			set_drv_str(host, reg_sdio0_data_io[i],
				sdio0_io_cfg[host->timing][IO_DATA]);
	} else { /* sdio1 devices */
		set_drv_str(host, REG_SDIO1_CLK_IO,
			sdio1_io_cfg[host->timing][IO_CLK]);
		set_drv_str(host, REG_SDIO1_CMD_IO,
			sdio1_io_cfg[host->timing][IO_CMD]);
		for (i = 0; i < priv->bus_width; i++)
			set_drv_str(host, reg_sdio1_data_io[i],
				sdio1_io_cfg[host->timing][IO_DATA]);
	}
}

void bsp_wait_sample_dll_ready(struct sdhci_host *host)
{
	unsigned int reg;
	unsigned int timeout = 20;
	struct sdhci_bsp_priv *priv = sdhci_get_pltfm_priv(host);

	do {
		regmap_read(priv->crg_regmap, PERI_CRG_MMC_STAT, &reg);
		if (reg & CRG_SAM_DLL_READY)
			return;

		udelay(1000); /* delay 1000us */
		timeout--;
	} while (timeout > 0);

	pr_err("%s: SAMPL DLL slave not ready.\n", mmc_hostname(host->mmc));
}

void bsp_wait_p4_dll_lock(struct sdhci_host *host)
{
	unsigned int reg;
	unsigned int timeout = 20;
	struct sdhci_bsp_priv *priv = sdhci_get_pltfm_priv(host);

	do {
		regmap_read(priv->crg_regmap, PERI_CRG_MMC_STAT, &reg);
		if (reg & CRG_P4_DLL_LOCKED)
			return;

		udelay(1000); /* delay 1000us */
		timeout--;
	} while (timeout > 0);

	pr_err("%s: P4 DLL master not locked.\n", mmc_hostname(host->mmc));
}

void bsp_wait_ds_dll_ready(struct sdhci_host *host)
{
	unsigned int reg;
	unsigned int timeout = 20;
	struct sdhci_bsp_priv *priv = sdhci_get_pltfm_priv(host);

	do {
		regmap_read(priv->crg_regmap, PERI_CRG_MMC_STAT, &reg);
		if (reg & CRG_DS_DLL_READY)
			return;

		udelay(1000); /* delay 1000us */
		timeout--;
	} while (timeout > 0);

	pr_err("%s: DS DLL slave not ready.\n", mmc_hostname(host->mmc));
}

static int set_signal_voltage_1v8(struct sdhci_host *host)
{
	struct sdhci_bsp_priv *priv = sdhci_get_pltfm_priv(host);
	unsigned int ctrl;
	void __iomem *pw_reg;

	pr_debug("%s: set voltage to 180\n", mmc_hostname(host->mmc));

	if (priv->devid == 0 || priv->devid == 2) /* device id 2 for sdio1 */
		return 0;

	if (priv->devid == 1) {
		pw_reg = ioremap_nocache(REG_MISC_PWR_SWITCH, 4); /* 4 bytes */
		if (pw_reg == NULL)
			return -ENOMEM;
		ctrl = readl(pw_reg);
		ctrl |= SDIO0_PWRSW_SEL_1V8;
		writel(ctrl, pw_reg);

		usleep_range(1000, 2000); /* Sleep between 1000 and 2000us */

		ctrl |= SDIO0_IO_MODE_SEL_1V8;
		writel(ctrl, pw_reg);

		ctrl = readl(pw_reg);
		iounmap(pw_reg);

		if ((ctrl & SDIO0_PWRSW_SEL_1V8) && (ctrl & SDIO0_IO_MODE_SEL_1V8))
			return 0;
	}

	pr_warn("%s: 1.8V output did not became stable\n",
		mmc_hostname(host->mmc));

	return -EAGAIN;
}

static int set_signal_voltage_3v3(struct sdhci_host *host)
{
	struct sdhci_bsp_priv *priv = sdhci_get_pltfm_priv(host);
	unsigned int ctrl;
	void __iomem *pw_reg;

	/* sdio1: it is fixed to 1v8, so we fake that 3v3 is ok */
	if (priv->devid == 2) /* device id 2 for sdio1 */
		return 0;

	pr_debug("%s: set voltage to 330\n", mmc_hostname(host->mmc));

	if (priv->devid == 1) {
		pw_reg = ioremap_nocache(REG_MISC_PWR_SWITCH, 4); /* 4 bytes */
		if (pw_reg == NULL)
			return -ENOMEM;
		ctrl = readl(pw_reg);
		ctrl |= SDIO0_PWR_CTRL_BY_MISC | SDIO0_PWR_EN;
		ctrl &= ~SDIO0_IO_MODE_SEL_1V8;
		writel(ctrl, pw_reg);

		usleep_range(1000, 2000); /* Sleep between 1000 and 2000us */

		ctrl &= ~SDIO0_PWRSW_SEL_1V8;
		writel(ctrl, pw_reg);

		ctrl = readl(pw_reg);
		iounmap(pw_reg);

		if ((ctrl & SDIO0_PWR_CTRL_BY_MISC)
				&& (ctrl & SDIO0_PWR_EN)
				&& !(ctrl & SDIO0_IO_MODE_SEL_1V8) &&
				!(ctrl & SDIO0_PWRSW_SEL_1V8))
			return 0;
	}

	pr_warn("%s: 3.3V output did not became stable\n",
		mmc_hostname(host->mmc));

	return -EAGAIN;
}

int sdhci_bsp_start_signal_voltage_switch(struct sdhci_host *host,
	struct mmc_ios *ios)
{
	switch (ios->signal_voltage) {
	case MMC_SIGNAL_VOLTAGE_330:
		if (!(host->flags & SDHCI_SIGNALING_330))
			return -EINVAL;
		return set_signal_voltage_3v3(host);
	case MMC_SIGNAL_VOLTAGE_180:
		if (!(host->flags & SDHCI_SIGNALING_180))
			return -EINVAL;
		return set_signal_voltage_1v8(host);
	default:
		/* No signal voltage switch required */
		return 0;
	}
}

void bsp_get_phase(struct sdhci_host *host)
{
	struct sdhci_bsp_priv *priv = sdhci_get_pltfm_priv(host);
	unsigned int timing = host->mmc->ios.timing;

	if (priv->devid == 0) {
		if (timing == MMC_TIMING_MMC_HS400 ||
			timing == MMC_TIMING_MMC_HS200)
			priv->sample_phase = priv->tuning_phase;
		else
			priv->sample_phase = mmc_phase_cfg[timing][SAMPLE];

		priv->drv_phase = mmc_phase_cfg[timing][DRIVE];
	} else if (priv->devid == 1) {
		if (timing == MMC_TIMING_UHS_SDR104 ||
			timing == MMC_TIMING_UHS_SDR50)
			priv->sample_phase = priv->tuning_phase;
		else
			priv->sample_phase = sdio0_phase_cfg[timing][SAMPLE];

		priv->drv_phase = sdio0_phase_cfg[timing][DRIVE];
	} else if (priv->devid == 2) { /* device id 2 for sdio1 */
		if (timing == MMC_TIMING_MMC_HS200 ||
			timing == MMC_TIMING_UHS_SDR104 ||
			timing == MMC_TIMING_UHS_SDR50)
			priv->sample_phase = priv->tuning_phase;
		else
			priv->sample_phase = sdio1_phase_cfg[timing][SAMPLE];

		priv->drv_phase = sdio1_phase_cfg[timing][DRIVE];
	}
}

void bsp_set_drv_phase(struct sdhci_host *host, unsigned int phase)
{
	struct sdhci_bsp_priv *priv = sdhci_get_pltfm_priv(host);

	if (priv->devid == 0)
		regmap_write_bits(priv->crg_regmap, PERI_CRG_MMC_DRV_DLL,
			CRG_DRV_PHASE_SEL_MASK, (phase << CRG_DRV_PHASE_SEL_SHIFT));
	else if (priv->devid == 1)
		regmap_write_bits(priv->crg_regmap, PERI_CRG_SDIO0_DRV_DLL,
			CRG_DRV_PHASE_SEL_MASK, (phase << CRG_DRV_PHASE_SEL_SHIFT));
	else if (priv->devid == 2) /* device id 2 for sdio1 */
		regmap_write_bits(priv->crg_regmap, PERI_CRG_SDIO1_DRV_DLL,
			CRG_DRV_PHASE_SEL_MASK, (phase << CRG_DRV_PHASE_SEL_SHIFT));
}

#ifndef GET_EMMC_BUS_WIDTH_FORM_DTS
unsigned int get_mmc_bus_width(void)
{
	void __iomem *sys_stat_reg;
	unsigned int sys_stat;
	unsigned int bus_width;

	sys_stat_reg = ioremap_nocache(REG_SYSSTAT, sizeof(sys_stat));
	sys_stat = readl(sys_stat_reg);
	iounmap(sys_stat_reg);

	if ((sys_stat & BOOT_FLAG_MASK) == BOOT_MEDIA_EMMC) {
		bus_width = (sys_stat & EMMC_BOOT_8BIT) ?
			MMC_BUS_WIDTH_8_BIT : MMC_BUS_WIDTH_4_BIT;
	} else {
		/* up to 4 bit mode support when spi nand start up */
		bus_width = MMC_BUS_WIDTH_4_BIT;
	}

	return bus_width;
}

void set_mmc_bus_width(struct sdhci_host *host)
{
	struct sdhci_bsp_priv *priv = sdhci_get_pltfm_priv(host);

	/* for eMMC devices only */
	if (priv->devid == 0) {
		priv->bus_width = get_mmc_bus_width();
		if (priv->bus_width == MMC_BUS_WIDTH_8_BIT) {
			host->mmc->caps |= MMC_CAP_8_BIT_DATA;
			host->mmc->caps &= ~MMC_CAP_4_BIT_DATA;
		} else {
			host->mmc->caps |= MMC_CAP_4_BIT_DATA;
			host->mmc->caps &= ~MMC_CAP_8_BIT_DATA;
		}
	}
}
#endif

static void sdhci_enhanced_strobe(
		struct mmc_host *mmc, struct mmc_ios *ios)
{
	u16 ctrl;
	struct sdhci_host *host = mmc_priv(mmc);

	ctrl = sdhci_readw(host, SDHCI_EMMC_CTRL);
	if (ios->enhanced_strobe)
		ctrl |= SDHCI_ENH_STROBE_EN;
	else
		ctrl &= ~SDHCI_ENH_STROBE_EN;

	sdhci_writew(host, ctrl, SDHCI_EMMC_CTRL);
}

static void mmc_crg_init(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_bsp_priv *priv = sdhci_pltfm_priv(pltfm_host);

	reset_control_assert(priv->crg_rst);
	udelay(25); /* delay 25us */
	reset_control_deassert(priv->crg_rst);
	udelay(10); /* delay 10us */
}

static inline void set_io_mux(struct sdhci_host *host,
		unsigned int offset, unsigned int pin_mux)
{
	struct sdhci_bsp_priv *priv = sdhci_get_pltfm_priv(host);

	regmap_write_bits(priv->iocfg_regmap, offset,
			  IO_CFG_MUX_MASK, pin_mux);
}

static void mmc_io_mux_config(struct sdhci_host *host)
{
	struct sdhci_bsp_priv *priv = sdhci_get_pltfm_priv(host);
	unsigned int devid = priv->devid;
	unsigned int bus_width = priv->bus_width;
	unsigned int i, pin_mux;

	pin_mux = devid == 0 ? IO_CFG_EMMC_MUX : IO_CFG_SDIO_MUX;

	if (devid == 0) { /* eMMC device */
		set_io_mux(host, REG_MMC_CLK_IO, IO_CFG_EMMC_MUX);
		set_io_mux(host, REG_MMC_CMD_IO, IO_CFG_EMMC_MUX);
		for (i = 0; i < bus_width; i++)
			set_io_mux(host, reg_mmc_data_io[i], IO_CFG_EMMC_MUX);

		set_io_mux(host, REG_MMC_RST_IO, IO_CFG_EMMC_MUX);
		if (bus_width == MMC_BUS_WIDTH_8_BIT)
			set_io_mux(host, REG_MMC_DQS_IO, IO_CFG_EMMC_MUX);
	} else if (devid == 1) {
		set_io_mux(host, REG_SDIO0_DETECT_IO, IO_CFG_SDIO_MUX);
		set_io_mux(host, REG_SDIO0_PWEN_N, IO_CFG_SDIO_MUX);
		set_io_mux(host, REG_SDIO0_CLK_IO, IO_CFG_SDIO_MUX);
		set_io_mux(host, REG_SDIO0_CMD_IO, IO_CFG_SDIO_MUX);
		for (i = 0; i < bus_width; i++)
			set_io_mux(host, reg_sdio0_data_io[i], IO_CFG_SDIO_MUX);
	}
}

static int priv_init(struct platform_device *pdev, struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_bsp_priv *priv = sdhci_pltfm_priv(pltfm_host);
	struct device_node *np = pdev->dev.of_node;
	int rc;

	priv->crg_rst = devm_reset_control_get(&pdev->dev, "crg_reset");
	if (IS_ERR_OR_NULL(priv->crg_rst)) {
		rc = PTR_ERR(priv->crg_rst);
		dev_err(&pdev->dev, "get crg_rst failed. %d\n", rc);
		return PTR_ERR(priv->crg_rst);
	}

	priv->dll_rst = devm_reset_control_get(&pdev->dev, "dll_reset");
	if (IS_ERR_OR_NULL(priv->dll_rst)) {
		rc = PTR_ERR(priv->dll_rst);
		dev_err(&pdev->dev, "get dll_reset failed. %d\n", rc);
		return PTR_ERR(priv->dll_rst);
	}

	priv->sampl_rst = NULL;

	priv->crg_regmap = syscon_regmap_lookup_by_phandle(np, "crg_regmap");
	if (IS_ERR(priv->crg_regmap)) {
		rc = PTR_ERR(priv->crg_regmap);
		dev_err(&pdev->dev, "get crg regmap failed. %d\n", rc);
		return PTR_ERR(priv->crg_regmap);
	}

	priv->iocfg_regmap = syscon_regmap_lookup_by_phandle(np,
								"iocfg_regmap");
	if (IS_ERR(priv->iocfg_regmap)) {
		rc = PTR_ERR(priv->iocfg_regmap);
		dev_err(&pdev->dev, "get iocfg regmap failed. %d\n", rc);
		return PTR_ERR(priv->iocfg_regmap);
	}

	if (of_property_read_u32(np, "devid", &priv->devid)) {
		dev_err(mmc_dev(host->mmc), "get devid failed.\n");
		return -EINVAL;
	}

	return 0;
}

static void sdhci_caps_quirks_init(struct sdhci_host *host)
{
	/*
	 * only eMMC has a hw reset, and now eMMC signaling
	 * is fixed to 180
	 */
	if (host->mmc->caps & MMC_CAP_HW_RESET) {
		host->flags &= ~SDHCI_SIGNALING_330;
		host->flags |= SDHCI_SIGNALING_180;
	}

	/*
	 * we parse the support timings from dts, so we read the
	 * host capabilities early and clear the timing capabilities,
	 * SDHCI_QUIRK_MISSING_CAPS is set so that sdhci driver would
	 * not read it again
	 */
	host->caps = sdhci_readl(host, SDHCI_CAPABILITIES);
	host->caps &= ~(SDHCI_CAN_DO_HISPD | SDHCI_CAN_VDD_300);
	host->caps1 = sdhci_readl(host, SDHCI_CAPABILITIES_1);
	host->caps1 &= ~(SDHCI_SUPPORT_SDR50 | SDHCI_SUPPORT_SDR104 |
			 SDHCI_SUPPORT_DDR50 | SDHCI_CAN_DO_ADMA3);
	host->quirks |= SDHCI_QUIRK_MISSING_CAPS |
			SDHCI_QUIRK_NO_ENDATTR_IN_NOPDESC |
			SDHCI_QUIRK_SINGLE_POWER_WRITE;
	host->quirks2 &= ~SDHCI_QUIRK2_ACMD23_BROKEN;
}

int sdhci_bsp_pltfm_init(struct platform_device *pdev,
		struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct clk *clk = NULL;
	int ret;

	ret = priv_init(pdev, host);
	if (ret)
		return ret;

	ret = resistance_calibration();
	if (ret)
		return ret;

	clk = devm_clk_get(mmc_dev(host->mmc), "mmc_clk");
	if (IS_ERR_OR_NULL(clk)) {
		dev_err(mmc_dev(host->mmc), "get clk failed.\n");
		return -EINVAL;
	}
	pltfm_host->clk = clk;
	ret = clk_prepare_enable(pltfm_host->clk);
	if (ret)
		return ret;

	mmc_crg_init(host);
	ret = sdhci_bsp_parse_dt(host);
	if (ret)
		return ret;

#ifndef GET_EMMC_BUS_WIDTH_FORM_DTS
	set_mmc_bus_width(host);
#endif

	sdhci_caps_quirks_init(host);
	host->mmc_host_ops.hs400_enhanced_strobe = sdhci_enhanced_strobe;
	mci_host[slot_index++] = host->mmc;
	/* Initialization pin multiplexing first */
	mmc_io_mux_config(host);

	return 0;
}

void sdhci_bsp_set_clock(struct sdhci_host *host, unsigned int clk)
{
	struct sdhci_bsp_priv *priv = sdhci_get_pltfm_priv(host);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	unsigned int timing = host->mmc->ios.timing;

	bsp_disable_card_clk(host);
	udelay(25); /* delay 25us */
	bsp_disable_internal_clk(host);

	if (clk == 0) {
		host->mmc->actual_clock = 0;
		return;
	}

	clk_set_rate(pltfm_host->clk, clk);
	host->mmc->actual_clock = clk_get_rate(pltfm_host->clk);

	bsp_get_phase(host);
	bsp_set_drv_phase(host, priv->drv_phase);
	bsp_enable_sample(host);
	bsp_set_sample_phase(host, priv->sample_phase);

	udelay(5); /* delay 5us */

	bsp_enable_internal_clk(host);

	if ((timing == MMC_TIMING_MMC_HS400) ||
			(timing == MMC_TIMING_MMC_HS200) ||
			(timing == MMC_TIMING_UHS_SDR104) ||
			(timing == MMC_TIMING_UHS_SDR50)) {
		reset_control_assert(priv->dll_rst);
		reset_control_deassert(priv->dll_rst);
		bsp_wait_p4_dll_lock(host);
		bsp_wait_sample_dll_ready(host);
	}

	if (timing == MMC_TIMING_MMC_HS400)
		bsp_wait_ds_dll_ready(host);

	bsp_enable_card_clk(host);
	udelay(75); /* delay 75us */
}

inline unsigned long sdhci_bsp_get_peri_lock(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_bsp_priv *priv = sdhci_pltfm_priv(pltfm_host);

	if (priv->devid != 0) {
		return bsp_peri_lock(BSP_PERI_SDIO);
	}

	return 0;
}

inline void sdhci_bsp_put_peri_lock(struct sdhci_host *host, unsigned long flags)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_bsp_priv *priv = sdhci_pltfm_priv(pltfm_host);

	if (priv->devid != 0) {
		bsp_peri_unlock(flags, BSP_PERI_SDIO);
	}
}

void sdhci_bsp_writel(struct sdhci_host *host, u32 val, int reg)
{
	unsigned long flags;

	flags = sdhci_bsp_get_peri_lock(host);
	writel(val, host->ioaddr + reg);
	sdhci_bsp_put_peri_lock(host, flags);
}

void sdhci_bsp_writew(struct sdhci_host *host, u16 val, int reg)
{
	unsigned long flags;

	flags = sdhci_bsp_get_peri_lock(host);
	writew(val, host->ioaddr + reg);
	sdhci_bsp_put_peri_lock(host, flags);
}

void sdhci_bsp_writeb(struct sdhci_host *host, u8 val, int reg)
{
	unsigned long flags;

	flags = sdhci_bsp_get_peri_lock(host);
	writeb(val, host->ioaddr + reg);
	sdhci_bsp_put_peri_lock(host, flags);
}

u32 sdhci_bsp_readl(struct sdhci_host *host, int reg)
{
	u32 val;
	unsigned long flags;

	flags = sdhci_bsp_get_peri_lock(host);
	val = readl(host->ioaddr + reg);
	sdhci_bsp_put_peri_lock(host, flags);

	return val;
}

u16 sdhci_bsp_readw(struct sdhci_host *host, int reg)
{
	u16 val;
	unsigned long flags;

	flags = sdhci_bsp_get_peri_lock(host);
	val = readw(host->ioaddr + reg);
	sdhci_bsp_put_peri_lock(host, flags);

	return val;
}

u8 sdhci_bsp_readb(struct sdhci_host *host, int reg)
{
	u8 val;
	unsigned long flags;

	flags = sdhci_bsp_get_peri_lock(host);
	val = readb(host->ioaddr + reg);
	sdhci_bsp_put_peri_lock(host, flags);

	return val;
}

int bsp_support_runtime_pm(struct sdhci_host *host)
{
	struct mmc_host *mmc = host->mmc;

	/* only eMMC/SD Card device support runtime_pm */
	if (mmc->caps2 & MMC_CAP2_NO_SDIO)
		return 1;
	else
		return 0;
}
