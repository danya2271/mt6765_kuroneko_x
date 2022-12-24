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
#include <linux/types.h>
#include <linux/kobject.h>
#include <linux/uaccess.h>
#include <linux/err.h>
#include <linux/syscalls.h>
#include <linux/timer.h>
#include <mtk_ccci_common.h>
#include "mt-plat/mtk_thermal_monitor.h"
#include <linux/uidgid.h>

int __attribute__ ((weak))
exec_ccci_kern_func_by_md_id(
int md_id, unsigned int id, char *buf, unsigned int len)
{
	pr_notice("E_WF: %s doesn't exist\n", __func__);
	return -316;
}

#define cl_type_upper               "cl-amutt-u"
#define cl_type_lower               "cl-amutt-l"

#define mtk_cooler_amutt_dprintk_always(fmt, args...)

#define mtk_cooler_amutt_dprintk(fmt, args...)

static int cl_amutt_klog_on;

/* over_up_time * polling interval > up_duration --> throttling */
static unsigned int over_up_time;	/* polling time */
static unsigned int up_duration = 30;	/* sec */
static unsigned int up_step = 1;	/* step */

/* below_low_time * polling interval > low_duration --> throttling */
static unsigned int below_low_time;	/* polling time */
static unsigned int low_duration = 10;	/* sec */
static unsigned int low_step = 1;	/* step */

static unsigned int low_rst_time;
static unsigned int low_rst_max = 3;

/* static unsigned int deepest_step = 0; */

static int polling_interval = 1;	/* second */

#define UNK_STAT -1
#define LOW_STAT 0
#define MID_STAT 1
#define HIGH_STAT 2

#define MAX_LEN	256
#define COOLER_STEPS 5

static unsigned int cl_upper_dev_state;
static unsigned int cl_lower_dev_state;

static struct thermal_cooling_device *cl_upper_dev;
static struct thermal_cooling_device *cl_lower_dev;

typedef int (*activate_cooler_opp_func) (int level);

static activate_cooler_opp_func opp_func[COOLER_STEPS] = { 0 };

static unsigned int amutt_param[COOLER_STEPS] = { 0 };

struct adaptive_coolers {
	int cur_level;
	int max_level;
	activate_cooler_opp_func *opp_func_array;
};

static struct adaptive_coolers amutt;

static int amutt_backoff(int level)
{
	int ret;

	if (level == 0) {
		/* no throttle */
		ret =
			exec_ccci_kern_func_by_md_id(MD_SYS1, ID_THROTTLING_CFG,
					(char *)&amutt_param[level], 4);
		mtk_cooler_amutt_dprintk_always("[%s] unlimit\n", __func__);

	} else if (level >= 1 && level <= COOLER_STEPS - 1) {
		ret =
			exec_ccci_kern_func_by_md_id(MD_SYS1, ID_THROTTLING_CFG,
					(char *)&amutt_param[level], 4);
		mtk_cooler_amutt_dprintk_always("[%s] limit %x\n", __func__,
							amutt_param[level]);
	} else {
		/* error... */
		ret = -1;
		mtk_cooler_amutt_dprintk_always("[%s] ouf of range\n",
								__func__);
	}

	return ret;

}



static int down_throttle(struct adaptive_coolers *p, int step)
{
	if (p == NULL)
		return -1;
	if (step <= 0)
		return p->cur_level;

	if (p->cur_level + step > p->max_level) {
		p->cur_level = p->max_level;
		p->opp_func_array[p->cur_level] (p->cur_level);
		return p->cur_level;
	}
	p->cur_level += step;
	p->opp_func_array[p->cur_level] (p->cur_level);
	return p->cur_level;
}

static int up_throttle(struct adaptive_coolers *p, int step)
{
	if (p == NULL)
		return -1;
	if (step <= 0)
		return p->cur_level;

	if (p->cur_level - step < 0) {
		p->cur_level = 0;
		p->opp_func_array[p->cur_level] (p->cur_level);
		return p->cur_level;
	}
	p->cur_level -= step;
	p->opp_func_array[p->cur_level] (p->cur_level);
	return p->cur_level;
}

static int rst_throttle(struct adaptive_coolers *p)
{
	if (p == NULL)
		return -1;

	p->cur_level = 0;
	p->opp_func_array[p->cur_level] (p->cur_level);
	return p->cur_level;
}

