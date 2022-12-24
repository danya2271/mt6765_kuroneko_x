/*
 * Copyright (C) 2015 MediaTek Inc.
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

#include <linux/module.h>
#include <linux/sched.h>
#include "mtk_mfgsys.h"
#ifndef MTK_BRINGUP
#include <mtk_gpufreq.h>
#endif

#include <mtk_boot.h>
#include "ospvr_gputrace.h"
#include "rgxdevice.h"
#include "osfunc.h"
#include "pvrsrv.h"
#include "rgxhwperf.h"
#include "device.h"
#include "rgxinit.h"
#include "pvr_dvfs.h"

#include "ged_dvfs.h"
#include "ged_log.h"
#include "ged_base.h"

#include "ged_fdvfs.h"

#include <linux/platform_device.h>
#include <linux/clk.h>

#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>


#define mt_gpufreq_get_frequency_by_level mt_gpufreq_get_freq_by_idx

/* MTK: disable 6795 DVFS temporarily, fix and remove me */

#ifdef CONFIG_MTK_HIBERNATION
#include "sysconfig.h"
#include <mach/mtk_hibernate_dpm.h>
#include <mach/mt_irq.h>
#include <mach/irqs.h>
#endif

#include <mtk_gpu_utility.h>
#include "mtk_mfg_counter.h"

#define mfg_readl(addr) readl(addr)
#define mfg_writel(val, addr) \
	do { writel(val, addr); wmb(); } while (0) /* sync_write */

void __iomem *topck_base;
#define TOPCK_CLK2 (topck_base + 0x0120)

#ifndef MTK_BRINGUP
#define MTK_GPU_DVFS					1
#endif
#define MTK_DEFER_DVFS_WORK_MS		  9000
#define MTK_DVFS_SWITCH_INTERVAL_MS	 1
#define MTK_SYS_BOOST_DURATION_MS	   350
#define MTK_WAIT_FW_RESPONSE_TIMEOUT_US 300
#define MTK_GPIO_REG_OFFSET			 0x30
#define MTK_GPU_FREQ_ID_INVALID		 0xFFFFFFFF
#define MTK_RGX_DEVICE_INDEX_INVALID	-1

static IMG_HANDLE g_RGXutilUser;

static IMG_CPU_PHYADDR gsRegsPBase;


static void *g_pvRegsKM;
#ifdef MTK_CAL_POWER_INDEX
static void *g_pvRegsBaseKM;
#endif

static IMG_BOOL g_bExit = IMG_TRUE;

static IMG_UINT32 gpu_power;
static IMG_UINT32 gpu_dvfs_enable;
static IMG_UINT32 gpu_debug_enable;

static IMG_UINT32 gpu_freq;

static IMG_BOOL g_bDeviceInit = IMG_FALSE;

static IMG_BOOL g_bUnsync = IMG_FALSE;
static IMG_UINT32 g_ui32_unsync_freq_id;


struct clk *mfg_clk_top;
struct clk *mfg_clk_off;
struct clk *mfg_clk_on;

struct clk *mtcmos_mfg0;
struct clk *mtcmos_mfg1;

unsigned int _mtk_ged_log;
unsigned int _track_ged_log;
unsigned int _mpu_ged_log;

#ifdef CONFIG_MTK_SEGMENT_TEST
static IMG_UINT32 efuse_mfg_enable;
#endif

#ifdef CONFIG_MTK_HIBERNATION
int gpu_pm_restore_noirq(struct device *device)
{
#if defined(MTK_CONFIG_OF) && defined(CONFIG_OF)
	int irq = MTKSysGetIRQ();
#else
	int irq = SYS_MTK_RGX_IRQ;
#endif
	mt_irq_set_sens(irq, MT_LEVEL_SENSITIVE);
	mt_irq_set_polarity(irq, MT_POLARITY_LOW);
	return 0;
}
#endif

static PVRSRV_DEVICE_NODE *MTKGetRGXDevNode(void)
{
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
	IMG_UINT32 i;

	if (psPVRSRVData) {
		for (i = 0; i < psPVRSRVData->ui32RegisteredDevices; i++) {
			PVRSRV_DEVICE_NODE *psDeviceNode =
			&psPVRSRVData->psDeviceNodeList[i];

			if (psDeviceNode && psDeviceNode->psDevConfig)
				return psDeviceNode;
		}
	}
	return NULL;
}

/* extern void ged_log_trace_counter(char *name, int count); */
static void MTKWriteBackFreqToRGX(PVRSRV_DEVICE_NODE *psDevNode,
		IMG_UINT32 ui32NewFreq)
{
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
	RGX_DATA *psRGXData = (RGX_DATA *)psDevNode->psDevConfig->hDevData;

	/* kHz to Hz write to RGX as the same unit */
	psRGXData->psRGXTimingInfo->ui32CoreClockSpeed = ui32NewFreq * 1000;
}


