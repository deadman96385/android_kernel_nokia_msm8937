#include <fih/fih_usb.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/uaccess.h>

#include <linux/device.h>
#include <linux/file.h>
#include <linux/fs.h>

#include <linux/switch.h>
#include <linux/reboot.h>

#include <linux/slab.h>

#define FIH_PROC_FS_NEW

#ifdef FIH_PROC_FS
#include <linux/proc_fs.h>
#endif

#ifdef FIH_SYS_FS
#include <linux/slab.h>
#endif

#ifdef FIH_PROC_FS_NEW
#include <linux/proc_fs.h>
#endif

#undef fih_debug
#define fih_debug(x, ...) \
	do {if (g_fih_usb_debuglog)\
	{pr_debug("F@USB-2 fih_usb: " "%s():" x, __func__, ##__VA_ARGS__); } \
	} while (0)

struct usb_info {
	struct delayed_work       usb_work;
	struct workqueue_struct  *usb_workqueue;
};

static int g_fih_usb_debuglog;

extern int fih_tool_register_func(int (*callback)(int, void*));
extern void fih_tool_unregister_func(void (*callback)(int, void*));

static int mode_switch_reboot_flag;
static int reboot_flag;

int g_fih_ums = 0;

extern int selinux_enforcing;
extern void selnl_notify_setenforce(int val);
extern void selinux_status_update_setenforce(int enforcing);

extern char *saved_command_line;

#define PATH_SUT_INFO           "/proc/sutinfo"
#define BUILD_ID_0              "/proc/fver"
#define BUILD_ID_1              "/system/build_id"
#define BATTERY_CAPACITY_PATH   "/sys/class/power_supply/battery/capacity"

struct usb_pid_mapping {
	unsigned short pid_no_adb;
	unsigned short pid_adb;
};

struct usb_pid_mapping usb_fih_pid_mapping[] = {
	{0xC025,0xC026}, //MTP
	{0xC029,0xC02A}, //PTP
	{0xC004,0xC001}, //mass_storage
	{0xC021,0xC000}, //All port
	{0x0, 0x0}
};

#ifdef EXTER_PROJECT_PID_VID
struct usb_pid_mapping usb_mo7_pid_mapping[] = {
	{0x7A13,0x7A14}, //MTP
	{0x7A15,0x7A16}, //PTP
	{0xC021,0xC000}, //All port
	{0x0, 0x0}
};
#endif

struct usb_pid_mapping *p_usb_pid_mapping = NULL;

/*
 * Example :
 *
 * PIDINFO10035P003000000FIHXGOXXXXX1APPXXXXX103GOX20400013033.24011
 *
 * PIDINFO1    | 8 bytes      | Signature as partition header
 * 0035        | 4 bytes      | Data size of following data
 * P           | 1 bytes      | Signature as SUT information
 * 0030        | 4 bytes      | Data size of the following data for SUT information
 * 0000...4011 | 0x0030 bytes | SUT information
 *
 */

/*-------------------------------------------------------------------------*/

#ifdef FIH_PROC_FS_NEW
void fih_usb_root_enable_remove_proc(char *root_path)
{
	char file_path[64];

	sprintf(file_path, "%s/fih_usb_root_enable", root_path);
	remove_proc_entry(file_path, NULL);
}

static int fih_usb_log_enable_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", g_fih_usb_debuglog);
	return 0;
}

static ssize_t fih_usb_log_enable_write(struct file *file, const char __user *buffer,
	size_t count, loff_t *ppos)
{
	unsigned char tmp[16];
	unsigned int size;

	size = (count > sizeof(tmp))? sizeof(tmp):count;

	if ( copy_from_user(tmp, buffer, size) ) {
		pr_err("%s: copy_from_user fail\n", __func__);
		return -EFAULT;
	}

	g_fih_usb_debuglog = simple_strtoul(tmp, NULL, size);

	return size;
}

static int fih_usb_log_enable_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, fih_usb_log_enable_read, NULL);
};

static struct file_operations fih_usb_log_enable_file_ops = {
	.owner   = THIS_MODULE,
	.open    = fih_usb_log_enable_proc_open,
	.read    = seq_read,
	.write   = fih_usb_log_enable_write,
	.llseek  = seq_lseek,
	.release = single_release
};

void fih_usb_log_enable_create_proc(char *root_path)
{
	struct proc_dir_entry *pfile;
	char file_path[64];

	sprintf(file_path, "%s/fih_usb_log_enable", root_path);
	pfile = proc_create(file_path, 0666, NULL, &fih_usb_log_enable_file_ops);
	if (!pfile) {
		fih_debug("Can't create /proc/%s\n", file_path);
		return ;
	}
}

