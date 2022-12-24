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

#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/smp.h>
#include <linux/poll.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/ktime.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <mt-plat/mtk_ccci_common.h>
#include <mt-plat/mtk_rtc.h>

#include "ccci_util_log.h"

/******************************************************************************/
/* Ring buffer part, this type log is block read, used for temp debug purpose */
/******************************************************************************/
#define CCCI_LOG_BUF_SIZE 256	/* must be power of 2 */
#define CCCI_LOG_MAX_WRITE 128

/*extern u64 local_clock(void); */

struct ccci_ring_buffer {
	void *buffer;
	unsigned int size;
	unsigned int read_pos;
	unsigned int write_pos;
	unsigned int ch_num;
	atomic_t reader_cnt;
	wait_queue_head_t log_wq;
	spinlock_t write_lock;
};

struct ccci_ring_buffer ccci_log_buf;

int ccci_log_write(const char *fmt, ...)
{
	va_list args;
	int write_len, first_half;
	unsigned long flags;
	char *temp_log;
	int this_cpu;
	char state = irqs_disabled() ? '-' : ' ';
	u64 ts_nsec = local_clock();
	unsigned long rem_nsec = do_div(ts_nsec, 1000000000);

	if (unlikely(ccci_log_buf.buffer == NULL))
		return -ENODEV;

	temp_log = kmalloc(CCCI_LOG_MAX_WRITE, GFP_ATOMIC);
	if (temp_log == NULL) {
		/*pr_notice("[ccci0/util]alloc local buff fail p01\n");*/
		return -ENODEV;
	}

	preempt_disable();
	this_cpu = smp_processor_id();
	preempt_enable();
	write_len = snprintf(temp_log, CCCI_LOG_MAX_WRITE,
						"[%5lu.%06lu]%c(%x)[%d:%s]",
						(unsigned long)ts_nsec,
						rem_nsec / 1000,
						state,
						this_cpu,
						current->pid,
						current->comm);

	va_start(args, fmt);
	write_len +=
		vsnprintf(temp_log + write_len,
			CCCI_LOG_MAX_WRITE - write_len,
			fmt, args);
	va_end(args);

	spin_lock_irqsave(&ccci_log_buf.write_lock, flags);
	if (ccci_log_buf.write_pos + write_len > CCCI_LOG_BUF_SIZE) {
		first_half = CCCI_LOG_BUF_SIZE - ccci_log_buf.write_pos;
		memcpy(ccci_log_buf.buffer + ccci_log_buf.write_pos,
				temp_log, first_half);
		memcpy(ccci_log_buf.buffer, temp_log + first_half,
				write_len - first_half);
	} else {
		memcpy(ccci_log_buf.buffer + ccci_log_buf.write_pos,
				temp_log, write_len);
	}
	ccci_log_buf.write_pos = (ccci_log_buf.write_pos + write_len)
		& (CCCI_LOG_BUF_SIZE - 1);
	if (write_len > 0)
		ccci_log_buf.ch_num += (unsigned int)write_len;
	spin_unlock_irqrestore(&ccci_log_buf.write_lock, flags);
	wake_up_all(&ccci_log_buf.log_wq);

	kfree(temp_log);
	temp_log = NULL;

	return write_len;
}

