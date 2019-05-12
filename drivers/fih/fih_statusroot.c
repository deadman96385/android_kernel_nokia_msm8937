#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
//#include <mach/msm_iomap.h>
#include <linux/slab.h>

#ifdef CONFIG_DIAG_OVER_USB
#include <linux/usb/usbdiag.h>
#endif

#define PROC_STATUSROOT "statusroot"
#define statusroot_size 4
#define STATUSROOT_LOCATION "/BBSYS/status.cfg"

char *efs_statusroot_buf;
int  efs_statusroot_len;
int copy_statusroot_done=0;

struct delayed_work statusroot_readinfo_work;
struct workqueue_struct *statusroot_readinfo_workq;

static void statusroot_readinfo_work_func(struct work_struct *work)
{
	struct file *fp = NULL;
	int output_size = 0;
	mm_segment_t oldfs;
	static int redo = 0;

	if (redo == 10) {
		printk("Open systeminfo partition fail!\n");
		return;
	}

	oldfs = get_fs();
	pr_info("fih_statusroot efs_open: ready to open statusroot\n");
	set_fs(KERNEL_DS);

	fp = filp_open(STATUSROOT_LOCATION,O_RDONLY|O_NONBLOCK, 0);
	if (IS_ERR(fp)){
		printk("[%s] Fail to open systeminfo partition!\n", __func__);
		fp=NULL;
		set_fs(oldfs);
		queue_delayed_work(statusroot_readinfo_workq, &statusroot_readinfo_work,
				msecs_to_jiffies(2000));
		redo++;
		return ;
	}

	if (!copy_statusroot_done){
		output_size = fp->f_op->read(fp, (unsigned char __user *)efs_statusroot_buf, statusroot_size, &fp->f_pos);
		filp_close(fp,NULL);
		copy_statusroot_done=1;
		printk("[efs_seq_show] Read File size=%d\n",output_size);
		set_fs(oldfs);
	}
}

/* This function is called at the beginning of a sequence. ie, when:
 * 1.the /proc file is read (first time)
 * 2.after the function stop (end of sequence) */
static void *efs_seq_start(struct seq_file *s, loff_t *pos)
{
	if (((*pos)*PAGE_SIZE) >= efs_statusroot_len) return NULL;
	return (void *)((unsigned long) *pos+1);
}

/* This function is called after the beginning of a sequence.
 * It's called untill the return is NULL (this ends the sequence). */
static void *efs_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	++*pos;
	return efs_seq_start(s, pos);
}

/* This function is called at the end of a sequence */
static void efs_seq_stop(struct seq_file *s, void *v)
{
	/* nothing to do, we use a static value in start() */
}

/* This function is called for each "step" of a sequence */
static int efs_seq_show(struct seq_file *s, void *v)
{
/*	int n = (int)v-1;*/
	long n = (long)v-1;
	struct file *fp= NULL;
	int output_size=0;
	mm_segment_t oldfs;

	oldfs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(STATUSROOT_LOCATION,O_RDONLY|O_NONBLOCK, 0);
	if (!IS_ERR(fp)){
		memset(efs_statusroot_buf, 0x00, statusroot_size);
		output_size = fp->f_op->read(fp, (unsigned char __user *)efs_statusroot_buf, statusroot_size, &fp->f_pos);
		filp_close(fp,NULL);
		printk("[efs_seq_show] Read File size=%d\n",output_size);
	}

	set_fs(oldfs);

	if (efs_statusroot_len < (PAGE_SIZE*(n+1))) {
		seq_write(s, (efs_statusroot_buf+(PAGE_SIZE*n)), (efs_statusroot_len - (PAGE_SIZE*n)));
	} else {
		seq_write(s, (efs_statusroot_buf+(PAGE_SIZE*n)), PAGE_SIZE);
	}

	return 0;
}

/* This structure gather "function" to manage the sequence */
static struct seq_operations efs_seq_ops = {
	.start = efs_seq_start,
	.next = efs_seq_next,
	.stop = efs_seq_stop,
	.show = efs_seq_show
};

/* This function is called when the /proc file is open. */
static int efs_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &efs_seq_ops);
};

/* This structure gather "function" that manage the /proc file */
static struct file_operations efs_file_ops = {
	.owner = THIS_MODULE,
	.open = efs_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release
};

/* This function is called when the module is loaded */
static int __init efs_statusroot_init(void)
{
	int i;

	if(strstr(saved_command_line,"androidboot.mode=2")!=NULL){ //for FTM mode
		pr_info("[%s] ftm, not probe\n", __func__);
		return 0;
	}

	/* get the total length of statusroot in byte */
	efs_statusroot_len = statusroot_size;

	/* check statusroot */
	if (0 >= efs_statusroot_len) {
		printk("fih_statusroot: [ERROR] efs_statusroot_len = %d\n", efs_statusroot_len);
		return (-1);
	}
	pr_info("fih_statusroot: [INFO] efs_statusroot_len = %d\n", efs_statusroot_len);

	/* alloc statusroot */
	i = (efs_statusroot_len + PAGE_SIZE) - (efs_statusroot_len & (PAGE_SIZE - 1));
	pr_info("fih_statusroot: [INFO] kzalloc = %d\n", i);
	efs_statusroot_buf = kmalloc(i, GFP_KERNEL);
	if (NULL == efs_statusroot_buf) {
		printk("fih_statusroot: [ERROR] efs_statusroot_buf is NULL\n");
		return (-1);
	}
	memset(efs_statusroot_buf, 0x00, (unsigned int) sizeof(efs_statusroot_buf));

	/* read statusroot */
	/* move to efs_seq_show*/
	/* create statusroot */
	if (proc_create(PROC_STATUSROOT, 0, NULL, &efs_file_ops) == NULL) {
		pr_err("fail to create proc/%s\n", PROC_STATUSROOT);
	}

	statusroot_readinfo_workq = create_singlethread_workqueue("statusroot_readinfo");
	if (!statusroot_readinfo_workq)
		return -ENOMEM;

	INIT_DELAYED_WORK(&statusroot_readinfo_work, statusroot_readinfo_work_func);

	queue_delayed_work(statusroot_readinfo_workq, &statusroot_readinfo_work,msecs_to_jiffies(30000));


	return (0);
}

/* This function is called when the module is unloaded. */
static void __exit efs_statusroot_exit(void)
{
	if (efs_statusroot_buf) kfree(efs_statusroot_buf);
	remove_proc_entry(PROC_STATUSROOT, NULL);
	cancel_delayed_work_sync(&statusroot_readinfo_work);
}

late_initcall(efs_statusroot_init);
module_exit(efs_statusroot_exit);
