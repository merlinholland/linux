/*
 * Copyright (c) 2022-2022 Shenshu Technologies Co., Ltd.
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

#ifndef __PERIPH_IO_H
#define __PERIPH_IO_H

#include <asm/io.h>
#include <linux/types.h>

enum bsp_peri_type {
	BSP_PERI_NONE = 0,
	BSP_PERI_SDIO,
	BSP_PERI_MMC,
	BSP_PERI_USB,
	BSP_PERI_SATA,
	BSP_PERI_GMAC,
	BSP_PERI_FMC,
};

unsigned long bsp_peri_lock(enum bsp_peri_type type);
void bsp_peri_unlock(unsigned long flags, enum bsp_peri_type type);

static inline u32 bsp_peri_readl(const volatile void __iomem *addr,
				enum bsp_peri_type type)
{
	u32 val;
	unsigned long flags;
	flags = bsp_peri_lock(type);
	val = readl(addr);
	bsp_peri_unlock(flags, type);
	return val;
}

static inline void bsp_peri_writel(u32 val, volatile void __iomem *addr,
				enum bsp_peri_type type)
{
	unsigned long flags;
	flags = bsp_peri_lock(type);
	writel(val, addr);
	bsp_peri_unlock(flags, type);
}

#endif /* __PERIPH_IO_H */
