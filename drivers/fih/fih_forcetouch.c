#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include "fih_forcetouch.h"

#define FIH_PROC_FT_IC_FW_VER	"AllHWList/ft_fw_ver"
#if 0
#define FIH_PROC_TP_SELF_TEST	"AllHWList/tp_self_test"
#define FIH_PROC_TP_UPGRADE		"AllHWList/tp_upgrade"
#define FIH_PROC_TP_FILE_FW_FW	"AllHWList/tp_fwim_ver"
#define FIH_PROC_TP_VENDOR		"AllHWList/tp_vendor"
#define FIH_PROC_TP_SMART_COVER  "AllHWList/tp_smart_cover"
#define FIH_PROC_TP_DOWN_GRADE  "AllHWList/tp_fw_back"
#endif

#if 0
extern bool ResultSelfTest;
extern void forcetouch_selftest(void);
extern void forcetouch_tpfwver_read(char *);
extern void forcetouch_fwback_write(void);
extern int forcetouch_fwback_read(void);
extern void forcetouch_tpfwimver_read(char *fw_ver);
extern int forcetouch_scover_read(void);
extern int forcetouch_scover_write(int);
extern void forcetouch_fwupgrade(int);
extern void forcetouch_fwupgrade_read(char *);
extern void forcetouch_vendor_read(char *);
#endif

#if 0
struct fih_forcetouch_cb forcetouch_cb = {
	.forcetouch_selftest	= NULL,
	.forcetouch_selftest_result = NULL,
	.forcetouch_tpfwver_read = NULL,
	.forcetouch_tpfwimver_read = NULL,
	.forcetouch_fwupgrade = NULL,
	.forcetouch_fwupgrade_read = NULL,
	.forcetouch_vendor_read = NULL,
	.forcetouch_scover_write = NULL,
	.forcetouch_scover_read = NULL
};
#endif

int ft_probe_success = 0;
int ft_core_probe_success = 0;
char fih_forcetouch[32] = "unknown";

#if 0
//touch self test  start
static int fih_forcetouch_self_test_show(struct seq_file *m, void *v)
{
	if (forcetouch_cb.forcetouch_selftest_result != NULL)
	{
		pr_info("F@Touch Touch Selftest Result Read\n");
		seq_printf(m, "%d\n", forcetouch_cb.forcetouch_selftest_result());
	}
	return 0;
}

static int fih_forcetouch_self_test_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, fih_forcetouch_self_test_show, NULL);
};

static ssize_t fih_forcetouch_self_test_proc_write(struct file *file, const char __user *buffer,
	size_t count, loff_t *ppos)
{
	if(forcetouch_cb.forcetouch_selftest != NULL)
	{
	pr_info("F@Touch Do Touch Selftest\n");
		forcetouch_cb.forcetouch_selftest();
	}
	return count;
}
static struct file_operations forcetouch_self_test_proc_file_ops = {
	.owner   = THIS_MODULE,
	.write   = fih_forcetouch_self_test_proc_write,
	.open    = fih_forcetouch_self_test_proc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release
};
//touch self test  end
#endif

//forcetouch_fwver start
#if 0
static int fih_forcetouch_read_fwver_show(struct seq_file *m, void *v)
{
	char fwver[30]={0};

	if(forcetouch_cb.forcetouch_tpfwver_read != NULL)
	{
		pr_info("F@Touch Read Force Touch Firmware Version\n");
		forcetouch_cb.forcetouch_tpfwver_read(fwver);
		seq_printf(m, "%s", fwver);
	}
	return 0;
}

static int fih_forcetouch_fwver_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, fih_forcetouch_read_fwver_show, NULL);
};
#else
void fih_info_set_forcetouch(char *info)
{
	strcpy(fih_forcetouch, info);
}

static int fih_forcetouch_read_fwver_show(struct seq_file *m, void *v)
{
	pr_info("F@ForceTouch Read Force Touch Firmware Version\n");
	seq_printf(m, "%s\n", fih_forcetouch);
	return 0;
}

static int fih_forcetouch_fwver_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, fih_forcetouch_read_fwver_show, NULL);
};
#endif

static struct file_operations forceforcetouch_fwver_proc_file_ops = {
	.owner   = THIS_MODULE,
	.open    = fih_forcetouch_fwver_proc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release
};
//forcetouch_fwver end

