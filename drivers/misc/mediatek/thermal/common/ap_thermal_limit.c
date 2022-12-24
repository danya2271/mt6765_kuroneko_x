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
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <mach/mtk_thermal.h> /* needed by tscpu_settings.h */
#include <tscpu_settings.h> /* needed by tscpu_warn */
#include <ap_thermal_limit.h>
#include <mt-plat/aee.h>


#if defined(ATM_USES_PPM)
#include "mtk_ppm_api.h"
#else
#include "mt_cpufreq.h"
#endif
#if defined(THERMAL_VPU_SUPPORT)
#if defined(CONFIG_MTK_VPU_SUPPORT)
#include "vpu_dvfs.h"
#endif
#endif
#if defined(THERMAL_MDLA_SUPPORT)
#if defined(CONFIG_MTK_MDLA_SUPPORT)
#include "mdla_dvfs.h"
#endif
#endif

/*=============================================================
 * Local variable definition
 *=============================================================
 */
#define AP_THERMO_LMT_MAX_USERS				(4)

/*=============================================================
 * Local variable definition
 *=============================================================
 */
static unsigned int apthermolmt_prev_cpu_pwr_lim;
static unsigned int apthermolmt_curr_cpu_pwr_lim = 0x7FFFFFFF;
#if defined(THERMAL_VPU_SUPPORT)
static unsigned int apthermolmt_prev_vpu_pwr_lim;
static unsigned int apthermolmt_curr_vpu_pwr_lim = 0x7FFFFFFF;
#endif
#if defined(THERMAL_MDLA_SUPPORT)
static unsigned int apthermolmt_prev_mdla_pwr_lim;
static unsigned int apthermolmt_curr_mdla_pwr_lim = 0x7FFFFFFF;
#endif
static unsigned int apthermolmt_prev_gpu_pwr_lim;
static unsigned int apthermolmt_curr_gpu_pwr_lim = 0x7FFFFFFF;

static struct apthermolmt_user _dummy = {
	.log = "dummy ",
	.cpu_limit = 0x7FFFFFFF,
	.vpu_limit = 0x7FFFFFFF,
	.mdla_limit = 0x7FFFFFFF,
	.gpu_limit = 0x7FFFFFFF,
	.ptr = &_dummy
};

static struct apthermolmt_user _gp = {
	.log = "set_gp_power ",
	.cpu_limit = 0x7FFFFFFF,
	.vpu_limit = 0x7FFFFFFF,
	.mdla_limit = 0x7FFFFFFF,
	.gpu_limit = 0x7FFFFFFF,
	.ptr = &_gp
};
static struct apthermolmt_user *_users[AP_THERMO_LMT_MAX_USERS] = {
			&_gp, &_dummy, &_dummy, &_dummy};

static unsigned int gp_prev_cpu_pwr_limit;
static unsigned int gp_curr_cpu_pwr_limit;
static unsigned int gp_prev_gpu_pwr_limit;
static unsigned int gp_curr_gpu_pwr_limit;

static DEFINE_MUTEX(apthermolmt_cpu_mutex);

/*=============================================================
 * Weak functions
 *=============================================================
 */
#if defined(ATM_USES_PPM)
	void __attribute__ ((weak))
mt_ppm_cpu_thermal_protect(unsigned int limited_power)
{
	pr_notice(TSCPU_LOG_TAG "E_WF: %s doesn't exist\n", __func__);
}
#else
	void __attribute__ ((weak))
mt_cpufreq_thermal_protect(unsigned int limited_power)
{
	pr_notice(TSCPU_LOG_TAG "E_WF: %s doesn't exist\n", __func__);
}
#endif


	void __attribute__ ((weak))
mt_gpufreq_thermal_protect(unsigned int limited_power)
{
	pr_notice(TSCPU_LOG_TAG "E_WF: %s doesn't exist\n", __func__);
}


/*=============================================================
 * Local function prototype
 *=============================================================
 */

/*=============================================================
 * Function definitions
 *=============================================================
 */
