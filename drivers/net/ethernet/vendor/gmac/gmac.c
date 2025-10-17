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

#include "gmac.h"
#include "util.h"
#include "autoeee/autoeee.h"
#include "sockioctl.h"
#include "pm.h"

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/unistd.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/mm.h>
#include <linux/mii.h>
#include <linux/ethtool.h>
#include <linux/phy.h>
#include <linux/dma-mapping.h>
#include <linux/workqueue.h>
#include <linux/device.h>
#include <linux/atomic.h>
#include <linux/platform_device.h>
#include <linux/capability.h>
#include <linux/time.h>
#include <asm/setup.h>
#include <linux/proc_fs.h>
#include <linux/module.h>

#include <linux/circ_buf.h>
#include <linux/of_net.h>
#include <linux/of_mdio.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/securec.h>

#define has_tso_cap(hw_cap)		((((hw_cap) >> 28) & 0x3) == VER_TSO)
#define has_rxhash_cap(hw_cap)		((hw_cap) & BIT(30))
#define has_rss_cap(hw_cap)		((hw_cap) & BIT(31))

#define DEFAULT_MSG_ENABLE (NETIF_MSG_DRV | NETIF_MSG_PROBE | NETIF_MSG_LINK)
static int debug = -1;
module_param(debug, int, 0000);
MODULE_PARM_DESC(debug, "Debug level (0=none,...,16=all)");

static void gmac_set_desc_depth(struct gmac_netdev_local const *priv,
		  u32 rx, u32 tx)
{
	u32 reg, val;
	int i;

	writel(BITS_RX_FQ_DEPTH_EN, priv->gmac_iobase + RX_FQ_REG_EN);
	val = readl(priv->gmac_iobase + RX_FQ_DEPTH);
	val &= ~Q_ADDR_HI8_MASK;
	val |= rx << DESC_WORD_SHIFT;
	writel(val, priv->gmac_iobase + RX_FQ_DEPTH);
	writel(0, priv->gmac_iobase + RX_FQ_REG_EN);

	writel(BITS_RX_BQ_DEPTH_EN, priv->gmac_iobase + RX_BQ_REG_EN);
	val = readl(priv->gmac_iobase + RX_BQ_DEPTH);
	val &= ~Q_ADDR_HI8_MASK;
	val |= rx << DESC_WORD_SHIFT;
	writel(val, priv->gmac_iobase + RX_BQ_DEPTH);
	for (i = 1; i < priv->num_rxqs; i++) {
		reg = rx_bq_depth_queue(i);
		val = readl(priv->gmac_iobase + reg);
		val &= ~Q_ADDR_HI8_MASK;
		val |= rx << DESC_WORD_SHIFT;
		writel(val, priv->gmac_iobase + reg);
	}
	writel(0, priv->gmac_iobase + RX_BQ_REG_EN);

	writel(BITS_TX_BQ_DEPTH_EN, priv->gmac_iobase + TX_BQ_REG_EN);
	val = readl(priv->gmac_iobase + TX_BQ_DEPTH);
	val &= ~Q_ADDR_HI8_MASK;
	val |= tx << DESC_WORD_SHIFT;
	writel(val, priv->gmac_iobase + TX_BQ_DEPTH);
	writel(0, priv->gmac_iobase + TX_BQ_REG_EN);

	writel(BITS_TX_RQ_DEPTH_EN, priv->gmac_iobase + TX_RQ_REG_EN);
	val = readl(priv->gmac_iobase + TX_RQ_DEPTH);
	val &= ~Q_ADDR_HI8_MASK;
	val |= tx << DESC_WORD_SHIFT;
	writel(val, priv->gmac_iobase + TX_RQ_DEPTH);
	writel(0, priv->gmac_iobase + TX_RQ_REG_EN);
}

static void gmac_set_rx_fq(struct gmac_netdev_local const *priv,
		  dma_addr_t phy_addr)
{
#if defined(CONFIG_GMAC_DDR_64BIT)
	u32 val;
#endif
	writel(BITS_RX_FQ_START_ADDR_EN, priv->gmac_iobase + RX_FQ_REG_EN);
#if defined(CONFIG_GMAC_DDR_64BIT)
	val = readl(priv->gmac_iobase + RX_FQ_DEPTH);
	val &= Q_ADDR_HI8_MASK;
	val |= (phy_addr >> REG_BIT_WIDTH) << Q_ADDR_HI8_OFFSET;
	writel(val, priv->gmac_iobase + RX_FQ_DEPTH);
#endif
	writel((u32)phy_addr, priv->gmac_iobase + RX_FQ_START_ADDR);
	writel(0, priv->gmac_iobase + RX_FQ_REG_EN);
}

static void gmac_set_rx_bq(struct gmac_netdev_local const *priv,
		  dma_addr_t phy_addr)
{
#if defined(CONFIG_GMAC_DDR_64BIT)
	u32 val;
#endif
	writel(BITS_RX_BQ_START_ADDR_EN, priv->gmac_iobase + RX_BQ_REG_EN);
#if defined(CONFIG_GMAC_DDR_64BIT)
	val = readl(priv->gmac_iobase + RX_BQ_DEPTH);
	val &= Q_ADDR_HI8_MASK;
	val |= (phy_addr >> REG_BIT_WIDTH) << Q_ADDR_HI8_OFFSET;
	writel(val, priv->gmac_iobase + RX_BQ_DEPTH);
#endif
	writel((u32)phy_addr, priv->gmac_iobase + RX_BQ_START_ADDR);
	writel(0, priv->gmac_iobase + RX_BQ_REG_EN);
}

static void gmac_set_tx_bq(struct gmac_netdev_local const *priv,
		  dma_addr_t phy_addr)
{
#if defined(CONFIG_GMAC_DDR_64BIT)
	u32 val;
#endif
	writel(BITS_TX_BQ_START_ADDR_EN, priv->gmac_iobase + TX_BQ_REG_EN);
#if defined(CONFIG_GMAC_DDR_64BIT)
	val = readl(priv->gmac_iobase + TX_BQ_DEPTH);
	val &= Q_ADDR_HI8_MASK;
	val |= (phy_addr >> REG_BIT_WIDTH) << Q_ADDR_HI8_OFFSET;
	writel(val, priv->gmac_iobase + TX_BQ_DEPTH);
#endif
	writel((u32)phy_addr, priv->gmac_iobase + TX_BQ_START_ADDR);
	writel(0, priv->gmac_iobase + TX_BQ_REG_EN);
}

static void gmac_set_tx_rq(struct gmac_netdev_local const *priv,
		  dma_addr_t phy_addr)
{
#if defined(CONFIG_GMAC_DDR_64BIT)
	u32 val;
#endif
	writel(BITS_TX_RQ_START_ADDR_EN, priv->gmac_iobase + TX_RQ_REG_EN);
#if defined(CONFIG_GMAC_DDR_64BIT)
	val = readl(priv->gmac_iobase + TX_RQ_DEPTH);
	val &= Q_ADDR_HI8_MASK;
	val |= (phy_addr >> REG_BIT_WIDTH) << Q_ADDR_HI8_OFFSET;
	writel(val, priv->gmac_iobase + TX_RQ_DEPTH);
#endif
	writel((u32)phy_addr, priv->gmac_iobase + TX_RQ_START_ADDR);
	writel(0, priv->gmac_iobase + TX_RQ_REG_EN);
}

static void gmac_hw_set_desc_addr(struct gmac_netdev_local const *priv)
{
	u32 reg;
	int i;
#if defined(CONFIG_GMAC_DDR_64BIT)
	u32 val;
#endif

	gmac_set_rx_fq(priv, priv->RX_FQ.phys_addr);
	gmac_set_rx_bq(priv, priv->RX_BQ.phys_addr);
	gmac_set_tx_rq(priv, priv->TX_RQ.phys_addr);
	gmac_set_tx_bq(priv, priv->TX_BQ.phys_addr);

	for (i = 1; i < priv->num_rxqs; i++) {
		reg = rx_bq_start_addr_queue(i);
		writel(BITS_RX_BQ_START_ADDR_EN,
		       priv->gmac_iobase + RX_BQ_REG_EN);
#if defined(CONFIG_GMAC_DDR_64BIT)
		val = readl(priv->gmac_iobase + reg);
		val &= Q_ADDR_HI8_MASK;
		val |= ((priv->pool[BASE_QUEUE_NUMS + i].phys_addr) >> REG_BIT_WIDTH) <<
		       Q_ADDR_HI8_OFFSET;
		writel(val, priv->gmac_iobase + reg);
#endif
		/* pool 3 add i */
		writel((u32)(priv->pool[BASE_QUEUE_NUMS + i].phys_addr),
		       priv->gmac_iobase + reg);
		writel(0, priv->gmac_iobase + RX_BQ_REG_EN);
	}
}

static void gmac_set_rss_cap(struct gmac_netdev_local const *priv)
{
	u32 val = 0;

	if (priv->has_rxhash_cap)
		val |= BIT_RXHASH_CAP;
	if (priv->has_rss_cap)
		val |= BIT_RSS_CAP;
	writel(val, priv->gmac_iobase + HW_CAP_EN);
}

/* config AXI bus burst and outstanding for better performance */
static void gmac_axi_bus_cfg(struct gmac_netdev_local *priv)
{
	if (!priv->axi_bus_cfg_base)
		return;
}

static void gmac_hw_init(struct gmac_netdev_local *priv)
{
	u32 val;
	u32 reg;
	int i;

	gmac_axi_bus_cfg(priv);

	/* disable and clear all interrupts */
	writel(0, priv->gmac_iobase + ENA_PMU_INT);
	writel(~0, priv->gmac_iobase + RAW_PMU_INT);

	for (i = 1; i < priv->num_rxqs; i++) {
		reg = rss_ena_int_queue(i);
		writel(0, priv->gmac_iobase + reg);
	}
	writel(~0, priv->gmac_iobase + RSS_RAW_PMU_INT);

	/* enable CRC erro packets filter */
	val = readl(priv->gmac_iobase + REC_FILT_CONTROL);
	val |= BIT_CRC_ERR_PASS;
	writel(val, priv->gmac_iobase + REC_FILT_CONTROL);

	/* set tx min packet length */
	val = readl(priv->gmac_iobase + CRF_MIN_PACKET);
	val &= ~BIT_MASK_TX_MIN_LEN;
	val |= ETH_HLEN << BIT_OFFSET_TX_MIN_LEN;
	writel(val, priv->gmac_iobase + CRF_MIN_PACKET);

	/* fix bug for udp and ip error check */
	writel(CONTROL_WORD_CONFIG, priv->gmac_iobase + CONTROL_WORD);

	writel(0, priv->gmac_iobase + COL_SLOT_TIME);

	writel(DUPLEX_HALF, priv->gmac_iobase + MAC_DUPLEX_HALF_CTRL);

	/* interrupt when rcv packets >= RX_BQ_INT_THRESHOLD */
	val = RX_BQ_INT_THRESHOLD |
		(TX_RQ_INT_THRESHOLD << BITS_OFFSET_TX_RQ_IN_TH);
	writel(val, priv->gmac_iobase + IN_QUEUE_TH);

	/* RX_BQ/TX_RQ in timeout threshold */
	writel(0x10000, priv->gmac_iobase + RX_BQ_IN_TIMEOUT_TH);

	writel(0x18000, priv->gmac_iobase + TX_RQ_IN_TIMEOUT_TH);

	gmac_set_desc_depth(priv, RX_DESC_NUM, TX_DESC_NUM);
}

static void gmac_irq_enable(struct gmac_netdev_local *ld)
{
	if (ld == NULL)
		return;
	writel(RX_BQ_IN_INT | RX_BQ_IN_TIMEOUT_INT
		| TX_RQ_IN_INT | TX_RQ_IN_TIMEOUT_INT,
		ld->gmac_iobase + ENA_PMU_INT);
}

