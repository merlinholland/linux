/*
 *
 * Copyright (c) 2012-2021 Shenshu Technologies Co., Ltd.
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

#include <linux/clk.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_mdio.h>
#include <linux/platform_device.h>
#include <linux/reset.h>
#include <linux/securec.h>
#include "mdio_bsp_gemac.h"

#define MDIO_SINGLE_CMD		0x00
#define MDIO_SINGLE_DATA	0x04
#define MDIO_RDATA_STATUS	0x10
#define BIT_PHY_ADDR_OFFSET	8
#define MDIO_WRITE		BIT(16)
#define MDIO_READ		BIT(17)
#define MDIO_START		BIT(20)
#define MDIO_START_READ		(MDIO_START | MDIO_READ)
#define MDIO_START_WRITE	(MDIO_START | MDIO_WRITE)

struct bsp_gemac_mdio_data {
	struct clk *clk;
	struct reset_control *phy_rst;
	void __iomem *membase;
};

static int bsp_gemac_mdio_wait_ready(struct bsp_gemac_mdio_data *data)
{
	u32 val;
#define DELAY_US 20
#define TIMEOUT_US 10000
	return readl_poll_timeout(data->membase + MDIO_SINGLE_CMD,
				  val, !(val & MDIO_START), DELAY_US, TIMEOUT_US);
}

static int bsp_gemac_mdio_read(struct mii_bus *bus, int mii_id, int regnum)
{
	struct bsp_gemac_mdio_data *data = bus->priv;
	int ret;

	ret = bsp_gemac_mdio_wait_ready(data);
	if (ret)
		return ret;

	writel(MDIO_START_READ | ((u32)mii_id << BIT_PHY_ADDR_OFFSET) |
	       ((u32)regnum),
	       data->membase + MDIO_SINGLE_CMD);

	ret = bsp_gemac_mdio_wait_ready(data);
	if (ret)
		return ret;

	/* if read data is invalid, we just return 0 instead of -EAGAIN.
	 * This can make MDIO more robust when reading PHY status.
	 */
	if (readl(data->membase + MDIO_RDATA_STATUS))
		return 0;

	return readl(data->membase + MDIO_SINGLE_DATA) >> 16; /* 16:right shift */
}

static int bsp_gemac_mdio_write(struct mii_bus *bus, int mii_id, int regnum,
				 u16 value)
{
	struct bsp_gemac_mdio_data *data = bus->priv;
	int ret;

	ret = bsp_gemac_mdio_wait_ready(data);
	if (ret)
		return ret;

	writel(value, data->membase + MDIO_SINGLE_DATA);
	writel(MDIO_START_WRITE | ((u32)mii_id << BIT_PHY_ADDR_OFFSET) |
	       ((u32)regnum),
	       data->membase + MDIO_SINGLE_CMD);

	return bsp_gemac_mdio_wait_ready(data);
}

static void bsp_gemac_external_phy_reset(struct bsp_gemac_mdio_data const *data)
{
	if (data->phy_rst) {
		/* write 0 to cancel reset */
		reset_control_deassert(data->phy_rst);
		msleep(50); /* 50:delay */

		/* use CRG register to reset phy */
		/* RST_BIT, write 0 to reset phy, write 1 to cancel reset */
		reset_control_assert(data->phy_rst);

		/* delay some time to ensure reset ok,
		 * this depends on PHY hardware feature
		 */
		msleep(50); /* 50:delay */

		/* write 0 to cancel reset */
		reset_control_deassert(data->phy_rst);
		/* delay some time to ensure later MDIO access */
		msleep(50); /* 50:delay */
	}
}

static void bsp_gemac_mdiobus_init(struct mii_bus *const bus, struct platform_device *const pdev)
{
	u32 str_len;
	str_len = strlen(pdev->name);
	bus->name = "bsp_gemac_mii_bus";
	bus->read = &bsp_gemac_mdio_read;
	bus->write = &bsp_gemac_mdio_write;
	if (snprintf_s(bus->id, MII_BUS_ID_SIZE, str_len, "%s", pdev->name) < 0)
		printk("snprintf_s failed! func:%s, line: %d\n", __func__, __LINE__);
	bus->parent = &pdev->dev;
	return;
}

static int bsp_gemac_mdio_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct mii_bus *bus = NULL;
	struct bsp_gemac_mdio_data *data = NULL;
	struct resource *res = NULL;

	int ret = bsp_gemac_pinctrl_config(pdev);
	if (ret) {
		pr_err("gmac pinctrl config error=%d.\n", ret);
		return ret;
	}

	bus = mdiobus_alloc_size(sizeof(*data));
	if (!bus)
		return -ENOMEM;

	bsp_gemac_mdiobus_init(bus, pdev);

	data = bus->priv;
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL || data == NULL) {
		ret = -ENXIO;
		goto err_out_free_mdiobus;
	}
	data->membase = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!data->membase) {
		ret = -ENOMEM;
		goto err_out_free_mdiobus;
	}

	data->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(data->clk)) {
		ret = PTR_ERR(data->clk);
		goto err_out_free_mdiobus;
	}

	ret = clk_prepare_enable(data->clk);
	if (ret)
		goto err_out_free_mdiobus;

	data->phy_rst = devm_reset_control_get(&pdev->dev, "phy_reset");
	if (IS_ERR(data->phy_rst))
		data->phy_rst = NULL;
	bsp_gemac_external_phy_reset(data);

	ret = of_mdiobus_register(bus, np);
	if (ret)
		goto err_out_disable_clk;

	platform_set_drvdata(pdev, bus);

	return 0;

err_out_disable_clk:
	clk_disable_unprepare(data->clk);
err_out_free_mdiobus:
	mdiobus_free(bus);
	return ret;
}

static int bsp_gemac_mdio_remove(struct platform_device *pdev)
{
	struct mii_bus *bus = platform_get_drvdata(pdev);
	struct bsp_gemac_mdio_data *data = bus->priv;

	mdiobus_unregister(bus);
	clk_disable_unprepare(data->clk);
	mdiobus_free(bus);

	return 0;
}

static const struct of_device_id bsp_gemac_mdio_dt_ids[] = {
	{ .compatible = "vendor,gemac-mdio" },
	{ }
};
MODULE_DEVICE_TABLE(of, bsp_gemac_mdio_dt_ids);

static struct platform_driver bsp_gemac_mdio_driver = {
	.probe = bsp_gemac_mdio_probe,
	.remove = bsp_gemac_mdio_remove,
	.driver = {
		.name = "bsp-gemac-mdio",
		.of_match_table = bsp_gemac_mdio_dt_ids,
	},
};

module_platform_driver(bsp_gemac_mdio_driver);

MODULE_DESCRIPTION("Gigabit Ethernet MAC MDIO interface driver");
MODULE_LICENSE("GPL v2");
