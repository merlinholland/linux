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

#ifndef __GMAC_UTIL_H__
#define __GMAC_UTIL_H__

#include "gmac.h"

#define GMAC_TRACE_LEVEL 10
#define GMAC_NORMAL_LEVEL 7

#define mk_bits(shift, nbits) ((((shift) & 0x1F) << 16) | ((nbits) & 0x3F))

#define FC_ACTIVE_MIN		1
#define FC_ACTIVE_DEFAULT	16
#define FC_ACTIVE_MAX		127
#define FC_DEACTIVE_MIN		1
#define FC_DEACTIVE_DEFAULT	32
#define FC_DEACTIVE_MAX		127

#define FC_PAUSE_TIME_DEFAULT		0xFFFF
#define FC_PAUSE_INTERVAL_DEFAULT	0xFFFF
#define FC_PAUSE_TIME_MAX		0xFFFF

#define HW_CAP_EN			0x0c00
#define BIT_RSS_CAP			BIT(0)
#define BIT_RXHASH_CAP			BIT(1)
#define RSS_HASH_KEY			0x0c04
#define RSS_HASH_CONFIG			0x0c08
#define TCPV4_L3_HASH_EN		BIT(0)
#define TCPV4_L4_HASH_EN		BIT(1)
#define TCPV4_VLAN_HASH_EN		BIT(2)
#define UDPV4_L3_HASH_EN		BIT(4)
#define UDPV4_L4_HASH_EN		BIT(5)
#define UDPV4_VLAN_HASH_EN		BIT(6)
#define IPV4_L3_HASH_EN			BIT(8)
#define IPV4_VLAN_HASH_EN		BIT(9)
#define TCPV6_L3_HASH_EN		BIT(12)
#define TCPV6_L4_HASH_EN		BIT(13)
#define TCPV6_VLAN_HASH_EN		BIT(14)
#define UDPV6_L3_HASH_EN		BIT(16)
#define UDPV6_L4_HASH_EN		BIT(17)
#define UDPV6_VLAN_HASH_EN		BIT(18)
#define IPV6_L3_HASH_EN			BIT(20)
#define IPV6_VLAN_HASH_EN		BIT(21)
#define DEF_HASH_CFG			0x377377

#define RGMII_SPEED_1000		0x2c
#define RGMII_SPEED_100			0x2f
#define RGMII_SPEED_10			0x2d
#define MII_SPEED_100			0x0f
#define MII_SPEED_10			0x0d
#define RMII_SPEED_100			0x8f
#define RMII_SPEED_10			0x8d
#define GMAC_FULL_DUPLEX		BIT(4)

void gmac_trace(int level, const char *fmt, ...);

void gmac_config_port(struct net_device const *dev, u32 speed, u32 duplex);
int gmac_rx_checksum(struct net_device *dev, struct sk_buff *skb,
		  struct gmac_desc const *desc);
void gmac_verify_flow_ctrl_args(void);
void gmac_set_flow_ctrl_args(struct gmac_netdev_local *ld);

int gmac_check_hw_capability(struct sk_buff *skb);
int gmac_get_pkt_info(struct gmac_netdev_local *ld,
			struct sk_buff *skb, struct gmac_tso_desc *tx_bq_desc);

void gmac_get_drvinfo(struct net_device *net_dev,
			struct ethtool_drvinfo *info);
unsigned int gmac_get_link(struct net_device *net_dev);
int gmac_get_settings(struct net_device *net_dev, struct ethtool_cmd *cmd);
int gmac_set_settings(struct net_device *net_dev, struct ethtool_cmd *cmd);
void gmac_get_pauseparam(struct net_device *net_dev,
			   struct ethtool_pauseparam *pause);
int gmac_set_pauseparam(struct net_device *net_dev,
			  struct ethtool_pauseparam *pause);
u32 gmac_ethtool_getmsglevel(struct net_device *ndev);
void gmac_ethtool_setmsglevel(struct net_device *ndev, u32 level);
u32 gmac_get_rxfh_key_size(struct net_device *ndev);
u32 gmac_get_rxfh_indir_size(struct net_device *ndev);
int gmac_get_rxfh(struct net_device *ndev, u32 *indir, u8 *hkey, u8 *hfunc);
int gmac_set_rxfh(struct net_device *ndev, const u32 *indir,
		    const u8 *hkey, const u8 hfunc);
int gmac_get_rxnfc(struct net_device *ndev,
		     struct ethtool_rxnfc *info, u32 *rules);
int gmac_set_rxnfc(struct net_device *ndev, struct ethtool_rxnfc *info);

void gmac_get_rss_key(struct gmac_netdev_local *priv);
void gmac_get_rss(struct gmac_netdev_local *priv);
void gmac_config_hash_policy(struct gmac_netdev_local const *priv);

void gmac_set_flow_ctrl_state(struct gmac_netdev_local const *ld, int pause);
void gmac_set_flow_ctrl_params(struct gmac_netdev_local const *ld);

void gmac_phy_register_fixups(void);
void gmac_phy_unregister_fixups(void);
#endif
