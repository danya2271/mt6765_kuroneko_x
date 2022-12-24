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
#include <linux/cpu.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/pm_qos.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/tick.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/delay.h>

#include <mtk_cpuidle.h>
#include <mtk_idle.h>

#include <mtk_mcdi.h>
#include <mtk_mcdi_governor.h>
#include <mtk_mcdi_profile.h>
#include <mtk_mcdi_util.h>
#include <mtk_mcdi_cpc.h>

#include <mtk_mcdi_plat.h>
#include <mtk_mcdi_reg.h>
#include <mtk_mcdi_state.h>
#include <mtk_mcdi_api.h>

#include <mtk_mcdi_governor_hint.h>

#include <linux/irqchip/mtk-gic-extend.h>

#define MCDI_DEBUG_INFO_MAGIC_NUM           0x1eef9487
#define MCDI_DEBUG_INFO_NON_REPLACE_OFFSET  0x0008

static unsigned long mcdi_cnt_wfi[NF_CPU];
static unsigned long mcdi_cnt_cpu[NF_CPU];
static unsigned long mcdi_cnt_cluster[NF_CLUSTER];

void __iomem *mcdi_sysram_base;
#define MCDI_SYSRAM (mcdi_sysram_base + MCDI_DEBUG_INFO_NON_REPLACE_OFFSET)

static unsigned long mcdi_cnt_cpu_last[NF_CPU];
static unsigned long mcdi_cnt_cluster_last[NF_CLUSTER];

static unsigned long ac_cpu_cond_info_last[NF_ANY_CORE_CPU_COND_INFO];

static const char *ac_cpu_cond_name[NF_ANY_CORE_CPU_COND_INFO] = {
	"pause",
	"multi core",
	"latency",
	"residency",
	"last core"
};

static unsigned long long mcdi_heart_beat_log_prev;
static DEFINE_SPINLOCK(mcdi_heart_beat_spin_lock);

static unsigned int mcdi_heart_beat_log_dump_thd = 5000;          /* 5 sec */

static bool mcdi_stress_en;
static unsigned int mcdi_stress_us = 10 * 1000;
static struct task_struct *mcdi_stress_tsk[NF_CPU];

int __attribute__((weak)) mtk_enter_idle_state(int mode)
{
	return 0;
}

int __attribute__((weak)) soidle_enter(int cpu)
{
	return 1;
}

int __attribute__((weak)) dpidle_enter(int cpu)
{
	return 1;
}

int __attribute__((weak)) soidle3_enter(int cpu)
{
	return 1;
}

unsigned long long __attribute__((weak)) idle_get_current_time_ms(void)
{
	return 0;
}

void __attribute__((weak)) aee_rr_rec_mcdi_val(int id, u32 val)
{
}

void __attribute__((weak)) mtk_idle_dump_cnt_in_interval(void)
{
}

void __attribute__((weak))
mcdi_set_state_lat(int cpu_type, int state, unsigned int val)
{
}

void __attribute__((weak))
mcdi_set_state_res(int cpu_type, int state, unsigned int val)
{
}

void wakeup_all_cpu(void)
{
	int cpu = 0;

	/*
	 * smp_proccessor_id() will be called in the flow of
	 * smp_send_reschedule(), hence disable preemtion to
	 * avoid being scheduled out.
	 */
	preempt_disable();

	for (cpu = 0; cpu < NF_CPU; cpu++) {
		if (cpu_online(cpu))
			smp_send_reschedule(cpu);
	}

	preempt_enable();
}

void wait_until_all_cpu_powered_on(void)
{
	while (!(mcdi_get_gov_data_num_mcusys() == 0x0))
		;
}

void mcdi_wakeup_all_cpu(void)
{
	wakeup_all_cpu();

	wait_until_all_cpu_powered_on();
}

/* debugfs */
static char dbg_buf[4096] = { 0 };
static char cmd_buf[512] = { 0 };

/* procfs entry */
static const char mcdi_procfs_dir_name[] = "mcdi";
struct proc_dir_entry *mcdi_dir;
static int mcdi_procfs_init(void)
{
	mcdi_dir = proc_mkdir(mcdi_procfs_dir_name, NULL);

	if (!mcdi_dir) {
		pr_notice("fail to create /proc/mcdi @ %s()\n", __func__);
		return -ENOMEM;
	}

	return 0;
}

static void __go_to_wfi(int cpu)
{
	remove_cpu_from_prefer_schedule_domain(cpu);

	isb();
	/* memory barrier before WFI */
	mb();
	wfi();

	add_cpu_to_prefer_schedule_domain(cpu);
}

