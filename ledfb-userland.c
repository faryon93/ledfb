#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/fb.h>

#include "ledfb-userland.h"


// ==========================================================================================
// global variables
// ==========================================================================================

static struct file_operations ledfb_user_fops =
{
	.open = ledfb_user_open,
	.read = ledfb_user_read,
	.write = ledfb_user_write,
	.release = ledfb_user_close,
};

static struct miscdevice ledfb_user_misc =
{
	MISC_DYNAMIC_MINOR,	// automatically assign a minor
    "ledfb-user",		// name for the device file
    &ledfb_user_fops
};

static struct fb_info *g_fbi;


// ==========================================================================================
// initialisation / uninitialization
// ==========================================================================================

int ledfb_user_init(void)
{
	if (misc_register(&ledfb_user_misc))
	{
		printk(KERN_ALERT "failed to register userland device\n");
		return -1;
	}

	return 0;
}

void ledfb_user_exit(void)
{
	misc_deregister(&ledfb_user_misc);
}


// ==========================================================================================
// device implementation
// ==========================================================================================

int ledfb_user_open(struct inode *inode, struct file *fil)
{
	module_put(THIS_MODULE);
	printk("ledfb-user: openend\n");
	return 0;
}

ssize_t ledfb_user_read(struct file *filp, char *buf, size_t len, loff_t *off)
{
	if (g_fbi == NULL || g_fbi->screen_base == NULL)
		return -1;

	copy_to_user(buf, g_fbi->screen_base, len);
	return len;
}

ssize_t ledfb_user_write(struct file *filp, const char *buff, size_t len, loff_t *off)
{
	return len;
}

int ledfb_user_close(struct inode *inod, struct file *filp)
{
	if (!try_module_get(THIS_MODULE))
		return -1;

	printk("ledfb-user: closed\n");
	return 0;
}