static void gmac_irq_enable_queue(struct gmac_netdev_local *ld,
				    int rxq_id)
{
	if (rxq_id) {
		u32 reg;

		reg = rss_ena_int_queue(rxq_id);
		writel(~0, ld->gmac_iobase + reg);
	} else {
		gmac_irq_enable(ld);
	}
}

static void gmac_irq_enable_all_queue(struct gmac_netdev_local *ld)
{
	int i;
	if (ld == NULL)
		return;
	for (i = 0; i < ld->num_rxqs; i++)
		gmac_irq_enable_queue(ld, i);
}

static inline void gmac_irq_disable(struct gmac_netdev_local const *ld)
{
	if (ld == NULL)
		return;
	writel(0, ld->gmac_iobase + ENA_PMU_INT);
}

static void gmac_irq_disable_queue(struct gmac_netdev_local const *ld,
		  int rxq_id)
{
	if (rxq_id) {
		u32 reg;

		reg = rss_ena_int_queue(rxq_id);
		writel(0, ld->gmac_iobase + reg);
	} else {
		gmac_irq_disable(ld);
	}
}

static void gmac_irq_disable_all_queue(struct gmac_netdev_local const *ld)
{
	int i;
	if (ld == NULL)
		return;
	for (i = 0; i < ld->num_rxqs; i++)
		gmac_irq_disable_queue(ld, i);
}

static bool gmac_queue_irq_disabled(struct gmac_netdev_local *ld,
				      int rxq_id)
{
	u32 reg, val;

	if (rxq_id)
		reg = rss_ena_int_queue(rxq_id);
	else
		reg = ENA_PMU_INT;
	val = readl(ld->gmac_iobase + reg);

	return !val;
}

static inline void gmac_hw_desc_enable(struct gmac_netdev_local const *ld)
{
	if (ld == NULL)
		return;
	writel(0xF, ld->gmac_iobase + DESC_WR_RD_ENA);
}

static inline void gmac_hw_desc_disable(struct gmac_netdev_local const *ld)
{
	if (ld == NULL)
		return;
	writel(0, ld->gmac_iobase + DESC_WR_RD_ENA);
}

static inline void gmac_port_enable(struct gmac_netdev_local const *ld)
{
	if (ld == NULL)
		return;
	writel(BITS_TX_EN | BITS_RX_EN, ld->gmac_iobase + PORT_EN);
}

static inline void gmac_port_disable(struct gmac_netdev_local const *ld)
{
	if (ld != NULL)
		writel(0, ld->gmac_iobase + PORT_EN);
}

/* set gmac's multicast list, here we setup gmac's mc filter */
static void gmac_gmac_multicast_list(struct net_device const *dev)
{
	struct gmac_netdev_local *ld = netdev_priv(dev);
	unsigned int rec_filter;

	rec_filter = readl(ld->gmac_iobase + REC_FILT_CONTROL);
	/*
	 * when set gmac in promisc mode
	 * a. dev in IFF_PROMISC mode
	 */
	if ((dev->flags & IFF_PROMISC)) {
		/* promisc mode.received all pkgs. */
		rec_filter &= ~(BIT_BC_DROP_EN | BIT_MC_MATCH_EN |
				BIT_UC_MATCH_EN);
	} else {
		/* drop uc pkgs with field 'DA' not match our's */
		rec_filter |= BIT_UC_MATCH_EN;

		if (dev->flags & IFF_BROADCAST) /* no broadcast */
			rec_filter &= ~BIT_BC_DROP_EN;
		else
			rec_filter |= BIT_BC_DROP_EN;

		if (netdev_mc_empty(dev) || !(dev->flags & IFF_MULTICAST)) {
			/* haven't join any mc group */
			writel(0, ld->gmac_iobase + PORT_MC_ADDR_LOW);
			writel(0, ld->gmac_iobase + PORT_MC_ADDR_HIGH);
			rec_filter |= BIT_MC_MATCH_EN;
		} else if (netdev_mc_count(dev) == 1 &&
				(dev->flags & IFF_MULTICAST)) {
			struct netdev_hw_addr *ha = NULL;
			unsigned int d;

			netdev_for_each_mc_addr(ha, dev) {
				d = (ha->addr[0] << 8) | (ha->addr[1]); /* shift left 8bits */
				writel(d, ld->gmac_iobase + PORT_MC_ADDR_HIGH);
				/* addr2 3 shift left 24 16 bits */
				d = (ha->addr[2] << 24) | (ha->addr[3] << 16) |
				     (ha->addr[4] << 8) | (ha->addr[5]); /* a4 << 8 | a5 */
				writel(d, ld->gmac_iobase + PORT_MC_ADDR_LOW);
			}
			rec_filter |= BIT_MC_MATCH_EN;
		} else {
			rec_filter &= ~BIT_MC_MATCH_EN;
		}
	}
	writel(rec_filter, ld->gmac_iobase + REC_FILT_CONTROL);
}

/*
 * the func stop the hw desc and relaim the software skb resource
 * before reusing the gmac, you'd better reset the gmac
 */
void gmac_reclaim_rx_tx_resource(struct gmac_netdev_local *ld)
{
	unsigned long rxflags, txflags;
	int rd_offset, wr_offset;
	int i;
	if (ld == NULL)
		return;
	gmac_irq_disable_all_queue(ld);
	gmac_hw_desc_disable(ld);
	writel(STOP_RX_TX, ld->gmac_iobase + STOP_CMD);

	spin_lock_irqsave(&ld->rxlock, rxflags);
	/* RX_BQ: logic write pointer */
	wr_offset = readl(ld->gmac_iobase + RX_BQ_WR_ADDR);
	/* RX_BQ: software read pointer */
	rd_offset = readl(ld->gmac_iobase + RX_BQ_RD_ADDR);
	/* prevent to reclaim skb in rx bottom half */
	writel(wr_offset, ld->gmac_iobase + RX_BQ_RD_ADDR);

	for (i = 1; i < ld->num_rxqs; i++) {
		u32 rx_bq_wr_reg, rx_bq_rd_reg;

		rx_bq_wr_reg = rx_bq_wr_addr_queue(i);
		rx_bq_rd_reg = rx_bq_rd_addr_queue(i);

		wr_offset = readl(ld->gmac_iobase + rx_bq_wr_reg);
		writel(wr_offset, ld->gmac_iobase + rx_bq_rd_reg);
	}

	/* RX_FQ: software write pointer */
	wr_offset = readl(ld->gmac_iobase + RX_FQ_WR_ADDR);
	/* RX_FQ: logic read pointer */
	rd_offset = readl(ld->gmac_iobase + RX_FQ_RD_ADDR);
	if (!rd_offset)
		rd_offset = (RX_DESC_NUM - 1) << DESC_BYTE_SHIFT;
	else
		rd_offset -= DESC_SIZE;
	/* stop to feed hw desc */
	writel(rd_offset, ld->gmac_iobase + RX_FQ_WR_ADDR);

	for (i = 0; i < ld->RX_FQ.count; i++) {
		if (!ld->RX_FQ.skb[i])
			ld->RX_FQ.skb[i] = SKB_MAGIC;
	}
	spin_unlock_irqrestore(&ld->rxlock, rxflags);

	/*
	 * no need to wait pkts in TX_RQ finish to free all skb,
	 * because gmac_xmit_reclaim is in the tx_lock,
	 */
	spin_lock_irqsave(&ld->txlock, txflags);
	/* TX_RQ: logic write */
	wr_offset = readl(ld->gmac_iobase + TX_RQ_WR_ADDR);
	/* TX_RQ: software read */
	rd_offset = readl(ld->gmac_iobase + TX_RQ_RD_ADDR);
	/* stop to reclaim tx skb */
	writel(wr_offset, ld->gmac_iobase + TX_RQ_RD_ADDR);

	/* TX_BQ: logic read */
	rd_offset = readl(ld->gmac_iobase + TX_BQ_RD_ADDR);
	if (!rd_offset)
		rd_offset = (TX_DESC_NUM - 1) << DESC_BYTE_SHIFT;
	else
		rd_offset -= DESC_SIZE;
	/* stop software tx skb */
	writel(rd_offset, ld->gmac_iobase + TX_BQ_WR_ADDR);

	for (i = 0; i < ld->TX_BQ.count; i++) {
		if (!ld->TX_BQ.skb[i])
			ld->TX_BQ.skb[i] = SKB_MAGIC;
	}
	spin_unlock_irqrestore(&ld->txlock, txflags);
}

static void gmac_monitor_func(struct timer_list *t);
static void gmac_set_multicast_list(struct net_device *dev);

static void gmac_hw_set_mac_addr(struct net_device *dev)
{
	struct gmac_netdev_local *priv = netdev_priv(dev);
	unsigned char *mac = dev->dev_addr;
	u32 val;

	val = mac[1] | (mac[0] << 8); /* shift left 8 bits */
	writel(val, priv->gmac_iobase + STATION_ADDR_HIGH);
	/* mac 2 3 4 5 shift left 24 16 8 0 bits */
	val = mac[5] | (mac[4] << 8) | (mac[3] << 16) | (mac[2] << 24);
	writel(val, priv->gmac_iobase + STATION_ADDR_LOW);
}

static u32 gmac_rx_refill(struct gmac_netdev_local *priv);

static void gmac_free_rx_skb(struct gmac_netdev_local *ld)
{
	struct sk_buff *skb = NULL;
	int i;

	for (i = 0; i < ld->RX_FQ.count; i++) {
		skb = ld->RX_FQ.skb[i];
		if (skb != NULL) {
			ld->rx_skb[i] = NULL;
			ld->RX_FQ.skb[i] = NULL;
			if (skb == SKB_MAGIC)
				continue;
			dev_kfree_skb_any(skb);
			/*
			 * need to unmap the skb here
			 * but there is no way to get the dma_addr here,
			 * and unmap(TO_DEVICE) ops do nothing in fact,
			 * so we ignore to call
			 * dma_unmap_single(dev, dma_addr, skb->len,
			 *      DMA_TO_DEVICE)
			 */
		}
	}
}

static void gmac_free_tx_skb(struct gmac_netdev_local *ld)
{
	struct sk_buff *skb = NULL;
	int i;

	for (i = 0; i < ld->TX_BQ.count; i++) {
		skb = ld->TX_BQ.skb[i];
		if (skb != NULL) {
			ld->tx_skb[i] = NULL;
			ld->TX_BQ.skb[i] = NULL;
			if (skb == SKB_MAGIC)
				continue;
			dev_kfree_skb_any(skb);
			/* unmap the skb */
		}
	}
}

/* reset and re-config gmac */
void gmac_restart(struct gmac_netdev_local *ld)
{
	unsigned long rxflags, txflags;
	if (ld == NULL || ld->netdev == NULL)
		return;
	/* restart hw engine now */
	gmac_mac_core_reset(ld);

	spin_lock_irqsave(&ld->rxlock, rxflags);
	spin_lock_irqsave(&ld->txlock, txflags);

	gmac_free_rx_skb(ld);
	gmac_free_tx_skb(ld);

	pmt_reg_restore(ld);
	gmac_hw_init(ld);
	gmac_hw_set_mac_addr(ld->netdev);
	gmac_hw_set_desc_addr(ld);

	/* we don't set macif here, it will be set in adjust_link */
	if (ld->netdev->flags & IFF_UP) {
		/*
		 * when resume, only do the following operations
		 * when dev is up before suspend.
		 */
		gmac_rx_refill(ld);
		gmac_set_multicast_list(ld->netdev);

		gmac_hw_desc_enable(ld);
		gmac_port_enable(ld);
		gmac_irq_enable_all_queue(ld);
	}
	spin_unlock_irqrestore(&ld->txlock, txflags);
	spin_unlock_irqrestore(&ld->rxlock, rxflags);
}

static int gmac_net_set_mac_address(struct net_device *dev, void *p)
{
	int ret;

	ret = eth_mac_addr(dev, p);
	if (!ret)
		gmac_hw_set_mac_addr(dev);

	return ret;
}

