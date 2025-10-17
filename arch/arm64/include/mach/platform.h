/*
 * Copyright (c) 2018 Shenshu Technologies Co., Ltd.
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

#ifndef __PLATFORM_H__
#define __PLATFORM_H__

#if defined(CONFIG_ARCH_SS919V100) || defined(CONFIG_ARCH_SS015V100)
#include "platform_ss919v100.h"
#endif

#if defined(CONFIG_ARCH_SS928V100) || defined(CONFIG_ARCH_SS927V100)
#include "platform_ss928v100.h"
#endif

#endif /* End of __PLATFORM_H__ */