void mcdi_heart_beat_log_dump(void)
{
	static struct mtk_mcdi_buf buf;
	int i;
	unsigned long long mcdi_heart_beat_log_curr = 0;
	unsigned long flags;
	bool dump_log = false;
	unsigned long mcdi_cnt;
	unsigned long any_core_info = 0;
	unsigned long ac_cpu_cond_info[NF_ANY_CORE_CPU_COND_INFO] = {0};
	unsigned int cpu_mask = 0;
	unsigned int cluster_mask = 0;
	struct mcdi_feature_status feature_stat;

	spin_lock_irqsave(&mcdi_heart_beat_spin_lock, flags);

	mcdi_heart_beat_log_curr = idle_get_current_time_ms();

	if (mcdi_heart_beat_log_prev == 0)
		mcdi_heart_beat_log_prev = mcdi_heart_beat_log_curr;

	if ((mcdi_heart_beat_log_curr - mcdi_heart_beat_log_prev)
			> mcdi_heart_beat_log_dump_thd) {
		dump_log = true;
		mcdi_heart_beat_log_prev = mcdi_heart_beat_log_curr;
	}

	spin_unlock_irqrestore(&mcdi_heart_beat_spin_lock, flags);

	if (!dump_log)
		return;

	reset_mcdi_buf(buf);

	mcdi_buf_append(buf, "mcdi cpu: ");

	for (i = 0; i < NF_CPU; i++) {
		mcdi_cnt = mcdi_cnt_cpu[i] - mcdi_cnt_cpu_last[i];
		mcdi_buf_append(buf, "%lu, ", mcdi_cnt);
		mcdi_cnt_cpu_last[i] = mcdi_cnt_cpu[i];
	}

	mcdi_buf_append(buf, "cluster : ");

	for (i = 0; i < NF_CLUSTER; i++) {
		mcdi_cnt_cluster[i] = mcdi_get_cluster_off_cnt(i);

		mcdi_cnt = mcdi_cnt_cluster[i] - mcdi_cnt_cluster_last[i];
		mcdi_buf_append(buf, "%lu, ", mcdi_cnt);

		mcdi_cnt_cluster_last[i] = mcdi_cnt_cluster[i];
	}

	any_core_cpu_cond_get(ac_cpu_cond_info);

	for (i = 0; i < NF_ANY_CORE_CPU_COND_INFO; i++) {
		any_core_info =
			ac_cpu_cond_info[i] - ac_cpu_cond_info_last[i];
		mcdi_buf_append(buf, "%s = %lu, ",
			ac_cpu_cond_name[i], any_core_info);
		ac_cpu_cond_info_last[i] = ac_cpu_cond_info[i];
	}

	get_mcdi_avail_mask(&cpu_mask, &cluster_mask);

	mcdi_buf_append(buf, "avail cpu = %04x, cluster = %04x",
		cpu_mask, cluster_mask);

	get_mcdi_feature_status(&feature_stat);

	mcdi_buf_append(buf, ", enabled = %d, max_s_state = %d",
						feature_stat.enable,
						feature_stat.s_state);

	mcdi_buf_append(buf, ", system_idle_hint = %08x",
						system_idle_hint_result_raw());

	pr_info("%s\n", get_mcdi_buf(buf));
}

int wfi_enter(int cpu)
{
	idle_refcnt_inc();

	set_mcdi_idle_state(cpu, MCDI_STATE_WFI);

	mcdi_usage_time_start(cpu);

	__go_to_wfi(cpu);

	mcdi_usage_time_stop(cpu);

	idle_refcnt_dec();

	mcdi_cnt_wfi[cpu]++;

	mcdi_usage_calc(cpu);

	return 0;
}

int mcdi_enter(int cpu)
{
	int cluster_idx = cluster_idx_get(cpu);
	int state = -1;
	struct cpuidle_state *mcdi_sta;

	/* Note: [DVT] Enter mtk idle state w/o mcdi enable
	 * Include mtk_idle.h for MTK_IDLE_DVT_TEST_ONLY
	 */
	#if defined(MTK_IDLE_DVT_TEST_ONLY)
	mtk_idle_enter_dvt(cpu);
	return 0;
	#endif

	mcdi_profile_ts(cpu, MCDI_PROFILE_ENTER);

	idle_refcnt_inc();

	if (likely(mcdi_fw_is_ready()))
		state = mcdi_governor_select(cpu, cluster_idx);
	else
		state = MCDI_STATE_WFI;

	if (state >= MCDI_STATE_WFI && state <= MCDI_STATE_CLUSTER_OFF) {
		mcdi_sta = &(mcdi_state_tbl_get(cpu)->states[state]);
		sched_idle_set_state(mcdi_sta, state);
	}

	set_mcdi_idle_state(cpu, state);

	mcdi_profile_ts(cpu, MCDI_PROFILE_CPU_DORMANT_ENTER);

	mcdi_usage_time_start(cpu);

	switch (state) {
	case MCDI_STATE_WFI:
		__go_to_wfi(cpu);

		break;
	case MCDI_STATE_CPU_OFF:

		aee_rr_rec_mcdi_val(cpu, MCDI_STATE_CPU_OFF << 16 | 0xff);

		mtk_enter_idle_state(MTK_MCDI_CPU_MODE);

		aee_rr_rec_mcdi_val(cpu, 0x0);

		mcdi_cnt_cpu[cpu]++;

		break;
	case MCDI_STATE_CLUSTER_OFF:

		aee_rr_rec_mcdi_val(cpu, MCDI_STATE_CLUSTER_OFF << 16 | 0xff);

		mtk_enter_idle_state(MTK_MCDI_CLUSTER_MODE);

		aee_rr_rec_mcdi_val(cpu, 0x0);

		mcdi_cnt_cpu[cpu]++;

		break;
	case MCDI_STATE_SODI:

		soidle_enter(cpu);

		break;
	case MCDI_STATE_DPIDLE:

		dpidle_enter(cpu);

		break;
	case MCDI_STATE_SODI3:

		soidle3_enter(cpu);

		break;
	}

	mcdi_usage_time_stop(cpu);

	mcdi_profile_ts(cpu, MCDI_PROFILE_CPU_DORMANT_LEAVE);

	mcdi_usage_calc(cpu);

	if (state >= MCDI_STATE_WFI && state <= MCDI_STATE_CLUSTER_OFF)
		sched_idle_set_state(NULL, -1);

	mcdi_governor_reflect(cpu, state);

	idle_refcnt_dec();

	mcdi_profile_ts(cpu, MCDI_PROFILE_LEAVE);
	mcdi_profile_calc(cpu);

	return 0;
}

