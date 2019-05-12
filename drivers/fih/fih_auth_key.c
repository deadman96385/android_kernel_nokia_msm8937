#include <linux/io.h>
#include <soc/qcom/restart.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include "fih_auth_key.h"

int read_ef(struct manuf_iqiyi_auth_key * rdata)
{
	struct file *fp = NULL;
	int len = 0;
	char *buf_manu = NULL;
	int fsize = 0;
	struct manuf_iqiyi_auth_key *read_data;

	read_data = rdata;
	
	fp = filp_open(KEY_FILE_LOCATION, O_RDWR|O_NONBLOCK|O_SYNC, 0644);
	if (IS_ERR(fp)) {
		pr_err("[%s] Fail to open %s\n", __func__, KEY_FILE_LOCATION);
		return FILE_NOT_FOUND;
	}

	fp->f_op->llseek(fp, KEY_AUTH_SHIFT, SEEK_SET);
	fsize = sizeof(struct manuf_iqiyi_auth_key);
	buf_manu = kzalloc(fsize, GFP_KERNEL);
	if (buf_manu == NULL) {
		pr_err("[%s] buf_manu fail\n", __func__);
		pr_err("Open skuid profile.xml failed. ERROR 2\n");
		filp_close(fp,NULL);
		return FILE_CORRUPTED;
	}
	
	len = fp->f_op->read(fp, buf_manu, sizeof(struct manuf_iqiyi_auth_key), &fp->f_pos);
	
	if (len != sizeof(struct manuf_iqiyi_auth_key))
	{
		kfree(buf_manu);
		return FILE_CORRUPTED;
	}
	filp_close(fp, NULL);

	memcpy(&read_data->name[0], buf_manu, len);
	kfree(buf_manu);
	printk("read %d byte from file\r\n", len);

	return OK;
}

int write_ef(struct manuf_iqiyi_auth_key * wdata)
{
	struct file *fp = NULL;
	struct manuf_iqiyi_auth_key *write_data;

	printk("write_ef\n");
	write_data = wdata;
	
	fp = filp_open(KEY_FILE_LOCATION, O_RDWR |S_IRWXU, 0644);

	if (IS_ERR(fp)) {
		pr_err("[%s] Fail to open %s\n", __func__, KEY_FILE_LOCATION);
		return FILE_NOT_FOUND;
	}
	
	fp->f_op->llseek(fp, KEY_AUTH_SHIFT, SEEK_SET);
	fp->f_op->write(fp, (char *)write_data, sizeof(struct manuf_iqiyi_auth_key), &fp->f_pos);

	filp_close(fp, NULL);
	
	return OK;
}

int fih_read_iqiyi_auth_key(char* auth_key, int *key_length)
{
	//struct manuf_data manuf_data;
	struct manuf_iqiyi_auth_key auth_data;
	int access;

	printk("fih_read_iqiyi_auth_key\n");

	memset(&auth_data, '\0', sizeof(struct manuf_iqiyi_auth_key) ); //clean buffer

	/* read manuf data from file system*/
	access = read_ef(&auth_data);
	if ((FILE_NOT_FOUND == access) || (FILE_CORRUPTED == access))
	{
		printk("Read failed\n");
		return READ_MANUFACTURE_FAIL;
	}

	/* check auth key name */
	if(strcmp(auth_data.name, IQIYI_AUTU_KEY_STRING) == 0 && auth_data.length <= IQIYI_AUTU_KEY_LEN)
	{
		memcpy(auth_key, auth_data.iqiyi_auth_key, auth_data.length);
		*key_length = auth_data.length;
		printk("fih_read_iqiyi_auth_key length = %d\n", auth_data.length);
	}
	else
	{
		printk("auth key tag (0x%s 0x%x)mismatch)\n", auth_data.name, auth_data.length);
		return FAILED;
	}
	return OK;
}

int fih_write_iqiyi_auth_key(char* auth_key, int key_length)
{
	//struct manuf_data manuf_data;
	struct manuf_iqiyi_auth_key auth_data;
	int access;

	memset(&auth_data, '\0', sizeof(struct manuf_iqiyi_auth_key)); //clean buffer

	/* read manuf data from file system*/
	access = read_ef(&auth_data);
	if ((FILE_NOT_FOUND == access) || (FILE_CORRUPTED == access))
	{
		printk("Read failed\n");
		return READ_MANUFACTURE_FAIL;
	}

	//copy testflag and write to flash
	/* clear product_id and copy written string */
	memset(auth_data.name, 0x0, MANUF_NAME_LEN);
	memset(auth_data.iqiyi_auth_key, 0x0, IQIYI_AUTU_KEY_LEN);
	memcpy(auth_data.name, IQIYI_AUTU_KEY_STRING, strlen(IQIYI_AUTU_KEY_STRING));
	memcpy(auth_data.iqiyi_auth_key, auth_key, key_length);
	auth_data.length = key_length;//strlen(auth_key);

	printk("fih_write_iqiyi_auth_key length = %d\n", auth_data.length);

	access = write_ef(&auth_data);
	if ((FILE_NOT_FOUND == access) || (FILE_CORRUPTED == access))
	{
		printk("Write failed\n");
		return WRITE_MANUFACTURE_FAIL;
	}
	return OK;
}

void hex2str(char hex, char str[2])
{
     char temp1, temp2;
     temp1 = (hex >> 4) & 0xF;
     temp2 = hex & 0x0F;

     //printk("temp1 = %d temp2 = %d\n", temp1, temp2);

     if (temp1 >= 0 && temp1 <= 9)
         temp1 = temp1 + '0';
     else if (temp1 >= 0xa && temp1 <= 0xf)
        temp1 = (temp1 - 10) + 'a';

    if (temp2 >= 0 && temp2 <= 9)
         temp2 = temp2 + '0';
    else if (temp2 >= 0xa && temp2 <= 0xf)
        temp2 = (temp2 - 10) + 'a';
    
	str[0] = temp1;
    str[1] = temp2;

    //printk("temp1 = %d temp2 = %d\n", temp1, temp2);
}
