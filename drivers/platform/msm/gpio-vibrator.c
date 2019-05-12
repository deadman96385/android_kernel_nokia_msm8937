/* Copyright (c) 2013-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/hrtimer.h>
#include <linux/of_device.h>
#include <linux/err.h>

#include "../../staging/android/timed_output.h"

#if 0
#undef pr_debug
#define pr_debug pr_err
#undef pr_info
#define pr_info pr_err
#undef dev_dbg
#define dev_dbg dev_err
#endif



struct gpio_vib_platform_data{
	const char *name;
	unsigned gpio;
	int timeout;
};

struct gpio_vib{
	const struct gpio_vib_platform_data *pdata;	
	struct timed_output_dev timed_dev;
	
	struct hrtimer vib_timer;	
	struct work_struct work;
	struct mutex lock;

	int state;
};


static void gpio_vib_enable(struct timed_output_dev *dev, int value)
{
	struct gpio_vib *vib = container_of(dev, struct gpio_vib,
					 timed_dev);


	mutex_lock(&vib->lock);
	hrtimer_cancel(&vib->vib_timer);

	if (value == 0) {
		vib->state = 0;
	} else {
		value = (value > vib->pdata->timeout ?
				 vib->pdata->timeout : value);
		vib->state = 1;
		hrtimer_start(&vib->vib_timer,
			      ktime_set(value / 1000, (value % 1000) * 1000000),
			      HRTIMER_MODE_REL);
	}
	mutex_unlock(&vib->lock);
	schedule_work(&vib->work);
}


static void gpio_vib_set(struct gpio_vib *vib, int on){
	if(vib && vib->pdata){
		gpio_set_value(vib->pdata->gpio,on);
	}
}


static void gpio_vib_update(struct work_struct *work)
{
	struct gpio_vib *vib = container_of(work, struct gpio_vib,
					 work);
	gpio_vib_set(vib, vib->state);
}

static int gpio_vib_get_time(struct timed_output_dev *dev)
{
	struct gpio_vib *vib = container_of(dev, struct gpio_vib,
							 timed_dev);

	if (hrtimer_active(&vib->vib_timer)) {
		ktime_t r = hrtimer_get_remaining(&vib->vib_timer);

		return (int)ktime_to_us(r);
	} else
		return 0;
}

static enum hrtimer_restart gpio_vib_timer_func(struct hrtimer *timer)
{
	struct gpio_vib *vib = container_of(timer, struct gpio_vib,
							 vib_timer);

	vib->state = 0;
	schedule_work(&vib->work);

	return HRTIMER_NORESTART;
}

#ifdef CONFIG_PM
static int gpio_vibrator_suspend(struct device *dev)
{
	struct gpio_vib *vib = dev_get_drvdata(dev);

	hrtimer_cancel(&vib->vib_timer);
	cancel_work_sync(&vib->work);
	/* turn-off vibrator */
	gpio_vib_set(vib, 0);

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(gpio_vibrator_pm_ops, gpio_vibrator_suspend, NULL);


struct gpio_vib_platform_data *gpio_vib_dt_to_pdata(struct platform_device *pdev){
	struct device_node *node = pdev->dev.of_node;
	struct gpio_vib_platform_data *pdata;
	
	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		pr_err("unable to allocate platform data\n");
		return NULL;
	}

	of_property_read_u32(node, "fih,gpio-num",
				&pdata->gpio);
	of_property_read_u32(node, "fih,timeout",
				&pdata->timeout);

	pr_err("gpio %d,timeout %d\n"
		,pdata->gpio
		,pdata->timeout);
	return pdata;
}



static int gpio_vibrator_probe(struct platform_device *pdev)
{	
	struct gpio_vib_platform_data *pdata;
	struct gpio_vib *gpio_vib;
	int ret = 0;

	pr_debug("%s()\n",__func__);

	gpio_vib = devm_kzalloc(&pdev->dev, sizeof(*gpio_vib),
					GFP_KERNEL);
	if (!gpio_vib) {
		dev_err(&pdev->dev, "Failed to alloc driver structure\n");
		return -ENOMEM;
	}

	if (pdev->dev.of_node) {
		dev_err(&pdev->dev, "device tree enabled\n");
		pdata = gpio_vib_dt_to_pdata(pdev);
		if (!pdata) {
			dev_err(&pdev->dev, "DT parsing failed\n");
			ret = -ENOMEM;
			goto err_free;
		}
		pdev->dev.platform_data = pdata;
	}else{
		ret = -ENODEV;
		goto err_free;
	}

	ret = gpio_request(pdata->gpio, dev_name(&pdev->dev));
	if (ret) {
		dev_err(&pdev->dev, "Failed to request gpio pin: %d\n", ret);
		goto err_free;
	}
	ret = gpio_direction_output(pdata->gpio , 0);
	if (ret) {
		dev_err(&pdev->dev, "Failed to set gpio to input: %d\n", ret);
		goto err_gpio_free;
	}

	mutex_init(&gpio_vib->lock);
	INIT_WORK(&gpio_vib->work, gpio_vib_update);

	hrtimer_init(&gpio_vib->vib_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	gpio_vib->vib_timer.function = gpio_vib_timer_func;

	gpio_vib->timed_dev.name = "vibrator";
	gpio_vib->timed_dev.get_time = gpio_vib_get_time;
	gpio_vib->timed_dev.enable = gpio_vib_enable;

	gpio_vib->pdata = pdata;
	platform_set_drvdata(pdev,gpio_vib);

	ret = timed_output_dev_register(&gpio_vib->timed_dev);
	if (ret >= 0)
		return ret;
	
err_gpio_free:
	gpio_free(pdata->gpio);
err_free:
	return ret;

}

static int gpio_vibrator_remove(struct platform_device *pdev)
{
	struct gpio_vib *vib = platform_get_drvdata(pdev);

	cancel_work_sync(&vib->work);
	hrtimer_cancel(&vib->vib_timer);
	timed_output_dev_unregister(&vib->timed_dev);
	mutex_destroy(&vib->lock);
	gpio_free(vib->pdata->gpio);
	
	return 0;
}

static struct of_device_id gpio_vibrator_match_table[] = {
	{	.compatible = "fih,gpio-vibrator",},
	{},
};

static struct platform_driver gpio_vibrator_driver = {
	.driver		= {
		.name	= "gpio-vibrator",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(gpio_vibrator_match_table),
	},
	.probe		= gpio_vibrator_probe,
	.remove		= gpio_vibrator_remove,
};

module_platform_driver(gpio_vibrator_driver);


MODULE_AUTHOR("<york.y.pan@mail.foxconn.com>");
MODULE_DESCRIPTION("gpio vibrator driver");
MODULE_LICENSE("GPL v2");