#define GMAC_LINK_CHANGE_PROTECT
#define GMAC_MAC_TX_RESET_IN_LINKUP

#ifdef GMAC_LINK_CHANGE_PROTECT
#define GMAC_MS_TO_NS 1000000ULL
#define GMAC_FLUSH_WAIT_TIME (100*GMAC_MS_TO_NS)
/* protect code */
static void gmac_linkup_flush(struct gmac_netdev_local *ld)
{
	int tx_bq_wr_offset, tx_bq_rd_offset;
	unsigned long long time_limit, time_now;

	time_now = sched_clock();
	time_limit = time_now + GMAC_FLUSH_WAIT_TIME;

	do {
		tx_bq_wr_offset = readl(ld->gmac_iobase + TX_BQ_WR_ADDR);
		tx_bq_rd_offset = readl(ld->gmac_iobase + TX_BQ_RD_ADDR);

		time_now = sched_clock();
		if (unlikely((long long)time_now -
				(long long)time_limit >= 0))
			break;
	} while (tx_bq_rd_offset != tx_bq_wr_offset);

	mdelay(1);
}
#endif

#ifdef GMAC_MAC_TX_RESET_IN_LINKUP
static void gmac_mac_tx_state_engine_reset(struct gmac_netdev_local *priv)
{
	u32 val;

	val = readl(priv->gmac_iobase + MAC_CLEAR);
	val |= BIT_TX_SOFT_RESET;
	writel(val, priv->gmac_iobase + MAC_CLEAR);

	mdelay(5); /* wait 5ms */

	val = readl(priv->gmac_iobase + MAC_CLEAR);
	val &= ~BIT_TX_SOFT_RESET;
	writel(val, priv->gmac_iobase + MAC_CLEAR);
}
#endif

static void gmac_adjust_link(struct net_device *dev)
{
	struct gmac_netdev_local *priv = NULL;
	struct phy_device *phy = NULL;
	bool link_status_changed = false;
	if (dev == NULL)
		return;
	priv = netdev_priv(dev);
	if (priv == NULL || priv->phy == NULL)
		return;
	phy = priv->phy;
	if (phy->link) {
		if ((priv->old_speed != phy->speed) ||
				(priv->old_duplex != phy->duplex)) {
#ifdef GMAC_LINK_CHANGE_PROTECT
			unsigned long txflags;

			spin_lock_irqsave(&priv->txlock, txflags);

			gmac_linkup_flush(priv);
#endif
			gmac_config_port(dev, phy->speed, phy->duplex);
#ifdef GMAC_MAC_TX_RESET_IN_LINKUP
			gmac_mac_tx_state_engine_reset(priv);
#endif
#ifdef GMAC_LINK_CHANGE_PROTECT
			spin_unlock_irqrestore(&priv->txlock, txflags);
#endif
			gmac_set_flow_ctrl_state(priv, phy->pause);

			if (priv->autoeee)
				init_autoeee(priv);

			link_status_changed = true;
			priv->old_link = 1;
			priv->old_speed = phy->speed;
			priv->old_duplex = phy->duplex;
		}
	} else if (priv->old_link) {
		link_status_changed = true;
		priv->old_link = 0;
		priv->old_speed = SPEED_UNKNOWN;
		priv->old_duplex = DUPLEX_UNKNOWN;
	}

	if (link_status_changed && netif_msg_link(priv))
		phy_print_status(phy);
}

int gmac_tx_avail(struct gmac_netdev_local const *ld)
{
	unsigned int tx_bq_wr_offset, tx_bq_rd_offset;
	if (ld == NULL)
		return -ENOMEM;
	tx_bq_wr_offset = readl(ld->gmac_iobase + TX_BQ_WR_ADDR);
	tx_bq_rd_offset = readl(ld->gmac_iobase + TX_BQ_RD_ADDR);

	return (tx_bq_rd_offset >> DESC_BYTE_SHIFT) + TX_DESC_NUM -
		(tx_bq_wr_offset >> DESC_BYTE_SHIFT) - 1;
}

static int gmac_init_sg_desc_queue(struct gmac_netdev_local *ld)
{
	ld->sg_count = ld->TX_BQ.count + GMAC_SG_DESC_ADD;
	if (has_cap_cci(ld->hw_cap)) {
		ld->dma_sg_desc = kmalloc_array(ld->sg_count,
						sizeof(struct sg_desc),
						GFP_KERNEL);
		if (ld->dma_sg_desc)
			ld->dma_sg_phy = virt_to_phys(ld->dma_sg_desc);
	} else {
		ld->dma_sg_desc = (struct sg_desc *)dma_alloc_coherent(ld->dev,
				  ld->sg_count * sizeof(struct sg_desc),
				  &ld->dma_sg_phy, GFP_KERNEL);
	}

	if (!ld->dma_sg_desc) {
		pr_err("alloc sg desc dma error!\n");
		return -ENOMEM;
	}
#ifdef GMAC_TSO_DEBUG
	pr_info("dma_sg_phy: 0x%pK\n", (void *)(uintptr_t)ld->dma_sg_phy);
#endif

	ld->sg_head = 0;
	ld->sg_tail = 0;

	return 0;
}

static void gmac_destroy_sg_desc_queue(struct gmac_netdev_local *ld)
{
	if (ld->dma_sg_desc) {
		if (has_cap_cci(ld->hw_cap))
			kfree(ld->dma_sg_desc);
		else
			dma_free_coherent(ld->dev,
					  ld->sg_count * sizeof(struct sg_desc),
					  ld->dma_sg_desc, ld->dma_sg_phy);
		ld->dma_sg_desc = NULL;
	}
}

static bool gmac_rx_fq_empty(struct gmac_netdev_local const *priv)
{
	u32 start, end;

	start = readl(priv->gmac_iobase + RX_FQ_WR_ADDR);
	end = readl(priv->gmac_iobase + RX_FQ_RD_ADDR);
	if (start == end)
		return true;
	else
		return false;
}

static bool gmac_rxq_has_packets(struct gmac_netdev_local const *priv, int rxq_id)
{
	u32 rx_bq_rd_reg, rx_bq_wr_reg;
	u32 start, end;

	rx_bq_rd_reg = rx_bq_rd_addr_queue(rxq_id);
	rx_bq_wr_reg = rx_bq_wr_addr_queue(rxq_id);

	start = readl(priv->gmac_iobase + rx_bq_rd_reg);
	end = readl(priv->gmac_iobase + rx_bq_wr_reg);
	if (start == end)
		return false;
	else
		return true;
}

static void gmac_monitor_func(struct timer_list *t)
{
	struct gmac_netdev_local *ld = from_timer(ld, t, monitor);
	struct net_device *dev = NULL;
	u32 refill_cnt;

	if (ld == NULL) {
		gmac_trace(GMAC_NORMAL_LEVEL, "ld is null");
		return;
	}

	if (ld->netdev == NULL) {
		gmac_trace(GMAC_NORMAL_LEVEL, "ld->netdev is null");
		return;
	}
	dev_hold(ld->netdev);
	dev = ld->netdev;
	if (!netif_running(dev)) {
		dev_put(dev);
		gmac_trace(GMAC_NORMAL_LEVEL, "network driver is stopped");
		return;
	}
	dev_put(dev);

	spin_lock(&ld->rxlock);
	refill_cnt = gmac_rx_refill(ld);
	if (!refill_cnt && gmac_rx_fq_empty(ld)) {
		int rxq_id;

		for (rxq_id = 0; rxq_id < ld->num_rxqs; rxq_id++) {
			if (gmac_rxq_has_packets(ld, rxq_id))
				napi_schedule(&ld->q_napi[rxq_id].napi);
		}
	}
	spin_unlock(&ld->rxlock);

	ld->monitor.expires = jiffies + GMAC_MONITOR_TIMER;
	mod_timer(&ld->monitor, ld->monitor.expires);
}

static u32 gmac_rx_refill(struct gmac_netdev_local *priv)
{
	struct gmac_desc *desc = NULL;
	struct sk_buff *skb = NULL;
	struct cyclic_queue_info dma_info;
	u32 len = ETH_MAX_FRAME_SIZE;
	dma_addr_t addr;
	u32 refill_cnt = 0;
	u32 i;
	/* software write pointer */
	dma_info.start = dma_cnt(readl(priv->gmac_iobase + RX_FQ_WR_ADDR));
	/* logic read pointer */
	dma_info.end = dma_cnt(readl(priv->gmac_iobase + RX_FQ_RD_ADDR));
	dma_info.num = CIRC_SPACE(dma_info.start, dma_info.end, RX_DESC_NUM);

	for (i = 0, dma_info.pos = dma_info.start; i < dma_info.num; i++) {
		if (priv->RX_FQ.skb[dma_info.pos] || priv->rx_skb[dma_info.pos])
			break;

		skb = netdev_alloc_skb_ip_align(priv->netdev, len);
		if (unlikely(skb == NULL))
			break;

		if (!has_cap_cci(priv->hw_cap)) {
			addr = dma_map_single(priv->dev, skb->data, len,
					      DMA_FROM_DEVICE);
			if (dma_mapping_error(priv->dev, addr)) {
				dev_kfree_skb_any(skb);
				break;
			}
		} else {
			addr = virt_to_phys(skb->data);
		}

		desc = priv->RX_FQ.desc + dma_info.pos;
		desc->data_buff_addr = (u32)addr;
#if defined(CONFIG_GMAC_DDR_64BIT)
		desc->reserve31 = addr >> REG_BIT_WIDTH;
#endif
		priv->RX_FQ.skb[dma_info.pos] = skb;
		priv->rx_skb[dma_info.pos] = skb;

		desc->buffer_len = len - 1;
		desc->data_len = 0;
		desc->fl = 0;
		desc->descvid = DESC_VLD_FREE;
		desc->skb_id = dma_info.pos;

		refill_cnt++;
		dma_info.pos = dma_ring_incr(dma_info.pos, RX_DESC_NUM);
	}

	/*
	 * This barrier is important here.  It is required to ensure
	 * the ARM CPU flushes it's DMA write buffers before proceeding
	 * to the next instruction, to ensure that GMAC will see
	 * our descriptor changes in memory
	 */
	gmac_sync_barrier();

	if (dma_info.pos != dma_info.start)
		writel(dma_byte(dma_info.pos), priv->gmac_iobase + RX_FQ_WR_ADDR);

	return refill_cnt;
}

static void gmac_rx_skbput(struct net_device *dev, struct sk_buff *skb,
			     struct gmac_desc *desc, int rxq_id)
{
	struct gmac_netdev_local *ld = netdev_priv(dev);
	dma_addr_t addr;
	u32 len;
	int ret;

	len = desc->data_len;

	if (!has_cap_cci(ld->hw_cap)) {
		addr = desc->data_buff_addr;
#if defined(CONFIG_GMAC_DDR_64BIT)
		addr |= (dma_addr_t)(desc->reserve31) << REG_BIT_WIDTH;
#endif
		dma_unmap_single(ld->dev, addr, ETH_MAX_FRAME_SIZE,
				 DMA_FROM_DEVICE);
	}

	skb_put(skb, len);
	if (skb->len > ETH_MAX_FRAME_SIZE) {
		netdev_err(dev, "rcv len err, len = %d\n", skb->len);
		dev->stats.rx_errors++;
		dev->stats.rx_length_errors++;
		dev_kfree_skb_any(skb);
		return;
	}

	skb->protocol = eth_type_trans(skb, dev);
	skb->ip_summed = CHECKSUM_NONE;

#if defined(CONFIG_GMAC_RXCSUM)
	ret = gmac_rx_checksum(dev, skb, desc);
	if (unlikely(ret))
		return;
#endif
	if ((dev->features & NETIF_F_RXHASH) && desc->has_hash)
		skb_set_hash(skb, desc->rxhash, desc->l3_hash ?
			     PKT_HASH_TYPE_L3 : PKT_HASH_TYPE_L4);

