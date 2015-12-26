/*
 * Copyright 2004-2013 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

/*!
 * @defgroup Framebuffer Framebuffer Driver for surface sharing.
 */

/*!
 * @file virtual_fb.c 
 *
 * @brief Virtual Frame buffer driver for surface sharing
 *
 * @ingroup Framebuffer
 */

/*!
 * Include files
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/dma-mapping.h>
#include <linux/clk.h>
#include <linux/console.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>

/*
 * Driver name
 */
#define VIRT_FB_NAME	"ledfb"
#define VIRT_FB_ID		0

static struct fb_info *g_fbi = NULL;

static int virtfb_map_video_memory(struct fb_info *fbi);
static int virtfb_unmap_video_memory(struct fb_info *fbi);

/*
 * Set fixed framebuffer parameters based on variable settings.
 * @param       info     framebuffer information pointer
 */
static int virtfb_set_fix(struct fb_info *info)
{
	struct fb_fix_screeninfo *fix = &info->fix;
	struct fb_var_screeninfo *var = &info->var;

	fix->line_length = var->xres_virtual * var->bits_per_pixel / 8;

	fix->type = FB_TYPE_PACKED_PIXELS;
	fix->accel = FB_ACCEL_NONE;
	fix->visual = FB_VISUAL_TRUECOLOR;
	fix->xpanstep = 1;
	fix->ywrapstep = 1;
	fix->ypanstep = 1;

	return 0;
}


/*
 * Set framebuffer parameters and change the operating mode.
 * @param       info     framebuffer information pointer
 */
static int virtfb_set_par(struct fb_info *fbi)
{
	int retval = 0;
	u32 mem_len;

	dev_dbg(fbi->device, "Reconfiguring framebuffer\n");

	virtfb_set_fix(fbi);

	mem_len = fbi->var.yres_virtual * fbi->fix.line_length;

	if (!fbi->screen_base || (mem_len > fbi->fix.smem_len)) {
		if (fbi->screen_base)
			virtfb_unmap_video_memory(fbi);

		if (virtfb_map_video_memory(fbi) < 0)
			return -ENOMEM;
	}


	return retval;
}


/*
 * Check framebuffer variable parameters and adjust to valid values.
 * @param       var      framebuffer variable parameters
 * @param       info     framebuffer information pointer
 */
static int virtfb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{

	/* fg should not bigger than bg */

	/*if (var->xres_virtual < var->xres)
		var->xres_virtual = var->xres;

	if (var->yres_virtual < var->yres)
		var->yres_virtual = var->yres;

	if ((var->bits_per_pixel != 32) && (var->bits_per_pixel != 24) &&
	    (var->bits_per_pixel != 16) && (var->bits_per_pixel != 12) &&
	    (var->bits_per_pixel != 8))
		var->bits_per_pixel = 16;

	switch (var->bits_per_pixel) {
	case 8:
		var->red.length = 3;
		var->red.offset = 5;
		var->red.msb_right = 0;

		var->green.length = 3;
		var->green.offset = 2;
		var->green.msb_right = 0;

		var->blue.length = 2;
		var->blue.offset = 0;
		var->blue.msb_right = 0;

		var->transp.length = 0;
		var->transp.offset = 0;
		var->transp.msb_right = 0;
		break;
	case 16:
		var->red.length = 5;
		var->red.offset = 11;
		var->red.msb_right = 0;

		var->green.length = 6;
		var->green.offset = 5;
		var->green.msb_right = 0;

		var->blue.length = 5;
		var->blue.offset = 0;
		var->blue.msb_right = 0;

		var->transp.length = 0;
		var->transp.offset = 0;
		var->transp.msb_right = 0;
		break;
	case 24:
		var->red.length = 8;
		var->red.offset = 16;
		var->red.msb_right = 0;

		var->green.length = 8;
		var->green.offset = 8;
		var->green.msb_right = 0;

		var->blue.length = 8;
		var->blue.offset = 0;
		var->blue.msb_right = 0;

		var->transp.length = 0;
		var->transp.offset = 0;
		var->transp.msb_right = 0;
		break;
	case 32:
		var->red.length = 8;
		var->red.offset = 16;
		var->red.msb_right = 0;

		var->green.length = 8;
		var->green.offset = 8;
		var->green.msb_right = 0;

		var->blue.length = 8;
		var->blue.offset = 0;
		var->blue.msb_right = 0;

		var->transp.length = 8;
		var->transp.offset = 24;
		var->transp.msb_right = 0;
		break;
	}*/

	var->height = -1;
	var->width = -1;
	var->grayscale = 0;

	return 0;
}

