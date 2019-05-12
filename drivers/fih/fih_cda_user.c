#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/types.h>
#include <linux/unistd.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <fih/share/cda.h>

#define FIH_PROC_DIR   "cda"
#define FIH_PROC_PATH  "cda/user"

#define MY_FILE_PATH  MANUF_FILE_LOCATION
#define MY_FILE_OFFSET  4096
#define MY_FILE_ACCESS  0644
#define MY_BUFFER_SIZE  16

/* used for function */
#define FIH_CDA_KERN_USER  (0)
#define FIH_CDA_KERN_ROOT  (1)

/* used for manufacture */
#define FIH_CDA_STAT_USER  (0)
#define FIH_CDA_STAT_ROOT  (1)

static int cda_user_read_show(struct seq_file *m, void *v)
{
	struct file *fp= NULL;
	mm_segment_t oldfs;
	int len;
	char *path = MY_FILE_PATH;
	unsigned int offset = MY_FILE_OFFSET;
	struct manuf_data *buf;
	unsigned int val;

	oldfs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(path, O_RDONLY|O_NONBLOCK, 0);
	if (IS_ERR(fp)) {
		pr_err("%s: open fail %s\n", __func__, path);
		set_fs(oldfs);
		return -EFAULT;
	}

	if (fp->f_op == NULL) {
		pr_err("%s: fp->f_op is NULL\n", __func__);
		set_fs(oldfs);
		return -EFAULT;
	}

	buf = kmalloc(sizeof(struct manuf_data), GFP_KERNEL);
	fp->f_op->llseek(fp, offset, 0);
	len = fp->f_op->read(fp, (unsigned char __user *)buf, sizeof(struct manuf_data), &fp->f_pos);

	filp_close(fp, NULL);
	set_fs(oldfs);

	pr_info("%s: status = 0x%08x\n", __func__, buf->rootflag.status);
	switch (buf->rootflag.status) {
		case FIH_CDA_STAT_USER:
			val = FIH_CDA_KERN_USER;
			break;
		case FIH_CDA_STAT_ROOT:
			val = FIH_CDA_KERN_ROOT;
			break;
		default:
			val = FIH_CDA_KERN_ROOT;
			break;
	}

	kfree(buf);

	seq_printf(m, "%d\n", val);
	return 0;
}

ssize_t cda_user_proc_write(struct file *file, const char  __user *buffer,
					size_t count, loff_t *data)
{
	struct file *fp= NULL;
	mm_segment_t oldfs;
	char tmp[MY_BUFFER_SIZE] = {0};
	int len = MY_BUFFER_SIZE;
	char *path = MY_FILE_PATH;
	unsigned int offset = MY_FILE_OFFSET;
	struct manuf_data *buf;
	unsigned int val;

	if (count < len) len = count;
	if ( copy_from_user(tmp, buffer, len) ) {
		return -EFAULT;
	}
	pr_info("%s: tmp = (%s)\n", __func__, tmp);

	oldfs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(path, O_RDONLY|O_NONBLOCK, 0);
	if (IS_ERR(fp)) {
		pr_err("%s: open fail %s\n", __func__, path);
		set_fs(oldfs);
		return -EFAULT;
	}

	if (fp->f_op == NULL) {
		pr_err("%s: fp->f_op is NULL\n", __func__);
		set_fs(oldfs);
		return -EFAULT;
	}

	buf = kmalloc(sizeof(struct manuf_data), GFP_KERNEL);
	fp->f_op->llseek(fp, offset, 0);
	len = fp->f_op->read(fp, (unsigned char __user *)buf, sizeof(struct manuf_data), &fp->f_pos);
	pr_info("%s: status = 0x%08x (old)\n", __func__, buf->rootflag.status);

	val = simple_strtoull(tmp, NULL, len);
	switch (val) {
		case FIH_CDA_KERN_USER:
			buf->rootflag.status = FIH_CDA_STAT_USER;
			break;
		case FIH_CDA_KERN_ROOT:
			buf->rootflag.status = FIH_CDA_STAT_ROOT;
			break;
		default:
			buf->rootflag.status = FIH_CDA_STAT_ROOT;
			break;
	}

	pr_info("%s: status = 0x%08x (new)\n", __func__, buf->rootflag.status);
	fp->f_op->llseek(fp, offset, 0);
	len = fp->f_op->write(fp, (unsigned char __user *)buf, sizeof(struct manuf_data), &fp->f_pos);

	filp_close(fp, NULL);
	set_fs(oldfs);
	kfree(buf);

	return count;
}


static int cda_user_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, cda_user_read_show, NULL);
};

static struct file_operations cda_user_file_ops = {
	.owner   = THIS_MODULE,
	.open    = cda_user_proc_open,
	.read    = seq_read,
	.write   = cda_user_proc_write,
	.llseek  = seq_lseek,
	.release = single_release
};

static int __init fih_cda_user_init(void)
{
	if (proc_create(FIH_PROC_PATH, 0, NULL, &cda_user_file_ops) == NULL) {
		pr_err("fail to create proc/%s\n", FIH_PROC_PATH);
		return (1);
	}
	return (0);
}

static void __exit fih_cda_user_exit(void)
{
	remove_proc_entry(FIH_PROC_PATH, NULL);
}

module_init(fih_cda_user_init);
module_exit(fih_cda_user_exit);
