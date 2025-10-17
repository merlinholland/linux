/*
 * Copyright (c) 2015-2016 Shenshu Technologies Co., Ltd.
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

#include <linux/io.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/reset-controller.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include "reset.h"

#define	BSP_RESET_BIT_MASK	0x1f
#define	BSP_RESET_OFFSET_SHIFT	8
#define	BSP_RESET_OFFSET_MASK	0xffff00

struct bsp_reset_controller {
	spinlock_t	lock;
	void __iomem	*membase;
	struct reset_controller_dev	rcdev;
};


#define to_bsp_reset_controller(rcdev)  \
	container_of(rcdev, struct bsp_reset_controller, rcdev)

static int bsp_reset_of_xlate(struct reset_controller_dev *rcdev,
			const struct of_phandle_args *reset_spec)
{
	u32 offset;
	u8 bit;

	offset = (reset_spec->args[0] << BSP_RESET_OFFSET_SHIFT)
		& BSP_RESET_OFFSET_MASK;
	bit = reset_spec->args[1] & BSP_RESET_BIT_MASK;

	return (offset | bit);
}

static int bsp_reset_assert(struct reset_controller_dev *rcdev,
			      unsigned long id)
{
	struct bsp_reset_controller *rstc = to_bsp_reset_controller(rcdev);
	unsigned long flags;
	u32 offset, reg;
	u8 bit;

	offset = (id & BSP_RESET_OFFSET_MASK) >> BSP_RESET_OFFSET_SHIFT;
	bit = id & BSP_RESET_BIT_MASK;

	spin_lock_irqsave(&rstc->lock, flags);

	reg = readl(rstc->membase + offset);
	writel(reg | BIT(bit), rstc->membase + offset);

	spin_unlock_irqrestore(&rstc->lock, flags);

	return 0;
}

static int bsp_reset_deassert(struct reset_controller_dev *rcdev,
				unsigned long id)
{
	struct bsp_reset_controller *rstc = to_bsp_reset_controller(rcdev);
	unsigned long flags;
	u32 offset, reg;
	u8 bit;

	offset = (id & BSP_RESET_OFFSET_MASK) >> BSP_RESET_OFFSET_SHIFT;
	bit = id & BSP_RESET_BIT_MASK;

	spin_lock_irqsave(&rstc->lock, flags);

	reg = readl(rstc->membase + offset);
	writel(reg & ~BIT(bit), rstc->membase + offset);

	spin_unlock_irqrestore(&rstc->lock, flags);

	return 0;
}

static const struct reset_control_ops bsp_reset_ops = {
	.assert		= bsp_reset_assert,
	.deassert	= bsp_reset_deassert,
};

#ifdef CONFIG_ARCH_BSP
int __init bsp_reset_init(struct device_node *np,
		int nr_rsts)
{
	struct bsp_reset_controller *rstc;

	rstc = kzalloc(sizeof(*rstc), GFP_KERNEL);
	if (!rstc)
		return -ENOMEM;

	rstc->membase = of_iomap(np, 0);
	if (!rstc->membase){
		kfree(rstc);
		return -EINVAL;
	}

	spin_lock_init(&rstc->lock);

	rstc->rcdev.owner = THIS_MODULE;
	rstc->rcdev.nr_resets = nr_rsts;
	rstc->rcdev.ops = &bsp_reset_ops;
	rstc->rcdev.of_node = np;
	rstc->rcdev.of_reset_n_cells = 2;
	rstc->rcdev.of_xlate = bsp_reset_of_xlate;

	return reset_controller_register(&rstc->rcdev);
}
EXPORT_SYMBOL_GPL(bsp_reset_init);
#endif

struct bsp_reset_controller *vendor_reset_init(struct platform_device *pdev)
{
	struct bsp_reset_controller *rstc;
	struct resource *res;

	rstc = devm_kmalloc(&pdev->dev, sizeof(*rstc), GFP_KERNEL);
	if (!rstc)
		return NULL;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	rstc->membase = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(rstc->membase))
		return NULL;

	spin_lock_init(&rstc->lock);
	rstc->rcdev.owner = THIS_MODULE;
	rstc->rcdev.ops = &bsp_reset_ops;
	rstc->rcdev.of_node = pdev->dev.of_node;
	rstc->rcdev.of_reset_n_cells = 2;
	rstc->rcdev.of_xlate = bsp_reset_of_xlate;
	reset_controller_register(&rstc->rcdev);

	return rstc;
}
EXPORT_SYMBOL_GPL(vendor_reset_init);

void bsp_reset_exit(struct bsp_reset_controller *rstc)
{
	reset_controller_unregister(&rstc->rcdev);
}
EXPORT_SYMBOL_GPL(bsp_reset_exit);