	skb_record_rx_queue(skb, rxq_id);

	napi_gro_receive(&ld->q_napi[rxq_id].napi, skb);
	dev->stats.rx_packets++;
	dev->stats.rx_bytes += len;
}

static int gmac_rx_skb(struct net_device *dev, struct gmac_desc *desc,
			 u16 skb_id, int rxq_id)
{
	struct gmac_netdev_local *ld = netdev_priv(dev);
	struct sk_buff *skb = NULL;

	spin_lock(&ld->rxlock);
	skb = ld->rx_skb[skb_id];
	if (unlikely(skb == NULL)) {
		spin_unlock(&ld->rxlock);
		netdev_err(dev, "inconsistent rx_skb\n");
		return -1;
	}

	/* data consistent check */
	if (unlikely(skb != ld->RX_FQ.skb[skb_id])) {
		netdev_err(dev, "desc->skb(0x%p),RX_FQ.skb[%d](0x%p)\n",
			   skb, skb_id, ld->RX_FQ.skb[skb_id]);
		if (ld->RX_FQ.skb[skb_id] == SKB_MAGIC) {
			spin_unlock(&ld->rxlock);
			return 0;
		}
		WARN_ON(1);
	} else {
		ld->RX_FQ.skb[skb_id] = NULL;
	}
	spin_unlock(&ld->rxlock);

	gmac_rx_skbput(dev, skb, desc, rxq_id);
	return 0;
}

static int gmac_rx(struct net_device *dev, int limit, int rxq_id)
{
	struct gmac_netdev_local *ld = netdev_priv(dev);
	struct gmac_desc *desc = NULL;
	struct cyclic_queue_info dma_info;
	u32 rx_bq_rd_reg, rx_bq_wr_reg;
	u16 skb_id;
	u32 i;

	rx_bq_rd_reg = rx_bq_rd_addr_queue(rxq_id);
	rx_bq_wr_reg = rx_bq_wr_addr_queue(rxq_id);

	/* software read pointer */
	dma_info.start = dma_cnt(readl(ld->gmac_iobase + rx_bq_rd_reg));
	/* logic write pointer */
	dma_info.end = dma_cnt(readl(ld->gmac_iobase + rx_bq_wr_reg));
	dma_info.num = CIRC_CNT(dma_info.end, dma_info.start, RX_DESC_NUM);
	if (dma_info.num > limit)
		dma_info.num = limit;

	/* ensure get updated desc */
	rmb();
	for (i = 0, dma_info.pos = dma_info.start; i < dma_info.num; i++) {
		if (rxq_id)
			desc = ld->pool[BASE_QUEUE_NUMS + rxq_id].desc + dma_info.pos;
		else
			desc = ld->RX_BQ.desc + dma_info.pos;
		skb_id = desc->skb_id;

		if (unlikely(gmac_rx_skb(dev, desc, skb_id, rxq_id)))
			break;

		spin_lock(&ld->rxlock);
		ld->rx_skb[skb_id] = NULL;
		spin_unlock(&ld->rxlock);
		dma_info.pos = dma_ring_incr(dma_info.pos, RX_DESC_NUM);
	}

	if (dma_info.pos != dma_info.start)
		writel(dma_byte(dma_info.pos), ld->gmac_iobase + rx_bq_rd_reg);

	spin_lock(&ld->rxlock);
	gmac_rx_refill(ld);
	spin_unlock(&ld->rxlock);

	return dma_info.num;
}

#ifdef GMAC_TSO_DEBUG
unsigned int id_send;
unsigned int id_free;
struct send_pkt_info pkt_rec[MAX_RECORD];
#endif

static int gmac_check_tx_err(struct gmac_netdev_local *ld,
			       struct gmac_tso_desc *tx_bq_desc, unsigned int desc_pos)
{
	unsigned int tx_err = tx_bq_desc->tx_err;

	if (unlikely(tx_err & ERR_ALL)) {
		struct sg_desc *desc_cur = NULL;
		int *sg_word = NULL;
		int i;

		WARN((tx_err & ERR_ALL),
		     "TX ERR: desc1=0x%x, desc2=0x%x, desc5=0x%x\n",
		     tx_bq_desc->data_buff_addr,
		     tx_bq_desc->desc1.val, tx_bq_desc->tx_err);

		desc_cur = ld->dma_sg_desc + ld->TX_BQ.sg_desc_offset[desc_pos];
		sg_word = (int *)desc_cur;
		for (i = 0; i < sizeof(struct sg_desc) / sizeof(int); i++)
			pr_err("%s,%d: sg_desc word[%d]=0x%x\n",
			       __func__, __LINE__, i, sg_word[i]);

		return -1;
	}

	return 0;
}

static void gmac_xmit_release_gso_sg(struct gmac_netdev_local *ld,
				       struct gmac_tso_desc *tx_rq_desc, unsigned int desc_pos)
{
	struct sg_desc *desc_cur = NULL;
	int nfrags = tx_rq_desc->desc1.tx.nfrags_num;
	unsigned int desc_offset;
	dma_addr_t addr;
	size_t len;
	int i;

	desc_offset = ld->TX_BQ.sg_desc_offset[desc_pos];
	WARN_ON(desc_offset != ld->sg_tail);
	desc_cur = ld->dma_sg_desc + desc_offset;

	addr = desc_cur->linear_addr;
#if defined(CONFIG_GMAC_DDR_64BIT)
	addr |= (dma_addr_t)(desc_cur->reserv3 >>
			     SG_DESC_HI8_OFFSET) <<
		REG_BIT_WIDTH;
#endif
	len = desc_cur->linear_len;
	dma_unmap_single(ld->dev, addr, len, DMA_TO_DEVICE);
	for (i = 0; i < nfrags; i++) {
		addr = desc_cur->frags[i].addr;
#if defined(CONFIG_GMAC_DDR_64BIT)
		addr |= (dma_addr_t)
			(desc_cur->frags[i].reserved >>
			 SG_DESC_HI8_OFFSET) <<
			REG_BIT_WIDTH;
#endif
		len = desc_cur->frags[i].size;
		dma_unmap_page(ld->dev, addr, len,
			       DMA_TO_DEVICE);
	}
}

static int gmac_xmit_release_gso(struct gmac_netdev_local *ld,
				   struct gmac_tso_desc *tx_rq_desc, unsigned int desc_pos)
{
	int pkt_type;
	int nfrags = tx_rq_desc->desc1.tx.nfrags_num;
	dma_addr_t addr;
	size_t len;

	if (unlikely(gmac_check_tx_err(ld, tx_rq_desc, desc_pos) < 0)) {
		/* dev_close */
		gmac_irq_disable_all_queue(ld);
		gmac_hw_desc_disable(ld);

		netif_carrier_off(ld->netdev);
		netif_stop_queue(ld->netdev);

		phy_stop(ld->phy);
		del_timer_sync(&ld->monitor);
		return -1;
	}

	if (tx_rq_desc->desc1.tx.tso_flag || nfrags)
		pkt_type = PKT_SG;
	else
		pkt_type = PKT_NORMAL;

	if (pkt_type == PKT_NORMAL) {
		if (!has_cap_cci(ld->hw_cap)) {
			addr = tx_rq_desc->data_buff_addr;
#if defined(CONFIG_GMAC_DDR_64BIT)
			addr |= (dma_addr_t)(tx_rq_desc->reserve_desc2 &
					     TX_DESC_HI8_MASK) <<
				REG_BIT_WIDTH;
#endif
			len = tx_rq_desc->desc1.tx.data_len;
			dma_unmap_single(ld->dev, addr, len, DMA_TO_DEVICE);
		}
	} else {
		if (!has_cap_cci(ld->hw_cap))
			gmac_xmit_release_gso_sg(ld, tx_rq_desc, desc_pos);

		ld->sg_tail = (ld->sg_tail + 1) % ld->sg_count;
	}

#ifdef GMAC_TSO_DEBUG
	if (id_free >= MAX_RECORD)
		id_free = 0;
	pkt_rec[id_free].status = 0;
	id_free++;
#endif

	return 0;
}

static int gmac_xmit_reclaim_release(struct net_device *dev,
				       struct sk_buff *skb, struct gmac_desc *desc, u32 pos)
{
	struct gmac_netdev_local *priv = netdev_priv(dev);
	struct gmac_tso_desc *tso_desc = NULL;
	dma_addr_t addr;

	if (priv->tso_supported) {
		tso_desc = (struct gmac_tso_desc *)desc;
		return gmac_xmit_release_gso(priv, tso_desc, pos);
	} else if (!has_cap_cci(priv->hw_cap)) {
		addr = desc->data_buff_addr;
#if defined(CONFIG_GMAC_DDR_64BIT)
		addr |= (dma_addr_t)(desc->rxhash & TX_DESC_HI8_MASK) <<
			REG_BIT_WIDTH;
#endif
		dma_unmap_single(priv->dev, addr, skb->len, DMA_TO_DEVICE);
	}
	return 0;
}

static void gmac_xmit_reclaim(struct net_device *dev)
{
	struct sk_buff *skb = NULL;
	struct gmac_desc *desc = NULL;
	struct gmac_netdev_local *priv = netdev_priv(dev);
	unsigned int bytes_compl = 0;
	unsigned int pkts_compl = 0;
	struct cyclic_queue_info dma_info;
	u32 i;

	spin_lock(&priv->txlock);

	/* software read */
	dma_info.start = dma_cnt(readl(priv->gmac_iobase + TX_RQ_RD_ADDR));
	/* logic write */
	dma_info.end = dma_cnt(readl(priv->gmac_iobase + TX_RQ_WR_ADDR));
	dma_info.num = CIRC_CNT(dma_info.end, dma_info.start, TX_DESC_NUM);

	for (i = 0, dma_info.pos = dma_info.start; i < dma_info.num; i++) {
		skb = priv->tx_skb[dma_info.pos];
		if (unlikely(skb == NULL)) {
			netdev_err(dev, "inconsistent tx_skb\n");
			break;
		}

		if (skb != priv->TX_BQ.skb[dma_info.pos]) {
			netdev_err(dev, "wired, tx skb[%d](%p) != skb(%p)\n",
				   dma_info.pos, priv->TX_BQ.skb[dma_info.pos], skb);
			if (priv->TX_BQ.skb[dma_info.pos] == SKB_MAGIC)
				goto next;
		}

		pkts_compl++;
		bytes_compl += skb->len;
		desc = priv->TX_RQ.desc + dma_info.pos;
		if (gmac_xmit_reclaim_release(dev, skb, desc, dma_info.pos) < 0)
			break;

		priv->TX_BQ.skb[dma_info.pos] = NULL;
next:
		priv->tx_skb[dma_info.pos] = NULL;
		dev_consume_skb_any(skb);
		dma_info.pos = dma_ring_incr(dma_info.pos, TX_DESC_NUM);
	}

	if (dma_info.pos != dma_info.start)
		writel(dma_byte(dma_info.pos), priv->gmac_iobase + TX_RQ_RD_ADDR);

	if (pkts_compl || bytes_compl)
		netdev_completed_queue(dev, pkts_compl, bytes_compl);

	if (unlikely(netif_queue_stopped(priv->netdev)) && pkts_compl)
		netif_wake_queue(priv->netdev);

	spin_unlock(&priv->txlock);
}