int ccci_log_write_raw(unsigned int set_flags, const char *fmt, ...)
{
	va_list args;
	int write_len, first_half;
	unsigned long flags;
	char *temp_log;
	int this_cpu;
	char state;
	u64 ts_nsec;
	unsigned long rem_nsec;

	if (unlikely(ccci_log_buf.buffer == NULL))
		return -ENODEV;

	temp_log = kmalloc(CCCI_LOG_MAX_WRITE, GFP_ATOMIC);
	if (temp_log == NULL) {
		/*pr_notice("[ccci0/util]alloc local buff fail p1\n");*/
		return -ENODEV;
	}

	if (set_flags & CCCI_DUMP_TIME_FLAG) {
		state = irqs_disabled() ? '-' : ' ';
		ts_nsec = local_clock();
		rem_nsec = do_div(ts_nsec, 1000000000);
		preempt_disable();
		this_cpu = smp_processor_id();
		preempt_enable();
		write_len = snprintf(temp_log, CCCI_LOG_MAX_WRITE,
					"[%5lu.%06lu]%c(%x)",
					(unsigned long)ts_nsec,
					rem_nsec / 1000, state,
					this_cpu);
	} else
		write_len = 0;

	if (set_flags & CCCI_DUMP_CURR_FLAG) {
		write_len += snprintf(temp_log + write_len,
						CCCI_LOG_MAX_WRITE - write_len,
						"[%d:%s]",
						current->pid, current->comm);
	}

	va_start(args, fmt);
	write_len += vsnprintf(temp_log + write_len,
					CCCI_LOG_MAX_WRITE - write_len,
					fmt, args);
	va_end(args);

	spin_lock_irqsave(&ccci_log_buf.write_lock, flags);
	if (ccci_log_buf.write_pos + write_len > CCCI_LOG_BUF_SIZE) {
		first_half = CCCI_LOG_BUF_SIZE - ccci_log_buf.write_pos;
		memcpy(ccci_log_buf.buffer + ccci_log_buf.write_pos,
				temp_log, first_half);
		memcpy(ccci_log_buf.buffer, temp_log + first_half,
				write_len - first_half);
	} else {
		memcpy(ccci_log_buf.buffer + ccci_log_buf.write_pos,
				temp_log, write_len);
	}
	ccci_log_buf.write_pos = (ccci_log_buf.write_pos + write_len)
		& (CCCI_LOG_BUF_SIZE - 1);
	if (write_len > 0)
		ccci_log_buf.ch_num += (unsigned int)write_len;
	spin_unlock_irqrestore(&ccci_log_buf.write_lock, flags);
	wake_up_all(&ccci_log_buf.log_wq);

	kfree(temp_log);
	temp_log = NULL;

	return write_len;
}

static ssize_t ccci_log_read(struct file *file, char __user *buf,
	size_t size, loff_t *ppos)
{
	unsigned int available, read_len, first_half, read_pos;
	unsigned long flags;
	int ret;

 retry:
	spin_lock_irqsave(&ccci_log_buf.write_lock, flags);
	available = ccci_log_buf.ch_num;
	if (available >= CCCI_LOG_BUF_SIZE) {
		/* This means over flow, write pos is oldest log */
		available = CCCI_LOG_BUF_SIZE;
		read_pos = ccci_log_buf.write_pos;
	} else
		read_pos = ccci_log_buf.read_pos;

	if (!available) {
		spin_unlock_irqrestore(&ccci_log_buf.write_lock, flags);
		if (!(file->f_flags & O_NONBLOCK)) {
			ret = wait_event_interruptible(ccci_log_buf.log_wq,
				ccci_log_buf.ch_num);
			if (ret == -ERESTARTSYS)
				return -EINTR;
			goto retry;
		} else {
			return -EAGAIN;
		}
	}

	read_len = size < available ? size : available;
	spin_unlock_irqrestore(&ccci_log_buf.write_lock, flags);

	if (read_pos + read_len > CCCI_LOG_BUF_SIZE) {
		first_half = CCCI_LOG_BUF_SIZE - read_pos;
		ret = copy_to_user(buf,
						ccci_log_buf.buffer + read_pos,
						first_half);
		ret += copy_to_user(buf + first_half, ccci_log_buf.buffer,
			read_len - first_half);
	} else {
		ret = copy_to_user(buf,
						ccci_log_buf.buffer + read_pos,
						read_len);
	}

	spin_lock_irqsave(&ccci_log_buf.write_lock, flags);
	read_len = read_len - ret;
	ccci_log_buf.read_pos = (read_pos + read_len) & (CCCI_LOG_BUF_SIZE - 1);
	ccci_log_buf.ch_num -= read_len;
	spin_unlock_irqrestore(&ccci_log_buf.write_lock, flags);
	return read_len;
}

