#include <linux/module.h>
#include <linux/fb.h>
#include <linux/console.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>


// ----------------------------------------------------------------------------------
//  constants
// ----------------------------------------------------------------------------------

#define VIRT_FB_NAME	"ledfb"
#define VIRT_FB_ID		0

#define BITS_PER_PIXEL 	24


// ----------------------------------------------------------------------------------
//  local variables
// ----------------------------------------------------------------------------------

struct fb_info *g_fbi = NULL;
int xres = 4 * 32;
int yres = 3 * 32;


// ----------------------------------------------------------------------------------
//  local function
// ----------------------------------------------------------------------------------

/**
 * Allocates the needed memory for the framebuffer in ram.
 * @param fbi framebuffer information pointer
 * @return error code indicating success or failure
 */
static int virtfb_map_video_memory(struct fb_info *fbi)
{   
	fbi->screen_size = fbi->var.xres * fbi->var.yres * (fbi->var.bits_per_pixel / 8);
    fbi->fix.smem_len = fbi->screen_size;
    fbi->fix.smem_start = 0;

	// allocate the virtual memory
	fbi->screen_base = vmalloc_user(fbi->fix.smem_len);
	if (!fbi->screen_base)
	{
		printk("ledfb: falied to allocate vram\n");
		return -ENOMEM;
	}
	
	printk("ledfb: allocated %d bytes of framebuffer memory at %p\n", fbi->var.xres * fbi->var.yres * (fbi->var.bits_per_pixel / 8), fbi->screen_base);

	return 0;
}

/**
 * De-allocates the memory for the frame buffer.
 * @param fbi framebuffer information pointer
 * @return error code indicating success or failure
 */
static int virtfb_unmap_video_memory(struct fb_info *fbi)
{
	fbi->screen_base = NULL;
	fbi->fix.smem_len = 0;
	vfree(fbi->screen_base);

	return 0;
}


// ----------------------------------------------------------------------------------
//  framebuffer implementation
// ----------------------------------------------------------------------------------

/**
 * Set fixed framebuffer parameters based on variable settings.
 * @param info framebuffer information pointer
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

/**
 * Set framebuffer parameters and change the operating mode.
 * @param info framebuffer information pointer
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

/**
 * Check framebuffer variable parameters and adjust to valid values.
 * @param var framebuffer variable parameters
 * @param info framebuffer information pointer
 */
static int virtfb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	// TODO: do we really want this to be modfied? i think not...
	if (var->xres_virtual < var->xres)
		var->xres_virtual = var->xres;

	if (var->yres_virtual < var->yres)
		var->yres_virtual = var->yres;

	// our led matrix is just 24bpp, nothing else
	var->bits_per_pixel = 24;
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

	// dont know why that...
	var->height = -1;
	var->width = -1;
	var->grayscale = 0;

	return 0;
}

/**
 * Pan or Wrap the Display
 * This call looks only at xoffset, yoffset and the FB_VMODE_YWRAP flag
 * @param var Variable screen buffer information
 * @param info Framebuffer information pointer
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

/**
 * Function to handle custom mmap for virtual framebuffer.
 * @param fbi framebuffer information pointer
 * @param vma Pointer to vm_area_struct
 */
static int virtfb_mmap(struct fb_info *fbi, struct vm_area_struct *vma)
{
	int err;

    printk("ledfb: remapping %lu bytes at %p\n", vma->vm_end - vma->vm_start, fbi->screen_base);
	err = remap_vmalloc_range(vma, fbi->screen_base, 0);
	if (0 != err)
	{
		printk("ledfb: failed to remap vram: %d!\n", err);
		return -ENOBUFS;
	}
	
	printk("ledfb: successfully remapped vram\n");

	return 0;
}


// ----------------------------------------------------------------------------------
//  framebuffer initialistation
// ----------------------------------------------------------------------------------

/**
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

/**
 * Initializes the framebuffer information pointer. After allocating
 * sufficient memory for the framebuffer structure, the fields are
 * filled with custom information passed in from the configurable
 * structures.  This includes information such as bits per pixel,
 * color maps, screen width/height and RGBA offsets.
 * @return Framebuffer structure initialized with our information
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
	fbi->var.xres_virtual = fbi->var.xres = xres;
	fbi->var.yres_virtual = fbi->var.yres = yres;
	fbi->var.bits_per_pixel = BITS_PER_PIXEL;
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


// ----------------------------------------------------------------------------------
//  module entry
// ----------------------------------------------------------------------------------

/**
 * Main entry function for the framebuffer.
 * @return error code indicating success or failure
 */
int __init ledfb_init(void)
{
	int ret;    

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

	printk("ledfb: Successfully initialized ledfb\n");

	return 0;

fail:
	if(g_fbi)
	{
		unregister_framebuffer(g_fbi);
    	framebuffer_release(g_fbi);
	}

	printk(KERN_ALERT "failed to initialize ledfb driver\n");

	return ret;
}

/**
 * When the module gets unloaded we deregister the framebuffer.
 */
void ledfb_exit(void)
{
	// destroy the freamebuffer device
	unregister_framebuffer(g_fbi);
	virtfb_unmap_video_memory(g_fbi);
	framebuffer_release(g_fbi);
}


// ----------------------------------------------------------------------------------
//  module decleration
// ----------------------------------------------------------------------------------

MODULE_AUTHOR("Maximilian Pachl");
MODULE_DESCRIPTION("framebuffer driver for led matrix");
MODULE_LICENSE("GPL");
MODULE_SUPPORTED_DEVICE("fb");

module_init(ledfb_init);
module_exit(ledfb_exit);

module_param(xres, int, 0);
module_param(yres, int, 0);
