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

#include <linux/kernel.h>
#include <linux/if_vlan.h>
#include <linux/ip.h>
#include <net/ipv6.h>
#include <linux/reset.h>
#include <linux/securec.h>
#include "util.h"

static unsigned int flow_ctrl_en = FLOW_OFF;
static int tx_flow_ctrl_pause_time = CONFIG_TX_FLOW_CTRL_PAUSE_TIME;
static int tx_flow_ctrl_pause_interval = CONFIG_TX_FLOW_CTRL_PAUSE_INTERVAL;
static int tx_flow_ctrl_active_threshold = CONFIG_TX_FLOW_CTRL_ACTIVE_THRESHOLD;
static int tx_flow_ctrl_deactive_threshold =
				CONFIG_TX_FLOW_CTRL_DEACTIVE_THRESHOLD;

void gmac_trace(int level, const char *fmt, ...)
{
	if (level >= GMAC_TRACE_LEVEL) {
		va_list args;
		va_start(args, fmt);
		printk("gmac_trace:");
		printk(fmt, args);
		printk("\n");
		va_end(args);
	}
}

void gmac_config_port(struct net_device const *dev, u32 speed, u32 duplex)
{
	struct gmac_netdev_local *priv = netdev_priv(dev);
	u32 val;

	switch (priv->phy_mode) {
	case PHY_INTERFACE_MODE_RGMII:
		if (speed == SPEED_1000)
			val = RGMII_SPEED_1000;
		else if (speed == SPEED_100)
			val = RGMII_SPEED_100;
		else
			val = RGMII_SPEED_10;
		break;
	case PHY_INTERFACE_MODE_MII:
		if (speed == SPEED_100)
			val = MII_SPEED_100;
		else
			val = MII_SPEED_10;
		break;
	case PHY_INTERFACE_MODE_RMII:
		if (speed == SPEED_100)
			val = RMII_SPEED_100;
		else
			val = RMII_SPEED_10;
		break;
	default:
		netdev_warn(dev, "not supported mode\n");
		val = MII_SPEED_10;
		break;
	}

	if (duplex)
		val |= GMAC_FULL_DUPLEX;

	reset_control_assert(priv->macif_rst);
	writel_relaxed(val, priv->macif_base);
	reset_control_deassert(priv->macif_rst);

	writel_relaxed(BIT_MODE_CHANGE_EN, priv->gmac_iobase + MODE_CHANGE_EN);
	if (speed == SPEED_1000)
		val = GMAC_SPEED_1000;
	else if (speed == SPEED_100)
		val = GMAC_SPEED_100;
	else
		val = GMAC_SPEED_10;
	writel_relaxed(val, priv->gmac_iobase + PORT_MODE);
	writel_relaxed(0, priv->gmac_iobase + MODE_CHANGE_EN);
	writel_relaxed(duplex, priv->gmac_iobase + MAC_DUPLEX_HALF_CTRL);
}

int gmac_rx_checksum(struct net_device *dev, struct sk_buff *skb,
		  struct gmac_desc const *desc)
{
	int hdr_csum_done, payload_csum_done;
	int hdr_csum_err, payload_csum_err;
	if (skb == NULL || desc == NULL || dev == NULL)
		return -EINVAL;
	if (dev->features & NETIF_F_RXCSUM) {
		hdr_csum_done =	desc->header_csum_done;
		payload_csum_done =	desc->payload_csum_done;
		hdr_csum_err = desc->header_csum_err;
		payload_csum_err = desc->payload_csum_err;

		if (hdr_csum_done && payload_csum_done) {
			if (unlikely(hdr_csum_err || payload_csum_err)) {
				dev->stats.rx_errors++;
				dev->stats.rx_crc_errors++;
				dev_kfree_skb_any(skb);
				return -1;
			} else {
				skb->ip_summed = CHECKSUM_UNNECESSARY;
			}
		}
	}
	return 0;
}

