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

#ifndef __TRUSTZONE_TA_DDP_LOG__
#define __TRUSTZONE_TA_DDP_LOG__

/* for self-defined log output marco */
#ifndef __MTEE_LOG_H__
#include <tz_private/log.h>
#endif

/* to control the DEBUG level output. define it some where else. */
extern unsigned int g_tee_dbg_log;

/* for temporary debugging purpose */
#define MTEE_LOG_CUSTOM_LEVEL MTEE_LOG_LVL_INFO

#define MTEE_LOG_I(args...)
#define MTEE_LOG_D(args...) 
#define MTEE_LOG_P(args...) 
#define MTEE_LOG_W(args...) 
#define MTEE_LOG_B(args...) 
#define MTEE_LOG_A(args...) 

#endif	/* __TRUSTZONE_TA_DDP_LOG__ */
