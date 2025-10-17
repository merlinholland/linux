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

#include <asm/io.h>
#include <linux/spinlock.h>
#include <linux/vendor/peri_io.h>

static DEFINE_SPINLOCK(peri_lock);

unsigned long bsp_peri_lock(enum bsp_peri_type type)
{
	unsigned long flags = 0;

	switch(type) {
	case BSP_PERI_SDIO:
		spin_lock_irqsave(&peri_lock, flags);
	default:
		break;
	}
	return flags;
}

void bsp_peri_unlock(unsigned long flags, enum bsp_peri_type type)
{
	switch(type) {
	case BSP_PERI_SDIO:
		spin_unlock_irqrestore(&peri_lock, flags);
	default:
		break;
	}
}

