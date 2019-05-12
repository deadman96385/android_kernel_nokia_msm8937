/* shirda_kdrv.h (Infrared driver module)
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
#ifndef __SHIRDA_CMN__
#define __SHIRDA_CMN__

#define SHIRDA_KERNEL_DRIVER_MASTER_VERSION	"2.68.00"

#define SHIRDA_DEBUG

#ifdef SHIRDA_DEBUG
#undef SHIRDA_ETC_DEBUG
#undef SHIRDA_TEST_GPIO_DEBUG
#endif

#ifndef SHIRDA_DRIVER_NAME
#ifdef SHIRDA_LDISC_DRIVER_NAME
#define SHIRDA_DRIVER_NAME	SHIRDA_LDISC_DRIVER_NAME
#else
#define SHIRDA_DRIVER_NAME	"shirda"
#endif
#endif

#define SHIRB_DRIVER_NAME "shirrc"

#define IRDALOG_FATAL(format, args...)	\
	printk(KERN_ALERT "[%s]%s() " format, SHIRDA_DRIVER_NAME, \
							__func__, ##args)
#define IRDALOG_ERROR(format, args...)	\
	printk(KERN_ERR "[%s]%s() " format, SHIRDA_DRIVER_NAME, \
							__func__, ##args)
#define IRDALOG_WARNING(format, args...)	\
	printk(KERN_WARNING "[%s]%s() " format, SHIRDA_DRIVER_NAME, \
							__func__, ##args)
#ifdef SHIRDA_DEBUG
#define IRDALOG_INFO(format, args...)	\
	printk(KERN_INFO "[%s]%s() " format, SHIRDA_DRIVER_NAME, \
							__func__, ##args)
#else
#define	IRDALOG_INFO(format, args...)
#endif

#define IRBLOG_FATAL(format, args...)	\
	printk(KERN_ALERT "[%s]%s() " format, SHIRB_DRIVER_NAME, \
							__func__, ##args)

#define IRBLOG_ERROR(format, args...)	\
	printk(KERN_ERR "[%s]%s() " format, SHIRB_DRIVER_NAME, \
							__func__, ##args)

#ifdef SHIRB_DEBUG
#define IRBLOG_INFO(format, args...)	\
	printk(KERN_INFO "[%s]%s() " format, SHIRB_DRIVER_NAME, \
							__func__, ##args)
#else
#define	IRBLOG_INFO(format, args...)
#endif

#endif
