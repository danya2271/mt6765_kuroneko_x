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

/* system includes */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/sched/rt.h>
#include <linux/atomic.h>
#include <linux/clk.h>
#include <linux/ktime.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/bitops.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>
#include <linux/types.h>
#include <linux/suspend.h>
#include <linux/topology.h>
#include <linux/math64.h>
#include <mt-plat/sync_write.h>
#include <mt-plat/mtk_io.h>
#include <mt-plat/aee.h>

#include <mtk_cm_mgr.h>
#include <mtk_cm_mgr_platform_data.h>
#ifdef CONFIG_MTK_CPU_FREQ
#include <mtk_cpufreq_api.h>
#endif /* CONFIG_MTK_CPU_FREQ */

#include <linux/pm_qos.h>
#include <helio-dvfsrc.h>

#define CMMGR_NOLOG 1

#if CMMGR_NOLOG

#ifdef pr_info
#undef pr_info
#endif

#ifdef pr_debug
#undef pr_debug
#endif

#ifdef pr_notice
#undef pr_notice
#endif

#ifdef pr_fmt
#undef pr_fmt
#endif

#ifdef pr_err
#undef pr_err
#endif

#ifdef pr_warn
#undef pr_warn
#endif

#define pr_info(format, args...)
#define pr_debug(format, args...)
#define pr_notice(format, args...)
#define pr_err(format, args...)
#define pr_warn(format, args...)
#define pr_fmt(fmt)
#define UNUSED(x) (void)(x)

#endif

__attribute__((weak))
void cm_mgr_update_dram_by_cpu_opp(int cpu_opp) {};

spinlock_t cm_mgr_lock;

static unsigned long long test_diff;
static unsigned long long cnt;
static unsigned int test_max;
static unsigned int prev_freq_idx[CM_MGR_CPU_CLUSTER];
static unsigned int prev_freq[CM_MGR_CPU_CLUSTER];
/* 0: < 50us */
/* 1: 50~100us */
/* 2: 100~200us */
/* 3: 200~300us */
/* 4: over 300us */
static unsigned int time_cnt_data[5];

unsigned int cpu_power_up_array[CM_MGR_CPU_CLUSTER];
unsigned int cpu_power_down_array[CM_MGR_CPU_CLUSTER];
unsigned int cpu_power_up[CM_MGR_CPU_CLUSTER];
unsigned int cpu_power_down[CM_MGR_CPU_CLUSTER];
unsigned int v2f[CM_MGR_CPU_CLUSTER];
int vcore_power_up;
int vcore_power_down;
int cpu_opp_cur[CM_MGR_CPU_CLUSTER];
int ratio_max[CM_MGR_CPU_CLUSTER];
int ratio[CM_MGR_CPU_COUNT];
int count[CM_MGR_CPU_CLUSTER];
int count_ack[CM_MGR_CPU_CLUSTER];
int vcore_dram_opp;
int vcore_dram_opp_cur;
int cm_mgr_abs_load;
int cm_mgr_rel_load;
int total_bw;
int cps_valid;
int debounce_times_up;
int debounce_times_down;
int ratio_scale[CM_MGR_CPU_CLUSTER];
int max_load[CM_MGR_CPU_CLUSTER];
int cpu_load[NR_CPUS];
int loading_acc[NR_CPUS];
int loading_cnt;

static void update_v2f(int update, int debug)
{
	int i, j;
	int _f, _v, _v2f;

	for (j = 0; j < CM_MGR_CPU_CLUSTER; j++) {
		for (i = 0; i < 16; i++) {
#ifdef CONFIG_MTK_CPU_FREQ
			_f = mt_cpufreq_get_freq_by_idx(j, i) / 1000;
			_v = mt_cpufreq_get_volt_by_idx(j, i) / 100;
#else
			_f = 0;
			_v = 0;
#endif /* CONFIG_MTK_CPU_FREQ */
			_v2f = (_v / 10) * (_v / 10) * _f / 100000;
			if (update)
				_v2f_all[i][j] = _v2f;
			if (debug)
				pr_debug("%d-i %.2d v %.8d f %.8d v2f %.8d\n",
						j, i, _v, _f, _v2f);
		}
	}
}

