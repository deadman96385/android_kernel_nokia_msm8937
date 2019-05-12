/* shirda_kdrv_config_MSM8994.h (Infrared driver module)
 *
 * Copyright (C) 2009-2010 SHARP CORPORATION All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define	SHIRDA_DRV_USE_DEVICE_TREE
#define	SHIRDA_PF_LOLLIPOP_OR_LATER
#undef	SHIRDA_USE_CLK_GET_SYS
#define SHIRDA_TTY_WITH_ENTITY_OF_TERMIOS

#undef	SHIRDA_TEST_GPIO_DEBUG

#define	SHIRDA_BLSP_NODE	"7AEF000.uart"

#define	GSBIREG_BASE		(0x7AEF000)
#define	UART_DM_REGISTER	(0x0)
#define	UART_DM_IRDA		(0x00B8)
