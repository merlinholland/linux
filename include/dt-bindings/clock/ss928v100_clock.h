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

#ifndef __DTS_SS928V100_CLOCK_H
#define __DTS_SS928V100_CLOCK_H

/*  fixed   rate    */
#define SS928V100_FIXED_2400M    1
#define SS928V100_FIXED_1200M    2
#define SS928V100_FIXED_1188M    3
#define SS928V100_FIXED_896M     4
#define SS928V100_FIXED_800M     5
#define SS928V100_FIXED_792M     6
#define SS928V100_FIXED_786M     7
#define SS928V100_FIXED_750M     8
#define SS928V100_FIXED_700M     9
#define SS928V100_FIXED_672M     10
#define SS928V100_FIXED_600M     11
#define SS928V100_FIXED_594M     12
#define SS928V100_FIXED_560M     13
#define SS928V100_FIXED_500M     14
#define SS928V100_FIXED_475M     15
#define SS928V100_FIXED_396M     16
#define SS928V100_FIXED_300M     17
#define SS928V100_FIXED_297M     18
#define SS928V100_FIXED_257M     19
#define SS928V100_FIXED_250M     20
#define SS928V100_FIXED_200M     21
#define SS928V100_FIXED_198M     22
#define SS928V100_FIXED_187P_5M   23
#define SS928V100_FIXED_150M     24
#define SS928V100_FIXED_148P_5M   25
#define SS928V100_FIXED_134M     26
#define SS928V100_FIXED_108M     27
#define SS928V100_FIXED_100M     28
#define SS928V100_FIXED_99M      29
#define SS928V100_FIXED_74P_25M   30
#define SS928V100_FIXED_72M      31
#define SS928V100_FIXED_64M      32
#define SS928V100_FIXED_60M      33
#define SS928V100_FIXED_54M      34
#define SS928V100_FIXED_50M      35
#define SS928V100_FIXED_49P_5M    36
#define SS928V100_FIXED_37P_125M  37
#define SS928V100_FIXED_36M      38
#define SS928V100_FIXED_27M      39
#define SS928V100_FIXED_25M      40
#define SS928V100_FIXED_24M      41
#define SS928V100_FIXED_12M      42
#define SS928V100_FIXED_12P_288M  43
#define SS928V100_FIXED_6M       44
#define SS928V100_FIXED_3M       45
#define SS928V100_FIXED_1P_6M     46
#define SS928V100_FIXED_400K     47
#define SS928V100_FIXED_100K     48

#define SS928V100_I2C0_CLK    50
#define SS928V100_I2C1_CLK    51
#define SS928V100_I2C2_CLK    52
#define SS928V100_I2C3_CLK    53
#define SS928V100_I2C4_CLK    54
#define SS928V100_I2C5_CLK    55

#define SS928V100_SPI0_CLK    62
#define SS928V100_SPI1_CLK    63
#define SS928V100_SPI2_CLK    64
#define SS928V100_SPI3_CLK    65

#define SS928V100_EDMAC_CLK   69
#define SS928V100_EDMAC_AXICLK   70

/*  mux clocks  */
#define SS928V100_PWM0_MUX		72
#define SS928V100_PWM1_MUX		73
#define SS928V100_I2C0_MUX	74
#define SS928V100_I2C1_MUX	75
#define SS928V100_I2C2_MUX	76
#define SS928V100_I2C3_MUX	77
#define SS928V100_I2C4_MUX	78
#define SS928V100_I2C5_MUX	79
#define SS928V100_FMC_MUX     80
#define SS928V100_HPAXI_MUX   81
#define SS928V100_DDRAXI_MUX  82
#define SS928V100_MMC0_MUX    83
#define SS928V100_UART0_MUX   84
#define SS928V100_UART1_MUX   85
#define SS928V100_UART2_MUX   86
#define SS928V100_UART3_MUX   87
#define SS928V100_UART4_MUX   88
#define SS928V100_UART5_MUX   89

/*  gate    clocks  */
#define SS928V100_FMC_CLK     90
#define SS928V100_UART0_CLK   91
#define SS928V100_UART1_CLK   92
#define SS928V100_UART2_CLK   93
#define SS928V100_UART3_CLK   94
#define SS928V100_UART4_CLK   95
#define SS928V100_UART5_CLK   96
#define SS928V100_MMC0_CLK    97
#define SS928V100_MMC1_CLK    98
#define SS928V100_MMC2_CLK    99
#define SS928V100_MMC3_CLK    100

#define SS928V100_ETH_CLK		101
#define SS928V100_ETH_MACIF_CLK	102
#define SS928V100_ETH1_CLK		103
#define SS928V100_ETH1_MACIF_CLK	104

/*  complex */
#define SS928V100_MAC0_CLK		110
#define SS928V100_MAC1_CLK		111
#define SS928V100_SATA_CLK		112
#define SS928V100_USB_CLK		113
#define SS928V100_USB1_CLK		114

#define SS928V100_MMC1_MUX    115
#define SS928V100_MMC2_MUX    116

/* lsadc clocks */
#define SS928V100_LSADC_CLK		120
#define SS928V100_PWM0_CLK		121
#define SS928V100_PWM1_CLK		122

/* pll clocks */
#define SS928V100_APLL_CLK		250

#define SS928V100_CRG_NR_CLKS		256

#endif	/* __DTS_SS928V100_CLOCK_H */

