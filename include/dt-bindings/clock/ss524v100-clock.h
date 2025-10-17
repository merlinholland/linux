/*
 * Copyright (c) 2019-2020 Shenshu Technologies Co., Ltd.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __DTS_SS524V100_CLOCK_H
#define __DTS_SS524V100_CLOCK_H

/* clk in SS524V100 CRG */
/* fixed rate clocks */
#define SS524V100_FIXED_100K      1
#define SS524V100_FIXED_400K      2
#define SS524V100_FIXED_3M        3
#define SS524V100_FIXED_6M        4
#define SS524V100_FIXED_24M       5
#define SS524V100_FIXED_24P75M    6
#define SS524V100_FIXED_49P5M     7
#define SS524V100_FIXED_50M       8
#define SS524V100_FIXED_54M       9
#define SS524V100_FIXED_90M       10
#define SS524V100_FIXED_99M       11
#define SS524V100_FIXED_112M      12
#define SS524V100_FIXED_125M      13
#define SS524V100_FIXED_148P5M    14
#define SS524V100_FIXED_150M      15
#define SS524V100_FIXED_198M      16
#define SS524V100_FIXED_250M      17
#define SS524V100_FIXED_297M      18
#define SS524V100_FIXED_324M      19
#define SS524V100_FIXED_342M      20
#define SS524V100_FIXED_375M      21
#define SS524V100_FIXED_396M      22
#define SS524V100_FIXED_448M      23
#define SS524V100_FIXED_500M      24
#define SS524V100_FIXED_540M      25
#define SS524V100_FIXED_600M      26
#define SS524V100_FIXED_750M      27
#define SS524V100_FIXED_900M      28
#define SS524V100_FIXED_1200M     29

/* mux clocks */
#define SS524V100_SYSAXI_CLK      30
#define SS524V100_SYSCFG_CLK      31
#define SS524V100_SYSAPB_CLK      32
#define SS524V100_FMC_MUX         33
#define SS524V100_MMC0_MUX        34
#define SS524V100_ETH_MUX         35
#define SS524V100_UART0_MUX       36
#define SS524V100_UART1_MUX       37
#define SS524V100_UART2_MUX       38
#define SS524V100_UART3_MUX       39
#define SS524V100_UART4_MUX       40
#define SS524V100_I2C0_MUX        41
#define SS524V100_I2C1_MUX        42


/* gate clocks */
#define SS524V100_FMC_CLK         50
#define SS524V100_EDMAC_AXICLK    51
#define SS524V100_EDMAC_CLK       52
#define SS524V100_MMC0_CLK        53
#define SS524V100_UART0_CLK       55
#define SS524V100_UART1_CLK       56
#define SS524V100_UART2_CLK       57
#define SS524V100_UART3_CLK       58
#define SS524V100_UART4_CLK       59
#define SS524V100_I2C0_CLK        60
#define SS524V100_I2C1_CLK        61
#define SS524V100_SPI0_CLK        62
#define SS524V100_SPI1_CLK        63


#define SS524V100_ETH0_CLK        100
#define SS524V100_ETH0_MACIF_CLK  101
#define SS524V100_ETH1_CLK        102
#define SS524V100_ETH1_FEPHY_CLK  103

#define SS524V100_NR_CLKS         256
#define SS524V100_NR_RSTS         256

#endif  /* __DTS_SS524V100_CLOCK_H */
