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

#ifdef pr_fmt
#undef pr_fmt
#endif

#define pr_fmt(fmt) "["KBUILD_MODNAME"]" fmt

#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <mt-plat/mtk_gpt.h>
#include <mt-plat/mtk_boot_common.h>
#include <linux/io.h>
#include <linux/scatterlist.h>
#include <linux/gpio.h>
#include "mtk_sd.h"
#include <mmc/core/core.h>
#include "dbg.h"
#include "autok_dvfs.h"

#if !defined(FPGA_PLATFORM) && !defined(CONFIG_MTK_MSDC_BRING_UP_BYPASS)
#include <mt-plat/upmu_common.h>
#endif

/* for get transfer time with each trunk size, default not open */
#ifdef MTK_MMC_PERFORMANCE_TEST
unsigned int g_mtk_mmc_perf_test;
#endif

static char cmd_buf[256];

inline void dbg_add_host_log(struct mmc_host *mmc, int type, int cmd, int arg)
{
}
void mmc_cmd_dump(char **buff, unsigned long *size, struct seq_file *m,
	struct mmc_host *mmc, u32 latest_cnt)
{
}
void msdc_dump_host_state(char **buff, unsigned long *size,
		struct seq_file *m, struct msdc_host *host)
{
}
void get_msdc_aee_buffer(unsigned long *vaddr, unsigned long *size)
{
}

void msdc_cmdq_status_print(struct msdc_host *host, struct seq_file *m)
{
}

void msdc_cmdq_func(struct msdc_host *host, const int num, struct seq_file *m)
{
}

/* Clone from core/mmc.c since it is static in mmc.c */

void msdc_get_host_mode_speed(struct seq_file *m, struct mmc_host *mmc)
{
}

void msdc_set_host_mode_speed(struct seq_file *m,
	struct mmc_host *mmc, int spd_mode)
{
}

#define COMPARE_ADDRESS_MMC             0x402000
#define COMPARE_ADDRESS_SD              0x2000
#define COMPARE_ADDRESS_SD_COMBO        0x2000

#define MSDC_MULTI_BUF_LEN  (4*4*1024) /*16KB write/read/compare*/

static DEFINE_MUTEX(sd_lock);
static DEFINE_MUTEX(emmc_lock);

u8 read_write_state; /* 0:stop, 1:read, 2:write */

static u8 wData_emmc[16] = {
	0x67, 0x45, 0x23, 0x01,
	0xef, 0xcd, 0xab, 0x89,
	0xce, 0x8a, 0x46, 0x02,
	0xde, 0x9b, 0x57, 0x13
};

static u8 wData_sd[200] = {
	0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00,    /*worst1*/
	0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00,
	0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00,

	0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00,    /*worst2*/
	0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00,
	0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00,

	0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0xff, 0xff,    /*worst3*/
	0xff, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x00,
	0x00, 0x00, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00,

	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,    /*worst4*/
	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,

	0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55,    /*worst5*/
	0x80, 0x80, 0x80, 0x7f, 0x80, 0x80, 0x80, 0x7f,
	0x7f, 0x7f, 0x80, 0x7f, 0x7f, 0x7f, 0x40, 0x40,
	0x04, 0xfb, 0x04, 0x04, 0x04, 0xfb, 0xfb, 0xfb,
	0x04, 0xfb, 0xfb, 0xfb, 0x02, 0x02, 0x02, 0xfd,
	0x02, 0x02, 0x02, 0xfd, 0xfd, 0xfd, 0x02, 0xfd,
	0xfd, 0xfd, 0x01, 0x01, 0x01, 0xfe, 0x01, 0x01,
	0x01, 0xfe, 0xfe, 0xfe, 0x01, 0xfe, 0xfe, 0xfe,
	0x80, 0x80, 0x80, 0x7f, 0x80, 0x80, 0x80, 0x7f,
	0x7f, 0x7f, 0x80, 0x7f, 0x7f, 0x7f, 0x40, 0x40,
	0x40, 0x40, 0x80, 0x7f, 0x7f, 0x7f, 0x40, 0x40,
	0x20, 0xdf, 0x20, 0x20, 0x20, 0xdf, 0xdf, 0xdf,
	0x10, 0x10, 0x10, 0xef, 0xef, 0x10, 0xef, 0xef,
};

