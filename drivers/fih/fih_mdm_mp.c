#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <soc/qcom/scm.h>
#include "fih_ramtable.h"
#include "fih_mdm_mp.h"

struct secure_memprot_cmd {
	u32	start;
	u32	size;
	u32	tag_id;
	u8	lock;
} __attribute__ ((__packed__));

#define TZBSP_MEM_MPU_USAGE_MSS_AP_DYNAMIC_REGION	0x6
#define SCM_MP_LOCK_CMD_ID      0x1
#define SCM_MP_PROTECT          0x1

static unsigned char modem_rf_nv_unlock_status = false;

static int nv_ram_do_scm_protect(phys_addr_t phy_base, unsigned long size)
{
	struct secure_memprot_cmd cmd;

	cmd.start = phy_base;
	cmd.size = size;
	cmd.tag_id = TZBSP_MEM_MPU_USAGE_MSS_AP_DYNAMIC_REGION;
	cmd.lock = SCM_MP_PROTECT;

	pr_info("%s: phy_base: 0x%x, size: 0x%x\n", __func__, (unsigned int)phy_base, (unsigned int)size);
	return scm_call(SCM_SVC_MP, SCM_MP_LOCK_CMD_ID, &cmd,
			sizeof(cmd), NULL, 0);
}
static int proc_comm_nv_backup_mp_unlock(void)
{
	return nv_ram_do_scm_protect(FIH_MODEM_RF_NV_BASE, (FIH_MODEM_RF_NV_SIZE+FIH_MODEM_CUST_NV_SIZE));
}

static int mdm_mp_dev_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int mdm_mp_dev_release(struct inode *inode, struct file *file)
{
	return 0;
}

static long mdm_mp_dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	pr_info("%s: cmd = 0x%x\n", __func__, cmd);

	if((IOCTL_NV_BACKUP_MP_UNLOCK == cmd) && true == modem_rf_nv_unlock_status) {
		pr_info("%s: IOCTL_NV_BACKUP_MP_UNLOCK already unlock\n", __func__);
		return 0;
	}

	switch(cmd)
	{
		case IOCTL_NV_BACKUP_MP_UNLOCK:
			pr_info("%s: IOCTL_NV_BACKUP_MP_UNLOCK\n", __func__);
			if(proc_comm_nv_backup_mp_unlock() != 0) {
				pr_err("%s: proc_comm_nv_backup_mp_unlock fail\n", __func__);
				return -1;
			}

			modem_rf_nv_unlock_status = true;
			pr_info("%s: IOCTL_NV_BACKUP_MP_UNLOCK PASS\n", __func__);
			break;
		default:
			pr_err("%s: Undefined FTM GPIO driver IOCTL command\n", __func__);
			return -1;
	}

	return 0;
}

/* This structure gather "function" that manage the /proc file */
static struct file_operations mdm_mp_dev_fops = {
	.owner   = THIS_MODULE,
	.open = mdm_mp_dev_open,
	.release = mdm_mp_dev_release,
	.unlocked_ioctl = mdm_mp_dev_ioctl,
};

static struct miscdevice mdm_mp_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "mdm_mp",
	.fops = &mdm_mp_dev_fops
};

/* This function is called when the module is loaded */
static int __init my_module_init(void)
{
	int ret;

	ret = misc_register(&mdm_mp_dev);

	if(ret) {
		pr_err("fail to register mdm_mp_dev\n");
		return ret;
	}

	return 0;
}

/* This function is called when the module is unloaded. */
static void __exit my_module_exit(void)
{
	misc_deregister(&mdm_mp_dev);
}

module_init(my_module_init);
module_exit(my_module_exit);