#ifndef MTK_BRINGUP
#define MTKCLK_prepare_enable(clk) \
	do { \
	if (clk) { \
	if (clk_prepare_enable(clk)) \
	pr_debug("PVR_K: clk_prepare_enable failed when enabling " #clk); } \
	} while (0)

#define MTKCLK_disable_unprepare(clk) \
	do { \
		if (clk) \
			clk_disable_unprepare(clk); \
	} while (0)

#define MTKCLK_set_parent(clkC, clkP) \
	do { \
	if (clkC && clkP) { \
	if (clk_set_parent(clkC, clkP)) \
	pr_debug("PVR_K: clk_set_parent failed when enable " #clkC, #clkP); } \
	} while (0)
#else
#define MTKCLK_prepare_enable(clk)

#define MTKCLK_disable_unprepare(clk)

#define MTKCLK_set_parent(clkC, clkP)
#endif

#ifdef MTK_MFGMTCMOS_AO
static bool isPowerOn;
#endif

static void MTKEnableMfgClock(IMG_BOOL bForce)
{
#ifdef MTK_GPU_DVFS
	ged_log_buf_print2(_mtk_ged_log, GED_LOG_ATTR_TIME, "BUCK_ON");
	mt_gpufreq_voltage_enable_set(BUCK_ON);
#endif

#ifdef MTCMOS_CONTROL
#ifndef MTK_USE_HW_APM
	mt_gpufreq_enable_MTCMOS(false);
#else
	mt_gpufreq_enable_MTCMOS(true);
#endif
#endif

	if (gpu_debug_enable)
		PVR_DPF((PVR_DBG_ERROR, "MTKEnableMfgClock"));

	ged_dvfs_gpu_clock_switch_notify(1);
	mtk_notify_gpu_power_change(1);
}

#define MFG_BUS_IDLE_BIT (1 << 16)

static void MTKDisableMfgClock(IMG_BOOL bForce)
{
	int buck_state;

	mtk_notify_gpu_power_change(0);
	ged_dvfs_gpu_clock_switch_notify(0);

#ifdef MTCMOS_CONTROL
#ifndef MTK_USE_HW_APM
	mt_gpufreq_disable_MTCMOS(false);
#else
	mt_gpufreq_disable_MTCMOS(true);
#endif
#endif

#ifdef MTK_GPU_DVFS
	buck_state = mt_gpufreq_voltage_enable_set(BUCK_OFF);
#endif
	ged_log_buf_print2(_mtk_ged_log, GED_LOG_ATTR_TIME, "BUCK_OFF");

	if (gpu_debug_enable)
		PVR_DPF((PVR_DBG_ERROR, "MTKDisableMfgClock"));
}

#if defined(MTK_USE_HW_APM)
static int MTKInitHWAPM(void)
{
	unsigned int regval;

	if (!g_pvRegsKM)
		g_pvRegsKM = OSMapPhysToLin(gsRegsPBase, 0x1000, 0);

	if (gpu_debug_enable) {
		PVR_DPF((PVR_DBG_ERROR, "g_pvRegsKM = 0x%p", g_pvRegsKM));
		PVR_DPF((PVR_DBG_ERROR, "LV0 *g_pvRegsKM = 0x%x",
		mfg_readl(g_pvRegsKM+0x01c)));
	}

	mfg_writel(0x01a80000, (g_pvRegsKM + 0x504));
	mfg_writel(0x00080010, (g_pvRegsKM + 0x508));
	mfg_writel(0x00100008, (g_pvRegsKM + 0x50c));
	mfg_writel(0x00b800c8, (g_pvRegsKM + 0x510));
	mfg_writel(0x00b000c0, (g_pvRegsKM + 0x514));
	mfg_writel(0x00c000c8, (g_pvRegsKM + 0x518));
	mfg_writel(0x00b000b8, (g_pvRegsKM + 0x51c));
	mfg_writel(0x00d000d0, (g_pvRegsKM + 0x520));
	mfg_writel(0x00d000d0, (g_pvRegsKM + 0x524));
	mfg_writel(0x00d00000, (g_pvRegsKM + 0x528));

	mfg_writel(0x9004001b, (g_pvRegsKM + 0x24));
	mfg_writel(0x8004001b, (g_pvRegsKM + 0x24));

	if (gpu_debug_enable) {
		PVR_DPF((PVR_DBG_ERROR, "HWAPM: *g_pvRegsKM+0x%x = 0x%x",
		(g_pvRegsKM + 0x504), mfg_readl(g_pvRegsKM + 0x504)));

		PVR_DPF((PVR_DBG_ERROR, "HWAPM: *g_pvRegsKM+0x%x = 0x%x",
		(g_pvRegsKM + 0x508), mfg_readl(g_pvRegsKM + 0x508)));

		PVR_DPF((PVR_DBG_ERROR, "HWAPM: *g_pvRegsKM+0x%x = 0x%x",
		(g_pvRegsKM + 0x50c), mfg_readl(g_pvRegsKM + 0x50c)));

		PVR_DPF((PVR_DBG_ERROR, "HWAPM: *g_pvRegsKM+0x%x = 0x%x",
		(g_pvRegsKM + 0x510), mfg_readl(g_pvRegsKM + 0x510)));

		PVR_DPF((PVR_DBG_ERROR, "HWAPM: *g_pvRegsKM+0x%x = 0x%x",
		(g_pvRegsKM + 0x514), mfg_readl(g_pvRegsKM + 0x514)));

		PVR_DPF((PVR_DBG_ERROR, "HWAPM: *g_pvRegsKM+0x%x = 0x%x",
		(g_pvRegsKM + 0x518), mfg_readl(g_pvRegsKM + 0x518)));

		PVR_DPF((PVR_DBG_ERROR, "HWAPM: *g_pvRegsKM+0x%x = 0x%x",
		(g_pvRegsKM + 0x51c), mfg_readl(g_pvRegsKM + 0x51c)));

		PVR_DPF((PVR_DBG_ERROR, "HWAPM: *g_pvRegsKM+0x%x = 0x%x",
		(g_pvRegsKM + 0x520), mfg_readl(g_pvRegsKM + 0x520)));

		PVR_DPF((PVR_DBG_ERROR, "HWAPM: *g_pvRegsKM+0x%x = 0x%x",
		(g_pvRegsKM + 0x524), mfg_readl(g_pvRegsKM + 0x524)));

		PVR_DPF((PVR_DBG_ERROR, "HWAPM: *g_pvRegsKM+0x%x = 0x%x",
		(g_pvRegsKM + 0x528), mfg_readl(g_pvRegsKM + 0x528)));

		PVR_DPF((PVR_DBG_ERROR, "LV1 *g_pvRegsKM = 0x%x",
		mfg_readl(g_pvRegsKM+0x01c)));
	}

	return PVRSRV_OK;
}

static int MTKDeInitHWAPM(void)
{
#if 0 /* No need to unmap during APM on/off to reduce system overhead */
	if (g_pvRegsKM) {
		OSUnMapPhysToLin(g_pvRegsKM, 0x1000, 0);
		g_pvRegsKM = NULL;
	}
#endif
#if 0
	if (g_pvRegsKM) {
		DRV_WriteReg32(g_pvRegsKM + 0x24, 0x0);
		DRV_WriteReg32(g_pvRegsKM + 0x28, 0x0);
	}
#endif
	return PVRSRV_OK;
}
#endif

#ifdef MTK_GPU_DVFS
/* For ged_dvfs idx commit */
static void MTKCommitFreqIdx(unsigned long ui32NewFreqID,
	GED_DVFS_COMMIT_TYPE eCommitType, int *pbCommited)
{
	PVRSRV_DEV_POWER_STATE ePowerState;
	PVRSRV_DEVICE_NODE *psDevNode = MTKGetRGXDevNode();
	PVRSRV_ERROR eResult;


	if (psDevNode) {
		eResult = PVRSRVDevicePreClockSpeedChange(psDevNode,
			IMG_FALSE, (void *)NULL);
		if ((eResult == PVRSRV_OK) || (eResult == PVRSRV_ERROR_RETRY)) {
			unsigned int ui32GPUFreq;
			unsigned int ui32CurFreqID;
			PVRSRV_DEV_POWER_STATE ePowerState;

			PVRSRVGetDevicePowerState(psDevNode, &ePowerState);

			if (ePowerState == PVRSRV_DEV_POWER_STATE_ON) {
				mt_gpufreq_target(ui32NewFreqID);
				g_bUnsync = IMG_FALSE;
			} else {
				g_ui32_unsync_freq_id = ui32NewFreqID;
				g_bUnsync = IMG_TRUE;
			}

			ui32CurFreqID = mt_gpufreq_get_cur_freq_index();

			ui32GPUFreq =
			mt_gpufreq_get_frequency_by_level(ui32CurFreqID);

			gpu_freq = ui32GPUFreq;

			if (eResult == PVRSRV_OK)
				PVRSRVDevicePostClockSpeedChange(psDevNode,
				IMG_FALSE, (void *)NULL);

			/* Always return true because the APM would almost
			 * letting GPU power down with high possibility
			 * while DVFS committing
			 */
			if (pbCommited)
				*pbCommited = IMG_TRUE;
			return;
		}
	}

	if (pbCommited)
		*pbCommited = IMG_FALSE;
}
unsigned int MTKCommitFreqForPVR(unsigned long ui32NewFreq)
{
	int i32MaxLevel = (int)(mt_gpufreq_get_dvfs_table_num()-1);
	unsigned int ui32NewFreqID = 0;
	unsigned long gpu_freq;
	int i;
	int promoted = 0;

	/* promotion bit at LSB  */
	promoted = ui32NewFreq & 0x1;

	ui32NewFreq /= 1000; /* shrink to KHz */
	ui32NewFreqID = i32MaxLevel;
	for (i = 0; i <= i32MaxLevel; i++) {
		gpu_freq = mt_gpufreq_get_freq_by_idx(i);

		if (ui32NewFreq > gpu_freq) {
			if (i == 0)
				ui32NewFreqID = 0;
			else
				ui32NewFreqID = i-1;
			break;
		}
	}

	if (ui32NewFreqID != 0)
		ui32NewFreqID -= promoted;

	MTKCommitFreqIdx(ui32NewFreqID, 0x7788, NULL);

	gpu_freq = mt_gpufreq_get_freq_by_idx(ui32NewFreqID);
	return gpu_freq * 1000; /* scale up to Hz */
}

#endif /* ifdef MTK_GPU_DVFS */
#ifdef MTK_CAL_POWER_INDEX
static void MTKStartPowerIndex(void)
{
	if (!g_pvRegsBaseKM) {
		PVRSRV_DEVICE_NODE *psDevNode = MTKGetRGXDevNode();

		if (psDevNode) {
			PVRSRV_RGXDEV_INFO *psDevInfo = psDevNode->pvDevice;

			if (psDevInfo)
				g_pvRegsBaseKM = psDevInfo->pvRegsBaseKM;
		}
	}

	if (g_pvRegsBaseKM)
		DRV_WriteReg32(g_pvRegsBaseKM + (uintptr_t)0x6320, 0x1);
}

static void MTKReStartPowerIndex(void)
{
	if (g_pvRegsBaseKM)
		DRV_WriteReg32(g_pvRegsBaseKM + (uintptr_t)0x6320, 0x1);
}

static void MTKStopPowerIndex(void)
{
	if (g_pvRegsBaseKM)
		DRV_WriteReg32(g_pvRegsBaseKM + (uintptr_t)0x6320, 0x0);
}

static IMG_UINT32 MTKCalPowerIndex(void)
{
	IMG_UINT32 ui32State, ui32Result;
	PVRSRV_DEV_POWER_STATE  ePowerState;
	IMG_BOOL bTimeout;
	IMG_UINT32 u32Deadline;
	void *pvGPIO_REG = g_pvRegsKM + (uintptr_t)MTK_GPIO_REG_OFFSET;
	void *pvPOWER_ESTIMATE_RESULT = g_pvRegsBaseKM + (uintptr_t)6328;

	if ((!g_pvRegsKM) || (!g_pvRegsBaseKM))
		return 0;

	if (PVRSRVPowerLock() != PVRSRV_OK)
		return 0;

	PVRSRVGetDevicePowerState(MTKGetRGXDevIdx(), &ePowerState);
	if (ePowerState != PVRSRV_DEV_POWER_STATE_ON) {
		PVRSRVPowerUnlock();
		return 0;
	}

	/* writes 1 to GPIO_INPUT_REQ, bit[0] */
	DRV_WriteReg32(pvGPIO_REG, DRV_Reg32(pvGPIO_REG) | 0x1);

	/* wait for 1 in GPIO_OUTPUT_REQ, bit[16] */
	bTimeout = IMG_TRUE;
	u32Deadline = OSClockus() + MTK_WAIT_FW_RESPONSE_TIMEOUT_US;

	while (OSClockus() < u32Deadline) {
		if (0x10000 & DRV_Reg32(pvGPIO_REG)) {
			bTimeout = IMG_FALSE;
			break;
		}
	}

	/* writes 0 to GPIO_INPUT_REQ, bit[0] */
	DRV_WriteReg32(pvGPIO_REG, DRV_Reg32(pvGPIO_REG) & (~0x1));
	if (bTimeout) {
		PVRSRVPowerUnlock();
		return 0;
	}

	/* read GPIO_OUTPUT_DATA, bit[24] */
	ui32State = DRV_Reg32(pvGPIO_REG) >> 24;

	/* read POWER_ESTIMATE_RESULT */
	ui32Result = DRV_Reg32(pvPOWER_ESTIMATE_RESULT);

	/* writes 1 to GPIO_OUTPUT_ACK, bit[17] */
	DRV_WriteReg32(pvGPIO_REG, DRV_Reg32(pvGPIO_REG)|0x20000);

	/* wait for 0 in GPIO_OUTPUT_REG, bit[16] */
	bTimeout = IMG_TRUE;
	u32Deadline = OSClockus() + MTK_WAIT_FW_RESPONSE_TIMEOUT_US;

	while (OSClockus() < u32Deadline) {
		if (!(0x10000 & DRV_Reg32(pvGPIO_REG))) {
			bTimeout = IMG_FALSE;
			break;
		}
	}

	/* writes 0 to GPIO_OUTPUT_ACK, bit[17] */
	DRV_WriteReg32(pvGPIO_REG, DRV_Reg32(pvGPIO_REG) & (~0x20000));
	if (bTimeout) {
		PVRSRVPowerUnlock();
		return 0;
	}

	MTKReStartPowerIndex();

	PVRSRVPowerUnlock();

	return (ui32State == 1) ? ui32Result : 0;
}
#endif

#ifdef MTK_GPU_DVFS
static void MTKCalGpuLoading(unsigned int *pui32Loading,
	unsigned int *pui32Block, unsigned int *pui32Idle)
{
	PVRSRV_DEVICE_NODE *psDevNode = MTKGetRGXDevNode();

	if (!psDevNode) {
		PVR_DPF((PVR_DBG_ERROR, "psDevNode not found"));
		return;
	}

	PVRSRV_RGXDEV_INFO *psDevInfo = psDevNode->pvDevice;

	if (psDevInfo && psDevInfo->pfnGetGpuUtilStats) {
		RGXFWIF_GPU_UTIL_STATS sGpuUtilStats = {0};

		if (g_RGXutilUser == NULL)
			return;

		psDevInfo->pfnGetGpuUtilStats(psDevInfo->psDeviceNode,
		g_RGXutilUser, &sGpuUtilStats);

		if (sGpuUtilStats.bValid) {
#if 0
			PVR_DPF((PVR_DBG_ERROR, "Loading: A(%d), I(%d), B(%d)",
				sGpuUtilStats.ui64GpuStatActiveHigh,
				sGpuUtilStats.ui64GpuStatIdle,
				sGpuUtilStats.ui64GpuStatBlocked));
#endif
		*pui32Loading =
			(100*(sGpuUtilStats.ui64GpuStatActiveHigh
				+ sGpuUtilStats.ui64GpuStatActiveLow))
			/ sGpuUtilStats.ui64GpuStatCumulative;
		*pui32Block =
			(100*(sGpuUtilStats.ui64GpuStatBlocked)) /
			sGpuUtilStats.ui64GpuStatCumulative;
		*pui32Idle =
			(100*(sGpuUtilStats.ui64GpuStatIdle)) /
			sGpuUtilStats.ui64GpuStatCumulative;
		}
	}
}
#endif

static bool MTKCheckDeviceInit(void)
{
	PVRSRV_DEVICE_NODE *psDevNode = MTKGetRGXDevNode();
	bool ret = false;

	if (psDevNode) {
		if (psDevNode->eDevState == PVRSRV_DEVICE_STATE_ACTIVE)
			ret = true;
	}
	return ret;
}

PVRSRV_ERROR MTKDevPrePowerState(IMG_HANDLE hSysData,
	PVRSRV_DEV_POWER_STATE eNewPowerState,
	PVRSRV_DEV_POWER_STATE eCurrentPowerState,
	IMG_BOOL bForced)
{
	if (eNewPowerState == PVRSRV_DEV_POWER_STATE_OFF &&
		eCurrentPowerState == PVRSRV_DEV_POWER_STATE_ON) {
#ifndef ENABLE_COMMON_DVFS
		if (g_hDVFSTimer && g_bDeviceInit) {
#else
		if (g_bDeviceInit) {
#endif
			g_bExit = IMG_TRUE;

#ifdef MTK_CAL_POWER_INDEX
			MTKStopPowerIndex();
#endif
		} else
		g_bDeviceInit = MTKCheckDeviceInit();

#if defined(MTK_USE_HW_APM)
		MTKDeInitHWAPM();
#endif
		MTKDisableMfgClock(bForced);
	}
#ifdef MTK_MFGMTCMOS_AO
	else if ((eNewPowerState == PVRSRV_DEV_POWER_STATE_OFF &&
			eCurrentPowerState == PVRSRV_DEV_POWER_STATE_OFF) &&
			bForced == true) {
		if (gpu_debug_enable)
			PVR_DPF((PVR_DBG_MESSAGE,
			"MTKDevPrePowerState MTKDisableMfgMtcmos0"));
		MTKDisableMfgMtcmos0();
	}
#endif
	return PVRSRV_OK;
}

PVRSRV_ERROR MTKDevPostPowerState(IMG_HANDLE hSysData,
	PVRSRV_DEV_POWER_STATE eNewPowerState,
	PVRSRV_DEV_POWER_STATE eCurrentPowerState,
	IMG_BOOL bForced)
{
	if (eCurrentPowerState == PVRSRV_DEV_POWER_STATE_OFF &&
		eNewPowerState == PVRSRV_DEV_POWER_STATE_ON) {
		MTKEnableMfgClock(bForced);
#if defined(MTK_USE_HW_APM)
		MTKInitHWAPM();
#endif
#ifndef ENABLE_COMMON_DVFS
		if (g_hDVFSTimer && g_bDeviceInit) {
#else
			if (g_bDeviceInit) {
#endif
#ifdef MTK_CAL_POWER_INDEX
				MTKStartPowerIndex();
#endif
				g_bExit = IMG_FALSE;
			} else
				g_bDeviceInit = MTKCheckDeviceInit();


			if (g_bUnsync == IMG_TRUE) {
#ifdef MTK_GPU_DVFS
				mt_gpufreq_target(g_ui32_unsync_freq_id);
#endif
				g_bUnsync = IMG_FALSE;
			}

		}

	return PVRSRV_OK;
}

PVRSRV_ERROR MTKSystemPrePowerState(PVRSRV_SYS_POWER_STATE eNewPowerState)
{
	if (eNewPowerState == PVRSRV_SYS_POWER_STATE_OFF)
		;

	return PVRSRV_OK;
}

PVRSRV_ERROR MTKSystemPostPowerState(PVRSRV_SYS_POWER_STATE eNewPowerState)
{
	if (eNewPowerState == PVRSRV_SYS_POWER_STATE_ON)
		;

	return PVRSRV_OK;
}
/* extern int (*ged_dvfs_vsync_trigger_fp)(int idx); */

#ifdef SUPPORT_PDVFS
#include "rgxpdvfs.h"
#define  mt_gpufreq_get_freq_by_idx mt_gpufreq_get_frequency_by_level
static IMG_OPP  *gpasOPPTable;

static int MTKMFGOppUpdate(int ui32ThrottlePoint)
{
	PVRSRV_DEVICE_NODE *psDevNode;
	int i;
	int ui32OPPTableSize;

	static RGXFWIF_PDVFS_OPP sPDFVSOppInfo;
	static int bNotReady;

	bNotReady = 1;
	psDevNode = MTKGetRGXDevNode();

	if (bNotReady) {
		ui32OPPTableSize = mt_gpufreq_get_dvfs_table_num();
		gpasOPPTable
		= (IMG_OPP *)OSAllocZMem(sizeof(IMG_OPP) * ui32OPPTableSize);

		for (i = 0; i < ui32OPPTableSize; i++) {
			gpasOPPTable[i].ui32Volt
			= mt_gpufreq_get_volt_by_idx(i);

			gpasOPPTable[i].ui32Freq
			= mt_gpufreq_get_freq_by_idx(i) * 1000;
		}

		if (psDevNode) {
			PVRSRV_RGXDEV_INFO *psDevInfo = psDevNode->pvDevice;
			PVRSRV_DEVICE_CONFIG *psDevConfig;

			psDevConfig = psDevNode->psDevConfig;

			psDevConfig->sDVFS.sDVFSDeviceCfg.pasOPPTable
			= gpasOPPTable;

			psDevConfig->sDVFS.sDVFSDeviceCfg.ui32OPPTableSize
			= ui32OPPTableSize;

			bNotReady = 0;

			PVR_DPF((PVR_DBG_ERROR,
				"PDVFS opptab=%p size=%d init completed",
				psDevConfig->sDVFS.sDVFSDeviceCfg.pasOPPTable,
				ui32OPPTableSize));
		} else {
			if (gpasOPPTable)
				OSFreeMem(gpasOPPTable);
		}
	}

}
#endif

void mtk_fdvfs_update_cur_freq(int ui32GPUFreq)
{
	PVRSRV_DEVICE_NODE *psDevNode = MTKGetRGXDevNode();
	PVRSRV_ERROR eResult;
	static int s_ui32GPUFreq;

	if (s_ui32GPUFreq != ui32GPUFreq) {
		if (psDevNode) {
		eResult = PVRSRVDevicePreClockSpeedChange(psDevNode,
			IMG_FALSE, (void *)NULL);
		if ((eResult == PVRSRV_OK) || (eResult == PVRSRV_ERROR_RETRY)) {
			MTKWriteBackFreqToRGX(psDevNode, ui32GPUFreq);
			if (eResult == PVRSRV_OK)
				PVRSRVDevicePostClockSpeedChange(psDevNode,
				IMG_FALSE, (void *)NULL);
		}
	}
	s_ui32GPUFreq = ui32GPUFreq;
}
}
EXPORT_SYMBOL(mtk_fdvfs_update_cur_freq);

PVRSRV_ERROR MTKMFGSystemInit(void)
{
	int i;
	PVRSRV_ERROR error;


#ifndef MTK_GPU_DVFS
	gpu_dvfs_enable = 0;
#else
	gpu_dvfs_enable = 1;


#ifndef ENABLE_COMMON_DVFS
	error = OSLockCreate(&ghDVFSLock);
	if (error != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR, "Create DVFS Lock Failed"));
		goto ERROR;
	}

	error = OSLockCreate(&ghDVFSTimerLock);

	if (error != PVRSRV_OK)	{
		PVR_DPF((PVR_DBG_ERROR, "Create DVFS Timer Lock Failed"));
		goto ERROR;
	}

	g_iSkipCount = MTK_DEFER_DVFS_WORK_MS / MTK_DVFS_SWITCH_INTERVAL_MS;

	g_hDVFSTimer = OSAddTimer(MTKDVFSTimerFuncCB,
			(void *)NULL, MTK_DVFS_SWITCH_INTERVAL_MS);

	if (!g_hDVFSTimer) {
		PVR_DPF((PVR_DBG_ERROR, "Create DVFS Timer Failed"));
		goto ERROR;
	}

	if (OSEnableTimer(g_hDVFSTimer) == PVRSRV_OK)
		g_bTimerEnable = IMG_TRUE;

	boost_gpu_enable = 1;

	g_sys_dvfs_time_ms = 0;

	g_bottom_freq_id = mt_gpufreq_get_dvfs_table_num() - 1;
	gpu_bottom_freq = mt_gpufreq_get_frequency_by_level(g_bottom_freq_id);

	g_cust_boost_freq_id = mt_gpufreq_get_dvfs_table_num() - 1;

	gpu_cust_boost_freq =
	mt_gpufreq_get_frequency_by_level(g_cust_boost_freq_id);

	g_cust_upbound_freq_id = 0;

	gpu_cust_upbound_freq =
	mt_gpufreq_get_frequency_by_level(g_cust_upbound_freq_id);

	gpu_debug_enable = 0;

	mt_gpufreq_input_boost_notify_registerCB(MTKFreqInputBoostCB);
	mt_gpufreq_power_limit_notify_registerCB(MTKFreqPowerLimitCB);

	mtk_boost_gpu_freq_fp = MTKBoostGpuFreq;

	mtk_set_bottom_gpu_freq_fp = MTKSetBottomGPUFreq;

	mtk_custom_get_gpu_freq_level_count_fp = MTKCustomGetGpuFreqLevelCount;

	mtk_custom_boost_gpu_freq_fp = MTKCustomBoostGpuFreq;

	mtk_custom_upbound_gpu_freq_fp = MTKCustomUpBoundGpuFreq;

	mtk_get_custom_boost_gpu_freq_fp = MTKGetCustomBoostGpuFreq;

	mtk_get_custom_upbound_gpu_freq_fp = MTKGetCustomUpBoundGpuFreq;

	mtk_get_gpu_power_loading_fp = MTKGetPowerIndex;

	mtk_get_gpu_loading_fp = MTKGetGpuLoading;
	mtk_get_gpu_block_fp = MTKGetGpuBlock;
	mtk_get_gpu_idle_fp = MTKGetGpuIdle;
#else
#ifdef SUPPORT_PDVFS
	/* ged_dvfs_vsync_trigger_fp = MTKMFGOppUpdate; */
	/* turn-off GED loading based DVFS */
	ged_dvfs_cal_gpu_utilization_fp = MTKFakeGpuLoading;
	/* mt_gpufreq_power_limit_notify_registerCB( ); */
#else
	ged_dvfs_cal_gpu_utilization_fp = MTKCalGpuLoading;
	ged_dvfs_gpu_freq_commit_fp = MTKCommitFreqIdx;
#endif

#endif

#endif /* ifdef MTK_GPU_DVFS */


#ifdef CONFIG_MTK_HIBERNATION
	register_swsusp_restore_noirq_func(ID_M_GPU,
	gpu_pm_restore_noirq, NULL);
#endif

#if defined(MTK_USE_HW_APM)
	if (!g_pvRegsKM) {
		PVRSRV_DEVICE_NODE *psDevNode = MTKGetRGXDevNode();

		if (psDevNode) {
			PVRSRV_RGXDEV_INFO *psDevInfo = psDevNode->pvDevice;

			PVRSRV_DEVICE_CONFIG *psDevConfig
				= psDevNode->psDevConfig;

			if (psDevConfig && (!g_pvRegsKM)) {
				gsRegsPBase = psDevConfig->sRegsCpuPBase;
				gsRegsPBase.uiAddr += 0xffe000;

				g_pvRegsKM =
				OSMapPhysToLin(gsRegsPBase, 0x1000, 0);

				PVR_DPF((PVR_DBG_ERROR,
				"g_pvRegsKM = 0x%p", g_pvRegsKM));
			}
		}
	}
#endif

	return PVRSRV_OK;

ERROR:
	MTKMFGSystemDeInit();

	return PVRSRV_ERROR_INIT_FAILURE;
}

void MTKMFGSystemDeInit(void)
{
#ifdef SUPPORT_PDVFS
	if (gpasOPPTable)
		OSFreeMem(gpasOPPTable);
#endif

#ifdef CONFIG_MTK_HIBERNATION
	unregister_swsusp_restore_noirq_func(ID_M_GPU);
#endif

	g_bExit = IMG_TRUE;

#ifdef MTK_GPU_DVFS
#ifndef ENABLE_COMMON_DVFS
	if (g_hDVFSTimer) {
		OSDisableTimer(g_hDVFSTimer);
		OSRemoveTimer(g_hDVFSTimer);
		g_hDVFSTimer = IMG_NULL;
	}

	if (ghDVFSLock) {
		OSLockDestroy(ghDVFSLock);
		ghDVFSLock = NULL;
	}

	if (ghDVFSTimerLock) {
		OSLockDestroy(ghDVFSTimerLock);
		ghDVFSTimerLock = NULL;
	}
#endif
#endif /* MTK_GPU_DVFS */

#ifdef MTK_CAL_POWER_INDEX
	g_pvRegsBaseKM = NULL;
#endif

#if defined(MTK_USE_HW_APM)
	if (g_pvRegsKM) {
		OSUnMapPhysToLin(g_pvRegsKM, 0x1000, 0);
		g_pvRegsKM = NULL;
	}
#endif
}


int MTKRGXDeviceInit(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	struct device *pdev;

	_mtk_ged_log = ged_log_buf_alloc(64, 64 * 32,
			GED_LOG_BUF_TYPE_RINGBUFFER, "PowerLog", "ppL");

	_track_ged_log = ged_log_buf_alloc(32, 32 * 32,
			GED_LOG_BUF_TYPE_RINGBUFFER, "RegStack", "RTrace");

	_mpu_ged_log = ged_log_buf_alloc(32, 32 * 32,
			GED_LOG_BUF_TYPE_RINGBUFFER, "GPUImport", "GImp");

#ifdef MTK_GPU_DVFS
	/* Only Enable buck to get cg & mtcmos */
	mt_gpufreq_voltage_enable_set(BUCK_ON);
#endif

#ifndef MTK_BRINGUP
	/* Enable CG & mtcmos */
	MTKEnableMfgClock(IMG_TRUE);
#endif
	if (psDevConfig && (!g_pvRegsKM)) {
		gsRegsPBase = psDevConfig->sRegsCpuPBase;
		gsRegsPBase.uiAddr += 0xffe000;
#if defined(MTK_USE_HW_APM)
		MTKInitHWAPM();
#endif
	} else
		PVR_DPF((PVR_DBG_ERROR,
		"psDevConfig = 0x%p, g_pvRegsKM = 0x%p",
		psDevConfig, g_pvRegsKM));

	/* 1.5+DDK support multiple user to query GPU utilization */
	/* Need Init here */
	if (g_RGXutilUser == NULL)
		SORgxGpuUtilStatsRegister(&g_RGXutilUser);

#if MTK_PM_SUPPORT
	MTKDisableMfgClock(IMG_TRUE);
#endif

	return 0;
}

int MTKRGXDeviceDeInit(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	return 0;
}

void MTKSaveFrame(const char func_name[])
{
	ged_log_buf_print2(_track_ged_log, GED_LOG_ATTR_TIME, "%s", func_name);
}
EXPORT_SYMBOL(MTKSaveFrame);

void MTKFWDump(void)
{
	PVRSRV_DEVICE_NODE *psDevNode = MTKGetRGXDevNode();

	MTK_PVRSRVDebugRequestSetSilence(IMG_TRUE);
	PVRSRVDebugRequest(psDevNode, DEBUG_REQUEST_VERBOSITY_MAX, NULL, NULL);
	MTK_PVRSRVDebugRequestSetSilence(IMG_FALSE);
}
EXPORT_SYMBOL(MTKFWDump);

#ifndef ENABLE_COMMON_DVFS
module_param(gpu_loading, uint, 0644);
module_param(gpu_block, uint, 0644);
module_param(gpu_idle, uint, 0644);
module_param(gpu_dvfs_enable, uint, 0644);
module_param(boost_gpu_enable, uint, 0644);
module_param(gpu_dvfs_force_idle, uint, 0644);
module_param(gpu_dvfs_cb_force_idle, uint, 0644);
module_param(gpu_bottom_freq, uint, 0644);
module_param(gpu_cust_boost_freq, uint, 0644);
module_param(gpu_cust_upbound_freq, uint, 0644);
module_param(gpu_freq, uint, 0644);
#endif

module_param(gpu_power, uint, 0644);
module_param(gpu_debug_enable, uint, 0644);


#ifdef CONFIG_MTK_SEGMENT_TEST
module_param(efuse_mfg_enable, uint, 0644);
#endif