/*
 * Pan or Wrap the Display
 * This call looks only at xoffset, yoffset and the FB_VMODE_YWRAP flag
 * @param               var     Variable screen buffer information
 * @param               info    Framebuffer information pointer
 */
static int virtfb_pan_display(struct fb_var_screeninfo *var, struct fb_info *info)
{

	if (info->var.yoffset == var->yoffset)
		return 0;	/* No change, do nothing */

	if ((var->yoffset + info->var.yres) > info->var.yres_virtual)
		return -EINVAL;

	info->var.yoffset = var->yoffset;

	return 0;
}

/*
 * Function to handle custom mmap for virtual framebuffer.
 * @param       fbi     framebuffer information pointer
 * @param       vma     Pointer to vm_area_struct
 */
static int virtfb_mmap(struct fb_info *fbi, struct vm_area_struct *vma)
{
	int err;

	err = remap_vmalloc_range(vma, fbi->screen_base, 0);
	if (0 != err)
	{
		printk("failed to remap vram: %d!\n", err);
		return -ENOBUFS;
	}
	
	printk("successfully remapped vram\n");

	return 0;
}

/*!
 * This structure contains the pointers to the control functions that are
 * invoked by the core framebuffer driver to perform operations like
 * blitting, rectangle filling, copy regions and cursor definition.
 */
static struct fb_ops virtfb_ops = {
	.owner = THIS_MODULE,
	.fb_set_par = virtfb_set_par,
	.fb_check_var = virtfb_check_var,
	.fb_pan_display = virtfb_pan_display,
	.fb_mmap = virtfb_mmap,
};


/*!
 * Allocates the DRAM memory for the frame buffer.      This buffer is remapped
 * into a non-cached, non-buffered, memory region to allow palette and pixel
 * writes to occur without flushing the cache.  Once this area is remapped,
 * all virtual memory access to the video memory should occur at the new region.
 * @param       fbi     framebuffer information pointer
 * @return      Error code indicating success or failure
 */
static int virtfb_map_video_memory(struct fb_info *fbi)
{
	if (fbi->fix.smem_len < fbi->var.yres_virtual * fbi->fix.line_length)
		fbi->fix.smem_len = fbi->var.yres_virtual * fbi->fix.line_length;

	fbi->screen_size = fbi->fix.smem_len;
	fbi->fix.smem_start = 0;

	// allocate the virtual memory
	fbi->screen_base = vmalloc_user(fbi->var.xres * fbi->var.yres * fbi->var.bits_per_pixel/8);
	if (!fbi->screen_base)
	{
		printk("ledfb: falied to allocate vram\n");
		return -ENOMEM;
	}

	return 0;
}

/*!
 * De-allocates the DRAM memory for the frame buffer.
 * @param       fbi     framebuffer information pointer
 * @return      Error code indicating success or failure
 */
static int virtfb_unmap_video_memory(struct fb_info *fbi)
{
	fbi->screen_base = NULL;
	fbi->fix.smem_len = 0;
	vfree(fbi->screen_base);

	return 0;
}

/*!
 * Initializes the framebuffer information pointer. After allocating
 * sufficient memory for the framebuffer structure, the fields are
 * filled with custom information passed in from the configurable
 * structures.  This includes information such as bits per pixel,
 * color maps, screen width/height and RGBA offsets.
 * @return      Framebuffer structure initialized with our information
 */
static struct fb_info *virtfb_init_fbinfo(struct fb_ops *ops)
{
	struct fb_info *fbi;

	// Allocate sufficient memory for the fb structure
	fbi = framebuffer_alloc(sizeof(unsigned int), NULL);
	if (!fbi)
		return NULL;

	fbi->var.activate = FB_ACTIVATE_NOW;
	fbi->fbops = ops;
	fbi->flags = FBINFO_FLAG_DEFAULT;