void gmac_verify_flow_ctrl_args(void)
{
#if defined(CONFIG_TX_FLOW_CTRL_SUPPORT)
	flow_ctrl_en |= FLOW_TX;
#endif
#if defined(CONFIG_RX_FLOW_CTRL_SUPPORT)
	flow_ctrl_en |= FLOW_RX;
#endif
	if (tx_flow_ctrl_active_threshold < FC_ACTIVE_MIN ||
		tx_flow_ctrl_active_threshold > FC_ACTIVE_MAX)
		tx_flow_ctrl_active_threshold = FC_ACTIVE_DEFAULT;

	if (tx_flow_ctrl_deactive_threshold < FC_DEACTIVE_MIN ||
		tx_flow_ctrl_deactive_threshold > FC_DEACTIVE_MAX)
		tx_flow_ctrl_deactive_threshold = FC_DEACTIVE_DEFAULT;

	if (tx_flow_ctrl_active_threshold >= tx_flow_ctrl_deactive_threshold) {
		tx_flow_ctrl_active_threshold = FC_ACTIVE_DEFAULT;
		tx_flow_ctrl_deactive_threshold = FC_DEACTIVE_DEFAULT;
	}

	if (tx_flow_ctrl_pause_time < 0 ||
		tx_flow_ctrl_pause_time > FC_PAUSE_TIME_MAX)
		tx_flow_ctrl_pause_time = FC_PAUSE_TIME_DEFAULT;

	if (tx_flow_ctrl_pause_interval < 0 ||
		tx_flow_ctrl_pause_interval > FC_PAUSE_TIME_MAX)
		tx_flow_ctrl_pause_interval = FC_PAUSE_INTERVAL_DEFAULT;

	/*
	 * pause interval should not bigger than pause time,
	 * but should not too smaller to avoid sending too many pause frame.
	 */
	if ((tx_flow_ctrl_pause_interval > tx_flow_ctrl_pause_time) ||
		(tx_flow_ctrl_pause_interval < ((unsigned int)tx_flow_ctrl_pause_time >> 1)))
		tx_flow_ctrl_pause_interval = tx_flow_ctrl_pause_time;
}

void gmac_set_flow_ctrl_args(struct gmac_netdev_local *ld)
{
	if (ld == NULL)
		return;
	ld->flow_ctrl = flow_ctrl_en;
	ld->pause = tx_flow_ctrl_pause_time;
	ld->pause_interval = tx_flow_ctrl_pause_interval;
	ld->flow_ctrl_active_threshold = tx_flow_ctrl_active_threshold;
	ld->flow_ctrl_deactive_threshold = tx_flow_ctrl_deactive_threshold;
}

void gmac_set_flow_ctrl_params(struct gmac_netdev_local const *ld)
{
	unsigned int rx_fq_empty_th;
	unsigned int rx_fq_full_th;
	unsigned int rx_bq_empty_th;
	unsigned int rx_bq_full_th;
	unsigned int rec_filter;
	if (ld == NULL)
		return;
	writel(ld->pause, ld->gmac_iobase + FC_TX_TIMER);
	writel(ld->pause_interval, ld->gmac_iobase + PAUSE_THR);

	rx_fq_empty_th = readl(ld->gmac_iobase + RX_FQ_ALEMPTY_TH);
	rx_fq_empty_th &= ~(BITS_Q_PAUSE_TH_MASK << BITS_Q_PAUSE_TH_OFFSET);
	rx_fq_empty_th |= (ld->flow_ctrl_active_threshold <<
			BITS_Q_PAUSE_TH_OFFSET);
	writel(rx_fq_empty_th, ld->gmac_iobase + RX_FQ_ALEMPTY_TH);

	rx_fq_full_th = readl(ld->gmac_iobase + RX_FQ_ALFULL_TH);
	rx_fq_full_th &= ~(BITS_Q_PAUSE_TH_MASK << BITS_Q_PAUSE_TH_OFFSET);
	rx_fq_full_th |= (ld->flow_ctrl_deactive_threshold <<
			BITS_Q_PAUSE_TH_OFFSET);
	writel(rx_fq_full_th, ld->gmac_iobase + RX_FQ_ALFULL_TH);

	rx_bq_empty_th = readl(ld->gmac_iobase + RX_BQ_ALEMPTY_TH);
	rx_bq_empty_th &= ~(BITS_Q_PAUSE_TH_MASK << BITS_Q_PAUSE_TH_OFFSET);
	rx_bq_empty_th |= (ld->flow_ctrl_active_threshold <<
			BITS_Q_PAUSE_TH_OFFSET);
	writel(rx_bq_empty_th, ld->gmac_iobase + RX_BQ_ALEMPTY_TH);

	rx_bq_full_th = readl(ld->gmac_iobase + RX_BQ_ALFULL_TH);
	rx_bq_full_th &= ~(BITS_Q_PAUSE_TH_MASK << BITS_Q_PAUSE_TH_OFFSET);
	rx_bq_full_th |= (ld->flow_ctrl_deactive_threshold <<
			BITS_Q_PAUSE_TH_OFFSET);
	writel(rx_bq_full_th, ld->gmac_iobase + RX_BQ_ALFULL_TH);

	writel(0, ld->gmac_iobase + CRF_TX_PAUSE);

	rec_filter = readl(ld->gmac_iobase + REC_FILT_CONTROL);
	rec_filter |= BIT_PAUSE_FRM_PASS;
	writel(rec_filter, ld->gmac_iobase + REC_FILT_CONTROL);
}