static int gmac_poll(struct napi_struct *napi, int budget)
{
	struct gmac_napi *q_napi = container_of(napi,
						  struct gmac_napi, napi);
	struct gmac_netdev_local *priv = q_napi->ndev_priv;
	int work_done = 0;
	int num;
	u32 ints;
	u32 raw_int_reg, raw_int_mask;

	dev_hold(priv->netdev);
	if (q_napi->rxq_id) {
		raw_int_reg = RSS_RAW_PMU_INT;
		raw_int_mask = def_int_mask_queue((u32)q_napi->rxq_id);
	} else {
		raw_int_reg = RAW_PMU_INT;
		raw_int_mask = DEF_INT_MASK;
	}

	do {
		if (!q_napi->rxq_id)
			gmac_xmit_reclaim(priv->netdev);
		num = gmac_rx(priv->netdev, budget - work_done, q_napi->rxq_id);
		work_done += num;
		if (work_done >= budget)
			break;

		ints = readl(priv->gmac_iobase + raw_int_reg);
		ints &= raw_int_mask;
		writel(ints, priv->gmac_iobase + raw_int_reg);
	} while (ints || gmac_rxq_has_packets(priv, q_napi->rxq_id));

	if (work_done < budget) {
		napi_complete(napi);
		gmac_irq_enable_queue(priv, q_napi->rxq_id);
	}

	dev_put(priv->netdev);
	return work_done;
}

static irqreturn_t gmac_interrupt(int irq, void *dev_id)
{
	struct gmac_napi *q_napi = (struct gmac_napi *)dev_id;
	struct gmac_netdev_local *ld = q_napi->ndev_priv;
	u32 ints;
	u32 raw_int_reg, raw_int_mask;

	if (gmac_queue_irq_disabled(ld, q_napi->rxq_id))
		return IRQ_NONE;

	if (q_napi->rxq_id) {
		raw_int_reg = RSS_RAW_PMU_INT;
		raw_int_mask = def_int_mask_queue((u32)q_napi->rxq_id);
	} else {
		raw_int_reg = RAW_PMU_INT;
		raw_int_mask = DEF_INT_MASK;
	}

	ints = readl(ld->gmac_iobase + raw_int_reg);
	ints &= raw_int_mask;
	writel(ints, ld->gmac_iobase + raw_int_reg);

	if (likely(ints || gmac_rxq_has_packets(ld, q_napi->rxq_id))) {
		gmac_irq_disable_queue(ld, q_napi->rxq_id);
		napi_schedule(&q_napi->napi);
	}

	return IRQ_HANDLED;
}

static int gmac_xmit_gso_sg_frag(struct gmac_netdev_local *ld,
				   struct sk_buff *skb, struct sg_desc *desc_cur,
				   struct gmac_tso_desc *tx_bq_desc, unsigned int desc_pos)
{
	int nfrags = skb_shinfo(skb)->nr_frags;
	dma_addr_t addr;
	dma_addr_t dma_addr;
	phys_addr_t phys_addr;
	int i, ret;

	for (i = 0; i < nfrags; i++) {
		skb_frag_t *frag = &skb_shinfo(skb)->frags[i];
		int len = frag->size;

		if (!has_cap_cci(ld->hw_cap)) {
			dma_addr = skb_frag_dma_map(ld->dev, frag, 0,
						    len, DMA_TO_DEVICE);
			ret = dma_mapping_error(ld->dev, dma_addr);
			if (unlikely(ret)) {
				pr_err("skb frag DMA Mapping fail");
				return -EFAULT;
			}
			desc_cur->frags[i].addr = (u32)dma_addr;
#if defined(CONFIG_GMAC_DDR_64BIT)
			desc_cur->frags[i].reserved =
				(dma_addr >> REG_BIT_WIDTH) <<
				SG_DESC_HI8_OFFSET;
#endif
		} else {
			phys_addr =
				page_to_phys(skb_frag_page(frag)) + frag->page_offset;
			desc_cur->frags[i].addr = (u32)phys_addr;
#if defined(CONFIG_GMAC_DDR_64BIT)
			desc_cur->frags[i].reserved =
				(phys_addr >> REG_BIT_WIDTH) <<
				SG_DESC_HI8_OFFSET;
#endif
		}
		desc_cur->frags[i].size = len;
	}

	addr = ld->dma_sg_phy + ld->sg_head * sizeof(struct sg_desc);
	tx_bq_desc->data_buff_addr = (u32)addr;
#if defined(CONFIG_GMAC_DDR_64BIT)
	tx_bq_desc->reserve_desc2 = (addr >> REG_BIT_WIDTH) &
				    TX_DESC_HI8_MASK;
#endif
	ld->TX_BQ.sg_desc_offset[desc_pos] = ld->sg_head;

	ld->sg_head = (ld->sg_head + 1) % ld->sg_count;

	return 0;
}

static int gmac_xmit_gso_sg(struct gmac_netdev_local *ld,
			      struct sk_buff *skb,
			      struct gmac_tso_desc *tx_bq_desc, unsigned int desc_pos)
{
	struct sg_desc *desc_cur = NULL;
	dma_addr_t dma_addr;
	phys_addr_t phys_addr;
	int ret;

	if (unlikely(((ld->sg_head + 1) % ld->sg_count) == ld->sg_tail)) {
		/* SG pkt, but sg desc all used */
		pr_err("WARNING: sg desc all used.\n");
		return -EBUSY;
	}

	desc_cur = ld->dma_sg_desc + ld->sg_head;

	/* deal with ipv6_id */
	if (tx_bq_desc->desc1.tx.tso_flag &&
			tx_bq_desc->desc1.tx.ip_ver == PKT_IPV6 &&
			tx_bq_desc->desc1.tx.prot_type == PKT_UDP)
		desc_cur->ipv6_id = ntohl(skb_shinfo(skb)->ip6_frag_id);

	desc_cur->total_len = skb->len;
	desc_cur->linear_len = skb_headlen(skb);
	if (!has_cap_cci(ld->hw_cap)) {
		dma_addr = dma_map_single(ld->dev, skb->data,
					  desc_cur->linear_len,
					  DMA_TO_DEVICE);
		ret = dma_mapping_error(ld->dev, dma_addr);
		if (unlikely(ret)) {
			pr_err("DMA Mapping fail");
			return -EFAULT;
		}
		desc_cur->linear_addr = (u32)dma_addr;
#if defined(CONFIG_GMAC_DDR_64BIT)
		desc_cur->reserv3 = (dma_addr >> REG_BIT_WIDTH) <<
				    SG_DESC_HI8_OFFSET;
#endif
	} else {
		phys_addr = virt_to_phys(skb->data);
		desc_cur->linear_addr = (u32)phys_addr;
#if defined(CONFIG_GMAC_DDR_64BIT)
		desc_cur->reserv3 = (phys_addr >> REG_BIT_WIDTH) <<
				    SG_DESC_HI8_OFFSET;
#endif
	}

	ret = gmac_xmit_gso_sg_frag(ld, skb, desc_cur, tx_bq_desc, desc_pos);
	if (unlikely(ret))
		return ret;

	return 0;
}

static int gmac_xmit_gso(struct gmac_netdev_local *ld, struct sk_buff *skb,
			   struct gmac_tso_desc *tx_bq_desc, unsigned int desc_pos)
{
	int pkt_type = PKT_NORMAL;
	int nfrags = skb_shinfo(skb)->nr_frags;
	dma_addr_t addr;
	int ret;

	if (skb_is_gso(skb) || nfrags)
		pkt_type = PKT_SG; /* TSO pkt or SG pkt */

	ret = gmac_check_hw_capability(skb);
	if (unlikely(ret))
		return ret;

	ret = gmac_get_pkt_info(ld, skb, tx_bq_desc);
	if (unlikely(ret))
		return ret;

	if (pkt_type == PKT_NORMAL) {
		if (!has_cap_cci(ld->hw_cap)) {
			addr = dma_map_single(ld->dev, skb->data, skb->len, DMA_TO_DEVICE);
			ret = dma_mapping_error(ld->dev, addr);
			if (unlikely(ret)) {
				pr_err("Normal Packet DMA Mapping fail.\n");
				return -EFAULT;
			}
			tx_bq_desc->data_buff_addr = (u32)addr;
#if defined(CONFIG_GMAC_DDR_64BIT)
			tx_bq_desc->reserve_desc2 = (addr >> REG_BIT_WIDTH) &
						    TX_DESC_HI8_MASK;
#endif
		} else {
			addr = virt_to_phys(skb->data);
			tx_bq_desc->data_buff_addr = (u32)addr;
#if defined(CONFIG_GMAC_DDR_64BIT)
			tx_bq_desc->reserve_desc2 = (addr >> REG_BIT_WIDTH) &
						    TX_DESC_HI8_MASK;
#endif
		}
	} else {
		ret = gmac_xmit_gso_sg(ld, skb, tx_bq_desc, desc_pos);
		if (unlikely(ret))
			return ret;
	}

#ifdef GMAC_TSO_DEBUG
	if (id_send >= MAX_RECORD)
		id_send = 0;
	if (memcpy_s(&pkt_rec[id_send].desc, sizeof(struct gmac_tso_desc),
				tx_bq_desc, sizeof(struct gmac_tso_desc)) < 0)
		printk("memcpy_s  err : %s %d.\n", __func__, __LINE__);
	pkt_rec[id_send].status = 1;
	id_send++;
#endif
	return 0;
}

static netdev_tx_t gmac_net_xmit(struct sk_buff *skb, struct net_device *dev);

static netdev_tx_t gmac_sw_gso(struct gmac_netdev_local *ld,
				 struct sk_buff *skb)
{
	struct sk_buff *segs = NULL;
	struct sk_buff *curr_skb = NULL;
	int ret;
	int gso_segs = skb_shinfo(skb)->gso_segs;
	if (gso_segs == 0 && skb_shinfo(skb)->gso_size != 0)
		gso_segs = DIV_ROUND_UP(skb->len, skb_shinfo(skb)->gso_size);

	/* Estimate the number of fragments in the worst case */
	if (unlikely(gmac_tx_avail(ld) < gso_segs)) {
		netif_stop_queue(ld->netdev);
		if (gmac_tx_avail(ld) < gso_segs) {
			ld->netdev->stats.tx_dropped++;
			ld->netdev->stats.tx_fifo_errors++;
			return NETDEV_TX_BUSY;
		}
		netif_wake_queue(ld->netdev);
	}

	segs = skb_gso_segment(skb, ld->netdev->features & ~(NETIF_F_CSUM_MASK |
			       NETIF_F_SG | NETIF_F_GSO_SOFTWARE));
	if (IS_ERR_OR_NULL(segs))
		goto drop;

	do {
		curr_skb = segs;
		segs = segs->next;
		curr_skb->next = NULL;
		ret = gmac_net_xmit(curr_skb, ld->netdev);
		if (unlikely(ret != NETDEV_TX_OK))
			pr_err_once("gmac_net_xmit error ret=%d\n", ret);
	} while (segs != NULL);

	dev_kfree_skb_any(skb);
	return NETDEV_TX_OK;

drop:
	dev_kfree_skb_any(skb);
	ld->netdev->stats.tx_dropped++;
	return NETDEV_TX_OK;
}

static int gmac_net_xmit_normal(struct sk_buff *skb, struct net_device *dev,
				  struct gmac_desc *desc, u32 pos)
{
	struct gmac_netdev_local *ld = netdev_priv(dev);
	dma_addr_t addr;

	if (!has_cap_cci(ld->hw_cap)) {
		addr = dma_map_single(ld->dev, skb->data, skb->len, DMA_TO_DEVICE);
		if (unlikely(dma_mapping_error(ld->dev, addr))) {
			dev_kfree_skb_any(skb);
			dev->stats.tx_dropped++;
			ld->tx_skb[pos] = NULL;
			ld->TX_BQ.skb[pos] = NULL;
			return -1;
		}
		desc->data_buff_addr = (u32)addr;
#if defined(CONFIG_GMAC_DDR_64BIT)
		desc->rxhash = (addr >> REG_BIT_WIDTH) & TX_DESC_HI8_MASK;
#endif
	} else {
		addr = virt_to_phys(skb->data);
		desc->data_buff_addr = (u32)addr;
#if defined(CONFIG_GMAC_DDR_64BIT)
		desc->rxhash = (addr >> REG_BIT_WIDTH) & TX_DESC_HI8_MASK;
#endif
	}
	desc->buffer_len = ETH_MAX_FRAME_SIZE - 1;
	desc->data_len = skb->len;
	desc->fl = DESC_FL_FULL;
	desc->descvid = DESC_VLD_BUSY;

