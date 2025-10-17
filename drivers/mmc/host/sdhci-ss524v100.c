/*
 * Copyright (c) Shenshu Technologies Co., Ltd. 2019-2020. All rights reserved.
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
 *
 */

#include "sdhci-bsp.h"
#include "mci_proc.h"

#define PERI_CRG_MMC_DRV_DLL	0x34c8
#define  CRG_DRV_PHASE_SEL_SHIFT	15
#define  CRG_DRV_PHASE_SEL_MASK	(0x1F << 15)

#define PERI_CRG_MMC_STAT	0x34d8
#define  CRG_SAM_DLL_READY	BIT(12)
#define  CRG_DS_DLL_READY	BIT(10)
#define  CRG_P4_DLL_LOCKED	BIT(9)

/* MMC IO */
#define REG_MMC_CLK_IO		0xc8
#define REG_MMC_CMD_IO		0xcc
#define REG_MMC_D0_IO		0xd0
#define REG_MMC_D1_IO		0xd4
#define REG_MMC_D2_IO		0xd8
#define REG_MMC_D3_IO		0xdc
#define REG_MMC_D4_IO		0xec
#define REG_MMC_D6_IO		0xf0
#define REG_MMC_D5_IO		0xe0
#define REG_MMC_D7_IO		0xe4
#define REG_MMC_DQS_IO		0xf4
#define REG_MMC_RST_IO		0xc4

#define REG_SD_PWR_EN_IO		0xc4
#define REG_SD_DETECT_IO		0xfc

/* IO CFG */
#define IO_CFG_DRV_STR_MASK	(0xf << 4)
#define io_cfg_drv_str_sel(str)	((str) << 4)
#define IO_CFG_PULL_UP		BIT(8)
#define IO_CFG_PULL_DOWN	BIT(9)
#define IO_CFG_SR		BIT(10)
#define IO_CFG_SDIO_MASK	(IO_CFG_DRV_STR_MASK | IO_CFG_PULL_UP | \
				 IO_CFG_PULL_DOWN | IO_CFG_SR)
#define IO_CFG_SDIO_MUX		0x2
#define IO_CFG_EMMC_MUX		0x1
#define IO_CFG_MUX_MASK		0xF
#define MMC_BUS_WIDTH_8_BIT	8
#define MMC_BUS_WIDTH_4_BIT	4

#define REG_SYSSTAT		0x11020018
#define  BOOT_MEDIA_NAND	BIT(2)
#define  EMMC_BOOT_8BIT	BIT(11)
#define  BOOT_FLAG_MASK	(0x3 << 2)

#define IO_CLK			0
#define IO_CMD			1
#define IO_DATA			2
#define IO_RST			3
#define IO_DS			4
#define EMMC_IO_TYPE_NUM	5
#define SDIO_IO_TYPE_NUM	3

/* sample drive phase */
#define DRIVE			0
#define SAMPLE			1
#define PHASE_TYPE_NUM		2

#define POLL_DELAY	1000  /* us */
#define POLL_TIMEOUT	20000 /* us */

static unsigned int reg_data_io[] = {
	REG_MMC_D0_IO, REG_MMC_D1_IO,
	REG_MMC_D2_IO, REG_MMC_D3_IO,
	REG_MMC_D4_IO, REG_MMC_D5_IO,
	REG_MMC_D6_IO, REG_MMC_D7_IO
};

