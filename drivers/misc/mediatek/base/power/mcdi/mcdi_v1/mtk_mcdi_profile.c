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

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/tick.h>
#include <linux/uaccess.h>

#include <mtk_mcdi.h>
#include <mtk_mcdi_governor.h>
#include <mtk_mcdi_profile.h>
#include <mtk_mcdi_util.h>

#include <mtk_mcdi_plat.h>
#include <mtk_mcdi_state.h>
#include <mtk_mcdi_reg.h>

/* default profile MCDI_CPU_OFF */
static int profile_state = -1;

static DEFINE_SPINLOCK(mcdi_prof_spin_lock);

static struct mcdi_prof_lat mcdi_lat = {
	.enable = false,
	.pause  = false,
	.section[MCDI_PROFILE_CPU_DORMANT_ENTER] = {
		.name	= "mcdi enter",
		.start	= MCDI_PROFILE_ENTER,
		.end	= MCDI_PROFILE_CPU_DORMANT_ENTER,
	},
	.section[MCDI_PROFILE_LEAVE] = {
		.name	= "mcdi leave",
		.start	= MCDI_PROFILE_CPU_DORMANT_LEAVE,
		.end	= MCDI_PROFILE_LEAVE,
	},
#if 0
	/**
	 * Profiling specific section:
	 *    1. Add MCDI_PROFILE_XXX in mtk_mcdi_profile.h
	 *    2. Need to set member information:
	 *           'name', 'start' and 'end' are necessary.
	 *    3. Put mcdi_profile_ts(cpu, MCDI_PROFILE_XXX)
	 */
	.section[MCDI_PROFILE_RSV1] = {
		.name	= "Test section",
		.start	= MCDI_PROFILE_RSV0,
		.end	= MCDI_PROFILE_RSV1,
	},
#endif
};

static struct mcdi_prof_usage mcdi_usage;

#ifdef MCDI_PWR_SEQ_PROF_BREAKDOWN
const char *prof_pwr_seq_item[MCDI_PROF_BK_NUM] = {
	"cluster",
	"cpu",
	"armpll",
	"buck",
};
#endif

void mcdi_prof_set_idle_state(int cpu, int state)
{
	mcdi_usage.dev[cpu].actual_state = state;
}

static void set_mcdi_profile_sampling(int en)
{
	struct mcdi_prof_lat_raw *curr;
	struct mcdi_prof_lat_raw *res;
	unsigned long flags;
	int i, j;

	if (mcdi_lat.enable == en)
		return;

	mcdi_lat.pause = true;
	mcdi_lat.enable = en;

	spin_lock_irqsave(&mcdi_prof_spin_lock, flags);

	if (!en) {
		for (i = 0; i < NF_MCDI_PROFILE; i++) {
			for (j = 0; j < NF_CPU_TYPE; j++) {

				curr = &mcdi_lat.section[i].curr[j];
				res = &mcdi_lat.section[i].result[j];

				res->avg = curr->cnt ?
					div64_u64(curr->sum, curr->cnt) : 0;
				res->max = curr->max;
				res->cnt = curr->cnt;

				memset(curr, 0,
					sizeof(struct mcdi_prof_lat_raw));
			}

			for (j = 0; j < NF_CPU; j++)
				mcdi_lat.section[i].ts[j] = 0;
		}
	}

	spin_unlock_irqrestore(&mcdi_prof_spin_lock, flags);

	mcdi_lat.pause = false;
}

void mcdi_prof_core_cluster_off_token(int cpu)
{
	unsigned long flags;

	if (cpu_is_invalid(cpu))
		return;

	spin_lock_irqsave(&mcdi_prof_spin_lock, flags);

	mcdi_usage.last_id[cluster_idx_get(cpu)] = cpu;

	spin_unlock_irqrestore(&mcdi_prof_spin_lock, flags);
}

static void mcdi_usage_save(struct mcdi_prof_dev *dev, int entered_state,
		unsigned long long enter_ts, unsigned long long leave_ts)
{
	int cpu_idx, cluster;
	unsigned long flags;
	s64 diff;

	diff = (s64)div64_u64(leave_ts - enter_ts, 1000);

	if (diff > INT_MAX)
		diff = INT_MAX;

	dev->last_residency = (int) diff;

	if (entered_state > MCDI_STATE_CPU_OFF)
		dev->state[MCDI_STATE_CPU_OFF].dur += dev->last_residency;
	if (entered_state > MCDI_STATE_CLUSTER_OFF)
		dev->state[MCDI_STATE_CLUSTER_OFF].dur += dev->last_residency;

	if (entered_state == MCDI_STATE_CLUSTER_OFF) {

		spin_lock_irqsave(&mcdi_prof_spin_lock, flags);

		cluster = cluster_idx_get(dev->cpu);
		cpu_idx = mcdi_usage.last_id[cluster];

		if (!cpu_is_invalid(cpu_idx)) {

			enter_ts = mcdi_usage.dev[cpu_idx].enter;

			if (enter_ts > mcdi_usage.start)
				diff = (s64)div64_u64(
						leave_ts - enter_ts, 1000);
			else
				diff = 0;

			dev->state[MCDI_STATE_CLUSTER_OFF].dur += (int) diff;

			mcdi_usage.last_id[cluster] = -1;
		}

		spin_unlock_irqrestore(&mcdi_prof_spin_lock, flags);

	} else {
		dev->state[entered_state].dur += dev->last_residency;
	}
}