/* index --> 0, lower; 1, upper */
/* is_on --> 0, off; 1, on */
static int judge_throttling(int index, int is_on, int interval)
{
	/*
	 *     throttling_stat
	 *        2 ( upper=1,lower=1 )
	 * UPPER ----
	 *        1 ( upper=0,lower=1 )
	 * LOWER ----
	 *        0 ( upper=0,lower=0 )
	 */
	static unsigned int throttling_pre_stat;
	static int mail_box[2] = { -1, -1 };

	static bool is_reset;

	/* unsigned long cur_thro = tx_throughput; */
	/* static unsigned long thro_constraint = 99 * 1000; */

	int cur_wifi_stat = 0;

	mtk_cooler_amutt_dprintk("[%s]+ [0]=%d, [1]=%d || [%d] is %s\n",
						__func__, mail_box[0],
						mail_box[1], index,
						(is_on == 1 ? "ON" : "OFF"));
	mail_box[index] = is_on;

	if (mail_box[0] >= 0 && mail_box[1] >= 0) {
		cur_wifi_stat = mail_box[0] + mail_box[1];

		switch (cur_wifi_stat) {
		case HIGH_STAT:
			if (throttling_pre_stat < HIGH_STAT) {
				/* 1st down throttle */
				int new_step = down_throttle(
						&amutt, up_step);

				mtk_cooler_amutt_dprintk_always(
						"LOW/MID-->HIGH: step %d\n",
						new_step);

				throttling_pre_stat = HIGH_STAT;
				over_up_time = 0;
			} else if (throttling_pre_stat == HIGH_STAT) {
				/* keep down throttle */
				over_up_time++;
				if ((over_up_time * interval)
				>= up_duration) {
					int new_step =
						down_throttle(&amutt,
							up_step);

					mtk_cooler_amutt_dprintk_always(
							"HIGH-->HIGH: step %d\n",
							new_step);

					over_up_time = 0;
				}
			} else {
				mtk_cooler_amutt_dprintk(
						"[%s] Error state1=%d!!\n",
						__func__,
						throttling_pre_stat);
			}
			mtk_cooler_amutt_dprintk_always(
						"case2 time=%d\n",
						over_up_time);
			break;

		case MID_STAT:
			if (throttling_pre_stat == LOW_STAT) {
				below_low_time = 0;
				throttling_pre_stat = MID_STAT;
				mtk_cooler_amutt_dprintk_always(
						"[%s] Go up!!\n",
						__func__);

			} else if (throttling_pre_stat == HIGH_STAT) {
				over_up_time = 0;
				throttling_pre_stat = MID_STAT;
				mtk_cooler_amutt_dprintk_always(
						"[%s] Go down!!\n",
						__func__);

			} else {
				throttling_pre_stat = MID_STAT;
				mtk_cooler_amutt_dprintk(
						"[%s] pre_stat=%d!!\n",
						__func__,
						throttling_pre_stat);
			}
			break;

		case LOW_STAT:
			if (throttling_pre_stat > LOW_STAT) {
				/* 1st up throttle */
				int new_step = up_throttle(&amutt,
							low_step);

				mtk_cooler_amutt_dprintk_always(
							"MID/HIGH-->LOW: step %d\n",
							new_step);
				throttling_pre_stat = LOW_STAT;
				below_low_time = 0;
				low_rst_time = 0;
				is_reset = false;
			} else if (throttling_pre_stat == LOW_STAT) {
				below_low_time++;
				if ((below_low_time * interval)
				>= low_duration) {
					if (low_rst_time >=
					low_rst_max && !is_reset) {
						/* rst */
						rst_throttle(&amutt);

						mtk_cooler_amutt_dprintk_always(
							"over rst time=%d\n",
							low_rst_time);

						low_rst_time =
							low_rst_max;
							is_reset = true;
					} else if (!is_reset) {
						/* keep up throttle */
						int new_step =
							up_throttle(
							&amutt,
							low_step);

						low_rst_time++;

						mtk_cooler_amutt_dprintk_always(
							"LOW-->LOW: step %d\n",
							new_step);

						below_low_time = 0;
					} else {
						mtk_cooler_amutt_dprintk(
							"Have reset, no control!!"
							);
					}
				}
			} else {
				mtk_cooler_amutt_dprintk_always(
						"[%s] Error state3 %d!!\n",
						__func__,
						throttling_pre_stat);
			}
			mtk_cooler_amutt_dprintk(
						"case0 time=%d, rst=%d %d\n",
						below_low_time,
						low_rst_time, is_reset);
			break;

		default:
			mtk_cooler_amutt_dprintk_always(
				"[%s] Error cur_wifi_stat=%d!!\n",
				__func__, cur_wifi_stat);
			break;
	}

		mail_box[0] = UNK_STAT;
		mail_box[1] = UNK_STAT;
	} else {
		mtk_cooler_amutt_dprintk(
				"[%s] dont get all info!!\n", __func__);
	}
	return 0;
}

/* +amutt_cooler_upper_ops+ */
static int amutt_cooler_upper_get_max_state(
struct thermal_cooling_device *cool_dev, unsigned long *pv)
{
	*pv = 1;
	mtk_cooler_amutt_dprintk("[%s] %lu\n", __func__, *pv);
	return 0;
}

