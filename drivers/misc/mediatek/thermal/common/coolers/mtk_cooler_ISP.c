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
#include <linux/pm_qos.h>
#include <mmdvfs_pmqos.h>


/*=============================================================
 *Local variable definition
 *=============================================================
 */
static unsigned int clVR_ISP_debug_log;

static unsigned int cl_dev_VR_ISP_state;
static unsigned int cl_dev_VR_ISP_cur_state;
static struct thermal_cooling_device *cl_dev_VR_ISP;

/*=============================================================
 */
/*=============================================================
 *Macro definition
 *=============================================================
 */
#define CLVR_ISP_LOG_TAG	"[Thermal/CL/ISP]"

#define clVR_ISP_dprintk(fmt, args...)

#define clVR_ISP_printk(fmt, args...)


void __attribute__ ((weak))
mmdvfs_qos_limit_config(u32 pm_qos_class, u32 limit_value,
	enum mmdvfs_limit_source source)
{
	pr_notice("E_WF: %s doesn't exist\n", __func__);
}

/*
 * cooling device callback functions (clVR_FPS_cooling_VR_ISP_ops)
 * 1 : ON and 0 : OFF
 */
static int clVR_ISP_get_max_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = 1;
	clVR_ISP_dprintk("%s\n", __func__);

	return 0;
}

static int clVR_ISP_get_cur_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = cl_dev_VR_ISP_state;
	clVR_ISP_dprintk("%s %d, %d\n", __func__,
		cl_dev_VR_ISP_state, cl_dev_VR_ISP_cur_state);
	return 0;
}

static int clVR_ISP_set_cur_state(
struct thermal_cooling_device *cdev, unsigned long state)
{
	cl_dev_VR_ISP_state = state;

	clVR_ISP_dprintk("%s %d,%d\n", __func__,
		cl_dev_VR_ISP_state, cl_dev_VR_ISP_cur_state);

	if ((cl_dev_VR_ISP_state == 1) && (cl_dev_VR_ISP_cur_state == 0)) {
		clVR_ISP_dprintk("mtkclVR_ISP triggered\n");
		mmdvfs_qos_limit_config(PM_QOS_IMG_FREQ, 1,
			MMDVFS_LIMIT_THERMAL);
		cl_dev_VR_ISP_cur_state = 1;
	}

	if ((cl_dev_VR_ISP_state == 0) && (cl_dev_VR_ISP_cur_state == 1)) {
		clVR_ISP_dprintk("mtkclVR_ISP exited\n");
		mmdvfs_qos_limit_config(PM_QOS_IMG_FREQ, 0,
			MMDVFS_LIMIT_THERMAL);
		cl_dev_VR_ISP_cur_state = 0;
	}
	return 0;
}

static struct thermal_cooling_device_ops mtkclVR_ISP_ops = {
	.get_max_state = clVR_ISP_get_max_state,
	.get_cur_state = clVR_ISP_get_cur_state,
	.set_cur_state = clVR_ISP_set_cur_state,
};

static int __init mtk_cooler_VR_ISP_init(void)
{

	clVR_ISP_dprintk("%s %d\n", __func__, __LINE__);
	cl_dev_VR_ISP = mtk_thermal_cooling_device_register(
					"mtkclVR_ISP", NULL, &mtkclVR_ISP_ops);

	clVR_ISP_dprintk("%s %d\n", __func__, __LINE__);
	return 0;
}

static void __exit mtk_cooler_VR_ISP_exit(void)
{

	clVR_ISP_dprintk("%s %d\n", __func__, __LINE__);
	if (cl_dev_VR_ISP) {
		mtk_thermal_cooling_device_unregister(cl_dev_VR_ISP);
		cl_dev_VR_ISP = NULL;
	}

	clVR_ISP_dprintk("%s %d\n", __func__, __LINE__);
}
module_init(mtk_cooler_VR_ISP_init);
module_exit(mtk_cooler_VR_ISP_exit);
