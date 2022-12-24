/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <mt-plat/aee.h>
#include <linux/types.h>
#include "mt-plat/mtk_thermal_monitor.h"
#if defined(CONFIG_MTK_CLKMGR)
#include <mach/mtk_clkmgr.h>
#else
#include <linux/clk.h>
#endif
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/uaccess.h>

/*=============================================================
 *Macro definition
 *=============================================================
 */
#define CLNR_LOG_TAG	"[Cooler_NR]"

#define clNR_dprintk(fmt, args...)

#define clNR_printk(fmt, args...) 
/*=============================================================
 *Local variable definition
 *=============================================================
 */
static unsigned int clNR_debug_log = 0;

static unsigned int cl_dev_NR_state;
static struct thermal_cooling_device *cl_dev_NR;
static char *clNR_mmap;
/*=============================================================
 */

static int mmap_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct page *page;
	char *info;

	clNR_dprintk("%s %d\n", __func__, __LINE__);
	/* the data is in vma->vm_private_data */
	info = (char *)vma->vm_private_data;

	if (!info) {
		clNR_printk("no data\n");
		return -1;
	}

	/* get the page */
	page = virt_to_page(info);

	get_page(page);
	vmf->page = page;
	clNR_dprintk("%s %d\n", __func__, __LINE__);
	return 0;
}

static const struct vm_operations_struct clNR_mmap_vm_ops = {
	.fault =   mmap_fault,
};

static int clNR_status_mmap(struct file *file, struct vm_area_struct *vma)
{
	clNR_dprintk("%s %d\n", __func__, __LINE__);
	vma->vm_ops = &clNR_mmap_vm_ops;
	vma->vm_flags |= VM_IO;
	/* assign the file private data to the vm private data */
	vma->vm_private_data = clNR_mmap;
	clNR_dprintk("%s %d\n", __func__, __LINE__);
	return 0;
}

/*
 * cooling device callback functions (clNR_cooling_NR_ops)
 * 1 : ON and 0 : OFF
 */
static int clNR_get_max_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = 1;
	return 0;
}

static int clNR_get_cur_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = cl_dev_NR_state;
	return 0;
}

static int clNR_set_cur_state(
struct thermal_cooling_device *cdev, unsigned long state)
{
	cl_dev_NR_state = state;

	if (cl_dev_NR_state == 1) {
		clNR_dprintk("mtkclNR triggered\n");
		*(unsigned int *)(clNR_mmap + 0x00) = 0x1;
	} else {
		clNR_dprintk("mtkclNR exited\n");
		*(unsigned int *)(clNR_mmap + 0x00) = 0x0;
	}

	return 0;
}
static struct thermal_cooling_device_ops mtkclNR_ops = {
	.get_max_state = clNR_get_max_state,
	.get_cur_state = clNR_get_cur_state,
	.set_cur_state = clNR_set_cur_state,
};

static int __init mtk_cooler_NR_init(void)
{
	clNR_dprintk("%s %d\n", __func__, __LINE__);
	cl_dev_NR = mtk_thermal_cooling_device_register("mtkclNR", NULL,
								&mtkclNR_ops);

	clNR_mmap = (char *)get_zeroed_page(GFP_KERNEL);
	*(unsigned int *)(clNR_mmap + 0x00) = 0x0;

	clNR_dprintk("%s %d\n", __func__, __LINE__);
	return 0;
}

static void __exit mtk_cooler_NR_exit(void)
{
	clNR_dprintk("%s %d\n", __func__, __LINE__);
	if (cl_dev_NR) {
		mtk_thermal_cooling_device_unregister(cl_dev_NR);
		cl_dev_NR = NULL;
	}

	free_page((unsigned long) clNR_mmap);
	clNR_dprintk("%s %d\n", __func__, __LINE__);
}
module_init(mtk_cooler_NR_init);
module_exit(mtk_cooler_NR_exit);
