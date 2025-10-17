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

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/io.h>

static unsigned int phy_mode = CONFIG_BSP_SATA_MODE;
static unsigned int ports_num;
unsigned int sata_port_map;

#define SLEEP_TIME 20
#ifdef MODULE
module_param(mode_3g, uint, 0600);
MODULE_PARM_DESC(phy_mode, "sata phy mode (0:1.5G;1:3G(default);2:6G)");
#endif

#ifdef CONFIG_ARCH_SS528V100
#include "phy-ss528v100-sata.c"
#endif

#ifdef CONFIG_ARCH_SS625V100
#include "phy-ss625v100-sata.c"
#endif

#if (defined(CONFIG_ARCH_SS524V100) || defined(CONFIG_ARCH_SS522V100) || defined(CONFIG_ARCH_SS522V101) || defined(CONFIG_ARCH_SS615V100))
#include "phy-ss524v100-sata.c"
#endif

static int bsp_sata_phy_init(struct phy *phy)
{
	unsigned int sata_port_num;
	void __iomem *mmio = phy_get_drvdata(phy);

	sata_port_num = bsp_sata_get_port_info();
	if ((sata_port_num < 1)) {
		pr_err("sata ports number:%d WRONG!!!\n", sata_port_num);
		return -EINVAL;
	}
	ports_num = sata_port_num;

	bsp_sata_poweron();
	bsp_sata_reset();
	bsp_sata_phy_reset();
	bsp_sata_phy_clk_sel();
	bsp_sata_clk_enable();
	msleep(SLEEP_TIME);
	bsp_sata_phy_unreset();
	msleep(SLEEP_TIME);
	bsp_sata_unreset();
	msleep(SLEEP_TIME);
	bsp_sata_phy_config(mmio, phy_mode);

	return 0;
}

static int bsp_sata_phy_exit(struct phy *phy)
{
	bsp_sata_phy_reset();
	msleep(SLEEP_TIME);
	bsp_sata_reset();
	msleep(SLEEP_TIME);
	bsp_sata_clk_reset();
	msleep(SLEEP_TIME);
	bsp_sata_clk_disable();
	bsp_sata_poweroff();
	msleep(SLEEP_TIME);

	return 0;
}

static struct phy_ops bsp_sata_phy_ops = {
	.init		= bsp_sata_phy_init,
	.exit		= bsp_sata_phy_exit,
	.owner		= THIS_MODULE,
};

static int bsp_sata_phy_probe(struct platform_device *pdev)
{
	struct phy_provider *phy_provider = NULL;
	struct device *dev = &pdev->dev;
	struct resource *res = NULL;
	struct phy *phy = NULL;
	void __iomem *mmio = NULL;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "failed to get reg base\n");
		return -ENOENT;
	}

	mmio = devm_ioremap(dev, res->start, resource_size(res));
	if (!mmio)
		return -ENOMEM;

	phy = devm_phy_create(dev, NULL, &bsp_sata_phy_ops);
	if (IS_ERR(phy)) {
		dev_err(dev, "failed to create PHY\n");
		return PTR_ERR(phy);
	}

	of_property_read_u32(dev->of_node, "ports_num_max", &ports_num);

	phy_set_drvdata(phy, mmio);

	phy_provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);
	if (IS_ERR(phy_provider))
		return PTR_ERR(phy_provider);

	return 0;
}

static int bsp_sata_phy_suspend(struct platform_device *pdev,
				 pm_message_t state)
{
	struct device *dev = &pdev->dev;
	struct phy *phy = to_phy(dev);

	bsp_sata_phy_exit(phy);

	return 0;
}

static int bsp_sata_phy_resume(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct phy *phy = to_phy(dev);
	int ret = 0;

	ret = bsp_sata_phy_init(phy);
	if (ret != 0)
		return -EINVAL;

	return 0;
}

static const struct of_device_id bsp_sata_phy_of_match[] = {
	{ .compatible = "vendor,sata-phy", },
	{ },
};
MODULE_DEVICE_TABLE(of, bsp_sata_phy_of_match);

static struct platform_driver bsp_sata_phy_driver = {
	.probe	= bsp_sata_phy_probe,
	.suspend = bsp_sata_phy_suspend,
	.resume  = bsp_sata_phy_resume,
	.driver = {
		.name	= "bsp-sata-phy",
		.of_match_table	= bsp_sata_phy_of_match,
	}
};
module_platform_driver(bsp_sata_phy_driver);

MODULE_AUTHOR("Vendor");
MODULE_DESCRIPTION("Vendor SATA PHY driver");
MODULE_ALIAS("platform:bsp-sata-phy");
MODULE_LICENSE("GPL v2");