static int amutt_cooler_upper_get_cur_state(
struct thermal_cooling_device *cool_dev, unsigned long *pv)
{
	*pv = cl_upper_dev_state;
	mtk_cooler_amutt_dprintk("[%s] %lu\n", __func__, *pv);
	return 0;
}

static int amutt_cooler_upper_set_cur_state(
struct thermal_cooling_device *cool_dev, unsigned long v)
{
	int ret = 0;

	mtk_cooler_amutt_dprintk("[%s] %lu\n", __func__, v);

	cl_upper_dev_state = (unsigned int)v;

	if (cl_upper_dev_state == 1)
		ret = judge_throttling(1, 1, polling_interval);
	else
		ret = judge_throttling(1, 0, polling_interval);

	if (ret != 0)
		mtk_cooler_amutt_dprintk_always("[%s] ret=%d\n", __func__, ret);
	return ret;
}

static struct thermal_cooling_device_ops amutt_cooler_upper_ops = {
	.get_max_state = amutt_cooler_upper_get_max_state,
	.get_cur_state = amutt_cooler_upper_get_cur_state,
	.set_cur_state = amutt_cooler_upper_set_cur_state,
};

/* -amutt_cooler_upper_ops- */

/* +amutt_cooler_lower_ops+ */
static int amutt_cooler_lower_get_max_state(
struct thermal_cooling_device *cool_dev, unsigned long *pv)
{
	*pv = 1;
	mtk_cooler_amutt_dprintk("[%s] %lu\n", __func__, *pv);
	return 0;
}

static int amutt_cooler_lower_get_cur_state(
struct thermal_cooling_device *cool_dev, unsigned long *pv)
{
	*pv = cl_lower_dev_state;
	mtk_cooler_amutt_dprintk("[%s] %lu\n", __func__, *pv);
	return 0;
}

static int amutt_cooler_lower_set_cur_state(
struct thermal_cooling_device *cool_dev, unsigned long v)
{
	int ret = 0;

	mtk_cooler_amutt_dprintk("[%s] %lu\n", __func__, v);

	cl_lower_dev_state = (unsigned int)v;

	if (cl_lower_dev_state == 1)
		ret = judge_throttling(0, 1, polling_interval);
	else
		ret = judge_throttling(0, 0, polling_interval);

	if (ret != 0)
		mtk_cooler_amutt_dprintk_always("[%s] ret=%d\n", __func__, ret);
	return ret;
}

static struct thermal_cooling_device_ops amutt_cooler_lower_ops = {
	.get_max_state = amutt_cooler_lower_get_max_state,
	.get_cur_state = amutt_cooler_lower_get_cur_state,
	.set_cur_state = amutt_cooler_lower_set_cur_state,
};

/* -amutt_cooler_lower_ops- */

static int mtk_cooler_amutt_register_ltf(void)
{
	mtk_cooler_amutt_dprintk("[%s]\n", __func__);

	cl_upper_dev = mtk_thermal_cooling_device_register("cl-amutt-upper",
						NULL, &amutt_cooler_upper_ops);

	cl_lower_dev = mtk_thermal_cooling_device_register("cl-amutt-lower",
						NULL, &amutt_cooler_lower_ops);

	return 0;
}

static void mtk_cooler_amutt_unregister_ltf(void)
{
	mtk_cooler_amutt_dprintk("[%s]\n", __func__);

	if (cl_upper_dev) {
		mtk_thermal_cooling_device_unregister(cl_upper_dev);
		cl_upper_dev = NULL;
	}

	if (cl_lower_dev) {
		mtk_thermal_cooling_device_unregister(cl_lower_dev);
		cl_lower_dev = NULL;
	}
}

static int amutt_proc_register(void)
{
	mtk_cooler_amutt_dprintk("[%s]\n", __func__);

	return 0;
}

static int __init mtk_cooler_amutt_init(void)
{
	int err = 0, i = 0;

	mtk_cooler_amutt_dprintk("[%s]\n", __func__);

	for (; i < COOLER_STEPS; i++)
		opp_func[i] = amutt_backoff;

	amutt.cur_level = 0;
	amutt.max_level = COOLER_STEPS - 1;
	amutt.opp_func_array = &opp_func[0];

	err = amutt_proc_register();
	if (err)
		return err;

	err = mtk_cooler_amutt_register_ltf();
	if (err)
		goto err_unreg;

	return 0;

err_unreg:
	mtk_cooler_amutt_unregister_ltf();
	return err;
}

static void __exit mtk_cooler_amutt_exit(void)
{
	mtk_cooler_amutt_dprintk("[%s]\n", __func__);

	/* remove the proc file */
	/* remove_proc_entry("amutt", NULL); */

	mtk_cooler_amutt_unregister_ltf();
}
module_init(mtk_cooler_amutt_init);
module_exit(mtk_cooler_amutt_exit);
