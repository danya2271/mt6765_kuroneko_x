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

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt)

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/kobject.h>
#include "mt-plat/mtk_thermal_monitor.h"

#define MAX_NUM_INSTANCE_MTK_COOLER_SHUTDOWN  3

/* #define MTK_COOLER_SHUTDOWN_UEVENT */
#define MTK_COOLER_SHUTDOWN_SIGNAL

#if defined(MTK_COOLER_SHUTDOWN_SIGNAL)
#include <linux/version.h>
#include <linux/uaccess.h>
#include <linux/pid.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/uidgid.h>

#define MAX_LEN	256
#endif

#if 1
#define mtk_cooler_shutdown_dprintk(fmt, args...)
#else
#define mtk_cooler_shutdown_dprintk(fmt, args...)
#endif

struct sd_state {
	unsigned long state;
	int sd_cnt;
};

static struct thermal_cooling_device
*cl_shutdown_dev[MAX_NUM_INSTANCE_MTK_COOLER_SHUTDOWN] = { 0 };

/* static unsigned long cl_shutdown_state[MAX_NUM_INSTANCE_MTK_COOLER_SHUTDOWN]
 * = { 0 };
 */

static struct sd_state cl_sd_state[MAX_NUM_INSTANCE_MTK_COOLER_SHUTDOWN];

#if defined(MTK_COOLER_SHUTDOWN_SIGNAL)

static unsigned int tm_pid;
static unsigned int tm_input_pid;
static struct task_struct *pg_task;

static int sd_debouncet = 1;
/* static int sd_cnt = 0; */
static int sd_happened;

static int _mtk_cl_sd_send_signal(void)
{
	int ret = 0;

	if (tm_input_pid == 0) {
		mtk_cooler_shutdown_dprintk("%s pid is empty\n", __func__);
		ret = -1;
	}

	mtk_cooler_shutdown_dprintk("%s pid is %d, %d\n", __func__,
			tm_pid, tm_input_pid);

	if (ret == 0 && tm_input_pid != tm_pid) {
		tm_pid = tm_input_pid;

		if (pg_task != NULL)
			put_task_struct(pg_task);
		pg_task = get_pid_task(find_vpid(tm_pid), PIDTYPE_PID);
	}

	if (ret == 0 && pg_task) {
		siginfo_t info;

		info.si_signo = SIGIO;
		info.si_errno = 0;
		info.si_code = 1;
		info.si_addr = NULL;
		ret = send_sig_info(SIGIO, &info, pg_task);
	}

	if (ret != 0)
		mtk_cooler_shutdown_dprintk("%s ret=%d\n", __func__, ret);

	return ret;
}

#endif

	static int mtk_cl_shutdown_get_max_state
(struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = 1;
	/* mtk_cooler_shutdown_dprintk(
	 * "mtk_cl_shutdown_get_max_state() %s %d\n",
	 * cdev->type, *state);
	 */

	return 0;
}

	static int mtk_cl_shutdown_get_cur_state
(struct thermal_cooling_device *cdev, unsigned long *state)
{
	/* *state = *((unsigned long *)cdev->devdata); */
	struct sd_state *cl_state = (struct sd_state *) cdev->devdata;

	if (state)
		*state = cl_state->state;
	/* mtk_cooler_shutdown_dprintk(
	 * "mtk_cl_shutdown_get_cur_state() %s %d\n",
	 * cdev->type, *state);
	 */
	return 0;
}

	static int mtk_cl_shutdown_set_cur_state
(struct thermal_cooling_device *cdev, unsigned long state)
{
	struct sd_state *cl_state = (struct sd_state *) cdev->devdata;
#if defined(MTK_COOLER_SHUTDOWN_SIGNAL)
	unsigned long original_state;
#endif
	/* mtk_cooler_shutdown_dprintk(
	 * "mtk_cl_shutdown_set_cur_state() %s %d\n",
	 * cdev->type, state);
	 */
	if (!cl_state)
		return -1;

#if defined(MTK_COOLER_SHUTDOWN_SIGNAL)
	original_state = cl_state->state;
#endif

	cl_state->state = state;

	if (state == 0) {
		if (cl_state->sd_cnt > 0)
			cl_state->sd_cnt--;
	} else if (state == 1) {
		cl_state->sd_cnt++;
	}

	if (sd_debouncet == cl_state->sd_cnt) {
#if defined(MTK_COOLER_SHUTDOWN_UEVENT)
		{
			/* send uevent to notify current call must be dropped */
			char event[11] = "SHUTDOWN=1";
			char *envp[2] = { event, NULL };

			kobject_uevent_env(&(cdev->device.kobj),
					KOBJ_CHANGE, envp);
		}
#endif

#if defined(MTK_COOLER_SHUTDOWN_SIGNAL)
		/* make this an edge trigger instead of level trigger */
		if (sd_happened == 0) {
			/* send signal to target process */
			_mtk_cl_sd_send_signal();
			sd_happened = 1;
		}
#endif
	}

	return 0;
}

/* bind fan callbacks to fan device */
static struct thermal_cooling_device_ops mtk_cl_shutdown_ops = {
	.get_max_state = mtk_cl_shutdown_get_max_state,
	.get_cur_state = mtk_cl_shutdown_get_cur_state,
	.set_cur_state = mtk_cl_shutdown_set_cur_state,
};

static int mtk_cooler_shutdown_register_ltf(void)
{
	int i;

	mtk_cooler_shutdown_dprintk("register ltf\n");

	for (i = MAX_NUM_INSTANCE_MTK_COOLER_SHUTDOWN; i-- > 0;) {
		char temp[20] = { 0 };

		sprintf(temp, "mtk-cl-shutdown%02d", i);
		cl_shutdown_dev[i] = mtk_thermal_cooling_device_register(
				temp, (void *)&cl_sd_state[i],
				&mtk_cl_shutdown_ops);
	}

	return 0;
}

static void mtk_cooler_shutdown_unregister_ltf(void)
{
	int i;

	mtk_cooler_shutdown_dprintk("unregister ltf\n");

	for (i = MAX_NUM_INSTANCE_MTK_COOLER_SHUTDOWN; i-- > 0;) {
		if (cl_shutdown_dev[i]) {
			mtk_thermal_cooling_device_unregister(
					cl_shutdown_dev[i]);
			cl_shutdown_dev[i] = NULL;
			cl_sd_state[i].state = 0;
			cl_sd_state[i].sd_cnt = 0;
		}
	}
}


static int __init mtk_cooler_shutdown_init(void)
{
	int err = 0;
	int i;

	for (i = MAX_NUM_INSTANCE_MTK_COOLER_SHUTDOWN; i-- > 0;) {
		cl_shutdown_dev[i] = NULL;
		cl_sd_state[i].state = 0;
		cl_sd_state[i].sd_cnt = 0;
	}

	mtk_cooler_shutdown_dprintk("init\n");

	err = mtk_cooler_shutdown_register_ltf();
	if (err)
		goto err_unreg;

	return 0;

err_unreg:
	mtk_cooler_shutdown_unregister_ltf();
	return err;
}

static void __exit mtk_cooler_shutdown_exit(void)
{
	mtk_cooler_shutdown_dprintk("exit\n");
	mtk_cooler_shutdown_unregister_ltf();
}
module_init(mtk_cooler_shutdown_init);
module_exit(mtk_cooler_shutdown_exit);