void gmac_set_flow_ctrl_state(struct gmac_netdev_local const *ld, int pause)
{
	unsigned int flow_rx_q_en;
	unsigned int flow;
	if (ld == NULL)
		return;
	flow_rx_q_en = readl(ld->gmac_iobase + RX_PAUSE_EN);
	flow_rx_q_en &= ~(BIT_RX_FQ_PAUSE_EN | BIT_RX_BQ_PAUSE_EN);
	if (pause && (ld->flow_ctrl & FLOW_TX))
		flow_rx_q_en |= (BIT_RX_FQ_PAUSE_EN | BIT_RX_BQ_PAUSE_EN);
	writel(flow_rx_q_en, ld->gmac_iobase + RX_PAUSE_EN);

	flow = readl(ld->gmac_iobase + PAUSE_EN);
	flow &= ~(BIT_RX_FDFC | BIT_TX_FDFC);
	if (pause) {
		if (ld->flow_ctrl & FLOW_RX)
			flow |= BIT_RX_FDFC;
		if (ld->flow_ctrl & FLOW_TX)
			flow |= BIT_TX_FDFC;
	}
	writel(flow, ld->gmac_iobase + PAUSE_EN);
}

static __be16 gmac_get_l3_proto(struct sk_buff *skb)
{
	__be16 l3_proto;

	l3_proto = skb->protocol;
	if (skb->protocol == htons(ETH_P_8021Q))
		l3_proto = vlan_get_protocol(skb);

	return l3_proto;
}

static unsigned int gmac_get_l4_proto(struct sk_buff *skb)
{
	__be16 l3_proto;
	unsigned int l4_proto = IPPROTO_MAX;

	l3_proto = gmac_get_l3_proto(skb);
	if (l3_proto == htons(ETH_P_IP))
		l4_proto = ip_hdr(skb)->protocol;
	else if (l3_proto == htons(ETH_P_IPV6))
		l4_proto = ipv6_hdr(skb)->nexthdr;

	return l4_proto;
}

static inline bool gmac_skb_is_ipv6(struct sk_buff *skb)
{
	return (gmac_get_l3_proto(skb) == htons(ETH_P_IPV6));
}

static inline bool gmac_skb_is_udp(struct sk_buff *skb)
{
	return (gmac_get_l4_proto(skb) == IPPROTO_UDP);
}

static int gmac_check_hw_capability_for_udp(struct sk_buff const *skb)
{
	struct ethhdr *eth;

	/* hardware can't dea with UFO broadcast packet */
	eth = (struct ethhdr *)(skb->data);
	if (skb_is_gso(skb) && is_broadcast_ether_addr(eth->h_dest))
		return -ENOTSUPP;

	return 0;
}

static int gmac_check_hw_capability_for_ipv6(struct sk_buff *skb)
{
	unsigned int l4_proto;

	l4_proto = ipv6_hdr(skb)->nexthdr;
	if ((l4_proto != IPPROTO_TCP) && (l4_proto != IPPROTO_UDP)) {
		/*
		 * when IPv6 next header is not tcp or udp,
		 * it means that IPv6 next header is extension header.
		 * Hardware can't deal with this case,
		 * so do checksumming by software or do GSO by software.
		 */
		if (skb_is_gso(skb))
			return -ENOTSUPP;

		if (skb->ip_summed == CHECKSUM_PARTIAL &&
		    skb_checksum_help(skb))
			return -EFAULT;
	}

	return 0;
}

static inline bool gmac_skb_is_ipv4_with_options(struct sk_buff *skb)
{
	return ((gmac_get_l3_proto(skb) == htons(ETH_P_IP)) &&
		(ip_hdr(skb)->ihl > IPV4_HEAD_LENGTH));
}

int gmac_check_hw_capability(struct sk_buff *skb)
{
	int ret;

	/*
	 * if tcp_mtu_probe() use (2 * tp->mss_cache) as probe_size,
	 * the linear data length will be larger than 2048,
	 * the MAC can't handle it, so let the software do it.
	 */
	 if (skb == NULL)
		return -EINVAL;
	if (skb_is_gso(skb) && (skb_headlen(skb) > 2048)) /* 2048(2k) */
		return -ENOTSUPP;

	if (gmac_skb_is_ipv6(skb)) {
		ret = gmac_check_hw_capability_for_ipv6(skb);
		if (ret)
			return ret;
	}

	if (gmac_skb_is_udp(skb)) {
		ret = gmac_check_hw_capability_for_udp(skb);
		if (ret)
			return ret;
	}

	if (((skb->ip_summed == CHECKSUM_PARTIAL) || skb_is_gso(skb)) &&
	    gmac_skb_is_ipv4_with_options(skb))
		return -ENOTSUPP;

	return 0;
}

