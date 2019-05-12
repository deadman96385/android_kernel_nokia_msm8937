#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#define FIH_PROC_IMEI_PATH  "skuid"
#define FIH_PROC_SIZE  FIH_SKUID_SIZE

#define FIH_SKUID_SIZE	16
static char fih_proc_data[FIH_PROC_SIZE] = {0};

static int __init parse_fac_n_mode(char *line)
{
	strlcpy(fih_proc_data, line, sizeof(fih_proc_data));
	return 1;
}
__setup("androidboot.skuid=", parse_fac_n_mode);

ssize_t skuid_proc_write(struct file *file, const char  __user *buffer,
					size_t count, loff_t *data)
{
	return 0;
}

static int fih_proc_read_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", fih_proc_data);
	return 0;
}

static int skuid_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, fih_proc_read_show, NULL);
};

static struct file_operations skuid_file_ops = {
	.owner   = THIS_MODULE,
	.open    = skuid_proc_open,
	.read    = seq_read,
	.write   = skuid_proc_write,
	.llseek  = seq_lseek,
	.release = single_release
};

static int __init fih_proc_init(void)
{
	if (proc_create(FIH_PROC_IMEI_PATH, 0644, NULL, &skuid_file_ops) == NULL) {
		pr_err("fail to create proc/%s\n", FIH_PROC_IMEI_PATH);
		return (1);
	}
	return (0);
}

static void __exit fih_proc_exit(void)
{
	remove_proc_entry(FIH_PROC_IMEI_PATH, NULL);
}

module_init(fih_proc_init);
module_exit(fih_proc_exit);
