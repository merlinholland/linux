/*
 * Copyright (c) 2012-2013 Shenshu Technologies Co., Ltd.
 * Copyright (c) 2012-2013 Linaro Limited.
 *
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#ifndef	__BSP_CLK_H
#define	__BSP_CLK_H

#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/spinlock.h>

struct platform_device;

struct bsp_clock_data {
	struct clk_onecell_data	clk_data;
	void __iomem		*base;
};

struct bsp_fixed_rate_clock {
	unsigned int		id;
	char			*name;
	const char		*parent_name;
	unsigned long		flags;
	unsigned long		fixed_rate;
};

struct bsp_fixed_factor_clock {
	unsigned int		id;
	char			*name;
	const char		*parent_name;
	unsigned long		mult;
	unsigned long		div;
	unsigned long		flags;
};

struct bsp_mux_clock {
	unsigned int		id;
	const char		*name;
	const char		*const *parent_names;
	u8			num_parents;
	unsigned long		flags;
	unsigned long		offset;
	u8			shift;
	u8			width;
	u8			mux_flags;
	u32			*table;
	const char		*alias;
};

struct bsp_phase_clock {
	unsigned int		id;
	const char		*name;
	const char		*parent_names;
	unsigned long		flags;
	unsigned long		offset;
	u8			shift;
	u8			width;
	u32			*phase_degrees;
	u32			*phase_regvals;
	u8			phase_num;
};

struct bsp_divider_clock {
	unsigned int		id;
	const char		*name;
	const char		*parent_name;
	unsigned long		flags;
	unsigned long		offset;
	u8			shift;
	u8			width;
	u8			div_flags;
	struct clk_div_table	*table;
	const char		*alias;
};

struct bsp_gate_clock {
	unsigned int		id;
	const char		*name;
	const char		*parent_name;
	unsigned long		flags;
	unsigned long		offset;
	u8			bit_idx;
	u8			gate_flags;
	const char		*alias;
};

struct clk *bsp_register_clkgate_sep(struct device *, const char *,
				const char *, unsigned long,
				void __iomem *, u8,
				u8, spinlock_t *);

struct bsp_clock_data *bsp_clk_alloc(struct platform_device *, int);
struct bsp_clock_data *bsp_clk_init(struct device_node *, int);
int bsp_clk_register_fixed_rate(const struct bsp_fixed_rate_clock *,
				int, struct bsp_clock_data *);
int bsp_clk_register_fixed_factor(const struct bsp_fixed_factor_clock *,
				int, struct bsp_clock_data *);
int bsp_clk_register_mux(const struct bsp_mux_clock *, int,
				struct bsp_clock_data *);
int bsp_clk_register_divider(const struct bsp_divider_clock *,
				int, struct bsp_clock_data *);
int bsp_clk_register_gate(const struct bsp_gate_clock *,
				int, struct bsp_clock_data *);
void bsp_clk_register_gate_sep(const struct bsp_gate_clock *,
				int, struct bsp_clock_data *);

#define bsp_clk_unregister(type) \
static inline \
void bsp_clk_unregister_##type(const struct bsp_##type##_clock *clks, \
				int nums, struct bsp_clock_data *data) \
{ \
	struct clk **clocks = data->clk_data.clks; \
	int i; \
	for (i = 0; i < nums; i++) { \
		int id = clks[i].id; \
		if (clocks[id])  \
			clk_unregister_##type(clocks[id]); \
	} \
}

bsp_clk_unregister(fixed_rate)
bsp_clk_unregister(fixed_factor)
bsp_clk_unregister(mux)
bsp_clk_unregister(divider)
bsp_clk_unregister(gate)

#endif	/* __BSP_CLK_H */