int apthermolmt_register_user(struct apthermolmt_user *handle, char *log)
{
	int i = 1;

	if (!handle || !log)
		return -1;

	for (; i < AP_THERMO_LMT_MAX_USERS; i++) {
		if (_users[i] == &_dummy) {
			_users[i] = handle;
			handle->log = log;
			handle->cpu_limit = 0x7FFFFFFF;
			handle->vpu_limit = 0x7FFFFFFF;
			handle->mdla_limit = 0x7FFFFFFF;
			handle->gpu_limit = 0x7FFFFFFF;
			handle->ptr = &_users[i];
			return 0;
		}
	}

	return -1;
}
EXPORT_SYMBOL(apthermolmt_register_user);

int apthermolmt_unregister_user(struct apthermolmt_user *handle)
{
	int i = 1;

	if (!handle)
		return -1;

	for (; i < AP_THERMO_LMT_MAX_USERS; i++) {
		if (handle->ptr == &_users[i]) {
			_users[i] = &_dummy;
			handle->ptr = NULL;
			return 0;
		}
	}

	return -1;
}
EXPORT_SYMBOL(apthermolmt_unregister_user);

void apthermolmt_set_cpu_power_limit(
struct apthermolmt_user *handle, unsigned int limit)
{
}
EXPORT_SYMBOL(apthermolmt_set_cpu_power_limit);

#if defined(THERMAL_VPU_SUPPORT)
void apthermolmt_set_vpu_power_limit(
struct apthermolmt_user *handle, unsigned int limit)
{
}
EXPORT_SYMBOL(apthermolmt_set_vpu_power_limit);
#endif

#if defined(THERMAL_MDLA_SUPPORT)
void apthermolmt_set_mdla_power_limit(
struct apthermolmt_user *handle, unsigned int limit)
{
}
EXPORT_SYMBOL(apthermolmt_set_mdla_power_limit);
#endif

void apthermolmt_set_gpu_power_limit(
struct apthermolmt_user *handle, unsigned int limit)
{
}
EXPORT_SYMBOL(apthermolmt_set_gpu_power_limit);

void apthermolmt_set_general_cpu_power_limit(unsigned int limit)
{
}
EXPORT_SYMBOL(apthermolmt_set_general_cpu_power_limit);

void apthermolmt_set_general_gpu_power_limit(unsigned int limit)
{
}
EXPORT_SYMBOL(apthermolmt_set_general_gpu_power_limit);

unsigned int apthermolmt_get_cpu_power_limit(void)
{
	return apthermolmt_curr_cpu_pwr_lim;
}
EXPORT_SYMBOL(apthermolmt_get_cpu_power_limit);

unsigned int apthermolmt_get_cpu_min_power(void)
{
	return tscpu_get_min_cpu_pwr();
}
EXPORT_SYMBOL(apthermolmt_get_cpu_min_power);

unsigned int apthermolmt_get_gpu_power_limit(void)
{
	return apthermolmt_curr_gpu_pwr_lim;
}
EXPORT_SYMBOL(apthermolmt_get_gpu_power_limit);

unsigned int apthermolmt_get_gpu_min_power(void)
{
	return tscpu_get_min_gpu_pwr();
}
EXPORT_SYMBOL(apthermolmt_get_gpu_min_power);

#if defined(THERMAL_VPU_SUPPORT)
unsigned int apthermolmt_get_vpu_power_limit(void)
{
	return apthermolmt_curr_vpu_pwr_lim;
}
EXPORT_SYMBOL(apthermolmt_get_vpu_power_limit);

unsigned int apthermolmt_get_vpu_min_power(void)
{
	return tscpu_get_min_vpu_pwr();
}
EXPORT_SYMBOL(apthermolmt_get_vpu_min_power);
#endif

#if defined(THERMAL_MDLA_SUPPORT)
unsigned int apthermolmt_get_mdla_power_limit(void)
{
	return apthermolmt_curr_mdla_pwr_lim;
}
EXPORT_SYMBOL(apthermolmt_get_mdla_power_limit);

unsigned int apthermolmt_get_mdla_min_power(void)
{
	return tscpu_get_min_mdla_pwr();
}
EXPORT_SYMBOL(apthermolmt_get_mdla_min_power);
#endif