	return fbi;
}

static int virtfb_register(struct fb_info *fbi, unsigned int id)
{
	struct fb_videomode m;
	int ret = 0;

	//TODO: Set framebuffer ID
	sprintf(fbi->fix.id, "virt_fb%d", id);

	//Setup small default resolution
	fbi->var.xres_virtual = fbi->var.xres = fbi->var.yres_virtual = fbi->var.yres  = 32;
	fbi->var.bits_per_pixel = 32;
	fbi->screen_base = 0;

	virtfb_check_var(&fbi->var, fbi);
	virtfb_set_fix(fbi);

	/*added first mode to fbi modelist*/
	if (!fbi->modelist.next || !fbi->modelist.prev)
		INIT_LIST_HEAD(&fbi->modelist);
	fb_var_to_videomode(&m, &fbi->var);
	fb_add_videomode(&m, &fbi->modelist);

	fbi->var.activate |= FB_ACTIVATE_FORCE;
	console_lock();
	fbi->flags |= FBINFO_MISC_USEREVENT;
	ret = fb_set_var(fbi, &fbi->var);
	fbi->flags &= ~FBINFO_MISC_USEREVENT;
	console_unlock();

	return register_framebuffer(fbi);
}

static void virtfb_unregister(struct fb_info *fbi)
{

	unregister_framebuffer(fbi);
}

static int dev_open(struct inode *inode, struct file *fil)
{
	module_put(THIS_MODULE);
	printk("ledfb-user: openend\n");
	return 0;
}

static ssize_t dev_read(struct file *filp, char *buf, size_t len, loff_t *off)
{
	if (g_fbi == NULL || g_fbi->screen_base == NULL)
		return -1;

	copy_to_user(buf, g_fbi->screen_base, len);
	return len;
}

static ssize_t dev_write(struct file *filp, const char *buff, size_t len, loff_t *off)
{
	return len;
}

static int dev_rls(struct inode *inod, struct file *filp)
{
	if (!try_module_get(THIS_MODULE))
		return -1;

	printk("ledfb-user: closed\n");
	return 0;
}

static struct file_operations fops =
{
	.read = dev_read,
	.open = dev_open,
	.write = dev_write,
	.release = dev_rls,
};

static struct miscdevice userland_dev = {
        MISC_DYNAMIC_MINOR,
        "ledfb-user",
        &fops
};

int userland_init(void)
{
	if (misc_register(&userland_dev))
	{
		printk(KERN_ALERT "failed to register userland device\n");
		return -1;
	}

	return 0;
}

void userland_destroy(void)
{
	misc_deregister(&userland_dev);
}


/*!
 * Main entry function for the framebuffer. The function registers the power
 * management callback functions with the kernel and also registers the MXCFB
 * callback functions with the core Linux framebuffer driver \b fbmem.c
 * @return      Error code indicating success or failure
 */
int __init ledfb_init(void)
{
	int ret;    

	// initialize the userland interface
    ret = userland_init();
    if (ret != 0)
    	goto fail;

	// initialize the framebuffer structure
    g_fbi = virtfb_init_fbinfo(&virtfb_ops);
    if (!g_fbi)
    {
            ret = -ENOMEM;
            goto fail;
    }
    *((u32*)g_fbi->par) = VIRT_FB_ID;

    // register the framebuffer
    ret = virtfb_register(g_fbi, VIRT_FB_ID);
    if (ret < 0)
		goto fail;

	return 0;

fail:
	if(g_fbi)
	{
		virtfb_unregister(g_fbi);
    	framebuffer_release(g_fbi);
	}

	return ret;
}

void ledfb_exit(void)
{
	// destroy the userland device
	userland_destroy();

	// destroy the freamebuffer device
	virtfb_unregister(g_fbi);
	virtfb_unmap_video_memory(g_fbi);
	framebuffer_release(g_fbi);
}


module_init(ledfb_init);
module_exit(ledfb_exit);

MODULE_AUTHOR("Maximilian Pachl");
MODULE_DESCRIPTION("framebuffer driver for led matrix");
MODULE_LICENSE("GPL");
MODULE_SUPPORTED_DEVICE("fb");