static void gmac_do_udp_checksum(struct sk_buff *skb)
{
	int offset;
	__wsum csum;
	__sum16 udp_csum;

	offset = skb_checksum_start_offset(skb);
	WARN_ON(offset >= skb_headlen(skb));
	csum = skb_checksum(skb, offset, skb->len - offset, 0);

	offset += skb->csum_offset;
	WARN_ON(offset + sizeof(__sum16) > skb_headlen(skb));
	udp_csum = csum_fold(csum);
	if (udp_csum == 0)
		udp_csum = CSUM_MANGLED_0;

	*(__sum16 *)(skb->data + offset) = udp_csum;

	skb->ip_summed = CHECKSUM_NONE;
}

static int gmac_get_pkt_info_l3l4(struct gmac_tso_desc *tx_bq_desc,
		  struct sk_buff *skb, unsigned int *l4_proto, unsigned int *max_mss,
		  unsigned char *coe_enable)
{
	__be16 l3_proto; /* level 3 protocol */
	int max_data_len = skb->len - ETH_HLEN;

	l3_proto = skb->protocol;
	if (skb->protocol == htons(ETH_P_8021Q)) {
		l3_proto = vlan_get_protocol(skb);
		tx_bq_desc->desc1.tx.vlan_flag = 1;
		max_data_len -= VLAN_HLEN;
	}

	if (l3_proto == htons(ETH_P_IP)) {
		struct iphdr *iph;

		iph = ip_hdr(skb);
		tx_bq_desc->desc1.tx.ip_ver = PKT_IPV4;
		tx_bq_desc->desc1.tx.ip_hdr_len = iph->ihl;

		if ((max_data_len >= GSO_MAX_SIZE) &&
			(ntohs(iph->tot_len) <= (iph->ihl << 2))) /* shift left 2 */
			iph->tot_len = htons(GSO_MAX_SIZE - 1);

		*max_mss -= iph->ihl * WORD_TO_BYTE;
		*l4_proto = iph->protocol;
	} else if (l3_proto == htons(ETH_P_IPV6)) {
		tx_bq_desc->desc1.tx.ip_ver = PKT_IPV6;
		tx_bq_desc->desc1.tx.ip_hdr_len = PKT_IPV6_HDR_LEN;
		*max_mss -= PKT_IPV6_HDR_LEN * WORD_TO_BYTE;
		*l4_proto = ipv6_hdr(skb)->nexthdr;
	} else {
		*coe_enable = 0;
	}

	if (*l4_proto == IPPROTO_TCP) {
		tx_bq_desc->desc1.tx.prot_type = PKT_TCP;
		if (tcp_hdr(skb)->doff < sizeof(struct tcphdr) / WORD_TO_BYTE)
			return -EFAULT;
		tx_bq_desc->desc1.tx.prot_hdr_len = tcp_hdr(skb)->doff;
		*max_mss -= tcp_hdr(skb)->doff * WORD_TO_BYTE;
	} else if (*l4_proto == IPPROTO_UDP) {
		tx_bq_desc->desc1.tx.prot_type = PKT_UDP;
		tx_bq_desc->desc1.tx.prot_hdr_len = PKT_UDP_HDR_LEN;
		if (l3_proto == htons(ETH_P_IPV6))
			*max_mss -= sizeof(struct frag_hdr);
	} else {
		*coe_enable = 0;
	}

	return 0;
}

int gmac_get_pkt_info(struct gmac_netdev_local *ld,
		  struct sk_buff *skb, struct gmac_tso_desc *tx_bq_desc)
{
	int nfrags;
	unsigned int l4_proto = IPPROTO_MAX;
	unsigned int max_mss = ETH_DATA_LEN;
	unsigned char coe_enable = 0;
	int ret;
	if (skb == NULL || tx_bq_desc == NULL)
		return -EINVAL;

	nfrags = skb_shinfo(skb)->nr_frags;
	if (likely(skb->ip_summed == CHECKSUM_PARTIAL))
		coe_enable = 1;

	tx_bq_desc->desc1.val = 0;

	if (skb_is_gso(skb)) {
		tx_bq_desc->desc1.tx.tso_flag = 1;
		tx_bq_desc->desc1.tx.sg_flag = 1;
	} else if (nfrags) {
		tx_bq_desc->desc1.tx.sg_flag = 1;
	}

	ret = gmac_get_pkt_info_l3l4(tx_bq_desc, skb, &l4_proto, &max_mss,
	    &coe_enable);
	if (ret < 0)
		return ret;

	if (skb_is_gso(skb))
		tx_bq_desc->desc1.tx.data_len =
			(skb_shinfo(skb)->gso_size > max_mss) ? max_mss :
					skb_shinfo(skb)->gso_size;
	else
		tx_bq_desc->desc1.tx.data_len = skb->len;

