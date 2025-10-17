/*
 * Copyright (c) 2016-2017 Shenshu Technologies Co., Ltd.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/mbus.h>
#include <asm/irq.h>
#include <linux/of.h>
#include <linux/of_pci.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/version.h>
#include <asm/siginfo.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/msi.h>
#include <linux/of_irq.h>
#include "../pci.h"
#ifdef CONFIG_ARM64
#include "pci.h"
#endif

#define PCIE_DBG_REG		1
#define PCIE_DBG_FUNC		2
#define PCIE_DBG_MODULE		3

#define PCIE_DEBUG_LEVEL PCIE_DBG_MODULE

#ifdef PCIE_DEBUG
#define pcie_debug(level, str, arg...) \
	do { \
		if ((level) <= PCIE_DEBUG_LEVEL) { \
			pr_debug("%s->%d," str "\n", \
				__func__, __LINE__, ##arg); \
		} \
	} while (0)
#else
#define pcie_debug(level, str, arg...)
#endif

#define pcie_assert(con) \
	do { \
		if (!(con)) { \
			pr_err("%s->%d,assert fail!\n", \
				__func__, __LINE__); \
		} \
	} while (0)

#define pcie_error(str, arg...) \
	pr_err("%s->%d" str "\n", __func__, __LINE__, ##arg)

#define __256MB__ 0x10000000
#define __128MB__ 0x8000000
#define __4KB__ 0x1000
#define __8KB__ 0x2000
#define __16KB__ 0x4000

enum pcie_sel {
	/*
	 * No controller selected.
	 */
	PCIE_SEL_NONE,
	/*
	 * PCIE0 selected.
	 */
	PCIE0_X1_SEL,
	/*
	 * PCIE1 selected.
	 */
	PCIE1_X1_SEL
};

enum pcie_rc_sel {
	PCIE_CONTROLLER_UNSELECTED,
	PCIE_CONTROLLER_SELECTED
};

enum pcie_controller {
	PCIE_CONTROLLER_NONE = -1,
	PCIE_CONTROLLER_0 = 0,
	PCIE_CONTROLLER_1 = 1
};

struct pcie_iatu {
	unsigned int viewport;          /* iATU Viewport Register        */
	unsigned int region_ctrl_1;     /* Region Control 1 Register     */
	unsigned int region_ctrl_2;     /* Region Control 2 Register     */
	unsigned int lbar;              /* Lower Base Address Register   */
	unsigned int ubar;              /* Upper Base Address Register   */
	unsigned int lar;               /* Limit Address Register        */
	unsigned int ltar; /* Lower Target Address Register */
	unsigned int utar;              /* Upper Target Address Register */
};

struct pcie_property {
	unsigned int pcie_mem_size;
	unsigned int pcie_cfg_size;
	unsigned int pcie_dbi_base;
	unsigned int pcie_ep_conf_base;
	unsigned int pcie_contrl;
};

#define MAX_IATU_PER_CTRLLER	(6)

struct pcie_info {
	/*
	 * Root bus number
	 */
	int		root_bus_nr;
	enum		pcie_controller controller;

	/*
	 * Devices configuration space base
	 */
	unsigned long	base_addr;

	/*
	 * RC configuration space base
	 */
	unsigned long	conf_base_addr;
};

#define MAX_PCIE_CONTROLLER_NUM  2
static struct pcie_info pcie_info[MAX_PCIE_CONTROLLER_NUM] = {
	{ .root_bus_nr = -1, },
	{ .root_bus_nr = -1, }
};

static int pcie_controllers_nr;

static unsigned int pcie_errorvalue;

struct device_node *g_of_node = NULL;

static DEFINE_SPINLOCK(cw_lock);

