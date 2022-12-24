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

#ifndef _H_DDP_LOG_
#define _H_DDP_LOG_
#include "display_recorder.h"
#include "ddp_debug.h"
#include "disp_drv_log.h"

#ifndef LOG_TAG
#define LOG_TAG
#endif

#define DDPSVPMSG(fmt, args...)

#define DISP_LOG_I(fmt, args...)	
#define DISP_LOG_V(fmt, args...)	
#define DISP_LOG_D(fmt, args...)	
#define DISP_LOG_W(fmt, args...)	
#define DISP_LOG_E(fmt, args...)	
#define DDPIRQ(fmt, args...)
#define DDPDBG(fmt, args...) 

#define DDPMSG(fmt, args...)

#define DDPWRN(fmt, args...)

#define DDPERR(fmt, args...)

#define DDPDUMP(fmt, ...)
#ifndef ASSERT
#define ASSERT(expr)
#endif

#define DDPAEE(string, args...)
#endif
