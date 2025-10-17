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

#ifndef __MDIO_BSP_GEMAC_H__
#define  __MDIO_BSP_GEMAC_H__

#if defined(CONFIG_ARCH_SS528V100) || defined(CONFIG_ARCH_SS625V100)
int bsp_gemac_pinctrl_config(struct platform_device *pdev);
#else
static inline int bsp_gemac_pinctrl_config(struct platform_device *pdev)
{
	return 0;
}
#endif

#endif /* __MDIO_BSP_GEMAC_H__ */