bool __mcdi_pause(unsigned int id, bool paused)
{
	mcdi_state_pause(id, paused);

	if (!(get_mcdi_feature_stat()->enable))
		return true;

	if (!mcdi_get_boot_time_check())
		return true;

	if (paused)
		mcdi_wakeup_all_cpu();

	return true;
}

bool _mcdi_task_pause(bool paused)
{
	if (!is_mcdi_working())
		return false;

	if (paused) {

		/* Notify SSPM to disable MCDI */
		mcdi_mbox_write(MCDI_MBOX_PAUSE_ACTION, 1);

		/* Polling until MCDI Task stopped */
		while (!(mcdi_mbox_read(MCDI_MBOX_PAUSE_ACK) == 1))
			;
	} else {
		/* Notify SSPM to enable MCDI */
		mcdi_mbox_write(MCDI_MBOX_PAUSE_ACTION, 0);
	}

	return true;
}

void mcdi_avail_cpu_mask(unsigned int cpu_mask)
{
	mcdi_mbox_write(MCDI_MBOX_AVAIL_CPU_MASK, cpu_mask);
}

/* Disable MCDI during cpu_up/cpu_down period */
static int mcdi_cpu_callback(struct notifier_block *nfb,
				   unsigned long action, void *hcpu)
{
	switch (action) {
	case CPU_UP_PREPARE:
	case CPU_UP_PREPARE_FROZEN:
	case CPU_DOWN_PREPARE:
	case CPU_DOWN_PREPARE_FROZEN:
		__mcdi_pause(MCDI_PAUSE_BY_HOTPLUG, true);
		break;
	}

	return NOTIFY_OK;
}

static int mcdi_cpu_callback_leave_hotplug(struct notifier_block *nfb,
				   unsigned long action, void *hcpu)
{
	switch (action) {
	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
	case CPU_UP_CANCELED:
	case CPU_UP_CANCELED_FROZEN:
	case CPU_DOWN_FAILED:
	case CPU_DOWN_FAILED_FROZEN:
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		mcdi_avail_cpu_cluster_update();

		__mcdi_pause(MCDI_PAUSE_BY_HOTPLUG, false);

		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block mcdi_cpu_notifier = {
	.notifier_call = mcdi_cpu_callback,
	.priority   = INT_MAX,
};

static struct notifier_block mcdi_cpu_notifier_leave_hotplug = {
	.notifier_call = mcdi_cpu_callback_leave_hotplug,
	.priority   = INT_MIN,
};

static int mcdi_hotplug_cb_init(void)
{
	register_cpu_notifier(&mcdi_cpu_notifier);
	register_cpu_notifier(&mcdi_cpu_notifier_leave_hotplug);

	return 0;
}

static void __init mcdi_pm_qos_init(void)
{
}

static int __init mcdi_sysram_init(void)
{
	/* of init */
	mcdi_of_init(&mcdi_sysram_base);

	if (!mcdi_sysram_base)
		return -1;

	memset_io((void __iomem *)MCDI_SYSRAM,
		0,
		MCDI_SYSRAM_SIZE - MCDI_DEBUG_INFO_NON_REPLACE_OFFSET);

	return 0;
}

subsys_initcall(mcdi_sysram_init);


static int __init mcdi_init(void)
{
	/* Activate MCDI after SMP */
	pr_info("mcdi_init\n");

	/* Register CPU up/down callbacks */
	mcdi_hotplug_cb_init();

	/* procfs init */
	mcdi_procfs_init();

	/* CPC init */
	mcdi_cpc_init();

	/* MCDI governor init */
	mcdi_governor_init();

	mcdi_pm_qos_init();

	mcdi_cpu_iso_mask(0x0);

	mcdi_prof_init();

	return 0;
}

late_initcall(mcdi_init);
