/*
 * Copyright (c) 2017-2018 Shenshu Technologies Co., Ltd.
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

#include "pcie_ss928v100.h"

static void *dbi_base;
static int __arch_pcie_info_setup(struct pcie_info *info, int *controllers_nr);
static int __arch_pcie_sys_init(struct pcie_info *info);
static void __arch_pcie_info_release(struct pcie_info *info);

struct pcie_iatu iatu_table[] = {
		{
			.viewport       = 0,				/* iAtu Vierport Register */
			.region_ctrl_1  = 0x00000004,			/* Region Control 1 Register */
			.region_ctrl_2  = 0x90000000,			/* Region Control 2 Register */
			.lbar           = PCIE_EP_CONF_BASE + (1<<20),	/* Lower Base Address Register */
			.ubar           = 0x0,				/* Upper Base Address Register */
			.lar            = PCIE_EP_CONF_BASE + (2<<20) - 1,	/* Limit Address Register */
			.ltar           = 0x01000000,			/* Lower Target Address Register */
			.utar           = 0x00000000,			/* Upper Target Address Register */
		},
		{
			.viewport       = 1,				/* iAtu Vierport Register */
			.region_ctrl_1  = 0x00000005,			/* Region Control 1 Register */
			.region_ctrl_2  = 0x90000000,			/* Region Control 2 Register */
			.lbar           = PCIE_EP_CONF_BASE + (2<<20),	/* Lower Base Address Register */
			.ubar           = 0x0,				/* Upper Base Address Register */
			.lar            = PCIE_EP_CONF_BASE + (__256MB__ - 1),	/* Limit Address Register */
			.ltar           = 0x02000000,			/* Lower Target Address Register */
			.utar           = 0x00000000,			/* Upper Target Address Register */
		},
};

static void __arch_config_iatu_tbl(struct pcie_info *info,
				   struct pci_sys_data *sys)
{
	int i;
	void __iomem *config_base = (void __iomem *)(uintptr_t)info->conf_base_addr;
	struct pcie_iatu *ptable = iatu_table;
	int table_size = ARRAY_SIZE(iatu_table);

	/* configure atu */
	for (i = 0; i < table_size; i++) {
		writel((ptable + i)->viewport, config_base + ATU_VIEWPORT_REG);
		writel((ptable + i)->lbar, config_base + ATU_BASE_LOW_REG);
		writel((ptable + i)->ubar, config_base + ATU_BASE_HIGH_REG);
		writel((ptable + i)->lar,  config_base + ATU_LIMIT_REG);
		writel((ptable + i)->ltar, config_base + ATU_TARGET_LOW_REG);
		writel((ptable + i)->utar, config_base + ATU_ATRGET_HIGH_REG);
		writel((ptable + i)->region_ctrl_1, config_base + ATU_REGION_CTRL1_REG);
		writel((ptable + i)->region_ctrl_2, config_base + ATU_REGION_CTRL2_REG);
	}
}

static unsigned int __arch_check_pcie_link(struct pcie_info *info)
{
	unsigned int val;

	val = readl((void *)(uintptr_t)(info->conf_base_addr + PCIE_SYS_STATE0));
	return ((val & (1 << PCIE_XMLH_LINK_UP))
		&& (val & (1 << PCIE_RDLH_LINK_UP))) ? 1 : 0;
}

static int __arch_get_ups_mode(void)
{
	unsigned int val;
	unsigned int mode;
	unsigned int sys_ctrl_base = 0;
	void *pcie_sys_stat = NULL;

	/* Get sys ctrl  base address */
	of_property_read_u32(g_of_node, "sys_ctrl_base", &sys_ctrl_base);

	pcie_sys_stat = ioremap_nocache(sys_ctrl_base + REG_SC_STAT, sizeof(int));
	if (!pcie_sys_stat) {
		pr_err("ioremap pcie sys status register failed!\n");
		return 0;
	}

	val = readl(pcie_sys_stat);
	mode = (val >> PCIE_MODE_SHIFT) & PCIE_MODE_MASK;

	iounmap(pcie_sys_stat);

	return mode;
}

static int  __arch_get_port_nr(void)
{
	unsigned int mode;
	int  port_nr;

	mode = __arch_get_ups_mode();
	switch (mode) {
	case 0x0:
	case 0x2:
		port_nr = 1;
		break;

	default:
		port_nr = 0;
		break;
	}

	return port_nr;
}