static int cm_mgr_check_up_status(int level, int *cpu_ratio_idx)
{
	int idx;
	int cpu_power_total;
	int i;

	idx = CM_MGR_CPU_CLUSTER * level;
	cpu_power_total = 0;
#ifdef PER_CPU_STALL_RATIO
	for (i = 0; i < CM_MGR_CPU_COUNT; i++) {
		if (i < CM_MGR_CPU_LIMIT)
			cpu_power_up_array[0] +=
				cpu_power_gain_opp(total_bw, IS_UP,
						cpu_opp_cur[0],
						cpu_ratio_idx[i], idx);
#ifndef USE_SINGLE_CLUSTER
		else
			cpu_power_up_array[1] +=
				cpu_power_gain_opp(total_bw, IS_UP,
						cpu_opp_cur[1],
						cpu_ratio_idx[i], idx + 1);
#endif /* ! USE_SINGLE_CLUSTER */
	}

	for (i = 0; i < CM_MGR_CPU_CLUSTER; i++) {
		cpu_power_up[i] = cpu_power_up_array[i] * v2f[i] / 100;
		cpu_power_total += cpu_power_up[i];
	}
#else
	for (i = 0; i < CM_MGR_CPU_CLUSTER; i++) {
		cpu_power_up_array[i] =
			cpu_power_gain_opp(total_bw, IS_UP,
					cpu_opp_cur[i],
					max_ratio_idx[i], idx + i);
		cpu_power_up[i] = cpu_power_up_array[i] *
			count[i] * v2f[i] / 100;
		cpu_power_total += cpu_power_up[i];
	}
#endif /* PER_CPU_STALL_RATIO */

	if (cm_mgr_opp_enable == 0) {
		if (vcore_dram_opp != CM_MGR_EMI_OPP) {
			vcore_dram_opp = CM_MGR_EMI_OPP;
#ifdef DEBUG_CM_MGR
			pr_info("#@# %s(%d) vcore_dram_opp %d->%d\n",
					__func__, __LINE__,
					vcore_dram_opp_cur, vcore_dram_opp);
#endif /* DEBUG_CM_MGR */
			cm_mgr_set_dram_level(
					CM_MGR_EMI_OPP - vcore_dram_opp);
		}

		return -1;
	}

	idx = level;
	vcore_power_up = vcore_power_gain(vcore_power_gain, total_bw, idx);
#ifdef DEBUG_CM_MGR
	pr_info("#@# vcore_power_up %d < cpu_power_total %d\n",
			vcore_power_up, cpu_power_total);
#endif /* DEBUG_CM_MGR */
	if ((vcore_power_up * vcore_power_ratio_up[idx]) <
			(cpu_power_total * cpu_power_ratio_up[idx])) {
		debounce_times_down = 0;
		if (++debounce_times_up >= debounce_times_up_adb[idx]) {
			if (debounce_times_reset_adb)
				debounce_times_up = 0;
			vcore_dram_opp = vcore_dram_opp_cur - 1;
#ifdef DEBUG_CM_MGR
			pr_info("#@# %s(%d) vcore_dram_opp up %d->%d\n",
					__func__, __LINE__,
					vcore_dram_opp_cur, vcore_dram_opp);
#endif /* DEBUG_CM_MGR */
			cm_mgr_set_dram_level(
					CM_MGR_EMI_OPP - vcore_dram_opp);
		} else {
			if (debounce_times_reset_adb)
				debounce_times_up = 0;
		}

		return -1;
	}

	return 0;
}

