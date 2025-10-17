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

#ifndef __EDMA_SS528V100_H__
#define __EDMA_SS528V100_H__

#include "edmacv310.h"
#define EDMAC_MAX_PERIPHERALS 32
#define EDMAC_CHANNEL_NUM 8


#define EDMAC_TX 1
#define EDMAC_RX 0

#define I2C0_REG_BASE  0x011060000
#define I2C1_REG_BASE  0x011061000
#define UART0_REG_BASE 0x011040000
#define UART1_REG_BASE 0x011041000
#define UART2_REG_BASE 0x011042000
#define UART3_REG_BASE 0x011043000
#define UART4_REG_BASE 0x011044000
#define SPI_REG_BASE   0x011070000

#define I2C0_RX_FIFO  (I2C0_REG_BASE + 0x0024)
#define I2C0_TX_FIFO  (I2C0_REG_BASE + 0x0020)
#define I2C1_RX_FIFO  (I2C1_REG_BASE + 0x0024)
#define I2C1_TX_FIFO  (I2C1_REG_BASE + 0x0020)
#define UART0_RX_FIFO UART0_REG_BASE
#define UART0_TX_FIFO UART0_REG_BASE
#define UART1_RX_FIFO UART1_REG_BASE
#define UART1_TX_FIFO UART1_REG_BASE
#define UART2_RX_FIFO UART2_REG_BASE
#define UART2_TX_FIFO UART2_REG_BASE
#define UART3_RX_FIFO UART3_REG_BASE
#define UART3_TX_FIFO UART3_REG_BASE
#define UART4_RX_FIFO UART4_REG_BASE
#define UART4_TX_FIFO UART4_REG_BASE
#define SPI_RX_FIFO   (0x011070000 + 0x0008)
#define SPI_TX_FIFO   (0x011070000 + 0x0008)


edmac_peripheral  g_peripheral[EDMAC_MAX_PERIPHERALS] = {
	{ 0, I2C0_RX_FIFO, DMAC_HOST1, (0x40000004), PERI_8BIT_MODE, 0},
	{ 1, I2C0_TX_FIFO, DMAC_HOST1, (0x80000004), PERI_8BIT_MODE, 1},
	{ 2, I2C1_RX_FIFO, DMAC_HOST1, (0x40000004), PERI_8BIT_MODE, 2},
	{ 3, I2C1_TX_FIFO, DMAC_HOST1, (0x80000004), PERI_8BIT_MODE, 3},
	{ 4, 0, DMAC_NOT_USE, 0, 0, 0},
	{ 5, 0, DMAC_NOT_USE, 0, 0, 0},
	{ 6, 0, DMAC_NOT_USE, 0, 0, 0},
	{ 7, 0, DMAC_NOT_USE, 0, 0, 0},
	{ 8, 0, DMAC_NOT_USE, 0, 0, 0},
	{ 9, 0, DMAC_NOT_USE, 0, 0, 0},
	{10, 0, DMAC_NOT_USE, 0, 0, 0},
	{11, 0, DMAC_NOT_USE, 0, 0, 0},
	{12, 0, DMAC_NOT_USE, 0, 0, 0},
	{13, 0, DMAC_NOT_USE, 0, 0, 0},
	{14, 0, DMAC_NOT_USE, 0, 0, 0},
	{15, 0, DMAC_NOT_USE, 0, 0, 0},
	{16, UART0_RX_FIFO, DMAC_HOST1, (0x40000004), PERI_8BIT_MODE, 16},
	{17, UART0_TX_FIFO, DMAC_HOST1, (0x80000004), PERI_8BIT_MODE, 17},
	{18, UART1_RX_FIFO, DMAC_HOST1, (0x40000004), PERI_8BIT_MODE, 18},
	{19, UART1_TX_FIFO, DMAC_HOST1, (0x80000004), PERI_8BIT_MODE, 19},
	{20, UART2_RX_FIFO, DMAC_HOST1, (0x40000004), PERI_8BIT_MODE, 20},
	{21, UART2_TX_FIFO, DMAC_HOST1, (0x80000004), PERI_8BIT_MODE, 21},
	{22, UART3_RX_FIFO, DMAC_HOST1, (0x40000004), PERI_8BIT_MODE, 22},
	{23, UART3_TX_FIFO, DMAC_HOST1, (0x80000004), PERI_8BIT_MODE, 23},
	{24, UART4_RX_FIFO, DMAC_HOST1, (0x40000004), PERI_8BIT_MODE, 24},
	{25, UART4_TX_FIFO, DMAC_HOST1, (0x80000004), PERI_8BIT_MODE, 25},
	{26, SPI_RX_FIFO, DMAC_HOST1, (0x40000004), PERI_8BIT_MODE, 26},
	{27, SPI_TX_FIFO, DMAC_HOST1, (0x80000004), PERI_8BIT_MODE, 27},
	{28, 0, DMAC_NOT_USE, 0, 0, 0},
	{29, 0, DMAC_NOT_USE, 0, 0, 0},
	{30, 0, DMAC_NOT_USE, 0, 0, 0},
	{31, 0, DMAC_NOT_USE, 0, 0, 0}
};
#endif