static int read_properties(struct pcie_property *property)
{
	int err;

	/* Get pcie deice memory size */
	err = of_property_read_u32(g_of_node, "dev_mem_size", &property->pcie_mem_size);
	if (err) {
		pcie_error("No dev_mem_size found!");
		return err;
	}

	/* Get pcie config space size */
	err = of_property_read_u32(g_of_node, "dev_conf_size", &property->pcie_cfg_size);
	if (err) {
		pcie_error("No dev_conf_size founcd!");
		return err;
	}

	/* Get pcie dib base address */
	err = of_property_read_u32(g_of_node, "pcie_dbi_base", &property->pcie_dbi_base);
	if (err) {
		pcie_error("No pcie_dbi_base found!");
		return err;
	}

	/* Get pcie device config base address */
	err = of_property_read_u32(g_of_node, "ep_conf_base", &property->pcie_ep_conf_base);
	if (err) {
		pcie_error("No ep_conf_base found!");
		return err;
	}

	if ((property->pcie_mem_size > __256MB__) || (property->pcie_cfg_size > __256MB__)) {
		pcie_error(
			"Invalid parameter: pcie mem size[0x%x], pcie cfg size[0x%x]!",
			property->pcie_mem_size, property->pcie_cfg_size);
		return err;
	}

	err = of_property_read_u32(g_of_node, "pcie_controller", &property->pcie_contrl);
	if (err) {
		pcie_error("No pcie_controller found!");
		return err;
	}

	return 0;
}

/*
 * ret:
 */
static int __arch_pcie_info_setup(struct pcie_info *info, int *controllers_nr)
{
	struct pcie_property property = {
		.pcie_mem_size = 0,
		.pcie_cfg_size = 0,
		.pcie_ep_conf_base = 0,
		.pcie_contrl = 0
	};
	int nr;
	int err;

	err = read_properties(&property);
	if (err)
		return -EINVAL;

	nr = __arch_get_port_nr();
	if (!nr) {
		pr_err("Pcie port number: 0\n");
		*controllers_nr = 0;
		return -EINVAL;
	}

	info->controller = property.pcie_contrl;

	/* RC configuration space */
	info->conf_base_addr = (uintptr_t)ioremap_nocache(property.pcie_dbi_base, __8KB__);
	if (!info->conf_base_addr) {
		pcie_error("Address mapping for RC dbi failed!");
		return -EIO;
	}

	/* Configuration space for all EPs */
	info->base_addr = (unsigned long)(uintptr_t)ioremap_nocache(
					property.pcie_ep_conf_base, property.pcie_cfg_size);
	if (!info->base_addr) {
		iounmap((void *)(uintptr_t)info->conf_base_addr);
		pcie_error("Address mapping for EPs cfg failed!");
		return -EIO;
	}

	return 0;
}

static void __arch_pcie_info_release(struct pcie_info *info)
{
	if (info->base_addr)
		iounmap((void *)(uintptr_t)info->base_addr);

	if (info->conf_base_addr)
		iounmap((void *)(uintptr_t)info->conf_base_addr);
}

