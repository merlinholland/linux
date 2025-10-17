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

#ifndef __BSP_PCIE_H__
#define __BSP_PCIE_H__
#define PCIE_EP_CONF_BASE	0x20000000
#define PCIE_EP1_CONF_BASE	0x30000000

#define PERI_CRG_BASE		0x11010000

#define PCIE_X2_SRST_REQ	2

#define PCIE_X2_AUX_CKEN	7
#define PCIE_X2_PIPE_CKEN	6
#define PCIE_X2_SYS_CKEN	5
#define PCIE_X2_BUS_CKEN	4
#define PCIE_PAD_OE_MASK	(0x7 << 8)

#define PCIE_SYS_CTRL0		0xc00
#define PCIE_DEVICE_TYPE	28
#define PCIE_WM_EP		0x0
#define PCIE_WM_LEGACY		0x1
#define PCIE_WM_RC		0x4

#define PCIE_SYS_CTRL7		0xc1C
#define PCIE_APP_LTSSM_ENBALE	11

#define PCIE_SYS_CTRL32		0xc80
#define PCIE_RESET_TO_PAD	30

#define PCIE_SYS_STATE0		0xf00
#define PCIE_XMLH_LINK_UP	15
#define PCIE_RDLH_LINK_UP	5

#define PCIE_INTA_PIN		1
#define PCIE_INTB_PIN		2
#define PCIE_INTC_PIN		3
#define PCIE_INTD_PIN		4

#define REG_SC_STAT		0x0018
#define PCI_CARD		0x44

#define PCIE_MODE_SHIFT		16
#define PCIE_MODE_MASK		0x7

#define ATU_VIEWPORT_REG	0x900
#define ATU_REGION_CTRL1_REG	0x904
#define ATU_REGION_CTRL2_REG	0x908
#define ATU_BASE_LOW_REG	0x90c
#define ATU_BASE_HIGH_REG	0x910
#define ATU_LIMIT_REG		0x914
#define ATU_TARGET_LOW_REG	0x918
#define ATU_ATRGET_HIGH_REG	0x91c

#endif
