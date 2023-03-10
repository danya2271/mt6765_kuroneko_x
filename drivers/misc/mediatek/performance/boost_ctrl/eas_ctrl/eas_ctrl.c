/*
 * Copyright (C) 2016-2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#define pr_fmt(fmt) "[eas_ctrl]"fmt

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#include "boost_ctrl.h"
#include "eas_ctrl_plat.h"
#include "eas_ctrl.h"
#include "mtk_perfmgr_internal.h"
#include <mt-plat/mtk_sched.h>
#include <linux/sched.h>

/* boost value */
static struct mutex boost_eas;
#ifdef CONFIG_CGROUP_SCHEDTUNE
static int current_boost_value[NR_CGROUP];
static unsigned long policy_mask[NR_CGROUP];
#endif
static int boost_value[NR_CGROUP][EAS_MAX_KIR];
static int debug_boost_value[NR_CGROUP];
static int debug_fix_boost;

/* uclamp */
#if defined(CONFIG_UCLAMP_TASK_GROUP) && defined(CONFIG_CGROUP_SCHEDTUNE)
static int cur_uclamp_min[NR_CGROUP];
static unsigned long uclamp_policy_mask[NR_CGROUP];
#endif
static int uclamp_min[NR_CGROUP][EAS_MAX_KIR];
static int debug_uclamp_min[NR_CGROUP];

/* log */
static int log_enable = 0;

static bool perf_sched_big_task_rotation;
static int  perf_sched_stune_task_thresh;

#define MAX_BOOST_VALUE	(100)
#define MIN_BOOST_VALUE	(-100)
#define MAX_UCLAMP_VALUE		(100)
#define MIN_UCLAMP_VALUE		(0)
#define MIN_DEBUG_UCLAMP_VALUE	(-1)

/************************/

static void walt_mode(int enable)
{
#ifdef CONFIG_SCHED_WALT
	sched_walt_enable(LT_WALT_POWERHAL, enable);
#else
	pr_debug("walt not be configured\n");
#endif
}

void ext_launch_start(void)
{
	pr_debug("ext_launch_start\n");
	/*--feature start from here--*/
	walt_mode(1);
}

void ext_launch_end(void)
{
	pr_debug("ext_launch_end\n");
	/*--feature end from here--*/
	walt_mode(0);
}
/************************/

static int check_boost_value(int boost_value)
{
	return clamp(boost_value, MIN_BOOST_VALUE, MAX_BOOST_VALUE);
}

static int check_uclamp_value(int value)
{
	return clamp(value, MIN_UCLAMP_VALUE, MAX_UCLAMP_VALUE);
}

/************************/

#ifdef CONFIG_CGROUP_SCHEDTUNE
int update_eas_boost_value(int kicker, int cgroup_idx, int value)
{
	int final_boost = 0;
	int i, len = 0, len1 = 0;

	char msg[LOG_BUF_SIZE];
	char msg1[LOG_BUF_SIZE];

	mutex_lock(&boost_eas);

	if (cgroup_idx >= NR_CGROUP) {
		mutex_unlock(&boost_eas);
		pr_debug(" cgroup_idx >= NR_CGROUP, error\n");
		return -1;
	}

	boost_value[cgroup_idx][kicker] = value;
	len += snprintf(msg + len, sizeof(msg) - len, "[%d] [%d] [%d]",
			kicker, cgroup_idx, value);

	/*ptr return error EIO:I/O error */
	if (len < 0) {
		mutex_unlock(&boost_eas);
		return -EIO;
	}

	for (i = 0; i < EAS_MAX_KIR; i++) {
		if (boost_value[cgroup_idx][i] == 0) {
			clear_bit(i, &policy_mask[cgroup_idx]);
			continue;
		}

		/* Always set first to handle negative input */
		if (final_boost == 0)
			final_boost = boost_value[cgroup_idx][i];
		else
			final_boost = MAX(final_boost,
				boost_value[cgroup_idx][i]);

		set_bit(i, &policy_mask[cgroup_idx]);
	}

	current_boost_value[cgroup_idx] = check_boost_value(final_boost);

	len += snprintf(msg + len, sizeof(msg) - len, "{%d} ", final_boost);
	/*ptr return error EIO:I/O error */
	if (len < 0) {
		mutex_unlock(&boost_eas);
		return -EIO;
	}
	len1 += snprintf(msg1 + len1, sizeof(msg1) - len1, "[0x %lx] ",
			policy_mask[cgroup_idx]);

	if (len1 < 0) {
		mutex_unlock(&boost_eas);
		return -EIO;
	}
	if (!debug_fix_boost)
		boost_write_for_perf_idx(cgroup_idx,
				current_boost_value[cgroup_idx]);

	if (strlen(msg) + strlen(msg1) < LOG_BUF_SIZE)
		strncat(msg, msg1, strlen(msg1));

	if (log_enable)
		pr_debug("%s\n", msg);
	mutex_unlock(&boost_eas);

	return current_boost_value[cgroup_idx];
}
#else
int update_eas_boost_value(int kicker, int cgroup_idx, int value)
{
	return -1;
}
#endif