static int cm_mgr_check_down_status(int level, int *cpu_ratio_idx)
{
	int idx;
	int cpu_power_total;
	int i;

	idx = CM_MGR_CPU_CLUSTER * (level - 1);
	cpu_power_total = 0;
#ifdef PER_CPU_STALL_RATIO
	for (i = 0; i < CM_MGR_CPU_COUNT; i++) {
		if (i < CM_MGR_CPU_LIMIT)
			cpu_power_down_array[0] +=
				cpu_power_gain_opp(total_bw, IS_DOWN,
						cpu_opp_cur[0],
						cpu_ratio_idx[i], idx);
#ifndef USE_SINGLE_CLUSTER
		else
			cpu_power_down_array[1] +=
				cpu_power_gain_opp(total_bw, IS_DOWN,
						cpu_opp_cur[1],
						cpu_ratio_idx[i], idx + 1);
#endif /* ! USE_SINGLE_CLUSTER */
	}

	for (i = 0; i < CM_MGR_CPU_CLUSTER; i++) {
		cpu_power_down[i] = cpu_power_down_array[i] * v2f[i] / 100;
		cpu_power_total += cpu_power_down[i];
	}
#else
	for (i = 0; i < CM_MGR_CPU_CLUSTER; i++) {
		cpu_power_down_array[i] =
			cpu_power_gain_opp(total_bw, IS_DOWN,
					cpu_opp_cur[i],
					max_ratio_idx[i], idx + i);
		cpu_power_down[i] = cpu_power_down_array[i] *
			count[i] * v2f[i] / 100;
		cpu_power_total += cpu_power_down[i];
	}
#endif /* PER_CPU_STALL_RATIO */

	if (cm_mgr_opp_enable == 0) {
		if (vcore_dram_opp != CM_MGR_EMI_OPP) {
			vcore_dram_opp = CM_MGR_EMI_OPP;
#ifdef DEBUG_CM_MGR
			pr_info("#@# %s(%d) vcore_dram_opp %d->%d\n",
					__func__, __LINE__,
					vcore_dram_opp_cur, vcore_dram_opp);
#endif /* DEBUG_CM_MGR */
			cm_mgr_set_dram_level(
					CM_MGR_EMI_OPP - vcore_dram_opp);
		}

		return -1;
	}

	idx = level - 1;
	vcore_power_down = vcore_power_gain(vcore_power_gain, total_bw, idx);
#ifdef DEBUG_CM_MGR
	pr_info("#@# vcore_power_down %d > cpu_power_total %d\n",
			vcore_power_down, cpu_power_total);
#endif /* DEBUG_CM_MGR */
	if ((vcore_power_down * vcore_power_ratio_down[idx]) >
			(cpu_power_total * cpu_power_ratio_down[idx])) {
		debounce_times_up = 0;
		if (++debounce_times_down >= debounce_times_down_adb[idx]) {
			if (debounce_times_reset_adb)
				debounce_times_down = 0;
			vcore_dram_opp = vcore_dram_opp_cur + 1;
#ifdef DEBUG_CM_MGR
			pr_info("#@# %s(%d) vcore_dram_opp down %d->%d\n",
					__func__, __LINE__,
					vcore_dram_opp_cur, vcore_dram_opp);
#endif /* DEBUG_CM_MGR */
			cm_mgr_set_dram_level(
					CM_MGR_EMI_OPP - vcore_dram_opp);
		} else {
			if (debounce_times_reset_adb)
				debounce_times_down = 0;
		}

		return -1;
	}

	return 0;
}

struct timer_list cm_mgr_perf_timer;
#define USE_TIMER_PERF_CHECK_TIME msecs_to_jiffies(50)

static void cm_mgr_perf_timer_fn(unsigned long data)
{
	if (cm_mgr_perf_timer_enable)
		check_cm_mgr_status_internal();
}

void cm_mgr_perf_set_status(int enable)
{
	cm_mgr_perf_platform_set_force_status(enable);

	if (cm_mgr_perf_force_enable)
		return;

	cm_mgr_perf_platform_set_status(enable);

	if (enable != cm_mgr_perf_timer_enable) {
		cm_mgr_perf_timer_enable = enable;

		if (enable == 1) {
			unsigned long expires;

			expires = jiffies + USE_TIMER_PERF_CHECK_TIME;
			mod_timer(&cm_mgr_perf_timer, expires);

			check_cm_mgr_status_internal();
		} else
			del_timer(&cm_mgr_perf_timer);
	}
}

void cm_mgr_perf_set_force_status(int enable)
{
	if (enable != cm_mgr_perf_force_enable) {
		cm_mgr_perf_force_enable = enable;
		if (enable == 0) {
			cm_mgr_perf_platform_set_force_status(enable);
			check_cm_mgr_status_internal();
		}
	}
}

