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
#include <linux/kernel.h>
#include <linux/reset.h>
#include "gmac.h"

void gmac_mac_core_reset(struct gmac_netdev_local *priv)
{
	/* undo reset */
	if (priv == NULL || priv->port_rst == NULL)
		return;
	reset_control_deassert(priv->port_rst);
	usleep_range(50, 60); /* wait 50~60us */

	/* soft reset mac port */
	reset_control_assert(priv->port_rst);
	usleep_range(50, 60); /* wait 50~60us */
	/* undo reset */
	reset_control_deassert(priv->port_rst);
}

void gmac_hw_internal_phy_reset(struct gmac_netdev_local *priv)
{
}

void gmac_hw_phy_reset(struct gmac_netdev_local *priv)
{
	if (priv == NULL)
		return;
	if (priv->internal_phy)
		gmac_hw_internal_phy_reset(priv);
	else
		gmac_hw_external_phy_reset(priv);
}

void gmac_hw_external_phy_reset(struct gmac_netdev_local *priv)
{
	if (priv == NULL)
		return;
	if (priv->phy_rst != NULL) {
		/* write 0 to cancel reset */
		reset_control_deassert(priv->phy_rst);
		msleep(50); /* wait 50ms */

		/* use CRG register to reset phy */
		/* RST_BIT, write 0 to reset phy, write 1 to cancel reset */
		reset_control_assert(priv->phy_rst);

		/*
		 * delay some time to ensure reset ok,
		 * this depends on PHY hardware feature
		 */
		msleep(50); /* wait 50ms */

		/* write 0 to cancel reset */
		reset_control_deassert(priv->phy_rst);
		/* delay some time to ensure later MDIO access */
		msleep(50); /* wait 50ms */
	}
}

void gmac_internal_phy_clk_disable(struct gmac_netdev_local const *priv)
{
}

void gmac_internal_phy_clk_enable(struct gmac_netdev_local const *priv)
{
}

void gmac_hw_all_clk_disable(struct gmac_netdev_local *priv)
{
	/*
	 * If macif clock is enabled when suspend, we should
	 * disable it here.
	 * Because when resume, PHY will link up again and
	 * macif clock will be enabled too. If we don't disable
	 * macif clock in suspend, macif clock will be enabled twice.
	 */
	if (priv == NULL || priv->clk == NULL || priv->netdev == NULL || priv->macif_clk == NULL)
		return;

	if (priv->netdev->flags & IFF_UP)
		clk_disable_unprepare(priv->macif_clk);

	/*
	 * This is called in suspend, when net device is down,
	 * MAC clk is disabled.
	 * So we need to judge whether MAC clk is enabled,
	 * otherwise kernel will WARNING if clk disable twice.
	 */
	if (priv->netdev->flags & IFF_UP)
		clk_disable_unprepare(priv->clk);

	if (priv->internal_phy)
		gmac_internal_phy_clk_disable(priv);
}

void gmac_hw_all_clk_enable(struct gmac_netdev_local *priv)
{
	if (priv == NULL || priv->netdev == NULL || priv->clk == NULL || priv->macif_clk == NULL)
		return;

	if (priv->internal_phy)
		gmac_internal_phy_clk_enable(priv);

	if (priv->netdev->flags & IFF_UP)
		clk_prepare_enable(priv->macif_clk);

	/* If net device is down when suspend, we should not enable MAC clk. */
	if (priv->netdev->flags & IFF_UP)
		clk_prepare_enable(priv->clk);
}
