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

#ifndef __GMAC_TSO_H__
#define __GMAC_TSO_H__

#define SG_FLAG		BIT(30)
#define COE_FLAG	BIT(29)
#define TSO_FLAG	BIT(28)
#define VLAN_FLAG	BIT(10)
#define IPV6_FLAG	BIT(9)
#define UDP_FLAG	BIT(8)

#define PKT_IPV6_HDR_LEN	10
#define PKT_UDP_HDR_LEN		2
#define WORD_TO_BYTE		4
enum {
	PKT_NORMAL,
	PKT_SG
};

enum {
	PKT_IPV4,
	PKT_IPV6
};

enum {
	PKT_TCP,
	PKT_UDP
};

struct frags_info {
	/* Word(2*i+2) */
	u32 addr;
	/* Word(2*i+3) */
	u32 size : 16;
	u32 reserved : 16;
};

struct sg_desc {
	/* Word0 */
	u32 total_len : 17;
	u32 reserv : 15;
	/* Word1 */
	u32 ipv6_id;
	/* Word2 */
	u32 linear_addr;
	/* Word3 */
	u32 linear_len : 16;
	u32 reserv3 : 16;
	/* MAX_SKB_FRAGS is 18 */
	struct frags_info frags[18];
};

#endif