void check_cm_mgr_status_internal(void)
{
	unsigned long long result = 0;
	ktime_t now, done;
	int level;
	unsigned long flags;

	if (!is_dvfsrc_enabled())
		return;

	if (cm_mgr_enable == 0)
		return;

	if (cm_mgr_disable_fb == 1 && cm_mgr_blank_status == 1)
		return;

	if (cm_mgr_perf_force_enable)
		return;

	if (!cm_mgr_check_bw_status()) {
		cm_mgr_set_dram_level(0);
		return;
	}

	if (spin_trylock_irqsave(&cm_mgr_lock, flags)) {
		int ret;
		int max_ratio_idx[CM_MGR_CPU_CLUSTER];
#if defined(LIGHT_LOAD) && defined(CONFIG_MTK_SCHED_RQAVG_US)
		unsigned int cpu;
		unsigned int rel_load, abs_load;
#endif /* defined(LIGHT_LOAD) && defined(CONFIG_MTK_SCHED_RQAVG_US) */
#ifdef PER_CPU_STALL_RATIO
		int cpu_ratio_idx[CM_MGR_CPU_COUNT];
#endif /* PER_CPU_STALL_RATIO */
		int i;

		vcore_dram_opp_cur = cm_mgr_get_dram_opp();
		if (vcore_dram_opp_cur > CM_MGR_EMI_OPP) {
			spin_unlock_irqrestore(&cm_mgr_lock, flags);
			return;
		}

		if (--cm_mgr_loop > 0)
			goto cm_mgr_opp_end;
		cm_mgr_loop = cm_mgr_loop_count;

#if defined(LIGHT_LOAD) && defined(CONFIG_MTK_SCHED_RQAVG_US)
		cm_mgr_abs_load = 0;
		cm_mgr_rel_load = 0;

		for_each_online_cpu(cpu) {
			int tmp;

			if (cpu >= CM_MGR_CPU_COUNT)
				break;

#ifdef CONFIG_MTK_CPU_FREQ
			tmp = mt_cpufreq_get_cur_phy_freq_no_lock(
					cpu / CM_MGR_CPU_LIMIT) /
				100000;
#else
			tmp = 0;
#endif /* CONFIG_MTK_CPU_FREQ */
			sched_get_percpu_load2(cpu, 1, &rel_load, &abs_load);
			cm_mgr_abs_load += abs_load * tmp;
			cm_mgr_rel_load += rel_load * tmp;

			cpu_load[cpu] = rel_load;
			loading_acc[cpu] += rel_load;
		}
		loading_cnt++;

		if ((cm_mgr_abs_load < light_load_cps) &&
				(vcore_dram_opp_cur == CM_MGR_EMI_OPP)) {
			cps_valid = 0;
			goto cm_mgr_opp_end;
		}
#endif /* defined(LIGHT_LOAD) && defined(CONFIG_MTK_SCHED_RQAVG_US) */
		cps_valid = 1;

		now = ktime_get();

#ifdef USE_NEW_CPU_OPP
#ifdef USE_SINGLE_CLUSTER
		ret = cm_mgr_check_stall_ratio(
				prev_freq[0] / 1000,
				0);
#else
		ret = cm_mgr_check_stall_ratio(
				prev_freq[0] / 1000,
				prev_freq[1] / 1000);
#endif /* USE_SINGLE_CLUSTER */
#else
#ifdef CONFIG_MTK_CPU_FREQ
#ifdef USE_AVG_PMU
#ifdef USE_SINGLE_CLUSTER
		ret = cm_mgr_check_stall_ratio(
				mt_cpufreq_get_cur_phy_freq_no_lock(0) / 1000,
				0);
#else
		ret = cm_mgr_check_stall_ratio(
				mt_cpufreq_get_cur_phy_freq_no_lock(0) / 1000,
				mt_cpufreq_get_cur_phy_freq_no_lock(1) / 1000);
#endif /* USE_SINGLE_CLUSTER */
#else
#ifdef USE_SINGLE_CLUSTER
		ret = cm_mgr_check_stall_ratio(
				mt_cpufreq_get_cur_freq(0) / 1000,
				0);
#else
		ret = cm_mgr_check_stall_ratio(
				mt_cpufreq_get_cur_freq(0) / 1000,
				mt_cpufreq_get_cur_freq(1) / 1000);
#endif /* USE_SINGLE_CLUSTER */
#endif /* USE_AVG_PMU */
#else
		ret = 0;
#endif /* CONFIG_MTK_CPU_FREQ */
#endif /* USE_NEW_CPU_OPP */
		total_bw = cm_mgr_get_bw() / 512;
		memset(count_ack, 0, ARRAY_SIZE(count_ack));

		if (total_bw_value)
			total_bw = total_bw_value;
		if (total_bw >= vcore_power_array_size(cm_mgr_get_idx()))
			total_bw = vcore_power_array_size(cm_mgr_get_idx()) - 1;
		if (total_bw < 0)
			total_bw = 0;

		if (update_v2f_table == 1) {
			update_v2f(1, 0);
			update_v2f_table++;
		}

		/* get max loading */
		memset(max_load, 0, ARRAY_SIZE(count_ack));

		for_each_possible_cpu(i) {
			int avg_load;

			if (i >= CM_MGR_CPU_COUNT)
				break;

			if (unlikely(loading_cnt == 0))
				break;
			avg_load = loading_acc[i] / loading_cnt;
			if (avg_load > max_load[i / CM_MGR_CPU_LIMIT])
				max_load[i / CM_MGR_CPU_LIMIT] = avg_load;
			loading_acc[i] = 0;
		}

		for (i = 0; i < CM_MGR_CPU_CLUSTER; i++) {
			count[i] = cm_mgr_get_cpu_count(i);
			ratio_max[i] = cm_mgr_get_max_stall_ratio(i);
			max_ratio_idx[i] = ratio_max[i] / 5;
			if (max_ratio_idx[i] > RATIO_COUNT)
				max_ratio_idx[i] = RATIO_COUNT;
#ifdef USE_NEW_CPU_OPP
			cpu_opp_cur[i] = prev_freq_idx[i];
#else
#ifdef CONFIG_MTK_CPU_FREQ
			cpu_opp_cur[i] = mt_cpufreq_get_cur_freq_idx(i);
#else
			cpu_opp_cur[i] = 0;
#endif /* CONFIG_MTK_CPU_FREQ */
#endif /* USE_NEW_CPU_OPP */
			v2f[i] = _v2f_all[cpu_opp_cur[i]][i];
			cpu_power_up_array[i] = cpu_power_up[i] = 0;
			cpu_power_down_array[i] = cpu_power_down[i] = 0;

			/* calc scaled ratio */
			ratio_scale[i] = (max_load[i] > 0) ?
				(ratio_max[i] * 100 / max_load[i]) :
				ratio_max[i];
			if (ratio_scale[i] > 100)
				ratio_scale[i] = 100;
		}
#ifdef DEBUG_CM_MGR
		print_hex_dump(KERN_INFO, "cpu_opp_cur: ", DUMP_PREFIX_NONE, 16,
				1, &cpu_opp_cur[0], sizeof(cpu_opp_cur), 0);
#endif /* DEBUG_CM_MGR */

		vcore_power_up = 0;
		vcore_power_down = 0;

		for (i = 0; i < CM_MGR_CPU_COUNT; i++) {
			ratio[i] = cm_mgr_get_stall_ratio(i);
			cpu_ratio_idx[i] = ratio[i] / 5;
			if (cpu_ratio_idx[i] > RATIO_COUNT)
				cpu_ratio_idx[i] = RATIO_COUNT;
		}
#ifdef DEBUG_CM_MGR
		print_hex_dump(KERN_INFO, "ratio: ", DUMP_PREFIX_NONE, 16,
				1, &ratio[0], sizeof(ratio), 0);
		print_hex_dump(KERN_INFO, "cpu_ratio_idx: ",
				DUMP_PREFIX_NONE, 16,
				1, &cpu_ratio_idx[0],
				sizeof(cpu_ratio_idx), 0);
#endif /* DEBUG_CM_MGR */

		level = CM_MGR_EMI_OPP - vcore_dram_opp_cur;
		if (vcore_dram_opp_cur != 0) {
			if (cm_mgr_check_up_status(level, cpu_ratio_idx) < 0)
				goto cm_mgr_opp_end;
		}
		if (vcore_dram_opp_cur != CM_MGR_EMI_OPP) {
			if (cm_mgr_check_down_status(level, cpu_ratio_idx) < 0)
				goto cm_mgr_opp_end;
		}

		vcore_dram_opp = vcore_dram_opp_cur;
		if (vcore_dram_opp == CM_MGR_EMI_OPP)
			cm_mgr_set_dram_level(0);

cm_mgr_opp_end:
		cm_mgr_update_met();

		if (cm_mgr_perf_timer_enable) {
			unsigned long expires;

			expires = jiffies + USE_TIMER_PERF_CHECK_TIME;
			mod_timer(&cm_mgr_perf_timer, expires);
		}


		done = ktime_get();
		if (!ktime_after(done, now)) {
			/* pr_debug("ktime overflow!!\n"); */
			spin_unlock_irqrestore(&cm_mgr_lock, flags);
			return;
		}
		result = ktime_to_us(ktime_sub(done, now));
		test_diff += result;
		cnt++;
		if (result > test_max)
			test_max = result;
		if (result >= 300)
			time_cnt_data[4]++;
		else if (result >= 200 && result < 300)
			time_cnt_data[3]++;
		else if (result >= 100 && result < 200)
			time_cnt_data[2]++;
		else if (result >= 50 && result < 100)
			time_cnt_data[1]++;
		else
			time_cnt_data[0]++;
		spin_unlock_irqrestore(&cm_mgr_lock, flags);
	}
}