	return 0;
}

static int gmac_check_skb_len(struct sk_buff *skb, struct net_device *dev)
{
	if (skb->len < ETH_HLEN) {
		dev_kfree_skb_any(skb);
		dev->stats.tx_errors++;
		dev->stats.tx_dropped++;
		return -1;
	}
	return 0;
}

static netdev_tx_t gmac_net_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct gmac_netdev_local *ld = netdev_priv(dev);
	struct gmac_desc *desc = NULL;
	unsigned long txflags;
	int ret;
	u32 pos;

	if (unlikely(gmac_check_skb_len(skb, dev) < 0))
		return NETDEV_TX_OK;

	/*
	 * if adding gmac_xmit_reclaim here, iperf tcp client
	 * performance will be affected, from 550M(avg) to 513M~300M
	 */

	/* software write pointer */
	pos = dma_cnt(readl(ld->gmac_iobase + TX_BQ_WR_ADDR));

	spin_lock_irqsave(&ld->txlock, txflags);

	if (unlikely(ld->tx_skb[pos] || ld->TX_BQ.skb[pos])) {
		dev->stats.tx_dropped++;
		dev->stats.tx_fifo_errors++;
		netif_stop_queue(dev);
		spin_unlock_irqrestore(&ld->txlock, txflags);

		return NETDEV_TX_BUSY;
	}

	ld->TX_BQ.skb[pos] = skb;
	ld->tx_skb[pos] = skb;

	desc = ld->TX_BQ.desc + pos;

	if (ld->tso_supported) {
		ret = gmac_xmit_gso(ld, skb, (struct gmac_tso_desc *)desc, pos);
		if (unlikely(ret < 0)) {
			ld->tx_skb[pos] = NULL;
			ld->TX_BQ.skb[pos] = NULL;
			spin_unlock_irqrestore(&ld->txlock, txflags);

			if (ret == -ENOTSUPP)
				return gmac_sw_gso(ld, skb);

			dev_kfree_skb_any(skb);
			dev->stats.tx_dropped++;
			return NETDEV_TX_OK;
		}
	} else {
		ret = gmac_net_xmit_normal(skb, dev, desc, pos);
		if (unlikely(ret < 0)) {
			spin_unlock_irqrestore(&ld->txlock, txflags);
			return NETDEV_TX_OK;
		}
	}

	/*
	 * This barrier is important here.  It is required to ensure
	 * the ARM CPU flushes it's DMA write buffers before proceeding
	 * to the next instruction, to ensure that GMAC will see
	 * our descriptor changes in memory
	 */
	gmac_sync_barrier();
	pos = dma_ring_incr(pos, TX_DESC_NUM);
	writel(dma_byte(pos), ld->gmac_iobase + TX_BQ_WR_ADDR);

	netif_trans_update(dev);
	dev->stats.tx_packets++;
	dev->stats.tx_bytes += skb->len;
	netdev_sent_queue(dev, skb->len);

	spin_unlock_irqrestore(&ld->txlock, txflags);

	return NETDEV_TX_OK;
}

void gmac_enable_napi(struct gmac_netdev_local *priv)
{
	struct gmac_napi *q_napi = NULL;
	int i;
	if (priv == NULL)
		return;
	for (i = 0; i < priv->num_rxqs; i++) {
		q_napi = &priv->q_napi[i];
		napi_enable(&q_napi->napi);
	}
}

void gmac_disable_napi(struct gmac_netdev_local *priv)
{
	struct gmac_napi *q_napi = NULL;
	int i;
	if (priv == NULL)
		return;
	for (i = 0; i < priv->num_rxqs; i++) {
		q_napi = &priv->q_napi[i];
		napi_disable(&q_napi->napi);
	}
}

static int gmac_net_open(struct net_device *dev)
{
	struct gmac_netdev_local *ld = netdev_priv(dev);
	unsigned long flags;

	clk_prepare_enable(ld->macif_clk);
	clk_prepare_enable(ld->clk);

	/*
	 * If we configure mac address by
	 * "ifconfig ethX hw ether XX:XX:XX:XX:XX:XX",
	 * the ethX must be down state and mac core clock is disabled
	 * which results the mac address has not been configured
	 * in mac core register.
	 * So we must set mac address again here,
	 * because mac core clock is enabled at this time
	 * and we can configure mac address to mac core register.
	 */
	gmac_hw_set_mac_addr(dev);

	/*
	 * We should use netif_carrier_off() here,
	 * because the default state should be off.
	 * And this call should before phy_start().
	 */
	netif_carrier_off(dev);
	gmac_enable_napi(ld);
	phy_start(ld->phy);

	gmac_hw_desc_enable(ld);
	gmac_port_enable(ld);
	gmac_irq_enable_all_queue(ld);

	spin_lock_irqsave(&ld->rxlock, flags);
	gmac_rx_refill(ld);
	spin_unlock_irqrestore(&ld->rxlock, flags);

	ld->monitor.expires = jiffies + GMAC_MONITOR_TIMER;
	mod_timer(&ld->monitor, ld->monitor.expires);

	netif_start_queue(dev);

	return 0;
}

static int gmac_net_close(struct net_device *dev)
{
	struct gmac_netdev_local *ld = netdev_priv(dev);

	gmac_irq_disable_all_queue(ld);
	gmac_hw_desc_disable(ld);

	gmac_disable_napi(ld);

	netif_carrier_off(dev);
	netif_stop_queue(dev);

	phy_stop(ld->phy);
	del_timer_sync(&ld->monitor);

	clk_disable_unprepare(ld->clk);
	clk_disable_unprepare(ld->macif_clk);

	return 0;
}

static void gmac_net_timeout(struct net_device *dev)
{
	dev->stats.tx_errors++;

	pr_err("tx timeout!\n");
}

static void gmac_set_multicast_list(struct net_device *dev)
{
	gmac_gmac_multicast_list(dev);
}

static void gmac_enable_rxcsum_drop(struct gmac_netdev_local const *ld,
		  bool drop)
{
	unsigned int v;

	v = readl(ld->gmac_iobase + TSO_COE_CTRL);
	if (drop)
		v |= COE_ERR_DROP;
	else
		v &= ~COE_ERR_DROP;
	writel(v, ld->gmac_iobase + TSO_COE_CTRL);
}

static int gmac_set_features(struct net_device *dev,
			       netdev_features_t features)
{
	struct gmac_netdev_local *ld = netdev_priv(dev);
	netdev_features_t changed = dev->features ^ features;

	if (changed & NETIF_F_RXCSUM) {
		if (features & NETIF_F_RXCSUM)
			gmac_enable_rxcsum_drop(ld, true);
		else
			gmac_enable_rxcsum_drop(ld, false);
	}

	return 0;
}

static struct net_device_stats *gmac_net_get_stats(struct net_device *dev)
{
	return &dev->stats;
}

static const struct ethtool_ops eth_ethtools_ops = {
	.get_drvinfo = gmac_get_drvinfo,
	.get_link = gmac_get_link,
	.get_settings = gmac_get_settings,
	.set_settings = gmac_set_settings,
	.get_pauseparam = gmac_get_pauseparam,
	.set_pauseparam = gmac_set_pauseparam,
	.get_msglevel = gmac_ethtool_getmsglevel,
	.set_msglevel = gmac_ethtool_setmsglevel,
	.get_rxfh_key_size = gmac_get_rxfh_key_size,
	.get_rxfh_indir_size = gmac_get_rxfh_indir_size,
	.get_rxfh = gmac_get_rxfh,
	.set_rxfh = gmac_set_rxfh,
	.get_rxnfc = gmac_get_rxnfc,
	.set_rxnfc = gmac_set_rxnfc,
};

static const struct net_device_ops eth_netdev_ops = {
	.ndo_open = gmac_net_open,
	.ndo_stop = gmac_net_close,
	.ndo_start_xmit = gmac_net_xmit,
	.ndo_tx_timeout = gmac_net_timeout,
	.ndo_set_rx_mode = gmac_set_multicast_list,
	.ndo_set_features = gmac_set_features,
	.ndo_do_ioctl = gmac_ioctl,
	.ndo_set_mac_address = gmac_net_set_mac_address,
	.ndo_change_mtu = eth_change_mtu,
	.ndo_get_stats = gmac_net_get_stats,
};

static int gmac_of_get_param(struct gmac_netdev_local *ld,
			       struct device_node const *node)
{
	/* get auto eee */
	ld->autoeee = of_property_read_bool(node, "autoeee");
	/* get internal flag */
	ld->internal_phy =
		of_property_read_bool(node, "internal-phy");

	return 0;
}

static void gmac_destroy_hw_desc_queue(struct gmac_netdev_local *priv)
{
	int i;

	for (i = 0; i < QUEUE_NUMS + RSS_NUM_RXQS - 1; i++) {
		if (priv->pool[i].desc) {
			if (has_cap_cci(priv->hw_cap))
				kfree(priv->pool[i].desc);
			else
				dma_free_coherent(priv->dev, priv->pool[i].size,
						  priv->pool[i].desc,
						  priv->pool[i].phys_addr);
			priv->pool[i].desc = NULL;
		}
	}

	kfree(priv->RX_FQ.skb);
	kfree(priv->TX_BQ.skb);
	priv->RX_FQ.skb = NULL;
	priv->TX_BQ.skb = NULL;

	if (priv->tso_supported) {
		kfree(priv->TX_BQ.sg_desc_offset);
		priv->TX_BQ.sg_desc_offset = NULL;
	}

	kfree(priv->tx_skb);
	priv->tx_skb = NULL;

	kfree(priv->rx_skb);
	priv->rx_skb = NULL;
}

static int gmac_init_desc_queue_mem(struct gmac_netdev_local *priv)
{
	priv->RX_FQ.skb = kzalloc(priv->RX_FQ.count
				  * sizeof(struct sk_buff *), GFP_KERNEL);
	if (!priv->RX_FQ.skb)
		return -ENOMEM;

	priv->rx_skb = kzalloc(priv->RX_FQ.count
			       * sizeof(struct sk_buff *), GFP_KERNEL);
	if (priv->rx_skb == NULL)
		return -ENOMEM;

	priv->TX_BQ.skb = kzalloc(priv->TX_BQ.count
				  * sizeof(struct sk_buff *), GFP_KERNEL);
	if (!priv->TX_BQ.skb)
		return -ENOMEM;

	priv->tx_skb = kzalloc(priv->TX_BQ.count
			       * sizeof(struct sk_buff *), GFP_KERNEL);
	if (priv->tx_skb == NULL)
		return -ENOMEM;

	if (priv->tso_supported) {
		priv->TX_BQ.sg_desc_offset = kzalloc(priv->TX_BQ.count
						     * sizeof(int), GFP_KERNEL);
		if (!priv->TX_BQ.sg_desc_offset)
			return -ENOMEM;
	}

	return 0;
}

