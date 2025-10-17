/*
 * Copyright (c) 2015 Shenshu Technologies Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef	__BSP_RESET_H
#define	__BSP_RESET_H

struct device_node;
struct bsp_reset_controller;

#ifdef CONFIG_RESET_CONTROLLER
struct bsp_reset_controller *vendor_reset_init(struct platform_device *pdev);
#ifdef CONFIG_ARCH_BSP
int __init bsp_reset_init(struct device_node *np, int nr_rsts);
#endif
void bsp_reset_exit(struct bsp_reset_controller *rstc);
#else
static inline
struct bsp_reset_controller *vendor_reset_init(struct platform_device *pdev)
{
	return 0;
}
static inline void bsp_reset_exit(struct bsp_reset_controller *rstc)
{}
#endif

#endif	/* __BSP_RESET_H */