#if defined(CONFIG_UCLAMP_TASK_GROUP) && defined(CONFIG_CGROUP_SCHEDTUNE)
int update_eas_uclamp_min(int kicker, int cgroup_idx, int value)
{
	int final_uclamp = 0;
	int i, len = 0, len1 = 0;

	char msg[LOG_BUF_SIZE];
	char msg1[LOG_BUF_SIZE];

	mutex_lock(&boost_eas);

	if (cgroup_idx >= NR_CGROUP) {
		mutex_unlock(&boost_eas);
		pr_debug(" cgroup_idx >= NR_CGROUP, error\n");
		return -1;
	}

	uclamp_min[cgroup_idx][kicker] = value;
	len += snprintf(msg + len, sizeof(msg) - len, "[%d] [%d] [%d]",
			kicker, cgroup_idx, value);

	/* ptr return error EIO:I/O error */
	if (len < 0) {
		mutex_unlock(&boost_eas);
		return -EIO;
	}

	for (i = 0; i < EAS_UCLAMP_MAX_KIR; i++) {
		if (uclamp_min[cgroup_idx][i] == 0) {
			clear_bit(i, &uclamp_policy_mask[cgroup_idx]);
			continue;
		}

		final_uclamp = MAX(final_uclamp,
			uclamp_min[cgroup_idx][i]);

		set_bit(i, &uclamp_policy_mask[cgroup_idx]);
	}

	cur_uclamp_min[cgroup_idx] = check_uclamp_value(final_uclamp);

	len += snprintf(msg + len, sizeof(msg) - len, "{%d} ", final_uclamp);

	/*ptr return error EIO:I/O error */
	if (len < 0) {
		mutex_unlock(&boost_eas);
		return -EIO;
	}

	len1 += snprintf(msg1 + len1, sizeof(msg1) - len1, "[0x %lx] ",
			uclamp_policy_mask[cgroup_idx]);

	if (len1 < 0) {
		mutex_unlock(&boost_eas);
		return -EIO;
	}
	if (debug_uclamp_min[cgroup_idx] == -1)
		uclamp_min_for_perf_idx(cgroup_idx,
				cur_uclamp_min[cgroup_idx]);

	if (strlen(msg) + strlen(msg1) < LOG_BUF_SIZE)
		strncat(msg, msg1, strlen(msg1));
	if (log_enable)
		pr_debug("%s\n", msg);
	mutex_unlock(&boost_eas);

	return cur_uclamp_min[cgroup_idx];
}
#else
int update_eas_uclamp_min(int kicker, int cgroup_idx, int value)
{
	return -1;
}
#endif
EXPORT_SYMBOL(update_eas_uclamp_min);