#define MSI_CONTRL_INTERRUPT 0x830
#define MSI_CTRL_UPPER_ADDR_OFF 0x824
#define MSI_CTRL_ADDR_OFF 0x820
#define MSI_CTRL_INT_EN_OFF0 0x828
#define MSI_CTRL_INT_EN_OFF1 0x834
#define MSI_CTRL_INT_EN_OFF2 0x840
#define MSI_CTRL_INT_EN_OFF3 0x84c
#define MSI_CTRL_INT_EN_OFF4 0x858
#define MSI_CTRL_INT_EN_OFF5 0x864
#define MSI_CTRL_INT_EN_OFF6 0x870
#define MSI_CTRL_INT_EN_OFF7 0x87c

#define PCIE0_MODE_SEL  (1 << 0)
#define PCIE1_MODE_SEL  (1 << 1)

#if defined(CONFIG_ARCH_SS528V100)
#include "pcie_ss528v100.c"
#elif defined(CONFIG_ARCH_SS625V100)
#include "pcie_ss625v100.c"
#elif defined(CONFIG_ARCH_SS918V100)
#include "pcie_ss918v100.c"
#elif defined(CONFIG_ARCH_SS919V100)
#include "pcie_ss919v100.c"
#elif defined(CONFIG_ARCH_SS928V100)
#include "pcie_ss928v100.c"
#elif defined(CONFIG_ARCH_SS927V100)
#include "pcie_ss928v100.c"
#else
#error You must have defined CONFIG_ARCH_...
#endif

static struct pcie_info *bus_to_info(int busnr)
{
	int i = pcie_controllers_nr;
	for (; i >= 0; i--) {
		if (pcie_info[i].controller != PCIE_CONTROLLER_NONE
				&& pcie_info[i].root_bus_nr <= busnr
				&& pcie_info[i].root_bus_nr != -1)
			return &pcie_info[i];
	}

	return NULL;
}

#define pcie_cfg_bus(busnr) ((busnr & 0xff) << 20)
#define pcie_cfg_dev(devfn) ((devfn & 0xff) << 12)
#define pcie_cfg_reg(reg) (reg & 0xffc) /* set dword align */

static unsigned long to_pcie_address(struct pci_bus *bus,
		unsigned int devfn, int where)
{
	struct pcie_info *info = bus_to_info(bus->number);
	unsigned long address;

	if (unlikely(!info)) {
		pcie_error(
			"%s:Cannot find corresponding controller for appointed device!", __func__);
		BUG();
	}

	address = info->base_addr + (pcie_cfg_bus(bus->number) |
				 pcie_cfg_dev(devfn) | pcie_cfg_reg((unsigned int)where));

	return address;
}

static int is_pcie_link_up(struct pcie_info *info)
{
	int i;
	/* check up to 1000 times */
	for (i = 0; i < 10000; i++) {
		if (__arch_check_pcie_link(info))
			break;
		udelay(100);
	}

	return (i < 10000);
}

static int pcie_read_from_device(struct pci_bus *bus, unsigned int devfn,
				 int where, int size, u32 *value)
{
	struct pcie_info *info = bus_to_info(bus->number);
	unsigned int val;
	void __iomem *addr;
	int i;

	if (unlikely(!info)) {
		pcie_error(
			"%s:Cannot find corresponding controller for appointed device!", __func__);
		BUG();
	}
	if (!is_pcie_link_up(info)) {
		pcie_debug(PCIE_DBG_MODULE, "pcie %d not link up!",
			   info->controller);
		return -1;
	}
	/* where[11:2] repsent the reg_num for register addressing */
	addr = (void __iomem *)(uintptr_t)to_pcie_address(bus, devfn, where);

	val = readl(addr);

	/* nop 2000 times */
	i = 0;
	while (i < 2000) {
		__asm__ __volatile__("nop\n");
		i++;
	}

	if (pcie_errorvalue == 1) {
		pcie_errorvalue = 0;
		val = 0xffffffff;
	}

	if (size == 1) {
		*value = ((val >> (((unsigned int)where & 0x3) << 3)) & 0xff);
	} else if (size == 2) {
		*value = ((val >> (((unsigned int)where & 0x3) << 3)) & 0xffff);
	} else if (size == 4) {
		*value = val;
	} else {
		pcie_error("Unknown size %d for read ops", size);
		BUG();
	}

	return PCIBIOS_SUCCESSFUL;
}

