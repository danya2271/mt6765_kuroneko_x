/*
 * Copyright (C) 2018 MediaTek Inc.
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
#include <linux/types.h>
#include <linux/kobject.h>
#include "mt-plat/mtk_thermal_monitor.h"


#include <linux/version.h>
#include <linux/uaccess.h>
#include <linux/pid.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/uidgid.h>

#define MAX_LEN	256

#if 1
#define mtk_thermal_policy_dprintk(fmt, args...)
#else
#define mtk_thermal_policy_dprintk(fmt, args...)
#endif

#define TM_CLIENT_chgpolicy 4


static unsigned int tm_pid;
static unsigned int tm_input_pid;
static struct task_struct *pg_task;

static int _mtk_cl_sd_send_signal(int val)
{
	int ret = 0;

	if (tm_input_pid == 0) {
		mtk_thermal_policy_dprintk("%s pid is empty\n", __func__);
		ret = -1;
	}

	mtk_thermal_policy_dprintk("%s pid is %d, %d, 0x%x\n", __func__,
			tm_pid, tm_input_pid, val);

	if (ret == 0 && tm_input_pid != tm_pid) {
		tm_pid = tm_input_pid;

		if (pg_task != NULL)
			put_task_struct(pg_task);
		pg_task = get_pid_task(find_vpid(tm_pid), PIDTYPE_PID);
	}

	if (ret == 0 && pg_task) {
		siginfo_t info;

		info.si_signo = SIGIO;
		info.si_errno = TM_CLIENT_chgpolicy;
		info.si_code = val;
		info.si_addr = NULL;
		ret = send_sig_info(SIGIO, &info, pg_task);
	}

	if (ret != 0)
		mtk_thermal_policy_dprintk("%s ret=%d\n", __func__, ret);

	return ret;
}


int mtk_change_thermal_policy(int tp_index, int onoff)
{
	int ret = 0;
	int mix_val = 0;

	mix_val = (onoff << 8) | tp_index;

	mtk_thermal_policy_dprintk("%s tp_index=%d, onoff=%d, mix_val=0x%3x\n",
		__func__, tp_index, onoff, mix_val);

	ret = _mtk_cl_sd_send_signal(mix_val);

	return ret;
}

static int __init mtk_thermal_policy_init(void)
{
	mtk_thermal_policy_dprintk("init\n");
	return 0;
}

static void __exit mtk_thermal_policy_exit(void)
{
	mtk_thermal_policy_dprintk("exit\n");
}
module_init(mtk_thermal_policy_init);
module_exit(mtk_thermal_policy_exit);
