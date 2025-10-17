/*
 * Copyright (c) Shenshu Technologies Co., Ltd. 2016-2020. All rights reserved.
 * Description: mci header
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

/*
 *  MCI connection table manager
 */
#ifndef __MCI_PROC_H__
#define __MCI_PROC_H__

#include <linux/proc_fs.h>

#define MAX_CARD_TYPE	4
#define MAX_SPEED_MODE	5

#if defined(CONFIG_ARCH_SS528V100) || defined(CONFIG_ARCH_SS625V100) ||\
	defined(CONFIG_ARCH_SS524V100) || defined(CONFIG_ARCH_SS522V100) || \
	defined(CONFIG_ARCH_SS615V100) || defined(CONFIG_ARCH_SS522V101)
#define MCI_SLOT_NUM 1
#elif defined(CONFIG_ARCH_SS919V100) || defined(CONFIG_ARCH_SS015V100)
#define MCI_SLOT_NUM 4
#elif defined(CONFIG_ARCH_SS318V100) || defined(CONFIG_ARCH_SS918V100) ||\
    defined(CONFIG_ARCH_SS101V200) || defined(CONFIG_ARCH_SS101V500) ||\
	defined(CONFIG_ARCH_SS101V300) || defined(CONFIG_ARCH_SS101V600) ||\
	defined(CONFIG_ARCH_SS013V100)  || defined(CONFIG_ARCH_SS928V100) ||\
	defined(CONFIG_ARCH_SS927V100)
#define MCI_SLOT_NUM 3
#else
#error MCI_SLOT_NUM should not be zero!
#endif

extern unsigned int slot_index;
extern struct mmc_host *mci_host[MCI_SLOT_NUM];

int mci_proc_init(void);
int mci_proc_shutdown(void);

#endif /*  __MCI_PROC_H__ */