static int pcie_read_from_dbi(struct pcie_info *info, unsigned int devfn,
			      int where, int size, u32 *value)
{
	unsigned int val;

	/*
	 * For host-side config space read, ignore device func nr.
	 */
	if (devfn > 0)
		return -EIO;

	val = (u32)readl((void *)(uintptr_t)(info->conf_base_addr +
					     ((unsigned int)where & (~0x3))));
	/* where[1:0] repsent which byte of the register should be read */
	if (size == 1) {
		*value = (val >> (((unsigned int)where & 0x3) << 3)) & 0xff;
	} else if (size == 2) {
		*value = (val >> (((unsigned int)where & 0x3) << 3)) & 0xffff;
	} else if (size == 4) {
		*value = val;
	} else {
		pcie_error("Unknown size for config read operation!");
		BUG();
	}

	return PCIBIOS_SUCCESSFUL;
}

static int pcie_read_conf(struct pci_bus *bus, unsigned int devfn,
			  int where, int size, u32 *value)
{
	struct pcie_info *info = bus_to_info(bus->number);
	int ret;

	if (unlikely(!info)) {
		pcie_error(
			"%s:Cannot find corresponding controller for appointed device!", __func__);
		BUG();
	}

	if (bus->number == info->root_bus_nr)
		ret =  pcie_read_from_dbi(info, devfn, where, size, value);
	else
		ret =  pcie_read_from_device(bus, devfn, where, size, value);

	pcie_debug(PCIE_DBG_REG,
		   "bus %d, devfn %d, where 0x%x, size 0x%x, value 0x%x",
		   bus->number & 0xff, devfn, where, size, *value);

	return ret;
}

static int pcie_write_to_device(struct pci_bus *bus, unsigned int devfn,
				int where, int size, u32 value)
{
	struct pcie_info *info = bus_to_info(bus->number);
	void __iomem *addr;
	unsigned int org;
	unsigned long flag;
	int ret;

	if (unlikely(!info)) {
		pcie_error(
			"%s:Cannot find corresponding controller for appointed device!", __func__);
		BUG();
	}

	if (!is_pcie_link_up(info)) {
		pcie_debug(PCIE_DBG_MODULE, "pcie %d not link up!",
			   info->controller);
		return -1;
	}

	spin_lock_irqsave(&cw_lock, flag);

	/* where[11:2] repsent the reg_num for register addressing */
	ret = pcie_read_from_device(bus, devfn, where, 4, &org);

	addr = (void __iomem *)(uintptr_t)to_pcie_address(bus, devfn, where);

	/* where[1:0] repsent which byte of the register should be read */
	if (size == 1) {
		org &= (~(0xff << (((unsigned int)where & 0x3) << 3)));
		org |= (value << (((unsigned int)where & 0x3) << 3));
	} else if (size == 2) {
		org &= (~(0xffff << (((unsigned int)where & 0x3) << 3)));
		org |= (value << (((unsigned int)where & 0x3) << 3));
	} else if (size == 4) {
		org = value;
	} else {
		pcie_error("Unknown size %d for read ops", size);
		BUG();
	}

	writel(org, addr);

	spin_unlock_irqrestore(&cw_lock, flag);

	return ret;
}

static int pcie_write_to_dbi(struct pcie_info *info, unsigned int devfn,
			     int where, int size, u32 value)
{
	unsigned long flag;
	unsigned int org;

	spin_lock_irqsave(&cw_lock, flag);
	/* where[11:2] repsent the reg_num for register addressing */
	if (pcie_read_from_dbi(info, devfn, (unsigned int)where, 4, &org)) {
		pcie_error("Cannot read from dbi! 0x%x:0x%x:0x%x!",
			   0, devfn, (unsigned int)where);
		spin_unlock_irqrestore(&cw_lock, flag);
		return -EIO;
	}
	/* where[1:0] repsent which byte of the register should be written */
	if (size == 1) {
		org &= (~(0xff << (((unsigned int)where & 0x3) << 3)));
		org |= (value << (((unsigned int)where & 0x3) << 3));
	} else if (size == 2) {
		org &= (~(0xffff << (((unsigned int)where & 0x3) << 3)));
		org |= (value << (((unsigned int)where & 0x3) << 3));
	} else if (size == 4) {
		org = value;
	} else {
		pcie_error("Unknown size %d for read ops", size);
		BUG();
	}
	writel(org, ((void __iomem *)(uintptr_t)info->conf_base_addr +
		     ((unsigned int)where & (~0x3))));

	spin_unlock_irqrestore(&cw_lock, flag);

	return PCIBIOS_SUCCESSFUL;
}

