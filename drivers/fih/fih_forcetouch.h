#ifndef __FIH_FORCETOUCH_H
#define __FIH_FORCETOUCH_H

void fih_info_set_forcetouch(char *info);

#if 0
struct fih_forcetouch_cb {
	void (*forcetouch_selftest)(void);
	int (*forcetouch_selftest_result)(void);
	void (*forcetouch_tpfwver_read)(char *);
	void (*forcetouch_tpfwimver_read)(char *);
	void (*forcetouch_fwupgrade)(int);
	void (*forcetouch_fwupgrade_read)(char *);
	void (*forcetouch_vendor_read)(char *);
	void (*forcetouch_scover_write)(int);
	int (*forcetouch_scover_read)(void);
	int (*forcetouch_fwback_read)(void);
	void (*forcetouch_fwback_write)(void);
};
#endif

#endif /* __FIH_FORCETOUCH_H */

