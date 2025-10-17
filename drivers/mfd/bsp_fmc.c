/* Vendor Flash Memory Controller Driver
 *
 * Copyright (c) 2016 Shenshu Technologies Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/mfd/core.h>
#include <linux/mfd/bsp_fmc.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>

unsigned char fmc_cs_user[FMC_MAX_CHIP_NUM];

DEFINE_MUTEX(fmc_switch_mutex);
EXPORT_SYMBOL_GPL(fmc_switch_mutex);

/* ------------------------------------------------------------------------ */
static const struct mfd_cell bsp_fmc_devs[] = {
	{
		.name = "bsp_spi_nor",
		.of_compatible = "vendor,fmc-spi-nor",
	},
	{
		.name = "bsp_spi_nand",
		.of_compatible = "vendor,fmc-spi-nand",
	},
	{
		.name = "bsp_nand",
		.of_compatible = "vendor,fmc-nand",
	},
};

static int bsp_fmc_probe(struct platform_device *pdev)
{
	struct bsp_fmc *fmc = NULL;
	struct resource *res = NULL;
	struct device *dev = &pdev->dev;
	int ret;

	fmc = devm_kzalloc(dev, sizeof(*fmc), GFP_KERNEL);
	if (!fmc)
		return -ENOMEM;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "control");
	fmc->regbase = devm_ioremap_resource(dev, res);
	if (IS_ERR(fmc->regbase))
		return PTR_ERR(fmc->regbase);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "memory");
	fmc->iobase = devm_ioremap_resource(dev, res);
	if (IS_ERR(fmc->iobase))
		return PTR_ERR(fmc->iobase);

	fmc->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(fmc->clk))
		return PTR_ERR(fmc->clk);

	if (of_property_read_u32(dev->of_node, "max-dma-size", &fmc->dma_len)) {
		dev_err(dev, "Please set the suitable max-dma-size value !!!\n");
		return -ENOMEM;
	}

	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(34));
	if (ret) {
		dev_warn(dev, "Unable to set dma mask\n");
		return ret;
	}

	fmc->buffer = dmam_alloc_coherent(dev, fmc->dma_len,
					  &fmc->dma_buffer, GFP_KERNEL);
	if (IS_ERR(fmc->buffer))
		return PTR_ERR(fmc->buffer);

	mutex_init(&fmc->lock);

	platform_set_drvdata(pdev, fmc);

	ret = mfd_add_devices(dev, 0, bsp_fmc_devs,
			      ARRAY_SIZE(bsp_fmc_devs), NULL, 0, NULL);
	if (ret) {
		dev_err(dev, "add mfd devices failed: %d\n", ret);
		return ret;
	}

	return 0;
}

static int bsp_fmc_remove(struct platform_device *pdev)
{
	struct bsp_fmc *fmc = platform_get_drvdata(pdev);

	dmam_free_coherent(&pdev->dev, fmc->dma_len,
			   fmc->buffer, fmc->dma_buffer);
	mfd_remove_devices(&pdev->dev);
	mutex_destroy(&fmc->lock);

	return 0;
}

static const struct of_device_id bsp_fmc_of_match_tbl[] = {
	{.compatible = "vendor,fmc"},
	{ }
};
MODULE_DEVICE_TABLE(of, bsp_fmc_of_match_tbl);

static struct platform_driver bsp_fmc_driver = {
	.driver = {
		.name = "fmc",
		.of_match_table = bsp_fmc_of_match_tbl,
	},
	.probe = bsp_fmc_probe,
	.remove = bsp_fmc_remove,
};
module_platform_driver(bsp_fmc_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Flash Memory Controller Driver");