/*
 * @read, bit0: 1:read/0:write; bit1: 0:compare/1:not compare
 */
static int multi_rw_compare_core(int host_num, int read, uint address,
	uint type, uint compare)
{
	struct scatterlist msdc_sg;
	struct mmc_data msdc_data;
	struct mmc_command msdc_cmd;
	struct mmc_command msdc_stop;

	u32 *multi_rwbuf = NULL;
	u8 *wPtr = NULL, *rPtr = NULL;

	struct mmc_request msdc_mrq;
	struct msdc_host *host_ctl;
	struct mmc_host *mmc;
	int result = 0, forIndex = 0;
#if defined(CONFIG_MTK_EMMC_CQ_SUPPORT) || defined(CONFIG_MTK_EMMC_HW_CQ)
	bool cmdq_en;
	int ret;
#endif

	u8 wData_len;
	u8 *wData;

	if (host_num >= HOST_MAX_NUM || host_num < 0) {
		pr_notice("[%s]invalid host id: %d\n", __func__, host_num);
		return -1;
	}

	host_ctl = mtk_msdc_host[host_num];

	if (!host_ctl || !host_ctl->mmc || !host_ctl->mmc->card) {
		pr_notice(" No card initialized in host[%d]\n", host_num);
		result = -1;
		goto free;
	}

	if (type == MMC_TYPE_MMC) {
		wData = wData_emmc;
		wData_len = 16;
	} else if (type == MMC_TYPE_SD) {
		wData = wData_sd;
		wData_len = 200;
	} else
		return -1;

	/*allock memory for test buf*/
	multi_rwbuf = kzalloc((MSDC_MULTI_BUF_LEN), GFP_KERNEL);
	if (multi_rwbuf == NULL) {
		result = -1;
		return result;
	}
	rPtr = wPtr = (u8 *)multi_rwbuf;

	if (!is_card_present(host_ctl)) {
		pr_info("  [%s]: card is removed!\n", __func__);
		result = -1;
		goto free;
	}

	mmc = host_ctl->mmc;

	mmc_get_card(mmc->card);

#if defined(CONFIG_MTK_EMMC_CQ_SUPPORT) || defined(CONFIG_MTK_EMMC_HW_CQ)
	cmdq_en = !!mmc_card_cmdq(mmc->card);
	if (cmdq_en) {
		/* cmdq enabled, turn it off first */
		pr_debug("[MSDC] cmdq enabled, turn it off\n");
		ret = mmc_blk_cmdq_switch(host_ctl->mmc->card, 0);
		if (ret) {
			pr_notice("[MSDC] turn off cmdq en failed\n");
			mmc_put_card(host_ctl->mmc->card);
			result = -1;
			goto free;
		}
	}
#endif

	if (host_ctl->hw->host_function == MSDC_EMMC)
		msdc_switch_part(host_ctl, 0);

	memset(&msdc_data, 0, sizeof(struct mmc_data));
	memset(&msdc_mrq, 0, sizeof(struct mmc_request));
	memset(&msdc_cmd, 0, sizeof(struct mmc_command));
	memset(&msdc_stop, 0, sizeof(struct mmc_command));

	msdc_mrq.cmd = &msdc_cmd;
	msdc_mrq.data = &msdc_data;
	msdc_data.blocks = MSDC_MULTI_BUF_LEN / 512;

	if (read) {
		/* init read command */
		msdc_data.flags = MMC_DATA_READ;
		msdc_cmd.opcode = MMC_READ_MULTIPLE_BLOCK;
	} else {
		/* init write command */
		msdc_data.flags = MMC_DATA_WRITE;
		msdc_cmd.opcode = MMC_WRITE_MULTIPLE_BLOCK;
		/* init write buffer */
		for (forIndex = 0; forIndex < MSDC_MULTI_BUF_LEN; forIndex++)
			*(wPtr + forIndex) = wData[forIndex % wData_len];
	}

#if defined(CONFIG_MTK_HW_FDE) && defined(CONFIG_MTK_HW_FDE_AES)
	if (fde_aes_check_cmd(FDE_AES_EN_RAW, fde_aes_get_raw(), host_num)) {
		fde_aes_set_msdc_id(host_num & 0xff);
		if (read)
			fde_aes_set_fde(0);
		else
			fde_aes_set_fde(1);
	}
#endif

	msdc_cmd.arg = address;

	msdc_stop.opcode = MMC_STOP_TRANSMISSION;
	msdc_stop.arg = 0;
	msdc_stop.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC;
	msdc_data.stop = &msdc_stop;

	if (!mmc_card_blockaddr(host_ctl->mmc->card)) {
		/* pr_info("this device use byte address!!\n"); */
		msdc_cmd.arg <<= 9;
	}
	msdc_cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC;

	msdc_data.blksz = 512;
	msdc_data.sg = &msdc_sg;
	msdc_data.sg_len = 1;

	sg_init_one(&msdc_sg, multi_rwbuf, MSDC_MULTI_BUF_LEN);

	mmc_set_data_timeout(&msdc_data, mmc->card);
	mmc_wait_for_req(mmc, &msdc_mrq);

	if (compare == 0 || !read)
		goto skip_check;

	/* compare */
	for (forIndex = 0; forIndex < MSDC_MULTI_BUF_LEN; forIndex++) {
		if (rPtr[forIndex] != wData[forIndex % wData_len]) {
			pr_info("index[%d]\tW_buffer[0x%x]\tR_buffer[0x%x]\tfailed\n",
				forIndex, wData[forIndex % wData_len],
				rPtr[forIndex]);
			result = -1;
		}
	}

skip_check:
#if defined(CONFIG_MTK_EMMC_CQ_SUPPORT) || defined(CONFIG_MTK_EMMC_HW_CQ)
	if (cmdq_en) {
		pr_debug("[MSDC_DBG] turn on cmdq\n");
		ret = mmc_blk_cmdq_switch(host_ctl->mmc->card, 1);
		if (ret)
			pr_notice("[MSDC] turn on cmdq en failed\n");
	}
#endif

	mmc_put_card(host_ctl->mmc->card);

	if (msdc_cmd.error)
		result = msdc_cmd.error;

	if (msdc_data.error)
		result = msdc_data.error;

free:
	kfree(multi_rwbuf);

	return result;
}