	if (coe_enable && skb_is_gso(skb) && (l4_proto == IPPROTO_UDP))
		gmac_do_udp_checksum(skb);

	if (coe_enable)
		tx_bq_desc->desc1.tx.coe_flag = 1;

	tx_bq_desc->desc1.tx.nfrags_num = nfrags;

	tx_bq_desc->desc1.tx.hw_own = DESC_VLD_BUSY;
	return 0;
}

void gmac_get_drvinfo(struct net_device *net_dev,
		  struct ethtool_drvinfo *info)
{
	if (info == NULL)
		return;
	if (strncpy_s(info->driver, sizeof(info->driver), "gmac driver", sizeof("gmac driver")))
		printk("strncpy_s  err : %s %d.\n", __func__, __LINE__);
	if (strncpy_s(info->version, sizeof(info->version), "gmac v200", sizeof("gmac v200")))
		printk("strncpy_s  err : %s %d.\n", __func__, __LINE__);
	if (strncpy_s(info->bus_info, sizeof(info->bus_info), "platform", sizeof("platform")))
		printk("strncpy_s  err : %s %d.\n", __func__, __LINE__);
}

unsigned int gmac_get_link(struct net_device *net_dev)
{
	struct gmac_netdev_local *ld = netdev_priv(net_dev);

	return ld->phy->link ? GMAC_LINKED : 0;
}

int gmac_get_settings(struct net_device *net_dev,
		  struct ethtool_cmd *cmd)
{
	struct gmac_netdev_local *ld = netdev_priv(net_dev);

	if (ld->phy != NULL)
		return phy_ethtool_gset(ld->phy, cmd);

	return -EINVAL;
}

int gmac_set_settings(struct net_device *net_dev,
		  struct ethtool_cmd *cmd)
{
	struct gmac_netdev_local *ld = netdev_priv(net_dev);

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	if (ld->phy != NULL)
		return phy_ethtool_sset(ld->phy, cmd);

	return -EINVAL;
}

void gmac_get_pauseparam(struct net_device *net_dev,
		  struct ethtool_pauseparam *pause)
{
	struct gmac_netdev_local *ld = NULL;
	if (net_dev == NULL || pause == NULL)
		return;
	ld = netdev_priv(net_dev);

	pause->rx_pause = 0;
	pause->tx_pause = 0;
	pause->autoneg = ld->phy->autoneg;

	if (ld->flow_ctrl & FLOW_RX)
		pause->rx_pause = 1;
	if (ld->flow_ctrl & FLOW_TX)
		pause->tx_pause = 1;
}

int gmac_set_pauseparam(struct net_device *net_dev,
		  struct ethtool_pauseparam *pause)
{
	struct gmac_netdev_local *ld = netdev_priv(net_dev);
	struct phy_device *phy = ld->phy;
	unsigned int new_pause = FLOW_OFF;

	if (pause == NULL)
		return -ENOMEM;

	if (pause->rx_pause)
		new_pause |= FLOW_RX;
	if (pause->tx_pause)
		new_pause |= FLOW_TX;

	if (new_pause != ld->flow_ctrl)
		ld->flow_ctrl = new_pause;

	gmac_set_flow_ctrl_state(ld, phy->pause);
	phy->advertising &= ~SUPPORTED_Pause;
	if (ld->flow_ctrl)
		phy->advertising |= SUPPORTED_Pause;

	if (phy->autoneg) {
		if (netif_running(net_dev))
			return phy_start_aneg(phy);
	}

	return 0;
}

u32 gmac_ethtool_getmsglevel(struct net_device *ndev)
{
	struct gmac_netdev_local *priv = netdev_priv(ndev);

	return priv->msg_enable;
}

void gmac_ethtool_setmsglevel(struct net_device *ndev, u32 level)
{
	struct gmac_netdev_local *priv = netdev_priv(ndev);

	priv->msg_enable = level;
}

u32 gmac_get_rxfh_key_size(struct net_device *ndev)
{
	return RSS_HASH_KEY_SIZE;
}

u32 gmac_get_rxfh_indir_size(struct net_device *ndev)
{
	struct gmac_netdev_local *priv = netdev_priv(ndev);

	return priv->rss_info.ind_tbl_size;
}