void check_cm_mgr_status(unsigned int cluster, unsigned int freq)
{
#ifdef CONFIG_MTK_CPU_FREQ
	int freq_idx = 0;
	struct mt_cpu_dvfs *p;

	p = id_to_cpu_dvfs(cluster);
	if (p)
		freq_idx = _search_available_freq_idx(p, freq, 0);

	if (freq_idx == prev_freq_idx[cluster])
		return;

	prev_freq_idx[cluster] = freq_idx;
	prev_freq[cluster] = freq;
#else
	prev_freq_idx[cluster] = 0;
	prev_freq[cluster] = 0;
#endif /* CONFIG_MTK_CPU_FREQ */

	cm_mgr_update_dram_by_cpu_opp(prev_freq_idx[CM_MGR_CPU_CLUSTER - 1]);

	check_cm_mgr_status_internal();
}

void cm_mgr_enable_fn(int enable)
{
	cm_mgr_enable = enable;
	if (!cm_mgr_enable)
		cm_mgr_set_dram_level(0);
}

#define CPU_FW_FILE "cpu_data.bin"
#include <linux/firmware.h>
static struct device cm_mgr_device = {
	.init_name = "cm_mgr_device",
};

static void cm_mgr_update_fw(void)
{
	int j = 0;
	const struct firmware *fw = NULL;
	int err;
	int copy_size = 0;
	int offset = 0;
	int count;

	do {
		j++;
		pr_debug("try to request_firmware() %s - %d\n",
				CPU_FW_FILE, j);
		err = request_firmware(&fw, CPU_FW_FILE, &cm_mgr_device);
		if (err)
			pr_info("Failed to load %s, err = %d.\n",
					CPU_FW_FILE, err);
	} while (err == -EAGAIN && j < 5);
	if (err)
		pr_info("Failed to load %s, err = %d.\n",
				CPU_FW_FILE, err);

	if (!err) {
		pr_info("request_firmware() %s, size 0x%x\n",
				CPU_FW_FILE, (int)fw->size);
		update++;

		for (count = 0; count < CM_MGR_MAX; count++) {
			copy_size = vcore_power_array_size(count) *
				VCORE_ARRAY_SIZE * sizeof(unsigned int);
			pr_info("offset 0x%x, copy_size 0x%x\n",
					offset, copy_size);
			if (fw->size < (copy_size + offset)) {
				pr_info("vcore_power_gain_%d 0x%x, 0x%x",
						count, (int)fw->size,
						copy_size + offset);
				goto out_fw;
			}
			memcpy(vcore_power_gain_ptr(count),
					fw->data, copy_size);
		}

#ifdef PER_CPU_STALL_RATIO
		for (count = 0; count < CM_MGR_MAX; count++) {
			offset += copy_size;
			copy_size = sizeof(cpu_power_gain_UpLow0);
			pr_info("offset 0x%x, copy_size 0x%x\n",
					offset, copy_size);
			if (fw->size < (copy_size + offset)) {
				pr_info("cpu_power_gain_UpLow%d 0x%x, 0x%x",
						count,
						(int)fw->size,
						copy_size + offset);
				goto out_fw;
			}
			memcpy(_cpu_power_gain_ptr(1, 1, count),
					fw->data + offset, copy_size);

			offset += copy_size;
			copy_size = sizeof(cpu_power_gain_DownLow0);
			pr_info("offset 0x%x, copy_size 0x%x\n",
					offset, copy_size);
			if (fw->size < (copy_size + offset)) {
				pr_info("cpu_power_gain_DownLow%d 0x%x, 0x%x",
						count,
						(int)fw->size,
						copy_size + offset);
				goto out_fw;
			}
			memcpy(_cpu_power_gain_ptr(0, 1, count),
					fw->data + offset, copy_size);

			offset += copy_size;
			copy_size = sizeof(cpu_power_gain_UpHigh0);
			pr_info("offset 0x%x, copy_size 0x%x\n",
					offset, copy_size);
			if (fw->size < (copy_size + offset)) {
				pr_info("cpu_power_gain_UpHigh%d 0x%x, 0x%x",
						count,
						(int)fw->size,
						copy_size + offset);
				goto out_fw;
			}
			memcpy(_cpu_power_gain_ptr(1, 0, count),
					fw->data + offset, copy_size);

			offset += copy_size;
			copy_size = sizeof(cpu_power_gain_DownHigh0);
			pr_info("offset 0x%x, copy_size 0x%x\n",
					offset, copy_size);
			if (fw->size < (copy_size + offset)) {
				pr_info("cpu_power_gain_DownHigh%d 0x%x, 0x%x",
						count,
						(int)fw->size,
						copy_size + offset);
				goto out_fw;
			}
			memcpy(_cpu_power_gain_ptr(0, 0, count),
					fw->data + offset, copy_size);
		}
#endif /* PER_CPU_STALL_RATIO */

		offset += copy_size;
		copy_size = sizeof(_v2f_all);
		pr_info("offset 0x%x, copy_size 0x%x\n", offset, copy_size);
		if (fw->size < (copy_size + offset)) {
			pr_info("_v2f_all 0x%x, 0x%x",
					(int)fw->size, copy_size + offset);
			goto out_fw;
		}
		memcpy(&_v2f_all, fw->data + offset, copy_size);

		release_firmware(fw);
		fw = NULL;
	}
out_fw:
	if (fw)
		release_firmware(fw);
}
	
MODULE_FIRMWARE(CPU_FW_FILE);

int __weak cm_mgr_platform_init(void)
{

	return 0;
}

int __init cm_mgr_module_init(void)
{
	int r;
	UNUSED(cm_mgr_update_fw);
	
	r = device_register(&cm_mgr_device);
	if (r) {
		pr_info("FAILED TO CREATE DEVICE(%d)\n", r);
		return r;
	}

	/* SW Governor Report */
	spin_lock_init(&cm_mgr_lock);

	r = cm_mgr_platform_init();
	if (r) {
		pr_info("FAILED TO INIT PLATFORM(%d)\n", r);
		return r;
	}

	vcore_power_gain = vcore_power_gain_ptr(cm_mgr_get_idx());

	init_timer_deferrable(&cm_mgr_perf_timer);
	cm_mgr_perf_timer.function = cm_mgr_perf_timer_fn;
	cm_mgr_perf_timer.data = 0;
	return 0;
}

late_initcall(cm_mgr_module_init);

MODULE_DESCRIPTION("CM Manager Driver v0.1");
