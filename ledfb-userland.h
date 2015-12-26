#ifndef _LEDFB_USERLAND_H_
#define _LEDFB_USERLAND_H_

int ledfb_user_init(void);
void ledfb_user_exit(void);

int ledfb_user_open(struct inode *inode, struct file *fil);
ssize_t ledfb_user_read(struct file *filp, char *buf, size_t len, loff_t *off);
ssize_t ledfb_user_write(struct file *filp, const char *buff, size_t len, loff_t *off);
int ledfb_user_close(struct inode *inod, struct file *filp);

#endif