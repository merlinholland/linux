/*
 *
 * Copyright (c) 2015-2021 Shenshu Technologies Co., Ltd.
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

/*
 *  MCI connection table manager
 */
#ifndef __MCI_PROC_H__
#define __MCI_PROC_H__

#define MAX_CARD_TYPE 4
#define MAX_SPEED_MODE 5
#define MMC_STATE_BLOCKADDR (1<<2) /* card uses block-addressing copy from core/card.h */
#define MMC_CARD_SDXC (1<<3) /* card is SDXC copy from core/card.h */
#define MCI_SLOT_NUM 2

extern struct mci_host *mci_host[MCI_SLOT_NUM];
int mci_proc_init(void);
int mci_proc_shutdown(void);

#endif /*  __MCI_PROC_H__ */
