/*
 * Copyright (c) 2015-2017 MICROTRUST Incorporated
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _ISEE_IMSG_LOG_H_
#define _ISEE_IMSG_LOG_H_

#ifndef IMSG_TAG
#define IMSG_TAG "[ISEE DRV]"
#endif

enum {
	IMSG_LV_DISBLE = 0,
	IMSG_LV_ERROR,
	IMSG_LV_WARN,
	IMSG_LV_INFO,
	IMSG_LV_DEBUG,
	IMSG_LV_TRACE
};

/* DO NOT change the log level, this is for production */
#define IMSG_LOG_LEVEL          IMSG_LV_WARN
#define IMSG_PROFILE_LEVEL      IMSG_LV_TRACE

#include <linux/printk.h>
#include <linux/ktime.h>
#include <linux/timekeeping.h>

uint32_t get_imsg_log_level(void);

static inline unsigned long now_ms(void)
{
	struct timeval now_time;

	do_gettimeofday(&now_time);
	return ((now_time.tv_sec * 1000000) + now_time.tv_usec)/1000;
}

#define IMSG_PRINTK(fmt, ...) 
#define IMSG_PRINTK_DEBUG(fmt, ...)

#define IMSG_PRINT_ERROR(fmt, ...)
#define IMSG_PRINT_WARN(fmt, ...)
#define IMSG_PRINT_INFO(fmt, ...)
#define IMSG_PRINT_DEBUG(fmt, ...)
#define IMSG_PRINT_TRACE(fmt, ...)
#define IMSG_PRINT_ENTER(fmt, ...) 
#define IMSG_PRINT_LEAVE(fmt, ...) 
#define IMSG_PRINT_PROFILE(fmt, ...)

#define IMSG_PRINT(level, func, fmt, ...)
#define IMSG_PRINT_TIME_S(level, fmt, ...)
#define IMSG_PRINT_TIME_E(level, fmt, ...) 
/*************************************************************************/
/* Declare macros ********************************************************/
/*************************************************************************/
#define IMSG_ERROR(fmt, ...) 
#define IMSG_WARN(fmt, ...) 
#define IMSG_INFO(fmt, ...) 
#define IMSG_DEBUG(fmt, ...) 
#define IMSG_TRACE(fmt, ...) 
#define IMSG_ENTER() 
#define IMSG_LEAVE() 

#define IMSG_PROFILE_S(fmt, ...) 
#define IMSG_PROFILE_E(fmt, ...) 

/*************************************************************************/
/* Declare Check Patch ***************************************************/
/*************************************************************************/
#define TZ_SEMA_INIT_0(x)           sema_init(x, 0)
#define TZ_SEMA_INIT_1(x)           sema_init(x, 1)
#define TZ_EMPTY_PARENTHESES(z)     z
#define TZ_VOLATILE(x)              TZ_EMPTY_PARENTHESES(vola##tile x)
#define TZ_NON_VOLATILE(x)          x

#endif /* _ISEE_IMSG_LOG_H_ */