static int gmac_init_hw_desc_queue(struct gmac_netdev_local *priv)
{
	struct device *dev = NULL;
	struct gmac_desc *virt_addr = NULL;
	dma_addr_t phys_addr = 0;
	int size, i, ret = 0;
	if (priv == NULL || priv->dev == NULL)
		return -EINVAL;
	dev = priv->dev;
	if (dev == NULL)
		return -EINVAL;
	priv->RX_FQ.count = RX_DESC_NUM;
	priv->RX_BQ.count = RX_DESC_NUM;
	priv->TX_BQ.count = TX_DESC_NUM;
	priv->TX_RQ.count = TX_DESC_NUM;

	for (i = 1; i < RSS_NUM_RXQS; i++)
		priv->pool[BASE_QUEUE_NUMS + i].count = RX_DESC_NUM;

	for (i = 0; i < (QUEUE_NUMS + RSS_NUM_RXQS - 1); i++) {
		size = priv->pool[i].count * sizeof(struct gmac_desc);
		if (has_cap_cci(priv->hw_cap)) {
			virt_addr = kmalloc(size, GFP_KERNEL);
			if (virt_addr != NULL) {
				ret = memset_s(virt_addr, size, 0, size);
				phys_addr = virt_to_phys(virt_addr);
			}
		} else {
			virt_addr = dma_alloc_coherent(dev, size, &phys_addr, GFP_KERNEL);
			if (virt_addr != NULL)
				ret = memset_s(virt_addr, size, 0, size);
		}
		if (ret != EOK)
			printk("memset_s  err : %s %d.\n", __func__, __LINE__);
		if (virt_addr == NULL)
			goto error_free_pool;

		priv->pool[i].size = size;
		priv->pool[i].desc = virt_addr;
		priv->pool[i].phys_addr = phys_addr;
	}

	if (gmac_init_desc_queue_mem(priv) == -ENOMEM)
		goto error_free_pool;

	gmac_hw_set_desc_addr(priv);
	if (has_cap_cci(priv->hw_cap))
		pr_info("gmac: ETH MAC supporte CCI.\n");

	return 0;

error_free_pool:
	gmac_destroy_hw_desc_queue(priv);

	return -ENOMEM;
}

void gmac_init_napi(struct gmac_netdev_local *priv)
{
	struct gmac_napi *q_napi = NULL;
	int i;
	if (priv == NULL || priv->netdev == NULL)
		return;
	for (i = 0; i < priv->num_rxqs; i++) {
		q_napi = &priv->q_napi[i];
		q_napi->rxq_id = i;
		q_napi->ndev_priv = priv;
		netif_napi_add(priv->netdev, &q_napi->napi, gmac_poll,
			       NAPI_POLL_WEIGHT);
	}
}

void gmac_destroy_napi(struct gmac_netdev_local *priv)
{
	struct gmac_napi *q_napi = NULL;
	int i;
	if (priv == NULL)
		return;
	for (i = 0; i < priv->num_rxqs; i++) {
		q_napi = &priv->q_napi[i];
		netif_napi_del(&q_napi->napi);
	}
}

int gmac_request_irqs(struct platform_device *pdev,
			struct gmac_netdev_local *priv)
{
	struct device *dev = NULL;
	int ret;
	int i;
	if (pdev == NULL || priv == NULL || priv->dev == NULL || pdev->name == NULL)
		return -ENOMEM;

	dev = priv->dev;
	for (i = 0; i < priv->num_rxqs; i++) {
		ret = platform_get_irq(pdev, i);
		if (ret < 0) {
			dev_err(dev, "No irq[%d] resource, ret=%d\n", i, ret);
			return ret;
		}
		priv->irq[i] = ret;

		ret = devm_request_irq(dev, priv->irq[i], gmac_interrupt,
				       IRQF_SHARED, pdev->name,
				       &priv->q_napi[i]);
		if (ret) {
			dev_err(dev, "devm_request_irq failed, ret=%d\n", ret);
			return ret;
		}
	}

	return 0;
}

static int gmac_dev_probe_res(struct platform_device *pdev,
				struct gmac_netdev_local *priv)
{
	struct device *dev = &pdev->dev;
	struct net_device *ndev = priv->netdev;
	struct resource *res = NULL;
	int ret;

	res = platform_get_resource(pdev, IORESOURCE_MEM, MEM_GMAC_IOBASE);
	priv->gmac_iobase = devm_ioremap_resource(dev, res);
	if (IS_ERR(priv->gmac_iobase)) {
		ret = PTR_ERR(priv->gmac_iobase);
		return ret;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, MEM_MACIF_IOBASE);
	priv->macif_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(priv->macif_base)) {
		ret = PTR_ERR(priv->macif_base);
		return ret;
	}

	/* only for some chip to fix AXI bus burst and outstanding config */
	res = platform_get_resource(pdev, IORESOURCE_MEM, MEM_AXI_BUS_CFG_IOBASE);
	priv->axi_bus_cfg_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(priv->axi_bus_cfg_base))
		priv->axi_bus_cfg_base = NULL;

	priv->port_rst = devm_reset_control_get(dev, GMAC_PORT_RST_NAME);
	if (IS_ERR(priv->port_rst)) {
		ret = PTR_ERR(priv->port_rst);
		return ret;
	}

	priv->macif_rst = devm_reset_control_get(dev, GMAC_MACIF_RST_NAME);
	if (IS_ERR(priv->macif_rst)) {
		ret = PTR_ERR(priv->macif_rst);
		return ret;
	}

	priv->phy_rst = devm_reset_control_get(dev, GMAC_PHY_RST_NAME);
	if (IS_ERR(priv->phy_rst))
		priv->phy_rst = NULL;

	priv->clk = devm_clk_get(&pdev->dev, GMAC_MAC_CLK_NAME);
	if (IS_ERR(priv->clk)) {
		netdev_err(ndev, "failed to get clk\n");
		ret = -ENODEV;
		return ret;
	}

	ret = clk_prepare_enable(priv->clk);
	if (ret < 0) {
		netdev_err(ndev, "failed to enable clk %d\n", ret);
		return ret;
	}
	return 0;
}

static int gmac_dev_macif_clk(struct platform_device *pdev,
				struct gmac_netdev_local *priv, struct net_device *ndev)
{
	int ret;

	priv->macif_clk = devm_clk_get(&pdev->dev, GMAC_MACIF_CLK_NAME);
	if (IS_ERR(priv->macif_clk))
		priv->macif_clk = NULL;

	if (priv->macif_clk != NULL) {
		ret = clk_prepare_enable(priv->macif_clk);
		if (ret < 0) {
			netdev_err(ndev, "failed enable macif_clk %d\n", ret);
			return ret;
		}
	}
	return 0;
}

static int gmac_dev_probe_init(struct platform_device *pdev,
				 struct gmac_netdev_local *priv, struct net_device *ndev)
{
	int ret;

	gmac_init_napi(priv);
	spin_lock_init(&priv->rxlock);
	spin_lock_init(&priv->txlock);
	spin_lock_init(&priv->pmtlock);

	/* init netdevice */
	ndev->irq = priv->irq[0];
	ndev->watchdog_timeo = 3 * HZ; /* 3HZ */
	ndev->netdev_ops = &eth_netdev_ops;
	ndev->ethtool_ops = &eth_ethtools_ops;

	if (priv->has_rxhash_cap)
		ndev->hw_features |= NETIF_F_RXHASH;
	if (priv->has_rss_cap)
		ndev->hw_features |= NETIF_F_NTUPLE;
	if (priv->tso_supported)
		ndev->hw_features |= NETIF_F_SG |
				     NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM |
				     NETIF_F_TSO | NETIF_F_TSO6;

#if defined(CONFIG_GMAC_RXCSUM)
	ndev->hw_features |= NETIF_F_RXCSUM;
	gmac_enable_rxcsum_drop(priv, true);
#endif

	ndev->features |= ndev->hw_features;
	ndev->features |= NETIF_F_HIGHDMA | NETIF_F_GSO;
	ndev->vlan_features |= ndev->features;

	timer_setup(&priv->monitor, gmac_monitor_func, 0);

	device_set_wakeup_capable(priv->dev, 1);
	/*
	 * when we can let phy powerdown?
	 * In some mode, we don't want phy powerdown,
	 * so I set wakeup enable all the time
	 */
	device_set_wakeup_enable(priv->dev, 1);

	priv->wol_enable = false;

	priv->msg_enable = netif_msg_init(debug, DEFAULT_MSG_ENABLE);

#if defined(CONFIG_GMAC_DDR_64BIT)
	if (!has_cap_cci(priv->hw_cap)) {
		struct device *dev = &pdev->dev;
		ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(64)); /* 64bit */
		if (ret) {
			pr_err("dma set mask 64 failed! ret=%d", ret);
			return ret;
		}
	}
#endif

	/* init hw desc queue */
	ret = gmac_init_hw_desc_queue(priv);
	if (ret)
		return ret;

	return 0;
}

static int gmac_dev_probe_phy(struct platform_device *pdev,
				struct gmac_netdev_local *priv, struct net_device *ndev,
				bool fixed_link)
{
	int ret;

	/* phy fix here?? other way ??? */
	gmac_phy_register_fixups();

	priv->phy = of_phy_connect(ndev, priv->phy_node,
				   &gmac_adjust_link, 0, priv->phy_mode);
	if (priv->phy == NULL || priv->phy->drv == NULL) {
		ret = -ENODEV;
		return ret;
	}

	/* If the phy_id is all zero and not fixed link, there is no device there */
	if ((priv->phy->phy_id == 0) && !fixed_link) {
		pr_info("phy %d not found\n", priv->phy->mdio.addr);
		ret = -ENODEV;
		return ret;
	}

	pr_info("attached PHY %d to driver %s, PHY_ID=0x%x\n",
		priv->phy->mdio.addr, priv->phy->drv->name, priv->phy->phy_id);

	/* Stop Advertising 1000BASE Capability if interface is not RGMII */
	if ((priv->phy_mode == PHY_INTERFACE_MODE_MII) ||
			(priv->phy_mode == PHY_INTERFACE_MODE_RMII)) {
		priv->phy->advertising &= ~(SUPPORTED_1000baseT_Half |
					    SUPPORTED_1000baseT_Full);

		/*
		 * Internal FE phy's reg BMSR bit8 is wrong, make the kernel
		 * believe it has the 1000base Capability, so fix it here
		 */
		if (priv->phy->phy_id == PHY_ID_FESTAV200)
			priv->phy->supported &= ~(ADVERTISED_1000baseT_Full |
						  ADVERTISED_1000baseT_Half);
	}

	gmac_set_flow_ctrl_args(priv);
	gmac_set_flow_ctrl_params(priv);
	priv->phy->supported |= SUPPORTED_Pause;
	if (priv->flow_ctrl)
		priv->phy->advertising |= SUPPORTED_Pause;

	if (priv->autoeee)
		init_autoeee(priv);

	ret = gmac_request_irqs(pdev, priv);
	if (ret)
		return ret;

	return 0;
}

static void gmac_set_hw_cap(struct platform_device *pdev,
			      struct gmac_netdev_local *priv)
{
	unsigned int hw_cap;

	hw_cap = readl(priv->gmac_iobase + CRF_MIN_PACKET);
	priv->tso_supported = has_tso_cap(hw_cap);
	priv->has_rxhash_cap = has_rxhash_cap(hw_cap);
	priv->has_rss_cap = has_rss_cap(hw_cap);

	gmac_set_rss_cap(priv);
	gmac_get_rss_key(priv);
	if (priv->has_rss_cap) {
		priv->rss_info.ind_tbl_size = RSS_INDIRECTION_TABLE_SIZE;
		gmac_get_rss(priv);
	}

	if (priv->has_rxhash_cap) {
		priv->rss_info.hash_cfg = DEF_HASH_CFG;
		gmac_config_hash_policy(priv);
	}
}

static void gmac_set_mac_addr(struct net_device *ndev,
				struct device_node *node)
{
	const char *mac_addr = NULL;

	mac_addr = of_get_mac_address(node);
	if (mac_addr != NULL)
		ether_addr_copy(ndev->dev_addr, mac_addr);
	if (!is_valid_ether_addr(ndev->dev_addr)) {
		eth_hw_addr_random(ndev);
		netdev_warn(ndev, "using random MAC address %pM\n",
			    ndev->dev_addr);
	}

	gmac_hw_set_mac_addr(ndev);
}

static int gmac_phy_init(struct device *dev, struct net_device *ndev,
			   struct gmac_netdev_local *priv, struct device_node *node, bool *fixed_link)
{
	int ret;

	/*
	 * phy reset, should be early than "of_mdiobus_register".
	 * becausue "of_mdiobus_register" will read PHY register by MDIO.
	 */
	gmac_hw_phy_reset(priv);