static u32 bsp_mmc_io_cfg[][EMMC_IO_TYPE_NUM] = {
	[MMC_TIMING_LEGACY] = {
		io_cfg_drv_str_sel(0xd) | IO_CFG_PULL_DOWN,
		io_cfg_drv_str_sel(0xe) | IO_CFG_PULL_UP,
		io_cfg_drv_str_sel(0xe) | IO_CFG_PULL_UP,
		io_cfg_drv_str_sel(0x3) | IO_CFG_PULL_UP
	},
	[MMC_TIMING_MMC_HS] = {
		io_cfg_drv_str_sel(0xd) | IO_CFG_PULL_DOWN,
		io_cfg_drv_str_sel(0xe) | IO_CFG_PULL_UP,
		io_cfg_drv_str_sel(0xe) | IO_CFG_PULL_UP,
		io_cfg_drv_str_sel(0x3) | IO_CFG_PULL_UP
	},
	[MMC_TIMING_MMC_HS200] = {
		io_cfg_drv_str_sel(0x8) | IO_CFG_PULL_DOWN,
		io_cfg_drv_str_sel(0xc) | IO_CFG_PULL_UP,
		io_cfg_drv_str_sel(0xc) | IO_CFG_PULL_UP,
		io_cfg_drv_str_sel(0x3) | IO_CFG_PULL_UP
	},
	[MMC_TIMING_MMC_HS400] = {
		io_cfg_drv_str_sel(0x8) | IO_CFG_PULL_DOWN,
		io_cfg_drv_str_sel(0x9) | IO_CFG_PULL_UP,
		io_cfg_drv_str_sel(0x9) | IO_CFG_PULL_UP,
		io_cfg_drv_str_sel(0x3) | IO_CFG_PULL_UP,
		io_cfg_drv_str_sel(0x3)  /* board-level pull-down */
	}
};

static u32 bsp_sd_io_cfg[][SDIO_IO_TYPE_NUM] = {
	[MMC_TIMING_LEGACY] = {
		io_cfg_drv_str_sel(0xd) | IO_CFG_PULL_DOWN,
		io_cfg_drv_str_sel(0xe) | IO_CFG_PULL_UP,
		io_cfg_drv_str_sel(0xe) | IO_CFG_PULL_UP
	},
	[MMC_TIMING_SD_HS] = {
		io_cfg_drv_str_sel(0xd) | IO_CFG_PULL_DOWN,
		io_cfg_drv_str_sel(0xe) | IO_CFG_PULL_UP,
		io_cfg_drv_str_sel(0xe) | IO_CFG_PULL_UP
	}
};

static u32 bsp_sdio_io_cfg[][SDIO_IO_TYPE_NUM] = {
	[MMC_TIMING_LEGACY] = {
		io_cfg_drv_str_sel(0xd) | IO_CFG_PULL_DOWN,
		io_cfg_drv_str_sel(0xe) | IO_CFG_PULL_UP,
		io_cfg_drv_str_sel(0xe) | IO_CFG_PULL_UP
	},
	[MMC_TIMING_SD_HS] = {
		io_cfg_drv_str_sel(0xd) | IO_CFG_PULL_DOWN,
		io_cfg_drv_str_sel(0xe) | IO_CFG_PULL_UP,
		io_cfg_drv_str_sel(0xe) | IO_CFG_PULL_UP
	},
	[MMC_TIMING_UHS_SDR12] = {
		io_cfg_drv_str_sel(0xb) | IO_CFG_PULL_DOWN,
		io_cfg_drv_str_sel(0xd) | IO_CFG_PULL_UP,
		io_cfg_drv_str_sel(0xd) | IO_CFG_PULL_UP
	},
	[MMC_TIMING_UHS_SDR25] = {
		io_cfg_drv_str_sel(0xb) | IO_CFG_PULL_DOWN,
		io_cfg_drv_str_sel(0xc) | IO_CFG_PULL_UP,
		io_cfg_drv_str_sel(0xc) | IO_CFG_PULL_UP
	},
	[MMC_TIMING_UHS_SDR50] = {
		io_cfg_drv_str_sel(0x8) | IO_CFG_PULL_DOWN,
		io_cfg_drv_str_sel(0xa) | IO_CFG_PULL_UP,
		io_cfg_drv_str_sel(0xa) | IO_CFG_PULL_UP
	},
	[MMC_TIMING_UHS_SDR104] = {
		io_cfg_drv_str_sel(0x0) | IO_CFG_PULL_DOWN,
		io_cfg_drv_str_sel(0x9) | IO_CFG_PULL_UP,
		io_cfg_drv_str_sel(0x9) | IO_CFG_PULL_UP
	}
};