static int pcie_write_conf(struct pci_bus *bus, unsigned int devfn,
			   int where, int size, u32 value)
{
	struct pcie_info *info = bus_to_info(bus->number);

	pcie_debug(PCIE_DBG_REG,
		   "bus %d, devfn %d, where 0x%x, size 0x%x, value 0x%x",
		   bus->number & 0xff, devfn, where, size, value);

	if (unlikely(!info)) {
		pcie_error(
			"%s:Cannot find corresponding controller for appointed device!", __func__);
		BUG();
	}

	if (bus->number == info->root_bus_nr)
		return pcie_write_to_dbi(info, devfn, where, size, value);
	else
		return pcie_write_to_device(bus, devfn, where, size, value);
}

static struct pci_ops pcie_ops = {
	.read = pcie_read_conf,
	.write = pcie_write_conf,
};

void pci_set_max_rd_req_size(struct pci_bus *bus)
{
	struct pci_dev *dev = NULL;
	struct pci_bus *child = NULL;
	int pos;
	unsigned short dev_contrl_reg_val = 0;
	unsigned int max_rd_req_size;

	list_for_each_entry(dev, &bus->devices, bus_list) {
		/* set device max read requset size */
		pos = pci_find_capability(dev, PCI_CAP_ID_EXP);
		if (pos) {
			pci_read_config_word(dev, pos + PCI_EXP_DEVCTL,
					     &dev_contrl_reg_val);
			max_rd_req_size = (dev_contrl_reg_val >> 12) & 0x7; /* get bit[14:12] */
			if (max_rd_req_size > 0x0) {
				dev_contrl_reg_val &= ~(max_rd_req_size << 12); /* modify bit[14:12] */
				pci_write_config_word(dev, pos + PCI_EXP_DEVCTL,
						      dev_contrl_reg_val);
			}
		}
	}

	list_for_each_entry(dev, &bus->devices, bus_list) {
		BUG_ON(!dev->is_probed);
		child = dev->subordinate;
		if (child)
			pci_set_max_rd_req_size(child);
	}
}

#ifdef CONFIG_PCI_MSI
int pci_msi_is_enable(struct platform_device *pdev)
{
	int msi_irq;

	msi_irq = platform_get_irq_byname(pdev, "msi");
	if (msi_irq < 0)
		return false;

	return true;
}
#endif

#ifdef CONFIG_ARM64

