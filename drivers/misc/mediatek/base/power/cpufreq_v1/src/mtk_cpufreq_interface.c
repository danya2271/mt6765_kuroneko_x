/*
 * Copyright (C) 2016 MediaTek Inc.
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

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#include "mtk_cpufreq_internal.h"
#include "mtk_cpufreq_hybrid.h"
#include "mtk_cpufreq_platform.h"

unsigned int func_lv_mask;
unsigned int do_dvfs_stress_test;
unsigned int dvfs_power_mode;
unsigned int sched_dvfs_enable;

ktime_t now[NR_SET_V_F];
ktime_t delta[NR_SET_V_F];
ktime_t max[NR_SET_V_F];

enum ppb_power_mode {
	PERFORMANCE_MODE,	/* sports mode */
	NUM_PPB_POWER_MODE
};

static const char *power_mode_str[NUM_PPB_POWER_MODE] = {
	"Performance(Sports) mode"
};

char *_copy_from_user_for_proc(const char __user *buffer, size_t count)
{
	static char buf[64];
	unsigned int len = 0;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);

	if (copy_from_user(buf, buffer, len))
		return NULL;

	buf[len] = '\0';

	return buf;
}

/* cpufreq_time_profile */
static int cpufreq_dvfs_time_profile_proc_show(struct seq_file *m, void *v)
{
	int i;

	for (i = 0; i < NR_SET_V_F; i++)
		seq_printf(m, "max[%d] = %lld us\n", i, ktime_to_us(max[i]));

#ifdef CONFIG_HYBRID_CPU_DVFS
	cpuhvfs_get_time_profile();
#endif

	return 0;
}

static ssize_t cpufreq_dvfs_time_profile_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int temp;
	int rc;
	int i;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	rc = kstrtoint(buf, 10, &temp);
	if (rc < 0)
		tag_pr_info
		("echo 1 > /proc/cpufreq/cpufreq_dvfs_time_profile\n");
	else {
		if (temp == 1) {
			for (i = 0; i < NR_SET_V_F; i++)
				max[i].tv64 = 0;
		}
	}

	return count;
}

#ifdef CCI_MAP_TBL_SUPPORT
/* cpufreq_cci_map_table */
static int cpufreq_cci_map_table_proc_show(struct seq_file *m, void *v)
{
	int i, j, k;
	unsigned int result;
	unsigned int pt_1 = 0, pt_2 = 0;

#ifdef CONFIG_HYBRID_CPU_DVFS
	for (k = 0; k < NR_CCI_TBL; k++) {
		if (k == 0 && !pt_1) {
			seq_puts(m, "CCI MAP Normal Mode:\n");
			pt_1 = 1;
		} else if (k == 1 && !pt_2) {
			seq_puts(m, "CCI MAP Perf Mode:\n");
			pt_2 = 1;
		}
		for (i = 0; i < NR_FREQ; i++) {
			for (j = 0; j < NR_FREQ; j++) {
				result = cpuhvfs_get_cci_result(i, j, k);
				if (j == 0)
					seq_printf(m, "{%d", result);
				else if (j == (NR_FREQ - 1))
					seq_printf(m, ", %d},", result);
				else
					seq_printf(m, ", %d", result);
			}
			seq_puts(m, "\n");
		}
	}
#endif
	return 0;
}

static ssize_t cpufreq_cci_map_table_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int idx_1, idx_2, result, mode;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (sscanf(buf, "%d %d %d %d",
		&idx_1, &idx_2, &result, &mode) == 4) {
#ifdef CONFIG_HYBRID_CPU_DVFS
		/* BY_PROC_FS */
		cpuhvfs_update_cci_map_tbl(idx_1,
			idx_2, result, mode, 0);
#endif
	} else
		tag_pr_info(
		"Usage: echo <L_idx> <B_idx> <result> <mode>\n");

	return count;
}
/* cpufreq_cci_mode */
static int cpufreq_cci_mode_proc_show(struct seq_file *m, void *v)
{
	unsigned int mode;

#ifdef CONFIG_HYBRID_CPU_DVFS
	mode = cpuhvfs_get_cci_mode();
	if (mode == 0)
		seq_puts(m, "cci_mode as Normal mode\n");
	else if (mode == 1)
		seq_puts(m, "cci_mode as Perf mode\n");
	else
		seq_puts(m, "cci_mode as Unknown mode\n");
#endif
	return 0;
}

static ssize_t cpufreq_cci_mode_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int mode;
	int rc;
	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	rc = kstrtoint(buf, 10, &mode);

	if (rc < 0)
		tag_pr_info(
		"Usage: echo <mode>(0:Nom 1:Perf)\n");
	else {
#ifdef CONFIG_HYBRID_CPU_DVFS
		/* BY_PROC_FS */
		cpuhvfs_update_cci_mode(mode, 0);
#endif
	}

	return count;
}
#endif

PROC_FOPS_RW(cpufreq_dvfs_time_profile);

int cpufreq_procfs_init(void)
{
	struct proc_dir_entry *dir = NULL;
	struct proc_dir_entry *cpu_dir = NULL;
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(0);
	int i, j;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(cpufreq_dvfs_time_profile),
	};

	const struct pentry cpu_entries[] = {
	};

	dir = proc_mkdir("cpufreq", NULL);

	if (!dir) {
		tag_pr_notice("fail to create /proc/cpufreq @ %s()\n",
								__func__);
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create
		    (entries[i].name, 0664, dir, entries[i].fops))
			tag_pr_notice("%s(), create /proc/cpufreq/%s failed\n",
				__func__, entries[i].name);
	}

	for_each_cpu_dvfs(j, p) {
		cpu_dir = proc_mkdir(p->name, dir);

		if (!cpu_dir) {
			tag_pr_notice
				("fail to create /proc/cpufreq/%s @ %s()\n",
				p->name, __func__);
			return -ENOMEM;
		}

		for (i = 0; i < ARRAY_SIZE(cpu_entries); i++) {
			if (!proc_create_data
			    (cpu_entries[i].name, 0664,
			    cpu_dir, cpu_entries[i].fops, p))
				tag_pr_notice
				("%s(), create /proc/cpufreq/%s/%s failed\n",
				__func__, p->name, entries[i].name);
		}
	}

	return 0;
}