static u32 bsp_phase_cfg[][PHASE_TYPE_NUM] = {
	[MMC_TIMING_LEGACY]     = { 16, 0 },
	[MMC_TIMING_MMC_HS]     = { 16, 4 },
	[MMC_TIMING_SD_HS]      = { 16, 4 },
	[MMC_TIMING_UHS_SDR12]  = { 16, 0 },
	[MMC_TIMING_UHS_SDR25]  = { 16, 4 },
	[MMC_TIMING_UHS_SDR50]  = { 20, 0 },
	[MMC_TIMING_UHS_SDR104] = { 20, 0 },
	[MMC_TIMING_MMC_HS200]  = { 22, 0 },
#if defined(CONFIG_ARCH_SS524V100) || defined(CONFIG_ARCH_SS522V100) || defined(CONFIG_ARCH_SS522V101)
	[MMC_TIMING_MMC_HS400]  = {  9, 0 }
#endif
};

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

static void bsp_set_drv_str(struct sdhci_host *host,
    unsigned int offset, unsigned int drv_str)
{
	struct sdhci_bsp_priv *priv = sdhci_get_pltfm_priv(host);

	regmap_write_bits(priv->iocfg_regmap, offset, IO_CFG_SDIO_MASK,
			  drv_str);
}

void bsp_set_drv_cap(struct sdhci_host *host)
{
	struct sdhci_bsp_priv *priv = sdhci_get_pltfm_priv(host);
	unsigned int i;

	if (priv->devid == 0) {        /* emmc devices */
		bsp_set_drv_str(host, REG_MMC_CLK_IO,
				 bsp_mmc_io_cfg[host->timing][IO_CLK]);
		bsp_set_drv_str(host, REG_MMC_CMD_IO,
				 bsp_mmc_io_cfg[host->timing][IO_CMD]);
		for (i = 0; i < priv->bus_width; i++)
			bsp_set_drv_str(host, reg_data_io[i],
					 bsp_mmc_io_cfg[host->timing][IO_DATA]);
		bsp_set_drv_str(host, REG_MMC_RST_IO,
				 bsp_mmc_io_cfg[host->timing][IO_RST]);
		if (host->timing == MMC_TIMING_MMC_HS400)
			bsp_set_drv_str(host, REG_MMC_DQS_IO,
					 bsp_mmc_io_cfg[host->timing][IO_DS]);
	} else if (priv->devid == 1) { /* sd devices */
		bsp_set_drv_str(host, REG_MMC_CLK_IO,
				 bsp_sd_io_cfg[host->timing][IO_CLK]);
		bsp_set_drv_str(host, REG_MMC_CMD_IO,
				 bsp_sd_io_cfg[host->timing][IO_CMD]);
		for (i = 0; i < priv->bus_width; i++)
			bsp_set_drv_str(host, reg_data_io[i],
					 bsp_sd_io_cfg[host->timing][IO_DATA]);
	} else {                       /* sdio devices */
		bsp_set_drv_str(host, REG_MMC_CLK_IO,
				 bsp_sdio_io_cfg[host->timing][IO_CLK]);
		bsp_set_drv_str(host, REG_MMC_CMD_IO,
				 bsp_sdio_io_cfg[host->timing][IO_CMD]);
		for (i = 0; i < priv->bus_width; i++)
			bsp_set_drv_str(host, reg_data_io[i],
					 bsp_sdio_io_cfg[host->timing][IO_DATA]);
	}
}

void bsp_wait_sample_dll_ready(struct sdhci_host *host)
{
	struct sdhci_bsp_priv *priv = sdhci_get_pltfm_priv(host);
	unsigned int val;
	int ret;

	ret = regmap_read_poll_timeout(priv->crg_regmap, PERI_CRG_MMC_STAT,
				       val, (val & CRG_SAM_DLL_READY),
				       POLL_DELAY, POLL_TIMEOUT);
	if (ret)
		pr_err("%s: SAMPL DLL slave not ready.\n",
		       mmc_hostname(host->mmc));
}

void bsp_wait_p4_dll_lock(struct sdhci_host *host)
{
	struct sdhci_bsp_priv *priv = sdhci_get_pltfm_priv(host);
	unsigned int val;
	int ret;

	ret = regmap_read_poll_timeout(priv->crg_regmap, PERI_CRG_MMC_STAT,
				       val, (val & CRG_P4_DLL_LOCKED),
				       POLL_DELAY, POLL_TIMEOUT);
	if (ret)
		pr_err("%s: P4 DLL master not locked.\n",
		       mmc_hostname(host->mmc));
}