static int pci_common_init(struct platform_device *pdev, struct hw_pci *bsp_pcie)
{
	struct device_node *dn = pdev->dev.of_node;
	struct pcie_info *info = NULL;
	struct pci_bus *bus = NULL;
	resource_size_t io_addr;
	int ret;
	int pcie_contrl = -1;
	struct resource bus_range;

	LIST_HEAD(res);

	ret = of_property_read_u32(dn, "pcie_controller", &pcie_contrl);
	if (ret) {
		pr_err("%s:No pcie_controller found!\n", __func__);
		return -EINVAL;
	}

	ret = of_pci_parse_bus_range(dn, &bus_range);
	if (ret != 0) {
		pr_err("%s:No \"bus-range\" found!\n", __func__);
		return ret;
	}

	pr_info("PCIe Controller %d: Bus range [%#llx, %#llx]\n", pcie_contrl,
		bus_range.start, bus_range.end);
 
	ret = devm_of_pci_get_host_bridge_resources(&pdev->dev, bus_range.start,
						    bus_range.end, &res, &io_addr);

	if (ret)
		return ret;
	bus = pci_create_root_bus(&pdev->dev, bus_range.start, &pcie_ops, bsp_pcie, &res);
	if (!bus)
		return -ENOMEM;

#ifdef CONFIG_PCI_MSI
	if (pci_msi_is_enable(pdev))
		bus->msi = &bsp_pcie->msi.chip;
#endif

#ifdef CONFIG_LIMIT_MAX_RD_REQ_SIZE
	pci_set_max_rd_req_size(bus);
#endif

	pcie_info[pcie_contrl].root_bus_nr = bus->number;
	info = bus_to_info(bus->number);
	if (info != NULL)
		__arch_config_iatu_tbl(info, NULL);

	pci_scan_child_bus(bus);
	pci_assign_unassigned_bus_resources(bus);
	pci_bus_add_devices(bus);

	platform_set_drvdata(pdev, bsp_pcie);

	return 0;
}
#else
static int pci_common_init_bsp(struct platform_device *pdev,
			       struct hw_pci *bsp_pcie)
{
	struct device_node *dn = pdev->dev.of_node;
	struct pcie_info *info = NULL;
	struct pci_bus *bus = NULL;
	resource_size_t io_addr;
	int ret;
	int pcie_contrl;
	int bus_start;

	LIST_HEAD(res);

	ret = of_property_read_u32(dn, "pcie_controller", &pcie_contrl);
	if (ret) {
		pr_err("%s:No pcie_controller found!\n", __func__);
		return -EINVAL;
	}

	if (pcie_contrl == 0)
		bus_start = 0;
	else
		bus_start = 2;

	ret = devm_of_pci_get_host_bridge_resources(&pdev->dev, bus_start, 0xff, &res, &io_addr);
	if (ret)
		return ret;

	bus = pci_create_root_bus(&pdev->dev, bus_start, &pcie_ops, bsp_pcie, &res);
	if (!bus)
		return -ENOMEM;

#ifdef CONFIG_PCI_MSI
	if (pci_msi_is_enable(pdev))
		bus->msi = &bsp_pcie->msi.chip;
#endif

#ifdef CONFIG_LIMIT_MAX_RD_REQ_SIZE
	pci_set_max_rd_req_size(bus);
#endif

	pcie_info[pcie_contrl].root_bus_nr = bus->number;
	info = bus_to_info(bus->number);
	if (info != NULL)
		__arch_config_iatu_tbl(info, NULL);

	pci_scan_child_bus(bus);
	pci_assign_unassigned_bus_resources(bus);
	pci_bus_add_devices(bus);

	platform_set_drvdata(pdev, bsp_pcie);

	return 0;
}
#endif

#ifdef CONFIG_PCI_MSI
static inline struct bsp_msi *to_bsp_msi(struct msi_controller *chip)
{
	return container_of(chip, struct bsp_msi, chip);
}

static int bsp_msi_alloc(struct bsp_msi *chip)
{
	int msi;

	mutex_lock(&chip->lock);

	msi = find_first_zero_bit(chip->used, BSP_PCI_MSI_NR);
	if (msi < BSP_PCI_MSI_NR)
		set_bit(msi, chip->used);
	else
		msi = -ENOSPC;

	mutex_unlock(&chip->lock);

	return msi;
}

static void bsp_msi_free(struct bsp_msi *chip, unsigned long irq)
{
	struct device *dev = chip->chip.dev;

	mutex_lock(&chip->lock);

	if (!test_bit(irq, chip->used))
		dev_err(dev, "trying to free unused MSI#%lu\n", irq);
	else
		clear_bit(irq, chip->used);

	mutex_unlock(&chip->lock);
}

