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

#ifndef __GMAC_SOCKIOCTL_H__
#define __GMAC_SOCKIOCTL_H__

#include <linux/sockios.h>

#define SIOCSETPM	(SIOCDEVPRIVATE + 4) /* set pmt wake up config */
#define SIOCSETSUSPEND	(SIOCDEVPRIVATE + 5) /* call dev->suspend, debug */
#define SIOCSETRESUME	(SIOCDEVPRIVATE + 6) /* call dev->resume, debug */

int gmac_ioctl(struct net_device *net_dev, struct ifreq *rq, int cmd);

#endif
