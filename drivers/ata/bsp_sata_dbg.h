/*
 * Copyright (c) 2009-2014 Shenshu Technologies Co., Ltd.
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

#ifndef _BSP_SATA_DBG_H
#define _BSP_SATA_DBG_H
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/libata.h>
#include "ahci.h"

void bsp_sata_mem_dump(unsigned int *addr, unsigned int size);
void bsp_sata_phys_mem_dump(unsigned int addr, unsigned int size);
void bsp_ahci_rx_fis_dump(struct ata_link *link, int pmp_port_num);
void bsp_ata_taskfile_dump(struct ata_taskfile *tf);
void bsp_ahci_st_dump(const void __iomem *port_base);
void bsp_ahci_reg_dump(void);

#define bsp_ahci_reg_dump_dbg(x) \
do { \
	pr_debug("------------------[ Start ]--------------------\n"); \
	pr_debug("Dump AHCI registers at %s %d\n", __func__, __LINE__); \
	bsp_ahci_reg_dump(); \
	pr_debug("------------------[  End  ]--------------------\n"); \
} while (0)

#define bsp_sata_readl(addr) do { \
		unsigned int reg = readl((unsigned int)(addr)); \
		pr_debug("AHCI(REG) %s:%d: readl(0x%08X) = 0x%08X\n", \
		__func__, __LINE__, (unsigned int)(addr), (reg)); \
		reg; \
	} while (0)

#define bsp_sata_writel(v, addr) do { writel(v, (unsigned int)(addr)); \
	pr_debug("AHCI(REG) %s:%d: writel(0x%08X) = 0x%08X\n", \
		__func__, __LINE__, (unsigned int)(addr), \
		(unsigned int)(v)); \
	} while (0)

#undef BSP_DUMP_AHCI_REG_OPS
#ifdef BSP_DUMP_AHCI_REG_OPS
#define readl(addr) bsp_sata_readl(addr)
#define write(v, addr) bsp_sata_writel(v, addr)
#endif
#endif /* _BSP_SATA_DBG_H */