void bsp_wait_ds_dll_ready(struct sdhci_host *host)
{
	struct sdhci_bsp_priv *priv = sdhci_get_pltfm_priv(host);
	unsigned int val;
	int ret;

	ret = regmap_read_poll_timeout(priv->crg_regmap, PERI_CRG_MMC_STAT,
				       val, (val & CRG_DS_DLL_READY),
				       POLL_DELAY, POLL_TIMEOUT);
	if (ret)
		pr_err("%s: DS DLL slave ready.\n", mmc_hostname(host->mmc));
}

void bsp_get_phase(struct sdhci_host *host)
{
	struct sdhci_bsp_priv *priv = sdhci_get_pltfm_priv(host);
	unsigned int timing = host->mmc->ios.timing;

	if (timing == MMC_TIMING_MMC_HS400 ||
			timing == MMC_TIMING_MMC_HS200 ||
			timing == MMC_TIMING_UHS_SDR104 ||
			timing == MMC_TIMING_UHS_SDR50)
		priv->sample_phase = priv->tuning_phase;
	else
		priv->sample_phase =
			bsp_phase_cfg[timing][SAMPLE];

	priv->drv_phase = bsp_phase_cfg[timing][DRIVE];
}

void bsp_set_drv_phase(struct sdhci_host *host, unsigned int phase)
{
	struct sdhci_bsp_priv *priv = sdhci_get_pltfm_priv(host);

	regmap_write_bits(priv->crg_regmap, PERI_CRG_MMC_DRV_DLL,
			  CRG_DRV_PHASE_SEL_MASK, (phase << CRG_DRV_PHASE_SEL_SHIFT));
}
#ifndef GET_EMMC_BUS_WIDTH_FORM_DTS
unsigned int bsp_get_mmc_bus_width(void)
{
	void __iomem *sys_stat_reg;
	unsigned int sys_stat;
	unsigned int bus_width;

	sys_stat_reg = ioremap_nocache(REG_SYSSTAT, sizeof(sys_stat));
	sys_stat = readl(sys_stat_reg);
	iounmap(sys_stat_reg);

	if ((sys_stat & BOOT_FLAG_MASK) == BOOT_MEDIA_NAND) {
		/* up to 4 bit mode support when spi nand start up */
		bus_width = MMC_BUS_WIDTH_4_BIT;
	} else {
		bus_width = (sys_stat & EMMC_BOOT_8BIT) ?
			    MMC_BUS_WIDTH_8_BIT : MMC_BUS_WIDTH_4_BIT;
	}

	return bus_width;
}