static int __arch_pcie_sys_init(struct pcie_info *info)
{
	unsigned int val;
	unsigned int pcie_clk_rst_reg = 0;
	void *crg_base = NULL;

	crg_base = (void *)ioremap_nocache(PERI_CRG_BASE, __16KB__);
	if (!crg_base) {
		pr_err("ioremap crg base address fiailed!func:%s, line:%d\n",
				__func__, __LINE__);
		return -EINVAL;
	}

	val = of_property_read_u32(g_of_node, "pcie_clk_rest_reg", &pcie_clk_rst_reg);
	if (val) {
		pcie_error("No pcie_clk_rest_reg found!");
		return -EINVAL;
	}

	dbi_base = (void *)(uintptr_t)info->conf_base_addr;

	/*
	 * Disable PCIE
	 */
	val = readl(dbi_base + PCIE_SYS_CTRL7);
	val &= (~(1 << PCIE_APP_LTSSM_ENBALE));
	writel(val, dbi_base + PCIE_SYS_CTRL7);

	/*
	 * Reset
	 */
	val = readl(crg_base + pcie_clk_rst_reg);
	val |= PCIE_X2_SRST_REQ;
	writel(val, crg_base + pcie_clk_rst_reg);

	/*
	 * Retreat from the reset state
	 */
	udelay(500);
	val = readl(crg_base + pcie_clk_rst_reg);
	val &= ~PCIE_X2_SRST_REQ;
	writel(val, crg_base + pcie_clk_rst_reg);
	mdelay(10);

	writel(PCIE_WK_RC, dbi_base);
	/*
	 * PCIE RC work mode
	 */
	val = readl(dbi_base + PCIE_SYS_CTRL0);
	val &= (~(0xf << PCIE_DEVICE_TYPE));
	val |= (PCIE_WM_RC << PCIE_DEVICE_TYPE);
	writel(val, dbi_base + PCIE_SYS_CTRL0);

	/*
	 * Enable clk
	 */
	val = readl(crg_base + pcie_clk_rst_reg);
	val |= ((1 << PCIE_X2_BUS_CKEN) |
		(1 << PCIE_X2_SYS_CKEN) |
		(1 << PCIE_X2_PIPE_CKEN) |
		(1 << PCIE_X2_AUX_CKEN));
	writel(val, crg_base + pcie_clk_rst_reg);

	mdelay(10);

	/*
	 *  * Set PCIe support the identification Board card
	 */
	val = readl(dbi_base + PCI_CARD);
	val |= (1<<3);
	writel(val, dbi_base + PCI_CARD);
	mdelay(10);

	/*
	 * Set PCIE controller class code to be PCI-PCI bridge device
	 */
	val = readl(dbi_base + PCI_CLASS_REVISION);
	val &= ~(0xffffff00);
	val |= (0x60400 << 8);
	writel(val, dbi_base + PCI_CLASS_REVISION);
	udelay(1000);

	/*
	 *  reset EP
	 */
	val = readl(dbi_base + PCIE_SYS_CTRL32);
	val &= ~(1 << PCIE_RESET_TO_PAD);
	writel(val, dbi_base + PCIE_SYS_CTRL32);

	/*
	 * Enable controller
	 */
	val = readl(dbi_base + PCIE_SYS_CTRL7);
	val |= (1 << PCIE_APP_LTSSM_ENBALE);
	writel(val, dbi_base + PCIE_SYS_CTRL7);
	udelay(1000);

	val = readl(dbi_base + PCI_COMMAND);
	val |= 7; /* Bus Master Enable */
	writel(val, dbi_base + PCI_COMMAND);

	udelay(1000);

	/*
	 * release EP
	 */
	val = readl(dbi_base + PCIE_SYS_CTRL32);
	val |= (1 << PCIE_RESET_TO_PAD);
	writel(val, dbi_base + PCIE_SYS_CTRL32);

	iounmap(crg_base);
	return 0;
}

static void __arch_pcie_sys_exit(void)
{
	unsigned int val;
	unsigned int pcie_clk_rst_reg = 0;
	void *crg_base = NULL;

	crg_base = (void *)ioremap_nocache(PERI_CRG_BASE, __8KB__);
	if (!crg_base) {
		pr_err("ioremap crg base address fiailed!func:%s, line:%d\n",
				__func__, __LINE__);
		return;
	}

	val = of_property_read_u32(g_of_node, "pcie_clk_rest_reg", &pcie_clk_rst_reg);
	if (val) {
		pcie_error("No pcie_clk_rest_reg found!");
		return;
	}

	/*
	 * Disable PCIE
	 */
	val = readl(dbi_base + PCIE_SYS_CTRL7);
	val &= (~(1 << PCIE_APP_LTSSM_ENBALE));
	writel(val, dbi_base + PCIE_SYS_CTRL7);

	/*
	 * Reset
	 */
	val = readl(crg_base + pcie_clk_rst_reg);
	val |= PCIE_X2_SRST_REQ;
	writel(val, crg_base + pcie_clk_rst_reg);

	udelay(1000);

	/*
	 * Disable clk
	 */
	val = readl(crg_base + pcie_clk_rst_reg);
	val &= (~(1 << PCIE_X2_AUX_CKEN));
	val &= (~(1 << PCIE_X2_PIPE_CKEN));
	val &= (~(1 << PCIE_X2_SYS_CKEN));
	val &= (~(1 << PCIE_X2_BUS_CKEN));
	writel(val, crg_base + pcie_clk_rst_reg);

	iounmap(crg_base);

	udelay(1000);
}