static irqreturn_t bsp_pcie_msi_irq(int irq, void *data)
{
	struct hw_pci *bsp_pcie = data;
	struct device *dev = bsp_pcie->dev;
	struct bsp_msi *msi = &bsp_pcie->msi;
	unsigned int i;
	unsigned int processed = 0;
	void *dbi_base = (void *)(uintptr_t)
			 pcie_info[bsp_pcie->nr_controllers].conf_base_addr;

	/* 8 EP interrupt managment unit */
	for (i = 0; i < 8; i++) {
		unsigned long reg = readl(dbi_base + 0x830 + i * 0xc);

		while (reg) {
			/* the offest base on the first set bit of reg*/
			unsigned int offset = find_first_bit(&reg, 32);
			unsigned int index = i * 32 + offset;
			unsigned int irq;

			/* clear the interrupt */
			writel(1 << offset, dbi_base + MSI_CONTRL_INTERRUPT + i * 0xc);

			irq = irq_find_mapping(msi->domain, index);
			if (irq && test_bit(index, msi->used)) {
				generic_handle_irq(irq);
			} else if (irq) {
				dev_info(dev, "unhandled MSI\n");
			} else {
				/*
				 * * that's weird who triggered this?
				 * * just clear it
				 *  */
				dev_info(dev, "unexpected MSI\n");
			}

			/* see if there's any more pending in this vector */
			reg = readl(dbi_base +  MSI_CONTRL_INTERRUPT + i * 0xc);
			processed++;
		}
	}
	return processed > 0 ? IRQ_HANDLED : IRQ_NONE;
}

static int bsp_msi_setup_irq(struct msi_controller *chip,
			      struct pci_dev *pdev, struct msi_desc *desc)
{
	struct bsp_msi *msi = to_bsp_msi(chip);
	struct msi_msg msg;
	unsigned int irq;
	int hwirq;

	if (pdev->bus->number == pcie_info[0].root_bus_nr ||
			pdev->bus->number == pcie_info[1].root_bus_nr)
		return 0;

	hwirq = bsp_msi_alloc(msi);
	if (hwirq < 0)
		return hwirq;

	irq = irq_create_mapping(msi->domain, hwirq);
	if (!irq) {
		bsp_msi_free(msi, hwirq);
		return -EINVAL;
	}

	irq_set_msi_desc(irq, desc);

	desc->msi_attrib.multiple = 0x0;

	msg.address_lo = virt_to_phys((void *)(uintptr_t)msi->pages); /* address low bits */
	msg.address_hi = (virt_to_phys((void *)(uintptr_t)msi->pages) >> 32); /* address high bits */
	msg.data = hwirq;

	pci_write_msi_msg(irq, &msg);

	return 0;
}

static void bsp_msi_teardown_irq(struct msi_controller *chip,
				  unsigned int irq)
{
	struct bsp_msi *msi = to_bsp_msi(chip);
	struct irq_data *d = irq_get_irq_data(irq);
	irq_hw_number_t hwirq = 0;

	if (d != NULL)
		hwirq = irqd_to_hwirq(d);

	irq_dispose_mapping(irq);
	bsp_msi_free(msi, hwirq);
}

static struct irq_chip bsp_msi_irq_chip = {
	.name = "PCIe MSI",
	.irq_enable = pci_msi_unmask_irq,
	.irq_disable = pci_msi_mask_irq,
	.irq_mask = pci_msi_mask_irq,
	.irq_unmask = pci_msi_unmask_irq,
};

static int bsp_msi_map(struct irq_domain *domain, unsigned int irq,
			irq_hw_number_t hwirq)
{
	irq_set_chip_and_handler(irq, &bsp_msi_irq_chip, handle_simple_irq);
	irq_set_chip_data(irq, domain->host_data);

	return 0;
}

static const struct irq_domain_ops msi_domain_ops = {
	.map = bsp_msi_map,
};