#if 0
//forcetouch_fwimver start
static int fih_forcetouch_read_fwimver_show(struct seq_file *m, void *v)
{
	char fwimver[30]={0};
	if(forcetouch_cb.forcetouch_tpfwimver_read != NULL)
	{
		pr_info("F@Touch Read Touch Firmware Image Version\n");
		forcetouch_cb.forcetouch_tpfwimver_read(fwimver);
	seq_printf(m, "%s", fwimver);
	}
	return 0;
}

static int fih_forcetouch_fwimver_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, fih_forcetouch_read_fwimver_show, NULL);
};

static struct file_operations forcetouch_fwimver_proc_file_ops = {
	.owner   = THIS_MODULE,
	.open    = fih_forcetouch_fwimver_proc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release
};
//forcetouch_fwimver end

//forcetouch_scover start
static int fih_forcetouch_scover_read_show(struct seq_file *m, void *v)
{
	if (forcetouch_cb.forcetouch_selftest_result != NULL)
	{
		pr_info("F@Touch Read Touch Scover Result\n");
		seq_printf(m, "%d\n", forcetouch_cb.forcetouch_scover_read());
	}
	return 0;
}

static int fih_forcetouch_scover_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, fih_forcetouch_scover_read_show, NULL);
};

static ssize_t fih_forcetouch_scover_proc_write(struct file *file, const char __user *buffer,
	size_t count, loff_t *ppos)
{
	int input;
	if (sscanf(buffer, "%u", &input) != 1)
	{
		 return -EINVAL;
	}
	if(forcetouch_cb.forcetouch_scover_write != NULL)
	{
		pr_info("F@Touch Write Touch Scover Flag To %d\n",input);
		forcetouch_scover_write(input);
	}
	return count;
}

static struct file_operations forcetouch_scover_proc_file_ops = {
	.owner   = THIS_MODULE,
	.write   = fih_forcetouch_scover_proc_write,
	.open    = fih_forcetouch_scover_proc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release
};
//forcetouch_scover end

//forcetouch_upgrade start
static int fih_forcetouch_upgrade_read_show(struct seq_file *m, void *v)
{
	char upgrade_flag[10]={0};

	if(forcetouch_cb.forcetouch_fwupgrade_read != NULL)
	{
		pr_info("F@Touch Read Touch Upgrade Flag\n");
		forcetouch_cb.forcetouch_fwupgrade_read(upgrade_flag);
	seq_printf(m, "%s", upgrade_flag);
	}
	return 0;
}

static int fih_forcetouch_upgrade_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, fih_forcetouch_upgrade_read_show, NULL);
};

static ssize_t fih_forcetouch_upgrade_proc_write(struct file *file, const char __user *buffer,
	size_t count, loff_t *ppos)
{
	int input;

	if (sscanf(buffer, "%u", &input) != 1)
	{
		 return -EINVAL;
	}

	if(forcetouch_cb.forcetouch_fwupgrade != NULL)
	{
		pr_info("F@Touch Write Touch Upgrade(%d)\n", input);
		forcetouch_cb.forcetouch_fwupgrade(input);
	}
	return count;
}

static struct file_operations forcetouch_upgrade_proc_file_ops = {
	.owner   = THIS_MODULE,
	.write   = fih_forcetouch_upgrade_proc_write,
	.open    = fih_forcetouch_upgrade_proc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release
};
//forcetouch_upgrade end

//forcetouch_vendor start
static int fih_forcetouch_read_vendor_show(struct seq_file *m, void *v)
{
	char vendor[30]={0};
	if (forcetouch_cb.forcetouch_vendor_read != NULL)
	{
		pr_info("F@Touch Read Touch Vendor\n");
		forcetouch_cb.forcetouch_vendor_read(vendor);
	seq_printf(m, "%s", vendor);
	}
	return 0;
}

static int fih_forcetouch_vendor_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, fih_forcetouch_read_vendor_show, NULL);
};

static struct file_operations forcetouch_vendor_proc_file_ops = {
	.owner   = THIS_MODULE,
	.open    = fih_forcetouch_vendor_proc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release
};
//forcetouch_vendor end
//forcetouch_fwback start
static int fih_forcetouch_fwback_read_show(struct seq_file *m, void *v)
{
	if (forcetouch_cb.forcetouch_fwback_read != NULL)
	{
		pr_info("F@Touch Read Touch Firmware Flag\n");
		seq_printf(m, "%d\n", forcetouch_cb.forcetouch_fwback_read());
	}
	return 0;
}

