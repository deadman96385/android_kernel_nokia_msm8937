#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/file.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
//#include <mach/msm_iomap.h>
#include <linux/io.h>
#include <linux/init.h>

#include <linux/time.h>
#include <linux/device.h>
#include <linux/hrtimer.h>
#include <linux/reboot.h>

#define FIH_IPO_SUSPEND_STATE "ipo_suspend"
#define FIH_IPO_RESUME_STATE "ipo_resume"

#define PRESS_PWRKEY_TIME 2

extern void fih_ipo_pon_to_pwrkey(void);
extern void fih_ipo_pon_to_rtc(void);
extern void fih_ipo_pon_to_usb(void);

static char ipo_status[32] = {0};

static struct kobject *fih_ipo_kobj = NULL;

static struct hrtimer *pwrkey_timer = NULL;
static char *ipo_pwrkey_str = "power_key";
static char *ipo_rtc_str = "rtc_alarm";
static char *ipo_usb_str = "usb_chg";
static struct timespec power_key_pressed , power_key_released;
static int rtc_flag = 0;
static int usb_flag = 0;
static int pwrkey_boot_on = 0;
static int ipo_shutdown_capacity = 5;
static int shutdown_flag = 0;

static enum hrtimer_restart ipo_pwrkey_timer_func(struct hrtimer *timer)
{
	pwrkey_boot_on = 1;
	return HRTIMER_NORESTART;
}

void fih_ipo_set_power_key_pressed(void) {
	getnstimeofday(&power_key_pressed);
	if (hrtimer_try_to_cancel(pwrkey_timer) >= 0) {
		pwrkey_boot_on = 0;
		hrtimer_start(pwrkey_timer, ktime_set(PRESS_PWRKEY_TIME, 0),
			HRTIMER_MODE_REL);
		pr_debug("%s:timer start\n", __func__);
	}
}
EXPORT_SYMBOL(fih_ipo_set_power_key_pressed);

void fih_ipo_set_power_key_released(void) {
	getnstimeofday(&power_key_released);
	if (hrtimer_try_to_cancel(pwrkey_timer) >= 0) {
		pwrkey_boot_on = 0;
		pr_debug("%s:timer cancel\n", __func__);
	}
}
EXPORT_SYMBOL(fih_ipo_set_power_key_released);

void fih_ipo_set_rtc_flag(void) {
	rtc_flag = 1;
    pr_info("%s: %d\n", __func__, rtc_flag);
}
EXPORT_SYMBOL(fih_ipo_set_rtc_flag);

void fih_ipo_set_usb_flag(void) {
	usb_flag = 1;
    pr_info("%s: %d\n", __func__, usb_flag);
}
EXPORT_SYMBOL(fih_ipo_set_usb_flag);

int fih_ipo_get_suspend_state(void) {
	if(strncmp(ipo_status, FIH_IPO_SUSPEND_STATE, strlen(FIH_IPO_SUSPEND_STATE)) == 0) {
		return 1;
	} else {
		return 0;
	}
}
EXPORT_SYMBOL(fih_ipo_get_suspend_state);

int fih_ipo_get_usbchg_state(void) {
	if(strncmp(ipo_status, ipo_usb_str, strlen(ipo_usb_str)) == 0) {
		return 1;
	} else {
		return 0;
	}
}
EXPORT_SYMBOL(fih_ipo_get_usbchg_state);

int fih_ipo_shutdown_capacity(void)
{
    return ipo_shutdown_capacity;
}
EXPORT_SYMBOL(fih_ipo_shutdown_capacity);

void fih_ipo_set_shutdown_flag(void)
{
    shutdown_flag = 1;
}
EXPORT_SYMBOL(fih_ipo_set_shutdown_flag);

