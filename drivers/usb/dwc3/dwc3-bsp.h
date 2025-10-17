// SPDX-License-Identifier: GPL-2.0
/*
*
* Copyright (c) 2012-2018 Shenshu Technologies Co., Ltd.
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
#ifndef USB_INCLUDE_DWC3_BSP_H
#define USB_INCLUDE_DWC3_BSP_H

struct bsp_priv {
	void __iomem *peri_crg;
	void __iomem *sys_ctrl;
	void __iomem *misc_ctrl;
	unsigned int speed_id;
};

struct dwc3_bsp {
	struct device *dev;
	struct clk **clks;
	int num_clocks;
	void __iomem *ctrl_base;
	void __iomem *crg_base;
	struct reset_control *port_rst;
	u32 crg_offset;
	u32 crg_ctrl_def_mask;
	u32 crg_ctrl_def_val;
};

extern int usb_get_max_speed(struct device *dev);
extern void bsp_dwc3_exited(void);

#define DEV_NODE_FLAG0	0
#define DEV_NODE_FLAG1	1
#define DEV_NODE_FLAG2	2

#define U_LEVEL2	200

#endif