static void mcdi_usage_enable(int en)
{
	struct mcdi_prof_dev *dev;
	int i, j;

	if (mcdi_usage.enable == en)
		return;

	mcdi_usage.enable = en;

	if (en) {

		for (i = 0; i < NF_CPU; i++) {

			dev = &mcdi_usage.dev[i];

			dev->enter = 0;
			dev->leave = 0;

			for (j = 0; j < NF_MCDI_STATE; j++)
				dev->state[j].dur = 0;
		}

		mcdi_usage.start = sched_clock();

	} else {

		mcdi_usage.prof_dur = div64_u64(
				sched_clock() - mcdi_usage.start, 1000);

		for (i = 0; i < NF_CPU; i++) {

			dev = &mcdi_usage.dev[i];

			if (dev->actual_state < 0
				|| dev->actual_state > MCDI_STATE_CPU_OFF)
				continue;

			if (dev->enter > dev->leave)
				mcdi_usage_save(dev, dev->actual_state,
					dev->enter, sched_clock());
		}

	}
}

static inline bool mcdi_usage_may_never_wakeup(int cpu)
{
	return is_mcdi_working() && mcdi_usage_cpu_valid(cpu);
}

unsigned int mcdi_usage_get_cnt(int cpu, int state_idx)
{
	if (cpu_is_invalid(cpu) || state_idx < 0)
		return 0;

	return mcdi_usage.dev[cpu].state[state_idx].cnt;
}

void mcdi_usage_time_start(int cpu)
{
	if (!mcdi_usage.enable)
		return;

	mcdi_usage.dev[cpu].enter = sched_clock();
}

void mcdi_usage_time_stop(int cpu)
{
	if (!mcdi_usage.enable)
		return;

	mcdi_usage.dev[cpu].leave = sched_clock();
}

void mcdi_usage_calc(int cpu)
{
	int entered_state;
	struct mcdi_prof_dev *dev = &mcdi_usage.dev[cpu];
	unsigned long long leave_ts, enter_ts;

	entered_state = dev->actual_state;

	dev->state[entered_state].cnt++;
	dev->last_state_idx = entered_state;
	dev->actual_state = -1;

	if (!mcdi_usage.enable)
		return;

	leave_ts = dev->leave;
	enter_ts = dev->enter;

	if (dev->leave && !dev->enter)
		enter_ts = mcdi_usage.start;
	else if (!dev->leave && dev->enter)
		leave_ts = sched_clock();

	mcdi_usage_save(dev, entered_state, enter_ts, leave_ts);
}

static bool mcdi_profile_matched_state(int cpu)
{
	if (profile_state < 0)
		return true;

	/* Idle state was saved to last_state_idx in mcdi_usage_calc() */
	return profile_state == mcdi_usage.dev[cpu].last_state_idx;
}

void mcdi_profile_ts(int cpu_idx, unsigned int prof_idx)
{
	if (!mcdi_lat.enable)
		return;

	if (unlikely(prof_idx >= NF_MCDI_PROFILE))
		return;

	mcdi_lat.section[prof_idx].ts[cpu_idx] = sched_clock();
}

void mcdi_profile_calc(int cpu)
{
	int cpu_type;
	struct mcdi_prof_lat_data *data;
	struct mcdi_prof_lat_raw *raw;
	unsigned long long start;
	unsigned long long end;
	unsigned int dur;
	unsigned long flags;
	int i = 0;

	if (!mcdi_lat.enable || mcdi_lat.pause)
		return;

	if (unlikely(cpu_is_invalid(cpu)))
		return;

	if (!mcdi_profile_matched_state(cpu))
		return;

	cpu_type = cpu_type_idx_get(cpu);

	spin_lock_irqsave(&mcdi_prof_spin_lock, flags);

	for (i = 0; i < NF_MCDI_PROFILE; i++) {

		data = &mcdi_lat.section[i];

		if (data->valid == false)
			continue;

		raw = &data->curr[cpu_type];

		start = data->start_ts[cpu];
		end = data->end_ts[cpu];

		if (unlikely(start == 0))
			continue;

		dur = (unsigned int)((end - start) & 0xFFFFFFFF);

		if ((dur & BIT(31)))
			continue;

		if (raw->max < dur)
			raw->max = dur;

		raw->sum += dur;
		raw->cnt++;
	}

	spin_unlock_irqrestore(&mcdi_prof_spin_lock, flags);
}

/* debugfs */
static char dbg_buf[4096] = { 0 };
static char cmd_buf[512] = { 0 };

bool mcdi_usage_cpu_valid(int cpu)
{
	return (mcdi_usage.cpu_valid & (1 << cpu));
}

void mcdi_procfs_profile_init(struct proc_dir_entry *mcdi_dir)
{
}

void mcdi_prof_init(void)
{
	struct mcdi_prof_lat_data *data;
	int i;

	for (i = 0; i < (NF_MCDI_PROFILE); i++) {

		data = &mcdi_lat.section[i];
		data->valid = false;

		if ((data->start == data->end) || !data->name)
			continue;

		if (data->start >= 0 && data->start < NF_MCDI_PROFILE
			&& data->end >= 0 && data->end < NF_MCDI_PROFILE) {

			data->valid = true;
			data->start_ts = mcdi_lat.section[data->start].ts;
			data->end_ts = mcdi_lat.section[data->end].ts;
		}
	}

	for (i = 0; i < NF_CLUSTER; i++)
		mcdi_usage.last_id[i] = -1;

	for (i = 0; i < NF_CPU; i++)
		mcdi_usage.dev[i].cpu = i;

	mcdi_usage.cpu_valid = (1 << NF_CPU) - 1;
}