/****************/
static ssize_t perfmgr_perfserv_fg_boost_proc_write(struct file *filp
		, const char *ubuf, size_t cnt, loff_t *pos)
{
	int data = 0;

	int rv = check_proc_write(&data, ubuf, cnt);

	if (rv != 0)
		return rv;

	data = check_boost_value(data);

	update_eas_boost_value(EAS_KIR_PERF, CGROUP_FG, data);

	return cnt;
}

static int perfmgr_perfserv_fg_boost_proc_show(struct seq_file *m, void *v)
{
	return 0;
}

/************************************************/
static int perfmgr_current_fg_boost_proc_show(struct seq_file *m, void *v)
{
	return 0;
}

/************************************************************/

static ssize_t perfmgr_perfserv_bg_boost_proc_write(struct file *filp,
		const char *ubuf, size_t cnt, loff_t *pos)
{
	int data = 0;

	int rv = check_proc_write(&data, ubuf, cnt);

	if (rv != 0)
		return rv;

	data = check_boost_value(data);

	update_eas_boost_value(EAS_KIR_PERF, CGROUP_BG, data);

	return cnt;
}

static int perfmgr_perfserv_bg_boost_proc_show(struct seq_file *m, void *v)
{
	return 0;
}
/*******************************************************/
static int perfmgr_current_bg_boost_proc_show(struct seq_file *m, void *v)
{
	return 0;
}

static ssize_t perfmgr_perfserv_ta_boost_proc_write(
		struct file *filp, const char *ubuf,
		size_t cnt, loff_t *pos)
{
	int data = 0;
	int rv = check_proc_write(&data, ubuf, cnt);

	if (rv != 0)
		return rv;

	data = check_boost_value(data);

	update_eas_boost_value(EAS_KIR_PERF, CGROUP_TA, data);

	return cnt;
}

static int perfmgr_perfserv_ta_boost_proc_show(
		struct seq_file *m, void *v)
{
	return 0;
}
/************************************************/
static ssize_t perfmgr_boot_boost_proc_write(
		struct file *filp, const char *ubuf,
		size_t cnt, loff_t *pos)
{
	int cgroup = 0, data = 0;

	int rv = check_boot_boost_proc_write(&cgroup, &data, ubuf, cnt);

	if (rv != 0)
		return rv;

	data = check_boost_value(data);

	if (cgroup >= 0 && cgroup < NR_CGROUP)
		update_eas_boost_value(EAS_KIR_BOOT, cgroup, data);

	return cnt;
}

static int perfmgr_boot_boost_proc_show(struct seq_file *m, void *v)
{
	return 0;
}
/************************************************/
static int perfmgr_current_ta_boost_proc_show(struct seq_file *m, void *v)
{
	return 0;
}

/****************/
/* uclamp min   */
/****************/
static ssize_t perfmgr_perfserv_uclamp_min_proc_write(struct file *filp
		, const char *ubuf, size_t cnt, loff_t *pos)
{
	int data = 0;

	int rv = check_proc_write(&data, ubuf, cnt);

	if (rv != 0)
		return rv;

	data = check_uclamp_value(data);

	update_eas_uclamp_min(EAS_UCLAMP_KIR_PERF, CGROUP_ROOT, data);

	return cnt;
}

static int perfmgr_perfserv_uclamp_min_proc_show(struct seq_file *m, void *v)
{
	return 0;
}

/************************************************/
static int perfmgr_current_uclamp_min_proc_show(struct seq_file *m, void *v)
{
	return 0;
}

/************************************************************/

static ssize_t perfmgr_perfserv_fg_uclamp_min_proc_write(struct file *filp
		, const char *ubuf, size_t cnt, loff_t *pos)
{
	int data = 0;

	int rv = check_proc_write(&data, ubuf, cnt);

	if (rv != 0)
		return rv;

	data = check_uclamp_value(data);

	update_eas_uclamp_min(EAS_UCLAMP_KIR_PERF, CGROUP_FG, data);

	return cnt;
}

