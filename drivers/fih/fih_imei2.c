#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include "fih_imei2.h"

#define FIH_PROC_DIR   "AllHWList"
#define FIH_PROC_IMEI2_PATH  "AllHWList/imei2"
#define FIH_PROC_SIZE  FIH_IMEI_SIZE

#define IMEI_SIZE	16
static char fih_proc_data[FIH_PROC_SIZE] = "0";

/*deviceinfo layout*/
#define DEVICEINFO_LOCATION "/dev/block/bootdevice/by-name/deviceinfo"
#define IMEI2_OFFSET (4096+4096+16+16) //E2P+CDA+BSET+IMEI1

/*imei2*/
/*Backup imei2 into deviceinfo partition*/
void fih_imei2_setup(char *info)
{
  struct file *fp = NULL;
  mm_segment_t oldfs = get_fs();

	memset(fih_proc_data, 0, sizeof(fih_proc_data));
	snprintf(fih_proc_data, sizeof(fih_proc_data), "%s", info);

 	set_fs(KERNEL_DS);
	fp = filp_open(DEVICEINFO_LOCATION, O_RDWR |O_NONBLOCK, 0);

	if (IS_ERR(fp)) {
		pr_err("[%s] Fail to open %s\n", __func__, DEVICEINFO_LOCATION);
		set_fs(oldfs);
		return;
	}

	fp->f_op->llseek(fp, IMEI2_OFFSET, SEEK_SET);
	fp->f_op->write(fp, (char *)fih_proc_data, IMEI_SIZE, &fp->f_pos);

	filp_close(fp, NULL);

}
ssize_t imei2_proc_write(struct file *file, const char  __user *buffer,
					size_t count, loff_t *data)
{
  char p_imei[IMEI_SIZE]={0};

	if(count <=2)
	{
		pr_info("input node is NULL \n");
		return -EFAULT;
	}

	if ( copy_from_user(p_imei, buffer, count) )
	{
		pr_info("copy_from_user fail\n");
		   return -EFAULT;
	}

	p_imei[IMEI_SIZE-1] = 0;
	fih_imei2_setup(p_imei);

	return count;
}
static int fih_proc_read_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", fih_proc_data);
	return 0;
}

static int imei2_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, fih_proc_read_show, NULL);
};

static struct file_operations imei2_file_ops = {
	.owner   = THIS_MODULE,
	.open    = imei2_proc_open,
	.read    = seq_read,
	.write   = imei2_proc_write,
	.llseek  = seq_lseek,
	.release = single_release
};

static int __init fih_proc_init(void)
{
	if (proc_create(FIH_PROC_IMEI2_PATH, 0644, NULL, &imei2_file_ops) == NULL) {
		proc_mkdir(FIH_PROC_DIR, NULL);
		if (proc_create(FIH_PROC_IMEI2_PATH, 0644, NULL, &imei2_file_ops) == NULL) {
			pr_err("fail to create proc/%s\n", FIH_PROC_IMEI2_PATH);
			return (1);
		}
	}
	return (0);
}

static void __exit fih_proc_exit(void)
{
	remove_proc_entry(FIH_PROC_IMEI2_PATH, NULL);
}

module_init(fih_proc_init);
module_exit(fih_proc_exit);
