/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kobject.h>
#include <linux/uaccess.h>
#include <linux/err.h>
#include <linux/syscalls.h>
#include "mt-plat/mtk_thermal_monitor.h"

static int cl_debug_flag;

#define mtk_cooler_3Gmutt_dprintk_always(fmt, args...)


#define mtk_cooler_3Gmutt_dprintk(fmt, args...)

#define MAX_NUM_INSTANCE_MTK_COOLER_3GMUTT  4

#define MTK_CL_3GMUTT_GET_LIMIT(limit, state) \
{ (limit) = (short) (((unsigned long) (state))>>16); }

#define MTK_CL_3GMUTT_SET_LIMIT(limit, state) \
{ state = ((((unsigned long) (state))&0xFFFF) | ((short) limit<<16)); }

#define MTK_CL_3GMUTT_GET_CURR_STATE(curr_state, state) \
{ curr_state = (((unsigned long) (state))&0xFFFF); }

#define MTK_CL_3GMUTT_SET_CURR_STATE(curr_state, state) \
	do { \
		if (curr_state == 0) \
		state &= ~0x1; \
		else \
		state |= 0x1; \
	} while (0)

static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);
static int cl_mutt_klog_on;
static struct thermal_cooling_device
		*cl_mutt_dev[MAX_NUM_INSTANCE_MTK_COOLER_3GMUTT] = { 0 };

static unsigned int cl_mutt_param[MAX_NUM_INSTANCE_MTK_COOLER_3GMUTT] = { 0 };
static unsigned long cl_mutt_state[MAX_NUM_INSTANCE_MTK_COOLER_3GMUTT] = { 0 };

static unsigned int cl_mutt_cur_limit;

static unsigned long last_md_boot_cnt;

static unsigned int tm_pid;
static unsigned int tm_input_pid_3Gtest;
static struct task_struct *pg_task;

static int mutt3G_send_signal(int level, int level2)
{
	int ret = 0;
	int thro = level;
	int thro2 = level2;
	/* g_limit_tput = level; */
	mtk_cooler_3Gmutt_dprintk_always("%s +++ ,level=%d,level2=%d\n",
						__func__, level, level2);

	if (tm_input_pid_3Gtest == 0) {
		mtk_cooler_3Gmutt_dprintk_always("[%s] pid is empty\n",
								__func__);
		ret = -1;
	}

	mtk_cooler_3Gmutt_dprintk_always("[%s] pid is %d, %d, %d, %d\n",
							__func__, tm_pid,
							tm_input_pid_3Gtest,
							thro, thro2);

	if (ret == 0 && tm_input_pid_3Gtest != tm_pid) {
		tm_pid = tm_input_pid_3Gtest;

		if (pg_task != NULL)
			put_task_struct(pg_task);
		pg_task = get_pid_task(find_vpid(tm_pid), PIDTYPE_PID);
		mtk_cooler_3Gmutt_dprintk_always("[%s] line %d\n", __func__,
								__LINE__);
	}

	if (ret == 0 && pg_task) {
		siginfo_t info;

		info.si_signo = SIGIO;
		info.si_errno = 4;
		info.si_code = thro;
		info.si_addr = NULL;
		ret = send_sig_info(SIGIO, &info, pg_task);
	}

	if (ret != 0)
		mtk_cooler_3Gmutt_dprintk_always("[%s] ret=%d\n", __func__,
									ret);

	return ret;
}


static void mtk_cl_mutt_set_mutt_limit(void)
{
	/* TODO: optimize */
	int i = 0, ret = 0;
	int min_limit = 255;
	unsigned int min_param = 0;
	unsigned int md_active = 0;
	unsigned int md_suspend = 0;
	unsigned int md_final = 0;
	int limit = 0;

	for (; i < MAX_NUM_INSTANCE_MTK_COOLER_3GMUTT; i++) {
		unsigned long curr_state;

		MTK_CL_3GMUTT_GET_CURR_STATE(curr_state, cl_mutt_state[i]);
		mtk_cooler_3Gmutt_dprintk_always("[%s] curr_state = %lu\n",
							__func__, curr_state);

		if (curr_state == 1) {

			md_active = (cl_mutt_param[i] & 0x0000FF00) >> 8;
			md_suspend = (cl_mutt_param[i] & 0x00FF0000) >> 16;

			md_final = (cl_mutt_param[i] & 0x000FFFF0) >> 4;

			/* mtk_cooler_3Gmutt_dprintk_always("[%s]i= %d ,
			 * active=0x%x,suspend=0x%x\n",
			 *	__func__, i,md_active,md_suspend);
			 */

			/* a cooler with 0 active or 0 suspend is not allowed */
			if (md_active == 0 || md_suspend == 0)
				goto err_unreg;

			/* compare the active/suspend ratio */
			if (md_active >= md_suspend)
				limit = md_active / md_suspend;
			else
				limit = (0 - md_suspend) / md_active;

			if (limit <= min_limit) {
				min_limit = limit;
				min_param = cl_mutt_param[i];
			}
		} else {
			/* mutt3G_send_signal(-1,-1);*/
		}
	}
	mtk_cooler_3Gmutt_dprintk_always(
			"[%s]i= %d ,min_param=%x,cl_mutt_cur_limit=%x\n",
			__func__, i, min_param, cl_mutt_cur_limit);

	mtk_cooler_3Gmutt_dprintk_always(
			"[%s]i= %d ,md_final=0x%x,active=0x%x,suspend=0x%x\n",
			__func__, i, md_final, md_active, md_suspend);

	if (min_param != cl_mutt_cur_limit) {
		cl_mutt_cur_limit = min_param;
		/* last_md_boot_cnt = ccci_get_md_boot_count(MD_SYS1);
		 */
		/* ret = exec_ccci_kern_func_by_md_id(MD_SYS1,
		 * ID_THROTTLING_CFG, (char*) &cl_mutt_cur_limit, 4);
		 */
		mutt3G_send_signal(md_final, cl_mutt_cur_limit);
		mtk_cooler_3Gmutt_dprintk_always(
				"[%s] ret %d param %x bcnt %lul\n", __func__,
				ret, cl_mutt_cur_limit, last_md_boot_cnt);
	}

err_unreg:
	return;

}