int gmac_get_rxfh(struct net_device *ndev, u32 *indir, u8 *hkey,
		  u8 *hfunc)
{
	struct gmac_netdev_local *priv = netdev_priv(ndev);
	struct gmac_rss_info *rss = &priv->rss_info;

	if (hfunc != NULL)
		*hfunc = ETH_RSS_HASH_TOP;

	if (hkey != NULL)
		if (memcpy_s(hkey, RSS_HASH_KEY_SIZE, rss->key, RSS_HASH_KEY_SIZE) < 0)
			printk("memcpy_s  err : %s %d.\n", __func__, __LINE__);

	if (indir != NULL) {
		int i;

		for (i = 0; i < rss->ind_tbl_size; i++)
			indir[i] = rss->ind_tbl[i];
	}

	return 0;
}

void gmac_get_rss_key(struct gmac_netdev_local *priv)
{
	struct gmac_rss_info *rss = NULL;
	u32 hkey;
	if (priv == NULL)
		return;
	rss = &priv->rss_info;
	hkey = readl(priv->gmac_iobase + RSS_HASH_KEY);
	*((u32 *)rss->key) = hkey;
}

static void gmac_set_rss_key(struct gmac_netdev_local *priv)
{
	struct gmac_rss_info *rss = &priv->rss_info;

	writel(*((u32 *)rss->key), priv->gmac_iobase + RSS_HASH_KEY);
}

static int gmac_wait_rss_ready(struct gmac_netdev_local const *priv)
{
	void __iomem *base = priv->gmac_iobase;
	int i;
	const int timeout = 10000;

	for (i = 0; !(readl(base + RSS_IND_TBL) & BIT_IND_TBL_READY); i++) {
		if (i == timeout) {
			netdev_err(priv->netdev, "wait rss ready timeout!\n");
			return -ETIMEDOUT;
		}
		usleep_range(10, 20); /* wait 10~20us */
	}

	return 0;
}

static void gmac_config_rss(struct gmac_netdev_local *priv)
{
	struct gmac_rss_info *rss = NULL;
	u32 rss_val;
	unsigned int i;
	if (priv == NULL)
		return;
	rss = &priv->rss_info;
	for (i = 0; i < rss->ind_tbl_size; i++) {
		if (gmac_wait_rss_ready(priv) != 0)
			break;
		rss_val = BIT_IND_TLB_WR | (rss->ind_tbl[i] << 8) | i; /* shift 8 */
		writel(rss_val, priv->gmac_iobase + RSS_IND_TBL);
	}
}

void gmac_get_rss(struct gmac_netdev_local *priv)
{
	struct gmac_rss_info *rss = NULL;
	u32 rss_val;
	int i;
	if (priv == NULL)
		return;
	rss = &priv->rss_info;
	for (i = 0; i < rss->ind_tbl_size; i++) {
		if (gmac_wait_rss_ready(priv) != 0)
			break;
		writel(i, priv->gmac_iobase + RSS_IND_TBL);
		if (gmac_wait_rss_ready(priv) != 0)
			break;
		rss_val = readl(priv->gmac_iobase + RSS_IND_TBL);
		rss->ind_tbl[i] = (rss_val >> 10) & 0x3; /* right shift 10 */
	}
}

int gmac_set_rxfh(struct net_device *ndev, const u32 *indir,
		  const u8 *hkey, const u8 hfunc)
{
	struct gmac_netdev_local *priv = netdev_priv(ndev);
	struct gmac_rss_info *rss = &priv->rss_info;

	if (hfunc != ETH_RSS_HASH_NO_CHANGE && hfunc != ETH_RSS_HASH_TOP)
		return -EOPNOTSUPP;

	if (indir != NULL) {
		int i;

		for (i = 0; i < rss->ind_tbl_size; i++)
			rss->ind_tbl[i] = indir[i];
	}

	if (hkey != NULL) {
		if (memcpy_s(rss->key, RSS_HASH_KEY_SIZE, hkey, RSS_HASH_KEY_SIZE) < 0)
			printk("memcpy_s  err : %s %d.\n", __func__, __LINE__);
		gmac_set_rss_key(priv);
	}

	gmac_config_rss(priv);

	return 0;
}

static void gmac_get_rss_hash(struct ethtool_rxnfc *info, u32 hash_cfg,
		  u32 l3_hash_en, u32 l4_hash_en, u32 vlan_hash_en)
{
	if (hash_cfg & l3_hash_en)
		info->data |= RXH_IP_SRC | RXH_IP_DST;
	if (hash_cfg & l4_hash_en)
		info->data |= RXH_L4_B_0_1 | RXH_L4_B_2_3;
	if (hash_cfg & vlan_hash_en)
		info->data |= RXH_VLAN;
}

static int gmac_get_rss_hash_opts(struct gmac_netdev_local const *priv,
		  struct ethtool_rxnfc *info)
{
	u32 hash_cfg = priv->rss_info.hash_cfg;

	info->data = 0;

