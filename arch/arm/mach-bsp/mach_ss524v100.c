/*
 * Copyright (c) 2019-2020 Shenshu Technologies Co., Ltd.
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
 *
*/

#include <linux/of_address.h>
#include <asm/smp_scu.h>

#include "mach_common.h"

#ifdef CONFIG_SMP

#define REG_CPU1_SRST_CRG    0x204c
#define CPU1_SRST_REQ       BIT(1)
#define DBG1_SRST_REQ       BIT(0)

#define REG_CPU2_SRST_CRG    0x2050
#define CPU2_SRST_REQ       BIT(1)
#define DBG2_SRST_REQ       BIT(0)

#define REG_CPU3_SRST_CRG    0x2054
#define CPU3_SRST_REQ       BIT(1)
#define DBG3_SRST_REQ       BIT(0)

void bsp_set_cpu(unsigned int cpu, bool enable)
{
	struct device_node *np = NULL;
	unsigned int regval;
	void __iomem *crg_base;

	np = of_find_compatible_node(NULL, NULL, "vendor,ss524v100-clock");
	if (!np) {
		pr_err("failed to find clock node\n");
		return;
	}

	crg_base = of_iomap(np, 0);
	if (!crg_base) {
		pr_err("failed to map address\n");
		return;
	}

	switch (cpu) {
	case 1:
		if (enable) {
			regval = readl(crg_base + REG_CPU1_SRST_CRG);
			regval &= ~CPU1_SRST_REQ;
			writel(regval, (crg_base + REG_CPU1_SRST_CRG));
		} else {
			regval = readl(crg_base + REG_CPU1_SRST_CRG);
			regval |= (DBG1_SRST_REQ | CPU1_SRST_REQ);
			writel(regval, (crg_base + REG_CPU1_SRST_CRG));
		}
		break;

	case 2: /* 2th cpu */
		if (enable) {
			regval = readl(crg_base + REG_CPU2_SRST_CRG);
			regval &= ~CPU2_SRST_REQ;
			writel(regval, (crg_base + REG_CPU2_SRST_CRG));
		} else {
			regval = readl(crg_base + REG_CPU2_SRST_CRG);
			regval |= (DBG2_SRST_REQ | CPU2_SRST_REQ);
			writel(regval, (crg_base + REG_CPU2_SRST_CRG));
		}
		break;

	case 3: /* 3th cpu */
		if (enable) {
			regval = readl(crg_base + REG_CPU3_SRST_CRG);
			regval &= ~CPU3_SRST_REQ;
			writel(regval, (crg_base + REG_CPU3_SRST_CRG));
		} else {
			regval = readl(crg_base + REG_CPU3_SRST_CRG);
			regval |= (DBG3_SRST_REQ | CPU3_SRST_REQ);
			writel(regval, (crg_base + REG_CPU3_SRST_CRG));
		}
		break;

	default:
		break;
	}

	if (crg_base) {
		iounmap(crg_base);
		crg_base = NULL;
	}
}

static const struct smp_operations bsp_smp_ops __initconst = {
	.smp_prepare_cpus       = bsp_smp_prepare_cpus,
	.smp_boot_secondary     = bsp_boot_secondary,
};

CPU_METHOD_OF_DECLARE(ss524v100_smp, "vendor,ss524v100",
		      &bsp_smp_ops);
#endif /* CONFIG_SMP */