static int fih_forcetouch_fwback_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, fih_forcetouch_fwback_read_show, NULL);
};

static ssize_t fih_forcetouch_fwback_proc_write(struct file *file, const char __user *buffer,
	size_t count, loff_t *ppos)
{
	if (forcetouch_cb.forcetouch_fwback_read != NULL)
	{
		pr_info("F@Touch Write Touch Firmware Flag To 1\n");
		forcetouch_cb.forcetouch_fwback_write();
	}
	return count;
}

static struct file_operations forcetouch_fwback_proc_file_ops = {
	.owner   = THIS_MODULE,
	.write   = fih_forcetouch_fwback_proc_write,
	.open    = fih_forcetouch_fwback_proc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release
};
//forcetouch_fwback end
#endif

static int __init fih_forcetouch_init(void)
{
	if(ft_probe_success && ft_core_probe_success)
	{
		pr_err("force touch driver ic probe success, create proc file\n");

		#if 0
		//F@Touch Self Test
		if (proc_create(FIH_PROC_TP_SELF_TEST, 0, NULL, &forcetouch_self_test_proc_file_ops) == NULL)
		{
			pr_err("fail to create proc/%s\n", FIH_PROC_TP_SELF_TEST);
			return (1);
		}
		#endif

		//F@Touch Read IC's firmware version
		if (proc_create(FIH_PROC_FT_IC_FW_VER, 0, NULL, &forceforcetouch_fwver_proc_file_ops) == NULL)
		{
			pr_err("fail to create proc/%s\n", FIH_PROC_FT_IC_FW_VER);
			return (1);
		}

		#if 0
		//F@Touch Read Touch firmware image version
		if (proc_create(FIH_PROC_TP_FILE_FW_FW, 0, NULL, &forcetouch_fwimver_proc_file_ops) == NULL)
		{
			pr_err("fail to create proc/%s\n", FIH_PROC_TP_FILE_FW_FW);
			return (1);
		}

		//F@Touch Firmware Upgrade
		if (proc_create(FIH_PROC_TP_UPGRADE, 0, NULL, &forcetouch_upgrade_proc_file_ops) == NULL)
		{
			pr_err("fail to create proc/%s\n", FIH_PROC_TP_UPGRADE);
			return (1);
		}

		//F@Touch Get Vendor name
		if (proc_create(FIH_PROC_TP_VENDOR, 0, NULL, &forcetouch_vendor_proc_file_ops) == NULL)
		{
			pr_err("fail to create proc/%s\n", FIH_PROC_TP_VENDOR);
			return (1);
		}

		//F@Touch Set Smart cover
		if (proc_create(FIH_PROC_TP_SMART_COVER, 0, NULL, &forcetouch_scover_proc_file_ops) == NULL)
		{
			pr_err("fail to create proc/%s\n", FIH_PROC_TP_SMART_COVER);
			return (1);
		}

		if (proc_create(FIH_PROC_TP_DOWN_GRADE, 0, NULL, &forcetouch_fwback_proc_file_ops) == NULL)
		{
			pr_err("fail to create proc/%s\n", FIH_PROC_TP_DOWN_GRADE);
			return (1);
		}
		#endif
	}
	else
	{
		pr_err("No panel probe, Fail to create proc file\n");
	}

	return (0);
}

static void __exit fih_forcetouch_exit(void)
{
	if(ft_probe_success && ft_core_probe_success)
	{
		#if 0
		//remove_proc_entry(FIH_PROC_TP_SELF_TEST, NULL);
		remove_proc_entry(FIH_PROC_TP_IC_FW_VER, NULL);
		remove_proc_entry(FIH_PROC_TP_FILE_FW_FW, NULL);
		remove_proc_entry(FIH_PROC_TP_UPGRADE, NULL);
		remove_proc_entry(FIH_PROC_TP_VENDOR, NULL);
		#endif
		remove_proc_entry(FIH_PROC_FT_IC_FW_VER, NULL);
	}
}

//module_init(fih_forcetouch_init);
late_initcall(fih_forcetouch_init);
module_exit(fih_forcetouch_exit);