void bsp_set_mmc_bus_width(struct sdhci_host *host)
{
	struct sdhci_bsp_priv *priv = sdhci_get_pltfm_priv(host);

	/* for eMMC devices only */
	if (priv->devid == 0) {
		priv->bus_width = bsp_get_mmc_bus_width();
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
static void sdhci_bsp_enhanced_strobe(
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

static void bsp_mmc_crg_init(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_bsp_priv *priv = sdhci_pltfm_priv(pltfm_host);

	reset_control_assert(priv->crg_rst);
	udelay(25); /* delay 25us */
	reset_control_deassert(priv->crg_rst);
	udelay(10); /* delay 10us */
}

static inline void bsp_set_io_mux(struct sdhci_host *host,
    unsigned int offset, unsigned int pin_mux)
{
	struct sdhci_bsp_priv *priv = sdhci_get_pltfm_priv(host);

	regmap_write_bits(priv->iocfg_regmap, offset,
			  IO_CFG_MUX_MASK, pin_mux);
}

static void bsp_mmc_io_mux_config(struct sdhci_host *host)
{
	struct sdhci_bsp_priv *priv = sdhci_get_pltfm_priv(host);
	unsigned int devid = priv->devid;
	unsigned int bus_width = priv->bus_width;
	unsigned int i, pin_mux;

	pin_mux = (devid == 0) ? IO_CFG_EMMC_MUX : IO_CFG_SDIO_MUX;
	bsp_set_io_mux(host, REG_MMC_CLK_IO, pin_mux);
	bsp_set_io_mux(host, REG_MMC_CMD_IO, pin_mux);
	for (i = 0; i < bus_width; i++)
		bsp_set_io_mux(host, reg_data_io[i], pin_mux);

	if (devid == 0) { /* eMMC device */
		bsp_set_io_mux(host, REG_MMC_RST_IO, IO_CFG_EMMC_MUX);
		if (bus_width == MMC_BUS_WIDTH_8_BIT)
			bsp_set_io_mux(host, REG_MMC_DQS_IO, IO_CFG_EMMC_MUX);
	}

	if (devid == 1) { /* sd device */
		bsp_set_io_mux(host, REG_SD_DETECT_IO, IO_CFG_SDIO_MUX);
		bsp_set_io_mux(host, REG_SD_PWR_EN_IO, IO_CFG_SDIO_MUX);
		/* Pull-up is required by default. */
		bsp_set_drv_str(host, REG_SD_DETECT_IO, IO_CFG_PULL_UP);
	}
}

static int bsp_priv_init(struct platform_device *pdev, struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_bsp_priv *bsp_priv = sdhci_pltfm_priv(pltfm_host);
	struct device_node *np = pdev->dev.of_node;
	int rc;

	bsp_priv->crg_rst = devm_reset_control_get(&pdev->dev, "crg_reset");
	if (IS_ERR_OR_NULL(bsp_priv->crg_rst)) {
		rc = PTR_ERR(bsp_priv->crg_rst);
		dev_err(&pdev->dev, "get crg_rst failed. %d\n", rc);
		return PTR_ERR(bsp_priv->crg_rst);
	}

	bsp_priv->dll_rst = devm_reset_control_get(&pdev->dev, "dll_reset");
	if (IS_ERR_OR_NULL(bsp_priv->dll_rst)) {
		rc = PTR_ERR(bsp_priv->dll_rst);
		dev_err(&pdev->dev, "get dll_reset failed. %d\n", rc);
		return PTR_ERR(bsp_priv->dll_rst);
	}

	bsp_priv->crg_regmap = syscon_regmap_lookup_by_phandle(np,
				"crg_regmap");
	if (IS_ERR(bsp_priv->crg_regmap)) {
		rc = PTR_ERR(bsp_priv->crg_regmap);
		dev_err(&pdev->dev, "get crg regmap failed. %d\n", rc);
		return PTR_ERR(bsp_priv->crg_regmap);
	}

	bsp_priv->iocfg_regmap = syscon_regmap_lookup_by_phandle(np,
				  "iocfg_regmap");
	if (IS_ERR(bsp_priv->iocfg_regmap)) {
		rc = PTR_ERR(bsp_priv->iocfg_regmap);
		dev_err(&pdev->dev, "get iocfg regmap failed. %d\n", rc);
		return PTR_ERR(bsp_priv->iocfg_regmap);
	}

	if (of_property_read_u32(np, "devid", &bsp_priv->devid)) {
		rc = PTR_ERR(bsp_priv->iocfg_regmap);
		dev_err(mmc_dev(host->mmc), "get devid failed. %d\n", rc);
		return -EINVAL;
	}

	return 0;
}

int sdhci_bsp_pltfm_init(struct platform_device *pdev,
    struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct clk *clk = NULL;
	int ret;

	ret = bsp_priv_init(pdev, host);
	if (ret)
		return ret;

	clk = devm_clk_get(mmc_dev(host->mmc), "mmc_clk");
	if (IS_ERR_OR_NULL(clk)) {
		dev_err(mmc_dev(host->mmc), "get clk failed.\n");
		return -EINVAL;
	}
	pltfm_host->clk = clk;

	ret = sdhci_bsp_parse_dt(host);
	if (ret)
		return ret;

#ifndef GET_EMMC_BUS_WIDTH_FORM_DTS
	bsp_set_mmc_bus_width(host);
#endif
	/* Initialization pin multiplexing first */
	bsp_mmc_io_mux_config(host);

	ret = clk_prepare_enable(pltfm_host->clk);
	if (ret)
		return ret;

	bsp_mmc_crg_init(host);

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

	host->mmc_host_ops.hs400_enhanced_strobe = sdhci_bsp_enhanced_strobe;

	mci_host[slot_index++] = host->mmc;
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

int bsp_support_runtime_pm(struct sdhci_host *host)
{
	struct sdhci_bsp_priv *priv = sdhci_get_pltfm_priv(host);

	/* only eMMC & sd device support runtime_pm */
	return (priv->devid == 0 || priv->devid == 1) ? 1 : 0;
}