static int bsp_pcie_enable_msi(struct hw_pci *pcie)
{
	struct device *dev = pcie->dev;
	struct platform_device *pdev = to_platform_device(dev);
	struct bsp_msi *msi = &pcie->msi;
	unsigned long base;
	int err;

	void *dbi_base = (void *)(uintptr_t)
			 pcie_info[pcie->nr_controllers].conf_base_addr;

	if (!msi) {
		dev_err(dev, "msi is null, error.\n");
		return -EINVAL;
	}

	mutex_init(&msi->lock);

	msi->chip.dev = dev;
	msi->chip.setup_irq = bsp_msi_setup_irq;
	msi->chip.teardown_irq = bsp_msi_teardown_irq;

	msi->domain = irq_domain_add_linear(dev->of_node, BSP_PCI_MSI_NR,
					    &msi_domain_ops, &msi->chip);
	if (!msi->domain) {
		dev_err(dev, "failed to create IRQ domain\n");
		return -ENOMEM;
	}

	err = platform_get_irq_byname(pdev, "msi");
	if (err < 0) {
		dev_err(dev, "failed to get IRQ: %d\n", err);
		goto err;
	}

	msi->irq = err;

	err = request_irq(msi->irq, bsp_pcie_msi_irq, IRQF_NO_THREAD,
			  bsp_msi_irq_chip.name, pcie);
	if (err < 0) {
		dev_err(dev, "failed to request IRQ: %d\n", err);
		goto err;
	}

	/* setup AFI/FPCI range */
	msi->pages = __get_free_pages(GFP_KERNEL, 0);
	base = virt_to_phys((void *)(uintptr_t)msi->pages);

	/* write high bits */
	writel(base >> 32, dbi_base + MSI_CTRL_UPPER_ADDR_OFF);
	/* write lower bits */
	writel(base, dbi_base + MSI_CTRL_ADDR_OFF);

	/* enable all MSI vectors */
	writel(0xffffffff, dbi_base + MSI_CTRL_INT_EN_OFF0);
	writel(0xffffffff, dbi_base + MSI_CTRL_INT_EN_OFF1);
	writel(0xffffffff, dbi_base + MSI_CTRL_INT_EN_OFF2);
	writel(0xffffffff, dbi_base + MSI_CTRL_INT_EN_OFF3);
	writel(0xffffffff, dbi_base + MSI_CTRL_INT_EN_OFF4);
	writel(0xffffffff, dbi_base + MSI_CTRL_INT_EN_OFF5);
	writel(0xffffffff, dbi_base + MSI_CTRL_INT_EN_OFF6);
	writel(0xffffffff, dbi_base + MSI_CTRL_INT_EN_OFF7);

	return 0;

err:
	irq_domain_remove(msi->domain);
	return err;
}

static int bsp_pcie_disable_msi(struct hw_pci *pcie)
{
	struct bsp_msi *msi = &pcie->msi;
	unsigned int i, irq;
	void *dbi_base = (void *)(uintptr_t)
			 pcie_info[pcie->nr_controllers].conf_base_addr;

	/* disable all MSI vectors */
	writel(0x0, dbi_base + MSI_CTRL_INT_EN_OFF0);
	writel(0x0, dbi_base + MSI_CTRL_INT_EN_OFF1);
	writel(0x0, dbi_base + MSI_CTRL_INT_EN_OFF2);
	writel(0x0, dbi_base + MSI_CTRL_INT_EN_OFF3);
	writel(0x0, dbi_base + MSI_CTRL_INT_EN_OFF4);
	writel(0x0, dbi_base + MSI_CTRL_INT_EN_OFF5);
	writel(0x0, dbi_base + MSI_CTRL_INT_EN_OFF6);
	writel(0x0, dbi_base + MSI_CTRL_INT_EN_OFF7);

	free_pages(msi->pages, 0);

	if (msi->irq > 0)
		free_irq(msi->irq, pcie);

	for (i = 0; i < BSP_PCI_MSI_NR; i++) {
		irq = irq_find_mapping(msi->domain, i);
		if (irq > 0)
			irq_dispose_mapping(irq);
	}

	irq_domain_remove(msi->domain);

	return 0;
}
#endif

