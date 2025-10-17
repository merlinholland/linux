/*
 * Vendor Clock and Reset Driver Header
 *
 * Copyright (c) 2016 Vendor Limited.
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
 */

#ifndef __BSP_CRG_H
#define __BSP_CRG_H

struct bsp_clock_data;
struct bsp_reset_controller;

struct bsp_crg_funcs {
	struct bsp_clock_data*	(*register_clks)(struct platform_device *pdev);
	void (*unregister_clks)(const struct platform_device *pdev);
};

struct bsp_crg_dev {
	struct bsp_clock_data *clk_data;
	struct bsp_reset_controller *rstc;
	const struct bsp_crg_funcs *funcs;
};

#endif	/* __BSP_CRG_H */