void fih_usb_log_enable_remove_proc(char *root_path)
{
	char file_path[64];

	sprintf(file_path, "%s/fih_usb_log_enable", root_path);
	remove_proc_entry(file_path, NULL);
}

void fih_usb_create_proc_new(void)
{
	static struct proc_dir_entry *fihdir;

	char root_path[64];
	char file_path[64];

	sprintf(root_path, "AllHWList");
	sprintf(file_path, "%s/fih_usb", root_path);

	fihdir = proc_mkdir(file_path, NULL);
	if (!fihdir) {
		fih_debug("Can't create /proc/%s\n", file_path);
		fihdir = proc_mkdir(root_path, NULL);
		if (!fihdir) {
			fih_debug("Can't create /proc/%s\n", root_path);
			return ;
		}
		fihdir = proc_mkdir(file_path, NULL);
		if (!fihdir) {
			fih_debug("Can't create /proc/%s\n", file_path);
			return ;
		}
	}
	fih_usb_log_enable_create_proc(file_path);
}
void fih_usb_remove_proc_new(void)
{
	char root_path[64];
	char file_path[64];

	sprintf(root_path, "AllHWList");
	sprintf(file_path, "%s/fih_usb", root_path);
	fih_usb_log_enable_remove_proc(file_path);
	fih_usb_root_enable_remove_proc(file_path);
}
#endif

#ifdef FIH_PROC_FS
static int fih_usb_root_enable_read(char *page, char **start, off_t off, int count, int *eof, void *data)
{
		int len = 0;
		char buf[255];
		sprintf(buf, "%d\n", scsi_set_adb_root);
		len = strlen(buf);
		if (off >= len)
			return 0;
		if (count > len - off)
			count = len - off;
		memcpy(page + off, buf + off, count);
		return off + count;
}


static int fih_usb_log_enable_read(char *page, char **start, off_t off, int count, int *eof, void *data)
{
		int len = 0;
		char buf[255];
		sprintf(buf, "%d\n", g_fih_usb_debuglog);
		len = strlen(buf);
		if (off >= len)
			return 0;
		if (count > len - off)
			count = len - off;
		memcpy(page + off, buf + off, count);
		return off + count;
}
static int fih_usb_log_enable_write(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
		unsigned long count2 = count;
		char buf[255];
		if (count2 >= sizeof(buf))
			count2 = sizeof(buf) - 1;
		if (copy_from_user(buf, buffer, count2))
			return -EFAULT;
		buf[count2] = '\0';
		sscanf(buf, "%d", &g_fih_usb_debuglog);
		fih_debug("g_fih_usb_debuglog = %d\n",  g_fih_usb_debuglog);
		return count;
}
void fih_usb_log_enable_create_proc(char *root_path)
{
	struct proc_dir_entry *pfile;
	char file_path[64];

	sprintf(file_path, "%s/fih_usb_log_enable", root_path);
	pfile = create_proc_entry(file_path, 0666, NULL);
	if (!pfile) {
		fih_debug("Can't create /proc/%s\n", file_path);
		return ;
	}
	pfile->read_proc = fih_usb_log_enable_read;
	pfile->write_proc = fih_usb_log_enable_write;
}

void fih_usb_create_proc(void)
{
	static struct proc_dir_entry *fihdir;

	char root_path[64];
	char file_path[64];

	sprintf(root_path, "AllHWList");
	sprintf(file_path, "%s/fih_usb", root_path);

	fihdir = proc_mkdir(file_path, NULL);
	if (!fihdir) {
		fih_debug("Can't create /proc/%s\n", file_path);
		fihdir = proc_mkdir(root_path, NULL);
		if (!fihdir) {
			fih_debug("Can't create /proc/%s\n", root_path);
			return ;
		}
		fihdir = proc_mkdir(file_path, NULL);
		if (!fihdir) {
			fih_debug("Can't create /proc/%s\n", file_path);
			return ;
		}
	}
	fih_usb_log_enable_create_proc(file_path);
}
#endif
#ifdef FIH_SYS_FS
static ssize_t fih_usb_root_enable_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", scsi_set_adb_root);
}
static ssize_t fih_usb_root_enable_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	sscanf(buf, "%d", &scsi_set_adb_root);
	fih_debug("fih_usb_root_enable(%d)\n", scsi_set_adb_root);
	return count;
}
static DEVICE_ATTR(fih_usb_root_enable, 0644, fih_usb_root_enable_show, fih_usb_root_enable_store);