static int perfmgr_perfserv_fg_uclamp_min_proc_show(struct seq_file *m, void *v)
{
	return 0;
}

/************************************************/
static int perfmgr_current_fg_uclamp_min_proc_show(struct seq_file *m, void *v)
{
	return 0;
}

/************************************************************/

static ssize_t perfmgr_perfserv_bg_uclamp_min_proc_write(struct file *filp
		, const char *ubuf, size_t cnt, loff_t *pos)
{
	int data = 0;

	int rv = check_proc_write(&data, ubuf, cnt);

	if (rv != 0)
		return rv;

	data = check_uclamp_value(data);

	update_eas_uclamp_min(EAS_UCLAMP_KIR_PERF, CGROUP_BG, data);

	return cnt;
}

static int perfmgr_perfserv_bg_uclamp_min_proc_show(struct seq_file *m, void *v)
{
	return 0;
}

/************************************************/
static int perfmgr_current_bg_uclamp_min_proc_show(struct seq_file *m, void *v)
{
	return 0;
}

static ssize_t perfmgr_perfserv_ta_uclamp_min_proc_write(struct file *filp
		, const char *ubuf, size_t cnt, loff_t *pos)
{
	int data = 0;

	int rv = check_proc_write(&data, ubuf, cnt);

	if (rv != 0)
		return rv;

	data = check_uclamp_value(data);

	update_eas_uclamp_min(EAS_UCLAMP_KIR_PERF, CGROUP_TA, data);

	return cnt;
}

static int perfmgr_perfserv_ta_uclamp_min_proc_show(struct seq_file *m, void *v)
{
	return 0;
}

/************************************************/
static int perfmgr_current_ta_uclamp_min_proc_show(struct seq_file *m, void *v)
{
	return 0;
}

static int ext_launch_state;
static ssize_t perfmgr_perfserv_ext_launch_mon_proc_write(struct file *filp,
		const char *ubuf, size_t cnt, loff_t *pos)
{
	int data = 0;
	int rv = check_proc_write(&data, ubuf, cnt);

	if (rv != 0)
		return rv;

	if (data) {
		ext_launch_start();
		ext_launch_state = 1;
	} else {
		ext_launch_end();
		ext_launch_state = 0;
	}

	pr_debug("perfmgr_perfserv_ext_launch_mon");
	return cnt;
}

	static int
perfmgr_perfserv_ext_launch_mon_proc_show(
		struct seq_file *m, void *v)
{
	return 0;
}

/* Add procfs to control sysctl_sched_migration_cost */
/* sysctl_sched_migration_cost: eas_ctrl_plat.h */
static ssize_t perfmgr_m_sched_migrate_cost_n_proc_write(struct file *filp,
		const char *ubuf, size_t cnt, loff_t *pos)
{
	int data = 0;
	int rv = check_proc_write(&data, ubuf, cnt);

	if (rv != 0)
		return rv;

	sysctl_sched_migration_cost = data;

	return cnt;
}

static int perfmgr_m_sched_migrate_cost_n_proc_show(struct seq_file *m, void *v)
{
	return 0;
}

/* Add procfs to control sysctl_sched_rotation_enable */
/* sysctl_sched_rotation_enable: eas_ctrl_plat.h */
static ssize_t perfmgr_sched_big_task_rotation_proc_write(struct file *filp,
		const char *ubuf, size_t cnt, loff_t *pos)
{
	int data = 0;
	int rv = check_proc_write(&data, ubuf, cnt);

	if (rv != 0)
		return rv;

	perf_sched_big_task_rotation = data;
	if (data)
		set_sched_rotation_enable(true);
	else
		set_sched_rotation_enable(false);

	return cnt;
}

static int perfmgr_sched_big_task_rotation_proc_show(struct seq_file *m,
	void *v)
{
	return 0;
}

