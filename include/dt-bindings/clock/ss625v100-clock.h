/*
 * Copyright (c) 2016-2017 Shenshu Technologies Co., Ltd.
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

#ifndef __DTS_SS625V100_CLOCK_H
#define __DTS_SS625V100_CLOCK_H

#define SS625V100_FIXED_1188M     1
#define SS625V100_FIXED_1000M     2
#define SS625V100_FIXED_842M      3
#define SS625V100_FIXED_792M      4
#define SS625V100_FIXED_750M      5
#define SS625V100_FIXED_710M      6
#define SS625V100_FIXED_680M      7
#define SS625V100_FIXED_667M      8
#define SS625V100_FIXED_631M      9
#define SS625V100_FIXED_600M      10
#define SS625V100_FIXED_568M      11
#define SS625V100_FIXED_500M      12
#define SS625V100_FIXED_475M      13
#define SS625V100_FIXED_428M      14
#define SS625V100_FIXED_400M      15
#define SS625V100_FIXED_396M      16
#define SS625V100_FIXED_300M      17
#define SS625V100_FIXED_250M      18
#define SS625V100_FIXED_198M      19
#define SS625V100_FIXED_187P_5M    20
#define SS625V100_FIXED_150M      21
#define SS625V100_FIXED_148P_5M    22
#define SS625V100_FIXED_125M      23
#define SS625V100_FIXED_107M      24
#define SS625V100_FIXED_100M      25
#define SS625V100_FIXED_99M       26
#define SS625V100_FIXED_74P_25M    27
#define SS625V100_FIXED_72M       28
#define SS625V100_FIXED_60M       29
#define SS625V100_FIXED_54M       30
#define SS625V100_FIXED_50M       31
#define SS625V100_FIXED_49P_5M     32
#define SS625V100_FIXED_37P_125M   33
#define SS625V100_FIXED_36M       34
#define SS625V100_FIXED_32P_4M     35
#define SS625V100_FIXED_27M       36
#define SS625V100_FIXED_25M       37
#define SS625V100_FIXED_24M       38
#define SS625V100_FIXED_12M       39
#define SS625V100_FIXED_3M        40
#define SS625V100_FIXED_1P_6M      41
#define SS625V100_FIXED_400K      42
#define SS625V100_FIXED_100K      43
#define SS625V100_FIXED_200M      44
#define SS625V100_FIXED_163M      45
#define SS625V100_FIXED_257M      46
#define SS625V100_FIXED_75M       47
#define SS625V100_FIXED_196P_5M    48
#define SS625V100_FIXED_175M      49
#define SS625V100_FIXED_297M      50

#define SS625V100_I2C0_CLK    51
#define SS625V100_I2C1_CLK    52
#define SS625V100_I2C2_CLK    53
#define SS625V100_I2C3_CLK    54
#define SS625V100_I2C4_CLK    55
#define SS625V100_I2C5_CLK    56

#define SS625V100_SPI0_CLK    62
#define SS625V100_SPI1_CLK    63
#define SS625V100_SPI2_CLK    64

#define SS625V100_EDMAC_CLK   69
#define SS625V100_EDMAC_AXICLK   70

#define SS625V100_VDMAC_CLK   73

/*  mux clocks  */
#define SS625V100_I2C0_MUX	78
#define SS625V100_I2C1_MUX	79
#define SS625V100_FMC_MUX     80
#define SS625V100_SYSAPB_MUX  81
#define SS625V100_SYSAXI_MUX  82
#define SS625V100_MMC0_MUX    83
#define SS625V100_UART0_MUX	84
#define SS625V100_UART1_MUX	85
#define SS625V100_UART2_MUX	86
#define SS625V100_UART3_MUX	87
#define SS625V100_UART4_MUX	88

/*  gate    clocks  */
#define SS625V100_FMC_CLK     90
#define SS625V100_UART0_CLK   91
#define SS625V100_UART1_CLK   92
#define SS625V100_UART2_CLK   93
#define SS625V100_UART3_CLK   94
#define SS625V100_UART4_CLK   95
#define SS625V100_MMC0_CLK    96
#define SS625V100_MMC1_CLK    97
#define SS625V100_MMC2_CLK    98
#define SS625V100_MMC3_CLK    99

#define SS625V100_ETH_CLK		100
#define SS625V100_ETH_MACIF_CLK	101
#define SS625V100_ETH1_CLK		102
#define SS625V100_ETH1_MACIF_CLK	103

/*  complex */
#define SS625V100_MAC0_CLK		110
#define SS625V100_MAC1_CLK		111
#define SS625V100_SATA_CLK		112
#define SS625V100_USB_CLK		113
#define SS625V100_USB1_CLK		114

/* pll clocks */
#define SS625V100_APLL_CLK		250
#define SS625V100_GPLL_CLK		251

#define SS625V100_CRG_NR_CLKS		256

#endif