static ssize_t fih_usb_log_enable_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", g_fih_usb_debuglog);
}
static ssize_t fih_usb_log_enable_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	sscanf(buf, "%d", &g_fih_usb_debuglog);
	fih_debug("g_fih_usb_debuglog(%d)\n", g_fih_usb_debuglog);
	return count;
}
static DEVICE_ATTR(fih_usb_log_enable, 0644, fih_usb_log_enable_show, fih_usb_log_enable_store);
/*
static void fih_usb_dev_release(struct device *dev)
{
	pr_info("device: '%s': %s\n", dev_name(dev), __func__);
	kfree(dev);
}
*/
int fih_usb_create_sys_fs(void)
{
	struct device *fih_usb_dev;
	int rc;

	fih_usb_dev = kzalloc(sizeof(*fih_usb_dev), GFP_KERNEL);
	device_initialize(fih_usb_dev);
	fih_usb_dev->parent = NULL;
	//fih_usb_dev->release = fih_usb_dev_release;
	rc = kobject_set_name(&fih_usb_dev->kobj, "%s", "fih_usb_interface");
	if (rc)
		goto kobject_set_name_failed;
	rc = device_add(fih_usb_dev);
	if (rc)
		goto device_add_failed;

	rc = device_create_file(fih_usb_dev, &dev_attr_fih_usb_root_enable);
	if (rc) {
		pr_err("F@BAT-1 %s: dev_attr_fih_usb_root_enable failed\n", __func__);
		return rc;
	}
	rc = device_create_file(fih_usb_dev, &dev_attr_fih_usb_log_enable);
	if (rc) {
		pr_err("F@BAT-1 %s: dev_attr_fih_usb_log_enable failed\n", __func__);
		return rc;
	}
kobject_set_name_failed:
device_add_failed:
	put_device(fih_usb_dev);
	return rc;
}
#endif
void fih_usb_init_value(void)
{
	g_fih_usb_debuglog = 0;
	mode_switch_reboot_flag = false;
	reboot_flag = false;
}

static void usb_all_port_setting(struct work_struct *work)
{
	struct usb_info *info;
	struct delayed_work *usb_work = container_of(work, struct delayed_work, work);
	info = container_of(usb_work, struct usb_info, usb_work);

	if(selinux_enforcing != 0) {
		pr_info("%s: disable selinux \n", __func__);
		selinux_enforcing = 0;
		selnl_notify_setenforce(selinux_enforcing);
		selinux_status_update_setenforce(selinux_enforcing);
	}
	pr_info("%s: enable all ports \n", __func__);

	//queue_delayed_work(info->usb_workqueue, &info->usb_work, msecs_to_jiffies(3000));
}

static int __init fih_usb_init(void)
{
	int rc = 0;
	struct usb_info *info = NULL;
	pr_info("%s\n", __func__);

	if (!strstr(saved_command_line,"androidboot.securityfused=false")) {
		pr_err("%s:fused status isn't false, disable SCSI command\n", __func__);
		return rc;
	}

	if (strstr(saved_command_line,"androidboot.allport=true")) {
		pr_info("%s: enable allport and disable selinux\n", __func__);
		info = kzalloc(sizeof (struct usb_info), GFP_KERNEL);
		info->usb_workqueue = alloc_workqueue("usb-queue", WQ_UNBOUND|WQ_HIGHPRI|WQ_CPU_INTENSIVE, 1);
		if (!info) {
			pr_info("%s: alloc workqueue fail \n", __func__);
		} else {
			INIT_DELAYED_WORK(&info->usb_work, usb_all_port_setting);
			pr_info("%s: Init workqueue  \n", __func__);
			queue_delayed_work(info->usb_workqueue, &info->usb_work, msecs_to_jiffies(11000));
		}
	}
	else
	{
		pr_info("%s: allport disable \n", __func__);
	}

	fih_usb_init_value();
#ifdef FIH_PROC_FS
	fih_usb_create_proc();
#endif
#ifdef FIH_SYS_FS
	fih_usb_create_sys_fs();
#endif

#ifdef FIH_PROC_FS_NEW
	fih_usb_create_proc_new();
#endif


#ifdef EXTER_PROJECT_PID_VID
	switch(fih_hwid_fetch(FIH_HWID_PRJ)) {
	case FIH_PRJ_MO7:
		p_usb_pid_mapping = usb_mo7_pid_mapping;
		break;
	case FIH_PRJ_VNA:
	default:
		p_usb_pid_mapping = usb_fih_pid_mapping;
		break;
	}
#else
	p_usb_pid_mapping = usb_fih_pid_mapping;
#endif

	return rc;
}
static void __exit fih_usb_exit(void)
{
   pr_info("%s\n", __func__);
#ifdef FIH_PROC_FS_NEW
	fih_usb_remove_proc_new();
#endif
}

//module_init(fih_usb_init);
late_initcall(fih_usb_init);
module_exit(fih_usb_exit);
MODULE_LICENSE("GPL v2");
