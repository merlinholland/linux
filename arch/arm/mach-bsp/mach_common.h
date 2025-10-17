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
 *
*/

#ifndef __SMP_COMMON_H
#define __SMP_COMMON_H

#ifdef CONFIG_SMP
void bsp_set_cpu(unsigned int cpu, bool enable);
void __init bsp_smp_prepare_cpus(unsigned int max_cpus);
int bsp_boot_secondary(unsigned int cpu, struct task_struct *idle);
#endif /* CONFIG_SMP */
#endif /* __SMP_COMMON_H */
