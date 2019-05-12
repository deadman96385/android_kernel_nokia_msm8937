/* shirda_msm_kdrv.c (Infrared driver module)
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
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/clk.h>
#include <linux/wakelock.h>
#include <linux/spinlock.h>
#include <linux/hrtimer.h>
#include <linux/delay.h>
#include <asm/io.h>


#include <linux/gpio.h>

#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>

#include <linux/moduleparam.h>

#include SHIRDA_CONFIG_H

#include "sharp/irda_kdrv_api.h"

#define	SHIRDA_DRIVER_NAME	SHIRDA_DEVFILE_NAME
#include "shirda_kdrv.h"


#define	SHIRDA_WAKELOCK_SUSPEND
#undef	SHIRDA_WAKELOCK_IDLE

#undef	SHIRDA_GSBI_CTRL
#undef	SHIRDA_GSBI_PROTOCOL_CHECK

#define	SHIRDA_PREPARE_CLK

#define	SHIRDA_SIR_MODE_SETUP
#define	SHIRDA_GPIOCTL_TXRX
#undef	SHIRDA_GPIO_FUNC_NOT_SET

#ifdef	SHIRDA_DEBUG
#define	SHIRDA_DEBUG_ENTRY_POINT
#define	SHIRDA_DEBUG_GPIO_INIT
#endif

#define	SHIRDA_KERNEL_DRIVER_VERSION	"1.55.00"
#define	SHIRDA_KERNEL_DRIVER_NAME	SHIRDA_DEVFILE_NAME
#ifdef	SHIRDA_TEST_DRIVER
#define SHIRDA_TEST_DRIVER_NAME		SHIRDA_TEST_DEVFILE_NAME
#endif

#define SHIRDA_CORE_CLK_FREQ	(7372800)

#define	SHIRDA_GPIO_HIGH	(1)
#define	SHIRDA_GPIO_LOW		(0)
#define	SHIRDA_GPIO_IDLE	(0)
#define	SHIRDA_SD_SHUTDOWN	(1)
#define	SHIRDA_SD_ACTIVE	(0)

#define	SHIRDA_WAIT_MODESEQ	(200*1000)
#define	SHIRDA_MODESEQ_SDACTIME	(1)
#define	SHIRDA_MODESEQ_WAITTXD	(400*1000)



#define	SHIRDA_UART_DM_IRDA    (GSBIREG_BASE + UART_DM_REGISTER + UART_DM_IRDA)

#ifdef	SHIRDA_GSBI_CTRL
#define SHIRDA_GSBI_CTRL_REG	(GSBIREG_BASE + GSBI_CTRL_REG)
#define GSBI_PROTOCOL_CODE	(0x00000070)
#define GSBI_PROTOCOL_IDLE	(0x00000000)
#define GSBI_PROTOCOL_I2C	(0x00000020)
#define GSBI_PROTOCOL_UART	(0x00000040)
#define GSBI_PROTOCOL_I2C_UART	(0x00000060)
#define CRCI_MUX_CTRL		(0x00000001)
#endif

#define	MEDIUM_RATE_EN		(0x0010)
#define	IRDA_LOOPBACK		(0x0008)
#define	INVERT_IRDA_TX		(0x0004)
#define	INVERT_IRDA_RX		(0x0002)
#define	IRDA_EN			(0x0001)


static int shirda_open(struct inode *inode, struct file *fp);
static int shirda_release(struct inode *inode, struct file *fp);
static int shirda_driver_init(struct platform_device *pdev);
static int shirda_driver_remove(struct platform_device *pdev);


#ifdef	SHIRDA_TEST_DRIVER
static int shirda_test_open(struct inode *inode, struct file *fp);
static int shirda_test_release(struct inode *inode, struct file *fp);
#endif

typedef enum {
	SHIRDA_GSBI_NORMAL,
	SHIRDA_GSBI_INVERT,
	SHIRDA_GSBI_IRDA,
	SHIRDA_GSBI_IRDA_MIR,
#ifdef	SHIRDA_TEST_DRIVER
	SHIRDA_GSBI_LOOPBACK,
#endif
} shirda_gsbi_mode;

typedef enum {
	SHIRDA_LED_SIR_FUNC,
	SHIRDA_LED_FIR_FUNC,
} shirda_led_function;

typedef enum {
	SHIRDA_STATE_INIT,
	SHIRDA_STATE_IDLE,
	SHIRDA_STATE_READY,
	SHIRDA_STATE_OPEN,
	SHIRDA_STATE_ERROR
} shirda_main_state;

static shirda_main_state shirda_state = SHIRDA_STATE_INIT;

typedef enum {
	SHIRDA_GPIO_MODE_IDLE,
	SHIRDA_GPIO_MODE_ACTIVE,
	SHIRDA_GPIO_MODE_ACTIVE2,
	SHIRDA_GPIO_MODE_MAX
} shirda_gpio_mode;

static void shirda_gpio_free(void);
static int shirda_gpios_enable(shirda_gpio_mode mode);

#ifdef	SHIRDA_DRV_USE_DEVICE_TREE
static int shirda_gpio_init(struct platform_device *pdev);
#include	"shirda_dt_pinctrl.c"

static struct of_device_id	shirda_of_match_table[] = {
	{	.compatible = SHIRDA_PINCTRL_OF_IRDA,	},
	{		}
};

#else
static int shirda_gpio_init(void);
#include	"shirda_msm_gpio.c"
#endif

struct platform_device *shirda_device = NULL;
static struct platform_driver shirda_driver = {
#ifdef SHIRDA_PF_LOLLIPOP_OR_LATER
	.remove = shirda_driver_remove,
#else
	.remove = __devexit_p(shirda_driver_remove),
#endif
	.driver = {
		.name  = SHIRDA_KERNEL_DRIVER_NAME,
		.owner = THIS_MODULE,
#ifdef	SHIRDA_DRV_USE_DEVICE_TREE
		.of_match_table = shirda_of_match_table,
#endif
	},
};

static struct file_operations shirda_ops = {
	.owner		= THIS_MODULE,
	.open		= shirda_open,
	.release	= shirda_release,
};

static struct miscdevice shirda_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= SHIRDA_KERNEL_DRIVER_NAME,
	.fops	= &shirda_ops,
};





#ifdef	SHIRDA_TEST_DRIVER
static struct file_operations shirda_test_ops = {
	.owner		= THIS_MODULE,
	.open		= shirda_test_open,
	.release	= shirda_test_release,
};

static struct miscdevice shirda_test_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= SHIRDA_TEST_DRIVER_NAME,
	.fops	= &shirda_test_ops,
};
#endif

static spinlock_t shirda_lock;
#ifdef	SHIRDA_WAKELOCK_SUSPEND
#define	SHIRDA_WLOCK_SUSPEND_NAME "shirda_suspend_lock"
static struct wake_lock shirda_wlock_suspend;
#endif

#ifdef	SHIRDA_WAKELOCK_IDLE
#define	SHIRDA_WLOCK_IDLE_NAME "shirda_idle_lock"
static struct wake_lock shirda_wlock_idle;
#endif









#ifdef	SHIRDA_SIR_MODE_SETUP
static void shirda_led_set_sir(void)
{
	struct timespec wait_time;
	unsigned long lock_flag;

	wait_time.tv_sec  = 0;
	wait_time.tv_nsec = SHIRDA_WAIT_MODESEQ;
	hrtimer_nanosleep(&wait_time, NULL, HRTIMER_MODE_REL, CLOCK_MONOTONIC);

	spin_lock_irqsave(&shirda_lock, lock_flag);

	gpio_set_value(SHIRDA_GPIO_SD, SHIRDA_SD_SHUTDOWN);
	udelay(SHIRDA_MODESEQ_SDACTIME);
	gpio_set_value(SHIRDA_GPIO_SD, SHIRDA_SD_ACTIVE);

	spin_unlock_irqrestore(&shirda_lock, lock_flag);

	wait_time.tv_sec  = 0;
	wait_time.tv_nsec = SHIRDA_MODESEQ_WAITTXD;
	hrtimer_nanosleep(&wait_time, NULL, HRTIMER_MODE_REL, CLOCK_MONOTONIC);

	return;
}
#endif











static int shirda_gpio_set_irda_enable(shirda_led_function func)
{
	int ret = 0;

#ifdef	SHIRDA_DEBUG_ENTRY_POINT
#ifdef	SHIRDA_DEBUG_GPIO_INIT
	IRDALOG_INFO("entry shirda_gpios = %d\n", shirda_gpios);
#else
	IRDALOG_INFO("entry\n");
#endif
#endif

	gpio_set_value(SHIRDA_GPIO_SD, SHIRDA_SD_ACTIVE);



#ifdef	SHIRDA_SIR_MODE_SETUP
	shirda_led_set_sir();
#endif

#ifdef	SHIRDA_GPIOCTL_TXRX
	ret = shirda_gpios_enable(SHIRDA_GPIO_MODE_ACTIVE);
	if (ret != 0) {
		IRDALOG_FATAL("gpios_enable() fail errno = %d\n", ret);
	}
#endif

	IRDALOG_INFO("include config file %s \n", SHIRDA_CONFIG_H);

	return ret;
}

static int shirda_gpio_set_irda_disable(shirda_led_function func)
{
	int ret = 0;

#ifdef	SHIRDA_DEBUG_ENTRY_POINT
#ifdef	SHIRDA_DEBUG_GPIO_INIT
	IRDALOG_INFO("entry shirda_gpios = %d\n", shirda_gpios);
#else
	IRDALOG_INFO("entry\n");
#endif
#endif

#ifdef	SHIRDA_GPIOCTL_TXRX
	ret = shirda_gpios_enable(SHIRDA_GPIO_MODE_IDLE);
	if (ret != 0) {
		IRDALOG_FATAL("gpios_enable() fail errno = %d\n", ret);
	}
#endif


	gpio_set_value(SHIRDA_GPIO_SD, SHIRDA_SD_SHUTDOWN);

	return ret;
}

#ifndef	SHIRDA_PREPARE_CLK
static unsigned int irda_core_clk  = 0;
static unsigned int irda_iface_clk = 0;
#else
struct shirda_clk_table {
	struct clk	*clk;
	struct clk	*pclk;
};
static struct shirda_clk_table	shirda_clk = {
	.clk	= NULL,
	.pclk	= NULL,
};

#ifndef	SHIRDA_USE_CLK_GET_SYS
static int shirda_clk_init(struct platform_device *pdev);
static int shirda_clk_get(struct platform_device *pdev, struct clk **clkp,
							const char *clk_name);

static int shirda_clk_init(struct platform_device *pdev)
{
	int	ret;

	if (shirda_clk.clk != NULL) {
		IRDALOG_ERROR("clock is already initialized\n");
		return -EIO;
	}

	ret = shirda_clk_get(pdev, &shirda_clk.clk, "core_clk");
	if (ret == 0) {
		ret = shirda_clk_get(pdev, &shirda_clk.pclk, "iface_clk");
		if (ret == -EPROBE_DEFER) {
			clk_put(shirda_clk.clk);
			shirda_clk.clk = NULL;
		}
#ifdef	SHIRDA_PCLK_NOT_REQ
		else {
			ret = 0;
		}
#endif
	}
	return ret;
}
static int shirda_clk_get(struct platform_device *pdev, struct clk **clkp,
							const char *clk_name)
{
	struct clk	*p;
	int		ret = 0;

	p = clk_get(&pdev->dev, clk_name);
	if (unlikely(IS_ERR(p))) {
		ret = PTR_ERR(p);
		if (ret != -EPROBE_DEFER) {
			IRDALOG_ERROR("clk_get(%s) error %d\n", clk_name, ret);
		} else {
			IRDALOG_ERROR("clk_get(%s) request retry\n", clk_name);
		}
	} else {
		*clkp = p;
	}
	return ret;
}
#endif
#endif

static int shirda_clk_enable(void)
#ifdef	SHIRDA_PREPARE_CLK
{
	int	ret;

#ifdef	SHIRDA_DEBUG_ENTRY_POINT
	IRDALOG_INFO("entry. clk=0x%lx, pclk=0x%lx\n",
					(unsigned long)shirda_clk.clk,
					(unsigned long)shirda_clk.pclk);
#endif

#ifdef	SHIRDA_USE_CLK_GET_SYS
	shirda_clk.clk = clk_get_sys(SHIRDA_UART_CLK_NAME, "core_clk");
	if (unlikely(IS_ERR(shirda_clk.clk))) {
#else
	if (shirda_clk.clk == NULL) {
#endif
		IRDALOG_FATAL("The core_clk address error\n");
		return -EIO;
	} else {
		ret = clk_set_rate(shirda_clk.clk, SHIRDA_CORE_CLK_FREQ);
		if (ret != 0) {
			IRDALOG_FATAL("core_clk set rate error %d\n", ret);
			goto	clk_error;
		}
		ret = clk_prepare_enable(shirda_clk.clk);
		if (ret != 0) {
			IRDALOG_FATAL("core_clk enable error %d\n", ret);
			goto	clk_error;
		}
	}

#ifdef	SHIRDA_USE_CLK_GET_SYS
	shirda_clk.pclk = clk_get_sys(SHIRDA_UART_CLK_NAME, "iface_clk");
	if (!(IS_ERR(shirda_clk.pclk))) {
#else
	if (shirda_clk.pclk != NULL) {
#endif
		ret = clk_prepare_enable(shirda_clk.pclk);
		if (ret != 0) {
			IRDALOG_FATAL("iface_clk enable error %d\n", ret);
#ifndef	SHIRDA_PCLK_NOT_REQ
			goto	clk_error_disable;
#endif
		}
	} else {
#ifdef	SHIRDA_PCLK_NOT_REQ
		IRDALOG_ERROR("The iface_clk is not active\n");
#else
		IRDALOG_FATAL("The iface_clk address error\n");
		goto	clk_error_disable;
#endif
	}

	return 0;

#ifndef	SHIRDA_PCLK_NOT_REQ
clk_error_disable:
	clk_disable_unprepare(shirda_clk.clk);
#endif
clk_error:
#ifdef	SHIRDA_USE_CLK_GET_SYS
	clk_put(shirda_clk.clk);
#endif
	return -EIO;
}
#else
{
	struct clk *core_clk;
	struct clk *iface_clk;

#ifdef	SHIRDA_DEBUG_ENTRY_POINT
	IRDALOG_INFO("entry\n");
#endif

	if (irda_core_clk == 0) {
		core_clk  = clk_get_sys(SHIRDA_UART_CLK_NAME, "core_clk");
		if (IS_ERR(core_clk)) {
			IRDALOG_FATAL("The core_clk address get error.\n");
			return -EIO;
		}

		irda_core_clk = clk_get_rate(core_clk);
		if (irda_core_clk == 0) {
			clk_set_rate(core_clk, SHIRDA_CORE_CLK_FREQ);
			clk_enable(core_clk);
#ifdef	SHIRDA_ETC_DEBUG
			IRDALOG_INFO("Enable UARTDM core_clk.\n");
		} else {
			IRDALOG_INFO(
			  "Already core_clk is enabled for IrDA uses GSBI.\n");
#endif
		}

	} else {
		IRDALOG_FATAL(
		   "Error! Already core_clk is enabled for IrDA uses GSBI.\n");
		return -EIO;
	}

	if (irda_iface_clk == 0) {
		iface_clk  = clk_get_sys(SHIRDA_UART_CLK_NAME, "iface_clk");
		if (IS_ERR(iface_clk)) {
			IRDALOG_FATAL("The iface_clk address get error.\n");
			return -EIO;
		}

		irda_iface_clk = clk_get_rate(iface_clk);
		if (irda_iface_clk == 0) {
			clk_enable(iface_clk);
#ifdef	SHIRDA_ETC_DEBUG
			IRDALOG_INFO("The pclk is enable.\n");
		} else {
			IRDALOG_INFO("Already pclk is enable.\n");
#endif
		}

	} else {
		IRDALOG_FATAL(
			"Fatsl error! Already pclk is enabled for IrDA.\n");
		return -EIO;
	}

	return 0;
}
#endif

static void shirda_clk_disable(void)
#ifdef	SHIRDA_PREPARE_CLK
{
#ifdef	SHIRDA_DEBUG_ENTRY_POINT
	IRDALOG_INFO("entry\n");
#endif

#ifdef	SHIRDA_USE_CLK_GET_SYS
	shirda_clk.clk = clk_get_sys(SHIRDA_UART_CLK_NAME, "core_clk");
	if (!(IS_ERR(shirda_clk.clk))) {
#else
	if (shirda_clk.clk != NULL) {
#endif
		clk_disable_unprepare(shirda_clk.clk);
#ifdef	SHIRDA_USE_CLK_GET_SYS
		clk_put(shirda_clk.clk);
	} else {
		IRDALOG_FATAL("The core_clk(%s) address get error\n",
							SHIRDA_UART_CLK_NAME);
#endif
	}

#ifdef	SHIRDA_USE_CLK_GET_SYS
	shirda_clk.pclk = clk_get_sys(SHIRDA_UART_CLK_NAME, "iface_clk");
	if (!(IS_ERR(shirda_clk.pclk))) {
#else
	if (shirda_clk.pclk != NULL) {
#endif
		clk_disable_unprepare(shirda_clk.pclk);
#ifdef	SHIRDA_USE_CLK_GET_SYS
		clk_put(shirda_clk.pclk);
	} else {
		IRDALOG_FATAL("The iface_clk(%s) address get error\n",
							SHIRDA_UART_CLK_NAME);
#endif
	}

	return;
}
#else
{
	struct clk *core_clk;
	struct clk *iface_clk;

#ifdef	SHIRDA_DEBUG_ENTRY_POINT
	IRDALOG_INFO("entry\n");
#endif

	if (irda_core_clk == 0) {
		core_clk  = clk_get_sys(SHIRDA_UART_CLK_NAME, "core_clk");
		if (IS_ERR(core_clk)) {
			IRDALOG_FATAL(
				"The core_clk address get error.\n");
			goto lclk_dis_err;
		}

		if (clk_get_rate(core_clk) != 0) {
			IRDALOG_INFO("Disable UARTDM core_clk.\n");
			clk_disable(core_clk);
			clk_put(core_clk);
		}
#ifdef	SHIRDA_ETC_DEBUG
	} else {
		IRDALOG_INFO("IrDA core_clk is oscillating.\n");
#endif
	}
	irda_core_clk = 0;

	if (irda_iface_clk == 0) {
		iface_clk  = clk_get_sys(SHIRDA_UART_CLK_NAME, "iface_clk");
		if (IS_ERR(iface_clk)) {
			IRDALOG_FATAL(
				"The iface_clk address get error..\n");
			return;
		}

#ifdef	SHIRDA_ETC_DEBUG
		IRDALOG_INFO("The pclk is disable.\n");
#endif

		clk_disable(iface_clk);

		clk_put(iface_clk);
	}
	irda_iface_clk = 0;

lclk_dis_err:
	return;
}
#endif

#ifdef	SHIRDA_GSBI_CTRL
static void shirda_set_gsbi_ctrl(void)
{
	unsigned long gsbi_ctrl_reg;
	unsigned long lock_flag;

	void __iomem *gsbi_ctrl = ioremap_nocache(SHIRDA_GSBI_CTRL_REG, 4);

#ifdef	SHIRDA_DEBUG_ENTRY_POINT
	IRDALOG_INFO("entry\n");
#endif

	if (shirda_clk_enable() != 0) {
		IRDALOG_FATAL("Fatal error! can't enable clk.\n");
		goto lset_gsbi_ctrl_err;
	}

	spin_lock_irqsave(&shirda_lock, lock_flag);

	gsbi_ctrl_reg = ioread32(gsbi_ctrl);
	mb();
#ifdef	SHIRDA_ETC_DEBUG
	IRDALOG_INFO("GSBI_CTRL REG initial value = 0x%lx\n", gsbi_ctrl_reg);
	if ((gsbi_ctrl_reg & GSBI_PROTOCOL_CODE) != GSBI_PROTOCOL_I2C_UART) {
		IRDALOG_ERROR("ERROR PROTOCOL CODE, Check again.\n");
	}
#endif
	gsbi_ctrl_reg &= ((~GSBI_PROTOCOL_CODE) & (~CRCI_MUX_CTRL));
	gsbi_ctrl_reg |= GSBI_PROTOCOL_I2C_UART;
	writel_relaxed(gsbi_ctrl_reg, gsbi_ctrl);
	mb();

#ifdef	SHIRDA_ETC_DEBUG
	gsbi_ctrl_reg = ioread32(gsbi_ctrl);
	mb();
	IRDALOG_INFO("GSBI_CTRL REG write value = 0x%lx\n", gsbi_ctrl_reg);
#endif

	spin_unlock_irqrestore(&shirda_lock, lock_flag);

	shirda_clk_disable();

lset_gsbi_ctrl_err:
	if (gsbi_ctrl != NULL)	iounmap(gsbi_ctrl);
}

#ifdef	SHIRDA_GSBI_PROTOCOL_CHECK
static unsigned long shirda_get_gsbi_ctrl(void)
{
	unsigned long gsbi_ctrl_reg = 0;
	unsigned long lock_flag;
	void __iomem *gsbi_ctrl = ioremap_nocache(SHIRDA_GSBI_CTRL_REG, 4);

#ifdef	SHIRDA_DEBUG_ENTRY_POINT
	IRDALOG_INFO("entry\n");
#endif

	if (shirda_clk_enable() != 0) {
		IRDALOG_FATAL("Fatal error! can't enable clk.\n");
		goto lget_gsbi_ctrl_err;
	}

	spin_lock_irqsave(&shirda_lock, lock_flag);

	gsbi_ctrl_reg = ioread32(gsbi_ctrl);
	mb();
#ifdef	SHIRDA_ETC_DEBUG
	IRDALOG_INFO("GSBI10 CTRL_REG value = 0x%lx",
					(unsigned long)gsbi_ctrl_reg);
#endif

	spin_unlock_irqrestore(&shirda_lock, lock_flag);

	shirda_clk_disable();

lget_gsbi_ctrl_err:
	if (gsbi_ctrl != NULL)	iounmap(gsbi_ctrl);

	return (gsbi_ctrl_reg);
}

static int shirda_check_gsbi_ctrl(void)
{
	unsigned long	gsbi_ctrl_reg;
	int		ret = 0;

#ifdef	SHIRDA_DEBUG_ENTRY_POINT
	IRDALOG_INFO("entry\n");
#endif
	gsbi_ctrl_reg = shirda_get_gsbi_ctrl();

	if ((gsbi_ctrl_reg & GSBI_PROTOCOL_CODE) != GSBI_PROTOCOL_I2C_UART) {
		IRDALOG_ERROR("GSBI_CTRL_REG value error. %lx\n",
							gsbi_ctrl_reg);
		if (gsbi_ctrl_reg != 0) {
			ret = gsbi_ctrl_reg;
		} else {
			ret = -1;
		}
	}

	return ret;
}
#endif
#endif

static int shirda_set_uartdm_irda_enable(shirda_gsbi_mode mode)
{
	unsigned long	lock_flag;
	void __iomem	*gsbi_irda = ioremap_nocache(SHIRDA_UART_DM_IRDA, 4);
	int		ret = 0;

#ifdef	SHIRDA_DEBUG_ENTRY_POINT
	IRDALOG_INFO("entry shirda_gsbi_mode = %d\n", mode);
#endif
	ret = shirda_clk_enable();
	if (ret != 0) {
		IRDALOG_FATAL("Fatal error! can't enable clk.\n");
		goto uartdm_irda_ena_err;
	}

	spin_lock_irqsave(&shirda_lock, lock_flag);

	switch (mode) {
	case SHIRDA_GSBI_NORMAL:
		writel_relaxed((INVERT_IRDA_RX), gsbi_irda);
		mb();
		break;
	case SHIRDA_GSBI_INVERT:
		writel_relaxed((INVERT_IRDA_RX | INVERT_IRDA_TX), gsbi_irda);
		mb();
		break;
	case SHIRDA_GSBI_IRDA:
		writel_relaxed((INVERT_IRDA_RX | IRDA_EN), gsbi_irda);
		mb();
		break;
	case SHIRDA_GSBI_IRDA_MIR:
		writel_relaxed((INVERT_IRDA_RX | IRDA_EN | MEDIUM_RATE_EN),
								 gsbi_irda);
		mb();
		break;
#ifdef	SHIRDA_TEST_DRIVER
	case SHIRDA_GSBI_LOOPBACK:
		writel_relaxed((IRDA_LOOPBACK | INVERT_IRDA_RX | IRDA_EN),
								 gsbi_irda);
		mb();
		break;
#endif
	default:
		IRDALOG_FATAL("Internal error! shirda_gsbi_mode = %d\n", mode);
		ret = -EIO;
		break;
	}

	spin_unlock_irqrestore(&shirda_lock, lock_flag);

	shirda_clk_disable();

uartdm_irda_ena_err:

	if (gsbi_irda != NULL)	iounmap(gsbi_irda);

	return ret;
}

static void shirda_set_uartdm_irda_disable(void)
{
	unsigned long lock_flag;
	void __iomem *gsbi_irda = ioremap_nocache(SHIRDA_UART_DM_IRDA, 4);

#ifdef	SHIRDA_DEBUG_ENTRY_POINT
	IRDALOG_INFO("entry\n");
#endif

	if (shirda_clk_enable() != 0) {
		IRDALOG_FATAL("Fatal error! can't enable clk.\n");
		goto uartdm_irda_dis_err;
	}

	spin_lock_irqsave(&shirda_lock, lock_flag);

	writel_relaxed((INVERT_IRDA_RX), gsbi_irda);
	mb();

	spin_unlock_irqrestore(&shirda_lock, lock_flag);

	shirda_clk_disable();

uartdm_irda_dis_err:
	if (gsbi_irda != NULL)	iounmap(gsbi_irda);
}

static int shirda_open(struct inode *inode, struct file *fp)
{
	int ret = 0;

#ifdef	SHIRDA_DEBUG_ENTRY_POINT
	IRDALOG_INFO("entry ver = %s, state = %d\n",
			 SHIRDA_KERNEL_DRIVER_VERSION, shirda_state);
#endif

	switch (shirda_state) {
	case SHIRDA_STATE_IDLE:
	case SHIRDA_STATE_READY:
		ret = shirda_set_uartdm_irda_enable(SHIRDA_GSBI_IRDA);
		if (ret != 0) {
			IRDALOG_ERROR("IrDA setting was not completed.\n");
		} else {
			ret = shirda_gpio_set_irda_enable(SHIRDA_LED_SIR_FUNC);
		}
		break;
	default:
		IRDALOG_ERROR("open error. state = %d\n", shirda_state);
		return -EPERM;
	}

	if (ret == 0) {
#ifdef	SHIRDA_WAKELOCK_SUSPEND
		wake_lock(&shirda_wlock_suspend);
#ifdef	SHIRDA_ETC_DEBUG
		IRDALOG_INFO("wake_lock_suspend\n");
#endif
#endif
#ifdef	SHIRDA_WAKELOCK_IDLE
		wake_lock(&shirda_wlock_idle);
#ifdef	SHIRDA_ETC_DEBUG
		IRDALOG_INFO("wake_lock_idle\n");
#endif
#endif
	}

#ifdef	SHIRDA_GSBI_PROTOCOL_CHECK
	if (shirda_check_gsbi_ctrl() != 0) {
#ifdef	SHIRDA_GSBI_CTRL
		shirda_set_gsbi_ctrl();
		IRDALOG_INFO("set GSBI_CTRL_REG\n");
#else
		IRDALOG_FATAL("GSBI_CTRL_REG value error.\n")
		shirda_state = SHIRDA_STATE_ERROR;
		ret = -EBUSY
#endif
	}
#endif

	if (ret != 0) {
		shirda_set_uartdm_irda_disable();
	} else {
		shirda_state = SHIRDA_STATE_OPEN;
#ifdef	SHIRDA_TEST_GPIO_DEBUG
		gpio_set_value(SHIRDA_GPIO_TEST1, 0);
		gpio_set_value(SHIRDA_GPIO_TEST2, 0);
#endif
	}

	return ret;
}

static int shirda_release(struct inode *inode, struct file *fp)
{
	int ret = 0;

#ifdef	SHIRDA_DEBUG_ENTRY_POINT
	IRDALOG_INFO("entry ver = %s, state = %d\n",
			 SHIRDA_KERNEL_DRIVER_VERSION, shirda_state);
#endif

	if (shirda_state != SHIRDA_STATE_OPEN) {
		IRDALOG_ERROR("close error. state = %d\n", shirda_state);
		ret = -EPERM;
	} else {
		ret = shirda_gpio_set_irda_disable(SHIRDA_LED_SIR_FUNC);
		if (ret == 0) {
			shirda_state = SHIRDA_STATE_READY;
#ifdef	SHIRDA_WAKELOCK_SUSPEND
			wake_unlock(&shirda_wlock_suspend);
#ifdef	SHIRDA_ETC_DEBUG
			IRDALOG_INFO("wake_unlock_suspend\n");
#endif
#endif
#ifdef	SHIRDA_WAKELOCK_IDLE
			wake_unlock(&shirda_wlock_idle);
#ifdef	SHIRDA_ETC_DEBUG
			IRDALOG_INFO("wake_unlock_idle\n");
#endif
#endif
		} else {
			IRDALOG_FATAL("irda disable fatal error.\n");
			shirda_state = SHIRDA_STATE_ERROR;
			shirda_gpio_free();
		}
		shirda_set_uartdm_irda_disable();
	}
#ifdef	SHIRDA_TEST_GPIO_DEBUG
	if (ret == 0) {
		gpio_set_value(SHIRDA_GPIO_TEST1, 1);
		gpio_set_value(SHIRDA_GPIO_TEST2, 1);
	}
#endif
	return ret;
}





























#ifdef	SHIRDA_TEST_DRIVER
static int shirda_test_open(struct inode *inode, struct file *fp)
{
	int	ret;
#ifdef	SHIRDA_DEBUG_ENTRY_POINT
	IRDALOG_INFO("entry ver = %s, state = %d\n",
			 SHIRDA_KERNEL_DRIVER_VERSION, shirda_state);
#endif
	ret = shirda_set_uartdm_irda_enable(SHIRDA_GSBI_LOOPBACK);

	return ret;
}

static int shirda_test_release(struct inode *inode, struct file *fp)
{
#ifdef	SHIRDA_DEBUG_ENTRY_POINT
	IRDALOG_INFO("entry ver = %s, state = %d\n",
			 SHIRDA_KERNEL_DRIVER_VERSION, shirda_state);
#endif
	shirda_set_uartdm_irda_disable();

	return 0;
}
#endif

static char* node =  SHIRDA_BLSP_NODE;
module_param( node, charp, S_IRUGO );
MODULE_PARM_DESC( node, "BLSP node name of tty device");

static char* ver = SHIRDA_KERNEL_DRIVER_MASTER_VERSION;
module_param( ver, charp, S_IRUGO );
MODULE_PARM_DESC( ver, "sharp irda kernel driver version");

#ifdef SHIRDA_PF_LOLLIPOP_OR_LATER
static int shirda_driver_init(struct platform_device *pdev)
#else
static int __devinit shirda_driver_init(struct platform_device *pdev)
#endif
{
	int ret = 0;

#ifdef	SHIRDA_DEBUG_ENTRY_POINT
	IRDALOG_INFO("entry ver = %s, state = %d\n",
			 SHIRDA_KERNEL_DRIVER_VERSION, shirda_state);
#endif

	if (shirda_state != SHIRDA_STATE_INIT &&
		shirda_state != SHIRDA_STATE_ERROR) {
		IRDALOG_ERROR("already initialized  Now state: %d\n",
								shirda_state);
		return ret;
	}

#ifndef SHIRDA_USE_CLK_GET_SYS
	ret = shirda_clk_init(pdev);
	if (ret != 0) {
		IRDALOG_FATAL("clock initialization failed. errno = %d\n",
									ret);
		shirda_state = SHIRDA_STATE_ERROR;
		return ret;
	}
#endif

#ifdef	SHIRDA_DRV_USE_DEVICE_TREE
	ret = shirda_gpio_init(pdev);
#else
	ret = shirda_gpio_init();
#endif
	if (ret != 0) {
		IRDALOG_FATAL("gpio initialization failed. errno = %d\n", ret);
		shirda_state = SHIRDA_STATE_ERROR;
		return ret;
	}

	spin_lock_init(&shirda_lock);

#ifdef	SHIRDA_GSBI_CTRL
	shirda_set_gsbi_ctrl();
#endif
	if (shirda_set_uartdm_irda_enable(SHIRDA_GSBI_NORMAL) == 0) {
		shirda_state = SHIRDA_STATE_READY;
	} else {
		IRDALOG_ERROR("IrDA setting was not completed.\n");
		shirda_state = SHIRDA_STATE_IDLE;
	}

	misc_register(&shirda_misc);


#ifdef	SHIRDA_TEST_DRIVER
	misc_register(&shirda_test_misc);
#endif

#ifdef	SHIRDA_WAKELOCK_SUSPEND
	wake_lock_init(&shirda_wlock_suspend, WAKE_LOCK_SUSPEND,
						SHIRDA_WLOCK_SUSPEND_NAME);
#endif
#ifdef	SHIRDA_WAKELOCK_IDLE
	wake_lock_init(&shirda_wlock_idle, WAKE_LOCK_IDLE,
						SHIRDA_WLOCK_IDLE_NAME);
#endif

#ifdef	SHIRDA_DEBUG_ENTRY_POINT
	IRDALOG_INFO("return %d\n", ret);
#endif
	return ret;
}

static int shirda_driver_remove(struct platform_device *pdev)
{

#ifdef	SHIRDA_DEBUG_ENTRY_POINT
	IRDALOG_INFO("entry ver = %s, state = %d\n",
			 SHIRDA_KERNEL_DRIVER_VERSION, shirda_state);
#endif

#ifdef	SHIRDA_GSBI_CTRL
	shirda_set_uartdm_irda_disable();
#endif
	shirda_gpio_free();

#ifdef	SHIRDA_WAKELOCK_SUSPEND
	wake_lock_destroy(&shirda_wlock_suspend);
#endif
#ifdef	SHIRDA_WAKELOCK_IDLE
	wake_lock_destroy(&shirda_wlock_idle);
#endif

	misc_deregister(&shirda_misc);


#ifdef	SHIRDA_TEST_DRIVER
	misc_deregister(&shirda_test_misc);
#endif

	return 0;
}

static int __init shirda_module_init(void)
{
	int ret = 0;

#ifdef	SHIRDA_DEBUG_ENTRY_POINT
	IRDALOG_INFO("entry ver = %s, state = %d\n",
			 SHIRDA_KERNEL_DRIVER_VERSION, shirda_state);
#endif

	shirda_device = platform_device_alloc(
				(const char *)SHIRDA_KERNEL_DRIVER_NAME,
							(unsigned int) -1);
	if (shirda_device == NULL) {
		IRDALOG_FATAL("platform_device_alloc() failed\n");
		return -ENODEV;
	}

	ret = platform_device_add(shirda_device);
	if (ret != 0) {
		platform_device_put(shirda_device);
		IRDALOG_FATAL("platform_device_add() failed errno = %d\n",
								 ret);
	} else {
		ret = platform_driver_probe(&shirda_driver,
							shirda_driver_init);
		if (ret != 0) {
			platform_device_unregister(shirda_device);
			IRDALOG_FATAL(
				"platform_driver_probe() failed errno = %d\n",
								ret);
		}
	}


	return ret;
}

static void __exit shirda_module_exit(void)
{

	platform_driver_unregister(&shirda_driver);
	platform_device_unregister(shirda_device);
}

module_init(shirda_module_init);
module_exit(shirda_module_exit);

MODULE_DESCRIPTION("msm IrDA driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("SHARP CORPORATION");
MODULE_VERSION(SHIRDA_KERNEL_DRIVER_VERSION);