int multi_rw_compare(struct seq_file *m, int host_num,
		uint address, int count, uint type, int multi_thread)
{
	int i = 0;
	int error = 0;
	struct mutex *rw_mutex;

	if (type == MMC_TYPE_SD)
		rw_mutex = &sd_lock;
	else if (type == MMC_TYPE_MMC)
		rw_mutex = &emmc_lock;
	else
		return 0;

	for (i = 0; i < count; i++) {
		mutex_lock(rw_mutex);
		/* write */
		error = multi_rw_compare_core(host_num, 0, address, type, 0);
		if (error) {
			if (!multi_thread)
				seq_printf(m, "[%s]: failed to write data, error=%d\n",
					__func__, error);
			else
				pr_info("[%s]: failed to write data, error=%d\n",
				__func__, error);
			goto skip_read;
		}

		/* read */
		error = multi_rw_compare_core(host_num, 1, address,
			type, 1);
		if (error) {
			if (!multi_thread)
				seq_printf(m, "[%s]: failed to read data, error=%d\n",
				__func__, error);
			else
				pr_notice("[%s]: failed to read data, error=%d\n",
				__func__, error);
		}

skip_read:
		if (!multi_thread)
			seq_printf(m, "== cpu[%d] pid[%d]: %s %d time compare ==\n",
				task_cpu(current), current->pid,
				(error ? "FAILED" : "FINISH"), i);
		else
			pr_info("== cpu[%d] pid[%d]: %s %d time compare ==\n",
				task_cpu(current), current->pid,
				(error ? "FAILED" : "FINISH"), i);

		mutex_unlock(rw_mutex);
		if (error)
			break;
	}

	if (i == count) {
		if (!multi_thread)
			seq_printf(m, "pid[%d]: success to compare data for %d times\n",
				current->pid, count);
		else
			pr_info("pid[%d]: success to compare data for %d times\n",
				current->pid, count);
	}

	return error;
}