static int mtk_cl_mutt_get_max_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = 1;
	mtk_cooler_3Gmutt_dprintk("mtk_cl_mutt_get_max_state() %s %lu\n",
							cdev->type, *state);
	return 0;
}

static int mtk_cl_mutt_get_cur_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	MTK_CL_3GMUTT_GET_CURR_STATE(*state, *((unsigned long *)cdev->devdata));
	mtk_cooler_3Gmutt_dprintk("mtk_cl_mutt_get_cur_state() %s %lu\n",
							cdev->type, *state);

	return 0;
}

static int mtk_cl_mutt_set_cur_state(
struct thermal_cooling_device *cdev, unsigned long state)
{
	mtk_cooler_3Gmutt_dprintk("mtk_cl_mutt_set_cur_state() %s %lu pid=%d\n",
					cdev->type, state, tm_input_pid_3Gtest);

	MTK_CL_3GMUTT_SET_CURR_STATE(state, *((unsigned long *)cdev->devdata));
	mtk_cl_mutt_set_mutt_limit();

	return 0;
}

/* bind fan callbacks to fan device */
static struct thermal_cooling_device_ops mtk_cl_mutt_ops = {
	.get_max_state = mtk_cl_mutt_get_max_state,
	.get_cur_state = mtk_cl_mutt_get_cur_state,
	.set_cur_state = mtk_cl_mutt_set_cur_state,
};

static int mtk_cooler_mutt_register_ltf(void)
{
	int i;

	mtk_cooler_3Gmutt_dprintk("register ltf\n");

	for (i = MAX_NUM_INSTANCE_MTK_COOLER_3GMUTT; i-- > 0;) {
		char temp[20] = { 0 };

		sprintf(temp, "mtk-cl-3gmutt%02d", i);
		cl_mutt_dev[i] = mtk_thermal_cooling_device_register(temp,
				/* put mutt state to cooler devdata */
				(void *)&cl_mutt_state[i],
				&mtk_cl_mutt_ops);
	}
	mtk_cooler_3Gmutt_dprintk_always("[%s]i= %d\n", __func__, i);
	return 0;
}

static void mtk_cooler_mutt_unregister_ltf(void)
{
	int i;

	mtk_cooler_3Gmutt_dprintk("unregister ltf\n");

	for (i = MAX_NUM_INSTANCE_MTK_COOLER_3GMUTT; i-- > 0;) {
		if (cl_mutt_dev[i]) {
			mtk_thermal_cooling_device_unregister(cl_mutt_dev[i]);
			cl_mutt_dev[i] = NULL;
			cl_mutt_state[i] = 0;
		}
	}
	mtk_cooler_3Gmutt_dprintk_always("[%s]i= %d\n", __func__, i);
}

static int __init mtk_cooler_mutt_init(void)
{
	int err = 0;
	int i;

	for (i = MAX_NUM_INSTANCE_MTK_COOLER_3GMUTT; i-- > 0;) {
		cl_mutt_dev[i] = NULL;
		cl_mutt_state[i] = 0;
	}

	mtk_cooler_3Gmutt_dprintk("init\n");

	err = mtk_cooler_mutt_register_ltf();
	if (err)
		goto err_unreg;

	return 0;

err_unreg:
	mtk_cooler_mutt_unregister_ltf();
	return err;
}

static void __exit mtk_cooler_mutt_exit(void)
{
	mtk_cooler_3Gmutt_dprintk("exit\n");

	mtk_cooler_mutt_unregister_ltf();
}
module_init(mtk_cooler_mutt_init);
module_exit(mtk_cooler_mutt_exit);