	gmac_of_get_param(priv, node);

	ret = of_get_phy_mode(node);
	if (ret < 0) {
		netdev_err(ndev, "not find phy-mode\n");
		return ret;
	}
	priv->phy_mode = ret;

	priv->phy_node = of_parse_phandle(node, "phy-handle", 0);
	if (priv->phy_node == NULL) {
		/* check if a fixed-link is defined in device-tree */
		if (of_phy_is_fixed_link(node)) {
			ret = of_phy_register_fixed_link(node);
			if (ret < 0) {
				dev_err(dev, "cannot register fixed PHY %d\n", ret);
				return ret;
			}

			/*
			 * In the case of a fixed PHY, the DT node associated
			 * to the PHY is the Ethernet MAC DT node.
			 */
			priv->phy_node = of_node_get(node);
			*fixed_link = true;
		} else {
			netdev_err(ndev, "not find phy-handle\n");
			ret = -EINVAL;
			return ret;
		}
	}
	return 0;
}

static int gmac_dev_probe_device(struct platform_device *pdev,
				   struct net_device **p_ndev, struct gmac_netdev_local **p_priv)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct net_device *ndev = NULL;
	struct gmac_netdev_local *priv = NULL;
	int num_rxqs;

	gmac_verify_flow_ctrl_args();

	if (of_device_is_compatible(node, "vendor,gmac-v5"))
		num_rxqs = RSS_NUM_RXQS;
	else
		num_rxqs = 1;

	ndev = alloc_etherdev_mqs(sizeof(struct gmac_netdev_local), 1,
				  num_rxqs);
	if (ndev == NULL)
		return -ENOMEM;

	platform_set_drvdata(pdev, ndev);
	SET_NETDEV_DEV(ndev, dev);

	priv = netdev_priv(ndev);
	priv->dev = dev;
	dev_hold(ndev);
	priv->netdev = ndev;
	priv->num_rxqs = num_rxqs;

	if (of_device_is_compatible(node, "vendor,gmac-v3"))
		priv->hw_cap |= HW_CAP_CCI;

	*p_ndev = ndev;
	*p_priv = priv;
	return 0;
}

static int gmac_dev_probe_queue(struct platform_device *pdev,
				  struct gmac_netdev_local *priv, struct net_device *ndev, bool fixed_link)
{
	int ret;

	ret = gmac_dev_probe_init(pdev, priv, ndev);
	if (ret)
		goto _error_hw_desc_queue;

	if (priv->tso_supported) {
		ret = gmac_init_sg_desc_queue(priv);
		if (ret)
			goto _error_sg_desc_queue;
	}

	/* register netdevice */
	ret = register_netdev(priv->netdev);
	if (ret) {
		pr_err("register_ndev failed!");
		goto _error_sg_desc_queue;
	}

	/*
	 * reset queue here to make BQL only reset once.
	 * if we put netdev_reset_queue() in gmac_net_open(),
	 * the BQL will be reset when ifconfig eth0 down and up,
	 * but the tx ring is not cleared before.
	 * As a result, the NAPI poll will call netdev_completed_queue()
	 * and BQL throw a bug.
	 */
	netdev_reset_queue(ndev);

	clk_disable_unprepare(priv->clk);
	if (priv->macif_clk != NULL)
		clk_disable_unprepare(priv->macif_clk);

	pr_info("ETH: %s, phy_addr=%d\n",
		phy_modes(priv->phy_mode), priv->phy->mdio.addr);

	dev_put(ndev);
	return ret;

_error_sg_desc_queue:
	if (priv->tso_supported)
		gmac_destroy_sg_desc_queue(priv);
_error_hw_desc_queue:
	gmac_destroy_hw_desc_queue(priv);
	gmac_destroy_napi(priv);

	return ret;
}

static int gmac_dev_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct net_device *ndev = NULL;
	struct gmac_netdev_local *priv = NULL;
	int ret;
	bool fixed_link = false;

	ret = gmac_dev_probe_device(pdev, &ndev, &priv);
	if (ret)
		return ret;

	ret = gmac_dev_probe_res(pdev, priv);
	if (ret)
		goto out_free_netdev;

	ret = gmac_dev_macif_clk(pdev, priv, ndev);
	if (ret)
		goto out_clk_disable;

	gmac_mac_core_reset(priv);

	ret = gmac_phy_init(dev, ndev, priv, node, &fixed_link);
	if (ret)
		goto out_macif_clk_disable;

	gmac_set_mac_addr(ndev, node);
	gmac_set_hw_cap(pdev, priv);

	/* init hw controller */
	gmac_hw_init(priv);

	ret = gmac_dev_probe_phy(pdev, priv, ndev, fixed_link);
	if (ret) {
		if (priv->phy == NULL)
			goto out_phy_node;
		else
			goto out_phy_disconnect;
	}

	ret = gmac_dev_probe_queue(pdev, priv, ndev, fixed_link);
	if (ret)
		goto out_phy_disconnect;

	return ret;

out_phy_disconnect:
	phy_disconnect(priv->phy);
out_phy_node:
	of_node_put(priv->phy_node);
out_macif_clk_disable:
	if (priv->macif_clk != NULL)
		clk_disable_unprepare(priv->macif_clk);
out_clk_disable:
	clk_disable_unprepare(priv->clk);
out_free_netdev:
	dev_put(ndev);
	free_netdev(ndev);

	return ret;
}

static int gmac_dev_remove(struct platform_device *pdev)
{
	struct net_device *ndev = platform_get_drvdata(pdev);
	struct gmac_netdev_local *priv = netdev_priv(ndev);

	/* stop the gmac and free all resource */
	del_timer_sync(&priv->monitor);
	gmac_destroy_napi(priv);

	unregister_netdev(ndev);

	gmac_reclaim_rx_tx_resource(priv);
	gmac_free_rx_skb(priv);
	gmac_free_tx_skb(priv);

	if (priv->tso_supported)
		gmac_destroy_sg_desc_queue(priv);
	gmac_destroy_hw_desc_queue(priv);

	phy_disconnect(priv->phy);
	of_node_put(priv->phy_node);

	free_netdev(ndev);

	gmac_phy_unregister_fixups();

	return 0;
}

#ifdef CONFIG_PM
static void gmac_disable_irq(struct gmac_netdev_local *priv)
{
	int i;

	for (i = 0; i < priv->num_rxqs; i++)
		disable_irq(priv->irq[i]);
}

static void gmac_enable_irq(struct gmac_netdev_local *priv)
{
	int i;

	for (i = 0; i < priv->num_rxqs; i++)
		enable_irq(priv->irq[i]);
}

int gmac_dev_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct net_device *ndev = platform_get_drvdata(pdev);
	struct gmac_netdev_local *priv = netdev_priv(ndev);

	gmac_disable_irq(priv);
	/*
	 * If support Wake on LAN, we should not disconnect phy
	 * because it will call phy_suspend to power down phy.
	 */
	if (!priv->wol_enable)
		phy_disconnect(priv->phy);
	del_timer_sync(&priv->monitor);
	/*
	 * If suspend when netif is not up, the napi_disable will run into
	 * dead loop and dpm_drv_timeout will give warning.
	 */
	if (netif_running(ndev))
		gmac_disable_napi(priv);
	netif_device_detach(ndev);

	netif_carrier_off(ndev);

	/*
	 * If netdev is down, MAC clock is disabled.
	 * So if we want to reclaim MAC rx and tx resource,
	 * we must first enable MAC clock and then disable it.
	 */
	if (!(ndev->flags & IFF_UP))
		clk_prepare_enable(priv->clk);

	gmac_reclaim_rx_tx_resource(priv);

	if (!(ndev->flags & IFF_UP))
		clk_disable_unprepare(priv->clk);

	pmt_enter(priv);

	if (!priv->wol_enable) {
		/*
		 * if no WOL, then poweroff
		 * no need to call genphy_resume() in resume,
		 * because we reset everything
		 */
		genphy_suspend(priv->phy); /* power down phy */
		msleep(20); /* wait 20ms */
		gmac_hw_all_clk_disable(priv);
	}

	return 0;
}
EXPORT_SYMBOL(gmac_dev_suspend);

int gmac_dev_resume(struct platform_device *pdev)
{
	struct net_device *ndev = platform_get_drvdata(pdev);
	struct gmac_netdev_local *priv = netdev_priv(ndev);
	int ret;

	/*
	 * If we support Wake on LAN, we doesn't call clk_disable.
	 * But when we resume, the uboot may off mac clock and reset phy
	 * by re-write the mac CRG register.
	 * So we first call clk_disable, and then clk_enable.
	 */
	if (priv->wol_enable)
		gmac_hw_all_clk_disable(priv);

	gmac_hw_all_clk_enable(priv);
	/* internal FE_PHY: enable clk and reset  */
	gmac_hw_phy_reset(priv);

	/*
	 * If netdev is down, MAC clock is disabled.
	 * So if we want to restart MAC and re-initialize it,
	 * we must first enable MAC clock and then disable it.
	 */
	if (!(ndev->flags & IFF_UP))
		clk_prepare_enable(priv->clk);

	/* power on gmac */
	gmac_restart(priv);

	/*
	 * If support WoL, we didn't disconnect phy.
	 * But when we resume, we reset PHY, so we want to
	 * call phy_connect to make phy_fixup excuted.
	 * This is important for internal PHY fix.
	 */
	if (priv->wol_enable)
		phy_disconnect(priv->phy);

	ret = phy_connect_direct(ndev, priv->phy, gmac_adjust_link,
				 priv->phy_mode);
	if (ret)
		return ret;

	/*
	 * If we suspend and resume when net device is down,
	 * some operations are unnecessary.
	 */
	if (ndev->flags & IFF_UP) {
		priv->monitor.expires = jiffies + GMAC_MONITOR_TIMER;
		mod_timer(&priv->monitor, priv->monitor.expires);
		priv->old_link = 0;
		priv->old_speed = SPEED_UNKNOWN;
		priv->old_duplex = DUPLEX_UNKNOWN;
	}
	if (netif_running(ndev))
		gmac_enable_napi(priv);
	netif_device_attach(ndev);
	if (ndev->flags & IFF_UP)
		phy_start(priv->phy);
	gmac_enable_irq(priv);

	pmt_exit(priv);

	if (!(ndev->flags & IFF_UP))
		clk_disable_unprepare(priv->clk);

	return 0;
}
EXPORT_SYMBOL(gmac_dev_resume);
#endif

static const struct of_device_id gmac_of_match[] = {
	{ .compatible = "vendor,gmac", },
	{ .compatible = "vendor,gmac-v1", },
	{ .compatible = "vendor,gmac-v2", },
	{ .compatible = "vendor,gmac-v3", },
	{ .compatible = "vendor,gmac-v4", },
	{ .compatible = "vendor,gmac-v5", },
	{ },
};

MODULE_DEVICE_TABLE(of, gmac_of_match);

static struct platform_driver gmac_dev_driver = {
	.probe = gmac_dev_probe,
	.remove = gmac_dev_remove,
#ifdef CONFIG_PM
	.suspend = gmac_dev_suspend,
	.resume = gmac_dev_resume,
#endif
	.driver = {
		.owner = THIS_MODULE,
		.name = GMAC_DRIVER_NAME,
		.of_match_table = gmac_of_match,
	},
};

#include "proc_dev.c"

static int __init gmac_init(void)
{
	int ret;

	ret = platform_driver_register(&gmac_dev_driver);
	if (ret)
		return ret;

	gmac_proc_create();

	return 0;
}

static void __exit gmac_exit(void)
{
	platform_driver_unregister(&gmac_dev_driver);

	gmac_proc_destroy();
}

module_init(gmac_init);
module_exit(gmac_exit);

MODULE_DESCRIPTION("double GMAC driver, base on driver gmacv200");
MODULE_LICENSE("GPL v2");