static ssize_t perfmgr_sched_stune_task_thresh_proc_write(struct file *filp,
		const char *ubuf, size_t cnt, loff_t *pos)
{
	int data = 0;
	int rv = check_proc_write(&data, ubuf, cnt);

	if (rv != 0)
		return rv;

	perf_sched_stune_task_thresh = data;
#ifdef CONFIG_CGROUP_SCHEDTUNE
	if (perf_sched_stune_task_thresh >= 0)
		set_stune_task_threshold(perf_sched_stune_task_thresh);
	else
		set_stune_task_threshold(-1);
#endif

	return cnt;
}

static int perfmgr_sched_stune_task_thresh_proc_show(struct seq_file *m,
	void *v)
{
	return 0;
}

/* boost value */
PROC_FOPS_RW(perfserv_fg_boost);
PROC_FOPS_RW(perfserv_bg_boost);
PROC_FOPS_RW(perfserv_ta_boost);
PROC_FOPS_RW(boot_boost);

/* uclamp */
PROC_FOPS_RW(perfserv_uclamp_min);
PROC_FOPS_RW(perfserv_fg_uclamp_min);
PROC_FOPS_RW(perfserv_bg_uclamp_min);
PROC_FOPS_RW(perfserv_ta_uclamp_min);

/* others */
PROC_FOPS_RW(perfserv_ext_launch_mon);
PROC_FOPS_RW(m_sched_migrate_cost_n);
PROC_FOPS_RW(sched_big_task_rotation);
PROC_FOPS_RW(sched_stune_task_thresh);

/*******************************************/
int eas_ctrl_init(struct proc_dir_entry *parent)
{
	int i, j, ret = 0;
	struct proc_dir_entry *boost_dir = NULL;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		/* boost value */
		PROC_ENTRY(perfserv_fg_boost),
		PROC_ENTRY(perfserv_bg_boost),
		PROC_ENTRY(perfserv_ta_boost),
		PROC_ENTRY(boot_boost),
		/* uclamp */
		PROC_ENTRY(perfserv_uclamp_min),
		PROC_ENTRY(perfserv_fg_uclamp_min),
		PROC_ENTRY(perfserv_bg_uclamp_min),
		PROC_ENTRY(perfserv_ta_uclamp_min),

		/*--ext_launch--*/
		PROC_ENTRY(perfserv_ext_launch_mon),
		/*--sched migrate cost n--*/
		PROC_ENTRY(m_sched_migrate_cost_n),
		PROC_ENTRY(sched_big_task_rotation),
		PROC_ENTRY(sched_stune_task_thresh),
	};
	mutex_init(&boost_eas);
	boost_dir = proc_mkdir("eas_ctrl", parent);

	if (!boost_dir)
		pr_debug("boost_dir null\n ");

	/* create procfs */
	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(entries[i].name, 0644,
					boost_dir, entries[i].fops)) {
			pr_debug("%s(), create /eas_ctrl%s failed\n",
					__func__, entries[i].name);
			ret = -EINVAL;
			goto out;
		}
	}

	/* boost value */
	for (i = 0; i < NR_CGROUP; i++) {
		current_boost_value[i] = 0;
		for (j = 0; j < EAS_MAX_KIR; j++)
			boost_value[i][j] = 0;
	}

#if defined(CONFIG_UCLAMP_TASK_GROUP) && defined(CONFIG_CGROUP_SCHEDTUNE)
	/* uclamp */
	for (i = 0; i < NR_CGROUP; i++) {
		cur_uclamp_min[i] = 0;
		debug_uclamp_min[i] = -1;
		for (j = 0; j < EAS_UCLAMP_MAX_KIR; j++)
			uclamp_min[i][j] = 0;
	}
#endif

	perf_sched_big_task_rotation = 0;
	perf_sched_stune_task_thresh = -1;

	debug_fix_boost = 0;

out:
	return ret;
}
