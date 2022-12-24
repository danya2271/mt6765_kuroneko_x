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
#define CLVR_FPS_LOG_TAG	"[Cooler_VR_FPS]"

#define clVR_FPS_dprintk(fmt, args...)  
#define clVR_FPS_printk(fmt, args...)
/*=============================================================
 *Local variable definition
 *=============================================================
 */
static unsigned int clVR_FPS_debug_log;

static unsigned int cl_dev_VR_FPS_state;
static struct thermal_cooling_device *cl_dev_VR_FPS;

/*
 * cooling device callback functions (clVR_FPS_cooling_VR_FPS_ops)
 * 1 : ON and 0 : OFF
 */
static int clVR_FPS_get_max_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = 1;

	return 0;
}

static int clVR_FPS_get_cur_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = cl_dev_VR_FPS_state;

	return 0;
}

static int clVR_FPS_set_cur_state(
struct thermal_cooling_device *cdev, unsigned long state)
{
	cl_dev_VR_FPS_state = state;

	if (cl_dev_VR_FPS_state == 1)
		clVR_FPS_dprintk("mtkclVR_FPS triggered\n");
	else
		clVR_FPS_dprintk("mtkclVR_FPS exited\n");

	return 0;
}

static struct thermal_cooling_device_ops mtkclVR_FPS_ops = {
	.get_max_state = clVR_FPS_get_max_state,
	.get_cur_state = clVR_FPS_get_cur_state,
	.set_cur_state = clVR_FPS_set_cur_state,
};

static int __init mtk_cooler_VR_FPS_init(void)
{
	clVR_FPS_dprintk("%s %d\n", __func__, __LINE__);
	cl_dev_VR_FPS = mtk_thermal_cooling_device_register(
					"mtkclVR_FPS", NULL, &mtkclVR_FPS_ops);

	clVR_FPS_dprintk("%s %d\n", __func__, __LINE__);
	return 0;
}

static void __exit mtk_cooler_VR_FPS_exit(void)
{

	clVR_FPS_dprintk("%s %d\n", __func__, __LINE__);
	if (cl_dev_VR_FPS) {
		mtk_thermal_cooling_device_unregister(cl_dev_VR_FPS);
		cl_dev_VR_FPS = NULL;
	}

	clVR_FPS_dprintk("%s %d\n", __func__, __LINE__);
}
module_init(mtk_cooler_VR_FPS_init);
module_exit(mtk_cooler_VR_FPS_exit);