	switch (info->flow_type) {
	case TCP_V4_FLOW:
		gmac_get_rss_hash(info, hash_cfg, TCPV4_L3_HASH_EN, TCPV4_L4_HASH_EN,
			TCPV4_VLAN_HASH_EN);
		break;
	case TCP_V6_FLOW:
		gmac_get_rss_hash(info, hash_cfg, TCPV6_L3_HASH_EN, TCPV6_L4_HASH_EN,
			TCPV6_VLAN_HASH_EN);
		break;
	case UDP_V4_FLOW:
		gmac_get_rss_hash(info, hash_cfg, UDPV4_L3_HASH_EN, UDPV4_L4_HASH_EN,
			UDPV4_VLAN_HASH_EN);
		break;
	case UDP_V6_FLOW:
		gmac_get_rss_hash(info, hash_cfg, UDPV6_L3_HASH_EN, UDPV6_L4_HASH_EN,
			UDPV6_VLAN_HASH_EN);
		break;
	case IPV4_FLOW:
		gmac_get_rss_hash(info, hash_cfg, IPV4_L3_HASH_EN, 0,
			IPV4_VLAN_HASH_EN);
		break;
	case IPV6_FLOW:
		gmac_get_rss_hash(info, hash_cfg, IPV6_L3_HASH_EN, 0,
			IPV6_VLAN_HASH_EN);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

int gmac_get_rxnfc(struct net_device *ndev,
		  struct ethtool_rxnfc *info, u32 *rules)
{
	struct gmac_netdev_local *priv = netdev_priv(ndev);
	int ret = -EOPNOTSUPP;
	if (info == NULL)
		return -EINVAL;
	switch (info->cmd) {
	case ETHTOOL_GRXRINGS:
		info->data = priv->num_rxqs;
		ret = 0;
		break;
	case ETHTOOL_GRXFH:
		return gmac_get_rss_hash_opts(priv, info);
	default:
		break;
	}
	return ret;
}

void gmac_config_hash_policy(struct gmac_netdev_local const *priv)
{
	if (priv == NULL)
		return;
	writel(priv->rss_info.hash_cfg, priv->gmac_iobase + RSS_HASH_CONFIG);
}

static int gmac_set_tcp_udp_hash_cfg(struct ethtool_rxnfc const *info,
		  u32 *hash_cfg, u32 l4_mask, u32 vlan_mask)
{
	switch (info->data & (RXH_L4_B_0_1 | RXH_L4_B_2_3)) {
	case 0: // all bits is 0
		*hash_cfg &= ~l4_mask;
		break;
	case (RXH_L4_B_0_1 | RXH_L4_B_2_3):
		*hash_cfg |= l4_mask;
		break;
	default:
		return -EINVAL;
	}
	if (info->data & RXH_VLAN)
		*hash_cfg |= vlan_mask;
	else
		*hash_cfg &= ~vlan_mask;
	return 0;
}

static int gmac_ip_hash_cfg(struct ethtool_rxnfc const *info,
		  u32 *hash_cfg, u32 vlan_mask)
{
	if (info->data & (RXH_L4_B_0_1 | RXH_L4_B_2_3))
		return -EINVAL;
	if (info->data & RXH_VLAN)
		*hash_cfg |= vlan_mask;
	else
		*hash_cfg &= ~vlan_mask;
	return 0;
}

static int gmac_set_rss_hash_opts(struct gmac_netdev_local *priv,
		  struct ethtool_rxnfc const *info)
{
	u32 hash_cfg;
	if (priv == NULL || priv->netdev == NULL)
		return -EINVAL;
	hash_cfg = priv->rss_info.hash_cfg;
	netdev_info(priv->netdev, "Set RSS flow type = %d, data = %lld\n",
		    info->flow_type, info->data);

	if (!(info->data & RXH_IP_SRC) || !(info->data & RXH_IP_DST))
		return -EINVAL;

	switch (info->flow_type) {
	case TCP_V4_FLOW:
		if (gmac_set_tcp_udp_hash_cfg(info, &hash_cfg,
			TCPV4_L4_HASH_EN, TCPV4_VLAN_HASH_EN) == -EINVAL)
			return -EINVAL;
		break;
	case TCP_V6_FLOW:
		if (gmac_set_tcp_udp_hash_cfg(info, &hash_cfg,
			TCPV6_L4_HASH_EN, TCPV6_VLAN_HASH_EN) == -EINVAL)
			return -EINVAL;
		break;
	case UDP_V4_FLOW:
		if (gmac_set_tcp_udp_hash_cfg(info, &hash_cfg,
			UDPV4_L4_HASH_EN, UDPV4_L4_HASH_EN) == -EINVAL)
			return -EINVAL;
		break;
	case UDP_V6_FLOW:
		if (gmac_set_tcp_udp_hash_cfg(info, &hash_cfg,
			UDPV6_L4_HASH_EN, UDPV6_L4_HASH_EN) == -EINVAL)
			return -EINVAL;
		break;
	case IPV4_FLOW:
		if (gmac_ip_hash_cfg(info, &hash_cfg,
			IPV4_VLAN_HASH_EN) == -EINVAL)
			return -EINVAL;
		break;
		break;
	case IPV6_FLOW:
		if (gmac_ip_hash_cfg(info, &hash_cfg,
			IPV6_VLAN_HASH_EN) == -EINVAL)
			return -EINVAL;
		break;
	default:
		return -EINVAL;
	}

	priv->rss_info.hash_cfg = hash_cfg;
	gmac_config_hash_policy(priv);

	return 0;
}

int gmac_set_rxnfc(struct net_device *ndev, struct ethtool_rxnfc *info)
{
	struct gmac_netdev_local *priv = netdev_priv(ndev);
	if (info == NULL)
		return -EINVAL;
	switch (info->cmd) {
	case ETHTOOL_SRXFH:
		return gmac_set_rss_hash_opts(priv, info);
	default:
		break;
	}
	return -EOPNOTSUPP;
}

static int ksz8051mnl_phy_fix(struct phy_device *phy_dev)
{
	u32 v;
	int ret;

	if (phy_dev->interface != PHY_INTERFACE_MODE_RMII)
		return 0;

	ret = phy_read(phy_dev, 0x1F);
	if (ret < 0)
		return ret;
	v = ret;
	v |= (1 << 7); /* set bit 7, phy RMII 50MHz clk; */
	phy_write(phy_dev, 0x1F, v);

	ret = phy_read(phy_dev, 0x16);
	if (ret < 0)
		return ret;
	v = ret;
	v |= (1 << 1); /* set phy RMII override; */
	phy_write(phy_dev, 0x16, v);

	return 0;
}

static int ksz8081rnb_phy_fix(struct phy_device *phy_dev)
{
	u32 v;
	int ret;

	if (phy_dev->interface != PHY_INTERFACE_MODE_RMII)
		return 0;

	ret = phy_read(phy_dev, 0x1F);
	if (ret < 0)
		return ret;
	v = ret;
	v |= (1 << 7); /* set bit 7, phy RMII 50MHz clk; */
	phy_write(phy_dev, 0x1F, v);

	return 0;
}

static int unknown_phy_fix(struct phy_device *phy_dev)
{
	u32 v;
	int ret;

	if (phy_dev->interface != PHY_INTERFACE_MODE_RMII)
		return 0;

	ret = phy_read(phy_dev, 0x1F);
	if (ret < 0)
		return ret;
	v = ret;
	v |= (1 << 7); /* set bit 7, phy RMII 50MHz clk; */
	phy_write(phy_dev, 0x1F, v);

	return 0;
}

static int rtl8211e_phy_fix(struct phy_device *phy_dev)
{
	u32 v;
	int ret;

	/* select Extension page */
	phy_write(phy_dev, 0x1f, 0x7);
	/* switch ExtPage 164 */
	phy_write(phy_dev, 0x1e, 0xa4);

	/* config RGMII rx pin io driver max */
	ret = phy_read(phy_dev, 0x1c);
	if (ret < 0)
		return ret;
	v = ret;
	v = (v & 0xff03) | 0xfc;
	phy_write(phy_dev, 0x1c, v);

	/* select to page 0 */
	phy_write(phy_dev, 0x1f, 0);

	return 0;
}

void gmac_phy_register_fixups(void)
{
	phy_register_fixup_for_uid(PHY_ID_UNKNOWN, DEFAULT_PHY_MASK,
				   unknown_phy_fix);
	phy_register_fixup_for_uid(PHY_ID_KSZ8051MNL, DEFAULT_PHY_MASK,
				   ksz8051mnl_phy_fix);
	phy_register_fixup_for_uid(PHY_ID_KSZ8081RNB, DEFAULT_PHY_MASK,
				   ksz8081rnb_phy_fix);
	phy_register_fixup_for_uid(REALTEK_PHY_ID_8211E, REALTEK_PHY_MASK,
				   rtl8211e_phy_fix);
}

void gmac_phy_unregister_fixups(void)
{
	phy_unregister_fixup_for_uid(PHY_ID_UNKNOWN, DEFAULT_PHY_MASK);
	phy_unregister_fixup_for_uid(PHY_ID_KSZ8051MNL, DEFAULT_PHY_MASK);
	phy_unregister_fixup_for_uid(PHY_ID_KSZ8081RNB, DEFAULT_PHY_MASK);
	phy_unregister_fixup_for_uid(REALTEK_PHY_ID_8211E, REALTEK_PHY_MASK);
}
