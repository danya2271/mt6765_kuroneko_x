/*
 * Copyright (C) 2011-2015 MediaTek Inc.
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

#include <linux/types.h>
#include <linux/module.h>       /* needed by all modules */
#include <linux/init.h>         /* needed by module macros */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/device.h>       /* needed by device_* */
#include <linux/vmalloc.h>      /* needed by kmalloc */
#include <linux/uaccess.h>      /* needed by copy_to_user */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/slab.h>         /* needed by kmalloc */
#include <linux/poll.h>         /* needed by poll */
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/suspend.h>
#include <linux/timer.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/types.h>
#include <linux/atomic.h>
#include <mt-plat/sync_write.h>
#include "sspm_define.h"
#include "sspm_ipi.h"
#include "sspm_reservedmem.h"
#include "sspm_reservedmem_define.h"
#include "sspm_sysfs.h"
#include "sspm_logger.h"

#ifdef SSPM_PLT_LOGGER_BUF_LEN
/* use platform-defined buffer length */
#define BUF_LEN				SSPM_PLT_LOGGER_BUF_LEN
#else
/* otherwise use default buffer length */
#define BUF_LEN				(1 * 1024 * 1024)
#endif
#define LBUF_LEN			(4 * 1024)
#define SSPM_TIMER_TIMEOUT	(1 * HZ) /* 1 seconds*/
#define ROUNDUP(a, b)		(((a) + ((b)-1)) & ~((b)-1))

struct log_ctrl_s {
	unsigned int base;
	unsigned int size;
	unsigned int enable;
	unsigned int info_ofs;
	unsigned int buff_ofs;
	unsigned int buff_size;
#if SSPM_LASTK_SUPPORT
	unsigned int linfo_ofs;
	unsigned int lbuff_ofs;
	unsigned int lbuff_size;
#endif
};

struct buffer_info_s {
	unsigned int r_pos;
	unsigned int w_pos;
};

static unsigned int sspm_logger_inited;
static struct log_ctrl_s *log_ctl;
static struct buffer_info_s *buf_info, *lbuf_info;
static struct timer_list sspm_log_timer;
#if SSPM_LASTK_SUPPORT
static unsigned int sspm_logger_lastk_exists;
#endif
static DEFINE_MUTEX(sspm_log_mutex);

static inline void sspm_log_timer_add(void)
{
}

static void sspm_log_timeout(unsigned long data)
{
}

ssize_t sspm_log_read(char __user *data, size_t len)
{
	return 0;
}

unsigned int sspm_log_poll(void)
{
	return 0;
}

static unsigned int sspm_log_enable_set(unsigned int enable)
{
	return 0;
}

#if SSPM_LASTK_SUPPORT
static unsigned int sspm_log_lastk_get(char *buf)
{
	return 0;
}

void sspm_log_lastk_recv(unsigned int exists)
{
}
#endif

static ssize_t sspm_mobile_log_show(struct device *kobj,
	struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t sspm_mobile_log_store(struct device *kobj,
	struct device_attribute *attr, const char *buf, size_t n)
{
	return 0;
}

DEVICE_ATTR(sspm_mobile_log, 0644, sspm_mobile_log_show, sspm_mobile_log_store);

#if SSPM_LASTK_SUPPORT
static ssize_t sspm_log_lastk_show(struct device *kobj,
	struct device_attribute *attr, char *buf)
{
	return 0;
}

DEVICE_ATTR(sspm_log_lastk, 0444, sspm_log_lastk_show, NULL);
#endif

unsigned int __init sspm_logger_init(phys_addr_t start, phys_addr_t limit)
{
	return 0;
}

int __init sspm_logger_init_done(void)
{
	return 0;
}