static int set_ipo_status(const char *str, int len)
{
	memset(ipo_status, 0, strlen(ipo_status));
	strncpy(ipo_status, str, len);
    pr_info("%s: %s\n", __func__, ipo_status);
	if (strncmp(str, ipo_pwrkey_str, strlen(ipo_pwrkey_str)) == 0) {
		fih_ipo_pon_to_pwrkey();
	} else if (strncmp(str, ipo_rtc_str, strlen(ipo_rtc_str)) == 0) {
		fih_ipo_pon_to_rtc();
	} else if (strncmp(str, ipo_usb_str, strlen(ipo_usb_str)) == 0) {
		fih_ipo_pon_to_usb();
	}

	return 0;
}

int fih_ipo_need_suspend(void) {
	int keep_suspend = 1;

	if(power_key_pressed.tv_sec != 0) {
		// Check power key pressed more than 2 seconds
		while(1) {
			msleep(100);
			if (pwrkey_boot_on) {
				set_ipo_status(ipo_pwrkey_str, strlen(ipo_pwrkey_str));
				keep_suspend = 0;
				break;
			}
			if((power_key_released.tv_sec != 0)) {
				if(power_key_released.tv_sec - power_key_pressed.tv_sec >= PRESS_PWRKEY_TIME) {
					set_ipo_status(ipo_pwrkey_str, strlen(ipo_pwrkey_str));
					keep_suspend = 0;
					break;
				} else {
					pr_info("power key pressed less than 2 seconds\n");
					keep_suspend = 1;
					break;
				}
			}
		}
	} else if (rtc_flag) {
		set_ipo_status(ipo_rtc_str, strlen(ipo_rtc_str));
		keep_suspend = 0;
	} else if (usb_flag) {
		set_ipo_status(ipo_usb_str, strlen(ipo_usb_str));
		keep_suspend = 0;
    } else if (shutdown_flag) {
        pr_emerg("[%s] Capacity is low to silent shutdown!\n", __func__);
        orderly_poweroff(true);
        shutdown_flag = 0;
        keep_suspend = 0;
	}

	memset(&power_key_pressed, 0, sizeof(power_key_pressed));
	memset(&power_key_released, 0, sizeof(power_key_released));
	rtc_flag = 0;
	usb_flag = 0;
	pwrkey_boot_on = 0;

	pr_info("fih_ipo_need_suspend return %d\n", keep_suspend);
	return keep_suspend;
}
EXPORT_SYMBOL(fih_ipo_need_suspend);

static ssize_t ipo_state_store(struct device *dev, struct  device_attribute *attr, const char *buf, size_t size)
{
	set_ipo_status(buf, size);
	return size;
}
static ssize_t ipo_state_show(struct device *dev, struct  device_attribute *attr , char *buf)
{
	snprintf(buf, sizeof(ipo_status), "%s", ipo_status);
	return strlen(buf);
}

static DEVICE_ATTR(ipo_state, 0644, ipo_state_show, ipo_state_store);

static struct attribute *fih_ipo_attributes[] = {
	&dev_attr_ipo_state.attr,
	NULL
};

static const struct attribute_group fih_ipo_attr_group = {
	.attrs = fih_ipo_attributes,
};


static int __init fih_ipo_init(void)
{
	int ret = -ENOMEM;

	//set power-key hrtimer
	pwrkey_timer = kzalloc(sizeof(*pwrkey_timer), GFP_KERNEL);
	if (pwrkey_timer) {
		hrtimer_init(pwrkey_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		pwrkey_timer->function = ipo_pwrkey_timer_func;
	} else {
		pr_err("[%s] Allocate memory failed\n", __func__);
	}

	// path: /sys/kernel/ipo
	fih_ipo_kobj = kobject_create_and_add("ipo", kernel_kobj);

	ret = sysfs_create_group(fih_ipo_kobj, &fih_ipo_attr_group);
	if (ret) {
		kobject_put(fih_ipo_kobj);
	}

	return ret;
}

static void __exit fih_ipo_exit(void)
{
	if (fih_ipo_kobj) {
		kobject_put(fih_ipo_kobj);
	}
	if (pwrkey_timer) {
		kfree(pwrkey_timer);
		pwrkey_timer = NULL;
	}
}

module_init(fih_ipo_init);
module_exit(fih_ipo_exit);