static int get_pcie_controller(struct device_node *node, int *controllers_nr)
{
	int err;
	if (!node) {
		pr_err("get node from dts failed! controller:%d\n", *controllers_nr);
		return -EIO;
	}

	err = of_property_read_u32(node, "pcie_controller", controllers_nr);
	if (err) {
		pr_err("%s:No pcie_controller found!\n", __func__);
		return -EINVAL;
	}

	if (__arch_pcie_info_setup(pcie_info, controllers_nr))
		return -EIO;

	if (*controllers_nr >= MAX_PCIE_CONTROLLER_NUM) {
		pr_err("pcie_controllers_nr is Invalid, pcie_controllers_nr: %d\n", *controllers_nr);
		return -EINVAL;
	}

	return 0;
}

static int pcie_init(struct platform_device *pdev)
{
	int err;
	struct hw_pci *bsp_pcie = NULL;

	if (!pdev) {
		pr_err("pdev is null!\n");
		return -ENOMEM;
	}

	bsp_pcie = kzalloc(sizeof(struct hw_pci), GFP_KERNEL);
	if (!bsp_pcie) {
		pr_err("kzalloc hw_pci space failed!\n");
		return -ENOMEM;
	}

	bsp_pcie->dev = &pdev->dev;

	g_of_node = pdev->dev.of_node;

	err = get_pcie_controller(g_of_node, &pcie_controllers_nr);
	if (err) {
		kfree(bsp_pcie);
		return err;
	}

	if (__arch_pcie_sys_init(&pcie_info[pcie_controllers_nr]))
		goto pcie_init_err;

	bsp_pcie->nr_controllers = pcie_controllers_nr;
	pr_err("Number of PCIe controllers: %d\n", bsp_pcie->nr_controllers);

#ifdef CONFIG_PCI_MSI
	if (pci_msi_is_enable(pdev)) {
		err = bsp_pcie_enable_msi(bsp_pcie);
		if (err < 0) {
			pr_err("failed to enable MSI support: %d\n", err);
			goto pcie_init_err;
		}
	}
#endif

#ifdef CONFIG_ARM64
	err = pci_common_init(pdev, bsp_pcie);
#else
	err = pci_common_init_bsp(pdev, bsp_pcie);
#endif

	if (err)
		goto disable_msi;

	return 0;

disable_msi:
#ifdef CONFIG_PCI_MSI
	if (pci_msi_is_enable(pdev))
		bsp_pcie_disable_msi(bsp_pcie);
#endif
pcie_init_err:
	__arch_pcie_info_release(&pcie_info[pcie_controllers_nr]);

	kfree(bsp_pcie);

	return -EIO;
}

static int __exit pcie_uinit(struct platform_device *pdev)
{
	__arch_pcie_info_release(pcie_info);
	return 0;
}

#include <linux/platform_device.h>
#include <linux/pm.h>

int  bsp_pcie_plat_driver_probe(struct platform_device *pdev)
{
	return 0;
}
int  bsp_pcie_plat_driver_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_PM
int bsp_pcie_plat_driver_suspend(struct device *dev)
{
	__arch_pcie_sys_exit();
	return 0;
}

int bsp_pcie_plat_driver_resume(struct device *dev)
{
	return __arch_pcie_sys_init(pcie_info);
}

const struct dev_pm_ops bsp_pcie_pm_ops = {
	.suspend = NULL,
	.suspend_noirq = bsp_pcie_plat_driver_suspend,
	.resume = NULL,
	.resume_noirq = bsp_pcie_plat_driver_resume
};

#define BSP_PCIE_PM_OPS (&bsp_pcie_pm_ops)
#else
#define BSP_PCIE_PM_OPS NULL
#endif

#define PCIE_RC_DRV_NAME "pcie root complex"

static const struct of_device_id bsp_pcie_match_table[] = {
	{ .compatible = "vendor,pcie", },
	{},
};

static struct platform_driver bsp_pcie_driver = {
	.driver = {
		.name = "bsp-pcie",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(bsp_pcie_match_table),
	},
	.probe = pcie_init,
};
module_platform_driver(bsp_pcie_driver);

MODULE_DESCRIPTION("Vendor PCI-Express Root Complex driver");
MODULE_LICENSE("GPL");