unsigned int ccci_log_poll(struct file *fp, struct poll_table_struct *poll)
{
	unsigned int mask = 0;

	poll_wait(fp, &ccci_log_buf.log_wq, poll);
	if (ccci_log_buf.ch_num)
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

static int ccci_log_open(struct inode *inode, struct file *file)
{
	if (atomic_read(&ccci_log_buf.reader_cnt))
		return -EBUSY;
	atomic_inc(&ccci_log_buf.reader_cnt);
	return 0;
}

static int ccci_log_close(struct inode *inode, struct file *file)
{
	atomic_dec(&ccci_log_buf.reader_cnt);
	return 0;
}

static const struct file_operations ccci_log_fops = {
	.read = ccci_log_read,
	.open = ccci_log_open,
	.release = ccci_log_close,
	.poll = ccci_log_poll,
};


/******************************************************************************/
/* Dump buffer part, this type log is NON block read, used for AED dump       */
/******************************************************************************/
#define CCCI_INIT_SETTING_BUF	(4096*2)
#define CCCI_BOOT_UP_BUF		(4096*16)
#ifndef CONFIG_FUCK_MTK_LOG
#define CCCI_NORMAL_BUF			(4096*2)
#define CCCI_REPEAT_BUF			(4096*32)
#define CCCI_HISTORY_BUF		(4096*128)
#define MD3_CCCI_INIT_SETTING_BUF   (4096*2)
#define MD3_CCCI_BOOT_UP_BUF                (4096*16)
#define MD3_CCCI_NORMAL_BUF                 (4096*2)
#define MD3_CCCI_REPEAT_BUF                 (4096*32)
#define MD3_CCCI_REG_DUMP_BUF               (4096*32)
#define MD3_CCCI_HISTORY_BUF                (4096*32)
#else
#define CCCI_NORMAL_BUF			(0)
#define CCCI_REPEAT_BUF			(0)
#define CCCI_HISTORY_BUF		(0)
#define MD3_CCCI_INIT_SETTING_BUF   (0)
#define MD3_CCCI_BOOT_UP_BUF                (0)
#define MD3_CCCI_NORMAL_BUF                 (0)
#define MD3_CCCI_REPEAT_BUF                 (0)
#define MD3_CCCI_REG_DUMP_BUF               (0)
#define MD3_CCCI_HISTORY_BUF                (0)
#endif
#define CCCI_REG_DUMP_BUF		(4096*64 * 2)


struct ccci_dump_buffer {
	void *buffer;
	unsigned int buf_size;
	unsigned int data_size;
	unsigned int write_pos;
	unsigned int max_num;
	unsigned int attr;
	spinlock_t lock;
};

struct ccci_user_ctlb {
	unsigned int read_idx[2][CCCI_DUMP_MAX];
	unsigned int sep_cnt1[2][CCCI_DUMP_MAX];
	unsigned int sep_cnt2[2]; /* 1st MD; 2nd MD */
	unsigned int busy;
};
static spinlock_t file_lock;

static struct ccci_dump_buffer init_setting_ctlb[2];
static struct ccci_dump_buffer normal_ctlb[2];
static struct ccci_dump_buffer boot_up_ctlb[2];
static struct ccci_dump_buffer repeat_ctlb[2];
static struct ccci_dump_buffer reg_dump_ctlb[2];
static struct ccci_dump_buffer history_ctlb[2];
static struct ccci_dump_buffer ke_dump_ctlb[2];
static int buff_bind_md_id[5];
static int md_id_bind_buf_id[5];
static int buff_en_bit_map;
static char sep_buf[64];
static char md_sep_buf[64];

struct buffer_node {
	struct ccci_dump_buffer *ctlb_ptr;
	unsigned int init_size;
	unsigned int init_attr;
	unsigned int index;
};

/* local attribute */
#define CCCI_DUMP_ATTR_BUSY	(1<<0)
#define CCCI_DUMP_ATTR_RING	(1<<1)

static int get_plat_capbility(int md_id)
{
	int en_flag = 0;

	/* MD1 */
	/* Fix me, may design more better solution to reduce memory usage */
	en_flag |= (1<<0);

	/* MD3 */
	en_flag |= (1<<2);

	return (en_flag & (1<<md_id));
}

static struct buffer_node node_array[2][CCCI_DUMP_MAX+1] = {
	{
		{&init_setting_ctlb[0], CCCI_INIT_SETTING_BUF,
		0, CCCI_DUMP_INIT},
		{&boot_up_ctlb[0], CCCI_BOOT_UP_BUF,
		CCCI_DUMP_ATTR_RING, CCCI_DUMP_BOOTUP},
		{&normal_ctlb[0], CCCI_NORMAL_BUF,
		CCCI_DUMP_ATTR_RING, CCCI_DUMP_NORMAL},
		{&repeat_ctlb[0], CCCI_REPEAT_BUF,
		CCCI_DUMP_ATTR_RING, CCCI_DUMP_REPEAT},
		{&reg_dump_ctlb[0], CCCI_REG_DUMP_BUF,
		CCCI_DUMP_ATTR_RING, CCCI_DUMP_MEM_DUMP},
		{&history_ctlb[0], CCCI_HISTORY_BUF,
		CCCI_DUMP_ATTR_RING, CCCI_DUMP_HISTORY},
		{&ke_dump_ctlb[0], 32*1024,
		CCCI_DUMP_ATTR_RING, CCCI_DUMP_REGISTER},
	},
	{
		{&init_setting_ctlb[1], MD3_CCCI_INIT_SETTING_BUF,
		0, CCCI_DUMP_INIT},
		{&boot_up_ctlb[1], MD3_CCCI_BOOT_UP_BUF,
		CCCI_DUMP_ATTR_RING, CCCI_DUMP_BOOTUP},
		{&normal_ctlb[1], MD3_CCCI_NORMAL_BUF,
		CCCI_DUMP_ATTR_RING, CCCI_DUMP_NORMAL},
		{&repeat_ctlb[1], MD3_CCCI_REPEAT_BUF,
		CCCI_DUMP_ATTR_RING, CCCI_DUMP_REPEAT},
		{&reg_dump_ctlb[1], MD3_CCCI_REG_DUMP_BUF,
		CCCI_DUMP_ATTR_RING, CCCI_DUMP_MEM_DUMP},
		{&history_ctlb[1], MD3_CCCI_HISTORY_BUF,
		CCCI_DUMP_ATTR_RING, CCCI_DUMP_HISTORY},
		{&ke_dump_ctlb[1], 1*1024,
		CCCI_DUMP_ATTR_RING, CCCI_DUMP_REGISTER},
	}
};

int ccci_dump_write(int md_id, int buf_type,
	unsigned int flag, const char *fmt, ...)
{
	return 0;
}

static void format_separate_str(char str[], int type)
{
	int i, j;
	char *sep_str;

	switch (type) {
	case CCCI_DUMP_INIT:
		sep_str = "[0]INIT LOG REGION";
		break;
	case CCCI_DUMP_NORMAL:
		sep_str = "[0]NORMAL LOG REGION";
		break;
	case CCCI_DUMP_BOOTUP:
		sep_str = "[0]BOOT LOG REGION";
		break;
	case CCCI_DUMP_REPEAT:
		sep_str = "[0]REPEAT LOG REGION";
		break;
	case CCCI_DUMP_MEM_DUMP:
		sep_str = "[0]MEM DUMP LOG REGION";
		break;
	case CCCI_DUMP_HISTORY:
		sep_str = "[0]HISTORY LOG REGION";
		break;
	case CCCI_DUMP_REGISTER:
		sep_str = "[0]REGISTER LOG REGION";
		break;
	default:
		sep_str = "[0]Unsupport REGION";
		break;
	}

	j = 0;
	for (i = 8; i < (strlen(sep_str) + 8); i++)
		str[i] = sep_str[j++];

	for (; i < (64-1); i++) {
		if (str[i] != '_')
			str[i] = '_';
		else
			break;
	}
}

static ssize_t ccci_dump_read(struct file *file, char __user *buf,
	size_t size, loff_t *ppos)
{
	return 0;
}

unsigned int ccci_dump_poll(struct file *fp, struct poll_table_struct *poll)
{
	return POLLIN | POLLRDNORM;
}

static int ccci_dump_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int ccci_dump_close(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations ccci_dump_fops = {};

static void ccci_dump_buffer_init(void)
{
}

/* functions will be called by external */
int get_dump_buf_usage(char buf[], int size)
{
	return 0;
}

void ccci_util_mem_dump(int md_id, int buf_type, void *start_addr, int len)
{
}

void ccci_util_cmpt_mem_dump(int md_id, int buf_type,
	void *start_addr, int len)
{
}

/******************************************************************************/
/* Ring buffer part, this type log is block read, used for temp debug purpose */
/******************************************************************************/
#define CCCI_EVENT_BUF_SIZE		(4096)

struct ccci_event_buf_t {
	void *buffer;
	unsigned int buf_size;
	unsigned int data_size;
	unsigned int write_pos;
	spinlock_t lock;
};

static struct ccci_event_buf_t ccci_event_buffer;

static void ccci_event_buffer_init(void)
{
	spin_lock_init(&ccci_event_buffer.lock);
	ccci_event_buffer.buffer = vmalloc(CCCI_EVENT_BUF_SIZE);
	ccci_event_buffer.buf_size = CCCI_EVENT_BUF_SIZE;
	ccci_event_buffer.data_size = 0;
	ccci_event_buffer.write_pos = 0;
}

int ccci_event_log(const char *fmt, ...)
{
	va_list args;
	unsigned int write_len = 0;
	char *temp_log;
	int this_cpu;
	char state;
	u64 ts_nsec;
	unsigned long rem_nsec;
	unsigned long flags;
	unsigned int wr_pose;
	int can_be_write;
	struct rtc_time tm;
	struct timeval tv = { 0 };
	struct timeval tv_android = { 0 };
	struct rtc_time tm_android;

	if (ccci_event_buffer.buffer == NULL)
		return 0;

	temp_log = kmalloc(CCCI_LOG_MAX_WRITE, GFP_ATOMIC);
	if (temp_log == NULL)
		return 0;

	/* prepare kernel time info */
	state = irqs_disabled() ? '-' : ' ';
	ts_nsec = local_clock();
	rem_nsec = do_div(ts_nsec, 1000000000);
	preempt_disable();
	this_cpu = smp_processor_id();
	preempt_enable();

	/* prepare andorid time info */
	do_gettimeofday(&tv);
	tv_android = tv;
	rtc_time_to_tm(tv.tv_sec, &tm);
	tv_android.tv_sec -= sys_tz.tz_minuteswest * 60;
	rtc_time_to_tm(tv_android.tv_sec, &tm_android);

	write_len = snprintf(temp_log, CCCI_LOG_MAX_WRITE,
			"%d%02d%02d-%02d:%02d:%02d.%03d [%5lu.%06lu]%c(%x)[%d:%s]",
			tm.tm_year + 1900,
			tm.tm_mon + 1,
			tm.tm_mday,
			tm_android.tm_hour,
			tm_android.tm_min,
			tm_android.tm_sec,
			(unsigned int)tv_android.tv_usec,
			(unsigned long)ts_nsec,
			rem_nsec / 1000,
			state,
			this_cpu,
			current->pid,
			current->comm);

	va_start(args, fmt);
	write_len += vsnprintf(temp_log
					+ write_len,
					CCCI_LOG_MAX_WRITE - write_len,
					fmt, args);
	va_end(args);

	spin_lock_irqsave(&ccci_event_buffer.lock, flags);

	wr_pose = ccci_event_buffer.write_pos;
	if (wr_pose + write_len > ccci_event_buffer.buf_size) {
		can_be_write = ccci_event_buffer.buf_size - wr_pose;
		memcpy(ccci_event_buffer.buffer + wr_pose,
				temp_log, can_be_write);
		memcpy(ccci_event_buffer.buffer,
				temp_log + can_be_write,
				write_len - can_be_write);
	} else {
		memcpy(ccci_event_buffer.buffer + wr_pose, temp_log, write_len);
	}
	ccci_event_buffer.data_size += write_len;
	if (ccci_event_buffer.data_size > ccci_event_buffer.buf_size)
		ccci_event_buffer.data_size = ccci_event_buffer.buf_size + 1;

	ccci_event_buffer.write_pos =
		(wr_pose + write_len) & (ccci_event_buffer.buf_size-1);

	spin_unlock_irqrestore(&ccci_event_buffer.lock, flags);

	kfree(temp_log);

	return write_len;
}

int ccci_event_log_cpy(char buf[], int size)
{
	unsigned long flags;
	unsigned int rd_pose;
	int cpy_size;
	int first_half;

	spin_lock_irqsave(&ccci_event_buffer.lock, flags);

	if (ccci_event_buffer.data_size > ccci_event_buffer.buf_size) {
		/* here using BUFFER size */
		cpy_size =
		ccci_event_buffer.buf_size > size
		?
		size
		:
		ccci_event_buffer.buf_size;
		rd_pose = ccci_event_buffer.write_pos;
		first_half = ccci_event_buffer.buf_size - rd_pose;
		memcpy(buf, ccci_event_buffer.buffer + rd_pose, first_half);
		memcpy(&buf[first_half], ccci_event_buffer.buffer,
			ccci_event_buffer.buf_size - first_half);
	} else {
		/* here using DATA size */
		cpy_size =
		ccci_event_buffer.data_size > size
		?
		size
		:
		ccci_event_buffer.data_size;
		memcpy(buf, ccci_event_buffer.buffer, cpy_size);
	}

	spin_unlock_irqrestore(&ccci_event_buffer.lock, flags);

	return cpy_size;
}


void ccci_log_init(void)
{
}

void get_ccci_aee_buffer(unsigned long *vaddr, unsigned long *size)
{
}
EXPORT_SYMBOL(get_ccci_aee_buffer);

void get_md_aee_buffer(unsigned long *vaddr, unsigned long *size)
{
}
EXPORT_SYMBOL(get_md_aee_buffer);
