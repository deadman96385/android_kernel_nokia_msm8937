#ifndef __FIH_AUTH_KEY_H
#define __FIH_AUTH_KEY_H

#define KEY_FILE_LOCATION "/dev/block/bootdevice/by-name/deviceinfo"

#define READ_MANUFACTURE_FAIL  (-1)
#define WRITE_MANUFACTURE_FAIL  (-2)

#define OK			0
#define FAILED				1
#define FILE_NOT_FOUND 		2
#define FILE_CORRUPTED  		3


#define MANUF_NAME_LEN 32
#define IQIYI_AUTU_KEY_LEN 512
#define MANUF_RESERVED_LEN 32
#define IQIYI_AUTU_KEY_STRING  "IQIYI_AUTU_KEY"

#define KEY_AUTH_SHIFT			((9)*(1024))

struct manuf_iqiyi_auth_key
{
	char name[MANUF_NAME_LEN];
	unsigned int length;
	char iqiyi_auth_key[IQIYI_AUTU_KEY_LEN];
	char reserved[MANUF_RESERVED_LEN];
};

int fih_write_iqiyi_auth_key(char* auth_key, int key_length);
int fih_read_iqiyi_auth_key(char* auth_key, int *key_length);
void hex2str(char hex, char str[2]);

#endif /* __FIH_RERE_H */