#define MAX_THREAD_NUM_FOR_SMP 20

/* make the test can run on 4GB card */
static uint smp_address_on_sd[MAX_THREAD_NUM_FOR_SMP] = {
	0x2000,
	0x80000,
	0x100000,
	0x180000,
	0x200000,		/* 1GB */
	0x202000,
	0x280000,
	0x300000,
	0x380000,
	0x400000,		/* 2GB */
	0x402000,
	0x480000,
	0x500000,
	0x580000,
	0x600000,
	0x602000,		/* 3GB */
	0x660000,		/* real total size of 4GB sd card is < 4GB */
	0x680000,
	0x6a0000,
	0x6b0000,
};

/* cause the system run on the emmc storage,
 * so do not to access the first 2GB region
 */
static uint smp_address_on_mmc[MAX_THREAD_NUM_FOR_SMP] = {
	0x402000,
	0x410000,
	0x520000,
	0x530000,
	0x640000,
	0x452000,
	0x460000,
	0x470000,
	0x480000,
	0x490000,
	0x4a2000,
	0x4b0000,
	0x5c0000,
	0x5d0000,
	0x6e0000,
	0x602000,
	0x660000,		/* real total size of 4GB sd card is < 4GB */
	0x680000,
	0x6a0000,
	0x6b0000,
};

struct write_read_data {
	int host_id;		/* target host you want to do SMP test on. */
	uint start_address;	/* Address of memcard you want to write/read */
	int count;		/* times you want to do read after write */
	struct seq_file *m;
};

static struct write_read_data wr_data[HOST_MAX_NUM][MAX_THREAD_NUM_FOR_SMP];
/* 2012-03-25
 * the SMP thread function
 * do read after write the memory card, and bit by bit comparison
 */

struct task_struct *rw_thread;
/*
 * data: bit0~4:id, bit4~7: mode
 */
void msdc_dump_gpd_bd(int id)
{
}

int g_count;
/* ========== driver proc interface =========== */
static const struct file_operations msdc_proc_fops = {};

static const struct file_operations msdc_help_fops = {};

static const struct file_operations sdcard_intr_gpio_value_fops = {};

#define MSDC_PROC_SHOW(name, fmt, args...) 

static const struct file_operations fw_version_fops = {};

static const struct file_operations ext_csd_fops = {};
static const char * const msdc_proc_list[] = {};
/*These two TYPES are requested by HUIWEI normalized project*/
#define  EMMC_TYPE      0
#define  UFS_TYPE       1
#ifdef	MTK_BOOT
#define  BOOTTYPE	(get_boot_type()&0x11 \
? (get_boot_type() == 2 ? UFS_TYPE : EMMC_TYPE):0xff)
#else
#define BOOTTYPE EMMC_TYPE
#endif
//static int boot_type = get_boot_type(); // 0 nand, 1 mmc, 2 ufs
static const struct file_operations *proc_fops_list[] = {};

#ifndef USER_BUILD_KERNEL
#define PROC_PERM		0660
#else
#define PROC_PERM		0440
#endif

int msdc_debug_proc_init_bootdevice(void)
{
	return 0;
}

int msdc_debug_proc_init(void)
{
	return 0;
}
EXPORT_SYMBOL_GPL(msdc_debug_proc_init);
